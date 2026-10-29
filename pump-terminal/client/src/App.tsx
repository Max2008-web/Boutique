import { useEffect } from "react";
import { Link, Route, Routes } from "react-router-dom";
import { Layout } from "./components/Layout";
import { TradeModal } from "./components/TradeModal";
import { connectLive, useLive } from "./lib/live";
import { useNow, useRegisterWallet } from "./lib/hooks";
import Pulse from "./pages/Pulse";
import Token from "./pages/Token";
import Portfolio from "./pages/Portfolio";
import Grok from "./pages/Grok";
import Automation from "./pages/Automation";
import Settings from "./pages/Settings";

let started = false;

function ProposalBanner() {
  const now = useNow(1000);
  const p = useLive((s) => s.proposals.find((x) => x.status === "pending" && x.expiresAt > now));
  if (!p) return null;
  return (
    <Link
      to="/auto"
      className="fixed z-40 left-3 right-3 bottom-20 lg:bottom-4 lg:left-auto lg:right-4 lg:w-96 bg-warn text-black rounded-xl px-4 py-3 shadow-2xl"
    >
      <div className="font-semibold text-sm">Validation requise : {p.side === "buy" ? "acheter" : "vendre"} ${p.symbol}</div>
      <div className="text-[12px] opacity-80">
        {p.source} · expire dans {Math.max(0, Math.round((p.expiresAt - now) / 1000))} s — toucher pour examiner
      </div>
    </Link>
  );
}

function AuthBanner() {
  const authError = useLive((s) => s.authError);
  if (!authError) return null;
  return (
    <Link to="/reglages" className="block bg-down/15 border-b border-down/40 text-down text-[13px] px-4 py-2">
      Le serveur exige un jeton d'accès. Renseignez-le dans Réglages.
    </Link>
  );
}

export default function App() {
  useRegisterWallet();
  useEffect(() => {
    if (!started) {
      started = true;
      connectLive();
    }
  }, []);
  return (
    <Layout>
      <AuthBanner />
      <Routes>
        <Route path="/" element={<Pulse />} />
        <Route path="/token/:mint" element={<Token />} />
        <Route path="/portefeuille" element={<Portfolio />} />
        <Route path="/ia" element={<Grok />} />
        <Route path="/auto" element={<Automation />} />
        <Route path="/reglages" element={<Settings />} />
        <Route path="*" element={<Pulse />} />
      </Routes>
      <TradeModal />
      <ProposalBanner />
    </Layout>
  );
}
