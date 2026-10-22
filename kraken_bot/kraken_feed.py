"""
kraken_feed.py
==============
Couche d'accès aux données Kraken :

* client `ccxt.kraken` avec nonce strictement croissant basé sur l'horloge
  serveur synchronisée (évite `EAPI:Invalid nonce`) ;
* réplique locale du *call counter* REST de Kraken et du compteur de trading
  par paire (évite `EAPI:Rate limit exceeded` et les bans temporaires) ;
* résolution des symboles (BTC/USD <-> XBT/USD <-> XXBTZUSD <-> XBTUSD) ;
* bougies OHLCV clôturées via REST, prix temps réel via WebSocket v2
  (`wss://ws.kraken.com/v2`, canal `ticker`) avec repli automatique sur REST.
"""
from __future__ import annotations

import asyncio
import json
import logging
import threading
import time
from typing import Callable, Dict, List, Optional, Tuple

import ccxt
import pandas as pd

from kraken_config import BotConfig

log = logging.getLogger("feed")

# (compteur max, décroissance par seconde) du call counter REST privé
REST_TIERS: Dict[str, Tuple[float, float]] = {
    "starter": (15.0, 0.33),
    "intermediate": (20.0, 0.5),
    "pro": (20.0, 1.0),
}
# (seuil max, décroissance par seconde) du compteur du moteur de trading, par paire
TRADING_TIERS: Dict[str, Tuple[float, float]] = {
    "starter": (60.0, 1.0),
    "intermediate": (125.0, 2.34),
    "pro": (180.0, 3.75),
}
# Coût de chaque appel privé dans le call counter REST (AddOrder/CancelOrder = 0,
# ils sont comptés par le moteur de trading à la place).
REST_COSTS: Dict[str, float] = {
    "fetch_balance": 1, "fetch_open_orders": 1, "fetch_closed_orders": 1,
    "fetch_order": 1, "fetch_orders": 1, "fetch_my_trades": 2, "fetch_ledger": 2,
    "fetch_trading_fees": 1, "fetch_trading_fee": 1, "create_order": 0, "cancel_order": 0,
    "cancel_all_orders": 0, "edit_order": 0,
}
TIMEFRAME_MS = {"1m": 60_000, "5m": 300_000, "15m": 900_000, "30m": 1_800_000,
                "1h": 3_600_000, "4h": 14_400_000, "1d": 86_400_000}


# ---------------------------------------------------------------------------
# Rate limiting
# ---------------------------------------------------------------------------
class KrakenCallCounter:
    """Réplique locale du call counter REST privé de Kraken.

    Chaque appel incrémente le compteur de son coût ; le compteur décroît
    linéairement. On attend avant d'appeler si l'appel ferait dépasser le
    maximum (avec une marge de sécurité d'une unité).
    """

    def __init__(self, tier: str):
        self.max_counter, self.decay = REST_TIERS[tier]
        self.counter = 0.0
        self._last = time.monotonic()
        self._lock = threading.Lock()

    def _apply_decay(self) -> None:
        now = time.monotonic()
        self.counter = max(0.0, self.counter - (now - self._last) * self.decay)
        self._last = now

    def acquire(self, cost: float = 1.0) -> None:
        if cost <= 0:
            return
        limit = self.max_counter - 1.0
        while True:
            with self._lock:
                self._apply_decay()
                if self.counter + cost <= limit:
                    self.counter += cost
                    return
                wait = (self.counter + cost - limit) / self.decay
            log.debug("Call counter saturé, attente %.2fs", wait)
            time.sleep(max(wait, 0.05))

    def penalize(self) -> None:
        """Après un `Rate limit exceeded`, on considère le compteur plein."""
        with self._lock:
            self._apply_decay()
            self.counter = self.max_counter


class KrakenTradingCounter:
    """Compteur du moteur de trading Kraken (par paire).

    AddOrder = +1. Une annulation/modification d'ordre récent est pénalisée
    (jusqu'à +8 si l'ordre a moins de 5 s), d'où l'intérêt de laisser vivre les
    ordres limites et de ne pas « spammer » le carnet.
    """

    CANCEL_PENALTY = [(5, 8), (10, 6), (15, 5), (45, 4), (90, 2), (300, 1)]
    EDIT_PENALTY = [(5, 6), (10, 5), (15, 4), (45, 2), (90, 1)]

    def __init__(self, tier: str):
        self.max_counter, self.decay = TRADING_TIERS[tier]
        self.counter = 0.0
        self._last = time.monotonic()
        self._lock = threading.Lock()

    def _apply_decay(self) -> None:
        now = time.monotonic()
        self.counter = max(0.0, self.counter - (now - self._last) * self.decay)
        self._last = now

    @staticmethod
    def _penalty(age_sec: float, table) -> float:
        for max_age, penalty in table:
            if age_sec < max_age:
                return float(penalty)
        return 0.0

    def acquire(self, cost: float) -> None:
        limit = self.max_counter * 0.9  # marge de 10 %
        while True:
            with self._lock:
                self._apply_decay()
                if self.counter + cost <= limit:
                    self.counter += cost
                    return
                wait = (self.counter + cost - limit) / self.decay
            log.warning("Compteur de trading proche du seuil, attente %.1fs", wait)
            time.sleep(max(wait, 0.1))

    def acquire_add(self) -> None:
        self.acquire(1.0)

    def acquire_cancel(self, order_age_sec: float) -> None:
        self.acquire(self._penalty(order_age_sec, self.CANCEL_PENALTY))

    def acquire_edit(self, order_age_sec: float) -> None:
        self.acquire(self._penalty(order_age_sec, self.EDIT_PENALTY))


# ---------------------------------------------------------------------------
# Horloge & nonce
# ---------------------------------------------------------------------------
class ClockSync:
    """Mesure le décalage entre l'horloge locale et celle de Kraken."""

    def __init__(self):
        self.offset_ms = 0.0
        self.last_sync = 0.0

    def sync(self, exchange: ccxt.Exchange) -> None:
        t0 = time.time() * 1000
        server_ms = exchange.fetch_time()  # endpoint public /0/public/Time
        t1 = time.time() * 1000
        self.offset_ms = server_ms - (t0 + t1) / 2
        self.last_sync = time.time()
        level = logging.WARNING if abs(self.offset_ms) > 2000 else logging.INFO
        log.log(level, "Horloge synchronisée avec Kraken : décalage %.0f ms (latence %.0f ms)",
                self.offset_ms, t1 - t0)
        if abs(self.offset_ms) > 2000:
            log.warning("Décalage > 2 s : activez NTP (sudo timedatectl set-ntp true)")

    def now_ms(self) -> int:
        return int(time.time() * 1000 + self.offset_ms)


class SyncedKraken(ccxt.kraken):
    """ccxt.kraken dont le nonce suit l'horloge serveur et reste strictement croissant
    même en cas d'appels concurrents (verrou interne)."""

    def __init__(self, config: dict, clock: ClockSync):
        super().__init__(config)
        self._clock = clock
        self._nonce_lock = threading.Lock()
        self._last_nonce = 0

    def nonce(self):
        with self._nonce_lock:
            candidate = self._clock.now_ms()
            self._last_nonce = candidate if candidate > self._last_nonce else self._last_nonce + 1
            return self._last_nonce


# ---------------------------------------------------------------------------
# Retries génériques
# ---------------------------------------------------------------------------
def call_with_retries(fn: Callable, *args, max_retries: int = 5, label: str = "",
                      on_invalid_nonce: Optional[Callable[[], None]] = None,
                      on_rate_limit: Optional[Callable[[], None]] = None, **kwargs):
    """Exécute `fn` avec backoff exponentiel sur les erreurs réseau transitoires.

    Les erreurs « métier » (fonds insuffisants, ordre invalide, auth) ne sont pas
    réessayées : elles remontent immédiatement.
    """
    delay = 1.0
    for attempt in range(1, max_retries + 1):
        try:
            return fn(*args, **kwargs)
        except ccxt.InvalidNonce as exc:
            log.warning("[%s] nonce invalide (%s), resynchronisation", label, exc)
            if on_invalid_nonce:
                on_invalid_nonce()
        except (ccxt.DDoSProtection, ccxt.RateLimitExceeded) as exc:
            log.warning("[%s] rate limit Kraken (%s), pause %.0fs", label, exc, delay * 5)
            if on_rate_limit:
                on_rate_limit()
            time.sleep(delay * 5)
        except (ccxt.RequestTimeout, ccxt.ExchangeNotAvailable, ccxt.NetworkError) as exc:
            log.warning("[%s] erreur réseau tentative %d/%d : %s", label, attempt, max_retries, exc)
            time.sleep(delay)
        if attempt == max_retries:
            break
        delay = min(delay * 2, 30.0)
    raise ccxt.NetworkError(f"[{label}] échec après {max_retries} tentatives")


# ---------------------------------------------------------------------------
# WebSocket ticker (Kraken WS v2)
# ---------------------------------------------------------------------------
class KrakenTickerStream(threading.Thread):
    """Thread de fond qui maintient un cache bid/ask/last via le WebSocket public v2."""

    URL = "wss://ws.kraken.com/v2"

    def __init__(self, symbols: List[str]):
        super().__init__(name="kraken-ws", daemon=True)
        self.symbols = symbols
        self.cache: Dict[str, dict] = {}
        self._stop_event = threading.Event()
        self._lock = threading.Lock()

    def stop(self) -> None:
        self._stop_event.set()

    def get(self, symbol: str, max_age_sec: float) -> Optional[dict]:
        with self._lock:
            item = self.cache.get(symbol)
        if item and time.time() - item["ts"] <= max_age_sec:
            return item
        return None

    def run(self) -> None:
        try:
            asyncio.run(self._main())
        except Exception as exc:  # pragma: no cover - sécurité du thread
            log.error("Thread WebSocket arrêté : %s", exc)

    async def _main(self) -> None:
        import websockets  # import local : dépendance utile seulement en live

        backoff = 1.0
        while not self._stop_event.is_set():
            try:
                async with websockets.connect(self.URL, ping_interval=20, ping_timeout=20,
                                              close_timeout=5) as ws:
                    await ws.send(json.dumps({
                        "method": "subscribe",
                        "params": {"channel": "ticker", "symbol": self.symbols},
                    }))
                    log.info("WebSocket Kraken connecté (%s)", ", ".join(self.symbols))
                    backoff = 1.0
                    while not self._stop_event.is_set():
                        # Kraken envoie un heartbeat ~1/s : 30 s sans message = connexion morte
                        raw = await asyncio.wait_for(ws.recv(), timeout=30)
                        self._handle(json.loads(raw))
            except Exception as exc:
                if self._stop_event.is_set():
                    break
                log.warning("WebSocket déconnecté (%s), reconnexion dans %.0fs", exc, backoff)
                await asyncio.sleep(backoff)
                backoff = min(backoff * 2, 60.0)

    def _handle(self, msg: dict) -> None:
        if msg.get("channel") != "ticker" or msg.get("type") not in ("snapshot", "update"):
            if msg.get("method") == "subscribe" and not msg.get("success", True):
                log.error("Souscription WebSocket refusée : %s", msg.get("error"))
            return
        now = time.time()
        with self._lock:
            for item in msg.get("data", []):
                symbol = item.get("symbol")
                if not symbol:
                    continue
                self.cache[symbol] = {
                    "bid": float(item["bid"]), "ask": float(item["ask"]),
                    "last": float(item["last"]), "ts": now,
                }


# ---------------------------------------------------------------------------
# Feed principal
# ---------------------------------------------------------------------------
class KrakenFeed:
    def __init__(self, cfg: BotConfig):
        self.cfg = cfg
        self.clock = ClockSync()
        self.exchange = SyncedKraken({
            "apiKey": cfg.api_key,
            "secret": cfg.secret_key,
            "enableRateLimit": True,   # throttling ccxt des endpoints publics
            "timeout": 20_000,
        }, self.clock)
        self.rest_counter = KrakenCallCounter(cfg.api_tier)
        self._trading_counters: Dict[str, KrakenTradingCounter] = {}
        self.ws: Optional[KrakenTickerStream] = None
        self.symbols: List[str] = []

    # --- Initialisation ------------------------------------------------------
    def connect(self) -> List[str]:
        """Synchronise l'horloge, charge les marchés et résout les symboles configurés."""
        call_with_retries(self.clock.sync, self.exchange, label="time",
                          max_retries=self.cfg.max_retries)
        call_with_retries(self.exchange.load_markets, label="markets",
                          max_retries=self.cfg.max_retries)
        self.symbols = [self.resolve_symbol(s) for s in self.cfg.symbols]
        for s in self.symbols:
            m = self.exchange.market(s)
            log.info("Marché %s -> id Kraken %s (altname %s, ordermin %s, costmin %s)",
                     s, m["id"], m["info"].get("altname"), m["limits"]["amount"]["min"],
                     m["limits"]["cost"]["min"])
        return self.symbols

    def resolve_symbol(self, raw: str) -> str:
        """Accepte BTC/USD, XBT/USD, XXBTZUSD, XBTUSD, btcusd... et retourne le symbole unifié ccxt."""
        markets = self.exchange.markets
        candidate = raw.strip().upper()
        if candidate in markets:
            return candidate
        if "/" in candidate:
            base, quote = candidate.split("/", 1)
            alias = {"XBT": "BTC", "XDG": "DOGE"}
            unified = f"{alias.get(base, base)}/{alias.get(quote, quote)}"
            if unified in markets:
                return unified
        by_id = self.exchange.markets_by_id or {}
        if candidate in by_id:
            found = by_id[candidate]
            return (found[0] if isinstance(found, list) else found)["symbol"]
        compact = candidate.replace("/", "")
        for symbol, market in markets.items():
            info = market.get("info", {})
            if compact in (str(info.get("altname", "")).upper(), market["id"].upper()) or \
                    candidate == str(info.get("wsname", "")).upper():
                return symbol
        raise ValueError(f"Symbole inconnu chez Kraken : {raw}")

    def start_ws(self) -> None:
        if self.ws is None and self.symbols:
            self.ws = KrakenTickerStream(self.symbols)
            self.ws.start()

    def stop_ws(self) -> None:
        if self.ws:
            self.ws.stop()

    def trading_counter(self, symbol: str) -> KrakenTradingCounter:
        if symbol not in self._trading_counters:
            self._trading_counters[symbol] = KrakenTradingCounter(self.cfg.api_tier)
        return self._trading_counters[symbol]

    # --- Appels génériques -------------------------------------------------
    def resync_clock(self) -> None:
        try:
            self.clock.sync(self.exchange)
        except Exception as exc:
            log.error("Resynchronisation de l'horloge impossible : %s", exc)

    def public(self, method: str, *args, **kwargs):
        fn = getattr(self.exchange, method)
        return call_with_retries(fn, *args, max_retries=self.cfg.max_retries, label=method, **kwargs)

    def private(self, method: str, *args, retries: Optional[int] = None, **kwargs):
        """Appel privé : passe par le call counter puis par les retries."""
        fn = getattr(self.exchange, method)
        cost = REST_COSTS.get(method, 1)

        def guarded(*a, **kw):
            self.rest_counter.acquire(cost)
            return fn(*a, **kw)

        return call_with_retries(guarded, *args, label=method,
                                 max_retries=self.cfg.max_retries if retries is None else retries,
                                 on_invalid_nonce=self.resync_clock,
                                 on_rate_limit=self.rest_counter.penalize, **kwargs)

    # --- Données de marché ---------------------------------------------------
    def server_time_ms(self) -> int:
        return self.clock.now_ms()

    def fetch_ohlcv_df(self, symbol: str, timeframe: Optional[str] = None,
                       limit: Optional[int] = None) -> pd.DataFrame:
        """Bougies **clôturées** uniquement (la bougie en cours est retirée)."""
        timeframe = timeframe or self.cfg.timeframe
        limit = limit or self.cfg.ohlcv_limit
        raw = self.public("fetch_ohlcv", symbol, timeframe, None, min(limit + 1, 720))
        tf_ms = TIMEFRAME_MS[timeframe]
        now_ms = self.server_time_ms()
        closed = [row for row in raw if row[0] + tf_ms <= now_ms]
        return ohlcv_to_df(closed).tail(limit)

    def ticker(self, symbol: str) -> dict:
        """bid/ask/last : WebSocket si frais, sinon REST."""
        if self.ws:
            cached = self.ws.get(symbol, self.cfg.ws_stale_sec)
            if cached:
                return cached
        t = self.public("fetch_ticker", symbol)
        last = t.get("last") or t.get("close")
        return {"bid": float(t["bid"] or last), "ask": float(t["ask"] or last),
                "last": float(last), "ts": time.time()}

    def last_price(self, symbol: str) -> float:
        return self.ticker(symbol)["last"]

    # --- Précision / limites de marché --------------------------------------
    def amount_to_precision(self, symbol: str, amount: float) -> float:
        if amount <= 0:
            return 0.0
        return float(self.exchange.amount_to_precision(symbol, amount))  # arrondi vers le bas

    def price_to_precision(self, symbol: str, price: float) -> float:
        return float(self.exchange.price_to_precision(symbol, price))

    def tick_size(self, symbol: str) -> float:
        precision = self.exchange.market(symbol)["precision"]["price"]
        # ccxt >= 4 : mode TICK_SIZE, la précision est directement le pas de prix
        return float(precision) if precision < 1 else 10 ** (-int(precision))

    def min_amount(self, symbol: str) -> float:
        return float(self.exchange.market(symbol)["limits"]["amount"]["min"] or 0.0)

    def min_cost(self, symbol: str) -> float:
        return float(self.exchange.market(symbol)["limits"]["cost"]["min"] or 0.0)

    def base_currency(self, symbol: str) -> str:
        return self.exchange.market(symbol)["base"]


def ohlcv_to_df(raw: list) -> pd.DataFrame:
    df = pd.DataFrame(raw, columns=["timestamp", "open", "high", "low", "close", "volume"])
    df["timestamp"] = pd.to_datetime(df["timestamp"], unit="ms", utc=True)
    df = df.set_index("timestamp").astype(float)
    return df[~df.index.duplicated(keep="last")].sort_index()
