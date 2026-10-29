import { useEffect, useRef, useState } from "react";
import { createChart, IChartApi, ISeriesApi, UTCTimestamp, CrosshairMode } from "lightweight-charts";
import { api } from "../lib/api";
import { watchTrades } from "../lib/live";
import { price as fmtPrice, usd } from "../lib/format";
import type { Candle } from "../lib/types";

const TFS = [
  { s: 1, l: "1s" },
  { s: 15, l: "15s" },
  { s: 60, l: "1m" },
  { s: 300, l: "5m" },
  { s: 900, l: "15m" },
  { s: 3600, l: "1h" },
];

export function CandleChart({ mint, supply }: { mint: string; supply: number | null }) {
  const box = useRef<HTMLDivElement>(null);
  const chart = useRef<IChartApi | null>(null);
  const candles = useRef<ISeriesApi<"Candlestick"> | null>(null);
  const volume = useRef<ISeriesApi<"Histogram"> | null>(null);
  const last = useRef<Candle | null>(null);
  const [tf, setTf] = useState(60);
  const [mode, setMode] = useState<"mcap" | "price">("mcap");
  const [info, setInfo] = useState("");
  const mult = mode === "mcap" && supply ? supply : 1;

  useEffect(() => {
    if (!box.current) return;
    const c = createChart(box.current, {
      layout: { background: { color: "transparent" }, textColor: "#7d8590", fontFamily: "JetBrains Mono, monospace", fontSize: 11 },
      grid: { vertLines: { color: "rgba(255,255,255,0.03)" }, horzLines: { color: "rgba(255,255,255,0.03)" } },
      rightPriceScale: { borderColor: "#20252e" },
      timeScale: { borderColor: "#20252e", timeVisible: true, secondsVisible: true },
      crosshair: { mode: CrosshairMode.Normal },
      autoSize: true,
    });
    candles.current = c.addCandlestickSeries({
      upColor: "#16c784",
      downColor: "#ea3943",
      borderVisible: false,
      wickUpColor: "#16c784",
      wickDownColor: "#ea3943",
      priceFormat: { type: "custom", minMove: 1e-12, formatter: (v: number) => (v >= 1000 ? usd(v) : fmtPrice(v)) },
    });
    volume.current = c.addHistogramSeries({ priceScaleId: "vol", priceFormat: { type: "volume" } });
    c.priceScale("vol").applyOptions({ scaleMargins: { top: 0.82, bottom: 0 } });
    chart.current = c;
    return () => {
      c.remove();
      chart.current = null;
    };
  }, []);

  useEffect(() => {
    let alive = true;
    setInfo("Chargement…");
    api<{ candles: Candle[]; sources: { live: number; geckoterminal: number }; historyError: string }>(
      `/api/tokens/${mint}/candles?tf=${tf}`,
    )
      .then((r) => {
        if (!alive || !candles.current || !volume.current) return;
        const cs = r.candles.map((k) => ({ ...k, open: k.open * mult, high: k.high * mult, low: k.low * mult, close: k.close * mult }));
        candles.current.setData(cs.map((k) => ({ time: k.time as UTCTimestamp, open: k.open, high: k.high, low: k.low, close: k.close })));
        volume.current.setData(
          cs.map((k) => ({ time: k.time as UTCTimestamp, value: k.volume, color: k.close >= k.open ? "rgba(22,199,132,0.35)" : "rgba(234,57,67,0.35)" })),
        );
        last.current = cs[cs.length - 1] ?? null;
        chart.current?.timeScale().fitContent();
        setInfo(
          cs.length
            ? `${r.sources.live} bougies du flux en direct${r.sources.geckoterminal ? ` + ${r.sources.geckoterminal} historiques (GeckoTerminal)` : ""}`
            : "Aucun trade observé pour l'instant — le graphique se construit en direct à chaque transaction.",
        );
      })
      .catch((e) => alive && setInfo(e.message));

    const off = watchTrades(mint, (p) => {
      if (!candles.current || !volume.current || p.priceUsd == null) return;
      const v = p.priceUsd * mult;
      const t = Math.floor(p.t / 1000 / tf) * tf;
      const volUsd = p.priceUsd && p.priceSol ? (p.sol * p.priceUsd) / p.priceSol : 0;
      let k = last.current;
      if (!k || t > k.time) {
        const open = k ? k.close : v;
        k = { time: t, open, high: Math.max(open, v), low: Math.min(open, v), close: v, volume: volUsd };
      } else if (t === k.time) {
        k = { ...k, high: Math.max(k.high, v), low: Math.min(k.low, v), close: v, volume: k.volume + volUsd };
      } else return;
      last.current = k;
      candles.current.update({ time: k.time as UTCTimestamp, open: k.open, high: k.high, low: k.low, close: k.close });
      volume.current.update({ time: k.time as UTCTimestamp, value: k.volume, color: k.close >= k.open ? "rgba(22,199,132,0.35)" : "rgba(234,57,67,0.35)" });
    });
    return () => {
      alive = false;
      off();
    };
  }, [mint, tf, mult]);

  return (
    <div className="flex flex-col h-full">
      <div className="flex items-center gap-1 px-2 py-1.5 border-b border-line overflow-x-auto">
        {TFS.map((x) => (
          <button
            key={x.s}
            onClick={() => setTf(x.s)}
            className={`px-2 py-0.5 rounded text-[12px] font-mono ${tf === x.s ? "bg-white/10 text-white" : "text-muted hover:text-white"}`}
          >
            {x.l}
          </button>
        ))}
        <span className="w-px h-4 bg-line mx-1" />
        {supply ? (
          <>
            <button onClick={() => setMode("mcap")} className={`px-2 py-0.5 rounded text-[12px] ${mode === "mcap" ? "bg-white/10 text-white" : "text-muted"}`}>
              MC
            </button>
            <button onClick={() => setMode("price")} className={`px-2 py-0.5 rounded text-[12px] ${mode === "price" ? "bg-white/10 text-white" : "text-muted"}`}>
              Prix
            </button>
          </>
        ) : null}
        <span className="ml-auto text-[10px] text-muted truncate hidden sm:block">{info}</span>
      </div>
      <div ref={box} className="flex-1 min-h-[280px]" />
      <div className="sm:hidden text-[10px] text-muted px-2 py-1">{info}</div>
    </div>
  );
}
