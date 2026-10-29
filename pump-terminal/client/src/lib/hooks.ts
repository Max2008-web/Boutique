import { useEffect, useState } from "react";
import { useConnection, useWallet } from "@solana/wallet-adapter-react";
import { api, post } from "./api";

/** Solde SOL réel du portefeuille connecté, lu sur Solana toutes les 15 s. */
export function useSolBalance() {
  const { publicKey } = useWallet();
  const { connection } = useConnection();
  const [balance, setBalance] = useState<number | null>(null);
  const [tick, setTick] = useState(0);
  useEffect(() => {
    if (!publicKey) {
      setBalance(null);
      return;
    }
    let alive = true;
    const load = () =>
      connection
        .getBalance(publicKey, "confirmed")
        .then((l) => alive && setBalance(l / 1e9))
        .catch(() => {});
    load();
    const id = setInterval(load, 15_000);
    return () => {
      alive = false;
      clearInterval(id);
    };
  }, [publicKey, connection, tick]);
  return { balance, refresh: () => setTick((t) => t + 1) };
}

/** Déclare l'adresse PUBLIQUE du portefeuille au serveur (surveillance des positions). */
export function useRegisterWallet() {
  const { publicKey } = useWallet();
  useEffect(() => {
    if (publicKey) post("/api/wallet/register", { address: publicKey.toBase58() }).catch(() => {});
  }, [publicKey]);
}

export function usePoll<T>(path: string | null, intervalMs = 0, deps: unknown[] = []) {
  const [data, setData] = useState<T | null>(null);
  const [error, setError] = useState<string>("");
  const [loading, setLoading] = useState(false);
  const [tick, setTick] = useState(0);
  useEffect(() => {
    if (!path) return;
    let alive = true;
    const load = () => {
      setLoading(true);
      api<T>(path)
        .then((d) => {
          if (!alive) return;
          setData(d);
          setError("");
        })
        .catch((e) => alive && setError(e.message))
        .finally(() => alive && setLoading(false));
    };
    load();
    const id = intervalMs ? setInterval(load, intervalMs) : undefined;
    return () => {
      alive = false;
      if (id) clearInterval(id);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [path, intervalMs, tick, ...deps]);
  return { data, error, loading, reload: () => setTick((t) => t + 1), setData };
}

export function useNow(ms = 1000) {
  const [now, setNow] = useState(Date.now());
  useEffect(() => {
    const id = setInterval(() => setNow(Date.now()), ms);
    return () => clearInterval(id);
  }, [ms]);
  return now;
}
