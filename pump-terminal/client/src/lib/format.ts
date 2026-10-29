export function usd(v: number | null | undefined, digits?: number): string {
  if (v == null || !Number.isFinite(v)) return "—";
  const a = Math.abs(v);
  if (digits === undefined) {
    if (a >= 1e9) return `$${(v / 1e9).toFixed(2)}B`;
    if (a >= 1e6) return `$${(v / 1e6).toFixed(2)}M`;
    if (a >= 1e3) return `$${(v / 1e3).toFixed(1)}K`;
  }
  return `$${v.toLocaleString("fr-FR", { maximumFractionDigits: digits ?? 2, minimumFractionDigits: digits ?? 0 })}`;
}

export function sol(v: number | null | undefined, digits = 3): string {
  if (v == null || !Number.isFinite(v)) return "—";
  return `${v.toLocaleString("fr-FR", { maximumFractionDigits: digits, minimumFractionDigits: Math.min(digits, 2) })} SOL`;
}

export function num(v: number | null | undefined, digits = 2): string {
  if (v == null || !Number.isFinite(v)) return "—";
  const a = Math.abs(v);
  if (a >= 1e9) return `${(v / 1e9).toFixed(2)}B`;
  if (a >= 1e6) return `${(v / 1e6).toFixed(2)}M`;
  if (a >= 1e3) return `${(v / 1e3).toFixed(1)}K`;
  return v.toLocaleString("fr-FR", { maximumFractionDigits: digits });
}

export function pct(v: number | null | undefined, digits = 1, signed = false): string {
  if (v == null || !Number.isFinite(v)) return "—";
  return `${signed && v > 0 ? "+" : ""}${v.toFixed(digits)} %`;
}

/** Prix très petits (memecoins) : 0.0₅123 */
export function price(v: number | null | undefined, prefix = "$"): string {
  if (v == null || !Number.isFinite(v) || v <= 0) return "—";
  if (v >= 1) return `${prefix}${v.toFixed(4)}`;
  const s = v.toFixed(20);
  const m = s.match(/^0\.(0*)(\d{1,4})/);
  if (!m) return `${prefix}${v}`;
  const zeros = m[1].length;
  if (zeros < 4) return `${prefix}${v.toFixed(zeros + 4)}`;
  const sub = String(zeros).split("").map((d) => "₀₁₂₃₄₅₆₇₈₉"[Number(d)]).join("");
  return `${prefix}0.0${sub}${m[2]}`;
}

export function age(sec: number | null | undefined): string {
  if (sec == null) return "—";
  if (sec < 60) return `${sec}s`;
  if (sec < 3600) return `${Math.floor(sec / 60)}m`;
  if (sec < 86400) return `${Math.floor(sec / 3600)}h`;
  return `${Math.floor(sec / 86400)}j`;
}

export function ago(ts: number | null | undefined): string {
  if (!ts) return "—";
  return age(Math.max(0, Math.round((Date.now() - ts) / 1000)));
}

export function short(addr?: string | null, n = 4): string {
  if (!addr) return "—";
  return `${addr.slice(0, n)}…${addr.slice(-n)}`;
}

export const solscanTx = (sig: string) => `https://solscan.io/tx/${sig}`;
export const solscanAccount = (a: string) => `https://solscan.io/account/${a}`;
