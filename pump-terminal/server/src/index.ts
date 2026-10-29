import fs from "node:fs";
import http from "node:http";
import path from "node:path";
import express, { NextFunction, Request, Response } from "express";
import { PublicKey } from "@solana/web3.js";
import { config } from "./config.js";
import { proxyRpc } from "./rpc.js";
import { feed } from "./pumpportal.js";
import { store } from "./tokens.js";
import { solPrice, startSolPrice } from "./prices.js";
import { dexToken, geckoCandles } from "./market.js";
import { computePnl, getMintInfo, getTopHolders, getWalletHistory, getWalletTokens } from "./onchain.js";
import { assessRisk } from "./risk.js";
import { prepareTrade, recordTrade, setProposalStatus } from "./trade.js";
import { analyzeMarket, analyzeToken, startAiAutoScan } from "./ai.js";
import { db, save, WatchAlert } from "./db.js";
import { broadcast, startHub } from "./hub.js";
import { alert, sendTelegram } from "./notify.js";
import { ledger, limitsStatus } from "./limits.js";
import { resetStrategyMatches, setKillSwitch, startAutomation, upsertStrategy } from "./automation.js";
import { errMsg, uid } from "./util.js";

if (!config.rpcUrl) {
  console.warn("⚠️  SOLANA_RPC_URL n'est pas définie : portefeuille, transactions et analyses on-chain seront indisponibles.");
}

const app = express();
app.disable("x-powered-by");
app.use(express.json({ limit: "1mb" }));

const wrap =
  (fn: (req: Request, res: Response) => Promise<unknown> | unknown) => async (req: Request, res: Response) => {
    try {
      const out = await fn(req, res);
      if (!res.headersSent) res.json(out ?? { ok: true });
    } catch (e) {
      if (!res.headersSent) res.status(400).json({ error: errMsg(e) });
    }
  };

function validAddress(s: string): string {
  try {
    return new PublicKey(s).toBase58();
  } catch {
    throw new Error("Adresse Solana invalide");
  }
}

app.get("/api/health", (_req, res) => res.json({ ok: true }));

// Authentification simple par jeton (ACCESS_TOKEN) pour toute l'API.
app.use("/api", (req: Request, res: Response, next: NextFunction) => {
  if (!config.accessToken) return next();
  if (req.header("x-access-token") === config.accessToken) return next();
  res.status(401).json({ error: "Jeton d'accès requis" });
});

app.get(
  "/api/status",
  wrap(() => ({
    feed: feed.status(),
    tokensTracked: store.tokens.size,
    solUsd: solPrice.usd,
    solPriceSource: solPrice.source,
    solPriceUpdatedAt: solPrice.updatedAt,
    rpcConfigured: Boolean(config.rpcUrl),
    ai: { enabled: Boolean(config.xaiApiKey), model: config.xaiModel, autoScanMinutes: config.aiAutoScanMinutes },
    telegram: Boolean(config.telegramToken && config.telegramChatId),
    jupiter: { base: config.jupiterBase, apiKey: Boolean(config.jupiterApiKey) },
    pumpPortalFeePct: config.pumpPortalFeePct,
    killSwitch: db.killSwitch,
  })),
);

app.post(
  "/api/rpc",
  wrap(async (req) => proxyRpc(req.body)),
);

/* --------------------------------- Tokens --------------------------------- */

app.get(
  "/api/tokens",
  wrap((req) => {
    const kind = String(req.query.list ?? "new") as "new" | "graduating" | "migrated" | "active";
    const limit = Math.min(200, Number(req.query.limit ?? 60));
    return store.list(kind, limit);
  }),
);

app.get(
  "/api/tokens/:mint",
  wrap(async (req) => {
    const mint = validAddress(req.params.mint);
    const t = store.ensure(mint);
    const [dex, mintInfo, holders] = await Promise.all([
      dexToken(mint).catch(() => null),
      getMintInfo(mint).catch((e) => ({ error: errMsg(e) }) as any),
      getTopHolders(mint).catch(() => null),
    ]);
    const s = store.summary(t);
    const mi = mintInfo && !mintInfo.error ? mintInfo : null;
    return {
      summary: s,
      description: t.description,
      dex,
      mint: mintInfo,
      holders,
      risk: assessRisk(s, mi, holders),
      solUsd: solPrice.usd,
    };
  }),
);

app.get(
  "/api/tokens/:mint/trades",
  wrap((req) => {
    const t = store.tokens.get(validAddress(req.params.mint));
    return (t?.trades ?? []).slice(-150).reverse();
  }),
);

app.get(
  "/api/tokens/:mint/candles",
  wrap(async (req) => {
    const mint = validAddress(req.params.mint);
    const tf = Math.max(1, Math.min(86400, Number(req.query.tf ?? 60)));
    const live = store.candles(mint, tf, "usd");
    let history: Awaited<ReturnType<typeof geckoCandles>> = [];
    let historyError = "";
    // Historique GeckoTerminal pour les intervalles ≥ 1 min (complète le flux temps réel).
    if (tf >= 60) {
      try {
        history = await geckoCandles(mint, tf);
      } catch (e) {
        historyError = errMsg(e);
      }
    }
    const firstLive = live[0]?.time ?? Infinity;
    const merged = [...history.filter((c) => c.time < firstLive), ...live];
    return {
      candles: merged,
      sources: { live: live.length, geckoterminal: history.length },
      historyError,
    };
  }),
);

/* ------------------------------- Portefeuille ------------------------------ */

app.get("/api/wallet/:addr/portfolio", wrap(async (req) => {
  const r = await getWalletTokens(validAddress(req.params.addr));
  return { ...r, solUsd: solPrice.usd };
}));
app.get("/api/wallet/:addr/history", wrap(async (req) => getWalletHistory(validAddress(req.params.addr), Number(req.query.limit ?? 60))));
app.get("/api/wallet/:addr/pnl", wrap(async (req) => computePnl(validAddress(req.params.addr), Number(req.query.limit ?? 150))));

app.post(
  "/api/wallet/register",
  wrap((req) => {
    const addr = validAddress(String(req.body?.address ?? ""));
    db.settings.wallet = addr;
    save();
    feed.subscribeAccount(addr);
    return { ok: true, wallet: addr };
  }),
);

/* -------------------------------- Trading --------------------------------- */

app.post("/api/trade/prepare", wrap(async (req) => prepareTrade(req.body)));

app.post(
  "/api/trades/record",
  wrap(async (req) => {
    const { signature, wallet, source, proposalId } = req.body ?? {};
    if (typeof signature !== "string" || signature.length < 60) throw new Error("Signature invalide");
    return recordTrade(signature, validAddress(wallet), String(source ?? "manuel"), proposalId);
  }),
);

app.get("/api/trades", wrap(() => db.trades.slice(-200).reverse()));
app.get("/api/positions", wrap(() => ({ ...ledger(), limits: limitsStatus() })));

/* ----------------------------------- IA ------------------------------------ */

app.post("/api/ai/token/:mint", wrap(async (req) => analyzeToken(validAddress(req.params.mint))));
app.post("/api/ai/market", wrap(async () => analyzeMarket()));
app.get("/api/ai/history", wrap((req) => {
  const mint = req.query.mint ? String(req.query.mint) : null;
  return db.aiHistory.filter((a) => !mint || a.mint === mint).slice(-30).reverse();
}));

/* ------------------------------ Automatisation ----------------------------- */

app.get(
  "/api/automation",
  wrap(() => ({
    strategies: db.strategies,
    watchAlerts: db.watchAlerts,
    limits: limitsStatus(),
    exits: db.exits,
    killSwitch: db.killSwitch,
    wallet: db.settings.wallet,
  })),
);

app.post("/api/automation/strategies", wrap((req) => {
  const s = upsertStrategy(req.body);
  resetStrategyMatches(s.id);
  return s;
}));
app.delete("/api/automation/strategies/:id", wrap((req) => {
  db.strategies = db.strategies.filter((s) => s.id !== req.params.id);
  save();
}));

app.put(
  "/api/automation/limits",
  wrap((req) => {
    const b = req.body ?? {};
    const pos = (v: any, d: number) => (Number.isFinite(Number(v)) && Number(v) >= 0 ? Number(v) : d);
    db.limits = {
      maxSolPerTrade: pos(b.maxSolPerTrade, db.limits.maxSolPerTrade),
      maxDailySpendSol: pos(b.maxDailySpendSol, db.limits.maxDailySpendSol),
      maxDailyLossSol: pos(b.maxDailyLossSol, db.limits.maxDailyLossSol),
      maxOpenPositions: Math.round(pos(b.maxOpenPositions, db.limits.maxOpenPositions)),
      applyToManual: b.applyToManual !== undefined ? Boolean(b.applyToManual) : db.limits.applyToManual,
    };
    save();
    return limitsStatus();
  }),
);

app.put(
  "/api/automation/exits",
  wrap((req) => {
    const b = req.body ?? {};
    db.exits = {
      enabled: Boolean(b.enabled) && !db.killSwitch,
      takeProfitPct: Math.max(1, Number(b.takeProfitPct ?? db.exits.takeProfitPct)),
      stopLossPct: Math.min(99, Math.max(1, Number(b.stopLossPct ?? db.exits.stopLossPct))),
      sellPercent: Math.min(100, Math.max(1, Number(b.sellPercent ?? db.exits.sellPercent))),
    };
    save();
    return db.exits;
  }),
);

app.post("/api/automation/killswitch", wrap((req) => {
  setKillSwitch(Boolean(req.body?.on));
  return { killSwitch: db.killSwitch };
}));

app.post(
  "/api/automation/watch",
  wrap((req) => {
    const b = req.body ?? {};
    const kinds: WatchAlert["kind"][] = ["mcap_above", "mcap_below", "dev_sold", "big_buy", "migration"];
    if (!kinds.includes(b.kind)) throw new Error("Type d'alerte invalide");
    const w: WatchAlert = {
      id: uid("wa_"),
      mint: validAddress(String(b.mint)),
      symbol: b.symbol ? String(b.symbol).slice(0, 20) : undefined,
      kind: b.kind,
      value: b.value != null ? Number(b.value) : undefined,
      enabled: true,
      repeat: Boolean(b.repeat),
    };
    db.watchAlerts.push(w);
    store.watch(w.mint);
    save();
    return w;
  }),
);
app.delete("/api/automation/watch/:id", wrap((req) => {
  const w = db.watchAlerts.find((x) => x.id === req.params.id);
  if (w) store.unwatch(w.mint);
  db.watchAlerts = db.watchAlerts.filter((x) => x.id !== req.params.id);
  save();
}));

app.get("/api/proposals", wrap(() => db.proposals.slice(-100).reverse()));
app.post("/api/proposals/:id/reject", wrap((req) => {
  setProposalStatus(req.params.id, "rejected");
  const p = db.proposals.find((x) => x.id === req.params.id);
  if (p) broadcast("proposal:update", p);
  return p;
}));

app.get("/api/alerts", wrap(() => db.alerts.slice(-150).reverse()));
app.post("/api/alerts/test-telegram", wrap(async () => {
  await sendTelegram("✅ Pump Terminal : les notifications Telegram fonctionnent.");
  return { ok: true };
}));

app.use("/api", (_req, res) => res.status(404).json({ error: "Route inconnue" }));

/* --------------------------- Frontend (build Vite) -------------------------- */

if (fs.existsSync(config.staticDir)) {
  app.use(express.static(config.staticDir, { index: false, maxAge: "1h" }));
  app.get("*", (_req, res) => res.sendFile(path.join(config.staticDir, "index.html")));
}

const server = http.createServer(app);
startHub(server);
startSolPrice();
store.start();
feed.start();
startAutomation();
startAiAutoScan();
if (db.settings.wallet) feed.subscribeAccount(db.settings.wallet);

setInterval(() => broadcast("status", { feed: feed.status(), solUsd: solPrice.usd, tokensTracked: store.tokens.size, killSwitch: db.killSwitch }), 5000).unref();

server.listen(config.port, () => {
  console.log(`Pump Terminal — serveur sur http://localhost:${config.port}`);
  alert("info", "Serveur démarré", "Surveillance du marché active.");
});
