import type { Server } from "node:http";
import { WebSocketServer, WebSocket } from "ws";
import { config } from "./config.js";
import { store } from "./tokens.js";

interface Client {
  ws: WebSocket;
  watching: Set<string>;
}

const clients = new Set<Client>();

export function broadcast(type: string, data: unknown) {
  const msg = JSON.stringify({ type, data });
  for (const c of clients) if (c.ws.readyState === WebSocket.OPEN) c.ws.send(msg);
}

export function startHub(server: Server) {
  const wss = new WebSocketServer({ server, path: "/ws" });
  wss.on("connection", (ws, req) => {
    const url = new URL(req.url ?? "/ws", "http://x");
    if (config.accessToken && url.searchParams.get("token") !== config.accessToken) {
      ws.close(4401, "Accès refusé");
      return;
    }
    const client: Client = { ws, watching: new Set() };
    clients.add(client);
    ws.on("message", (raw) => {
      let msg: any;
      try {
        msg = JSON.parse(raw.toString());
      } catch {
        return;
      }
      if (msg?.type === "watch" && typeof msg.mint === "string" && msg.mint.length >= 32 && msg.mint.length <= 44) {
        if (!client.watching.has(msg.mint)) {
          client.watching.add(msg.mint);
          store.watch(msg.mint);
        }
      } else if (msg?.type === "unwatch" && client.watching.has(msg.mint)) {
        client.watching.delete(msg.mint);
        store.unwatch(msg.mint);
      } else if (msg?.type === "ping") {
        ws.send(JSON.stringify({ type: "pong", data: Date.now() }));
      }
    });
    ws.on("close", () => {
      for (const m of client.watching) store.unwatch(m);
      clients.delete(client);
    });
  });

  store.on("new", (s) => broadcast("token:new", s));
  store.on("updates", (list) => broadcast("token:updates", list));
  store.on("migration", (s) => broadcast("token:migration", s));
  store.on("trade", (mint: string, p) => {
    const msg = JSON.stringify({ type: "trade", data: { mint, ...p } });
    for (const c of clients) if (c.watching.has(mint) && c.ws.readyState === WebSocket.OPEN) c.ws.send(msg);
  });
}
