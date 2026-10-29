import { useState } from "react";
import { useSettings } from "../lib/settings";
import { useLive, reconnectLive } from "../lib/live";
import { post } from "../lib/api";
import { Badge, Button, Field, inputCls, Panel, Toggle } from "../components/ui";

function Check({ ok, label, hint }: { ok: boolean | undefined; label: string; hint: string }) {
  return (
    <div className="flex items-start gap-2 py-1.5">
      <Badge tone={ok ? "up" : "down"}>{ok ? "OK" : "manquant"}</Badge>
      <div>
        <div className="text-[13px]">{label}</div>
        <div className="text-[11px] text-muted">{hint}</div>
      </div>
    </div>
  );
}

export default function Settings() {
  const s = useSettings();
  const status = useLive((x) => x.status);
  const authError = useLive((x) => x.authError);
  const [token, setToken] = useState(s.accessToken);
  const [presets, setPresets] = useState(s.buyPresets.join(", "));
  const [msg, setMsg] = useState("");

  const enableNotifs = async (v: boolean) => {
    if (v && "Notification" in window) {
      const p = await Notification.requestPermission();
      if (p !== "granted") {
        setMsg("Notifications refusées par le navigateur.");
        return;
      }
    }
    s.set({ notifications: v });
  };

  return (
    <div className="px-3 sm:px-4 pt-3 grid lg:grid-cols-2 gap-3 max-w-6xl">
      <Panel title="Accès au serveur">
        <div className="space-y-2">
          {authError && <div className="text-down text-[13px]">Jeton d'accès refusé ou manquant.</div>}
          <Field label="Jeton d'accès (ACCESS_TOKEN du serveur)" hint="Stocké uniquement dans ce navigateur.">
            <input className={inputCls} type="password" value={token} onChange={(e) => setToken(e.target.value)} />
          </Field>
          <Button
            size="sm"
            tone="brand"
            onClick={() => {
              s.set({ accessToken: token.trim() });
              reconnectLive();
              setMsg("Jeton enregistré, reconnexion…");
            }}
          >
            Enregistrer
          </Button>
        </div>
      </Panel>

      <Panel title="État des connexions">
        <Check ok={status?.feed?.connected} label="Flux temps réel PumpPortal" hint={status?.feed?.lastError || "wss://pumpportal.fun/api/data (gratuit)"} />
        <Check ok={status?.rpcConfigured} label="RPC Solana" hint="SOLANA_RPC_URL (Helius, QuickNode… — clé API requise pour un usage fiable)" />
        <Check ok={status?.solUsd != null} label="Prix SOL/USD" hint={status?.solPriceSource ? `Source : ${status.solPriceSource}` : "Jupiter / CoinGecko"} />
        <Check ok={status?.ai?.enabled} label="Grok (xAI)" hint="XAI_API_KEY — payant à l'usage sur console.x.ai" />
        <Check ok={status?.telegram} label="Notifications Telegram" hint="TELEGRAM_BOT_TOKEN + TELEGRAM_CHAT_ID (optionnel)" />
        <Check ok={status?.feed?.pumpSwapData} label="Données PumpSwap (après migration)" hint="PUMPPORTAL_API_KEY (optionnel, facturé par PumpPortal)" />
        {status?.telegram && (
          <Button size="sm" className="mt-2" onClick={() => post("/api/alerts/test-telegram").then(() => setMsg("Message Telegram envoyé ✓")).catch((e) => setMsg(e.message))}>
            Tester Telegram
          </Button>
        )}
      </Panel>

      <Panel title="Paramètres de transaction">
        <div className="space-y-3">
          <div className="grid grid-cols-2 gap-2">
            <Field label="Slippage (%)" hint="Écart de prix maximum accepté">
              <input className={inputCls} value={s.slippagePct} onChange={(e) => s.set({ slippagePct: Number(e.target.value.replace(",", ".")) || 0 })} inputMode="decimal" />
            </Field>
            <Field label="Frais de priorité (SOL)" hint="Accélère l'inclusion du bloc">
              <input className={inputCls} value={s.priorityFeeSol} onChange={(e) => s.set({ priorityFeeSol: Number(e.target.value.replace(",", ".")) || 0 })} inputMode="decimal" />
            </Field>
            <Field label="Route">
              <select className={inputCls + " !font-sans"} value={s.route} onChange={(e) => s.set({ route: e.target.value as any })}>
                <option value="pumpportal">PumpPortal (Pump.fun + PumpSwap, {status?.pumpPortalFeePct ?? 0.5} % de frais)</option>
                <option value="jupiter">Jupiter (agrégateur, devis avec minimum garanti)</option>
              </select>
            </Field>
            <Field label="Pool (PumpPortal)">
              <select className={inputCls + " !font-sans"} value={s.pool} onChange={(e) => s.set({ pool: e.target.value as any })}>
                <option value="auto">Auto</option>
                <option value="pump">Courbe Pump.fun</option>
                <option value="pump-amm">PumpSwap</option>
              </select>
            </Field>
          </div>
          <Field label="Montants d'achat rapides (SOL, séparés par des virgules)">
            <input
              className={inputCls}
              value={presets}
              onChange={(e) => setPresets(e.target.value)}
              onBlur={() => {
                const v = presets
                  .split(/[;,\s]+/)
                  .map((x) => Number(x.replace(",", ".")))
                  .filter((x) => x > 0)
                  .slice(0, 4);
                if (v.length) s.set({ buyPresets: v });
              }}
            />
          </Field>
        </div>
      </Panel>

      <Panel title="Notifications">
        <div className="space-y-3">
          <Toggle checked={s.notifications} onChange={enableNotifs} label="Notifications du navigateur (opportunités, risques, propositions)" />
          <Toggle checked={s.sound} onChange={(v) => s.set({ sound: v })} label="Son" />
          <p className="text-[12px] text-muted">
            Les notifications du navigateur ne fonctionnent que lorsque le terminal est ouvert. Pour être alerté 24 h/24 sur Android même application
            fermée, configurez Telegram côté serveur.
          </p>
        </div>
      </Panel>

      <Panel title="Sécurité" className="lg:col-span-2">
        <ul className="text-[13px] text-white/80 space-y-1 list-disc pl-5">
          <li>Ce terminal ne vous demandera jamais votre phrase de récupération ni votre clé privée. Si une page le fait, c'est une arnaque.</li>
          <li>Toutes les transactions sont construites, simulées, affichées avec leurs frais et risques, puis signées dans Phantom.</li>
          <li>Une transaction n'est affichée comme réussie qu'après confirmation sur Solana (statut « confirmed »).</li>
          <li>Sur Android : ouvrez le terminal dans le navigateur intégré de l'application Phantom, ou utilisez Chrome (Mobile Wallet Adapter).</li>
        </ul>
      </Panel>
      {msg && <div className="text-[13px] text-muted lg:col-span-2">{msg}</div>}
    </div>
  );
}
