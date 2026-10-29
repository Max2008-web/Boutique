import { useState } from "react";
import { Link } from "react-router-dom";
import { post } from "../lib/api";
import { useLive } from "../lib/live";
import { usePoll } from "../lib/hooks";
import { ago } from "../lib/format";
import type { AiResult } from "../lib/types";
import { AiView } from "../components/AiView";
import { Badge, Button, Empty, Panel, Spinner } from "../components/ui";

const LEVEL: Record<string, { label: string; tone: "up" | "down" | "warn" | "brand" | "neutral" }> = {
  opportunity: { label: "opportunité", tone: "up" },
  risk: { label: "risque", tone: "down" },
  trade: { label: "transaction", tone: "brand" },
  system: { label: "système", tone: "warn" },
  info: { label: "info", tone: "neutral" },
};

export function AlertsFeed({ limit = 80 }: { limit?: number }) {
  const alerts = useLive((s) => s.alerts);
  if (!alerts.length) return <Empty>Aucune alerte pour le moment.</Empty>;
  return (
    <div className="divide-y divide-line/60">
      {alerts.slice(0, limit).map((a) => (
        <div key={a.id} className="px-3 py-2">
          <div className="flex items-center gap-2">
            <Badge tone={LEVEL[a.level]?.tone ?? "neutral"}>{LEVEL[a.level]?.label ?? a.level}</Badge>
            {a.mint ? (
              <Link to={`/token/${a.mint}`} className="text-[13px] font-medium hover:underline truncate">
                {a.title}
              </Link>
            ) : (
              <span className="text-[13px] font-medium truncate">{a.title}</span>
            )}
            <span className="ml-auto text-[11px] text-muted shrink-0">{ago(a.at)}</span>
          </div>
          <p className="text-[12px] text-white/70 mt-0.5">{a.body}</p>
        </div>
      ))}
    </div>
  );
}

export default function Grok() {
  const status = useLive((s) => s.status);
  const hist = usePoll<AiResult[]>("/api/ai/history");
  const [current, setCurrent] = useState<AiResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [err, setErr] = useState("");
  const latestMarket = current ?? hist.data?.find((h) => h.kind === "market") ?? null;

  const run = async () => {
    setLoading(true);
    setErr("");
    try {
      const r = await post<AiResult>("/api/ai/market");
      setCurrent(r);
      hist.reload();
    } catch (e: any) {
      setErr(e.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="px-3 sm:px-4 pt-3 grid lg:grid-cols-[1fr_380px] gap-3">
      <div className="space-y-3 min-w-0">
        <Panel
          title="Grok — analyse du marché Pump.fun"
          right={
            <Button tone="brand" size="sm" onClick={run} disabled={loading || !status?.ai?.enabled}>
              {loading ? "Analyse… (1–2 min)" : "Analyser le marché maintenant"}
            </Button>
          }
        >
          {!status?.ai?.enabled && (
            <div className="text-[13px] text-warn mb-2">
              Grok est désactivé. Ajoutez une clé <code>XAI_API_KEY</code> (console.x.ai, payante à l'usage) dans la configuration du serveur.
            </div>
          )}
          {status?.ai?.enabled && (
            <div className="text-[12px] text-muted mb-3">
              Modèle : {status.ai.model} · Analyse automatique : {status.ai.autoScanMinutes ? `toutes les ${status.ai.autoScanMinutes} min` : "désactivée"}.
              Grok reçoit l'instantané réel des tokens suivis et recherche les tendances sur X.
            </div>
          )}
          {err && <div className="text-down text-[13px] mb-2">{err}</div>}
          {loading && (
            <div className="flex items-center gap-2 text-sm text-muted mb-2">
              <Spinner /> Grok analyse les tokens en direct et les publications sur X…
            </div>
          )}
          {latestMarket ? <AiView r={latestMarket} /> : !loading && <Empty>Lancez une première analyse.</Empty>}
        </Panel>

        <Panel title="Analyses de tokens récentes" bodyClass="p-0">
          {hist.data?.filter((h) => h.kind === "token").length ? (
            <div className="divide-y divide-line/60">
              {hist.data
                .filter((h) => h.kind === "token")
                .map((h) => (
                  <Link key={h.id} to={`/token/${h.mint}`} className="flex items-center gap-2 px-3 py-2 hover:bg-white/[0.02]">
                    <span className="font-semibold text-[13px] w-24 truncate">${h.symbol ?? h.mint?.slice(0, 6)}</span>
                    <Badge tone={h.result?.verdict === "eviter" ? "down" : h.result?.verdict === "opportunite_speculative" ? "up" : "warn"}>
                      {String(h.result?.verdict ?? "analyse").replace(/_/g, " ")}
                    </Badge>
                    <span className="text-[12px] text-white/70 truncate flex-1">{h.result?.resume}</span>
                    <span className="text-[11px] text-muted shrink-0">{ago(h.at)}</span>
                  </Link>
                ))}
            </div>
          ) : (
            <Empty>Ouvrez un token et cliquez sur « Analyser avec Grok ».</Empty>
          )}
        </Panel>
      </div>
      <Panel title="Alertes en direct" bodyClass="p-0 max-h-[calc(100vh-150px)] overflow-y-auto">
        <AlertsFeed />
      </Panel>
    </div>
  );
}
