import { create } from "zustand";
import { api } from "./api";
import { useSettings } from "./settings";
import type { AlertItem, Proposal, ServerStatus, TokenSummary, TradePoint } from "./types";

interface LiveState {
  connected: boolean;
  authError: boolean;
  status: ServerStatus | null;
  tokens: Record<string, TokenSummary>;
  alerts: AlertItem[];
  proposals: Proposal[];
  upsert: (list: TokenSummary[]) => void;
}

export const useLive = create<LiveState>((set) => ({
  connected: false,
  authError: false,
  status: null,
  tokens: {},
  alerts: [],
  proposals: [],
  upsert: (list) =>
    set((s) => {
      const tokens = { ...s.tokens };
      for (const t of list) tokens[t.mint] = t;
      const keys = Object.keys(tokens);
      if (keys.length > 2500) {
        keys
          .sort((a, b) => Math.max(tokens[a].lastTradeAt, tokens[a].firstSeenAt) - Math.max(tokens[b].lastTradeAt, tokens[b].firstSeenAt))
          .slice(0, keys.length - 2000)
          .forEach((k) => delete tokens[k]);
      }
      return { tokens };
    }),
}));

type TradeListener = (p: TradePoint) => void;
const tradeListeners = new Map<string, Set<TradeListener>>();
const watchCounts = new Map<string, number>();
let ws: WebSocket | null = null;
let retry = 1000;

function send(obj: unknown) {
  if (ws?.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

/** Abonne le composant aux trades en direct d'un token. Retourne la fonction de désabonnement. */
export function watchTrades(mint: string, fn: TradeListener): () => void {
  const set = tradeListeners.get(mint) ?? new Set();
  set.add(fn);
  tradeListeners.set(mint, set);
  const n = (watchCounts.get(mint) ?? 0) + 1;
  watchCounts.set(mint, n);
  if (n === 1) send({ type: "watch", mint });
  return () => {
    set.delete(fn);
    const m = (watchCounts.get(mint) ?? 1) - 1;
    watchCounts.set(mint, m);
    if (m <= 0) {
      watchCounts.delete(mint);
      send({ type: "unwatch", mint });
    }
  };
}

let audioCtx: AudioContext | null = null;
function beep(freq = 880) {
  if (!useSettings.getState().sound) return;
  try {
    audioCtx ??= new AudioContext();
    const o = audioCtx.createOscillator();
    const g = audioCtx.createGain();
    o.frequency.value = freq;
    g.gain.setValueAtTime(0.08, audioCtx.currentTime);
    g.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime + 0.25);
    o.connect(g).connect(audioCtx.destination);
    o.start();
    o.stop(audioCtx.currentTime + 0.25);
  } catch {}
}

function notify(a: AlertItem) {
  if (a.level === "info") return;
  if (useSettings.getState().notifications && "Notification" in window && Notification.permission === "granted") {
    try {
      new Notification(a.title, { body: a.body, icon: "/icon.svg", tag: a.id });
    } catch {}
  }
  beep(a.level === "risk" ? 440 : 880);
}

async function loadInitial() {
  const [n, g, m, a, alerts, proposals, status] = await Promise.all([
    api<TokenSummary[]>("/api/tokens?list=new&limit=120"),
    api<TokenSummary[]>("/api/tokens?list=graduating&limit=60"),
    api<TokenSummary[]>("/api/tokens?list=migrated&limit=60"),
    api<TokenSummary[]>("/api/tokens?list=active&limit=60"),
    api<AlertItem[]>("/api/alerts"),
    api<Proposal[]>("/api/proposals"),
    api<ServerStatus>("/api/status"),
  ]);
  useLive.getState().upsert([...n, ...g, ...m, ...a]);
  useLive.setState({ alerts, proposals, status, authError: false });
}

export function connectLive() {
  const token = useSettings.getState().accessToken;
  const proto = location.protocol === "https:" ? "wss" : "ws";
  ws = new WebSocket(`${proto}://${location.host}/ws${token ? `?token=${encodeURIComponent(token)}` : ""}`);
  ws.onopen = () => {
    retry = 1000;
    useLive.setState({ connected: true });
    for (const mint of watchCounts.keys()) send({ type: "watch", mint });
    loadInitial().catch((e) => {
      if (e?.status === 401) useLive.setState({ authError: true });
    });
  };
  ws.onclose = (ev) => {
    useLive.setState({ connected: false, authError: ev.code === 4401 });
    setTimeout(connectLive, retry);
    retry = Math.min(retry * 2, 15000);
  };
  ws.onmessage = (ev) => {
    let msg: { type: string; data: any };
    try {
      msg = JSON.parse(ev.data);
    } catch {
      return;
    }
    const st = useLive.getState();
    switch (msg.type) {
      case "token:new":
      case "token:migration":
        st.upsert([msg.data]);
        break;
      case "token:updates":
        st.upsert(msg.data);
        break;
      case "trade":
        tradeListeners.get(msg.data.mint)?.forEach((fn) => fn(msg.data));
        break;
      case "alert":
        useLive.setState({ alerts: [msg.data, ...st.alerts].slice(0, 300) });
        notify(msg.data);
        break;
      case "proposal":
        useLive.setState({ proposals: [msg.data, ...st.proposals] });
        break;
      case "proposal:update":
        useLive.setState({ proposals: st.proposals.map((p) => (p.id === msg.data.id ? msg.data : p)) });
        break;
      case "killswitch":
        useLive.setState({ status: st.status ? { ...st.status, killSwitch: msg.data } : st.status });
        if (msg.data) useLive.setState({ proposals: st.proposals.map((p) => (p.status === "pending" ? { ...p, status: "rejected" } : p)) });
        break;
      case "status":
        useLive.setState({ status: { ...(st.status ?? ({} as ServerStatus)), ...msg.data } });
        break;
    }
  };
}

export function reconnectLive() {
  ws?.close();
}
