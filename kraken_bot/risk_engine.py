"""
risk_engine.py
==============
Gestion du risque, partagée par le bot live et le backtester :

* taille de position = (capital de base x risque%) / (distance au stop + frais aller-retour)
* risque% fixe (2-3 %) ou demi-Kelly calculé sur l'historique des trades (en multiples de R)
* intérêts composés : une fraction paramétrable des gains réalisés est réinvestie,
  les pertes réduisent toujours intégralement le capital de base
* plafond de risque ouvert cumulé et de notionnel (aucun levier)
* trailing stop chandelier basé sur l'ATR
* coupe-circuit : drawdown sur 7 jours glissants >= seuil -> arrêt du trading
  persistant (fichier d'état), réarmable uniquement à la main
"""
from __future__ import annotations

import json
import logging
import os
import time
from dataclasses import asdict, dataclass
from typing import Dict, List, Optional, Tuple

from kraken_config import BotConfig

log = logging.getLogger("risk")

WEEK_SEC = 7 * 24 * 3600


@dataclass
class Position:
    symbol: str
    amount: float                 # quantité de base détenue
    entry_price: float            # prix moyen d'entrée
    stop_price: float             # stop courant (trailing)
    initial_stop: float
    highest_price: float          # plus haut atteint depuis l'entrée
    entry_time: float             # epoch secondes
    entry_atr: float
    risk_amount: float            # perte max prévue à l'entrée (en devise de cotation)
    cost: float                   # somme prix x quantité payée
    fees_paid: float              # frais d'entrée cumulés (devise de cotation)
    adds: int = 0
    last_add_price: float = 0.0
    last_atr: float = 0.0
    exchange_stop_id: Optional[str] = None
    exchange_stop_price: float = 0.0

    @property
    def open_risk(self) -> float:
        """Perte encore possible si le stop est touché (0 si le stop protège un gain)."""
        return max(0.0, self.entry_price - self.stop_price) * self.amount

    def add_fill(self, amount: float, price: float, fee: float) -> None:
        self.cost += amount * price
        self.fees_paid += fee
        self.amount += amount
        self.entry_price = self.cost / self.amount


@dataclass
class TradeRecord:
    symbol: str
    entry_time: float
    exit_time: float
    entry_price: float
    exit_price: float
    amount: float
    pnl: float                    # net de tous les frais
    fees: float
    r_multiple: float
    reason: str


def update_trailing_stop(pos: Position, price_high: float, atr_value: float,
                         trail_mult: float) -> bool:
    """Chandelier exit : stop = plus haut depuis l'entrée - k x ATR. Le stop ne descend jamais."""
    if price_high > pos.highest_price:
        pos.highest_price = price_high
    if atr_value > 0:
        pos.last_atr = atr_value
    atr_ref = pos.last_atr or pos.entry_atr
    candidate = pos.highest_price - trail_mult * atr_ref
    if candidate > pos.stop_price:
        pos.stop_price = candidate
        return True
    return False


class RiskEngine:
    def __init__(self, cfg: BotConfig, state_path: Optional[str] = None):
        self.cfg = cfg
        self.state_path = state_path
        self.initial_capital: float = 0.0
        self.realized_pnl: float = 0.0
        self.trades: List[TradeRecord] = []
        self.positions: Dict[str, Position] = {}
        self.equity_history: List[Tuple[float, float]] = []
        self.halted: bool = False
        self.halt_reason: str = ""
        self.volume_30d: List[Tuple[float, float]] = []   # (ts, notionnel) pour le palier de frais
        self._last_equity_record = 0.0
        if state_path:
            self.load()

    # ------------------------------------------------------------------ état
    def initialize(self, equity: float) -> None:
        if self.initial_capital <= 0:
            self.initial_capital = equity
            log.info("Capital initial enregistré : %.2f %s", equity, self.cfg.quote_currency)
            self.save()

    def save(self) -> None:
        if not self.state_path:
            return
        state = {
            "initial_capital": self.initial_capital,
            "realized_pnl": self.realized_pnl,
            "trades": [asdict(t) for t in self.trades[-1000:]],
            "positions": {k: asdict(v) for k, v in self.positions.items()},
            "equity_history": self.equity_history,
            "halted": self.halted,
            "halt_reason": self.halt_reason,
            "volume_30d": self.volume_30d,
            "dry_run": self.cfg.dry_run,
        }
        tmp = self.state_path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump(state, fh, indent=2)
        os.replace(tmp, self.state_path)  # écriture atomique

    def load(self) -> None:
        if not self.state_path or not os.path.exists(self.state_path):
            return
        with open(self.state_path, encoding="utf-8") as fh:
            state = json.load(fh)
        if state.get("dry_run") is not None and state["dry_run"] != self.cfg.dry_run:
            raise RuntimeError(
                f"Le fichier d'état {self.state_path} a été créé en dry_run={state['dry_run']} "
                f"mais le bot tourne en dry_run={self.cfg.dry_run}. Utilisez un STATE_FILE distinct.")
        self.initial_capital = state.get("initial_capital", 0.0)
        self.realized_pnl = state.get("realized_pnl", 0.0)
        self.trades = [TradeRecord(**t) for t in state.get("trades", [])]
        self.positions = {k: Position(**v) for k, v in state.get("positions", {}).items()}
        self.equity_history = [tuple(x) for x in state.get("equity_history", [])]
        self.halted = state.get("halted", False)
        self.halt_reason = state.get("halt_reason", "")
        self.volume_30d = [tuple(x) for x in state.get("volume_30d", [])]
        log.info("État chargé : %d positions, %d trades, PnL réalisé %.2f, halted=%s",
                 len(self.positions), len(self.trades), self.realized_pnl, self.halted)

    def reset_halt(self) -> None:
        self.halted = False
        self.halt_reason = ""
        self.equity_history = []   # nouvelle fenêtre de référence pour le drawdown
        self.save()

    # -------------------------------------------------------- capital & Kelly
    def capital_basis(self, equity: float) -> float:
        """Capital sur lequel on calcule le risque.

        - compound=True : capital initial + compound_ratio x gains réalisés (les pertes
          comptent à 100 %), plafonné par l'équité réelle.
        - compound=False : capital initial, réduit par les pertes, jamais augmenté.
        """
        pnl = self.realized_pnl
        if self.cfg.compound:
            basis = self.initial_capital + (self.cfg.compound_ratio * pnl if pnl > 0 else pnl)
        else:
            basis = self.initial_capital + min(pnl, 0.0)
        return max(0.0, min(basis, equity))

    def kelly_stats(self) -> Optional[Tuple[float, float, float]]:
        """(win_rate, payoff_ratio, kelly_full) sur les derniers trades, en multiples de R."""
        recent = self.trades[-self.cfg.kelly_lookback:]
        if len(recent) < self.cfg.kelly_min_trades:
            return None
        wins = [t.r_multiple for t in recent if t.r_multiple > 0]
        losses = [-t.r_multiple for t in recent if t.r_multiple <= 0]
        if not wins:
            return 0.0, 0.0, -1.0
        if not losses:
            return 1.0, float("inf"), 1.0
        p = len(wins) / len(recent)
        b = (sum(wins) / len(wins)) / max(sum(losses) / len(losses), 1e-9)
        return p, b, p - (1 - p) / b

    def risk_fraction(self) -> float:
        cap = self.cfg.max_risk_per_trade
        if self.cfg.sizing_mode == "kelly":
            stats = self.kelly_stats()
            if stats is None:
                return min(self.cfg.risk_per_trade, cap)
            _, _, k = stats
            if k <= 0:
                return self.cfg.kelly_floor
            return max(self.cfg.kelly_floor, min(self.cfg.kelly_fraction * k, cap))
        return min(self.cfg.risk_per_trade, cap)

    def open_risk(self) -> float:
        return sum(p.open_risk for p in self.positions.values())

    # ------------------------------------------------------------- sizing
    def size_position(self, equity: float, free_quote: float, entry_price: float,
                      stop_price: float, fee_rate: float, risk_scale: float = 1.0
                      ) -> Tuple[float, float]:
        """Retourne (quantité brute non arrondie, montant risqué). 0 si refus."""
        if self.halted or entry_price <= 0 or stop_price >= entry_price:
            return 0.0, 0.0
        basis = self.capital_basis(equity)
        risk_budget = basis * self.risk_fraction() * risk_scale
        remaining = basis * self.cfg.max_portfolio_risk - self.open_risk()
        risk_budget = min(risk_budget, remaining)
        if risk_budget <= 0:
            log.info("Budget de risque du portefeuille épuisé (risque ouvert %.2f)", self.open_risk())
            return 0.0, 0.0
        # risque par unité : distance au stop + frais d'entrée et de sortie
        per_unit = (entry_price - stop_price) + entry_price * fee_rate + stop_price * fee_rate
        amount = risk_budget / per_unit
        max_notional = min(free_quote / (1 + fee_rate) * 0.995, basis * self.cfg.max_position_pct)
        amount = min(amount, max(0.0, max_notional) / entry_price)
        return amount, amount * per_unit

    def pyramid_allowed(self, pos: Position, price: float, atr_value: float) -> bool:
        if self.cfg.pyramid_max_adds <= 0 or pos.adds >= self.cfg.pyramid_max_adds:
            return False
        ref = pos.last_add_price or pos.entry_price
        # on n'ajoute que si le stop courant protège déjà le prix moyen (trade "gratuit")
        return price >= ref + self.cfg.pyramid_step_atr * atr_value and pos.stop_price >= pos.entry_price

    # -------------------------------------------------------- suivi des trades
    def register_fill_volume(self, ts: float, notional: float) -> None:
        self.volume_30d.append((ts, notional))
        cutoff = ts - 30 * 24 * 3600
        self.volume_30d = [(t, v) for t, v in self.volume_30d if t >= cutoff]

    def rolling_volume(self, now: float) -> float:
        cutoff = now - 30 * 24 * 3600
        return sum(v for t, v in self.volume_30d if t >= cutoff) + self.cfg.fee_volume_30d

    def close_position(self, symbol: str, exit_price: float, exit_amount: float,
                       exit_fee: float, ts: float, reason: str) -> Optional[TradeRecord]:
        pos = self.positions.pop(symbol, None)
        if pos is None:
            return None
        proceeds = exit_price * exit_amount
        # coût proportionnel si la sortie est partielle (arrondis de précision)
        ratio = min(1.0, exit_amount / pos.amount) if pos.amount > 0 else 1.0
        entry_cost = pos.cost * ratio
        entry_fees = pos.fees_paid * ratio
        pnl = proceeds - exit_fee - entry_cost - entry_fees
        r_mult = pnl / pos.risk_amount if pos.risk_amount > 0 else 0.0
        trade = TradeRecord(symbol=symbol, entry_time=pos.entry_time, exit_time=ts,
                            entry_price=pos.entry_price, exit_price=exit_price,
                            amount=exit_amount, pnl=pnl, fees=entry_fees + exit_fee,
                            r_multiple=r_mult, reason=reason)
        self.trades.append(trade)
        self.realized_pnl += pnl
        log.info("Trade clôturé %s (%s) : PnL %+.2f, %.2fR, frais %.2f", symbol, reason,
                 pnl, r_mult, trade.fees)
        self.save()
        return trade

    # ---------------------------------------------------- coupe-circuit
    def weekly_drawdown(self, now: float) -> float:
        window = [e for t, e in self.equity_history if t >= now - WEEK_SEC]
        if not window:
            return 0.0
        peak = max(window)
        return 0.0 if peak <= 0 else 1.0 - window[-1] / peak

    def record_equity(self, now: float, equity: float, min_interval: float = 0.0) -> bool:
        """Enregistre l'équité et déclenche le hard stop si besoin. Retourne True si le bot
        vient d'être stoppé par cet appel."""
        if now - self._last_equity_record >= min_interval:
            self.equity_history.append((now, equity))
            self._last_equity_record = now
            cutoff = now - WEEK_SEC - 24 * 3600
            self.equity_history = [(t, e) for t, e in self.equity_history if t >= cutoff]
        else:
            # mesure non archivée, mais on vérifie quand même le seuil sur la valeur courante
            peak = max([e for t, e in self.equity_history if t >= now - WEEK_SEC] or [equity])
            if peak > 0 and 1.0 - equity / peak >= self.cfg.max_weekly_drawdown and not self.halted:
                self.equity_history.append((now, equity))
        dd = self.weekly_drawdown(now)
        if dd >= self.cfg.max_weekly_drawdown and not self.halted:
            self.halted = True
            self.halt_reason = (f"Drawdown 7 jours {dd:.2%} >= seuil {self.cfg.max_weekly_drawdown:.2%} "
                                f"(équité {equity:.2f}) le {time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(now))} UTC")
            log.critical("HARD STOP : %s", self.halt_reason)
            self.save()
            return True
        return False
