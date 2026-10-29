import { config } from "./config.js";
import { AlertItem, db, save } from "./db.js";
import { broadcast } from "./hub.js";
import { errMsg, fetchJson, uid } from "./util.js";

export async function sendTelegram(text: string): Promise<void> {
  if (!config.telegramToken || !config.telegramChatId) throw new Error("Telegram non configuré");
  await fetchJson(`https://api.telegram.org/bot${config.telegramToken}/sendMessage`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ chat_id: config.telegramChatId, text, disable_web_page_preview: true }),
    timeoutMs: 8000,
  });
}

/** Enregistre une alerte, la pousse en temps réel aux navigateurs connectés et sur Telegram si configuré. */
export function alert(level: AlertItem["level"], title: string, body: string, mint?: string): AlertItem {
  const item: AlertItem = { id: uid("al_"), at: Date.now(), level, title, body, mint };
  db.alerts.push(item);
  save();
  broadcast("alert", item);
  if (config.telegramToken && config.telegramChatId && level !== "info") {
    const link = mint ? `\nhttps://pump.fun/coin/${mint}` : "";
    sendTelegram(`${title}\n${body}${link}`).catch((e) => console.warn("[telegram]", errMsg(e)));
  }
  return item;
}
