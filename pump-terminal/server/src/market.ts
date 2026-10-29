import { fetchJson, TtlCache } from "./util.js";

/** Données de marché DexScreener (API publique gratuite, ~300 req/min). */
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
  txns: { m5?: { buys: number; sells: number }; h1?: { buys: number; sells: number }; h24?: { buys: number; sells: number } };
  priceChange: { m5?: number; h1?: number; h6?: number; h24?: number };
  imageUrl?: string;
  websites: string[];
  socials: { type: string; url: string }[];
  pairCreatedAt?: number;
  url: string;
}

const dexCache = new TtlCache<DexInfo | null>(15_000);

function mapPair(p: any): DexInfo {
  return {
    mint: p.baseToken?.address,
    name: p.baseToken?.name ?? "",
    symbol: p.baseToken?.symbol ?? "",
    pairAddress: p.pairAddress,
    dexId: p.dexId,
    priceUsd: p.priceUsd != null ? Number(p.priceUsd) : null,
    priceNative: p.priceNative != null ? Number(p.priceNative) : null,
    marketCapUsd: p.marketCap ?? p.fdv ?? null,
    liquidityUsd: p.liquidity?.usd ?? null,
    volume: p.volume ?? {},
    txns: p.txns ?? {},
    priceChange: p.priceChange ?? {},
    imageUrl: p.info?.imageUrl,
    websites: (p.info?.websites ?? []).map((w: any) => w.url),
    socials: p.info?.socials ?? [],
    pairCreatedAt: p.pairCreatedAt,
    url: p.url,
  };
}

/** Retourne, pour chaque mint, la paire la plus liquide. */
export async function dexTokens(mints: string[]): Promise<Map<string, DexInfo>> {
  const out = new Map<string, DexInfo>();
  const unique = [...new Set(mints)];
  for (let i = 0; i < unique.length; i += 30) {
    const chunk = unique.slice(i, i + 30);
    const pairs = await fetchJson<any[]>(`https://api.dexscreener.com/tokens/v1/solana/${chunk.join(",")}`, {
      timeoutMs: 8000,
    });
    for (const p of pairs ?? []) {
      const mint = p.baseToken?.address;
      if (!mint || !chunk.includes(mint)) continue;
      const prev = out.get(mint);
      if (!prev || (p.liquidity?.usd ?? 0) > (prev.liquidityUsd ?? 0)) out.set(mint, mapPair(p));
    }
  }
  return out;
}

export async function dexToken(mint: string): Promise<DexInfo | null> {
  return dexCache.get(mint, async () => (await dexTokens([mint])).get(mint) ?? null);
}

/** Bougies historiques GeckoTerminal (API publique gratuite, ~30 req/min). */
export interface Candle {
  time: number; // secondes unix
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

const poolCache = new TtlCache<string | null>(10 * 60_000);
const ohlcvCache = new TtlCache<Candle[]>(20_000);

async function geckoPool(mint: string): Promise<string | null> {
  return poolCache.get(mint, async () => {
    const data = await fetchJson<any>(`https://api.geckoterminal.com/api/v2/networks/solana/tokens/${mint}/pools?page=1`, {
      headers: { accept: "application/json" },
      timeoutMs: 8000,
    });
    const pool = data?.data?.[0]?.attributes?.address;
    return pool ?? null;
  });
}

export async function geckoCandles(mint: string, tfSec: number): Promise<Candle[]> {
  const pool = await geckoPool(mint);
  if (!pool) return [];
  let timeframe = "minute";
  let aggregate = 1;
  if (tfSec >= 86400) timeframe = "day";
  else if (tfSec >= 3600) {
    timeframe = "hour";
    aggregate = [1, 4, 12].includes(tfSec / 3600) ? tfSec / 3600 : 1;
  } else aggregate = [1, 5, 15].includes(tfSec / 60) ? Math.max(1, tfSec / 60) : 1;
  const key = `${pool}:${timeframe}:${aggregate}`;
  return ohlcvCache.get(key, async () => {
    const data = await fetchJson<any>(
      `https://api.geckoterminal.com/api/v2/networks/solana/pools/${pool}/ohlcv/${timeframe}?aggregate=${aggregate}&limit=500&currency=usd&token=base`,
      { headers: { accept: "application/json" }, timeoutMs: 10_000 },
    );
    const list: number[][] = data?.data?.attributes?.ohlcv_list ?? [];
    return list
      .map(([time, open, high, low, close, volume]) => ({ time, open, high, low, close, volume }))
      .sort((a, b) => a.time - b.time);
  });
}
