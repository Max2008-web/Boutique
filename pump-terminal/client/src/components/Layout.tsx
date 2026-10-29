import { FormEvent, ReactNode, useState } from "react";
import { NavLink, useNavigate } from "react-router-dom";
import { WalletMultiButton } from "@solana/wallet-adapter-react-ui";
import { useWallet } from "@solana/wallet-adapter-react";
import { useLive } from "../lib/live";
import { useSolBalance } from "../lib/hooks";
import { sol, usd } from "../lib/format";
import { Badge } from "./ui";

const NAV = [
  { to: "/", label: "Découverte", icon: "M3 12h4l3-8 4 16 3-8h4" },
  { to: "/portefeuille", label: "Portefeuille", icon: "M3 7h18v12H3z M16 13h2 M3 7l3-3h12l3 3" },
  { to: "/ia", label: "Grok IA", icon: "M12 3l2.5 5.5L20 11l-5.5 2.5L12 19l-2.5-5.5L4 11l5.5-2.5z" },
  { to: "/auto", label: "Automatisation", icon: "M12 8v4l3 2 M12 3a9 9 0 100 18 9 9 0 000-18z" },
  { to: "/reglages", label: "Réglages", icon: "M4 6h16 M4 12h10 M4 18h16 M18 9v6" },
];

function Icon({ d }: { d: string }) {
  return (
    <svg viewBox="0 0 24 24" className="w-5 h-5" fill="none" stroke="currentColor" strokeWidth={1.8} strokeLinecap="round" strokeLinejoin="round">
      <path d={d} />
    </svg>
  );
}

function FeedDot() {
  const { connected, status } = useLive();
  const live = connected && status?.feed?.connected && Date.now() - (status.feed.lastMessageAt || 0) < 30_000;
  const label = !connected ? "Serveur déconnecté" : status?.feed?.connected ? "Flux Pump.fun en direct" : "Flux Pump.fun déconnecté";
  return (
    <span className="flex items-center gap-1.5 text-[11px] text-muted" title={`${label}${status?.feed?.lastError ? " — " + status.feed.lastError : ""}`}>
      <span className={`w-2 h-2 rounded-full ${live ? "bg-up animate-pulse" : "bg-down"}`} />
      <span className="hidden sm:inline">{live ? "LIVE" : "HORS LIGNE"}</span>
    </span>
  );
}

export function Layout({ children }: { children: ReactNode }) {
  const nav = useNavigate();
  const [q, setQ] = useState("");
  const status = useLive((s) => s.status);
  const pending = useLive((s) => s.proposals.filter((p) => p.status === "pending" && p.expiresAt > Date.now()).length);
  const { publicKey } = useWallet();
  const { balance } = useSolBalance();

  const onSearch = (e: FormEvent) => {
    e.preventDefault();
    const m = q.trim().match(/[1-9A-HJ-NP-Za-km-z]{32,44}/);
    if (m) {
      nav(`/token/${m[0]}`);
      setQ("");
    }
  };

  return (
    <div className="min-h-screen flex flex-col">
      <header className="sticky top-0 z-30 bg-bg/90 backdrop-blur border-b border-line">
        <div className="flex items-center gap-3 px-3 sm:px-4 h-14">
          <NavLink to="/" className="flex items-center gap-2 shrink-0">
            <img src="/icon.svg" className="w-7 h-7" alt="" />
            <span className="font-bold text-white hidden md:inline">Pump Terminal</span>
          </NavLink>
          <nav className="hidden lg:flex items-center gap-1 ml-2">
            {NAV.map((n) => (
              <NavLink
                key={n.to}
                to={n.to}
                end={n.to === "/"}
                className={({ isActive }) => `px-3 py-1.5 rounded-lg text-sm ${isActive ? "bg-white/10 text-white" : "text-muted hover:text-white"}`}
              >
                {n.label}
                {n.to === "/auto" && pending > 0 && <span className="ml-1.5 bg-warn text-black rounded-full px-1.5 text-[10px] font-bold">{pending}</span>}
              </NavLink>
            ))}
          </nav>
          <form onSubmit={onSearch} className="flex-1 min-w-0 max-w-md">
            <input
              value={q}
              onChange={(e) => setQ(e.target.value)}
              placeholder="Coller l'adresse d'un token…"
              className="w-full bg-panel border border-line rounded-lg px-3 py-1.5 text-sm outline-none focus:border-brand/60"
            />
          </form>
          <div className="flex items-center gap-3 shrink-0">
            {status?.killSwitch && <Badge tone="down">ARRÊT D'URGENCE</Badge>}
            <span className="hidden md:inline font-mono text-xs text-muted" title={`Source : ${status?.solPriceSource ?? "—"}`}>
              SOL {usd(status?.solUsd ?? null, 2)}
            </span>
            <FeedDot />
            {publicKey && balance != null && (
              <span className="hidden sm:inline font-mono text-xs text-white/90">{sol(balance, 3)}</span>
            )}
            <WalletMultiButton />
          </div>
        </div>
      </header>
      <main className="flex-1 pb-20 lg:pb-6">{children}</main>
      <nav className="lg:hidden fixed bottom-0 inset-x-0 z-30 bg-panel/95 backdrop-blur border-t border-line grid grid-cols-5 pb-[env(safe-area-inset-bottom)]">
        {NAV.map((n) => (
          <NavLink
            key={n.to}
            to={n.to}
            end={n.to === "/"}
            className={({ isActive }) => `relative flex flex-col items-center gap-0.5 py-2 text-[10px] ${isActive ? "text-white" : "text-muted"}`}
          >
            <Icon d={n.icon} />
            {n.label}
            {n.to === "/auto" && pending > 0 && <span className="absolute top-1 right-[28%] bg-warn text-black rounded-full px-1 text-[9px] font-bold">{pending}</span>}
          </NavLink>
        ))}
      </nav>
    </div>
  );
}
