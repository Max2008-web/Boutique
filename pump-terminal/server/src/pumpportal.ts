import { EventEmitter } from "node:events";
import WebSocket from "ws";
import { config } from "./config.js";

/**
 * Client du flux de données temps réel PumpPortal (https://pumpportal.fun).
 * Une seule connexion WebSocket est partagée par tout le serveur (recommandation PumpPortal).
 *
 * Événements émis : "create", "trade", "migration", "status".
 */
export interface PumpCreate {
  signature: string;
  mint: string;
  traderPublicKey: string;
  txType: "create";
  initialBuy?: number;
  solAmount?: number;
  bondingCurveKey?: string;
  vTokensInBondingCurve?: number;
  vSolInBondingCurve?: number;
  marketCapSol?: number;
  name?: string;
  symbol?: string;
  uri?: string;
  pool?: string;
}

export interface PumpTrade {
  signature: string;
  mint: string;
  traderPublicKey: string;
  txType: "buy" | "sell";
  tokenAmount: number;
  solAmount?: number;
  newTokenBalance?: number;
  bondingCurveKey?: string;
  vTokensInBondingCurve?: number;
  vSolInBondingCurve?: number;
  marketCapSol?: number;
  pool?: string;
}

class PumpPortalFeed extends EventEmitter {
  private ws?: WebSocket;
  private tokenSubs = new Set<string>();
  private accountSubs = new Set<string>();
  private backoff = 1000;
  connected = false;
  lastMessageAt = 0;
  messages = 0;
  lastError = "";

  start() {
    this.connect();
    // Si aucun message depuis 60 s, on force une reconnexion.
    setInterval(() => {
      if (this.connected && Date.now() - this.lastMessageAt > 60_000) {
        this.lastError = "Aucun message depuis 60 s, reconnexion";
        this.ws?.terminate();
      }
    }, 15_000).unref();
  }

  private url() {
    return config.pumpPortalApiKey
      ? `${config.pumpPortalWs}?api-key=${encodeURIComponent(config.pumpPortalApiKey)}`
      : config.pumpPortalWs;
  }

  private connect() {
    const ws = new WebSocket(this.url());
    this.ws = ws;
    ws.on("open", () => {
      this.connected = true;
      this.backoff = 1000;
      this.lastMessageAt = Date.now();
      this.send({ method: "subscribeNewToken" });
      this.send({ method: "subscribeMigration" });
      const keys = [...this.tokenSubs];
      for (let i = 0; i < keys.length; i += 100) this.send({ method: "subscribeTokenTrade", keys: keys.slice(i, i + 100) });
      if (this.accountSubs.size) this.send({ method: "subscribeAccountTrade", keys: [...this.accountSubs] });
      this.emit("status", this.status());
      console.log("[pumpportal] connecté");
    });
    ws.on("message", (raw) => {
      this.lastMessageAt = Date.now();
      this.messages++;
      let msg: any;
      try {
        msg = JSON.parse(raw.toString());
      } catch {
        return;
      }
      if (!msg || typeof msg !== "object") return;
      if (msg.errors) {
        this.lastError = String(msg.errors);
        return;
      }
      const t = msg.txType;
      if (t === "create" && msg.mint) this.emit("create", msg as PumpCreate);
      else if ((t === "buy" || t === "sell") && msg.mint) this.emit("trade", msg as PumpTrade);
      else if ((t === "migrate" || t === "migration" || msg.method === "migration") && msg.mint) this.emit("migration", msg);
    });
    ws.on("close", () => {
      this.connected = false;
      this.emit("status", this.status());
      const delay = this.backoff;
      this.backoff = Math.min(this.backoff * 2, 30_000);
      setTimeout(() => this.connect(), delay);
    });
    ws.on("error", (err) => {
      this.lastError = err.message;
      console.warn("[pumpportal] erreur :", err.message);
    });
  }

  private send(obj: unknown) {
    if (this.ws?.readyState === WebSocket.OPEN) this.ws.send(JSON.stringify(obj));
  }

  subscribeTokens(mints: string[]) {
    const fresh = mints.filter((m) => !this.tokenSubs.has(m));
    if (!fresh.length) return;
    fresh.forEach((m) => this.tokenSubs.add(m));
    this.send({ method: "subscribeTokenTrade", keys: fresh });
  }

  unsubscribeTokens(mints: string[]) {
    const gone = mints.filter((m) => this.tokenSubs.delete(m));
    if (gone.length) this.send({ method: "unsubscribeTokenTrade", keys: gone });
  }

  subscribeAccount(address: string) {
    if (this.accountSubs.has(address)) return;
    this.accountSubs.add(address);
    this.send({ method: "subscribeAccountTrade", keys: [address] });
  }

  status() {
    return {
      connected: this.connected,
      lastMessageAt: this.lastMessageAt,
      messages: this.messages,
      tokenSubscriptions: this.tokenSubs.size,
      lastError: this.lastError,
      pumpSwapData: Boolean(config.pumpPortalApiKey),
    };
  }
}

export const feed = new PumpPortalFeed();
