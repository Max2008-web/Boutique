import { useEffect, useMemo, useState } from "react";
import { useParams } from "react-router-dom";
import { useWallet } from "@solana/wallet-adapter-react";
import { api, post } from "../lib/api";
import { useLive, watchTrades } from "../lib/live";
import { usePoll } from "../lib/hooks";
import { age, ago, num, pct, price, short, sol, solscanAccount, solscanTx, usd } from "../lib/format";
import type { AiResult, TokenDetail, TradePoint } from "../lib/types";
import { CandleChart } from "../components/CandleChart";
import { TradePanel } from "../components/TradePanel";
import { AiView } from "../components/AiView";
import { Badge, Bar, Button, Copy, Empty, inputCls, Panel, Spinner, Stat, TokenAvatar } from "../components/ui";

function TradesTable({ mint, creator, me }: { mint: string; creator?: string; me?: string }) {
  const [trades, setTrades] = useState<TradePoint[]>([]);
  const solUsd = useLive((s) => s.status?.solUsd ?? null);
  useEffect(() => {
    let alive = true;
    api<TradePoint[]>(`/api/tokens/${mint}/trades`).then((t) => alive && setTrades(t)).catch(() => {});
    const off = watchTrades(mint, (p) => setTrades((list) => [p, ...list].slice(0, 200)));
    return () => {
      alive = false;
      off();
    };
  }, [mint]);
  if (!trades.length) return <Empty>Aucune transaction observée depuis l'ouverture du suivi. Les nouvelles transactions apparaîtront ici en direct.</Empty>;
  return (
    <div className="overflow-x-auto">
      <table className="w-full text-[12px] font-mono">
        <thead className="text-muted text-[11px]">
          <tr className="text-left">
            <th className="py-1.5 px-2 font-normal">Âge</th>
            <th className="px-2 font-normal">Type</th>
            <th className="px-2 font-normal text-right">SOL</th>
            <th className="px-2 font-normal text-right hidden sm:table-cell">USD</th>
            <th className="px-2 font-normal text-right">Tokens</th>
            <th className="px-2 font-normal text-right hidden sm:table-cell">MC</th>
            <th className="px-2 font-normal">Trader</th>
            <th className="px-2 font-normal" />
          </tr>
        </thead>
        <tbody>
          {trades.map((t) => (
            <tr key={t.sig + t.side} className="border-t border-line/60 hover:bg-white/[0.02]">
              <td className="py-1 px-2 text-muted">{ago(t.t)}</td>
              <td className={`px-2 ${t.side === "buy" ? "text-up" : "text-down"}`}>{t.side === "buy" ? "Achat" : "Vente"}</td>
              <td className={`px-2 text-right ${t.side === "buy" ? "text-up" : "text-down"}`}>{num(t.sol, 3)}</td>
              <td className="px-2 text-right text-muted hidden sm:table-cell">{solUsd ? usd(t.sol * solUsd, 0) : "—"}</td>
              <td className="px-2 text-right">{num(t.tokens, 1)}</td>
              <td className="px-2 text-right text-muted hidden sm:table-cell">{t.mcapSol && solUsd ? usd(t.mcapSol * solUsd) : "—"}</td>
              <td className="px-2">
                <a href={solscanAccount(t.trader)} target="_blank" rel="noreferrer" className="hover:text-white text-white/70">
                  {short(t.trader, 4)}
                </a>
                {t.trader === creator && <Badge tone="warn">DEV</Badge>}
                {t.trader === me && <Badge tone="brand">VOUS</Badge>}
              </td>
              <td className="px-2">
                <a href={solscanTx(t.sig)} target="_blank" rel="noreferrer" className="text-muted hover:text-white">
                  ↗
                </a>
              </td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

function AlertCreator({ mint, symbol }: { mint: string; symbol: string }) {
  const [kind, setKind] = useState("mcap_above");
  const [value, setValue] = useState("");
  const [msg, setMsg] = useState("");
  const needsValue = kind === "mcap_above" || kind === "mcap_below" || kind === "big_buy";
  const create = async () => {
    try {
      await post("/api/automation/watch", { mint, symbol, kind, value: needsValue ? Number(value) : undefined, repeat: false });
      setMsg("Alerte créée ✓ (surveillée 24 h/24 par le serveur)");
      setValue("");
    } catch (e: any) {
      setMsg(e.message);
    }
  };
  return (
    <div className="space-y-2">
      <div className="flex gap-2">
        <select value={kind} onChange={(e) => setKind(e.target.value)} className={inputCls + " !font-sans"}>
          <option value="mcap_above">MC au-dessus de ($)</option>
          <option value="mcap_below">MC en dessous de ($)</option>
          <option value="big_buy">Achat supérieur à (SOL)</option>
          <option value="dev_sold">Le dev vend</option>
          <option value="migration">Migration PumpSwap</option>
        </select>
        {needsValue && <input className={inputCls} value={value} onChange={(e) => setValue(e.target.value)} placeholder="Valeur" inputMode="decimal" />}
      </div>
      <Button size="sm" onClick={create} disabled={needsValue && !(Number(value) > 0)}>
        Créer l'alerte
      </Button>
      {msg && <div className="text-[12px] text-muted">{msg}</div>}
    </div>
  );
}

export default function Token() {
  const { mint = "" } = useParams();
  const { publicKey } = useWallet();
  const live = useLive((s) => s.tokens[mint]);
  const { data, error, loading } = usePoll<TokenDetail>(`/api/tokens/${mint}`, 20_000);
  const [ai, setAi] = useState<AiResult | null>(null);
  const [aiLoading, setAiLoading] = useState(false);
  const [aiErr, setAiErr] = useState("");
  const aiEnabled = useLive((s) => s.status?.ai?.enabled);

  useEffect(() => {
    setAi(null);
    setAiErr("");
    api<AiResult[]>(`/api/ai/history?mint=${mint}`).then((h) => setAi(h[0] ?? null)).catch(() => {});
  }, [mint]);

  // Garder le flux de trades actif pour ce token tant que la page est ouverte.
  useEffect(() => watchTrades(mint, () => {}), [mint]);

  const s = live ?? data?.summary;
  const solUsd = data?.solUsd ?? useLive.getState().status?.solUsd ?? null;
  const dex = data?.dex;
  const supply = data?.mint && !data.mint.error ? data.mint.supply : s?.fromStream ? 1_000_000_000 : null;
  const mcUsd = s?.marketCapUsd ?? dex?.marketCapUsd ?? null;
  const priceUsd = s?.priceUsd ?? dex?.priceUsd ?? null;
  const priceSol = s?.priceSol ?? dex?.priceNative ?? null;

  const runAi = async () => {
    setAiLoading(true);
    setAiErr("");
    try {
      setAi(await post<AiResult>(`/api/ai/token/${mint}`));
    } catch (e: any) {
      setAiErr(e.message);
    } finally {
      setAiLoading(false);
    }
  };

  const holdersReal = useMemo(() => data?.holders?.filter((h) => !h.isProgram) ?? [], [data]);

  if (!s && loading) return <div className="p-8 flex justify-center"><Spinner /></div>;
  if (!s && error) return <Empty>{error}</Empty>;
  if (!s) return null;

  const symbol = s.symbol || dex?.symbol || "";
  const name = s.name || dex?.name || "";

  return (
    <div className="px-3 sm:px-4 pt-3 space-y-3">
      <div className="flex items-start gap-3">
        <TokenAvatar src={s.image || dex?.imageUrl} symbol={symbol} size={52} />
        <div className="min-w-0 flex-1">
          <div className="flex items-center gap-2 flex-wrap">
            <h1 className="text-lg font-bold">${symbol || "…"}</h1>
            <span className="text-muted text-sm truncate">{name}</span>
            <Badge tone={s.migrated ? "up" : "brand"}>{s.migrated ? `Migré · ${s.pool}` : "Courbe Pump.fun"}</Badge>
            {data?.risk && (
              <Badge tone={data.risk.score >= 45 ? "down" : data.risk.score >= 20 ? "warn" : "up"}>
                Risque {data.risk.score}/100 · {data.risk.level}
              </Badge>
            )}
          </div>
          <div className="flex items-center gap-3 flex-wrap mt-1 text-[12px]">
            <Copy text={mint} />
            {s.twitter && <a className="text-muted hover:text-white" href={s.twitter} target="_blank" rel="noreferrer">X</a>}
            {s.telegram && <a className="text-muted hover:text-white" href={s.telegram} target="_blank" rel="noreferrer">Telegram</a>}
            {s.website && <a className="text-muted hover:text-white" href={s.website} target="_blank" rel="noreferrer">Site</a>}
            <a className="text-muted hover:text-white" href={`https://pump.fun/coin/${mint}`} target="_blank" rel="noreferrer">pump.fun</a>
            <a className="text-muted hover:text-white" href={`https://dexscreener.com/solana/${mint}`} target="_blank" rel="noreferrer">DexScreener</a>
            <a className="text-muted hover:text-white" href={solscanAccount(mint)} target="_blank" rel="noreferrer">Solscan</a>
            <a className="text-muted hover:text-white" href={`https://x.com/search?q=${encodeURIComponent(mint)}&f=live`} target="_blank" rel="noreferrer">Recherche X</a>
          </div>
        </div>
      </div>

      <div className="grid grid-cols-2 sm:grid-cols-4 lg:grid-cols-8 gap-2">
        <Stat label="Prix" value={price(priceUsd)} sub={priceSol ? price(priceSol, "") + " SOL" : undefined} />
        <Stat label="Capitalisation" value={usd(mcUsd)} sub={s.marketCapSol ? sol(s.marketCapSol, 1) : undefined} />
        <Stat
          label="Liquidité"
          value={s.liquiditySol != null ? sol(s.liquiditySol, 2) : usd(dex?.liquidityUsd ?? null)}
          sub={s.liquiditySol != null ? "SOL réels dans la courbe" : dex ? "pool DEX" : undefined}
        />
        <Stat label="Volume (suivi)" value={sol(s.volumeSol, 2)} sub={dex?.volume?.h24 != null ? `24 h : ${usd(dex.volume.h24)}` : undefined} />
        <Stat
          label="Achats / Ventes"
          value={
            <span>
              <span className="text-up">{s.buys}</span> / <span className="text-down">{s.sells}</span>
            </span>
          }
          sub={dex?.txns?.h24 ? `24 h : ${dex.txns.h24.buys} / ${dex.txns.h24.sells}` : undefined}
        />
        <Stat label="Acheteurs / Vendeurs" value={`${s.uniqueBuyers} / ${s.uniqueSellers}`} sub={`${s.holdersSeen} détenteurs vus`} />
        <Stat label="Courbe" value={s.migrated ? "100 % · migré" : pct(s.bondingPct, 1)} sub={!s.migrated ? <Bar value={s.bondingPct} /> : undefined} />
        <Stat label="Âge" value={s.createdAt ? age(Math.round((Date.now() - s.createdAt) / 1000)) : dex?.pairCreatedAt ? ago(dex.pairCreatedAt) : "—"} sub={`dernier trade ${ago(s.lastTradeAt)}`} />
      </div>

      <div className="grid lg:grid-cols-[1fr_340px] gap-3">
        <Panel bodyClass="p-0 h-[380px] sm:h-[460px]">
          <CandleChart mint={mint} supply={supply} />
        </Panel>
        <Panel title="Trader">
          <TradePanel mint={mint} symbol={symbol} priceSol={priceSol} solUsd={solUsd} />
        </Panel>
      </div>

      <div className="grid lg:grid-cols-[1fr_340px] gap-3">
        <div className="space-y-3 min-w-0">
          <Panel title="Transactions en direct" right={<span className="text-[11px] text-muted">flux PumpPortal</span>} bodyClass="p-0 max-h-[520px] overflow-y-auto">
            <TradesTable mint={mint} creator={s.creator} me={publicKey?.toBase58()} />
          </Panel>
          <Panel
            title="Analyse Grok"
            right={
              <Button size="sm" tone="brand" onClick={runAi} disabled={aiLoading || !aiEnabled}>
                {aiLoading ? "Analyse en cours (jusqu'à 1–2 min)…" : ai ? "Relancer" : "Analyser avec Grok"}
              </Button>
            }
          >
            {!aiEnabled && <div className="text-[13px] text-warn">Grok est désactivé : ajoutez XAI_API_KEY sur le serveur.</div>}
            {aiErr && <div className="text-[13px] text-down mb-2">{aiErr}</div>}
            {ai ? <AiView r={ai} /> : aiEnabled && !aiLoading && <Empty>Aucune analyse pour ce token. Chaque analyse consomme des crédits xAI.</Empty>}
            {aiLoading && <div className="flex items-center gap-2 text-sm text-muted"><Spinner /> Grok consulte X et les données on-chain…</div>}
          </Panel>
        </div>
        <div className="space-y-3">
          <Panel title="Sécurité et risques">
            {data?.risk ? (
              <ul className="space-y-1.5">
                {data.risk.flags.map((f, i) => (
                  <li key={i} className="flex gap-2 text-[13px]">
                    <span className={f.level === "high" ? "text-down" : f.level === "medium" ? "text-warn" : f.level === "ok" ? "text-up" : "text-muted"}>
                      {f.level === "ok" ? "✓" : "⚠"}
                    </span>
                    <span className="text-white/85">{f.label}</span>
                  </li>
                ))}
                {s.sniperPct != null && (
                  <li className="text-[12px] text-muted pt-1">
                    Dev : {pct(s.devPct)} · vendu {pct(s.devSoldPct ?? 0, 0)} · snipers {pct(s.sniperPct)} ({s.sniperCount})
                  </li>
                )}
              </ul>
            ) : (
              <Spinner />
            )}
            <p className="text-[11px] text-muted mt-2">Analyse automatique basée sur des données vérifiables. Un score faible ne garantit pas la sécurité.</p>
          </Panel>
          <Panel title="Top détenteurs (on-chain)" bodyClass="p-0">
            {data?.holders ? (
              <div className="divide-y divide-line/60">
                {data.holders.slice(0, 12).map((h, i) => (
                  <div key={h.account} className="flex items-center gap-2 px-3 py-1.5 text-[12px] font-mono">
                    <span className="text-muted w-5">{i + 1}</span>
                    <a href={solscanAccount(h.owner ?? h.account)} target="_blank" rel="noreferrer" className="text-white/80 hover:text-white">
                      {short(h.owner ?? h.account, 4)}
                    </a>
                    {h.isProgram && <Badge>courbe/pool</Badge>}
                    {h.owner === s.creator && <Badge tone="warn">DEV</Badge>}
                    <span className="ml-auto">{pct(h.pct, 2)}</span>
                  </div>
                ))}
                <div className="px-3 py-2 text-[11px] text-muted">
                  Top 10 hors courbe/pool : {pct(holdersReal.slice(0, 10).reduce((a, h) => a + h.pct, 0), 1)}
                </div>
              </div>
            ) : (
              <Empty>Détenteurs indisponibles (RPC).</Empty>
            )}
          </Panel>
          <Panel title="Alerte sur ce token">
            <AlertCreator mint={mint} symbol={symbol} />
          </Panel>
          {data?.mint && !data.mint.error && (
            <Panel title="Mint">
              <div className="text-[12px] space-y-1 font-mono">
                <div className="flex justify-between"><span className="text-muted">Programme</span><span>{data.mint.program.startsWith("Tokenz") ? "Token-2022" : "SPL Token"}</span></div>
                <div className="flex justify-between"><span className="text-muted">Offre</span><span>{num(data.mint.supply, 0)}</span></div>
                <div className="flex justify-between"><span className="text-muted">Décimales</span><span>{data.mint.decimals}</span></div>
                <div className="flex justify-between"><span className="text-muted">Créateur</span><span>{short(s.creator)}</span></div>
              </div>
            </Panel>
          )}
        </div>
      </div>
    </div>
  );
}
