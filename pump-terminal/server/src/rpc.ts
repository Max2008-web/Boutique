import { config } from "./config.js";
import { fetchJson } from "./util.js";

let nextId = 1;

export class RpcError extends Error {
  constructor(message: string, public code?: number, public data?: unknown) {
    super(message);
  }
}

export async function rpc<T = any>(method: string, params: unknown[] = [], timeoutMs = 15_000): Promise<T> {
  if (!config.rpcUrl) throw new RpcError("SOLANA_RPC_URL n'est pas configurée sur le serveur.");
  const body = { jsonrpc: "2.0", id: nextId++, method, params };
  const res = await fetchJson<any>(config.rpcUrl, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(body),
    timeoutMs,
  });
  if (res?.error) throw new RpcError(`${method}: ${res.error.message}`, res.error.code, res.error.data);
  return res.result as T;
}

/** Méthodes autorisées via le proxy RPC exposé au navigateur (la clé du RPC reste côté serveur). */
const ALLOWED = new Set([
  "getLatestBlockhash",
  "getSignatureStatuses",
  "getBalance",
  "getAccountInfo",
  "getMultipleAccounts",
  "getTokenAccountBalance",
  "getTokenAccountsByOwner",
  "getParsedTokenAccountsByOwner",
  "getFeeForMessage",
  "getMinimumBalanceForRentExemption",
  "getRecentPrioritizationFees",
  "getBlockHeight",
  "getSlot",
  "getEpochInfo",
  "getGenesisHash",
  "getVersion",
  "getTransaction",
  "isBlockhashValid",
  "sendTransaction",
  "simulateTransaction",
]);

export async function proxyRpc(payload: any): Promise<any> {
  const items = Array.isArray(payload) ? payload : [payload];
  if (items.length > 20) throw new RpcError("Batch RPC trop volumineux");
  for (const it of items) {
    if (!it || typeof it.method !== "string" || !ALLOWED.has(it.method)) {
      throw new RpcError(`Méthode RPC non autorisée : ${it?.method}`);
    }
  }
  if (!config.rpcUrl) throw new RpcError("SOLANA_RPC_URL n'est pas configurée sur le serveur.");
  return fetchJson(config.rpcUrl, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(payload),
    timeoutMs: 30_000,
  });
}
