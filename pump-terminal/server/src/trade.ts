import { PublicKey, VersionedTransaction } from "@solana/web3.js";
import { getAssociatedTokenAddressSync } from "@solana/spl-token";
import { config, LAMPORTS, SOL_MINT } from "./config.js";
import { rpc } from "./rpc.js";
import { getMintInfo, getTx, parseTx } from "./onchain.js";
import { store } from "./tokens.js";
import { dexToken } from "./market.js";
import { assessRisk, RiskReport } from "./risk.js";
import { checkBuy } from "./limits.js";
import { db, save } from "./db.js";
import { alert } from "./notify.js";
import { solPrice } from "./prices.js";
import { errMsg, sleep } from "./util.js";

const COMPUTE_BUDGET = "ComputeBudget111111111111111111111111111111";

export interface PrepareRequest {
  wallet: string;
  mint: string;
  side: "buy" | "sell";
  amountSol?: number;
  percent?: number;
  slippagePct: number;
  priorityFeeSol: number;
  route: "pumpportal" | "jupiter";
  pool?: string;
  source?: "manual" | "auto";
  proposalId?: string;
}

export interface TradePreview {
  txBase64: string;
  preparedAt: number;
  route: string;
  pool: string;
  side: "buy" | "sell";
  mint: string;
  symbol: string;
  name: string;
  amountSol?: number;
  percent?: number;
  tokenBalanceBefore: number;
  solBalanceBefore: number;
  simulation: {
    ok: boolean;
    error: string | null;
    logs: string[];
    unitsConsumed: number | null;
    solChange: number | null;
    tokenChange: number | null;
  };
  fees: {
    baseFeeSol: number;
    priorityFeeSol: number;
    computeUnitLimit: number | null;
    computeUnitPriceMicroLamports: number | null;
    platformFeePct: number;
    platformFeeSolEst: number | null;
    rentNote: string | null;
  };
  slippagePct: number;
  minOut: number | null;
  spotPriceSol: number | null;
  effectivePriceSol: number | null;
  priceImpactPct: number | null;
  solUsd: number | null;
  risk: RiskReport;
  warnings: string[];
}

function assertPubkey(s: string, label: string) {
  try {
    return new PublicKey(s);
  } catch {
    throw new Error(`${label} invalide`);
  }
}

async function buildPumpPortal(r: PrepareRequest): Promise<Uint8Array> {
  const body = {
    publicKey: r.wallet,
    action: r.side,
    mint: r.mint,
    amount: r.side === "buy" ? r.amountSol : `${r.percent}%`,
    denominatedInSol: r.side === "buy" ? "true" : "false",
    slippage: r.slippagePct,
    priorityFee: r.priorityFeeSol,
    pool: r.pool || "auto",
  };
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), 15_000);
  try {
    const res = await fetch(config.pumpPortalTradeUrl, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(body),
      signal: ctrl.signal,
    });
    if (res.status !== 200) throw new Error(`PumpPortal a refusé la transaction : ${(await res.text()).slice(0, 300)}`);
    return new Uint8Array(await res.arrayBuffer());
  } finally {
    clearTimeout(timer);
  }
}

async function buildJupiter(r: PrepareRequest, rawAmount: bigint): Promise<{ bytes: Uint8Array; minOut: number; impact: number }> {
  const headers: Record<string, string> = { "content-type": "application/json" };
  if (config.jupiterApiKey) headers["x-api-key"] = config.jupiterApiKey;
  const inputMint = r.side === "buy" ? SOL_MINT : r.mint;
  const outputMint = r.side === "buy" ? r.mint : SOL_MINT;
  const qs = new URLSearchParams({
    inputMint,
    outputMint,
    amount: rawAmount.toString(),
    slippageBps: String(Math.round(r.slippagePct * 100)),
  });
  const qRes = await fetch(`${config.jupiterBase}/quote?${qs}`, { headers });
  const quote = await qRes.json();
  if (!qRes.ok || quote.error) throw new Error(`Jupiter (devis) : ${quote.error ?? qRes.status}`);
  const sRes = await fetch(`${config.jupiterBase}/swap`, {
    method: "POST",
    headers,
    body: JSON.stringify({
      quoteResponse: quote,
      userPublicKey: r.wallet,
      wrapAndUnwrapSol: true,
      dynamicComputeUnitLimit: true,
      prioritizationFeeLamports: Math.round(r.priorityFeeSol * LAMPORTS),
    }),
  });
  const swap = await sRes.json();
  if (!sRes.ok || !swap.swapTransaction) throw new Error(`Jupiter (transaction) : ${swap.error ?? sRes.status}`);
  return {
    bytes: Buffer.from(swap.swapTransaction, "base64"),
    minOut: Number(quote.otherAmountThreshold),
    impact: Number(quote.priceImpactPct) * 100,
  };
}

function readComputeBudget(tx: VersionedTransaction) {
  const keys = tx.message.staticAccountKeys.map((k) => k.toBase58());
  const cbIndex = keys.indexOf(COMPUTE_BUDGET);
  let limit: number | null = null;
  let price: number | null = null;
  if (cbIndex >= 0) {
    for (const ix of tx.message.compiledInstructions) {
      if (ix.programIdIndex !== cbIndex) continue;
      const d = Buffer.from(ix.data);
      if (d[0] === 2 && d.length >= 5) limit = d.readUInt32LE(1);
      if (d[0] === 3 && d.length >= 9) price = Number(d.readBigUInt64LE(1));
    }
  }
  return { limit, price };
}

export async function prepareTrade(r: PrepareRequest): Promise<TradePreview> {
  const owner = assertPubkey(r.wallet, "Adresse du portefeuille");
  const mintPk = assertPubkey(r.mint, "Adresse du token");
  if (!["buy", "sell"].includes(r.side)) throw new Error("Sens de transaction invalide");
  if (!(r.slippagePct > 0 && r.slippagePct <= 50)) throw new Error("Slippage invalide (0–50 %)");
  if (!(r.priorityFeeSol >= 0 && r.priorityFeeSol <= 0.1)) throw new Error("Frais de priorité invalides (0–0,1 SOL)");
  if (r.side === "buy" && !(Number(r.amountSol) > 0)) throw new Error("Montant en SOL invalide");
  if (r.side === "sell" && !(Number(r.percent) > 0 && Number(r.percent) <= 100)) throw new Error("Pourcentage invalide");

  const source = r.source ?? "manual";
  if (r.side === "buy") {
    const limitErr = checkBuy(Number(r.amountSol), source, r.mint);
    if (limitErr) throw new Error(limitErr);
  }

  const mint = await getMintInfo(r.mint);
  const programId = new PublicKey(mint.program);
  const ata = getAssociatedTokenAddressSync(mintPk, owner, false, programId).toBase58();

  const [solBal, tokBal] = await Promise.all([
    rpc<any>("getBalance", [r.wallet, { commitment: "confirmed" }]),
    rpc<any>("getTokenAccountBalance", [ata, { commitment: "confirmed" }]).catch(() => null),
  ]);
  const lamportsBefore = Number(solBal?.value ?? 0);
  const rawTokensBefore = BigInt(tokBal?.value?.amount ?? "0");
  const tokensBefore = Number(rawTokensBefore) / 10 ** mint.decimals;

  if (r.side === "buy" && lamportsBefore < Number(r.amountSol) * LAMPORTS) {
    throw new Error(`Solde insuffisant : ${(lamportsBefore / LAMPORTS).toFixed(4)} SOL disponibles`);
  }
  if (r.side === "sell" && rawTokensBefore === 0n) throw new Error("Vous ne détenez pas ce token (compte associé vide).");

  let bytes: Uint8Array;
  let minOut: number | null = null;
  let jupImpact: number | null = null;
  if (r.route === "jupiter") {
    const raw =
      r.side === "buy"
        ? BigInt(Math.round(Number(r.amountSol) * LAMPORTS))
        : r.percent === 100
          ? rawTokensBefore
          : (rawTokensBefore * BigInt(Math.round(Number(r.percent) * 100))) / 10000n;
    const j = await buildJupiter(r, raw);
    bytes = j.bytes;
    minOut = r.side === "buy" ? j.minOut / 10 ** mint.decimals : j.minOut / LAMPORTS;
    jupImpact = j.impact;
  } else {
    bytes = await buildPumpPortal(r);
  }

  const tx = VersionedTransaction.deserialize(bytes);
  const payer = tx.message.staticAccountKeys[0]?.toBase58();
  if (payer !== r.wallet) throw new Error("Transaction reçue invalide : le payeur n'est pas votre portefeuille.");

  const txBase64 = Buffer.from(bytes).toString("base64");
  const sim = await rpc<any>("simulateTransaction", [
    txBase64,
    {
      encoding: "base64",
      sigVerify: false,
      replaceRecentBlockhash: true,
      commitment: "processed",
      accounts: { encoding: "base64", addresses: [r.wallet, ata] },
    },
  ]);
  const v = sim?.value ?? {};
  const simOk = v.err == null;
  let solChange: number | null = null;
  let tokenChange: number | null = null;
  if (simOk && v.accounts) {
    const w = v.accounts[0];
    if (w) solChange = (Number(w.lamports) - lamportsBefore) / LAMPORTS;
    const a = v.accounts[1];
    let rawAfter = 0n;
    if (a?.data?.[0]) {
      const buf = Buffer.from(a.data[0], "base64");
      if (buf.length >= 72) rawAfter = buf.readBigUInt64LE(64);
    }
    tokenChange = Number(rawAfter - rawTokensBefore) / 10 ** mint.decimals;
  }

  const cb = readComputeBudget(tx);
  const baseFeeSol = (tx.message.header.numRequiredSignatures * 5000) / LAMPORTS;
  const cuLimit = cb.limit ?? (v.unitsConsumed ? Number(v.unitsConsumed) : null);
  const priorityFeeSol = cb.price != null && cuLimit != null ? (cb.price * cuLimit) / 1e6 / LAMPORTS : 0;

  const summary = store.tokens.get(r.mint) ? store.summary(store.tokens.get(r.mint)!) : null;
  let spot = summary?.priceSol ?? null;
  let symbol = summary?.symbol ?? "";
  let name = summary?.name ?? "";
  if (spot == null || !symbol) {
    const d = await dexToken(r.mint).catch(() => null);
    spot ??= d?.priceNative ?? null;
    symbol ||= d?.symbol ?? "";
    name ||= d?.name ?? "";
  }
  let effective: number | null = null;
  let impact: number | null = jupImpact;
  if (solChange != null && tokenChange != null && tokenChange !== 0) {
    effective = Math.abs(solChange) / Math.abs(tokenChange);
    if (spot && impact == null) {
      impact = r.side === "buy" ? (effective / spot - 1) * 100 : (1 - effective / spot) * 100;
    }
  }
  const platformFeePct = r.route === "pumpportal" ? config.pumpPortalFeePct : 0;
  const platformBase = r.side === "buy" ? Number(r.amountSol) : solChange != null ? Math.max(0, solChange) : null;

  const risk = assessRisk(
    summary ?? {
      mint: r.mint, name, symbol, createdAt: null, firstSeenAt: Date.now(), ageSec: null, pool: "", migrated: true,
      priceSol: spot, priceUsd: null, marketCapSol: null, marketCapUsd: null, liquiditySol: null, bondingPct: null,
      buys: 0, sells: 0, volumeSol: 0, buyVolumeSol: 0, sellVolumeSol: 0, uniqueBuyers: 0, uniqueSellers: 0,
      holdersSeen: 0, top10PctSeen: null, devPct: null, devSoldPct: null, sniperPct: null, sniperCount: 0,
      lastTradeAt: 0, tradesTracked: 0, fromStream: false, twitter: "-",
    },
    mint,
    null,
  );

  const warnings: string[] = [];
  if (!simOk) warnings.push(`La simulation a échoué : la transaction échouerait probablement (${JSON.stringify(v.err)}).`);
  if (impact != null && impact > 10) warnings.push(`Écart de prix élevé : ${impact.toFixed(1)} % par rapport au prix spot.`);
  if (r.slippagePct > 20) warnings.push(`Slippage très élevé (${r.slippagePct} %) : vous acceptez un prix bien pire que prévu.`);
  if (mint.freezeAuthority) warnings.push("Autorité de gel active : risque de ne jamais pouvoir revendre (honeypot).");
  if (mint.mintAuthority) warnings.push("Autorité de mint active : l'offre peut être diluée à tout moment.");
  for (const f of risk.flags) if (f.level === "high" && !warnings.some((w) => w.includes(f.label))) warnings.push(f.label);
  if (r.side === "buy") warnings.push("Les tokens Pump.fun sont extrêmement volatils : vous pouvez perdre la totalité du montant investi.");

  return {
    txBase64,
    preparedAt: Date.now(),
    route: r.route,
    pool: r.pool || "auto",
    side: r.side,
    mint: r.mint,
    symbol,
    name,
    amountSol: r.amountSol,
    percent: r.percent,
    tokenBalanceBefore: tokensBefore,
    solBalanceBefore: lamportsBefore / LAMPORTS,
    simulation: {
      ok: simOk,
      error: simOk ? null : JSON.stringify(v.err),
      logs: (v.logs ?? []).slice(-12),
      unitsConsumed: v.unitsConsumed ?? null,
      solChange,
      tokenChange,
    },
    fees: {
      baseFeeSol,
      priorityFeeSol,
      computeUnitLimit: cb.limit,
      computeUnitPriceMicroLamports: cb.price,
      platformFeePct,
      platformFeeSolEst: platformBase != null ? (platformBase * platformFeePct) / 100 : null,
      rentNote:
        r.side === "buy" && rawTokensBefore === 0n && !tokBal
          ? "Création du compte de token : ~0,002 SOL de rente (récupérable en fermant le compte)."
          : null,
    },
    slippagePct: r.slippagePct,
    minOut,
    spotPriceSol: spot,
    effectivePriceSol: effective,
    priceImpactPct: impact,
    solUsd: solPrice.usd,
    risk,
    warnings,
  };
}

/**
 * Enregistre une transaction après confirmation : le serveur relit la transaction sur Solana,
 * vérifie qu'elle est signée par le portefeuille et qu'elle a réussi.
 */
export async function recordTrade(signature: string, wallet: string, source: string, proposalId?: string) {
  if (db.trades.some((t) => t.signature === signature)) return db.trades.find((t) => t.signature === signature)!;
  let tx: any = null;
  let lastErr = "";
  for (let i = 0; i < 10 && !tx; i++) {
    try {
      tx = await getTx(signature);
    } catch (e) {
      lastErr = errMsg(e);
      await sleep(2000);
    }
  }
  if (!tx) throw new Error(`Transaction introuvable : ${lastErr}`);
  const signer = tx.transaction.message.accountKeys.find((k: any) => k.pubkey === wallet && k.signer);
  if (!signer) throw new Error("Cette transaction n'est pas signée par votre portefeuille.");
  const p = parseTx(tx, wallet, signature);
  if (p.err) {
    if (proposalId) setProposalStatus(proposalId, "failed", signature);
    throw new Error("La transaction a échoué on-chain.");
  }
  if ((p.kind !== "buy" && p.kind !== "sell") || !p.mint) throw new Error("Transaction confirmée mais ce n'est pas un achat/vente de token.");
  const rec = {
    signature,
    wallet,
    mint: p.mint,
    side: p.kind,
    solDelta: p.solDelta,
    tokenDelta: Math.abs(p.tokenDeltas[p.mint]),
    time: p.time ?? Date.now(),
    source,
    proposalId,
  } as const;
  db.trades.push({ ...rec });
  if (proposalId) setProposalStatus(proposalId, "executed", signature);
  save();
  const t = store.tokens.get(p.mint);
  const sym = t?.symbol ? `$${t.symbol}` : p.mint.slice(0, 6);
  alert(
    "trade",
    p.kind === "buy" ? `Achat confirmé ${sym}` : `Vente confirmée ${sym}`,
    `${p.kind === "buy" ? "Dépensé" : "Reçu"} ${Math.abs(p.solDelta).toFixed(4)} SOL — confirmé sur Solana (${signature.slice(0, 10)}…)`,
    p.mint,
  );
  return rec;
}

export function setProposalStatus(id: string, status: "executed" | "failed" | "rejected", signature?: string) {
  const prop = db.proposals.find((x) => x.id === id);
  if (!prop) return;
  prop.status = status;
  if (signature) prop.signature = signature;
  save();
}
