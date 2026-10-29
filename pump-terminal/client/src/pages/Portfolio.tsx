import { Link } from "react-router-dom";
import { useWallet } from "@solana/wallet-adapter-react";
import { useWalletModal } from "@solana/wallet-adapter-react-ui";
import { usePoll } from "../lib/hooks";
import { useTradeFlow } from "../lib/trade";
import { num, price, short, sol, solscanTx, usd } from "../lib/format";
import type { ParsedTx, PnlRow, WalletToken } from "../lib/types";
import { Badge, Button, Empty, Panel, Spinner, Stat, TokenAvatar } from "../components/ui";

const tone = (v: number | null | undefined) => (v == null ? undefined : v > 0 ? "up" : v < 0 ? "down" : undefined);
const signed = (v: number | null | undefined, d = 4) => (v == null ? "—" : `${v > 0 ? "+" : ""}${sol(v, d)}`);

export default function Portfolio() {
  const { publicKey } = useWallet();
  const { setVisible } = useWalletModal();
  const open = useTradeFlow((s) => s.open);
  const addr = publicKey?.toBase58();
  const pf = usePoll<{ sol: number; tokens: WalletToken[]; solUsd: number | null }>(addr ? `/api/wallet/${addr}/portfolio` : null, 20_000);
  const pnl = usePoll<{ rows: PnlRow[]; totals: { realizedSol: number; unrealizedSol: number }; analyzedTransactions: number; solUsd: number | null }>(
    addr ? `/api/wallet/${addr}/pnl?limit=150` : null,
    60_000,
  );
  const hist = usePoll<ParsedTx[]>(addr ? `/api/wallet/${addr}/history?limit=60` : null, 30_000);

  if (!publicKey) {
    return (
      <div className="max-w-md mx-auto mt-16 text-center space-y-4 px-4">
        <h1 className="text-xl font-bold">Connectez votre portefeuille Phantom</h1>
        <p className="text-muted text-sm">
          Le terminal lit votre solde, vos tokens et vos transactions directement sur Solana. Votre phrase de récupération et votre clé privée ne sont
          jamais demandées : chaque transaction est signée dans Phantom.
        </p>
        <Button tone="brand" size="lg" onClick={() => setVisible(true)}>
          Connecter Phantom
        </Button>
      </div>
    );
  }

  const solUsd = pf.data?.solUsd ?? null;
  const tokensValueSol = pf.data?.tokens.reduce((s, t) => s + (t.valueSol ?? 0), 0) ?? 0;
  const totalSol = (pf.data?.sol ?? 0) + tokensValueSol;
  const pnlByMint = new Map(pnl.data?.rows.map((r) => [r.mint, r]) ?? []);
  const symbolOf = (mint?: string) => {
    if (!mint) return "";
    const t = pf.data?.tokens.find((x) => x.mint === mint) ?? pnl.data?.rows.find((x) => x.mint === mint);
    return t?.symbol ? `$${t.symbol}` : short(mint);
  };

  return (
    <div className="px-3 sm:px-4 pt-3 space-y-3">
      <div className="grid grid-cols-2 lg:grid-cols-5 gap-2">
        <Stat label="Solde SOL" value={pf.data ? sol(pf.data.sol, 4) : "…"} sub={pf.data && solUsd ? usd(pf.data.sol * solUsd, 2) : undefined} />
        <Stat label="Valeur des tokens" value={pf.data ? sol(tokensValueSol, 4) : "…"} sub={solUsd ? usd(tokensValueSol * solUsd, 2) : undefined} />
        <Stat label="Valeur totale" value={pf.data ? sol(totalSol, 4) : "…"} sub={solUsd ? usd(totalSol * solUsd, 2) : undefined} />
        <Stat
          label="P&L réalisé"
          value={pnl.data ? signed(pnl.data.totals.realizedSol) : "…"}
          tone={tone(pnl.data?.totals.realizedSol)}
          sub={pnl.data && solUsd ? usd(pnl.data.totals.realizedSol * solUsd, 2) : undefined}
        />
        <Stat
          label="P&L latent"
          value={pnl.data ? signed(pnl.data.totals.unrealizedSol) : "…"}
          tone={tone(pnl.data?.totals.unrealizedSol)}
          sub={pnl.data ? `sur ${pnl.data.analyzedTransactions} dernières transactions` : undefined}
        />
      </div>

      <Panel title="Mes tokens" right={pf.loading ? <Spinner /> : <span className="text-[11px] text-muted">{short(addr, 6)}</span>} bodyClass="p-0">
        {pf.error && <div className="p-3 text-down text-sm">{pf.error}</div>}
        {pf.data && pf.data.tokens.length === 0 && <Empty>Aucun token dans ce portefeuille.</Empty>}
        <div className="divide-y divide-line/60">
          {pf.data?.tokens.map((t) => {
            const r = pnlByMint.get(t.mint);
            return (
              <div key={t.account} className="flex flex-wrap sm:flex-nowrap items-center gap-3 px-3 py-2.5">
                <Link to={`/token/${t.mint}`} className="flex items-center gap-2.5 min-w-0 flex-1">
                  <TokenAvatar src={t.image} symbol={t.symbol} size={36} />
                  <div className="min-w-0">
                    <div className="font-semibold truncate">{t.symbol ? `$${t.symbol}` : short(t.mint)}</div>
                    <div className="text-[12px] text-muted font-mono truncate">
                      {num(t.amount, 2)} · {price(t.priceUsd)}
                    </div>
                  </div>
                </Link>
                <div className="text-right font-mono text-[13px] w-28">
                  <div>{usd(t.valueUsd, 2)}</div>
                  <div className="text-muted text-[11px]">{sol(t.valueSol, 4)}</div>
                </div>
                <div className={`text-right font-mono text-[13px] w-28 ${r?.totalSol != null ? (r.totalSol >= 0 ? "text-up" : "text-down") : "text-muted"}`}>
                  {r ? signed(r.totalSol ?? r.realizedSol) : "—"}
                  <div className="text-[11px] text-muted">{r?.avgCostSol ? `coût moy. ${price(r.avgCostSol, "")}` : "P&L n/d"}</div>
                </div>
                <div className="flex gap-1 w-full sm:w-auto">
                  {[25, 50, 75, 100].map((p) => (
                    <Button key={p} size="sm" tone={p === 100 ? "sell" : "neutral"} className="flex-1 sm:flex-none font-mono" onClick={() => open({ mint: t.mint, side: "sell", percent: p })}>
                      {p}%
                    </Button>
                  ))}
                </div>
              </div>
            );
          })}
        </div>
      </Panel>

      <div className="grid lg:grid-cols-2 gap-3">
        <Panel title="Gains et pertes par token (on-chain)" right={pnl.loading ? <Spinner /> : null} bodyClass="p-0">
          {pnl.error && <div className="p-3 text-down text-sm">{pnl.error}</div>}
          {pnl.data && pnl.data.rows.length === 0 && <Empty>Aucun achat/vente détecté dans l'historique analysé.</Empty>}
          <div className="divide-y divide-line/60">
            {pnl.data?.rows.map((r) => (
              <Link to={`/token/${r.mint}`} key={r.mint} className="flex items-center gap-2 px-3 py-2 text-[12px] font-mono hover:bg-white/[0.02]">
                <TokenAvatar src={r.image} symbol={r.symbol} size={26} />
                <span className="font-sans font-semibold w-24 truncate">{r.symbol ? `$${r.symbol}` : short(r.mint)}</span>
                <span className="text-muted hidden sm:inline">
                  investi {sol(r.solSpent, 3)} · reçu {sol(r.solReceived, 3)}
                </span>
                <span className={`ml-auto ${r.realizedSol >= 0 ? "text-up" : "text-down"}`} title="Réalisé">
                  {signed(r.realizedSol, 3)}
                </span>
                <span className={`w-24 text-right ${r.unrealizedSol == null ? "text-muted" : r.unrealizedSol >= 0 ? "text-up" : "text-down"}`} title="Latent">
                  {r.holding > 0 ? signed(r.unrealizedSol, 3) : "clôturé"}
                </span>
              </Link>
            ))}
          </div>
          <p className="text-[11px] text-muted px-3 py-2">
            Calculé à partir de vos transactions on-chain (coût moyen, frais réseau et rente inclus). Les tokens reçus par transfert ne sont pas
            comptés comme achats.
          </p>
        </Panel>

        <Panel title="Transactions récentes" right={hist.loading ? <Spinner /> : null} bodyClass="p-0 max-h-[600px] overflow-y-auto">
          {hist.error && <div className="p-3 text-down text-sm">{hist.error}</div>}
          <div className="divide-y divide-line/60">
            {hist.data?.map((h) => (
              <a key={h.signature} href={solscanTx(h.signature)} target="_blank" rel="noreferrer" className="flex items-center gap-2 px-3 py-2 text-[12px] hover:bg-white/[0.02]">
                <span className="text-muted font-mono w-24 shrink-0">{h.time ? new Date(h.time).toLocaleString("fr-FR", { dateStyle: "short", timeStyle: "short" }) : "—"}</span>
                {h.err ? (
                  <Badge tone="down">échec</Badge>
                ) : h.kind === "buy" ? (
                  <Badge tone="up">achat</Badge>
                ) : h.kind === "sell" ? (
                  <Badge tone="down">vente</Badge>
                ) : (
                  <Badge>{h.kind === "transfer" ? "transfert" : "autre"}</Badge>
                )}
                <span className="truncate">{symbolOf(h.mint)}</span>
                <span className={`ml-auto font-mono ${h.solDelta >= 0 ? "text-up" : "text-down"}`}>{signed(h.solDelta, 4)}</span>
              </a>
            ))}
          </div>
        </Panel>
      </div>
    </div>
  );
}
