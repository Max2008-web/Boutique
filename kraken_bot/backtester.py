"""
backtester.py
=============
Backtest portefeuille multi-paires de la stratégie, avec :

* les mêmes indicateurs / signaux (`strategy.py`) et le même dimensionnement /
  coupe-circuit (`risk_engine.py`) que le bot live ;
* les paliers réels de frais Kraken (maker/taker selon le volume 30 jours glissant) ;
* entrées en limite post-only (maker) à la clôture du signal, exécutées seulement si
  la bougie suivante traverse strictement le prix, sinon repli taker (+ glissement) ;
* stops exécutés en taker avec glissement, gaps d'ouverture pris en compte ;
* aucune anticipation : signal calculé sur bougie clôturée, exécution à la suivante.

Sources de données :
  --source ohlc       API OHLC Kraken (limitée par Kraken aux 720 dernières bougies)
  --source trades     reconstruit les bougies depuis l'historique complet des trades
                      (illimité mais lent ; résultat mis en cache dans data/)
  --source csv        fichiers CSV (ex. archives OHLCVT téléchargeables chez Kraken)
  --source synthetic  marché simulé, uniquement pour tester la chaîne logicielle

Exemples :
  python backtester.py --source ohlc --timeframe 1h
  python backtester.py --source trades --days 120 --timeframe 15m --symbols BTC/USD,ETH/USD
  python backtester.py --source csv --csv BTC/USD=data/XBTUSD_60.csv --timeframe 1h
"""
from __future__ import annotations

import argparse
import logging
import math
import time
from dataclasses import dataclass
from typing import Dict, List

import ccxt
import numpy as np
import pandas as pd

from kraken_config import BASE_DIR, BotConfig, fee_for_volume, load_config
from kraken_feed import TIMEFRAME_MS, call_with_retries, ohlcv_to_df
from risk_engine import Position, RiskEngine, update_trailing_stop
from strategy import StrategyParams, generate_signals

log = logging.getLogger("backtest")
DATA_DIR = BASE_DIR / "data"


# ---------------------------------------------------------------------------
# Chargement des données
# ---------------------------------------------------------------------------
def load_csv(path: str) -> pd.DataFrame:
    """CSV avec en-tête (timestamp,open,high,low,close,volume) ou archive Kraken OHLCVT
    sans en-tête (epoch s, open, high, low, close, volume, trades)."""
    head = pd.read_csv(path, nrows=1, header=None)
    has_header = not str(head.iloc[0, 0]).replace(".", "").isdigit()
    if has_header:
        df = pd.read_csv(path)
        df.columns = [c.lower() for c in df.columns]
        ts = df["timestamp"]
    else:
        df = pd.read_csv(path, header=None)
        df = df.iloc[:, :6]
        df.columns = ["timestamp", "open", "high", "low", "close", "volume"]
        ts = df["timestamp"]
    if np.issubdtype(ts.dtype, np.number):
        unit = "ms" if ts.iloc[0] > 1e11 else "s"
        df["timestamp"] = pd.to_datetime(ts, unit=unit, utc=True)
    else:
        df["timestamp"] = pd.to_datetime(ts, utc=True)
    df = df.set_index("timestamp")[["open", "high", "low", "close", "volume"]].astype(float)
    return df[~df.index.duplicated(keep="last")].sort_index()


def fetch_ohlc_api(exchange: ccxt.kraken, symbol: str, timeframe: str) -> pd.DataFrame:
    raw = call_with_retries(exchange.fetch_ohlcv, symbol, timeframe, None, 720, label="ohlcv")
    tf_ms = TIMEFRAME_MS[timeframe]
    now_ms = exchange.milliseconds()
    return ohlcv_to_df([r for r in raw if r[0] + tf_ms <= now_ms])


def fetch_trades_ohlcv(exchange: ccxt.kraken, symbol: str, timeframe: str, days: int) -> pd.DataFrame:
    """Reconstruit les bougies depuis l'endpoint public Trades (1000 trades / appel)."""
    DATA_DIR.mkdir(exist_ok=True)
    cache = DATA_DIR / f"{symbol.replace('/', '')}_{timeframe}_{days}d.csv"
    if cache.exists() and time.time() - cache.stat().st_mtime < 12 * 3600:
        log.info("Cache utilisé : %s", cache)
        return load_csv(str(cache))
    tf_ms = TIMEFRAME_MS[timeframe]
    until = exchange.milliseconds()
    since = until - days * 86_400_000
    candles: Dict[int, list] = {}
    seen_boundary: set = set()
    calls = 0
    while since < until:
        trades = call_with_retries(exchange.fetch_trades, symbol, since, 1000, label="trades")
        trades = [t for t in trades if t["id"] not in seen_boundary]
        if not trades:
            break
        for t in trades:
            ts = (t["timestamp"] // tf_ms) * tf_ms
            p, a = float(t["price"]), float(t["amount"])
            c = candles.get(ts)
            if c is None:
                candles[ts] = [ts, p, p, p, p, a]
            else:
                c[2] = max(c[2], p)
                c[3] = min(c[3], p)
                c[4] = p
                c[5] += a
        last_ts = trades[-1]["timestamp"]
        seen_boundary = {t["id"] for t in trades if t["timestamp"] == last_ts}
        since = last_ts
        calls += 1
        if calls % 50 == 0:
            log.info("%s : %d appels, données jusqu'au %s", symbol, calls,
                     pd.Timestamp(last_ts, unit="ms", tz="UTC"))
        if len(trades) < 2 and since >= until - tf_ms:
            break
    rows = [candles[k] for k in sorted(candles) if k + tf_ms <= until]
    df = ohlcv_to_df(rows)
    # bougies sans aucun trade : on reporte la dernière clôture, volume nul
    full = pd.date_range(df.index[0], df.index[-1], freq=pd.Timedelta(milliseconds=tf_ms))
    df = df.reindex(full)
    df["close"] = df["close"].ffill()
    for col in ("open", "high", "low"):
        df[col] = df[col].fillna(df["close"])
    df["volume"] = df["volume"].fillna(0.0)
    df.to_csv(cache, index_label="timestamp")
    return df


def synthetic_ohlcv(timeframe: str, bars: int, seed: int, start_price: float) -> pd.DataFrame:
    """Marché synthétique à régimes de volatilité (test de la chaîne, pas de la rentabilité)."""
    rng = np.random.default_rng(seed)
    tf_min = TIMEFRAME_MS[timeframe] / 60_000
    base_vol = 0.006 * math.sqrt(tf_min / 15)
    regime = np.repeat(rng.choice([0.6, 1.0, 2.5], size=bars // 50 + 1, p=[0.4, 0.45, 0.15]), 50)[:bars]
    drift = np.repeat(rng.normal(0, 0.0008, size=bars // 80 + 1), 80)[:bars]
    rets = drift + rng.standard_normal(bars) * base_vol * regime
    close = start_price * np.exp(np.cumsum(rets))
    open_ = np.concatenate([[start_price], close[:-1]])
    spread = np.abs(rng.standard_normal(bars)) * base_vol * regime * close
    high = np.maximum(open_, close) + spread * 0.6
    low = np.minimum(open_, close) - spread * 0.6
    volume = rng.lognormal(3, 0.4, bars) * regime ** 1.5
    idx = pd.date_range("2025-01-01", periods=bars, freq=pd.Timedelta(minutes=tf_min), tz="UTC")
    return pd.DataFrame({"open": open_, "high": high, "low": low, "close": close, "volume": volume}, index=idx)


# ---------------------------------------------------------------------------
# Moteur de backtest
# ---------------------------------------------------------------------------
@dataclass
class PendingOrder:
    limit: float
    max_price: float
    atr: float
    risk_scale: float
    is_add: bool


class Backtester:
    def __init__(self, cfg: BotConfig, data: Dict[str, pd.DataFrame], halt_pause_days: float = 0.0):
        self.cfg = cfg
        self.params = StrategyParams.from_config(cfg)
        self.data = {s: generate_signals(df, self.params) for s, df in data.items()}
        self.risk = RiskEngine(cfg, state_path=None)
        self.risk.initial_capital = cfg.sim_starting_balance
        self.cash = cfg.sim_starting_balance
        self.slip = cfg.slippage_bps / 10_000
        self.halt_pause_days = halt_pause_days
        self.pending: Dict[str, PendingOrder] = {}
        self.equity_curve: List[tuple] = []
        self.total_fees = 0.0
        self.maker_volume = 0.0
        self.taker_volume = 0.0
        self.halts: List[str] = []
        self.bars_in_market = 0
        self._last_close: Dict[str, float] = {}

    def fees(self, ts: float):
        return fee_for_volume(self.risk.rolling_volume(ts))

    # --- opérations ----------------------------------------------------------
    def _buy(self, symbol: str, ts: float, price: float, maker: bool, order: PendingOrder) -> None:
        maker_fee, taker_fee = self.fees(ts)
        fee_rate = maker_fee if maker else taker_fee
        pos = self.risk.positions.get(symbol)
        stop = pos.stop_price if order.is_add else price - self.cfg.initial_stop_atr * order.atr
        equity = self.mark_to_market(ts)
        amount, _ = self.risk.size_position(equity, self.cash, price, stop,
                                            taker_fee if self.cfg.taker_fallback else maker_fee,
                                            risk_scale=order.risk_scale)
        if amount * price < 10.0:   # ~ minimum de coût Kraken
            return
        cost = amount * price
        fee = cost * fee_rate
        self.cash -= cost + fee
        self.total_fees += fee
        if maker:
            self.maker_volume += cost
        else:
            self.taker_volume += cost
        self.risk.register_fill_volume(ts, cost)
        if order.is_add and pos:
            pos.add_fill(amount, price, fee)
            pos.adds += 1
            pos.last_add_price = price
            pos.risk_amount += amount * max(0.0, price - pos.stop_price) + fee
        else:
            risk_amount = amount * (price - stop) + fee + amount * stop * taker_fee
            self.risk.positions[symbol] = Position(
                symbol=symbol, amount=amount, entry_price=price, stop_price=stop, initial_stop=stop,
                highest_price=price, entry_time=ts, entry_atr=order.atr, risk_amount=risk_amount,
                cost=cost, fees_paid=fee, last_add_price=price, last_atr=order.atr)

    def _sell(self, symbol: str, ts: float, price: float, reason: str) -> None:
        pos = self.risk.positions[symbol]
        _, taker_fee = self.fees(ts)
        proceeds = pos.amount * price
        fee = proceeds * taker_fee
        self.cash += proceeds - fee
        self.total_fees += fee
        self.taker_volume += proceeds
        self.risk.register_fill_volume(ts, proceeds)
        self.risk.close_position(symbol, price, pos.amount, fee, ts, reason)

    def mark_to_market(self, ts: float) -> float:
        """Équité = cash + positions valorisées à la dernière clôture connue."""
        eq = self.cash
        for s, pos in self.risk.positions.items():
            eq += pos.amount * self._last_close.get(s, pos.entry_price)
        return eq

    # --- boucle --------------------------------------------------------------
    def run(self) -> pd.DataFrame:
        index = sorted(set().union(*[df.index for df in self.data.values()]))
        resume_at = None
        for t in index:
            ts = t.timestamp()
            if self.risk.halted and resume_at is not None and ts >= resume_at:
                self.risk.reset_halt()
                resume_at = None
            for symbol, df in self.data.items():
                if t not in df.index:
                    continue
                bar = df.loc[t]
                if pd.isna(bar["close"]):
                    continue
                self._process_bar(symbol, ts, bar)
                self._last_close[symbol] = float(bar["close"])
            if self.risk.positions:
                self.bars_in_market += 1
            equity = self.mark_to_market(ts)
            self.equity_curve.append((t, equity))
            if self.risk.record_equity(ts, equity):
                self.halts.append(self.risk.halt_reason)
                for symbol in list(self.risk.positions):
                    self._sell(symbol, ts, self._last_close[symbol] * (1 - self.slip), "hard_stop")
                self.pending.clear()
                if self.halt_pause_days > 0:
                    resume_at = ts + self.halt_pause_days * 86_400
                else:
                    log.warning("Hard stop atteint : fin du trading dans le backtest (comme en live)")
        return pd.DataFrame(self.equity_curve, columns=["timestamp", "equity"]).set_index("timestamp")

    def _process_bar(self, symbol: str, ts: float, bar: pd.Series) -> None:
        o, h, l, c = (float(bar[k]) for k in ("open", "high", "low", "close"))
        atr_now = float(bar["atr"]) if pd.notna(bar["atr"]) else 0.0

        # 1) ordre en attente placé à la clôture précédente
        order = self.pending.pop(symbol, None)
        if order and not self.risk.halted:
            filled = False
            if self.cfg.post_only and l < order.limit:
                self._buy(symbol, ts, order.limit, maker=True, order=order)
                filled = True
            elif self.cfg.taker_fallback or not self.cfg.post_only:
                price = o * (1 + self.slip)
                if price <= order.max_price:
                    self._buy(symbol, ts, price, maker=False, order=order)
                    filled = True
            pos = self.risk.positions.get(symbol)
            if filled and pos and not order.is_add and l <= pos.stop_price:
                # cas défavorable : la même bougie touche le stop initial après l'entrée
                self._sell(symbol, ts, pos.stop_price * (1 - self.slip), "initial_stop")
                return

        # 2) gestion de la position
        pos = self.risk.positions.get(symbol)
        if pos:
            if o <= pos.stop_price:
                self._sell(symbol, ts, o * (1 - self.slip), "gap_stop")
                return
            if l <= pos.stop_price:
                self._sell(symbol, ts, pos.stop_price * (1 - self.slip), "trailing_stop")
                return
            update_trailing_stop(pos, h, atr_now, self.cfg.trailing_stop_atr)
            if c <= pos.stop_price:
                self._sell(symbol, ts, c * (1 - self.slip), "trailing_stop")
                return
            if self.cfg.exit_on_mid_band and bool(bar["below_mid"]):
                self._sell(symbol, ts, c * (1 - self.slip), "mid_band")
                return
            if not self.risk.halted and atr_now > 0 and self.risk.pyramid_allowed(pos, c, atr_now):
                self.pending[symbol] = PendingOrder(limit=c, max_price=c + self.cfg.entry_chase_atr * atr_now,
                                                    atr=atr_now, risk_scale=self.cfg.pyramid_risk_scale,
                                                    is_add=True)
            return

        # 3) nouveau signal -> ordre limite post-only pour la bougie suivante
        if (not self.risk.halted and bool(bar["long_entry"]) and atr_now > 0
                and len(self.risk.positions) + sum(not p.is_add for p in self.pending.values())
                < self.cfg.max_open_positions):
            self.pending[symbol] = PendingOrder(limit=c, max_price=c + self.cfg.entry_chase_atr * atr_now,
                                                atr=atr_now, risk_scale=1.0, is_add=False)

    # --- rapport -------------------------------------------------------------
    def report(self, curve: pd.DataFrame) -> dict:
        eq = curve["equity"]
        start, end = self.cfg.sim_starting_balance, float(eq.iloc[-1])
        days = max((curve.index[-1] - curve.index[0]).total_seconds() / 86_400, 1e-9)
        rets = eq.pct_change().dropna()
        periods_per_year = 365 * 86_400_000 / TIMEFRAME_MS[self.cfg.timeframe]
        sharpe = rets.mean() / rets.std() * math.sqrt(periods_per_year) if rets.std() > 0 else 0.0
        downside = rets[rets < 0].std()
        sortino = rets.mean() / downside * math.sqrt(periods_per_year) if downside and downside > 0 else 0.0
        dd = 1 - eq / eq.cummax()
        trades = self.risk.trades
        wins = [t for t in trades if t.pnl > 0]
        losses = [t for t in trades if t.pnl <= 0]
        gross_win = sum(t.pnl for t in wins)
        gross_loss = -sum(t.pnl for t in losses)
        streak = worst = 0
        for t in trades:
            streak = streak + 1 if t.pnl <= 0 else 0
            worst = max(worst, streak)
        bh = np.mean([df["close"].iloc[-1] / df["close"].iloc[self.params.warmup] - 1
                      for df in self.data.values()])
        total_volume = self.maker_volume + self.taker_volume
        return {
            "Période": f"{curve.index[0]:%Y-%m-%d} -> {curve.index[-1]:%Y-%m-%d} ({days:.0f} j)",
            "Capital initial": f"{start:,.2f}",
            "Capital final": f"{end:,.2f}",
            "Rendement total": f"{end / start - 1:+.2%}",
            "Rendement annualisé (CAGR)": f"{(end / start) ** (365 / days) - 1:+.2%}" if end > 0 else "n/a",
            "Buy & hold (moyenne paires)": f"{bh:+.2%}",
            "Drawdown max": f"{dd.max():.2%}",
            "Sharpe (annualisé)": f"{sharpe:.2f}",
            "Sortino (annualisé)": f"{sortino:.2f}",
            "Nombre de trades": len(trades),
            "Taux de réussite": f"{len(wins) / len(trades):.1%}" if trades else "n/a",
            "Gain moyen / perte moyenne": (f"{gross_win / len(wins):,.2f} / {-gross_loss / len(losses):,.2f}"
                                           if wins and losses else "n/a"),
            "Profit factor": f"{gross_win / gross_loss:.2f}" if gross_loss > 0 else "inf",
            "Espérance (R moyen)": f"{np.mean([t.r_multiple for t in trades]):+.2f}R" if trades else "n/a",
            "Meilleur trade (R)": f"{max(t.r_multiple for t in trades):+.2f}R" if trades else "n/a",
            "Pertes consécutives max": worst,
            "Frais payés": f"{self.total_fees:,.2f} ({self.total_fees / start:.2%} du capital)",
            "Part du volume en maker": f"{self.maker_volume / total_volume:.1%}" if total_volume else "n/a",
            "Temps investi": f"{self.bars_in_market / max(len(curve), 1):.1%}",
            "Hard stops déclenchés": len(self.halts),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(description="Backtest Kraken momentum / volatility breakout")
    parser.add_argument("--source", choices=["ohlc", "trades", "csv", "synthetic"], default="ohlc")
    parser.add_argument("--symbols", help="ex. BTC/USD,ETH/USD (défaut : SYMBOLS du .env)")
    parser.add_argument("--timeframe", help="15m, 1h... (défaut : TIMEFRAME du .env)")
    parser.add_argument("--days", type=int, default=90, help="profondeur pour --source trades")
    parser.add_argument("--csv", action="append", default=[], help="SYMBOLE=chemin.csv (répétable)")
    parser.add_argument("--capital", type=float, help="capital de départ")
    parser.add_argument("--risk", type=float, help="risque par trade, ex. 0.02")
    parser.add_argument("--sizing", choices=["fixed", "kelly"])
    parser.add_argument("--no-compound", action="store_true")
    parser.add_argument("--pyramid", type=int, help="nombre max d'ajouts (pyramiding)")
    parser.add_argument("--fee-volume", type=float, help="volume 30j initial (palier de frais)")
    parser.add_argument("--halt-pause-days", type=float, default=0.0,
                        help="reprise automatique après un hard stop (0 = arrêt définitif, comme en live)")
    parser.add_argument("--save-trades", help="export CSV des trades")
    parser.add_argument("--save-equity", help="export CSV de la courbe d'équité")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO, format="%(asctime)s | %(levelname)-7s | %(message)s")
    logging.getLogger("risk").setLevel(logging.INFO if args.verbose else logging.WARNING)

    cfg = load_config()
    cfg.dry_run = True
    if args.symbols:
        cfg.symbols = [s.strip() for s in args.symbols.split(",")]
    if args.timeframe:
        cfg.timeframe = args.timeframe
    if args.capital:
        cfg.sim_starting_balance = args.capital
    if args.risk:
        cfg.risk_per_trade = args.risk
    if args.sizing:
        cfg.sizing_mode = args.sizing
    if args.no_compound:
        cfg.compound = False
    if args.pyramid is not None:
        cfg.pyramid_max_adds = args.pyramid
    if args.fee_volume is not None:
        cfg.fee_volume_30d = args.fee_volume
    cfg.validate()

    data: Dict[str, pd.DataFrame] = {}
    if args.source == "synthetic":
        for i, s in enumerate(cfg.symbols):
            data[s] = synthetic_ohlcv(cfg.timeframe, 6000, seed=42 + i, start_price=100.0 * (i + 1))
        log.warning("Données SYNTHÉTIQUES : ce test valide le code, pas la rentabilité.")
    elif args.source == "csv":
        mapping = dict(item.split("=", 1) for item in args.csv)
        if not mapping:
            parser.error("--source csv exige au moins un --csv SYMBOLE=chemin.csv")
        data = {s: load_csv(p) for s, p in mapping.items()}
    else:
        exchange = ccxt.kraken({"enableRateLimit": True, "timeout": 20_000})
        exchange.load_markets()
        for s in cfg.symbols:
            if args.source == "ohlc":
                data[s] = fetch_ohlc_api(exchange, s, cfg.timeframe)
                log.info("%s : %d bougies (l'API OHLC Kraken est limitée à 720)", s, len(data[s]))
            else:
                log.info("%s : reconstruction des bougies depuis les trades sur %d jours...", s, args.days)
                data[s] = fetch_trades_ohlcv(exchange, s, cfg.timeframe, args.days)

    params = StrategyParams.from_config(cfg)
    for s, df in data.items():
        if len(df) < params.warmup + 50:
            raise SystemExit(f"{s} : seulement {len(df)} bougies, insuffisant pour un backtest")

    maker, taker = fee_for_volume(cfg.fee_volume_30d)
    log.info("Backtest %s | %s | capital %.0f | risque %.1f%% (%s) | compound=%s | frais initiaux %.2f%%/%.2f%%",
             ",".join(data), cfg.timeframe, cfg.sim_starting_balance, cfg.risk_per_trade * 100,
             cfg.sizing_mode, cfg.compound, maker * 100, taker * 100)
    bt = Backtester(cfg, data, halt_pause_days=args.halt_pause_days)
    curve = bt.run()
    print("\n" + "=" * 64 + "\n RÉSULTATS DU BACKTEST\n" + "=" * 64)
    for k, v in bt.report(curve).items():
        print(f" {k:<32} {v}")
    print("=" * 64)
    for reason in bt.halts:
        print(" HARD STOP :", reason)

    if args.save_trades:
        pd.DataFrame([t.__dict__ for t in bt.risk.trades]).to_csv(args.save_trades, index=False)
        log.info("Trades exportés vers %s", args.save_trades)
    if args.save_equity:
        curve.to_csv(args.save_equity)
        log.info("Courbe d'équité exportée vers %s", args.save_equity)


if __name__ == "__main__":
    main()
