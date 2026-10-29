import { useMemo, useState } from "react";
import { useLive } from "../lib/live";
import { useNow } from "../lib/hooks";
import type { TokenSummary } from "../lib/types";
import { TokenCard } from "../components/TokenCard";
import { Empty, inputCls, Toggle } from "../components/ui";

type Col = "new" | "graduating" | "migrated";
const TITLES: Record<Col, string> = { new: "Nouveaux", graduating: "Bientôt migrés", migrated: "Migrés (PumpSwap)" };

interface Filters {
  minMcap: string;
  maxMcap: string;
  minBuyers: string;
  socials: boolean;
  hideDevSold: boolean;
  maxSnipers: string;
}

function applyFilters(list: TokenSummary[], f: Filters) {
  return list.filter((t) => {
    if (f.minMcap && (t.marketCapUsd ?? 0) < Number(f.minMcap)) return false;
    if (f.maxMcap && (t.marketCapUsd ?? Infinity) > Number(f.maxMcap)) return false;
    if (f.minBuyers && t.uniqueBuyers < Number(f.minBuyers)) return false;
    if (f.socials && !t.twitter && !t.telegram && !t.website) return false;
    if (f.hideDevSold && (t.devSoldPct ?? 0) > 0) return false;
    if (f.maxSnipers && (t.sniperPct ?? 0) > Number(f.maxSnipers)) return false;
    return true;
  });
}

export default function Pulse() {
  const tokens = useLive((s) => s.tokens);
  const connected = useLive((s) => s.connected);
  const status = useLive((s) => s.status);
  const now = useNow(1000);
  const [tab, setTab] = useState<Col>("new");
  const [showFilters, setShowFilters] = useState(false);
  const [f, setF] = useState<Filters>({ minMcap: "", maxMcap: "", minBuyers: "", socials: false, hideDevSold: false, maxSnipers: "" });

  const cols = useMemo(() => {
    const all = Object.values(tokens);
    const fresh = all.filter((t) => t.createdAt && !t.migrated).sort((a, b) => b.createdAt! - a.createdAt!);
    const grad = all
      .filter((t) => !t.migrated && (t.bondingPct ?? 0) >= 50)
      .sort((a, b) => (b.bondingPct ?? 0) - (a.bondingPct ?? 0));
    const mig = all.filter((t) => t.migrated).sort((a, b) => b.lastTradeAt - a.lastTradeAt);
    return {
      new: applyFilters(fresh, f).slice(0, 50),
      graduating: applyFilters(grad, f).slice(0, 40),
      migrated: applyFilters(mig, f).slice(0, 40),
    } as Record<Col, TokenSummary[]>;
  }, [tokens, f]);

  const column = (c: Col) => (
    <div key={c} className="min-w-0 flex flex-col">
      <div className="hidden lg:flex items-center justify-between px-1 pb-2">
        <h2 className="font-semibold text-sm">{TITLES[c]}</h2>
        <span className="text-[11px] text-muted">{cols[c].length}</span>
      </div>
      <div className="space-y-2 lg:max-h-[calc(100vh-170px)] lg:overflow-y-auto lg:pr-1">
        {cols[c].length === 0 ? (
          <Empty>
            {!connected
              ? "Connexion au serveur…"
              : !status?.feed?.connected
                ? "Le flux PumpPortal n'est pas connecté. Vérifiez le serveur."
                : "En attente de tokens correspondant à ce filtre…"}
          </Empty>
        ) : (
          cols[c].map((t) => <TokenCard key={t.mint} t={t} now={now} />)
        )}
      </div>
    </div>
  );

  return (
    <div className="px-3 sm:px-4 pt-3">
      <div className="flex items-center gap-2 mb-3">
        <div className="lg:hidden flex bg-panel border border-line rounded-lg p-0.5 flex-1">
          {(Object.keys(TITLES) as Col[]).map((c) => (
            <button
              key={c}
              onClick={() => setTab(c)}
              className={`flex-1 text-[12px] py-1.5 rounded-md ${tab === c ? "bg-white/10 text-white" : "text-muted"}`}
            >
              {TITLES[c].split(" (")[0]} <span className="text-muted">{cols[c].length}</span>
            </button>
          ))}
        </div>
        <span className="hidden lg:block text-[12px] text-muted flex-1">
          {status?.tokensTracked ?? 0} tokens suivis en direct via PumpPortal · données réelles, aucune simulation
        </span>
        <button onClick={() => setShowFilters((v) => !v)} className="text-[12px] bg-panel border border-line rounded-lg px-3 py-1.5 text-white/80">
          Filtres
        </button>
      </div>
      {showFilters && (
        <div className="bg-panel border border-line rounded-xl p-3 mb-3 grid grid-cols-2 sm:grid-cols-4 lg:grid-cols-7 gap-2 items-end">
          <input className={inputCls} placeholder="MC min $" value={f.minMcap} onChange={(e) => setF({ ...f, minMcap: e.target.value })} inputMode="numeric" />
          <input className={inputCls} placeholder="MC max $" value={f.maxMcap} onChange={(e) => setF({ ...f, maxMcap: e.target.value })} inputMode="numeric" />
          <input className={inputCls} placeholder="Acheteurs min" value={f.minBuyers} onChange={(e) => setF({ ...f, minBuyers: e.target.value })} inputMode="numeric" />
          <input className={inputCls} placeholder="Snipers max %" value={f.maxSnipers} onChange={(e) => setF({ ...f, maxSnipers: e.target.value })} inputMode="numeric" />
          <Toggle checked={f.socials} onChange={(v) => setF({ ...f, socials: v })} label="Avec réseaux" />
          <Toggle checked={f.hideDevSold} onChange={(v) => setF({ ...f, hideDevSold: v })} label="Dev n'a pas vendu" />
        </div>
      )}
      <div className="lg:hidden">{column(tab)}</div>
      <div className="hidden lg:grid grid-cols-3 gap-3">{(Object.keys(TITLES) as Col[]).map(column)}</div>
    </div>
  );
}
