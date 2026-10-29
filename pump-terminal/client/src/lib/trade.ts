import { Connection, VersionedTransaction } from "@solana/web3.js";
import type { WalletContextState } from "@solana/wallet-adapter-react";
import { create } from "zustand";
import { post } from "./api";
import { useSettings } from "./settings";
import type { TradePreview } from "./types";

export interface TradeRequest {
  mint: string;
  side: "buy" | "sell";
  amountSol?: number;
  percent?: number;
  slippagePct?: number;
  proposalId?: string;
  source?: "manual" | "auto";
  label?: string;
}

export type Stage =
  | { kind: "idle" }
  | { kind: "preparing" }
  | { kind: "review"; preview: TradePreview }
  | { kind: "error"; message: string; preview?: TradePreview }
  | { kind: "signing"; preview: TradePreview }
  | { kind: "pending"; preview: TradePreview; signature: string; elapsed: number }
  | { kind: "confirmed"; preview: TradePreview; signature: string; recorded: string }
  | { kind: "failed"; preview: TradePreview; signature?: string; message: string }
  | { kind: "timeout"; preview: TradePreview; signature: string };

interface Flow {
  request: TradeRequest | null;
  stage: Stage;
  open: (r: TradeRequest) => void;
  close: () => void;
  setStage: (s: Stage) => void;
}

export const useTradeFlow = create<Flow>((set) => ({
  request: null,
  stage: { kind: "idle" },
  open: (request) => set({ request, stage: { kind: "preparing" } }),
  close: () => set({ request: null, stage: { kind: "idle" } }),
  setStage: (stage) => set({ stage }),
}));

export function makeConnection(): Connection {
  const token = useSettings.getState().accessToken;
  return new Connection(`${location.origin}/api/rpc`, {
    commitment: "confirmed",
    httpHeaders: token ? { "x-access-token": token } : undefined,
    disableRetryOnRateLimit: false,
  });
}

export async function prepare(req: TradeRequest, wallet: string): Promise<TradePreview> {
  const s = useSettings.getState();
  return post<TradePreview>("/api/trade/prepare", {
    wallet,
    mint: req.mint,
    side: req.side,
    amountSol: req.amountSol,
    percent: req.percent,
    slippagePct: req.slippagePct ?? s.slippagePct,
    priorityFeeSol: s.priorityFeeSol,
    route: s.route,
    pool: s.pool,
    source: req.source ?? "manual",
    proposalId: req.proposalId,
  });
}

/**
 * Fait signer la transaction par Phantom, puis attend la confirmation RÉELLE sur Solana.
 * La transaction n'est déclarée réussie que si getSignatureStatuses renvoie
 * "confirmed"/"finalized" sans erreur.
 */
export async function signAndConfirm(
  preview: TradePreview,
  wallet: WalletContextState,
  connection: Connection,
  onStage: (s: Stage) => void,
  req: TradeRequest,
): Promise<void> {
  if (!wallet.publicKey || !wallet.sendTransaction) throw new Error("Portefeuille non connecté");
  if (Date.now() - preview.preparedAt > 50_000) {
    onStage({ kind: "error", message: "L'aperçu a expiré (blockhash trop ancien). Relancez l'aperçu.", preview });
    return;
  }
  const tx = VersionedTransaction.deserialize(Uint8Array.from(atob(preview.txBase64), (c) => c.charCodeAt(0)));
  onStage({ kind: "signing", preview });
  let signature: string;
  try {
    signature = await wallet.sendTransaction(tx, connection, { maxRetries: 3, preflightCommitment: "confirmed" });
  } catch (e: any) {
    const msg = String(e?.message ?? e);
    onStage({
      kind: "failed",
      preview,
      message: /reject|denied|cancel/i.test(msg) ? "Signature refusée dans Phantom. Aucune transaction envoyée." : msg,
    });
    return;
  }
  const started = Date.now();
  onStage({ kind: "pending", preview, signature, elapsed: 0 });
  while (Date.now() - started < 90_000) {
    await new Promise((r) => setTimeout(r, 1500));
    try {
      const res = await connection.getSignatureStatuses([signature], { searchTransactionHistory: true });
      const st = res.value[0];
      if (st?.err) {
        onStage({ kind: "failed", preview, signature, message: `Échec on-chain : ${JSON.stringify(st.err)}` });
        post("/api/trades/record", { signature, wallet: wallet.publicKey.toBase58(), source: req.source ?? "manuel", proposalId: req.proposalId }).catch(() => {});
        return;
      }
      if (st && (st.confirmationStatus === "confirmed" || st.confirmationStatus === "finalized")) {
        let recorded = "";
        try {
          await post("/api/trades/record", {
            signature,
            wallet: wallet.publicKey.toBase58(),
            source: req.source === "auto" ? `auto:${req.label ?? ""}` : "manuel",
            proposalId: req.proposalId,
          });
          recorded = "Transaction vérifiée et enregistrée par le serveur.";
        } catch (e: any) {
          recorded = `Confirmée sur Solana, mais non enregistrée par le serveur : ${e?.message ?? e}`;
        }
        onStage({ kind: "confirmed", preview, signature, recorded });
        return;
      }
    } catch {}
    onStage({ kind: "pending", preview, signature, elapsed: Math.round((Date.now() - started) / 1000) });
  }
  onStage({ kind: "timeout", preview, signature });
}
