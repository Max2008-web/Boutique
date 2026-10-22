"""
strategy.py
===========
Stratégie « Aggressive Momentum & Volatility Breakout », entièrement vectorisée
(pandas / numpy).

Signal d'achat (sur bougie clôturée) :
  1. Clôture au-dessus de la bande de Bollinger haute (20, 2σ)
     [option : la bougie précédente clôturait encore sous la bande -> cassure fraîche]
  2. Volume > `volume_multiplier` x moyenne mobile du volume des N bougies précédentes
  3. ADX > 25 (tendance forte) et +DI > -DI (pression acheteuse dominante)
  4. [option] largeur des bandes en expansion (la volatilité « explose »)

Sortie : trailing stop « chandelier » = plus haut depuis l'entrée - k x ATR
(géré par risk_engine / bot / backtester), et optionnellement clôture sous la
moyenne de Bollinger.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np
import pandas as pd

from kraken_config import BotConfig


@dataclass
class StrategyParams:
    bb_period: int = 20
    bb_std: float = 2.0
    volume_sma_period: int = 20
    volume_multiplier: float = 1.8
    adx_period: int = 14
    adx_threshold: float = 25.0
    atr_period: int = 14
    require_fresh_breakout: bool = True
    require_di_confirmation: bool = True
    require_bandwidth_expansion: bool = True

    @classmethod
    def from_config(cls, cfg: BotConfig) -> "StrategyParams":
        return cls(
            bb_period=cfg.bb_period, bb_std=cfg.bb_std,
            volume_sma_period=cfg.volume_sma_period, volume_multiplier=cfg.volume_multiplier,
            adx_period=cfg.adx_period, adx_threshold=cfg.adx_threshold, atr_period=cfg.atr_period,
            require_fresh_breakout=cfg.require_fresh_breakout,
            require_di_confirmation=cfg.require_di_confirmation,
            require_bandwidth_expansion=cfg.require_bandwidth_expansion,
        )

    @property
    def warmup(self) -> int:
        """Nombre de bougies nécessaires avant que les indicateurs soient fiables."""
        return max(self.bb_period, self.volume_sma_period + 1, 3 * self.adx_period,
                   3 * self.atr_period) + 2


# ---------------------------------------------------------------------------
# Indicateurs
# ---------------------------------------------------------------------------
def wilder(series: pd.Series, period: int) -> pd.Series:
    """Moyenne lissée de Wilder (RMA), utilisée par l'ATR et l'ADX."""
    return series.ewm(alpha=1.0 / period, adjust=False, min_periods=period).mean()


def true_range(df: pd.DataFrame) -> pd.Series:
    prev_close = df["close"].shift(1)
    ranges = pd.concat([
        df["high"] - df["low"],
        (df["high"] - prev_close).abs(),
        (df["low"] - prev_close).abs(),
    ], axis=1)
    return ranges.max(axis=1)


def bollinger(close: pd.Series, period: int, n_std: float):
    mid = close.rolling(period, min_periods=period).mean()
    std = close.rolling(period, min_periods=period).std(ddof=0)
    upper = mid + n_std * std
    lower = mid - n_std * std
    width = (upper - lower) / mid
    return mid, upper, lower, width


def atr(df: pd.DataFrame, period: int) -> pd.Series:
    return wilder(true_range(df), period)


def adx(df: pd.DataFrame, period: int):
    up_move = df["high"].diff()
    down_move = -df["low"].diff()
    plus_dm = pd.Series(np.where((up_move > down_move) & (up_move > 0), up_move, 0.0), index=df.index)
    minus_dm = pd.Series(np.where((down_move > up_move) & (down_move > 0), down_move, 0.0), index=df.index)
    atr_ = wilder(true_range(df), period)
    plus_di = 100.0 * wilder(plus_dm, period) / atr_
    minus_di = 100.0 * wilder(minus_dm, period) / atr_
    di_sum = (plus_di + minus_di).replace(0.0, np.nan)
    dx = 100.0 * (plus_di - minus_di).abs() / di_sum
    return wilder(dx, period), plus_di, minus_di


def compute_indicators(df: pd.DataFrame, p: StrategyParams) -> pd.DataFrame:
    out = df.copy()
    out["bb_mid"], out["bb_upper"], out["bb_lower"], out["bb_width"] = bollinger(
        out["close"], p.bb_period, p.bb_std)
    out["atr"] = atr(out, p.atr_period)
    out["adx"], out["plus_di"], out["minus_di"] = adx(out, p.adx_period)
    # moyenne des N bougies *précédentes* : la bougie testée n'entre pas dans sa référence
    out["vol_sma"] = out["volume"].rolling(p.volume_sma_period,
                                           min_periods=p.volume_sma_period).mean().shift(1)
    out["vol_ratio"] = out["volume"] / out["vol_sma"]
    return out


def generate_signals(df: pd.DataFrame, p: StrategyParams) -> pd.DataFrame:
    out = compute_indicators(df, p)
    breakout = out["close"] > out["bb_upper"]
    if p.require_fresh_breakout:
        breakout &= out["close"].shift(1) <= out["bb_upper"].shift(1)
    volume_ok = out["vol_ratio"] > p.volume_multiplier
    trend_ok = out["adx"] > p.adx_threshold
    if p.require_di_confirmation:
        trend_ok &= out["plus_di"] > out["minus_di"]
    signal = breakout & volume_ok & trend_ok
    if p.require_bandwidth_expansion:
        signal &= out["bb_width"] > out["bb_width"].shift(1)
    warm = pd.Series(np.arange(len(out)) >= p.warmup, index=out.index)
    out["long_entry"] = (signal & warm & out["atr"].notna()).astype(bool)
    out["below_mid"] = (out["close"] < out["bb_mid"]).astype(bool)
    return out


# ---------------------------------------------------------------------------
# Façade utilisée par le bot
# ---------------------------------------------------------------------------
@dataclass
class Signal:
    timestamp: pd.Timestamp
    close: float
    high: float
    atr: float
    adx: float
    vol_ratio: float
    bb_upper: float
    bb_mid: float
    long_entry: bool
    below_mid: bool


class MomentumBreakoutStrategy:
    def __init__(self, params: StrategyParams):
        self.params = params

    def prepare(self, df: pd.DataFrame) -> pd.DataFrame:
        return generate_signals(df, self.params)

    def latest_signal(self, df: pd.DataFrame) -> Optional[Signal]:
        """Évalue la dernière bougie clôturée. `df` ne doit contenir que des bougies clôturées."""
        if len(df) < self.params.warmup + 1:
            return None
        sig = self.prepare(df).iloc[-1]
        if pd.isna(sig["atr"]) or sig["atr"] <= 0:
            return None
        return Signal(
            timestamp=sig.name, close=float(sig["close"]), high=float(sig["high"]),
            atr=float(sig["atr"]), adx=float(sig["adx"]),
            vol_ratio=float(sig["vol_ratio"]) if pd.notna(sig["vol_ratio"]) else 0.0,
            bb_upper=float(sig["bb_upper"]), bb_mid=float(sig["bb_mid"]),
            long_entry=bool(sig["long_entry"]), below_mid=bool(sig["below_mid"]),
        )
