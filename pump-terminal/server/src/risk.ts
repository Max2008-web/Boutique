import { TokenSummary } from "./tokens.js";
import { Holder, MintInfo } from "./onchain.js";

export interface RiskFlag {
  level: "high" | "medium" | "low" | "ok";
  code: string;
  label: string;
}

export interface RiskReport {
  score: number; // 0 (faible) → 100 (très risqué)
  level: "faible" | "modéré" | "élevé" | "critique";
  flags: RiskFlag[];
  metrics: Record<string, number | string | boolean | null>;
}

/**
 * Analyse de risque déterministe, calculée uniquement à partir de données vérifiables :
 * flux de trades observé, comptes on-chain (autorités du mint, plus gros détenteurs).
 * Ce n'est pas une garantie : un score faible ne signifie pas qu'un token est sûr.
 */
export function assessRisk(s: TokenSummary, mint?: MintInfo | null, holders?: Holder[] | null): RiskReport {
  const flags: RiskFlag[] = [];
  let score = 0;
  const add = (level: RiskFlag["level"], code: string, label: string, pts: number) => {
    flags.push({ level, code, label });
    score += pts;
  };

  if (mint) {
    if (mint.mintAuthority) add("high", "mint_authority", "Autorité de mint active : l'offre peut être augmentée", 35);
    else add("ok", "mint_revoked", "Autorité de mint révoquée", 0);
    if (mint.freezeAuthority) add("high", "freeze_authority", "Autorité de gel active : vos tokens peuvent être bloqués (honeypot)", 40);
    else add("ok", "freeze_revoked", "Autorité de gel révoquée", 0);
    const dangerous = mint.extensions.filter((e) =>
      ["transferHook", "permanentDelegate", "transferFeeConfig", "nonTransferable", "pausableConfig"].includes(e),
    );
    if (dangerous.length) add("high", "token2022_ext", `Extensions Token-2022 sensibles : ${dangerous.join(", ")}`, 30);
  } else if (mint === null) {
    add("medium", "no_onchain", "Autorités du mint non vérifiées (données on-chain indisponibles)", 15);
  }
  if (mint !== undefined && !s.fromStream && !holders?.length) {
    add("medium", "no_flow", "Historique de création non observé : dev, snipers et répartition inconnus", 10);
  }

  let top10 = s.top10PctSeen;
  if (holders?.length) {
    const real = holders.filter((h) => !h.isProgram);
    top10 = real.slice(0, 10).reduce((a, h) => a + h.pct, 0);
    const biggest = real[0];
    if (biggest && biggest.pct > 15) add("high", "whale", `Un seul portefeuille détient ${biggest.pct.toFixed(1)} % de l'offre`, 20);
  }
  if (top10 != null) {
    if (top10 > 50) add("high", "top10", `Top 10 détenteurs : ${top10.toFixed(1)} % de l'offre`, 25);
    else if (top10 > 30) add("medium", "top10", `Top 10 détenteurs : ${top10.toFixed(1)} % de l'offre`, 12);
    else add("ok", "top10", `Top 10 détenteurs : ${top10.toFixed(1)} %`, 0);
  }

  if (s.devPct != null) {
    if (s.devPct > 10) add("high", "dev_holds", `Le créateur détient ${s.devPct.toFixed(1)} %`, 15);
    else if (s.devPct > 5) add("medium", "dev_holds", `Le créateur détient ${s.devPct.toFixed(1)} %`, 7);
  }
  if (s.devSoldPct != null && s.devSoldPct > 0) {
    if (s.devSoldPct >= 50) add("high", "dev_sold", `Le créateur a vendu ${s.devSoldPct.toFixed(0)} % de ses tokens`, 25);
    else add("medium", "dev_sold", `Le créateur a vendu ${s.devSoldPct.toFixed(0)} % de ses tokens`, 10);
  }
  if (s.sniperPct != null) {
    if (s.sniperPct > 20) add("high", "snipers", `${s.sniperPct.toFixed(1)} % acheté par ${s.sniperCount} snipers/bundles (≤ 3 s)`, 20);
    else if (s.sniperPct > 8) add("medium", "snipers", `${s.sniperPct.toFixed(1)} % acheté par ${s.sniperCount} snipers (≤ 3 s)`, 10);
  }
  if (s.buys + s.sells >= 20 && s.uniqueBuyers > 0 && s.buys / s.uniqueBuyers > 4) {
    add("medium", "wash", `Activité concentrée : ${s.buys} achats pour ${s.uniqueBuyers} acheteurs uniques (wash trading possible)`, 10);
  }
  if (!s.twitter && !s.telegram && !s.website) add("medium", "no_socials", "Aucun réseau social déclaré", 5);
  if (s.liquiditySol != null && s.liquiditySol < 2) add("medium", "low_liq", `Liquidité faible sur la courbe (${s.liquiditySol.toFixed(2)} SOL)`, 8);
  if (s.ageSec != null && s.ageSec < 60) add("low", "very_new", "Token créé il y a moins d'une minute", 5);

  score = Math.min(100, score);
  const level = score >= 70 ? "critique" : score >= 45 ? "élevé" : score >= 20 ? "modéré" : "faible";
  return {
    score,
    level,
    flags,
    metrics: {
      top10Pct: top10 ?? null,
      devPct: s.devPct,
      devSoldPct: s.devSoldPct,
      sniperPct: s.sniperPct,
      sniperCount: s.sniperCount,
      mintAuthority: mint ? Boolean(mint.mintAuthority) : null,
      freezeAuthority: mint ? Boolean(mint.freezeAuthority) : null,
    },
  };
}
