import { config, SOL_MINT } from "./config.js";
import { errMsg, fetchJson } from "./util.js";

export const solPrice = {
  usd: null as number | null,
  source: "",
  updatedAt: 0,
  error: "",
};

async function fromJupiter(): Promise<number> {
  const headers: Record<string, string> = {};
  if (config.jupiterApiKey) headers["x-api-key"] = config.jupiterApiKey;
  const data = await fetchJson<any>(`${config.jupiterPriceUrl}?ids=${SOL_MINT}`, { headers, timeoutMs: 6000 });
  const p = Number(data?.[SOL_MINT]?.usdPrice ?? data?.data?.[SOL_MINT]?.price);
  if (!Number.isFinite(p) || p <= 0) throw new Error("prix Jupiter invalide");
  return p;
}

async function fromCoinGecko(): Promise<number> {
  const data = await fetchJson<any>("https://api.coingecko.com/api/v3/simple/price?ids=solana&vs_currencies=usd", {
    timeoutMs: 6000,
  });
  const p = Number(data?.solana?.usd);
  if (!Number.isFinite(p) || p <= 0) throw new Error("prix CoinGecko invalide");
  return p;
}

async function refresh() {
  const sources: [string, () => Promise<number>][] = [
    ["Jupiter", fromJupiter],
    ["CoinGecko", fromCoinGecko],
  ];
  const errors: string[] = [];
  for (const [name, fn] of sources) {
    try {
      solPrice.usd = await fn();
      solPrice.source = name;
      solPrice.updatedAt = Date.now();
      solPrice.error = "";
      return;
    } catch (e) {
      errors.push(`${name}: ${errMsg(e)}`);
    }
  }
  solPrice.error = errors.join(" | ");
  // Un prix de plus de 10 minutes n'est plus affiché comme valide.
  if (Date.now() - solPrice.updatedAt > 10 * 60_000) solPrice.usd = null;
}

export function startSolPrice() {
  refresh();
  setInterval(refresh, 20_000).unref();
}
