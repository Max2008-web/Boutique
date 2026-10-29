import { EventEmitter } from "node:events";
import { config } from "./config.js";
import { feed, PumpCreate, PumpTrade } from "./pumpportal.js";
import { solPrice } from "./prices.js";
import { Candle, dexToken } from "./market.js";
import { fetchJson } from "./util.js";

/**
 * Paramètres standards de la courbe de liaison pump.fun (bonding curve) :
 * réserves virtuelles initiales 1 073 000 000 tokens / 30 SOL,
 * 793 100 000 tokens vendables sur la courbe, offre totale 1 000 000 000 (6 décimales).
 */
const INITIAL_VIRTUAL_TOKENS = 1_073_000_000;
const CURVE_SELLABLE_TOKENS = 793_100_000;
const INITIAL_VIRTUAL_SOL = 30;
export const PUMP_TOTAL_SUPPLY = 1_000_000_000;
/** Achats non-dev survenus dans cette fenêtre après la création = snipers / bundles probables. */
const SNIPER_WINDOW_MS = 3_000;

export interface TradePoint {
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

export interface TokenState {
  mint: string;
  name: string;
  symbol: string;
  uri?: string;
  image?: string;
  description?: string;
  twitter?: string;
  telegram?: string;
  website?: string;
  creator?: string;
  createdAt: number | null; // ms ; null si le token n'a pas été vu à sa création
  firstSeenAt: number;
  createSignature?: string;
  bondingCurveKey?: string;
  pool: string;
  migrated: boolean;
  migratedAt?: number;
  vSol: number | null;
  vTokens: number | null;
  marketCapSol: number | null;
  priceSol: number | null;
  buys: number;
  sells: number;
  volumeSol: number;
  buyVolumeSol: number;
  sellVolumeSol: number;
  buyers: Set<string>;
  sellers: Set<string>;
  holders: Map<string, number>;
  devInitialTokens: number;
  devTokens: number;
  devSoldTokens: number;
  sniperTokens: number;
  snipers: Set<string>;
  bigBuys: number; // achats >= 1 SOL
  trades: TradePoint[];
  lastTradeAt: number;
  watchers: number;
  fromStream: boolean;
  metaLoaded: boolean;
}

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

class TokenStore extends EventEmitter {
  tokens = new Map<string, TokenState>();
  private dirty = new Set<string>();
  private metaQueue: string[] = [];
  private metaActive = 0;

  start() {
    feed.on("create", (m: PumpCreate) => this.onCreate(m));
    feed.on("trade", (m: PumpTrade) => this.onTrade(m));
    feed.on("migration", (m: any) => this.onMigration(m));
    setInterval(() => this.flush(), 1000).unref();
    setInterval(() => this.evict(), 60_000).unref();
  }

  private blank(mint: string): TokenState {
    return {
      mint,
      name: "",
      symbol: "",
      createdAt: null,
      firstSeenAt: Date.now(),
      pool: "pump",
      migrated: false,
      vSol: null,
      vTokens: null,
      marketCapSol: null,
      priceSol: null,
      buys: 0,
      sells: 0,
      volumeSol: 0,
      buyVolumeSol: 0,
      sellVolumeSol: 0,
      buyers: new Set(),
      sellers: new Set(),
      holders: new Map(),
      devInitialTokens: 0,
      devTokens: 0,
      devSoldTokens: 0,
      sniperTokens: 0,
      snipers: new Set(),
      bigBuys: 0,
      trades: [],
      lastTradeAt: 0,
      watchers: 0,
      fromStream: false,
      metaLoaded: false,
    };
  }

  private onCreate(m: PumpCreate) {
    const t = this.tokens.get(m.mint) ?? this.blank(m.mint);
    t.name = m.name ?? t.name;
    t.symbol = m.symbol ?? t.symbol;
    t.uri = m.uri;
    t.creator = m.traderPublicKey;
    t.createdAt = Date.now();
    t.createSignature = m.signature;
    t.bondingCurveKey = m.bondingCurveKey;
    t.pool = m.pool ?? "pump";
    t.fromStream = true;
    this.applyCurve(t, m.vSolInBondingCurve, m.vTokensInBondingCurve, m.marketCapSol);
    if (m.initialBuy && m.initialBuy > 0) {
      t.devInitialTokens = m.initialBuy;
      t.devTokens = m.initialBuy;
      t.holders.set(m.traderPublicKey, m.initialBuy);
      if (m.solAmount) {
        t.buys++;
        t.volumeSol += m.solAmount;
        t.buyVolumeSol += m.solAmount;
        t.buyers.add(m.traderPublicKey);
        this.pushTrade(t, {
          t: Date.now(),
          sig: m.signature,
          side: "buy",
          sol: m.solAmount,
          tokens: m.initialBuy,
          trader: m.traderPublicKey,
        });
      }
    }
    this.tokens.set(m.mint, t);
    feed.subscribeTokens([m.mint]);
    this.queueMeta(m.mint);
    this.emit("new", this.summary(t));
  }

  private applyCurve(t: TokenState, vSol?: number, vTokens?: number, mcapSol?: number) {
    if (vSol && vTokens) {
      t.vSol = vSol;
      t.vTokens = vTokens;
      t.priceSol = vSol / vTokens;
    }
    if (mcapSol) t.marketCapSol = mcapSol;
    else if (t.priceSol) t.marketCapSol = t.priceSol * PUMP_TOTAL_SUPPLY;
  }

  private pushTrade(t: TokenState, p: Omit<TradePoint, "priceSol" | "priceUsd" | "mcapSol">) {
    const priceSol = t.priceSol ?? (p.tokens > 0 ? p.sol / p.tokens : 0);
    const point: TradePoint = {
      ...p,
      priceSol,
      priceUsd: solPrice.usd ? priceSol * solPrice.usd : null,
      mcapSol: t.marketCapSol,
    };
    t.trades.push(point);
    if (t.trades.length > 6000) t.trades.splice(0, t.trades.length - 5000);
    t.lastTradeAt = p.t;
    this.emit("trade", t.mint, point);
  }

  private onTrade(m: PumpTrade) {
    let t = this.tokens.get(m.mint);
    if (!t) {
      t = this.blank(m.mint);
      this.tokens.set(m.mint, t);
      this.queueMeta(m.mint);
    }
    const now = Date.now();
    const sol = Number(m.solAmount ?? 0);
    const tokens = Number(m.tokenAmount ?? 0);
    if (m.pool) {
      if (m.pool !== "pump" && !t.migrated && t.pool === "pump") t.migrated = true;
      t.pool = m.pool;
    }
    if (m.bondingCurveKey) t.bondingCurveKey = m.bondingCurveKey;
    if (m.vSolInBondingCurve && m.vTokensInBondingCurve) {
      this.applyCurve(t, m.vSolInBondingCurve, m.vTokensInBondingCurve, m.marketCapSol);
    } else {
      if (sol > 0 && tokens > 0) t.priceSol = sol / tokens;
      if (m.marketCapSol) t.marketCapSol = m.marketCapSol;
      else if (t.priceSol) t.marketCapSol = t.priceSol * PUMP_TOTAL_SUPPLY;
    }
    t.volumeSol += sol;
    if (m.txType === "buy") {
      t.buys++;
      t.buyVolumeSol += sol;
      t.buyers.add(m.traderPublicKey);
      if (sol >= 1) t.bigBuys++;
      if (
        t.createdAt &&
        now - t.createdAt <= SNIPER_WINDOW_MS &&
        m.traderPublicKey !== t.creator
      ) {
        t.sniperTokens += tokens;
        t.snipers.add(m.traderPublicKey);
      }
    } else {
      t.sells++;
      t.sellVolumeSol += sol;
      t.sellers.add(m.traderPublicKey);
    }
    if (m.newTokenBalance !== undefined) {
      if (m.newTokenBalance > 0) t.holders.set(m.traderPublicKey, m.newTokenBalance);
      else t.holders.delete(m.traderPublicKey);
    }
    if (t.creator && m.traderPublicKey === t.creator) {
      if (m.txType === "sell") t.devSoldTokens += tokens;
      if (m.newTokenBalance !== undefined) t.devTokens = m.newTokenBalance;
    }
    this.pushTrade(t, { t: now, sig: m.signature, side: m.txType, sol, tokens, trader: m.traderPublicKey });
    this.dirty.add(t.mint);
  }

  private onMigration(m: any) {
    const t = this.tokens.get(m.mint) ?? this.blank(m.mint);
    t.migrated = true;
    t.migratedAt = Date.now();
    if (m.pool) t.pool = m.pool;
    else if (t.pool === "pump") t.pool = "pump-amm";
    this.tokens.set(m.mint, t);
    if (!t.metaLoaded) this.queueMeta(m.mint);
    this.dirty.add(m.mint);
    this.emit("migration", this.summary(t));
  }

  private flush() {
    if (!this.dirty.size) return;
    const list: TokenSummary[] = [];
    for (const mint of this.dirty) {
      const t = this.tokens.get(mint);
      if (t) list.push(this.summary(t));
    }
    this.dirty.clear();
    this.emit("updates", list);
  }

  private evict() {
    const now = Date.now();
    const ttl = config.trackMinutes * 60_000;
    const stale: string[] = [];
    for (const t of this.tokens.values()) {
      if (t.watchers > 0) continue;
      const last = Math.max(t.lastTradeAt, t.createdAt ?? 0, t.firstSeenAt);
      if (now - last > ttl) stale.push(t.mint);
    }
    // Si trop de tokens suivis, on retire d'abord les plus inactifs.
    const active = [...this.tokens.values()].filter((t) => t.watchers === 0 && !stale.includes(t.mint));
    const excess = this.tokens.size - stale.length - config.maxTracked;
    if (excess > 0) {
      active
        .sort((a, b) => a.lastTradeAt - b.lastTradeAt)
        .slice(0, excess)
        .forEach((t) => stale.push(t.mint));
    }
    if (!stale.length) return;
    feed.unsubscribeTokens(stale);
    for (const m of stale) this.tokens.delete(m);
  }

  /** Charge les métadonnées (image, réseaux sociaux) depuis l'URI on-chain du token. */
  private queueMeta(mint: string) {
    this.metaQueue.push(mint);
    this.pumpMeta();
  }

  private pumpMeta() {
    while (this.metaActive < 6 && this.metaQueue.length) {
      const mint = this.metaQueue.shift()!;
      const t = this.tokens.get(mint);
      if (!t || t.metaLoaded) continue;
      this.metaActive++;
      this.loadMeta(t)
        .catch(() => {})
        .finally(() => {
          t.metaLoaded = true;
          this.metaActive--;
          this.dirty.add(mint);
          this.pumpMeta();
        });
    }
  }

  private async loadMeta(t: TokenState) {
    if (t.uri) {
      const uri = t.uri.replace(/^ipfs:\/\//, "https://ipfs.io/ipfs/");
      const meta = await fetchJson<any>(uri, { timeoutMs: 6000 });
      t.image = typeof meta?.image === "string" ? meta.image : undefined;
      t.description = typeof meta?.description === "string" ? meta.description.slice(0, 500) : undefined;
      t.twitter = meta?.twitter || meta?.extensions?.twitter;
      t.telegram = meta?.telegram || meta?.extensions?.telegram;
      t.website = meta?.website || meta?.extensions?.website;
      return;
    }
    const d = await dexToken(t.mint);
    if (d) {
      t.name ||= d.name;
      t.symbol ||= d.symbol;
      t.image ||= d.imageUrl;
      t.website ||= d.websites[0];
      t.twitter ||= d.socials.find((s) => s.type === "twitter")?.url;
      t.telegram ||= d.socials.find((s) => s.type === "telegram")?.url;
      if (t.priceSol == null && d.priceNative) {
        t.priceSol = d.priceNative;
        t.marketCapSol = d.marketCapUsd && solPrice.usd ? d.marketCapUsd / solPrice.usd : null;
      }
      if (d.dexId && d.dexId !== "pumpfun") t.migrated = true;
    }
  }

  /** Suivi explicite d'un token (page ouverte, position détenue, proposition en cours). */
  watch(mint: string): TokenState {
    let t = this.tokens.get(mint);
    if (!t) {
      t = this.blank(mint);
      this.tokens.set(mint, t);
      this.queueMeta(mint);
    }
    t.watchers++;
    feed.subscribeTokens([mint]);
    return t;
  }

  unwatch(mint: string) {
    const t = this.tokens.get(mint);
    if (t && t.watchers > 0) t.watchers--;
  }

  ensure(mint: string): TokenState {
    let t = this.tokens.get(mint);
    if (!t) {
      t = this.blank(mint);
      this.tokens.set(mint, t);
      this.queueMeta(mint);
      feed.subscribeTokens([mint]);
    }
    return t;
  }

  summary(t: TokenState): TokenSummary {
    const now = Date.now();
    const usd = solPrice.usd;
    const holders = [...t.holders.entries()]
      .filter(([addr]) => addr !== t.bondingCurveKey)
      .map(([, v]) => v)
      .sort((a, b) => b - a);
    const top10 = holders.slice(0, 10).reduce((s, v) => s + v, 0);
    const onCurve = t.pool === "pump" && !t.migrated && t.vTokens != null;
    const bondingPct = onCurve
      ? Math.max(0, Math.min(100, ((INITIAL_VIRTUAL_TOKENS - t.vTokens!) / CURVE_SELLABLE_TOKENS) * 100))
      : t.migrated
        ? 100
        : null;
    return {
      mint: t.mint,
      name: t.name,
      symbol: t.symbol,
      image: t.image,
      twitter: t.twitter,
      telegram: t.telegram,
      website: t.website,
      creator: t.creator,
      createdAt: t.createdAt,
      firstSeenAt: t.firstSeenAt,
      ageSec: t.createdAt ? Math.round((now - t.createdAt) / 1000) : null,
      pool: t.pool,
      migrated: t.migrated,
      priceSol: t.priceSol,
      priceUsd: t.priceSol != null && usd ? t.priceSol * usd : null,
      marketCapSol: t.marketCapSol,
      marketCapUsd: t.marketCapSol != null && usd ? t.marketCapSol * usd : null,
      liquiditySol: onCurve && t.vSol != null ? Math.max(0, t.vSol - INITIAL_VIRTUAL_SOL) : null,
      bondingPct,
      buys: t.buys,
      sells: t.sells,
      volumeSol: t.volumeSol,
      buyVolumeSol: t.buyVolumeSol,
      sellVolumeSol: t.sellVolumeSol,
      uniqueBuyers: t.buyers.size,
      uniqueSellers: t.sellers.size,
      holdersSeen: holders.length,
      top10PctSeen: t.fromStream && holders.length ? (top10 / PUMP_TOTAL_SUPPLY) * 100 : null,
      devPct: t.fromStream ? (t.devTokens / PUMP_TOTAL_SUPPLY) * 100 : null,
      devSoldPct: t.fromStream && t.devInitialTokens > 0 ? Math.min(100, (t.devSoldTokens / t.devInitialTokens) * 100) : null,
      sniperPct: t.fromStream ? (t.sniperTokens / PUMP_TOTAL_SUPPLY) * 100 : null,
      sniperCount: t.snipers.size,
      lastTradeAt: t.lastTradeAt,
      tradesTracked: t.trades.length,
      fromStream: t.fromStream,
    };
  }

  list(kind: "new" | "graduating" | "migrated" | "active", limit = 60): TokenSummary[] {
    const all = [...this.tokens.values()];
    let sel: TokenState[];
    if (kind === "new") {
      sel = all.filter((t) => t.createdAt && !t.migrated).sort((a, b) => b.createdAt! - a.createdAt!);
    } else if (kind === "graduating") {
      sel = all
        .filter((t) => !t.migrated && t.vTokens != null)
        .map((t) => ({ t, pct: (INITIAL_VIRTUAL_TOKENS - t.vTokens!) / CURVE_SELLABLE_TOKENS }))
        .filter((x) => x.pct >= 0.5)
        .sort((a, b) => b.pct - a.pct)
        .map((x) => x.t);
    } else if (kind === "migrated") {
      sel = all.filter((t) => t.migrated).sort((a, b) => (b.migratedAt ?? b.lastTradeAt) - (a.migratedAt ?? a.lastTradeAt));
    } else {
      const since = Date.now() - 5 * 60_000;
      sel = all
        .map((t) => ({ t, v: t.trades.filter((p) => p.t >= since).reduce((s, p) => s + p.sol, 0) }))
        .filter((x) => x.v > 0)
        .sort((a, b) => b.v - a.v)
        .map((x) => x.t);
    }
    return sel.slice(0, limit).map((t) => this.summary(t));
  }

  /** Bougies construites à partir des trades réellement observés dans le flux. */
  candles(mint: string, tfSec: number, unit: "usd" | "sol" = "usd"): Candle[] {
    const t = this.tokens.get(mint);
    if (!t) return [];
    const out: Candle[] = [];
    let cur: Candle | null = null;
    for (const p of t.trades) {
      const price = unit === "usd" ? p.priceUsd : p.priceSol;
      if (price == null || !(price > 0)) continue;
      const bucket = Math.floor(p.t / 1000 / tfSec) * tfSec;
      const vol = unit === "usd" && solPrice.usd ? p.sol * solPrice.usd : p.sol;
      if (!cur || cur.time !== bucket) {
        const open: number = cur ? cur.close : price;
        cur = { time: bucket, open, high: Math.max(open, price), low: Math.min(open, price), close: price, volume: vol };
        out.push(cur);
      } else {
        cur.high = Math.max(cur.high, price);
        cur.low = Math.min(cur.low, price);
        cur.close = price;
        cur.volume += vol;
      }
    }
    return out;
  }
}

export const store = new TokenStore();
