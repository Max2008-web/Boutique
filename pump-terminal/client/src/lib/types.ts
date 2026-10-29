export interface TokenSummary {
  mint: string;
  name: string;
  symbol: string;
  image?: string;
  twitter?: string;
  telegram?: string;
  website?: string;
  creator?: string;
  createdAt: number | null;
  firstSeenAt: number;
  ageSec: number | null;
  pool: string;
  migrated: boolean;
  priceSol: number | null;
  priceUsd: number | null;
  marketCapSol: number | null;
  marketCapUsd: number | null;
  liquiditySol: number | null;
  bondingPct: number | null;
  buys: number;
  sells: number;
  volumeSol: number;
  buyVolumeSol: number;
  sellVolumeSol: number;
  uniqueBuyers: number;
  uniqueSellers: number;
  holdersSeen: number;
  top10PctSeen: number | null;
  devPct: number | null;
  devSoldPct: number | null;
  sniperPct: number | null;
  sniperCount: number;
  lastTradeAt: number;
  tradesTracked: number;
  fromStream: boolean;
}

export interface TradePoint {
  mint?: string;
  t: number;
  sig: string;
  side: "buy" | "sell";
  sol: number;
  tokens: number;
  trader: string;
  priceSol: number;
  priceUsd: number | null;
  mcapSol: number | null;
}

export interface RiskFlag {
  level: "high" | "medium" | "low" | "ok";
  code: string;
  label: string;
}

export interface RiskReport {
  score: number;
  level: string;
  flags: RiskFlag[];
  metrics: Record<string, number | string | boolean | null>;
}

export interface Holder {
  account: string;
  owner: string | null;
  amount: number;
  pct: number;
  isProgram: boolean;
}

export interface DexInfo {
  mint: string;
  name: string;
  symbol: string;
  pairAddress: string;
  dexId: string;
  priceUsd: number | null;
  priceNative: number | null;
  marketCapUsd: number | null;
  liquidityUsd: number | null;
  volume: { m5?: number; h1?: number; h6?: number; h24?: number };
  txns: Record<string, { buys: number; sells: number }>;
  priceChange: Record<string, number>;
  imageUrl?: string;
  pairCreatedAt?: number;
  url: string;
}

export interface TokenDetail {
  summary: TokenSummary;
  description?: string;
  dex: DexInfo | null;
  mint: { decimals: number; supply: number; mintAuthority: string | null; freezeAuthority: string | null; program: string; extensions: string[]; error?: string } | null;
  holders: Holder[] | null;
  risk: RiskReport;
  solUsd: number | null;
}

export interface Candle {
  time: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
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

export interface WalletToken {
  mint: string;
  account: string;
  amount: number;
  decimals: number;
  name: string;
  symbol: string;
  image?: string;
  priceUsd: number | null;
  priceSol: number | null;
  valueUsd: number | null;
  valueSol: number | null;
}

export interface ParsedTx {
  signature: string;
  time: number | null;
  err: boolean;
  feeSol: number;
  solDelta: number;
  tokenDeltas: Record<string, number>;
  kind: "buy" | "sell" | "transfer" | "other";
  mint?: string;
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

export interface AlertItem {
  id: string;
  at: number;
  level: "info" | "opportunity" | "risk" | "trade" | "system";
  title: string;
  body: string;
  mint?: string;
}

export interface Proposal {
  id: string;
  createdAt: number;
  expiresAt: number;
  source: string;
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
  value?: number;
  enabled: boolean;
  repeat: boolean;
  triggeredAt?: number;
}

export interface LimitsStatus {
  maxSolPerTrade: number;
  maxDailySpendSol: number;
  maxDailyLossSol: number;
  maxOpenPositions: number;
  applyToManual: boolean;
  killSwitch: boolean;
  spentToday: number;
  realizedToday: number;
  openPositions: number;
}

export interface Exits {
  enabled: boolean;
  takeProfitPct: number;
  stopLossPct: number;
  sellPercent: number;
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

export interface ServerStatus {
  feed: { connected: boolean; lastMessageAt: number; messages: number; tokenSubscriptions: number; lastError: string; pumpSwapData: boolean };
  tokensTracked: number;
  solUsd: number | null;
  solPriceSource?: string;
  rpcConfigured?: boolean;
  ai?: { enabled: boolean; model: string; autoScanMinutes: number };
  telegram?: boolean;
  jupiter?: { base: string; apiKey: boolean };
  pumpPortalFeePct?: number;
  killSwitch: boolean;
}
