import { db, Proposal, save, Strategy, StrategyFilters } from "./db.js";
import { store, TokenSummary, TradePoint } from "./tokens.js";
import { assessRisk } from "./risk.js";
import { checkBuy, ledger } from "./limits.js";
import { alert } from "./notify.js";
import { broadcast } from "./hub.js";
import { dexToken } from "./market.js";
import { solPrice } from "./prices.js";
import { uid } from "./util.js";

/**
 * Moteur d'automatisation 24 h/24.
 * IMPORTANT : le serveur ne détient AUCUNE clé privée. Une stratégie ne peut que
 * (1) envoyer une alerte, ou (2) créer une PROPOSITION de transaction.
 * Chaque proposition doit être examinée puis signée manuellement dans Phantom.
 */

const PROPOSAL_TTL_MS = 120_000;
const matched = new Map<string, Set<string>>(); // strategyId -> mints déjà déclenchés
const watchedForPositions = new Set<string>();

export function matchFilters(s: TokenSummary, f: StrategyFilters): { ok: boolean; why: string[] } {
  const why: string[] = [];
  const fail = (c: boolean, label: string) => {
    if (c) why.push(label);
  };
  const age = s.ageSec;
  fail(f.minAgeSec != null && (age == null || age < f.minAgeSec), "âge");
  fail(f.maxAgeSec != null && (age == null || age > f.maxAgeSec), "âge max");
  fail(f.minMcapUsd != null && (s.marketCapUsd == null || s.marketCapUsd < f.minMcapUsd), "mcap min");
  fail(f.maxMcapUsd != null && (s.marketCapUsd == null || s.marketCapUsd > f.maxMcapUsd), "mcap max");
  fail(f.minBuys != null && s.buys < f.minBuys, "achats");
  fail(f.minUniqueBuyers != null && s.uniqueBuyers < f.minUniqueBuyers, "acheteurs uniques");
  fail(f.minVolumeSol != null && s.volumeSol < f.minVolumeSol, "volume");
  fail(f.minBondingPct != null && (s.bondingPct == null || s.bondingPct < f.minBondingPct), "courbe min");
  fail(f.maxBondingPct != null && (s.bondingPct == null || s.bondingPct > f.maxBondingPct), "courbe max");
  fail(f.maxDevPct != null && (s.devPct == null || s.devPct > f.maxDevPct), "dev %");
  fail(f.maxSniperPct != null && (s.sniperPct == null || s.sniperPct > f.maxSniperPct), "snipers %");
  fail(f.maxTop10Pct != null && (s.top10PctSeen == null || s.top10PctSeen > f.maxTop10Pct), "top10 %");
  fail(Boolean(f.requireSocials) && !s.twitter && !s.telegram && !s.website, "réseaux sociaux");
  fail(Boolean(f.requireDevNotSold) && (s.devSoldPct ?? 0) > 0, "dev a vendu");
  fail(f.maxRiskScore != null && assessRisk(s).score > f.maxRiskScore, "score de risque");
  fail(
    Boolean(f.nameIncludes) &&
      !`${s.name} ${s.symbol}`.toLowerCase().includes(String(f.nameIncludes).toLowerCase()),
    "nom",
  );
  return { ok: why.length === 0, why };
}

export function createProposal(p: Omit<Proposal, "id" | "createdAt" | "expiresAt" | "status">): Proposal | null {
  if (db.killSwitch) return null;
  if (db.proposals.some((x) => x.status === "pending" && x.mint === p.mint && x.side === p.side)) return null;
  if (p.side === "buy") {
    const err = checkBuy(p.amountSol ?? 0, "auto", p.mint);
    if (err) {
      alert("system", `Proposition bloquée : $${p.symbol}`, err, p.mint);
      return null;
    }
  }
  const prop: Proposal = { ...p, id: uid("pr_"), createdAt: Date.now(), expiresAt: Date.now() + PROPOSAL_TTL_MS, status: "pending" };
  db.proposals.push(prop);
  save();
  store.watch(prop.mint);
  setTimeout(() => store.unwatch(prop.mint), PROPOSAL_TTL_MS + 5000);
  broadcast("proposal", prop);
  alert(
    p.side === "buy" ? "opportunity" : "risk",
    `Validation requise : ${p.side === "buy" ? "acheter" : "vendre"} $${p.symbol}`,
    `${p.reason} — ${p.side === "buy" ? `${p.amountSol} SOL` : `${p.percent} %`}. Ouvrez le terminal pour examiner et signer (expire dans 2 min).`,
    p.mint,
  );
  return prop;
}

function evaluate(s: TokenSummary) {
  if (db.killSwitch) return;
  for (const st of db.strategies) {
    if (!st.enabled) continue;
    const seen = matched.get(st.id) ?? new Set<string>();
    matched.set(st.id, seen);
    if (seen.has(s.mint)) continue;
    if (!matchFilters(s, st.filters).ok) continue;
    seen.add(s.mint);
    st.matches++;
    save();
    const risk = assessRisk(s);
    const desc = `Stratégie « ${st.name} » : mcap ${s.marketCapUsd ? "$" + Math.round(s.marketCapUsd).toLocaleString("fr-FR") : "?"}, ${s.uniqueBuyers} acheteurs, risque ${risk.score}/100 (${risk.level})`;
    if (st.mode === "alert") alert("opportunity", `Signal : $${s.symbol || s.mint.slice(0, 6)}`, desc, s.mint);
    else
      createProposal({
        source: st.name,
        side: "buy",
        mint: s.mint,
        symbol: s.symbol || s.mint.slice(0, 6),
        amountSol: Math.min(st.buySol, db.limits.maxSolPerTrade),
        slippagePct: st.slippagePct,
        reason: desc,
      });
  }
}

function checkWatchAlerts(s: TokenSummary) {
  for (const w of db.watchAlerts) {
    if (!w.enabled || w.mint !== s.mint) continue;
    let hit = false;
    let body = "";
    if (w.kind === "mcap_above" && s.marketCapUsd != null && w.value != null && s.marketCapUsd >= w.value) {
      hit = true;
      body = `Market cap $${Math.round(s.marketCapUsd).toLocaleString("fr-FR")} ≥ $${w.value.toLocaleString("fr-FR")}`;
    } else if (w.kind === "mcap_below" && s.marketCapUsd != null && w.value != null && s.marketCapUsd <= w.value) {
      hit = true;
      body = `Market cap $${Math.round(s.marketCapUsd).toLocaleString("fr-FR")} ≤ $${w.value.toLocaleString("fr-FR")}`;
    } else if (w.kind === "dev_sold" && (s.devSoldPct ?? 0) > 0) {
      hit = true;
      body = `Le créateur a vendu ${s.devSoldPct!.toFixed(0)} % de ses tokens`;
    } else if (w.kind === "migration" && s.migrated) {
      hit = true;
      body = "Le token a migré vers PumpSwap";
    }
    if (hit) fireWatch(w.id, s.symbol || w.symbol || s.mint.slice(0, 6), body, s.mint, w.kind === "dev_sold" || w.kind === "mcap_below");
  }
}

function fireWatch(id: string, sym: string, body: string, mint: string, isRisk: boolean) {
  const w = db.watchAlerts.find((x) => x.id === id);
  if (!w) return;
  if (w.triggeredAt && Date.now() - w.triggeredAt < 5 * 60_000) return;
  w.triggeredAt = Date.now();
  if (!w.repeat) w.enabled = false;
  save();
  alert(isRisk ? "risk" : "opportunity", `Alerte $${sym}`, body, mint);
}

function onTrade(mint: string, p: TradePoint) {
  for (const w of db.watchAlerts) {
    if (w.enabled && w.mint === mint && w.kind === "big_buy" && p.side === "buy" && p.sol >= (w.value ?? 1)) {
      fireWatch(w.id, w.symbol ?? mint.slice(0, 6), `Gros achat : ${p.sol.toFixed(2)} SOL par ${p.trader.slice(0, 6)}…`, mint, false);
    }
  }
  // Alerte immédiate si un token détenu voit son créateur vendre.
  const t = store.tokens.get(mint);
  if (t?.creator && p.trader === t.creator && p.side === "sell" && ledger().positions.some((x) => x.mint === mint)) {
    alert("risk", `Le dev vend $${t.symbol}`, `Le créateur vient de vendre ${p.sol.toFixed(2)} SOL d'un token que vous détenez.`, mint);
  }
}

const priceCache = new Map<string, { at: number; priceSol: number | null }>();

async function positionPrice(mint: string): Promise<number | null> {
  const t = store.tokens.get(mint);
  if (t?.priceSol != null && Date.now() - t.lastTradeAt < 120_000) return t.priceSol;
  const c = priceCache.get(mint);
  if (c && Date.now() - c.at < 20_000) return c.priceSol;
  const d = await dexToken(mint).catch(() => null);
  const priceSol = d?.priceNative ?? (d?.priceUsd && solPrice.usd ? d.priceUsd / solPrice.usd : null) ?? t?.priceSol ?? null;
  priceCache.set(mint, { at: Date.now(), priceSol });
  return priceSol;
}

async function checkExits() {
  const { positions } = ledger();
  for (const p of positions) {
    if (!watchedForPositions.has(p.mint)) {
      watchedForPositions.add(p.mint);
      store.watch(p.mint);
    }
  }
  if (!db.exits.enabled || db.killSwitch) return;
  for (const p of positions) {
    const price = await positionPrice(p.mint);
    if (price == null || p.costSol <= 0) continue;
    const pnlPct = ((price * p.tokens) / p.costSol - 1) * 100;
    const sym = store.tokens.get(p.mint)?.symbol || p.mint.slice(0, 6);
    if (pnlPct >= db.exits.takeProfitPct || pnlPct <= -db.exits.stopLossPct) {
      createProposal({
        source: pnlPct > 0 ? "Take-profit" : "Stop-loss",
        side: "sell",
        mint: p.mint,
        symbol: sym,
        percent: db.exits.sellPercent,
        slippagePct: 15,
        reason: `${pnlPct > 0 ? "Objectif de gain" : "Stop-loss"} atteint : ${pnlPct >= 0 ? "+" : ""}${pnlPct.toFixed(1)} % (valeur estimée ${(price * p.tokens).toFixed(4)} SOL pour un coût de ${p.costSol.toFixed(4)} SOL)`,
      });
    }
  }
}

function expireProposals() {
  const now = Date.now();
  let changed = false;
  for (const p of db.proposals) {
    if (p.status === "pending" && p.expiresAt < now) {
      p.status = "expired";
      changed = true;
      broadcast("proposal:update", p);
    }
  }
  if (changed) save();
}

export function setKillSwitch(on: boolean) {
  db.killSwitch = on;
  if (on) {
    for (const p of db.proposals) if (p.status === "pending") p.status = "rejected";
    for (const s of db.strategies) s.enabled = false;
  }
  save();
  broadcast("killswitch", on);
  alert("system", on ? "ARRÊT D'URGENCE ACTIVÉ" : "Arrêt d'urgence levé", on
    ? "Toutes les stratégies sont désactivées et les propositions en attente annulées."
    : "Les automatisations peuvent être réactivées manuellement.");
}

export function resetStrategyMatches(id: string) {
  matched.delete(id);
}

export function upsertStrategy(input: Partial<Strategy> & { name: string }): Strategy {
  const existing = input.id ? db.strategies.find((s) => s.id === input.id) : undefined;
  const clean: Strategy = {
    id: existing?.id ?? uid("st_"),
    name: String(input.name).slice(0, 60),
    enabled: Boolean(input.enabled) && !db.killSwitch,
    mode: input.mode === "propose" ? "propose" : "alert",
    filters: input.filters ?? {},
    buySol: Math.max(0.001, Number(input.buySol ?? 0.05)),
    slippagePct: Math.min(50, Math.max(0.5, Number(input.slippagePct ?? 15))),
    createdAt: existing?.createdAt ?? Date.now(),
    matches: existing?.matches ?? 0,
  };
  if (existing) Object.assign(existing, clean);
  else db.strategies.push(clean);
  save();
  return clean;
}

export function startAutomation() {
  store.on("new", (s: TokenSummary) => evaluate(s));
  store.on("updates", (list: TokenSummary[]) => {
    for (const s of list) {
      evaluate(s);
      checkWatchAlerts(s);
    }
  });
  store.on("migration", (s: TokenSummary) => checkWatchAlerts(s));
  store.on("trade", onTrade);
  setInterval(() => checkExits().catch(() => {}), 5000).unref();
  setInterval(expireProposals, 5000).unref();
}
