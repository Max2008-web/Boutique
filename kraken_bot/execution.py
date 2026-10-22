"""
execution.py
============
Envoi, suivi, modification et annulation des ordres Kraken.

* Entrées en **post-only** (flag Kraken `oflags=post`) au meilleur bid + 1 tick :
  on paie les frais *maker*. Repricing jusqu'à `max_reprice` fois si non exécuté,
  abandon si le prix s'est enfui de plus de `entry_chase_atr` ATR, repli market optionnel.
* Frais toujours prélevés en devise de cotation (`oflags=fciq`) : les quantités de
  crypto restent exactes.
* Idempotence : chaque ordre porte un `userref` aléatoire ; en cas de timeout réseau
  on vérifie chez Kraken si l'ordre existe avant de le renvoyer (pas de doublon).
* Stop-loss « catastrophe » posé chez Kraken (ordre `stop-loss`) pour protéger le
  capital même si le bot / le VPS tombe.
* Mode `dry_run` : un broker papier simule exécutions, frais et soldes à partir
  des prix réels.
"""
from __future__ import annotations

import itertools
import json
import logging
import os
import random
import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import ccxt

from kraken_config import BotConfig, fee_for_volume
from kraken_feed import KrakenFeed

log = logging.getLogger("execution")

FINAL_STATUSES = ("closed", "canceled", "cancelled", "expired", "rejected")


@dataclass
class FillResult:
    filled: float = 0.0
    cost: float = 0.0            # somme prix x quantité
    fee: float = 0.0             # en devise de cotation
    maker_filled: float = 0.0
    order_ids: List[str] = field(default_factory=list)

    @property
    def average(self) -> float:
        return self.cost / self.filled if self.filled > 0 else 0.0

    def merge(self, order: dict) -> None:
        filled = float(order.get("filled") or 0.0)
        if filled <= 0:
            return
        avg = float(order.get("average") or order.get("price") or 0.0)
        cost = float(order.get("cost") or avg * filled)
        self.filled += filled
        self.cost += cost
        fee = order.get("fee") or {}
        self.fee += float(fee.get("cost") or 0.0)
        if order.get("_maker"):
            self.maker_filled += filled
        if order.get("id"):
            self.order_ids.append(str(order["id"]))


class PaperBroker:
    """Broker simulé : soldes, ordres limites/market, frais Kraken du palier courant."""

    def __init__(self, cfg: BotConfig, feed: KrakenFeed):
        self.cfg = cfg
        self.feed = feed
        self.path = cfg.state_file + ".paper.json"
        self.balances: Dict[str, float] = {cfg.quote_currency: cfg.sim_starting_balance}
        self.orders: Dict[str, dict] = {}
        self._ids = itertools.count(1)
        if os.path.exists(self.path):
            with open(self.path, encoding="utf-8") as fh:
                self.balances = json.load(fh)
            log.info("[paper] soldes simulés rechargés : %s", self.balances)

    def save(self) -> None:
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump(self.balances, fh, indent=2)
        os.replace(tmp, self.path)

    def fees(self):
        return fee_for_volume(self.cfg.fee_volume_30d)

    def balance(self, currency: str) -> float:
        return self.balances.get(currency, 0.0)

    def _apply_fill(self, symbol: str, side: str, amount: float, price: float, maker: bool) -> dict:
        base, quote = symbol.split("/")
        maker_fee, taker_fee = self.fees()
        cost = amount * price
        fee = cost * (maker_fee if maker else taker_fee)
        if side == "buy":
            if self.balance(quote) < cost + fee - 1e-9:
                raise ccxt.InsufficientFunds(f"[paper] solde {quote} insuffisant")
            self.balances[quote] = self.balance(quote) - cost - fee
            self.balances[base] = self.balance(base) + amount
        else:
            if self.balance(base) < amount - 1e-12:
                raise ccxt.InsufficientFunds(f"[paper] solde {base} insuffisant")
            self.balances[base] = self.balance(base) - amount
            self.balances[quote] = self.balance(quote) + cost - fee
        return {"filled": amount, "average": price, "cost": cost,
                "fee": {"cost": fee, "currency": quote}, "_maker": maker}

    def create_limit(self, symbol: str, side: str, amount: float, price: float, post_only: bool) -> dict:
        t = self.feed.ticker(symbol)
        oid = f"paper-{next(self._ids)}"
        if post_only and ((side == "buy" and price >= t["ask"]) or (side == "sell" and price <= t["bid"])):
            order = {"id": oid, "status": "canceled", "filled": 0.0, "reason": "Post only order"}
        else:
            order = {"id": oid, "symbol": symbol, "side": side, "amount": amount, "price": price,
                     "status": "open", "filled": 0.0, "created": time.time()}
        self.orders[oid] = order
        return dict(order)

    def poll(self, oid: str) -> dict:
        order = self.orders[oid]
        if order["status"] == "open":
            t = self.feed.ticker(order["symbol"])
            crossed = (t["last"] <= order["price"] or t["ask"] <= order["price"]) if order["side"] == "buy" \
                else (t["last"] >= order["price"] or t["bid"] >= order["price"])
            if crossed:
                order.update(self._apply_fill(order["symbol"], order["side"], order["amount"],
                                              order["price"], maker=True))
                order["status"] = "closed"
        return dict(order)

    def cancel(self, oid: str) -> None:
        order = self.orders.get(oid)
        if order and order["status"] == "open":
            order["status"] = "canceled"

    def market(self, symbol: str, side: str, amount: float) -> dict:
        t = self.feed.ticker(symbol)
        price = t["ask"] if side == "buy" else t["bid"]
        oid = f"paper-{next(self._ids)}"
        order = {"id": oid, "status": "closed"}
        order.update(self._apply_fill(symbol, side, amount, price, maker=False))
        self.orders[oid] = order
        return dict(order)


class ExecutionEngine:
    def __init__(self, cfg: BotConfig, feed: KrakenFeed):
        self.cfg = cfg
        self.feed = feed
        self.paper = PaperBroker(cfg, feed) if cfg.dry_run else None
        self._placed_at: Dict[str, float] = {}
        self._balance_cache: Optional[dict] = None
        self._balance_ts = 0.0

    # ------------------------------------------------------------- soldes
    def fetch_balance(self, max_age: float = 5.0) -> dict:
        if self.paper:
            return {"free": dict(self.paper.balances), "total": dict(self.paper.balances)}
        if self._balance_cache is None or time.time() - self._balance_ts > max_age:
            self._balance_cache = self.feed.private("fetch_balance")
            self._balance_ts = time.time()
        return self._balance_cache

    def invalidate_balance(self) -> None:
        self._balance_ts = 0.0

    def persist(self) -> None:
        if self.paper:
            self.paper.save()

    def free(self, currency: str) -> float:
        return float((self.fetch_balance()["free"] or {}).get(currency) or 0.0)

    def total(self, currency: str) -> float:
        return float((self.fetch_balance()["total"] or {}).get(currency) or 0.0)

    def equity(self, positions: Dict[str, float], prices: Dict[str, float]) -> float:
        """Équité du bot = devise de cotation + valeur des positions gérées par le bot."""
        eq = self.total(self.cfg.quote_currency)
        for symbol, amount in positions.items():
            eq += amount * prices.get(symbol, 0.0)
        return eq

    def current_fees(self, volume_30d: float):
        return fee_for_volume(volume_30d)

    # ----------------------------------------------------- primitives live
    def _new_userref(self) -> int:
        return random.randint(1, 2**31 - 1)

    def _find_by_userref(self, symbol: str, userref: int) -> Optional[dict]:
        for method in ("fetch_open_orders", "fetch_closed_orders"):
            try:
                orders = self.feed.private(method, symbol, None, None, {"userref": userref})
            except Exception as exc:
                log.warning("Recherche userref %s via %s impossible : %s", userref, method, exc)
                continue
            if orders:
                return orders[0]
        return None

    def _create_order(self, symbol: str, order_type: str, side: str, amount: float,
                      price: Optional[float] = None, params: Optional[dict] = None,
                      post_only: bool = False) -> dict:
        params = dict(params or {})
        userref = self._new_userref()
        params["userref"] = userref
        params["oflags"] = "fciq,post" if post_only else "fciq"
        self.feed.trading_counter(symbol).acquire_add()
        delay = 1.0
        for attempt in range(1, self.cfg.max_retries + 1):
            try:
                order = self.feed.exchange.create_order(symbol, order_type, side, amount, price, params)
                self._placed_at[str(order["id"])] = time.time()
                log.info("Ordre envoyé %s %s %s %.8f @ %s -> %s", order_type, side, symbol, amount,
                         price if price else params.get("stopLossPrice", "market"), order["id"])
                return order
            except ccxt.InvalidNonce:
                self.feed.resync_clock()          # l'ordre a été refusé : renvoi sûr
            except (ccxt.DDoSProtection, ccxt.RateLimitExceeded) as exc:
                log.warning("Rate limit à l'envoi d'ordre (%s), pause", exc)
                self.feed.rest_counter.penalize()
                time.sleep(delay * 5)
            except (ccxt.RequestTimeout, ccxt.ExchangeNotAvailable, ccxt.NetworkError) as exc:
                # issue ambiguë : l'ordre a peut-être été accepté -> on vérifie avant de renvoyer
                log.warning("Erreur réseau à l'envoi (%s), vérification userref %s", exc, userref)
                time.sleep(delay)
                existing = self._find_by_userref(symbol, userref)
                if existing:
                    log.info("Ordre retrouvé après timeout : %s", existing["id"])
                    self._placed_at[str(existing["id"])] = time.time()
                    return existing
            delay = min(delay * 2, 30.0)
        raise ccxt.NetworkError(f"Envoi d'ordre {side} {symbol} impossible après {self.cfg.max_retries} essais")

    def _fetch_order(self, symbol: str, oid: str) -> dict:
        if self.paper:
            return self.paper.poll(oid)
        return self.feed.private("fetch_order", oid, symbol)

    def cancel_order(self, symbol: str, oid: str) -> None:
        if self.paper:
            self.paper.cancel(oid)
            return
        age = time.time() - self._placed_at.get(str(oid), 0.0)
        self.feed.trading_counter(symbol).acquire_cancel(age)
        try:
            self.feed.private("cancel_order", oid, symbol)
            log.info("Ordre annulé %s", oid)
        except ccxt.OrderNotFound:
            log.info("Ordre %s déjà exécuté ou annulé", oid)
        finally:
            self._placed_at.pop(str(oid), None)
            self.invalidate_balance()

    def cancel_all(self, symbols: List[str]) -> None:
        if self.paper:
            for oid, order in self.paper.orders.items():
                if order.get("status") == "open":
                    order["status"] = "canceled"
            return
        for symbol in symbols:
            try:
                for order in self.feed.private("fetch_open_orders", symbol):
                    self.cancel_order(symbol, order["id"])
            except Exception as exc:
                log.error("Annulation des ordres %s impossible : %s", symbol, exc)

    def _wait_order(self, symbol: str, oid: str, timeout: float, poll: float = 3.0) -> dict:
        deadline = time.time() + timeout
        while True:
            order = self._fetch_order(symbol, oid)
            if order.get("status") in FINAL_STATUSES or time.time() >= deadline:
                return order
            time.sleep(poll if not self.paper else 2.0)

    def _cancel_and_final(self, symbol: str, oid: str) -> dict:
        self.cancel_order(symbol, oid)
        order = self._fetch_order(symbol, oid)
        for _ in range(5):
            if order.get("status") in FINAL_STATUSES:
                break
            time.sleep(1.0)
            order = self._fetch_order(symbol, oid)
        return order

    def _post_only_limit(self, symbol: str, side: str, amount: float, price: float) -> dict:
        if self.paper:
            return self.paper.create_limit(symbol, side, amount, price, post_only=True)
        try:
            return self._create_order(symbol, "limit", side, amount, price, post_only=True)
        except (ccxt.OrderImmediatelyFillable, ccxt.InvalidOrder) as exc:
            if "post" in str(exc).lower() or isinstance(exc, ccxt.OrderImmediatelyFillable):
                return {"id": None, "status": "canceled", "filled": 0.0, "reason": "Post only order"}
            raise

    def _market(self, symbol: str, side: str, amount: float) -> dict:
        if self.paper:
            return self.paper.market(symbol, side, amount)
        order = self._create_order(symbol, "market", side, amount)
        final = self._wait_order(symbol, order["id"], timeout=20, poll=1.5)
        self.invalidate_balance()
        return final

    # -------------------------------------------------------- haut niveau
    def buy(self, symbol: str, amount: float, max_price: float) -> FillResult:
        """Achat priorité maker : post-only au bid + 1 tick, repricing, repli market optionnel."""
        result = FillResult()
        tick = self.feed.tick_size(symbol)
        min_amount = self.feed.min_amount(symbol)
        min_cost = self.feed.min_cost(symbol)
        remaining = self.feed.amount_to_precision(symbol, amount)

        if self.cfg.post_only:
            for attempt in range(self.cfg.max_reprice + 1):
                t = self.feed.ticker(symbol)
                price = t["bid"] + tick if t["bid"] + tick < t["ask"] else t["bid"]
                price = self.feed.price_to_precision(symbol, price)
                if price > max_price:
                    log.info("%s : prix %.4f au-delà du plafond de poursuite %.4f, abandon", symbol, price, max_price)
                    return result
                order = self._post_only_limit(symbol, "buy", remaining, price)
                if not order.get("id") or order.get("status") in ("canceled", "rejected") and not order.get("filled"):
                    log.info("%s : post-only rejeté (aurait été taker), repricing %d", symbol, attempt + 1)
                    time.sleep(1.0)
                    continue
                order = self._wait_order(symbol, order["id"], self.cfg.order_timeout_sec)
                if order.get("status") not in FINAL_STATUSES:
                    order = self._cancel_and_final(symbol, order["id"])
                order["_maker"] = True
                result.merge(order)
                remaining = self.feed.amount_to_precision(symbol, amount - result.filled)
                if remaining < min_amount or remaining * price < min_cost:
                    break

        if self.cfg.taker_fallback or not self.cfg.post_only:
            remaining = self.feed.amount_to_precision(symbol, amount - result.filled)
            t = self.feed.ticker(symbol)
            if remaining >= min_amount and remaining * t["ask"] >= min_cost:
                if t["ask"] <= max_price:
                    log.info("%s : repli market pour %.8f", symbol, remaining)
                    result.merge(self._market(symbol, "buy", remaining))
                else:
                    log.info("%s : ask %.4f > plafond %.4f, pas de repli market", symbol, t["ask"], max_price)
        self.invalidate_balance()
        return result

    def sell(self, symbol: str, amount: float, urgent: bool = True) -> FillResult:
        """Vente de `amount` (plafonnée au solde libre). Market direct si urgent."""
        result = FillResult()
        base = self.feed.base_currency(symbol)
        self.invalidate_balance()
        amount = self.feed.amount_to_precision(symbol, min(amount, self.free(base)))
        if amount < self.feed.min_amount(symbol):
            log.warning("%s : quantité %.8f sous le minimum Kraken, rien à vendre", symbol, amount)
            return result
        if self.cfg.post_only and (not urgent or self.cfg.exit_post_only_first):
            t = self.feed.ticker(symbol)
            tick = self.feed.tick_size(symbol)
            price = t["ask"] - tick if t["ask"] - tick > t["bid"] else t["ask"]
            order = self._post_only_limit(symbol, "sell", amount, self.feed.price_to_precision(symbol, price))
            if order.get("id"):
                order = self._wait_order(symbol, order["id"], max(10, self.cfg.order_timeout_sec // 2))
                if order.get("status") not in FINAL_STATUSES:
                    order = self._cancel_and_final(symbol, order["id"])
                order["_maker"] = True
                result.merge(order)
        remaining = self.feed.amount_to_precision(symbol, amount - result.filled)
        if remaining >= self.feed.min_amount(symbol):
            result.merge(self._market(symbol, "sell", remaining))
        self.invalidate_balance()
        return result

    # ------------------------------------------------ stop-loss chez Kraken
    def place_stop(self, symbol: str, amount: float, stop_price: float) -> Optional[str]:
        if not self.cfg.use_exchange_stop:
            return None
        if self.paper:
            return f"paper-stop-{symbol}-{stop_price:.2f}"
        amount = self.feed.amount_to_precision(symbol, amount)
        if amount < self.feed.min_amount(symbol):
            return None
        try:
            order = self._create_order(symbol, "market", "sell", amount, None,
                                       {"stopLossPrice": stop_price, "trigger": "last"})
            return str(order["id"])
        except Exception as exc:
            log.error("Pose du stop-loss %s @ %.4f impossible : %s", symbol, stop_price, exc)
            return None

    def replace_stop(self, symbol: str, old_id: Optional[str], amount: float,
                     stop_price: float) -> Optional[str]:
        if old_id:
            self.cancel_order(symbol, old_id)
        return self.place_stop(symbol, amount, stop_price)

    def stop_fill(self, symbol: str, stop_id: Optional[str]) -> Optional[dict]:
        """Si le stop exchange a été déclenché, retourne l'ordre exécuté."""
        if not stop_id or self.paper:
            return None
        try:
            order = self.feed.private("fetch_order", stop_id, symbol)
        except ccxt.OrderNotFound:
            return None
        if order.get("status") == "closed" and float(order.get("filled") or 0) > 0:
            return order
        return None
