import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { api, del, post, put } from "../lib/api";
import { useLive } from "../lib/live";
import { useNow, usePoll } from "../lib/hooks";
import { useTradeFlow } from "../lib/trade";
import { ago, sol, short, solscanTx, usd } from "../lib/format";
import type { Exits, LimitsStatus, Proposal, Strategy, StrategyFilters, WatchAlert } from "../lib/types";
import { Badge, Button, Empty, Field, inputCls, Panel, Toggle } from "../components/ui";

interface AutoState {
  strategies: Strategy[];
  watchAlerts: WatchAlert[];
  limits: LimitsStatus;
  exits: Exits;
  killSwitch: boolean;
  wallet: string | null;
}

const FILTER_FIELDS: { key: keyof StrategyFilters; label: string; hint?: string }[] = [
  { key: "minAgeSec", label: "Âge min (s)" },
  { key: "maxAgeSec", label: "Âge max (s)" },
  { key: "minMcapUsd", label: "MC min ($)" },
  { key: "maxMcapUsd", label: "MC max ($)" },
  { key: "minBuys", label: "Achats min" },
  { key: "minUniqueBuyers", label: "Acheteurs uniques min" },
  { key: "minVolumeSol", label: "Volume min (SOL)" },
  { key: "minBondingPct", label: "Courbe min (%)" },
  { key: "maxBondingPct", label: "Courbe max (%)" },
  { key: "maxDevPct", label: "Dev max (%)" },
  { key: "maxSniperPct", label: "Snipers max (%)" },
  { key: "maxTop10Pct", label: "Top 10 max (%)" },
  { key: "maxRiskScore", label: "Score de risque max" },
];

const KIND_LABEL: Record<WatchAlert["kind"], string> = {
  mcap_above: "MC ≥",
  mcap_below: "MC ≤",
  dev_sold: "Le dev vend",
  big_buy: "Achat ≥ (SOL)",
  migration: "Migration",
};

function ProposalCard({ p, now }: { p: Proposal; now: number }) {
  const open = useTradeFlow((s) => s.open);
  const left = Math.max(0, Math.round((p.expiresAt - now) / 1000));
  return (
    <div className="bg-panel2 border border-warn/30 rounded-xl p-3 space-y-2">
      <div className="flex items-center gap-2">
        <Badge tone={p.side === "buy" ? "up" : "down"}>{p.side === "buy" ? "ACHAT" : "VENTE"}</Badge>
        <Link to={`/token/${p.mint}`} className="font-semibold hover:underline">
          ${p.symbol}
        </Link>
        <span className="font-mono text-[13px]">{p.side === "buy" ? sol(p.amountSol, 3) : `${p.percent} %`}</span>
        <span className="ml-auto text-[12px] text-warn font-mono">{left}s</span>
      </div>
      <div className="text-[12px] text-white/70">
        <span className="text-muted">{p.source} — </span>
        {p.reason}
      </div>
      <div className="flex gap-2">
        <Button
          tone={p.side === "buy" ? "buy" : "sell"}
          size="sm"
          className="flex-1"
          onClick={() =>
            open({ mint: p.mint, side: p.side, amountSol: p.amountSol, percent: p.percent, slippagePct: p.slippagePct, proposalId: p.id, source: "auto", label: p.source })
          }
        >
          Examiner et signer
        </Button>
        <Button size="sm" tone="ghost" onClick={() => post(`/api/proposals/${p.id}/reject`).catch(() => {})}>
          Refuser
        </Button>
      </div>
    </div>
  );
}

function StrategyForm({ initial, onSaved, onCancel }: { initial?: Strategy; onSaved: () => void; onCancel: () => void }) {
  const [name, setName] = useState(initial?.name ?? "Nouvelle stratégie");
  const [mode, setMode] = useState<Strategy["mode"]>(initial?.mode ?? "alert");
  const [buySol, setBuySol] = useState(String(initial?.buySol ?? 0.05));
  const [slip, setSlip] = useState(String(initial?.slippagePct ?? 15));
  const [filters, setFilters] = useState<StrategyFilters>(
    initial?.filters ?? { minAgeSec: 30, maxAgeSec: 600, minUniqueBuyers: 25, maxDevPct: 5, maxSniperPct: 10, maxTop10Pct: 35, requireDevNotSold: true, requireSocials: true, maxRiskScore: 30 },
  );
  const [err, setErr] = useState("");
  const save = async () => {
    try {
      await post("/api/automation/strategies", {
        id: initial?.id,
        name,
        mode,
        enabled: initial?.enabled ?? false,
        buySol: Number(buySol),
        slippagePct: Number(slip),
        filters,
      });
      onSaved();
    } catch (e: any) {
      setErr(e.message);
    }
  };
  return (
    <div className="space-y-3 bg-panel2 rounded-xl p-3">
      <div className="grid sm:grid-cols-2 gap-2">
        <Field label="Nom">
          <input className={inputCls + " !font-sans"} value={name} onChange={(e) => setName(e.target.value)} />
        </Field>
        <Field label="Action">
          <select className={inputCls + " !font-sans"} value={mode} onChange={(e) => setMode(e.target.value as Strategy["mode"])}>
            <option value="alert">Alerte uniquement</option>
            <option value="propose">Proposer un achat (signature requise)</option>
          </select>
        </Field>
        {mode === "propose" && (
          <>
            <Field label="Montant par achat (SOL)">
              <input className={inputCls} value={buySol} onChange={(e) => setBuySol(e.target.value)} inputMode="decimal" />
            </Field>
            <Field label="Slippage (%)">
              <input className={inputCls} value={slip} onChange={(e) => setSlip(e.target.value)} inputMode="decimal" />
            </Field>
          </>
        )}
      </div>
      <div className="grid grid-cols-2 sm:grid-cols-3 lg:grid-cols-4 gap-2">
        {FILTER_FIELDS.map((f) => (
          <Field key={f.key} label={f.label}>
            <input
              className={inputCls}
              inputMode="decimal"
              value={(filters[f.key] as number | undefined) ?? ""}
              onChange={(e) => {
                const v = e.target.value.trim();
                setFilters({ ...filters, [f.key]: v === "" ? undefined : Number(v) });
              }}
            />
          </Field>
        ))}
        <Field label="Nom/ticker contient">
          <input className={inputCls} value={filters.nameIncludes ?? ""} onChange={(e) => setFilters({ ...filters, nameIncludes: e.target.value || undefined })} />
        </Field>
      </div>
      <div className="flex flex-wrap gap-4">
        <Toggle checked={Boolean(filters.requireSocials)} onChange={(v) => setFilters({ ...filters, requireSocials: v })} label="Réseaux sociaux requis" />
        <Toggle checked={Boolean(filters.requireDevNotSold)} onChange={(v) => setFilters({ ...filters, requireDevNotSold: v })} label="Le dev n'a pas vendu" />
      </div>
      {err && <div className="text-down text-[13px]">{err}</div>}
      <div className="flex gap-2">
        <Button tone="brand" onClick={save}>
          Enregistrer
        </Button>
        <Button tone="ghost" onClick={onCancel}>
          Annuler
        </Button>
      </div>
    </div>
  );
}

export default function Automation() {
  const now = useNow(1000);
  const proposals = useLive((s) => s.proposals);
  const killLive = useLive((s) => s.status?.killSwitch);
  const st = usePoll<AutoState>("/api/automation", 10_000, [killLive]);
  const [editing, setEditing] = useState<Strategy | "new" | null>(null);
  const [limits, setLimits] = useState<LimitsStatus | null>(null);
  const [exits, setExits] = useState<Exits | null>(null);
  const [msg, setMsg] = useState("");

  useEffect(() => {
    if (st.data) {
      setLimits((l) => l ?? st.data!.limits);
      setExits((e) => e ?? st.data!.exits);
    }
  }, [st.data]);

  useEffect(() => {
    api<Proposal[]>("/api/proposals").then((p) => useLive.setState({ proposals: p })).catch(() => {});
  }, []);

  const pending = proposals.filter((p) => p.status === "pending" && p.expiresAt > now);
  const history = proposals.filter((p) => !(p.status === "pending" && p.expiresAt > now)).slice(0, 30);
  const kill = st.data?.killSwitch ?? false;

  const toggleKill = async () => {
    if (!kill && !confirm("Activer l'arrêt d'urgence ? Toutes les stratégies seront désactivées et les propositions annulées.")) return;
    await post("/api/automation/killswitch", { on: !kill });
    st.reload();
  };

  const saveLimits = async () => {
    try {
      const r = await put<LimitsStatus>("/api/automation/limits", limits);
      setLimits(r);
      setMsg("Limites enregistrées ✓");
    } catch (e: any) {
      setMsg(e.message);
    }
  };
  const saveExits = async () => {
    try {
      setExits(await put<Exits>("/api/automation/exits", exits));
      setMsg("Take-profit / stop-loss enregistrés ✓");
    } catch (e: any) {
      setMsg(e.message);
    }
  };

  return (
    <div className="px-3 sm:px-4 pt-3 space-y-3">
      <div className={`rounded-xl border p-3 flex flex-wrap items-center gap-3 ${kill ? "bg-down/10 border-down/40" : "bg-panel border-line"}`}>
        <div className="flex-1 min-w-[200px]">
          <div className="font-semibold">{kill ? "Arrêt d'urgence ACTIVÉ" : "Automatisations actives"}</div>
          <div className="text-[12px] text-muted">
            Le serveur surveille le marché 24 h/24. Il ne possède aucune clé : il peut uniquement alerter ou proposer. Chaque transaction exige votre
            signature dans Phantom.
          </div>
        </div>
        <Button tone={kill ? "neutral" : "danger"} size="lg" onClick={toggleKill}>
          {kill ? "Lever l'arrêt d'urgence" : "■ ARRÊT D'URGENCE"}
        </Button>
      </div>
      {msg && <div className="text-[13px] text-muted">{msg}</div>}

      <Panel title={`Propositions en attente de validation (${pending.length})`}>
        {pending.length ? (
          <div className="grid md:grid-cols-2 xl:grid-cols-3 gap-2">
            {pending.map((p) => (
              <ProposalCard key={p.id} p={p} now={now} />
            ))}
          </div>
        ) : (
          <Empty>Aucune proposition. Les stratégies en mode « proposer » et le take-profit/stop-loss en créeront ici.</Empty>
        )}
      </Panel>

      <div className="grid lg:grid-cols-2 gap-3">
        <Panel title="Limites de dépenses et de pertes">
          {limits && (
            <div className="space-y-3">
              <div className="grid grid-cols-2 gap-2">
                <Field label="Max par transaction (SOL)">
                  <input className={inputCls} value={limits.maxSolPerTrade} onChange={(e) => setLimits({ ...limits, maxSolPerTrade: e.target.value as any })} inputMode="decimal" />
                </Field>
                <Field label="Dépense max / jour (SOL)">
                  <input className={inputCls} value={limits.maxDailySpendSol} onChange={(e) => setLimits({ ...limits, maxDailySpendSol: e.target.value as any })} inputMode="decimal" />
                </Field>
                <Field label="Perte réalisée max / jour (SOL)">
                  <input className={inputCls} value={limits.maxDailyLossSol} onChange={(e) => setLimits({ ...limits, maxDailyLossSol: e.target.value as any })} inputMode="decimal" />
                </Field>
                <Field label="Positions ouvertes max">
                  <input className={inputCls} value={limits.maxOpenPositions} onChange={(e) => setLimits({ ...limits, maxOpenPositions: e.target.value as any })} inputMode="numeric" />
                </Field>
              </div>
              <Toggle checked={limits.applyToManual} onChange={(v) => setLimits({ ...limits, applyToManual: v })} label="Appliquer aussi aux achats manuels" />
              {st.data && (
                <div className="text-[12px] text-muted font-mono">
                  Aujourd'hui : dépensé {sol(st.data.limits.spentToday, 3)} · P&L réalisé {sol(st.data.limits.realizedToday, 3)} · positions{" "}
                  {st.data.limits.openPositions}
                </div>
              )}
              <Button tone="brand" size="sm" onClick={saveLimits}>
                Enregistrer les limites
              </Button>
            </div>
          )}
        </Panel>

        <Panel title="Take-profit / Stop-loss (positions ouvertes via le terminal)">
          {exits && (
            <div className="space-y-3">
              <Toggle checked={exits.enabled} onChange={(v) => setExits({ ...exits, enabled: v })} label="Surveiller mes positions" />
              <div className="grid grid-cols-3 gap-2">
                <Field label="Take-profit (+%)">
                  <input className={inputCls} value={exits.takeProfitPct} onChange={(e) => setExits({ ...exits, takeProfitPct: e.target.value as any })} inputMode="decimal" />
                </Field>
                <Field label="Stop-loss (−%)">
                  <input className={inputCls} value={exits.stopLossPct} onChange={(e) => setExits({ ...exits, stopLossPct: e.target.value as any })} inputMode="decimal" />
                </Field>
                <Field label="Part à vendre (%)">
                  <input className={inputCls} value={exits.sellPercent} onChange={(e) => setExits({ ...exits, sellPercent: e.target.value as any })} inputMode="decimal" />
                </Field>
              </div>
              <p className="text-[12px] text-muted">
                Quand un seuil est atteint, une proposition de vente apparaît (notification + Telegram si configuré). Elle n'est exécutée qu'après votre
                signature.
              </p>
              <Button tone="brand" size="sm" onClick={saveExits}>
                Enregistrer
              </Button>
            </div>
          )}
        </Panel>
      </div>

      <Panel
        title="Stratégies de détection"
        right={
          <Button size="sm" tone="brand" onClick={() => setEditing("new")}>
            + Nouvelle stratégie
          </Button>
        }
      >
        {editing && (
          <div className="mb-3">
            <StrategyForm
              initial={editing === "new" ? undefined : editing}
              onSaved={() => {
                setEditing(null);
                st.reload();
              }}
              onCancel={() => setEditing(null)}
            />
          </div>
        )}
        {st.data?.strategies.length ? (
          <div className="divide-y divide-line/60">
            {st.data.strategies.map((s) => (
              <div key={s.id} className="flex flex-wrap items-center gap-3 py-2">
                <Toggle
                  checked={s.enabled}
                  onChange={async (v) => {
                    await post("/api/automation/strategies", { ...s, enabled: v });
                    st.reload();
                  }}
                />
                <div className="min-w-0 flex-1">
                  <div className="font-medium text-[14px]">{s.name}</div>
                  <div className="text-[11px] text-muted">
                    {s.mode === "propose" ? `Propose un achat de ${s.buySol} SOL` : "Alerte uniquement"} · {s.matches} déclenchements ·{" "}
                    {Object.entries(s.filters)
                      .filter(([, v]) => v !== undefined && v !== false)
                      .map(([k, v]) => `${k}=${v}`)
                      .join(", ")}
                  </div>
                </div>
                <Button size="sm" onClick={() => setEditing(s)}>
                  Modifier
                </Button>
                <Button
                  size="sm"
                  tone="ghost"
                  onClick={async () => {
                    if (confirm(`Supprimer « ${s.name} » ?`)) {
                      await del(`/api/automation/strategies/${s.id}`);
                      st.reload();
                    }
                  }}
                >
                  Supprimer
                </Button>
              </div>
            ))}
          </div>
        ) : (
          <Empty>Aucune stratégie. Créez-en une pour être alerté (ou recevoir des propositions) sur les nouveaux tokens correspondant à vos critères.</Empty>
        )}
      </Panel>

      <div className="grid lg:grid-cols-2 gap-3">
        <Panel title="Alertes personnalisées sur des tokens" bodyClass="p-0">
          {st.data?.watchAlerts.length ? (
            <div className="divide-y divide-line/60">
              {st.data.watchAlerts.map((w) => (
                <div key={w.id} className="flex items-center gap-2 px-3 py-2 text-[13px]">
                  <Link to={`/token/${w.mint}`} className="font-semibold hover:underline w-24 truncate">
                    ${w.symbol ?? short(w.mint)}
                  </Link>
                  <span className="text-white/80">
                    {KIND_LABEL[w.kind]} {w.kind.startsWith("mcap") ? usd(w.value ?? null) : w.value ?? ""}
                  </span>
                  <Badge tone={w.enabled ? "up" : "neutral"}>{w.enabled ? "active" : w.triggeredAt ? `déclenchée ${ago(w.triggeredAt)}` : "inactive"}</Badge>
                  <button
                    className="ml-auto text-muted hover:text-down"
                    onClick={async () => {
                      await del(`/api/automation/watch/${w.id}`);
                      st.reload();
                    }}
                  >
                    ×
                  </button>
                </div>
              ))}
            </div>
          ) : (
            <Empty>Créez des alertes depuis la page d'un token.</Empty>
          )}
        </Panel>
        <Panel title="Historique des propositions" bodyClass="p-0">
          {history.length ? (
            <div className="divide-y divide-line/60">
              {history.map((p) => (
                <div key={p.id} className="flex items-center gap-2 px-3 py-2 text-[12px]">
                  <Badge tone={p.side === "buy" ? "up" : "down"}>{p.side === "buy" ? "achat" : "vente"}</Badge>
                  <span className="font-semibold">${p.symbol}</span>
                  <span className="text-muted truncate">{p.source}</span>
                  <Badge tone={p.status === "executed" ? "up" : p.status === "failed" ? "down" : "neutral"}>
                    {{ pending: "expirée", rejected: "refusée", expired: "expirée", executed: "exécutée", failed: "échouée" }[p.status]}
                  </Badge>
                  {p.signature && (
                    <a href={solscanTx(p.signature)} target="_blank" rel="noreferrer" className="text-brand">
                      ↗
                    </a>
                  )}
                  <span className="ml-auto text-muted">{ago(p.createdAt)}</span>
                </div>
              ))}
            </div>
          ) : (
            <Empty>Aucun historique.</Empty>
          )}
        </Panel>
      </div>
    </div>
  );
}
