import { ReactNode, useCallback, useEffect, useState } from "react";
import { useConnection, useWallet } from "@solana/wallet-adapter-react";
import { useWalletModal } from "@solana/wallet-adapter-react-ui";
import { prepare, signAndConfirm, useTradeFlow } from "../lib/trade";
import { num, pct, price, sol, solscanTx, usd } from "../lib/format";
import { useLive } from "../lib/live";
import { post } from "../lib/api";
import type { TradePreview } from "../lib/types";
import { Badge, Button, Spinner } from "./ui";

function Row({ label, value, tone, hint }: { label: string; value: ReactNode; tone?: "up" | "down" | "warn"; hint?: string }) {
  const c = tone === "up" ? "text-up" : tone === "down" ? "text-down" : tone === "warn" ? "text-warn" : "text-white";
  return (
    <div className="flex items-start justify-between gap-3 py-1.5 border-b border-line/60 last:border-0">
      <span className="text-muted text-[13px]" title={hint}>
        {label}
      </span>
      <span className={`font-mono text-[13px] text-right ${c}`}>{value}</span>
    </div>
  );
}

function PreviewBody({ p }: { p: TradePreview }) {
  const [showLogs, setShowLogs] = useState(false);
  const usdOf = (v: number | null | undefined) => (v != null && p.solUsd ? ` (${usd(v * p.solUsd, 2)})` : "");
  const sim = p.simulation;
  const totalFees = p.fees.baseFeeSol + p.fees.priorityFeeSol;
  return (
    <div className="space-y-4">
      <div>
        <h3 className="text-xs uppercase tracking-wide text-muted mb-1">Résultat simulé sur Solana (état actuel)</h3>
        {sim.ok ? (
          <div className="bg-panel2 rounded-lg px-3">
            <Row
              label={p.side === "buy" ? "SOL dépensés (frais inclus)" : "SOL reçus (frais déduits)"}
              value={sim.solChange != null ? `${sol(Math.abs(sim.solChange), 5)}${usdOf(Math.abs(sim.solChange))}` : "—"}
              tone={p.side === "buy" ? "down" : "up"}
            />
            <Row
              label={p.side === "buy" ? "Tokens reçus (estimés)" : "Tokens vendus"}
              value={sim.tokenChange != null ? `${num(Math.abs(sim.tokenChange), 2)} ${p.symbol}` : "—"}
            />
            {p.minOut != null && (
              <Row label="Minimum garanti (slippage)" value={p.side === "buy" ? `${num(p.minOut, 2)} ${p.symbol}` : sol(p.minOut, 5)} />
            )}
            <Row label="Prix spot" value={price(p.spotPriceSol, "") + " SOL"} />
            <Row label="Prix effectif" value={price(p.effectivePriceSol, "") + " SOL"} />
            <Row
              label="Écart vs spot (impact + frais)"
              value={pct(p.priceImpactPct, 2)}
              tone={p.priceImpactPct != null && p.priceImpactPct > 5 ? "warn" : undefined}
            />
          </div>
        ) : (
          <div className="bg-down/10 border border-down/30 rounded-lg p-3 text-sm text-down">
            La simulation a échoué : cette transaction échouerait probablement. {sim.error}
          </div>
        )}
      </div>

      <div>
        <h3 className="text-xs uppercase tracking-wide text-muted mb-1">Frais et paramètres</h3>
        <div className="bg-panel2 rounded-lg px-3">
          <Row label="Frais réseau de base" value={sol(p.fees.baseFeeSol, 6)} />
          <Row
            label="Frais de priorité"
            value={sol(p.fees.priorityFeeSol, 6)}
            hint={p.fees.computeUnitPriceMicroLamports != null ? `${p.fees.computeUnitPriceMicroLamports} µlamports × ${p.fees.computeUnitLimit} CU` : undefined}
          />
          <Row label="Total frais réseau" value={`${sol(totalFees, 6)}${usdOf(totalFees)}`} />
          <Row
            label={`Frais plateforme (${p.route === "pumpportal" ? "PumpPortal" : "Jupiter"})`}
            value={p.fees.platformFeePct ? `${p.fees.platformFeePct} % ≈ ${sol(p.fees.platformFeeSolEst, 5)}` : "0 %"}
          />
          <Row label="Frais du protocole Pump.fun / PumpSwap" value="inclus dans le prix effectif" />
          <Row label="Slippage maximum accepté" value={`${p.slippagePct} %`} tone={p.slippagePct > 20 ? "warn" : undefined} />
          <Row label="Route" value={`${p.route} · ${p.pool}`} />
          {p.fees.rentNote && <div className="text-[12px] text-muted py-1.5">{p.fees.rentNote}</div>}
        </div>
      </div>

      <div>
        <h3 className="text-xs uppercase tracking-wide text-muted mb-1 flex items-center gap-2">
          Risques
          <Badge tone={p.risk.score >= 45 ? "down" : p.risk.score >= 20 ? "warn" : "up"}>
            {p.risk.score}/100 · {p.risk.level}
          </Badge>
        </h3>
        <ul className="space-y-1">
          {p.warnings.map((w, i) => (
            <li key={i} className="text-[13px] text-warn flex gap-2">
              <span>⚠</span>
              <span>{w}</span>
            </li>
          ))}
        </ul>
      </div>

      {sim.logs.length > 0 && (
        <div>
          <button className="text-[11px] text-muted underline" onClick={() => setShowLogs((v) => !v)}>
            {showLogs ? "Masquer" : "Voir"} les logs de simulation
          </button>
          {showLogs && (
            <pre className="mt-1 text-[10px] bg-black/40 rounded p-2 overflow-x-auto max-h-40 text-white/60">{sim.logs.join("\n")}</pre>
          )}
        </div>
      )}
    </div>
  );
}

export function TradeModal() {
  const { request, stage, close, setStage } = useTradeFlow();
  const wallet = useWallet();
  const { connection } = useConnection();
  const { setVisible } = useWalletModal();
  const [ack, setAck] = useState(false);
  const symbol = useLive((s) => (request ? s.tokens[request.mint]?.symbol : ""));

  const run = useCallback(async () => {
    if (!request) return;
    if (!wallet.publicKey) {
      setStage({ kind: "error", message: "Connectez votre portefeuille Phantom pour continuer." });
      return;
    }
    setStage({ kind: "preparing" });
    setAck(false);
    try {
      const p = await prepare(request, wallet.publicKey.toBase58());
      setStage({ kind: "review", preview: p });
    } catch (e: any) {
      setStage({ kind: "error", message: e?.message ?? String(e) });
    }
  }, [request, wallet.publicKey, setStage]);

  useEffect(() => {
    if (request && stage.kind === "preparing") run();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [request, wallet.publicKey]);

  if (!request) return null;
  const busy = stage.kind === "signing" || stage.kind === "pending";
  const preview = "preview" in stage ? stage.preview : undefined;
  const sym = preview?.symbol || symbol || request.mint.slice(0, 6);
  const title =
    request.side === "buy" ? `Acheter $${sym} — ${request.amountSol} SOL` : `Vendre ${request.percent} % de $${sym}`;

  const reject = () => {
    if (request.proposalId) post(`/api/proposals/${request.proposalId}/reject`).catch(() => {});
    close();
  };

  return (
    <div className="fixed inset-0 z-50 bg-black/70 backdrop-blur-sm flex items-end sm:items-center justify-center" onClick={() => !busy && close()}>
      <div
        className="bg-panel border border-line w-full sm:max-w-lg max-h-[92vh] overflow-y-auto rounded-t-2xl sm:rounded-2xl"
        onClick={(e) => e.stopPropagation()}
      >
        <div className="sticky top-0 bg-panel border-b border-line px-4 py-3 flex items-center justify-between">
          <div>
            <div className={`font-semibold ${request.side === "buy" ? "text-up" : "text-down"}`}>{title}</div>
            {request.proposalId && <div className="text-[11px] text-warn">Proposition automatique — votre validation est requise</div>}
          </div>
          {!busy && (
            <button onClick={close} className="text-muted hover:text-white text-xl leading-none px-2">
              ×
            </button>
          )}
        </div>
        <div className="p-4 space-y-4">
          {stage.kind === "preparing" && (
            <div className="flex items-center gap-3 text-sm text-muted py-8 justify-center">
              <Spinner /> Construction et simulation de la transaction sur Solana…
            </div>
          )}

          {stage.kind === "error" && (
            <div className="space-y-3">
              <div className="bg-down/10 border border-down/30 rounded-lg p-3 text-sm text-down">{stage.message}</div>
              <div className="flex gap-2">
                {!wallet.publicKey ? (
                  <Button tone="brand" onClick={() => setVisible(true)}>
                    Connecter Phantom
                  </Button>
                ) : (
                  <Button onClick={run}>Réessayer</Button>
                )}
                <Button tone="ghost" onClick={close}>
                  Fermer
                </Button>
              </div>
            </div>
          )}

          {stage.kind === "review" && (
            <>
              <PreviewBody p={stage.preview} />
              <label className="flex items-start gap-2 text-[13px] text-white/80 bg-panel2 rounded-lg p-3">
                <input type="checkbox" checked={ack} onChange={(e) => setAck(e.target.checked)} className="mt-0.5" />
                <span>
                  J'ai vérifié les montants, les frais, le slippage et les risques. Je comprends que cette transaction est réelle et
                  irréversible.
                </span>
              </label>
              <div className="flex gap-2">
                <Button
                  tone={request.side === "buy" ? "buy" : "sell"}
                  size="lg"
                  className="flex-1"
                  disabled={!ack || !stage.preview.simulation.ok}
                  onClick={() => signAndConfirm(stage.preview, wallet, connection, setStage, request)}
                >
                  Signer avec Phantom
                </Button>
                <Button onClick={run} size="lg" tone="neutral">
                  ↻
                </Button>
              </div>
              <button className="w-full text-center text-[12px] text-muted hover:text-white" onClick={reject}>
                {request.proposalId ? "Refuser la proposition" : "Annuler"}
              </button>
            </>
          )}

          {stage.kind === "signing" && (
            <div className="flex flex-col items-center gap-3 py-8 text-sm text-center">
              <Spinner className="w-8 h-8" />
              <div>Validez la transaction dans Phantom…</div>
              <div className="text-muted text-[12px]">Vérifiez dans Phantom que les montants correspondent à l'aperçu.</div>
            </div>
          )}

          {stage.kind === "pending" && (
            <div className="flex flex-col items-center gap-3 py-8 text-sm text-center">
              <Spinner className="w-8 h-8" />
              <div className="text-warn font-medium">Transaction envoyée — en attente de confirmation Solana ({stage.elapsed} s)</div>
              <div className="text-muted text-[12px]">Elle n'est PAS encore réussie. Ne fermez pas cette fenêtre.</div>
              <a className="text-brand underline text-[12px]" href={solscanTx(stage.signature)} target="_blank" rel="noreferrer">
                Suivre sur Solscan
              </a>
            </div>
          )}

          {stage.kind === "confirmed" && (
            <div className="flex flex-col items-center gap-3 py-6 text-center">
              <div className="w-14 h-14 rounded-full bg-up/15 text-up grid place-items-center text-3xl">✓</div>
              <div className="text-up font-semibold">Confirmée sur Solana</div>
              <div className="text-muted text-[12px]">{stage.recorded}</div>
              <a className="text-brand underline text-[12px]" href={solscanTx(stage.signature)} target="_blank" rel="noreferrer">
                Voir la transaction sur Solscan
              </a>
              <Button onClick={close}>Fermer</Button>
            </div>
          )}

          {stage.kind === "failed" && (
            <div className="space-y-3 text-center py-4">
              <div className="w-14 h-14 mx-auto rounded-full bg-down/15 text-down grid place-items-center text-3xl">×</div>
              <div className="text-down font-semibold">Transaction non réalisée</div>
              <div className="text-[13px] text-white/80 break-words">{stage.message}</div>
              {stage.signature && (
                <a className="text-brand underline text-[12px]" href={solscanTx(stage.signature)} target="_blank" rel="noreferrer">
                  Voir sur Solscan
                </a>
              )}
              <div className="flex gap-2 justify-center">
                <Button onClick={run}>Nouvel aperçu</Button>
                <Button tone="ghost" onClick={close}>
                  Fermer
                </Button>
              </div>
            </div>
          )}

          {stage.kind === "timeout" && (
            <div className="space-y-3 text-center py-4">
              <div className="text-warn font-semibold">Statut inconnu après 90 s</div>
              <div className="text-[13px] text-white/80">
                La transaction n'a pas été confirmée dans le délai. Elle a pu expirer ou être encore en cours. Vérifiez sur Solscan avant de
                réessayer pour éviter un double achat.
              </div>
              <a className="text-brand underline text-[12px]" href={solscanTx(stage.signature)} target="_blank" rel="noreferrer">
                Vérifier sur Solscan
              </a>
              <div>
                <Button tone="ghost" onClick={close}>
                  Fermer
                </Button>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
}
