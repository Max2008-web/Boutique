import { config } from "./config.js";
import { AiResult, db, save } from "./db.js";
import { store, TokenSummary } from "./tokens.js";
import { dexToken } from "./market.js";
import { getMintInfo, getTopHolders } from "./onchain.js";
import { assessRisk } from "./risk.js";
import { solPrice } from "./prices.js";
import { alert } from "./notify.js";
import { errMsg, fetchJson, uid } from "./util.js";

/**
 * Intégration xAI Grok.
 * - Utilise l'API Responses (https://api.x.ai/v1/responses) avec l'outil serveur `x_search`
 *   pour analyser les publications réelles sur X.
 * - Si l'outil n'est pas disponible pour le modèle/compte, repli sur /chat/completions SANS recherche X
 *   (signalé explicitement dans le résultat : usedXSearch = false).
 */

interface GrokOut {
  text: string;
  citations: string[];
  usedXSearch: boolean;
}

function extractText(res: any): { text: string; citations: string[] } {
  if (typeof res?.output_text === "string") return { text: res.output_text, citations: res.citations ?? [] };
  const parts: string[] = [];
  const cites = new Set<string>(res?.citations ?? []);
  for (const item of res?.output ?? []) {
    if (item.type !== "message") continue;
    for (const c of item.content ?? []) {
      if (typeof c.text === "string") parts.push(c.text);
      for (const a of c.annotations ?? []) if (a.url) cites.add(a.url);
    }
  }
  return { text: parts.join("\n"), citations: [...cites] };
}

async function grok(system: string, user: string, xSearch: boolean): Promise<GrokOut> {
  if (!config.xaiApiKey) throw new Error("XAI_API_KEY manquante : l'analyse Grok est désactivée.");
  const headers = { "content-type": "application/json", authorization: `Bearer ${config.xaiApiKey}` };
  if (xSearch) {
    try {
      const res = await fetchJson<any>(`${config.xaiBase}/responses`, {
        method: "POST",
        headers,
        body: JSON.stringify({
          model: config.xaiModel,
          input: [
            { role: "system", content: system },
            { role: "user", content: user },
          ],
          tools: [{ type: "x_search" }],
        }),
        timeoutMs: 180_000,
      });
      const { text, citations } = extractText(res);
      if (text) return { text, citations, usedXSearch: true };
    } catch (e) {
      console.warn("[grok] x_search indisponible, repli sans recherche X :", errMsg(e));
    }
  }
  const res = await fetchJson<any>(`${config.xaiBase}/chat/completions`, {
    method: "POST",
    headers,
    body: JSON.stringify({
      model: config.xaiModel,
      messages: [
        { role: "system", content: system },
        { role: "user", content: user },
      ],
      temperature: 0.2,
    }),
    timeoutMs: 120_000,
  });
  return { text: res?.choices?.[0]?.message?.content ?? "", citations: [], usedXSearch: false };
}

function parseJson(text: string): any {
  const m = text.match(/```(?:json)?\s*([\s\S]*?)```/);
  const raw = m ? m[1] : text.slice(text.indexOf("{"), text.lastIndexOf("}") + 1);
  try {
    return JSON.parse(raw);
  } catch {
    return { resume: text.trim() };
  }
}

const TOKEN_SYSTEM = `Tu es un analyste on-chain spécialisé dans les memecoins Solana (Pump.fun / PumpSwap).
Tu reçois des données RÉELLES (flux de trades, comptes on-chain, DexScreener). Tu ne dois jamais inventer de chiffres :
si une donnée manque, dis-le. Utilise la recherche X pour vérifier les mentions réelles du ticker, du nom et de l'adresse du contrat
(nombre de posts, comptes influents, signes de campagnes coordonnées / bots, usurpation d'un projet existant).
Sois prudent : la plupart des tokens Pump.fun perdent l'essentiel de leur valeur. Ne promets jamais de gain.
Réponds UNIQUEMENT avec un objet JSON valide, en français, au format :
{
 "score_opportunite": 0-100,
 "score_risque": 0-100,
 "verdict": "eviter" | "surveiller" | "opportunite_speculative",
 "resume": "3 à 5 phrases en français clair",
 "risques": ["..."],
 "signaux_positifs": ["..."],
 "manipulation": "analyse des signes de bundle, wash trading, dev qui vend, snipers",
 "sentiment_x": { "mentions_constatees": "description factuelle de ce que tu as trouvé sur X", "tonalite": "positive|neutre|negative|inconnue", "bots_suspectes": true|false },
 "a_surveiller": ["conditions précises qui changeraient l'analyse"]
}`;

export async function analyzeToken(mint: string): Promise<AiResult> {
  const t = store.ensure(mint);
  const s = store.summary(t);
  const [dex, mintInfo, holders] = await Promise.all([
    dexToken(mint).catch(() => null),
    getMintInfo(mint).catch(() => null),
    getTopHolders(mint).catch(() => null),
  ]);
  const risk = assessRisk(s, mintInfo, holders);
  const recent = t.trades.slice(-40).map((p) => ({
    il_y_a_s: Math.round((Date.now() - p.t) / 1000),
    sens: p.side,
    sol: +p.sol.toFixed(4),
    trader: p.trader.slice(0, 6),
  }));
  const context = {
    token: { mint, nom: s.name || dex?.name, symbole: s.symbol || dex?.symbol, description: t.description, twitter: s.twitter, telegram: s.telegram, site: s.website },
    prix_sol_usd: solPrice.usd,
    flux_pumpportal: s,
    dexscreener: dex,
    mint_onchain: mintInfo,
    top_detenteurs: holders?.slice(0, 12).map((h) => ({ owner: h.owner, pct: +h.pct.toFixed(2), programme_ou_pool: h.isProgram })),
    analyse_risque_deterministe: risk,
    derniers_trades: recent,
  };
  const out = await grok(
    TOKEN_SYSTEM,
    `Analyse ce token. Recherche sur X : "${s.symbol || dex?.symbol || ""}", "${s.name || dex?.name || ""}", "${mint}".\nDonnées :\n${JSON.stringify(context)}`,
    true,
  );
  const result: AiResult = {
    id: uid("ai_"),
    at: Date.now(),
    kind: "token",
    mint,
    symbol: s.symbol || dex?.symbol,
    model: config.xaiModel,
    usedXSearch: out.usedXSearch,
    result: parseJson(out.text),
    citations: out.citations.slice(0, 20),
  };
  db.aiHistory.push(result);
  save();
  return result;
}

const MARKET_SYSTEM = `Tu es un analyste du marché des memecoins Solana (Pump.fun / PumpSwap).
Tu reçois un instantané RÉEL des tokens suivis en direct. Utilise la recherche X pour identifier les narratifs et tendances actuels
autour de Pump.fun et des tickers listés. N'invente aucun chiffre. Réponds UNIQUEMENT en JSON valide, en français :
{
 "resume": "synthèse du marché en 4-6 phrases",
 "tendances": [{"theme": "...", "explication": "...", "tokens": ["SYMBOLE"]}],
 "opportunites": [{"mint": "...", "symbole": "...", "raison": "...", "risque": "faible|modere|eleve|critique"}],
 "alertes_risque": [{"mint": "...", "symbole": "...", "raison": "..."}],
 "sentiment_x": "ce qui est réellement observé sur X",
 "conseils_prudence": ["..."]
}`;

export async function analyzeMarket(): Promise<AiResult> {
  const pick = (list: TokenSummary[]) =>
    list.map((s) => ({
      mint: s.mint,
      sym: s.symbol,
      nom: s.name,
      age_s: s.ageSec,
      mcap_usd: s.marketCapUsd && Math.round(s.marketCapUsd),
      courbe_pct: s.bondingPct && Math.round(s.bondingPct),
      achats: s.buys,
      ventes: s.sells,
      acheteurs: s.uniqueBuyers,
      vol_sol: +s.volumeSol.toFixed(2),
      dev_pct: s.devPct && +s.devPct.toFixed(1),
      dev_vendu_pct: s.devSoldPct && Math.round(s.devSoldPct),
      snipers_pct: s.sniperPct && +s.sniperPct.toFixed(1),
      risque: assessRisk(s).score,
    }));
  const snapshot = {
    prix_sol_usd: solPrice.usd,
    tokens_suivis: store.tokens.size,
    plus_actifs_5min: pick(store.list("active", 20)),
    proches_migration: pick(store.list("graduating", 10)),
    nouveaux: pick(store.list("new", 15)),
    migres_recemment: pick(store.list("migrated", 8)),
  };
  const out = await grok(MARKET_SYSTEM, `Instantané du marché :\n${JSON.stringify(snapshot)}`, true);
  const result: AiResult = {
    id: uid("ai_"),
    at: Date.now(),
    kind: "market",
    model: config.xaiModel,
    usedXSearch: out.usedXSearch,
    result: parseJson(out.text),
    citations: out.citations.slice(0, 20),
  };
  db.aiHistory.push(result);
  save();
  return result;
}

/** Analyse automatique périodique (désactivée par défaut, car chaque appel Grok est facturé). */
export function startAiAutoScan() {
  if (!config.xaiApiKey || !config.aiAutoScanMinutes) return;
  const analysed = new Set<string>();
  setInterval(async () => {
    const candidates = store
      .list("active", 30)
      .filter((s) => !analysed.has(s.mint) && assessRisk(s).score < 45 && s.uniqueBuyers >= 15)
      .slice(0, 2);
    for (const c of candidates) {
      analysed.add(c.mint);
      try {
        const r = await analyzeToken(c.mint);
        const v = r.result?.verdict;
        const level = v === "opportunite_speculative" ? "opportunity" : v === "eviter" ? "risk" : "info";
        alert(level, `Grok : $${c.symbol} — ${v ?? "analyse"}`, String(r.result?.resume ?? "").slice(0, 400), c.mint);
      } catch (e) {
        console.warn("[grok autoscan]", errMsg(e));
      }
    }
  }, config.aiAutoScanMinutes * 60_000).unref();
}
