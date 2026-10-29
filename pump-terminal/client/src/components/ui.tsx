import { ReactNode, useState } from "react";
import { short } from "../lib/format";

export function Panel({ title, right, children, className = "", bodyClass = "p-3" }: { title?: ReactNode; right?: ReactNode; children: ReactNode; className?: string; bodyClass?: string }) {
  return (
    <section className={`bg-panel border border-line rounded-xl overflow-hidden ${className}`}>
      {(title || right) && (
        <header className="flex items-center justify-between gap-2 px-3 py-2 border-b border-line">
          <h2 className="text-[13px] font-semibold text-white/90 truncate">{title}</h2>
          {right}
        </header>
      )}
      <div className={bodyClass}>{children}</div>
    </section>
  );
}

export function Stat({ label, value, sub, tone }: { label: string; value: ReactNode; sub?: ReactNode; tone?: "up" | "down" | "warn" }) {
  const c = tone === "up" ? "text-up" : tone === "down" ? "text-down" : tone === "warn" ? "text-warn" : "text-white";
  return (
    <div className="bg-panel2 rounded-lg px-3 py-2 min-w-0">
      <div className="text-[11px] text-muted uppercase tracking-wide truncate">{label}</div>
      <div className={`font-mono text-sm font-medium truncate ${c}`}>{value}</div>
      {sub && <div className="text-[11px] text-muted truncate">{sub}</div>}
    </div>
  );
}

export function Badge({ children, tone = "neutral", title }: { children: ReactNode; tone?: "neutral" | "up" | "down" | "warn" | "brand"; title?: string }) {
  const map = {
    neutral: "bg-white/5 text-white/70 border-white/10",
    up: "bg-up/10 text-up border-up/20",
    down: "bg-down/10 text-down border-down/20",
    warn: "bg-warn/10 text-warn border-warn/20",
    brand: "bg-brand/15 text-[#a99bff] border-brand/30",
  } as const;
  return (
    <span title={title} className={`inline-flex items-center gap-1 rounded-md border px-1.5 py-0.5 text-[11px] font-medium whitespace-nowrap ${map[tone]}`}>
      {children}
    </span>
  );
}

export function Button({
  children,
  onClick,
  tone = "neutral",
  disabled,
  className = "",
  type = "button",
  size = "md",
}: {
  children: ReactNode;
  onClick?: () => void;
  tone?: "neutral" | "buy" | "sell" | "brand" | "danger" | "ghost";
  disabled?: boolean;
  className?: string;
  type?: "button" | "submit";
  size?: "sm" | "md" | "lg";
}) {
  const tones = {
    neutral: "bg-panel2 hover:bg-white/10 text-white border border-line",
    buy: "bg-up hover:bg-up/90 text-black font-semibold",
    sell: "bg-down hover:bg-down/90 text-white font-semibold",
    brand: "bg-brand hover:bg-brand/90 text-white font-semibold",
    danger: "bg-down/15 hover:bg-down/25 text-down border border-down/40 font-semibold",
    ghost: "hover:bg-white/5 text-white/80",
  } as const;
  const sizes = { sm: "px-2 py-1 text-xs", md: "px-3 py-2 text-sm", lg: "px-4 py-3 text-base" } as const;
  return (
    <button
      type={type}
      onClick={onClick}
      disabled={disabled}
      className={`rounded-lg transition active:scale-[0.98] disabled:opacity-40 disabled:pointer-events-none ${tones[tone]} ${sizes[size]} ${className}`}
    >
      {children}
    </button>
  );
}

export function TokenAvatar({ src, symbol, size = 36 }: { src?: string; symbol?: string; size?: number }) {
  const [err, setErr] = useState(false);
  const letter = (symbol || "?").slice(0, 1).toUpperCase();
  if (!src || err) {
    return (
      <div style={{ width: size, height: size }} className="rounded-lg bg-gradient-to-br from-brand/40 to-up/30 grid place-items-center font-bold text-white/90 shrink-0">
        {letter}
      </div>
    );
  }
  return <img src={src} alt={symbol} loading="lazy" onError={() => setErr(true)} style={{ width: size, height: size }} className="rounded-lg object-cover bg-panel2 shrink-0" />;
}

export function Copy({ text, label }: { text: string; label?: string }) {
  const [ok, setOk] = useState(false);
  return (
    <button
      className="font-mono text-[11px] text-muted hover:text-white transition"
      title={text}
      onClick={(e) => {
        e.preventDefault();
        e.stopPropagation();
        navigator.clipboard?.writeText(text).then(() => {
          setOk(true);
          setTimeout(() => setOk(false), 1200);
        });
      }}
    >
      {ok ? "copié ✓" : label ?? short(text)}
    </button>
  );
}

export function Empty({ children }: { children: ReactNode }) {
  return <div className="text-center text-muted text-sm py-8 px-4">{children}</div>;
}

export function Spinner({ className = "" }: { className?: string }) {
  return <span className={`inline-block w-4 h-4 border-2 border-white/20 border-t-white rounded-full animate-spin ${className}`} />;
}

export function Bar({ value, tone = "brand" }: { value: number | null; tone?: "brand" | "up" | "warn" }) {
  const v = Math.max(0, Math.min(100, value ?? 0));
  const c = tone === "up" ? "bg-up" : tone === "warn" ? "bg-warn" : "bg-gradient-to-r from-brand to-up";
  return (
    <div className="h-1.5 rounded-full bg-white/5 overflow-hidden">
      <div className={`h-full ${c}`} style={{ width: `${v}%` }} />
    </div>
  );
}

export function Field({ label, children, hint }: { label: string; children: ReactNode; hint?: string }) {
  return (
    <label className="block">
      <span className="text-[11px] text-muted uppercase tracking-wide">{label}</span>
      <div className="mt-1">{children}</div>
      {hint && <span className="text-[11px] text-muted">{hint}</span>}
    </label>
  );
}

export const inputCls =
  "w-full bg-panel2 border border-line rounded-lg px-3 py-2 text-sm text-white font-mono outline-none focus:border-brand/60";

export function Toggle({ checked, onChange, label }: { checked: boolean; onChange: (v: boolean) => void; label?: string }) {
  return (
    <button type="button" onClick={() => onChange(!checked)} className="inline-flex items-center gap-2 text-sm">
      <span className={`w-9 h-5 rounded-full transition relative ${checked ? "bg-up" : "bg-white/15"}`}>
        <span className={`absolute top-0.5 w-4 h-4 rounded-full bg-white transition ${checked ? "left-[18px]" : "left-0.5"}`} />
      </span>
      {label && <span className="text-white/80">{label}</span>}
    </button>
  );
}
