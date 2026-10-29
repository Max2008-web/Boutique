import "dotenv/config";
import path from "node:path";

function num(name: string, def: number): number {
  const v = process.env[name];
  if (v === undefined || v === "") return def;
  const n = Number(v);
  return Number.isFinite(n) ? n : def;
}

export const config = {
  port: num("PORT", 8080),
  /** Mot de passe d'accès à l'API du terminal (fortement recommandé en production). */
  accessToken: process.env.ACCESS_TOKEN ?? "",

  /** RPC Solana mainnet (Helius, QuickNode, Triton…). Obligatoire. */
  rpcUrl: process.env.SOLANA_RPC_URL ?? "",

  pumpPortalWs: process.env.PUMPPORTAL_WS_URL ?? "wss://pumpportal.fun/api/data",
  pumpPortalApiKey: process.env.PUMPPORTAL_API_KEY ?? "",
  pumpPortalTradeUrl: process.env.PUMPPORTAL_TRADE_URL ?? "https://pumpportal.fun/api/trade-local",
  /** Frais de plateforme PumpPortal (API locale) en %, affiché dans l'aperçu. */
  pumpPortalFeePct: num("PUMPPORTAL_FEE_PCT", 0.5),

  jupiterBase: (process.env.JUPITER_API_BASE ?? "https://lite-api.jup.ag/swap/v1").replace(/\/$/, ""),
  jupiterApiKey: process.env.JUPITER_API_KEY ?? "",
  jupiterPriceUrl: process.env.JUPITER_PRICE_URL ?? "https://lite-api.jup.ag/price/v3",

  xaiApiKey: process.env.XAI_API_KEY ?? "",
  xaiModel: process.env.XAI_MODEL ?? "grok-4",
  xaiBase: (process.env.XAI_API_BASE ?? "https://api.x.ai/v1").replace(/\/$/, ""),
  /** 0 = désactivé. Sinon, Grok analyse automatiquement les meilleurs candidats toutes les N minutes. */
  aiAutoScanMinutes: num("AI_AUTOSCAN_MINUTES", 0),

  telegramToken: process.env.TELEGRAM_BOT_TOKEN ?? "",
  telegramChatId: process.env.TELEGRAM_CHAT_ID ?? "",

  dataDir: path.resolve(process.env.DATA_DIR ?? "./data"),
  /** Durée pendant laquelle un nouveau token est suivi trade par trade (minutes). */
  trackMinutes: num("TRACK_MINUTES", 45),
  maxTracked: num("MAX_TRACKED", 600),

  staticDir: process.env.STATIC_DIR ?? path.resolve("../client/dist"),
};

export const SOL_MINT = "So11111111111111111111111111111111111111112";
export const LAMPORTS = 1_000_000_000;
