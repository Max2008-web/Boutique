import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { PublicKey } from "@solana/web3.js";
import { useConnection, useWallet } from "@solana/wallet-adapter-react";
import { useWalletModal } from "@solana/wallet-adapter-react-ui";
import { useTradeFlow } from "../lib/trade";
import { useSettings } from "../lib/settings";
import { useSolBalance } from "../lib/hooks";
import { num, sol, usd } from "../lib/format";
import { Button, inputCls } from "./ui";

/** Solde réel du token dans le portefeuille connecté (lu sur Solana). */
export function useTokenBalance(mint: string) {
  const { publicKey } = useWallet();
  const { connection } = useConnection();
  const [bal, setBal] = useState<number | null>(null);
  const stage = useTradeFlow((s) => s.stage.kind);
  useEffect(() => {
    if (!publicKey) {
      setBal(null);
      return;
    }
    let alive = true;
    const load = () =>
      connection
        .getParsedTokenAccountsByOwner(publicKey, { mint: new PublicKey(mint) }, "confirmed")
        .then((r) => {
          if (!alive) return;
          const total = r.value.reduce((s, a) => s + Number(a.account.data.parsed.info.tokenAmount.uiAmountString ?? 0), 0);
          setBal(total);
        })
        .catch(() => {});
    load();
    const id = setInterval(load, 12_000);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, [publicKey, connection, mint, stage === "confirmed"]);
  return bal;
}

export function TradePanel({ mint, symbol, priceSol, solUsd }: { mint: string; symbol: string; priceSol: number | null; solUsd: number | null }) {
  const [side, setSide] = useState<"buy" | "sell">("buy");
  const [custom, setCustom] = useState("");
  const open = useTradeFlow((s) => s.open);
  const s = useSettings();
  const { publicKey } = useWallet();
  const { setVisible } = useWalletModal();
  const { balance } = useSolBalance();
  const tokenBal = useTokenBalance(mint);
  const holdingSol = tokenBal != null && priceSol != null ? tokenBal * priceSol : null;

  const buy = (amountSol: number) => {
    if (!publicKey) return setVisible(true);
    if (!(amountSol > 0)) return;
    open({ mint, side: "buy", amountSol });
  };
  const sell = (percent: number) => {
    if (!publicKey) return setVisible(true);
    open({ mint, side: "sell", percent });
  };

  return (
    <div className="space-y-3">
      <div className="grid grid-cols-2 bg-panel2 rounded-lg p-0.5">
        <button onClick={() => setSide("buy")} className={`py-2 rounded-md text-sm font-semibold ${side === "buy" ? "bg-up text-black" : "text-muted"}`}>
          Acheter
        </button>
        <button onClick={() => setSide("sell")} className={`py-2 rounded-md text-sm font-semibold ${side === "sell" ? "bg-down text-white" : "text-muted"}`}>
          Vendre
        </button>
      </div>

      <div className="flex justify-between text-[12px]">
        <span className="text-muted">Solde SOL</span>
        <span className="font-mono">{publicKey ? sol(balance, 4) : "non connecté"}</span>
      </div>
      <div className="flex justify-between text-[12px]">
        <span className="text-muted">Détenu</span>
        <span className="font-mono text-right">
          {publicKey ? `${num(tokenBal, 2)} ${symbol}` : "—"}
          {holdingSol != null && tokenBal ? (
            <span className="text-muted"> ≈ {sol(holdingSol, 4)}{solUsd ? ` (${usd(holdingSol * solUsd, 2)})` : ""}</span>
          ) : null}
        </span>
      </div>

      {side === "buy" ? (
        <>
          <div className="grid grid-cols-4 gap-1.5">
            {s.buyPresets.map((a) => (
              <Button key={a} tone="neutral" size="sm" className="font-mono py-2" onClick={() => buy(a)}>
                {a}
              </Button>
            ))}
          </div>
          <div className="flex gap-2">
            <input
              value={custom}
              onChange={(e) => setCustom(e.target.value.replace(",", "."))}
              inputMode="decimal"
              placeholder="Montant en SOL"
              className={inputCls}
            />
            <Button tone="buy" onClick={() => buy(Number(custom))} disabled={!(Number(custom) > 0)}>
              Acheter
            </Button>
          </div>
        </>
      ) : (
        <div className="grid grid-cols-4 gap-1.5">
          {[25, 50, 75, 100].map((p) => (
            <Button key={p} tone={p === 100 ? "sell" : "neutral"} size="sm" className="font-mono py-2.5" onClick={() => sell(p)} disabled={publicKey != null && !tokenBal}>
              {p} %
            </Button>
          ))}
        </div>
      )}

      <div className="text-[11px] text-muted flex flex-wrap gap-x-3 gap-y-1">
        <span>Slippage {s.slippagePct} %</span>
        <span>Priorité {s.priorityFeeSol} SOL</span>
        <span>Route {s.route}</span>
        <Link to="/reglages" className="text-brand hover:underline">
          modifier
        </Link>
      </div>
      <p className="text-[11px] text-muted leading-snug">
        Chaque ordre est d'abord simulé sur Solana, puis affiché avec ses frais, son slippage et ses risques. Rien n'est envoyé sans votre signature
        dans Phantom.
      </p>
    </div>
  );
}
