import fs from "node:fs";
import path from "node:path";
import { config } from "./config.js";

export interface Limits {
  maxSolPerTrade: number;
  maxDailySpendSol: number;
  maxDailyLossSol: number;
  maxOpenPositions: number;
  /** Appliquer aussi les limites aux transactions manuelles. */
  applyToManual: boolean;
}

export interface StrategyFilters {
  minAgeSec?: number;
  maxAgeSec?: number;
  minMcapUsd?: number;
  maxMcapUsd?: number;
  minBuys?: number;
  minUniqueBuyers?: number;
  minVolumeSol?: number;
  minBondingPct?: number;
  maxBondingPct?: number;
  maxDevPct?: number;
  maxSniperPct?: number;
  maxTop10Pct?: number;
  requireSocials?: boolean;
  requireDevNotSold?: boolean;
  maxRiskScore?: number;
  nameIncludes?: string;
}

export interface Strategy {
  id: string;
  name: string;
  enabled: boolean;
  /** alert = notification seulement ; propose = proposition d'achat à valider et signer. */
  mode: "alert" | "propose";
  filters: StrategyFilters;
  buySol: number;
  slippagePct: number;
  createdAt: number;
  matches: number;
}

export interface WatchAlert {
  id: string;
  mint: string;
  symbol?: string;
  kind: "mcap_above" | "mcap_below" | "dev_sold" | "big_buy" | "migration";
  value?: number; // USD pour mcap, SOL pour big_buy
  enabled: boolean;
  triggeredAt?: number;
  repeat: boolean;
}

export interface Exits {
  enabled: boolean;
  takeProfitPct: number;
  stopLossPct: number;
  sellPercent: number;
}

export interface Proposal {
  id: string;
  createdAt: number;
  expiresAt: number;
  source: string; // nom de la stratégie / règle
  side: "buy" | "sell";
  mint: string;
  symbol: string;
  amountSol?: number;
  percent?: number;
  slippagePct: number;
  reason: string;
  status: "pending" | "rejected" | "expired" | "executed" | "failed";
  signature?: string;
}

export interface RecordedTrade {
  signature: string;
  wallet: string;
  mint: string;
  side: "buy" | "sell";
  solDelta: number;
  tokenDelta: number;
  time: number;
  source: string;
  proposalId?: string;
}

export interface AlertItem {
  id: string;
  at: number;
  level: "info" | "opportunity" | "risk" | "trade" | "system";
  title: string;
  body: string;
  mint?: string;
}

export interface AiResult {
  id: string;
  at: number;
  kind: "token" | "market";
  mint?: string;
  symbol?: string;
  model: string;
  usedXSearch: boolean;
  result: any;
  citations: string[];
}

export interface Db {
  settings: { wallet: string | null };
  limits: Limits;
  killSwitch: boolean;
  exits: Exits;
  strategies: Strategy[];
  watchAlerts: WatchAlert[];
  proposals: Proposal[];
  trades: RecordedTrade[];
  alerts: AlertItem[];
  aiHistory: AiResult[];
}

const defaults: Db = {
  settings: { wallet: null },
  limits: { maxSolPerTrade: 0.25, maxDailySpendSol: 1, maxDailyLossSol: 0.5, maxOpenPositions: 5, applyToManual: true },
  killSwitch: false,
  exits: { enabled: false, takeProfitPct: 100, stopLossPct: 35, sellPercent: 100 },
  strategies: [],
  watchAlerts: [],
  proposals: [],
  trades: [],
  alerts: [],
  aiHistory: [],
};

const file = path.join(config.dataDir, "db.json");
let timer: NodeJS.Timeout | null = null;

function load(): Db {
  try {
    const raw = JSON.parse(fs.readFileSync(file, "utf8"));
    return { ...defaults, ...raw, limits: { ...defaults.limits, ...raw.limits }, exits: { ...defaults.exits, ...raw.exits } };
  } catch {
    return structuredClone(defaults);
  }
}

export const db: Db = load();

export function save() {
  if (timer) return;
  timer = setTimeout(() => {
    timer = null;
    db.proposals = db.proposals.slice(-300);
    db.alerts = db.alerts.slice(-500);
    db.aiHistory = db.aiHistory.slice(-100);
    db.trades = db.trades.slice(-5000);
    fs.mkdirSync(config.dataDir, { recursive: true });
    const tmp = file + ".tmp";
    fs.writeFileSync(tmp, JSON.stringify(db, null, 2));
    fs.renameSync(tmp, file);
  }, 400);
}
