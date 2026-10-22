"""
check_setup.py
==============
Vérifie l'intégration Kraken **avant** de lancer le bot en live. N'envoie AUCUN ordre réel.

Contrôles effectués :
  1. configuration (.env) valide ;
  2. API publique Kraken joignable, statut du système, décalage d'horloge ;
  3. symboles reconnus chez Kraken (BTC/EUR -> XXBTZEUR, etc.) ;
  4. clé API : authentification, soldes, lecture des ordres, frais réels du compte,
     droit de trading testé via AddOrder en mode `validate` (Kraken vérifie l'ordre
     sans jamais l'envoyer au carnet) ;
  5. faisabilité des trades avec ton capital : taille de position, minimums Kraken, frais ;
  6. flux WebSocket temps réel.

Usage :
  python check_setup.py            # tous les contrôles
  python check_setup.py --skip-ws  # sans le test WebSocket
Code de sortie : 0 si aucun échec bloquant, 1 sinon.
"""
from __future__ import annotations

import argparse
import logging
import sys
import time

import ccxt

from kraken_config import BotConfig, fee_for_volume, load_config
from kraken_feed import KrakenFeed, KrakenTickerStream
from risk_engine import RiskEngine
from strategy import StrategyParams, compute_indicators


class Report:
    def __init__(self):
        self.fails = 0
        self.warns = 0

    @staticmethod
    def section(title: str) -> None:
        print(f"\n== {title} ==")

    @staticmethod
    def ok(msg: str) -> None:
        print(f"  [OK]     {msg}")

    @staticmethod
    def info(msg: str) -> None:
        print(f"           {msg}")

    def warn(self, msg: str) -> None:
        self.warns += 1
        print(f"  [ALERTE] {msg}")

    def fail(self, msg: str) -> None:
        self.fails += 1
        print(f"  [ÉCHEC]  {msg}")


def trade_feasibility(feed: KrakenFeed, cfg: BotConfig, symbol: str, capital: float,
                      maker: float, taker: float) -> dict:
    """Simule le dimensionnement d'une entrée au prix actuel avec le capital donné."""
    params = StrategyParams.from_config(cfg)
    ind = compute_indicators(feed.fetch_ohlcv_df(symbol), params)
    last = ind.iloc[-1]
    price, atr_value = float(last["close"]), float(last["atr"])
    stop = price - cfg.initial_stop_atr * atr_value
    risk = RiskEngine(cfg, state_path=None)
    risk.initial_capital = capital
    raw, _ = risk.size_position(capital, capital, price, stop, taker if cfg.taker_fallback else maker)
    amount = feed.amount_to_precision(symbol, raw)
    notional = amount * price
    min_amount, min_cost = feed.min_amount(symbol), feed.min_cost(symbol)
    loss_at_stop = amount * (price - stop) + notional * (maker + taker)
    return {
        "price": price,
        "atr_pct": atr_value / price,
        "stop_pct": (price - stop) / price,
        "amount": amount,
        "notional": notional,
        "min_amount": min_amount,
        "min_notional": max(min_amount * price, min_cost),
        "feasible": amount >= min_amount and notional >= min_cost and amount > 0,
        "capped": raw * price >= capital * cfg.max_position_pct * 0.99,
        "loss_at_stop": loss_at_stop,
        "loss_pct": loss_at_stop / capital if capital > 0 else 0.0,
        "round_trip_fees": notional * (maker + taker),
    }


def check_private(feed: KrakenFeed, cfg: BotConfig, r: Report, symbols) -> dict:
    """Retourne {'quote_total': float, 'fees': {symbol: (maker, taker)}}."""
    out = {"quote_total": 0.0, "fees": {}}
    quote = cfg.quote_currency
    try:
        bal = feed.private("fetch_balance", retries=2)
    except ccxt.AuthenticationError as exc:
        r.fail(f"Clé API refusée par Kraken ({exc}). Vérifie API_KEY / SECRET_KEY et la restriction d'IP.")
        return out
    except ccxt.PermissionDenied as exc:
        r.fail(f"Permission 'Query Funds' manquante ({exc})")
        return out
    totals = {k: v for k, v in (bal.get("total") or {}).items() if v}
    r.ok(f"Authentification réussie. Soldes : {totals or 'aucun'}")
    out["quote_total"] = float((bal.get("total") or {}).get(quote) or 0.0)
    free_quote = float((bal.get("free") or {}).get(quote) or 0.0)
    if out["quote_total"] <= 0:
        r.warn(f"Aucun {quote} disponible sur le compte : dépose des fonds (virement SEPA) avant le live")
    else:
        r.ok(f"{quote} : {out['quote_total']:.2f} au total, {free_quote:.2f} disponibles pour trader")
    variants = [k for k in totals if k.upper().startswith(quote + ".")]
    if variants:
        r.warn(f"Fonds aussi présents sous {variants} : vérifie dans Kraken qu'ils sont disponibles "
               f"au trading (programme de récompenses / épargne), sinon le bot ne les verra pas")

    for method, perm in (("fetch_open_orders", "Query Open Orders & Trades"),
                         ("fetch_closed_orders", "Query Closed Orders & Trades")):
        try:
            orders = feed.private(method, None, None, 5, retries=2)
            r.ok(f"Lecture des ordres ({method}) : {len(orders)} trouvé(s)")
        except ccxt.PermissionDenied as exc:
            r.fail(f"Permission '{perm}' manquante ({exc})")

    for s in symbols:
        try:
            fee = feed.private("fetch_trading_fee", s, retries=2)
            maker, taker = float(fee["maker"]), float(fee["taker"])
            out["fees"][s] = (maker, taker)
            r.ok(f"Frais réels {s} : maker {maker:.2%} / taker {taker:.2%}")
        except Exception as exc:
            r.warn(f"Frais réels {s} illisibles ({exc}), la grille publique sera utilisée")

    # Droit de trading : AddOrder avec validate=true -> Kraken contrôle sans exécuter
    s = symbols[0]
    market = feed.exchange.market(s)
    t = feed.ticker(s)
    request = {
        "pair": market["id"], "type": "buy", "ordertype": "limit",
        "price": feed.exchange.price_to_precision(s, t["bid"] * 0.5),
        "volume": feed.exchange.amount_to_precision(s, max(feed.min_amount(s), 1e-8)),
        "oflags": "post,fciq", "validate": True,
    }
    try:
        feed.private("privatePostAddOrder", request, retries=2)
        r.ok(f"Droit de trading confirmé (ordre de test validé par Kraken sur {s}, non envoyé)")
    except ccxt.InsufficientFunds:
        r.ok("Droit de trading confirmé (Kraken signale seulement des fonds insuffisants)")
    except ccxt.PermissionDenied as exc:
        r.fail(f"Permission 'Create & Modify Orders' manquante ({exc})")
    except Exception as exc:
        r.warn(f"Test du droit de trading non concluant : {exc}")
    r.info("À vérifier toi-même dans Kraken : la clé ne doit PAS avoir 'Withdraw Funds'.")
    return out


def check_ws(symbols, r: Report, wait_sec: float = 15.0) -> None:
    stream = KrakenTickerStream(symbols)
    stream.start()
    deadline = time.time() + wait_sec
    while time.time() < deadline and any(stream.get(s, 60) is None for s in symbols):
        time.sleep(0.5)
    stream.stop()
    missing = [s for s in symbols if stream.get(s, 60) is None]
    if missing:
        r.warn(f"Pas de prix WebSocket reçu pour {missing} en {wait_sec:.0f}s "
               f"(le bot basculera sur REST, mais vérifie ton pare-feu)")
    else:
        prices = ", ".join(f"{s} {stream.get(s, 60)['last']:.2f}" for s in symbols)
        r.ok(f"WebSocket temps réel opérationnel : {prices}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Vérification de l'intégration Kraken (aucun ordre envoyé)")
    parser.add_argument("--skip-ws", action="store_true", help="ne pas tester le WebSocket")
    args = parser.parse_args()
    logging.basicConfig(level=logging.ERROR)
    r = Report()

    r.section("Configuration")
    try:
        cfg = load_config()
    except ValueError as exc:
        print(exc)
        return 1
    has_keys = bool(cfg.api_key and cfg.secret_key)
    r.ok(f"Mode : {'SIMULATION (DRY_RUN=true)' if cfg.dry_run else 'LIVE - ordres réels'} | "
         f"{', '.join(cfg.symbols)} | {cfg.timeframe} | risque {cfg.risk_per_trade:.1%} | "
         f"{cfg.max_open_positions} position(s) max")
    if not has_keys:
        r.warn("Pas de clé API dans .env : les contrôles privés sont ignorés")

    r.section("Connexion à Kraken")
    feed = KrakenFeed(cfg)
    try:
        status = feed.exchange.fetch_status()
        if status.get("status") == "ok":
            r.ok("API publique joignable, système Kraken opérationnel")
        else:
            r.warn(f"Statut Kraken : {status.get('status')} ({status.get('info')})")
        symbols = feed.connect()
    except ValueError as exc:
        r.fail(str(exc))
        return finish(r)
    except Exception as exc:
        r.fail(f"Kraken injoignable : {exc}")
        return finish(r)
    offset = feed.clock.offset_ms
    (r.ok if abs(offset) < 1000 else r.warn)(f"Décalage d'horloge avec Kraken : {offset:.0f} ms")
    for s in symbols:
        m = feed.exchange.market(s)
        r.ok(f"{s} reconnu (id Kraken {m['id']}, minimum {m['limits']['amount']['min']} {m['base']})")

    private = {"quote_total": 0.0, "fees": {}}
    if has_keys:
        r.section("Clé API (lecture seule + ordre de validation)")
        private = check_private(feed, cfg, r, symbols)

    capital = private["quote_total"] if private["quote_total"] > 0 else cfg.sim_starting_balance
    source = "solde Kraken" if private["quote_total"] > 0 else "SIM_STARTING_BALANCE"
    r.section(f"Faisabilité avec {capital:.2f} {cfg.quote_currency} ({source})")
    feasible_any = False
    for s in symbols:
        maker, taker = private["fees"].get(s, fee_for_volume(cfg.fee_volume_30d))
        f = trade_feasibility(feed, cfg, s, capital, maker, taker)
        line = (f"{s} : prix {f['price']:.2f}, ATR {f['atr_pct']:.2%}, stop -{f['stop_pct']:.2%} -> "
                f"position {f['notional']:.2f} {cfg.quote_currency}, perte au stop "
                f"{f['loss_at_stop']:.2f} ({f['loss_pct']:.1%}), frais aller-retour {f['round_trip_fees']:.2f}")
        if f["feasible"]:
            feasible_any = True
            r.ok(line)
            if f["capped"]:
                r.info(f"Position plafonnée par MAX_POSITION_PCT : le risque réel ({f['loss_pct']:.1%}) "
                       f"est inférieur à RISK_PER_TRADE")
        else:
            r.fail(f"{line} -> sous le minimum Kraken ({f['min_notional']:.2f} {cfg.quote_currency})")
    if not feasible_any:
        r.fail("Aucune paire tradable avec ce capital : augmente le capital ou choisis d'autres paires")
    if capital < 300 and cfg.max_open_positions > 1:
        r.warn("Petit capital : mets MAX_OPEN_POSITIONS=1, une seule position mobilise déjà presque tout le capital")
    if capital < 1000 and cfg.timeframe in ("5m", "15m"):
        r.warn("En 5m/15m, les frais pèsent lourd sur chaque trade : TIMEFRAME=1h est recommandé")

    if not args.skip_ws:
        r.section("Flux temps réel")
        check_ws(symbols, r)
    return finish(r)


def finish(r: Report) -> int:
    print(f"\nRésultat : {r.fails} échec(s), {r.warns} alerte(s).")
    if r.fails:
        print("Corrige les échecs avant de lancer le bot en live.")
    else:
        print("Intégration Kraken prête." if r.warns == 0 else "Intégration fonctionnelle, lis les alertes ci-dessus.")
    return 1 if r.fails else 0


if __name__ == "__main__":
    sys.exit(main())
