import { PublicKey } from "@solana/web3.js";
import { LAMPORTS, SOL_MINT } from "./config.js";
import { rpc } from "./rpc.js";
import { dexTokens } from "./market.js";
import { solPrice } from "./prices.js";
import { mapLimit, TtlCache } from "./util.js";

export const TOKEN_PROGRAM = "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
export const TOKEN_2022_PROGRAM = "TokenzQdBNbLqP5VEhdkAjS6EPFLzUrHrnMKrXsoF4Mc";

export interface MintInfo {
  mint: string;
  program: string;
  decimals: number;
  supply: number;
  mintAuthority: string | null;
  freezeAuthority: string | null;
  extensions: string[];
}

const mintCache = new TtlCache<MintInfo>(60_000);

export async function getMintInfo(mint: string): Promise<MintInfo> {
  return mintCache.get(mint, async () => {
    const res = await rpc<any>("getAccountInfo", [mint, { encoding: "jsonParsed", commitment: "confirmed" }]);
    const acc = res?.value;
    if (!acc) throw new Error("Mint introuvable sur Solana");
    const info = acc.data?.parsed?.info;
    if (!info) throw new Error("Ce compte n'est pas un mint SPL");
    return {
      mint,
      program: acc.owner,
      decimals: info.decimals,
      supply: Number(info.supply) / 10 ** info.decimals,
      mintAuthority: info.mintAuthority ?? null,
      freezeAuthority: info.freezeAuthority ?? null,
      extensions: (info.extensions ?? []).map((e: any) => e.extension),
    };
  });
}

export interface Holder {
  account: string;
  owner: string | null;
  amount: number;
  pct: number;
  isProgram: boolean; // propriétaire hors courbe = PDA (courbe de liaison, pool AMM…)
}

const holdersCache = new TtlCache<Holder[]>(30_000);

export async function getTopHolders(mint: string): Promise<Holder[]> {
  return holdersCache.get(mint, async () => {
    const info = await getMintInfo(mint);
    const res = await rpc<any>("getTokenLargestAccounts", [mint, { commitment: "confirmed" }]);
    const list: any[] = res?.value ?? [];
    const accounts = list.map((a) => a.address);
    const owners = accounts.length
      ? await rpc<any>("getMultipleAccounts", [accounts, { encoding: "jsonParsed", commitment: "confirmed" }])
      : { value: [] };
    return list.map((a, i) => {
      const owner: string | null = owners.value?.[i]?.data?.parsed?.info?.owner ?? null;
      let isProgram = false;
      if (owner) {
        try {
          isProgram = !PublicKey.isOnCurve(new PublicKey(owner).toBytes());
        } catch {}
      }
      const amount = Number(a.uiAmount ?? 0);
      return { account: a.address, owner, amount, pct: info.supply ? (amount / info.supply) * 100 : 0, isProgram };
    });
  });
}

/* ------------------------------ Portefeuille ------------------------------ */

export interface WalletToken {
  mint: string;
  account: string;
  program: string;
  amount: number;
  rawAmount: string;
  decimals: number;
  name: string;
  symbol: string;
  image?: string;
  priceUsd: number | null;
  priceSol: number | null;
  valueUsd: number | null;
  valueSol: number | null;
}

export async function getWalletTokens(owner: string): Promise<{ sol: number; tokens: WalletToken[] }> {
  const [bal, a, b] = await Promise.all([
    rpc<any>("getBalance", [owner, { commitment: "confirmed" }]),
    rpc<any>("getParsedTokenAccountsByOwner", [owner, { programId: TOKEN_PROGRAM }, { encoding: "jsonParsed", commitment: "confirmed" }]),
    rpc<any>("getParsedTokenAccountsByOwner", [owner, { programId: TOKEN_2022_PROGRAM }, { encoding: "jsonParsed", commitment: "confirmed" }]),
  ]);
  const raw = [...(a?.value ?? []), ...(b?.value ?? [])]
    .map((x: any) => {
      const info = x.account.data.parsed.info;
      return {
        mint: info.mint as string,
        account: x.pubkey as string,
        program: x.account.owner as string,
        amount: Number(info.tokenAmount.uiAmountString ?? info.tokenAmount.uiAmount ?? 0),
        rawAmount: String(info.tokenAmount.amount),
        decimals: Number(info.tokenAmount.decimals),
      };
    })
    .filter((x) => x.amount > 0 && x.mint !== SOL_MINT);
  let dex = new Map<string, any>();
  try {
    dex = await dexTokens(raw.map((r) => r.mint));
  } catch {}
  const usd = solPrice.usd;
  const tokens: WalletToken[] = raw.map((r) => {
    const d = dex.get(r.mint);
    const priceUsd = d?.priceUsd ?? null;
    const priceSol = d?.priceNative ?? (priceUsd != null && usd ? priceUsd / usd : null);
    return {
      ...r,
      name: d?.name ?? "",
      symbol: d?.symbol ?? "",
      image: d?.imageUrl,
      priceUsd,
      priceSol,
      valueUsd: priceUsd != null ? priceUsd * r.amount : null,
      valueSol: priceSol != null ? priceSol * r.amount : null,
    };
  });
  tokens.sort((x, y) => (y.valueUsd ?? -1) - (x.valueUsd ?? -1));
  return { sol: Number(bal?.value ?? 0) / LAMPORTS, tokens };
}

/* ------------------------- Historique et P&L réels ------------------------- */

export interface ParsedTx {
  signature: string;
  time: number | null;
  slot: number;
  err: boolean;
  feeSol: number;
  solDelta: number; // variation SOL du portefeuille (WSOL inclus), frais compris
  tokenDeltas: Record<string, number>;
  kind: "buy" | "sell" | "transfer" | "other";
  mint?: string;
}

const txCache = new TtlCache<any>(24 * 3600_000, 20_000);

export async function getTx(signature: string): Promise<any> {
  return txCache.get(signature, async () => {
    const tx = await rpc<any>("getTransaction", [
      signature,
      { encoding: "jsonParsed", maxSupportedTransactionVersion: 0, commitment: "confirmed" },
    ]);
    if (!tx) throw new Error("Transaction introuvable (pas encore confirmée ?)");
    return tx;
  });
}

export function parseTx(tx: any, wallet: string, signature: string): ParsedTx {
  const keys: any[] = tx.transaction.message.accountKeys;
  const idx = keys.findIndex((k: any) => (k.pubkey ?? k) === wallet);
  const meta = tx.meta ?? {};
  let solDelta = idx >= 0 ? (meta.postBalances[idx] - meta.preBalances[idx]) / LAMPORTS : 0;
  const deltas: Record<string, number> = {};
  const add = (b: any, sign: number) => {
    if (b.owner !== wallet) return;
    const v = Number(b.uiTokenAmount?.uiAmountString ?? b.uiTokenAmount?.uiAmount ?? 0) * sign;
    deltas[b.mint] = (deltas[b.mint] ?? 0) + v;
  };
  (meta.preTokenBalances ?? []).forEach((b: any) => add(b, -1));
  (meta.postTokenBalances ?? []).forEach((b: any) => add(b, +1));
  if (deltas[SOL_MINT]) {
    solDelta += deltas[SOL_MINT];
    delete deltas[SOL_MINT];
  }
  for (const k of Object.keys(deltas)) if (Math.abs(deltas[k]) < 1e-12) delete deltas[k];
  const mints = Object.keys(deltas);
  let kind: ParsedTx["kind"] = "other";
  let mint: string | undefined;
  if (mints.length === 1) {
    mint = mints[0];
    const d = deltas[mint];
    if (d > 0 && solDelta < 0) kind = "buy";
    else if (d < 0 && solDelta > 0) kind = "sell";
    else kind = "transfer";
  } else if (mints.length === 0 && solDelta !== 0) kind = "transfer";
  return {
    signature,
    time: tx.blockTime ? tx.blockTime * 1000 : null,
    slot: tx.slot,
    err: meta.err != null,
    feeSol: idx === 0 ? (meta.fee ?? 0) / LAMPORTS : 0,
    solDelta,
    tokenDeltas: deltas,
    kind,
    mint,
  };
}

export async function getWalletHistory(wallet: string, limit = 100): Promise<ParsedTx[]> {
  const sigs = await rpc<any[]>("getSignaturesForAddress", [wallet, { limit: Math.min(limit, 300), commitment: "confirmed" }]);
  const results = await mapLimit(sigs ?? [], 6, async (s: any) => {
    try {
      const tx = await getTx(s.signature);
      return parseTx(tx, wallet, s.signature);
    } catch {
      return null;
    }
  });
  return results.filter((x): x is ParsedTx => x !== null);
}

export interface PnlRow {
  mint: string;
  name: string;
  symbol: string;
  image?: string;
  boughtTokens: number;
  soldTokens: number;
  solSpent: number;
  solReceived: number;
  avgCostSol: number | null;
  holding: number;
  priceSol: number | null;
  valueSol: number | null;
  realizedSol: number;
  unrealizedSol: number | null;
  totalSol: number | null;
  trades: number;
}

/**
 * P&L calculé uniquement à partir des transactions on-chain du portefeuille
 * (méthode du coût moyen). Les frais réseau et la rente des comptes créés sont inclus dans le coût.
 */
export async function computePnl(wallet: string, limit = 200) {
  const [history, holdings] = await Promise.all([getWalletHistory(wallet, limit), getWalletTokens(wallet)]);
  const rows = new Map<string, PnlRow>();
  const chronological = [...history].filter((h) => !h.err).reverse();
  for (const h of chronological) {
    if ((h.kind !== "buy" && h.kind !== "sell") || !h.mint) continue;
    const r =
      rows.get(h.mint) ??
      ({
        mint: h.mint,
        name: "",
        symbol: "",
        boughtTokens: 0,
        soldTokens: 0,
        solSpent: 0,
        solReceived: 0,
        avgCostSol: null,
        holding: 0,
        priceSol: null,
        valueSol: null,
        realizedSol: 0,
        unrealizedSol: null,
        totalSol: null,
        trades: 0,
      } as PnlRow);
    const qty = Math.abs(h.tokenDeltas[h.mint]);
    r.trades++;
    if (h.kind === "buy") {
      const costBefore = (r.avgCostSol ?? 0) * r.holding;
      r.boughtTokens += qty;
      r.solSpent += -h.solDelta;
      r.holding += qty;
      r.avgCostSol = r.holding > 0 ? (costBefore - h.solDelta) / r.holding : null;
    } else {
      r.soldTokens += qty;
      r.solReceived += h.solDelta;
      const basis = (r.avgCostSol ?? 0) * Math.min(qty, r.holding);
      r.realizedSol += h.solDelta - basis;
      r.holding = Math.max(0, r.holding - qty);
    }
    rows.set(h.mint, r);
  }
  const held = new Map(holdings.tokens.map((t) => [t.mint, t]));
  for (const r of rows.values()) {
    const h = held.get(r.mint);
    if (h) {
      r.name = h.name;
      r.symbol = h.symbol;
      r.image = h.image;
      r.priceSol = h.priceSol;
      // Le solde réel on-chain fait foi (transferts éventuels hors historique analysé).
      const tracked = r.holding;
      r.holding = h.amount;
      r.valueSol = h.valueSol;
      if (h.valueSol != null && r.avgCostSol != null) {
        r.unrealizedSol = h.valueSol - r.avgCostSol * Math.min(tracked, h.amount);
      }
    } else {
      r.holding = 0;
      r.valueSol = 0;
      r.unrealizedSol = 0;
    }
    r.totalSol = r.unrealizedSol != null ? r.realizedSol + r.unrealizedSol : null;
  }
  const missingNames = [...rows.values()].filter((r) => !r.symbol).map((r) => r.mint);
  if (missingNames.length) {
    try {
      const dex = await dexTokens(missingNames);
      for (const m of missingNames) {
        const d = dex.get(m);
        const r = rows.get(m)!;
        if (d) {
          r.name = d.name;
          r.symbol = d.symbol;
          r.image = d.imageUrl;
        }
      }
    } catch {}
  }
  const list = [...rows.values()].sort((a, b) => Math.abs(b.totalSol ?? b.realizedSol) - Math.abs(a.totalSol ?? a.realizedSol));
  const totals = list.reduce(
    (s, r) => ({
      realizedSol: s.realizedSol + r.realizedSol,
      unrealizedSol: s.unrealizedSol + (r.unrealizedSol ?? 0),
    }),
    { realizedSol: 0, unrealizedSol: 0 },
  );
  return {
    rows: list,
    totals,
    analyzedTransactions: history.length,
    solUsd: solPrice.usd,
  };
}
