import { db } from "./db.js";
import { dayKey } from "./util.js";

export interface Position {
  mint: string;
  tokens: number;
  costSol: number;
  avgCostSol: number;
  openedAt: number;
}

/** Positions et P&L réalisé du jour, reconstruits à partir des transactions confirmées enregistrées par le terminal. */
export function ledger(wallet = db.settings.wallet) {
  const today = dayKey();
  const pos = new Map<string, Position>();
  let spentToday = 0;
  let realizedToday = 0;
  const trades = db.trades.filter((t) => !wallet || t.wallet === wallet).sort((a, b) => a.time - b.time);
  for (const t of trades) {
    const p = pos.get(t.mint) ?? { mint: t.mint, tokens: 0, costSol: 0, avgCostSol: 0, openedAt: t.time };
    if (t.side === "buy") {
      if (p.tokens <= 0) p.openedAt = t.time;
      p.tokens += t.tokenDelta;
      p.costSol += -t.solDelta;
      if (dayKey(t.time) === today) spentToday += -t.solDelta;
    } else {
      const qty = Math.min(Math.abs(t.tokenDelta), p.tokens);
      const basis = p.tokens > 0 ? (p.costSol / p.tokens) * qty : 0;
      if (dayKey(t.time) === today) realizedToday += t.solDelta - basis;
      p.tokens -= qty;
      p.costSol -= basis;
    }
    p.avgCostSol = p.tokens > 0 ? p.costSol / p.tokens : 0;
    pos.set(t.mint, p);
  }
  const open = [...pos.values()].filter((p) => p.tokens > 1e-9);
  return { positions: open, spentToday, realizedToday };
}

export function limitsStatus() {
  const l = ledger();
  return {
    ...db.limits,
    killSwitch: db.killSwitch,
    spentToday: l.spentToday,
    realizedToday: l.realizedToday,
    openPositions: l.positions.length,
  };
}

/** Retourne un message d'erreur si l'achat viole une limite, sinon null. */
export function checkBuy(amountSol: number, source: "manual" | "auto", mint?: string): string | null {
  if (source === "auto" && db.killSwitch) return "Arrêt d'urgence activé : les automatisations sont stoppées.";
  if (source === "manual" && !db.limits.applyToManual) return null;
  const lim = db.limits;
  const l = ledger();
  if (amountSol > lim.maxSolPerTrade) return `Montant ${amountSol} SOL > limite par transaction (${lim.maxSolPerTrade} SOL).`;
  if (l.spentToday + amountSol > lim.maxDailySpendSol)
    return `Limite de dépense quotidienne atteinte (${l.spentToday.toFixed(3)} / ${lim.maxDailySpendSol} SOL).`;
  if (-l.realizedToday >= lim.maxDailyLossSol)
    return `Limite de perte quotidienne atteinte (perte réalisée ${(-l.realizedToday).toFixed(3)} SOL).`;
  const alreadyOpen = mint ? l.positions.some((p) => p.mint === mint) : false;
  if (!alreadyOpen && l.positions.length >= lim.maxOpenPositions) return `Nombre maximum de positions ouvertes atteint (${lim.maxOpenPositions}).`;
  return null;
}
