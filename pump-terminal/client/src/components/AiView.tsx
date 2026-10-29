import type { AiResult } from "../lib/types";
import { Badge } from "./ui";

function List({ title, items, tone }: { title: string; items?: any[]; tone?: "up" | "down" }) {
  if (!items?.length) return null;
  return (
    <div>
      <div className="text-[11px] uppercase tracking-wide text-muted mb-1">{title}</div>
      <ul className="space-y-1">
        {items.map((x, i) => (
          <li key={i} className={`text-[13px] flex gap-2 ${tone === "up" ? "text-up/90" : tone === "down" ? "text-down/90" : "text-white/85"}`}>
            <span>•</span>
            <span>{typeof x === "string" ? x : [x.symbole ?? x.theme, x.raison ?? x.explication].filter(Boolean).join(" — ")}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}

export function AiView({ r }: { r: AiResult }) {
  const x = r.result ?? {};
  const verdictTone = x.verdict === "eviter" ? "down" : x.verdict === "opportunite_speculative" ? "up" : "warn";
  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-center gap-1.5">
        {x.verdict && <Badge tone={verdictTone}>{String(x.verdict).replace(/_/g, " ")}</Badge>}
        {x.score_opportunite != null && <Badge tone="up">opportunité {x.score_opportunite}/100</Badge>}
        {x.score_risque != null && <Badge tone="down">risque {x.score_risque}/100</Badge>}
        <Badge tone={r.usedXSearch ? "brand" : "warn"}>{r.usedXSearch ? "recherche X réelle" : "sans recherche X"}</Badge>
        <span className="text-[11px] text-muted ml-auto">
          {r.model} · {new Date(r.at).toLocaleTimeString("fr-FR")}
        </span>
      </div>
      {x.resume && <p className="text-[14px] leading-relaxed text-white/90 whitespace-pre-wrap">{x.resume}</p>}
      {x.manipulation && (
        <div>
          <div className="text-[11px] uppercase tracking-wide text-muted mb-1">Manipulation / fraude</div>
          <p className="text-[13px] text-white/85">{x.manipulation}</p>
        </div>
      )}
      {x.sentiment_x && (
        <div>
          <div className="text-[11px] uppercase tracking-wide text-muted mb-1">Sur X</div>
          <p className="text-[13px] text-white/85">
            {typeof x.sentiment_x === "string"
              ? x.sentiment_x
              : `${x.sentiment_x.mentions_constatees ?? ""} ${x.sentiment_x.tonalite ? `(tonalité : ${x.sentiment_x.tonalite})` : ""} ${x.sentiment_x.bots_suspectes ? "— bots suspectés" : ""}`}
          </p>
        </div>
      )}
      <List title="Tendances" items={x.tendances} />
      <List title="Opportunités (spéculatives)" items={x.opportunites} tone="up" />
      <List title="Alertes de risque" items={x.alertes_risque} tone="down" />
      <List title="Risques" items={x.risques} tone="down" />
      <List title="Signaux positifs" items={x.signaux_positifs} tone="up" />
      <List title="À surveiller" items={x.a_surveiller} />
      <List title="Prudence" items={x.conseils_prudence} />
      {r.citations?.length > 0 && (
        <div>
          <div className="text-[11px] uppercase tracking-wide text-muted mb-1">Sources consultées</div>
          <div className="flex flex-col gap-0.5">
            {r.citations.map((c) => (
              <a key={c} href={c} target="_blank" rel="noreferrer" className="text-[11px] text-brand truncate hover:underline">
                {c}
              </a>
            ))}
          </div>
        </div>
      )}
      <p className="text-[11px] text-muted">Analyse générée par Grok (xAI) à partir de données réelles. Ce n'est pas un conseil financier.</p>
    </div>
  );
}
