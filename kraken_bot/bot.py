"""
bot.py
======
Orchestrateur live / paper trading.

Boucle principale (toutes les `POLL_INTERVAL_SEC` secondes) :
  1. prix temps réel (WebSocket, repli REST) ;
  2. équité -> contrôle du drawdown 7 jours (hard stop) ;
  3. positions ouvertes : mise à jour du trailing stop ATR sur le prix live, sortie si touché ;
  4. à chaque clôture de bougie : recalcul des indicateurs, signaux d'entrée,
     pyramiding, déplacement du stop-loss posé chez Kraken ;
  5. sauvegarde atomique de l'état.

Usage :
  python bot.py              # lance le bot (dry_run selon .env, simulation par défaut)
  python bot.py --status     # affiche l'état persistant
  python bot.py --reset-halt # réarme le bot après un hard stop (décision manuelle)
"""
from __future__ import annotations

import argparse
import json
import logging
import signal
import sys
import time
from datetime import datetime, timezone
from typing import Dict

from execution import ExecutionEngine, FillResult
from kraken_config import BotConfig, load_config, setup_logging
from kraken_feed import TIMEFRAME_MS, KrakenFeed
from risk_engine import Position, RiskEngine, update_trailing_stop
from strategy import MomentumBreakoutStrategy, Signal, StrategyParams

log = logging.getLogger("bot")

EXIT_HALTED = 2
CANDLE_SETTLE_MS = 5_000       # attente après la clôture pour que Kraken finalise la bougie
RECONCILE_EVERY_SEC = 60


class TradingBot:
    def __init__(self, cfg: BotConfig):
        self.cfg = cfg
        self.feed = KrakenFeed(cfg)
        self.symbols = self.feed.connect()
        self.exec = ExecutionEngine(cfg, self.feed)
        self.risk = RiskEngine(cfg, cfg.state_file)
        self.strategy = MomentumBreakoutStrategy(StrategyParams.from_config(cfg))
        self.tf_ms = TIMEFRAME_MS[cfg.timeframe]
        self.last_candle: Dict[str, int] = {}
        self.running = True
        self._last_reconcile = 0.0

    # ----------------------------------------------------------- utilitaires
    def now(self) -> float:
        return self.feed.server_time_ms() / 1000.0

    def prices(self) -> Dict[str, float]:
        return {s: self.feed.last_price(s) for s in self.symbols}

    def equity(self, prices: Dict[str, float]) -> float:
        held = {s: p.amount for s, p in self.risk.positions.items()}
        return self.exec.equity(held, prices)

    def fees(self, symbol: str = None):
        return self.exec.current_fees(self.risk.rolling_volume(self.now()), symbol)

    def stop(self, *_):
        log.info("Signal d'arrêt reçu, fin de la boucle après l'itération en cours")
        self.running = False

    # ------------------------------------------------------------- démarrage
    def startup(self) -> None:
        mode = "SIMULATION (dry_run)" if self.cfg.dry_run else "LIVE - ORDRES RÉELS"
        log.info("=== Démarrage du bot Kraken | %s | %s | %s ===", mode, self.cfg.timeframe,
                 ", ".join(self.symbols))
        if self.risk.halted:
            log.critical("Bot en HARD STOP : %s", self.risk.halt_reason)
            log.critical("Analysez la situation puis relancez avec --reset-halt.")
            sys.exit(EXIT_HALTED)
        self.reconcile(force=True)
        prices = self.prices()
        self.risk.initialize(self.equity(prices))
        maker, taker = self.fees(self.symbols[0])
        log.info("Frais du palier courant : maker %.2f%% / taker %.2f%%", maker * 100, taker * 100)
        self.feed.start_ws()
        # bougies déjà clôturées au démarrage : on met à jour les stops sans ouvrir de position
        for s in self.symbols:
            self.on_candle_close(s, allow_entry=False)
            self.last_candle[s] = self.last_closed_candle_ms()

    def reconcile(self, force: bool = False) -> None:
        """Vérifie que les positions de l'état correspondent à la réalité chez Kraken."""
        if self.cfg.dry_run:
            return
        if not force and time.time() - self._last_reconcile < RECONCILE_EVERY_SEC:
            return
        self._last_reconcile = time.time()
        self.exec.invalidate_balance()
        for symbol, pos in list(self.risk.positions.items()):
            stop_order = self.exec.stop_fill(symbol, pos.exchange_stop_id)
            if stop_order:
                log.warning("%s : stop-loss Kraken déclenché pendant l'absence du bot", symbol)
                self._record_exit(symbol, FillResult(), "exchange_stop", stop_order)
                continue
            held = self.exec.total(self.feed.base_currency(symbol))
            if held < self.feed.min_amount(symbol):
                log.error("%s : plus aucun solde chez Kraken, position retirée de l'état", symbol)
                self.risk.close_position(symbol, self.feed.last_price(symbol), pos.amount, 0.0,
                                         self.now(), "external_close")
                continue
            if held < pos.amount * 0.99:
                log.warning("%s : solde %.8f < position %.8f, ajustement", symbol, held, pos.amount)
                ratio = held / pos.amount
                pos.cost *= ratio
                pos.fees_paid *= ratio
                pos.risk_amount *= ratio
                pos.amount = held
            if self.cfg.use_exchange_stop and pos.exchange_stop_id:
                try:
                    o = self.feed.private("fetch_order", pos.exchange_stop_id, symbol)
                    alive = o.get("status") == "open"
                except Exception:
                    alive = False
                if not alive:
                    log.warning("%s : stop-loss Kraken absent, nouvelle pose", symbol)
                    pos.exchange_stop_id = self.exec.place_stop(symbol, pos.amount, pos.stop_price)
                    pos.exchange_stop_price = pos.stop_price
            elif self.cfg.use_exchange_stop:
                pos.exchange_stop_id = self.exec.place_stop(symbol, pos.amount, pos.stop_price)
                pos.exchange_stop_price = pos.stop_price
        self.risk.save()

    # ---------------------------------------------------------------- boucle
    def last_closed_candle_ms(self) -> int:
        now_ms = self.feed.server_time_ms() - CANDLE_SETTLE_MS
        return (now_ms // self.tf_ms) * self.tf_ms - self.tf_ms

    def run(self) -> int:
        signal.signal(signal.SIGINT, self.stop)
        signal.signal(signal.SIGTERM, self.stop)
        self.startup()
        while self.running:
            started = time.time()
            try:
                self.loop_once()
            except Exception as exc:
                log.exception("Erreur dans la boucle principale : %s", exc)
                time.sleep(5)
            if self.risk.halted:
                self.shutdown()
                return EXIT_HALTED
            time.sleep(max(0.0, self.cfg.poll_interval_sec - (time.time() - started)))
        self.shutdown()
        return 0

    def loop_once(self) -> None:
        self.reconcile()
        prices = self.prices()
        equity = self.equity(prices)
        if self.risk.record_equity(self.now(), equity, min_interval=60):
            self.hard_stop()
            return
        for symbol in self.symbols:
            pos = self.risk.positions.get(symbol)
            if pos:
                self.manage_intrabar(symbol, pos, prices[symbol])
            candle = self.last_closed_candle_ms()
            if self.last_candle.get(symbol) != candle:
                self.on_candle_close(symbol, allow_entry=True)
                self.last_candle[symbol] = candle
        self.risk.save()
        self.exec.persist()

    def manage_intrabar(self, symbol: str, pos: Position, price: float) -> None:
        update_trailing_stop(pos, price, 0.0, self.cfg.trailing_stop_atr)
        if price <= pos.stop_price:
            log.info("%s : prix %.4f <= stop %.4f -> sortie", symbol, price, pos.stop_price)
            self.exit_position(symbol, "trailing_stop")

    def on_candle_close(self, symbol: str, allow_entry: bool) -> None:
        df = self.feed.fetch_ohlcv_df(symbol)
        sig = self.strategy.latest_signal(df)
        if sig is None:
            log.warning("%s : historique insuffisant pour les indicateurs", symbol)
            return
        log.info("%s | %s | close %.4f | ATR %.4f | ADX %.1f | vol x%.2f | BB+ %.4f | signal=%s",
                 symbol, sig.timestamp.strftime("%Y-%m-%d %H:%M"), sig.close, sig.atr, sig.adx,
                 sig.vol_ratio, sig.bb_upper, sig.long_entry)
        pos = self.risk.positions.get(symbol)
        if pos:
            update_trailing_stop(pos, sig.high, sig.atr, self.cfg.trailing_stop_atr)
            if sig.close <= pos.stop_price:
                self.exit_position(symbol, "trailing_stop")
                return
            if self.cfg.exit_on_mid_band and sig.below_mid:
                self.exit_position(symbol, "mid_band")
                return
            self.sync_exchange_stop(symbol, pos)
            if allow_entry and self.risk.pyramid_allowed(pos, sig.close, sig.atr):
                self.add_to_position(symbol, pos, sig)
        elif allow_entry and sig.long_entry:
            self.open_position(symbol, sig)

    # ------------------------------------------------------------ entrées
    def open_position(self, symbol: str, sig: Signal) -> None:
        if self.risk.halted:
            return
        if len(self.risk.positions) >= self.cfg.max_open_positions:
            log.info("%s : signal ignoré, %d positions déjà ouvertes", symbol, len(self.risk.positions))
            return
        maker, taker = self.fees(symbol)
        entry_ref = self.feed.ticker(symbol)["ask"]
        stop = entry_ref - self.cfg.initial_stop_atr * sig.atr
        equity = self.equity(self.prices())
        free_quote = self.exec.free(self.cfg.quote_currency)
        fee_rate = taker if self.cfg.taker_fallback else maker
        raw_amount, _ = self.risk.size_position(equity, free_quote, entry_ref, stop, fee_rate)
        amount = self.feed.amount_to_precision(symbol, raw_amount)
        if amount < self.feed.min_amount(symbol) or amount * entry_ref < self.feed.min_cost(symbol):
            log.info("%s : taille %.8f sous les minimums Kraken, pas d'entrée", symbol, amount)
            return
        log.info("%s : SIGNAL BREAKOUT -> achat %.8f (~%.2f %s), stop initial %.4f, risque %.2f%%",
                 symbol, amount, amount * entry_ref, self.cfg.quote_currency, stop,
                 self.risk.risk_fraction() * 100)
        fill = self.exec.buy(symbol, amount, max_price=sig.close + self.cfg.entry_chase_atr * sig.atr)
        if fill.filled <= 0:
            log.info("%s : entrée non exécutée", symbol)
            return
        avg = fill.average
        stop = avg - self.cfg.initial_stop_atr * sig.atr
        risk_amount = fill.filled * (avg - stop) + fill.fee + fill.filled * stop * taker
        pos = Position(symbol=symbol, amount=fill.filled, entry_price=avg, stop_price=stop,
                       initial_stop=stop, highest_price=avg, entry_time=self.now(),
                       entry_atr=sig.atr, risk_amount=risk_amount, cost=fill.cost,
                       fees_paid=fill.fee, last_add_price=avg, last_atr=sig.atr)
        pos.exchange_stop_id = self.exec.place_stop(symbol, pos.amount, stop)
        pos.exchange_stop_price = stop
        self.risk.positions[symbol] = pos
        self.risk.register_fill_volume(self.now(), fill.cost)
        self.risk.save()
        log.info("%s : position ouverte %.8f @ %.4f (maker %.0f%%), frais %.4f, stop %.4f",
                 symbol, fill.filled, avg, 100 * fill.maker_filled / fill.filled, fill.fee, stop)

    def add_to_position(self, symbol: str, pos: Position, sig: Signal) -> None:
        maker, taker = self.fees(symbol)
        entry_ref = self.feed.ticker(symbol)["ask"]
        equity = self.equity(self.prices())
        free_quote = self.exec.free(self.cfg.quote_currency)
        raw_amount, _ = self.risk.size_position(equity, free_quote, entry_ref, pos.stop_price,
                                                taker if self.cfg.taker_fallback else maker,
                                                risk_scale=self.cfg.pyramid_risk_scale)
        amount = self.feed.amount_to_precision(symbol, raw_amount)
        if amount < self.feed.min_amount(symbol) or amount * entry_ref < self.feed.min_cost(symbol):
            return
        log.info("%s : PYRAMIDING #%d -> achat %.8f", symbol, pos.adds + 1, amount)
        fill = self.exec.buy(symbol, amount, max_price=sig.close + self.cfg.entry_chase_atr * sig.atr)
        if fill.filled <= 0:
            return
        pos.add_fill(fill.filled, fill.average, fill.fee)
        pos.adds += 1
        pos.last_add_price = fill.average
        pos.risk_amount += fill.filled * max(0.0, fill.average - pos.stop_price) + fill.fee
        self.risk.register_fill_volume(self.now(), fill.cost)
        pos.exchange_stop_id = self.exec.replace_stop(symbol, pos.exchange_stop_id, pos.amount, pos.stop_price)
        pos.exchange_stop_price = pos.stop_price
        self.risk.save()

    def sync_exchange_stop(self, symbol: str, pos: Position) -> None:
        """Remonte le stop-loss posé chez Kraken quand le trailing a suffisamment progressé."""
        if not self.cfg.use_exchange_stop or self.cfg.dry_run:
            return
        step = self.cfg.stop_update_min_atr * (pos.last_atr or pos.entry_atr)
        if pos.stop_price >= pos.exchange_stop_price + step:
            pos.exchange_stop_id = self.exec.replace_stop(symbol, pos.exchange_stop_id, pos.amount, pos.stop_price)
            pos.exchange_stop_price = pos.stop_price if pos.exchange_stop_id else 0.0
            log.info("%s : stop-loss Kraken remonté à %.4f", symbol, pos.stop_price)

    # ------------------------------------------------------------- sorties
    def exit_position(self, symbol: str, reason: str) -> None:
        pos = self.risk.positions.get(symbol)
        if pos is None:
            return
        if pos.exchange_stop_id and not self.cfg.dry_run:
            triggered = self.exec.stop_fill(symbol, pos.exchange_stop_id)
            if not triggered:
                self.exec.cancel_order(symbol, pos.exchange_stop_id)
                triggered = self.exec.stop_fill(symbol, pos.exchange_stop_id)  # course possible
            if triggered:
                self._record_exit(symbol, FillResult(), "exchange_stop", triggered)
                return
            pos.exchange_stop_id = None
        total = FillResult()
        for _ in range(3):
            remaining = pos.amount - total.filled
            if remaining < self.feed.min_amount(symbol):
                break
            part = self.exec.sell(symbol, remaining, urgent=True)
            if part.filled <= 0:
                break
            total.filled += part.filled
            total.cost += part.cost
            total.fee += part.fee
        if total.filled <= 0:
            log.error("%s : sortie impossible, nouvelle pose du stop-loss de protection", symbol)
            pos.exchange_stop_id = self.exec.place_stop(symbol, pos.amount, pos.stop_price)
            return
        self._record_exit(symbol, total, reason)

    def _record_exit(self, symbol: str, fill: FillResult, reason: str, order: dict = None) -> None:
        if order is not None:
            fill = FillResult()
            fill.merge(order)
        self.risk.register_fill_volume(self.now(), fill.cost)
        self.risk.close_position(symbol, fill.average, fill.filled, fill.fee, self.now(), reason)

    # ------------------------------------------------------------- arrêts
    def hard_stop(self) -> None:
        log.critical("HARD STOP DÉCLENCHÉ : annulation des ordres%s",
                     " et liquidation des positions" if self.cfg.flatten_on_halt else "")
        if self.cfg.flatten_on_halt:
            for symbol in list(self.risk.positions):
                self.exit_position(symbol, "hard_stop")
        self.exec.cancel_all(self.symbols)
        if not self.cfg.flatten_on_halt:
            # on garde les positions, protégées par des stops exchange reposés
            for symbol, pos in self.risk.positions.items():
                pos.exchange_stop_id = self.exec.place_stop(symbol, pos.amount, pos.stop_price)
        self.risk.save()
        self.exec.persist()
        self.running = False

    def shutdown(self) -> None:
        self.feed.stop_ws()
        self.risk.save()
        self.exec.persist()
        log.info("Bot arrêté proprement. Positions ouvertes : %s (stops Kraken conservés)",
                 list(self.risk.positions) or "aucune")


def print_status(cfg: BotConfig) -> None:
    risk = RiskEngine(cfg, cfg.state_file)
    print(json.dumps({
        "dry_run": cfg.dry_run,
        "halted": risk.halted,
        "halt_reason": risk.halt_reason,
        "initial_capital": risk.initial_capital,
        "realized_pnl": round(risk.realized_pnl, 2),
        "trades": len(risk.trades),
        "win_rate": round(sum(t.pnl > 0 for t in risk.trades) / len(risk.trades), 3) if risk.trades else None,
        "weekly_drawdown": round(risk.weekly_drawdown(time.time()), 4),
        "risk_fraction": risk.risk_fraction(),
        "positions": {s: {"amount": p.amount, "entry": p.entry_price, "stop": p.stop_price,
                          "opened": datetime.fromtimestamp(p.entry_time, timezone.utc).isoformat()}
                      for s, p in risk.positions.items()},
    }, indent=2, ensure_ascii=False))


def main() -> int:
    parser = argparse.ArgumentParser(description="Bot Kraken momentum / volatility breakout")
    parser.add_argument("--status", action="store_true", help="affiche l'état persistant et quitte")
    parser.add_argument("--reset-halt", action="store_true", help="réarme le bot après un hard stop")
    args = parser.parse_args()

    cfg = load_config()
    setup_logging(cfg)
    if args.reset_halt:
        risk = RiskEngine(cfg, cfg.state_file)
        log.warning("Réarmement manuel du hard stop (ancien motif : %s)", risk.halt_reason or "aucun")
        risk.reset_halt()
    if args.status:
        print_status(cfg)
        return 0
    if not cfg.dry_run:
        log.warning("MODE LIVE : des ordres réels vont être envoyés sur Kraken.")
    return TradingBot(cfg).run()


if __name__ == "__main__":
    sys.exit(main())
