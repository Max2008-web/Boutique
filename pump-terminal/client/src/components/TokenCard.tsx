import { memo } from "react";
import { Link } from "react-router-dom";
import type { TokenSummary } from "../lib/types";
import { age, num, pct, sol, usd } from "../lib/format";
import { Bar, Badge, TokenAvatar } from "./ui";
import { useTradeFlow } from "../lib/trade";
import { useSettings } from "../lib/settings";

function quickRisk(t: TokenSummary): { score: number; tone: "up" | "warn" | "down" } {
  let s = 0;
  if ((t.devSoldPct ?? 0) >= 50) s += 25;
  else if ((t.devSoldPct ?? 0) > 0) s += 10;
  if ((t.devPct ?? 0) > 10) s += 15;
  if ((t.sniperPct ?? 0) > 20) s += 20;
  else if ((t.sniperPct ?? 0) > 8) s += 10;
  if ((t.top10PctSeen ?? 0) > 50) s += 25;
  else if ((t.top10PctSeen ?? 0) > 30) s += 12;
  if (!t.twitter && !t.telegram && !t.website) s += 5;
  return { score: s, tone: s >= 45 ? "down" : s >= 20 ? "warn" : "up" };
}

export const TokenCard = memo(function TokenCard({ t, now }: { t: TokenSummary; now: number }) {
  const open = useTradeFlow((s) => s.open);
  const preset = useSettings((s) => s.buyPresets[0]);
  const r = quickRisk(t);
  const buyRatio = t.buys + t.sells > 0 ? (t.buys / (t.buys + t.sells)) * 100 : 50;
  const ageSec = t.createdAt ? Math.round((now - t.createdAt) / 1000) : null;
  return (
    <Link
      to={`/token/${t.mint}`}
      className="block bg-panel hover:bg-panel2 border border-line rounded-xl p-2.5 transition group"
    >
      <div className="flex gap-2.5">
        <TokenAvatar src={t.image} symbol={t.symbol} size={44} />
        <div className="min-w-0 flex-1">
          <div className="flex items-center gap-1.5 min-w-0">
            <span className="font-semibold text-white truncate">{t.symbol || "…"}</span>
            <span className="text-[12px] text-muted truncate">{t.name}</span>
            <span className="ml-auto text-[11px] text-muted font-mono shrink-0">{age(ageSec)}</span>
          </div>
          <div className="flex items-center gap-3 text-[12px] font-mono mt-0.5">
            <span className="text-white" title="Capitalisation">MC {usd(t.marketCapUsd)}</span>
            <span className="text-muted" title="Volume observé">V {sol(t.volumeSol, 1)}</span>
            <span className="text-muted" title="Acheteurs uniques">👥 {t.uniqueBuyers}</span>
          </div>
          <div className="flex items-center gap-1 mt-1.5 flex-wrap">
            {t.devPct != null && (
              <Badge tone={t.devPct > 10 ? "down" : t.devPct > 5 ? "warn" : "neutral"} title="Part détenue par le créateur">
                dev {pct(t.devPct, 1)}
              </Badge>
            )}
            {(t.devSoldPct ?? 0) > 0 && <Badge tone="down">dev a vendu {pct(t.devSoldPct, 0)}</Badge>}
            {t.sniperPct != null && t.sniperPct > 0 && (
              <Badge tone={t.sniperPct > 20 ? "down" : t.sniperPct > 8 ? "warn" : "neutral"} title="Achetés ≤ 3 s après la création">
                snipers {pct(t.sniperPct, 0)}
              </Badge>
            )}
            {t.top10PctSeen != null && <Badge tone={t.top10PctSeen > 50 ? "down" : "neutral"}>top10 {pct(t.top10PctSeen, 0)}</Badge>}
            {t.twitter && <Badge>X</Badge>}
            {t.telegram && <Badge>TG</Badge>}
            {t.website && <Badge>web</Badge>}
            <Badge tone={r.tone} title="Score de risque rapide (flux)">
              risque {r.score}
            </Badge>
          </div>
        </div>
      </div>
      <div className="mt-2 flex items-center gap-2">
        <div className="flex-1">
          {t.migrated ? (
            <div className="text-[11px] text-up">Migré · {t.pool}</div>
          ) : (
            <>
              <div className="flex justify-between text-[10px] text-muted mb-0.5">
                <span>Courbe {pct(t.bondingPct, 0)}</span>
                <span>
                  {num(t.buys, 0)} A / {num(t.sells, 0)} V
                </span>
              </div>
              <Bar value={t.bondingPct} />
            </>
          )}
          <div className="h-1 mt-1 rounded-full bg-down/40 overflow-hidden">
            <div className="h-full bg-up" style={{ width: `${buyRatio}%` }} />
          </div>
        </div>
        <button
          onClick={(e) => {
            e.preventDefault();
            e.stopPropagation();
            open({ mint: t.mint, side: "buy", amountSol: preset });
          }}
          className="shrink-0 bg-up/10 hover:bg-up/20 border border-up/30 text-up text-[12px] font-semibold rounded-lg px-2.5 py-1.5"
        >
          ⚡ {preset}
        </button>
      </div>
    </Link>
  );
});
