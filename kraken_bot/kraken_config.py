"""
kraken_config.py
================
Configuration centralisée du bot : clés API (lues depuis `.env`), paramètres de
stratégie, de risque et d'exécution, paliers de frais Kraken.

Toutes les valeurs peuvent être surchargées par variable d'environnement
(voir `.env.example`). Le mode simulation (`DRY_RUN=true`) est actif par défaut :
aucun ordre réel n'est envoyé tant que vous ne l'avez pas explicitement désactivé.
"""
from __future__ import annotations

import logging
import os
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import List, Tuple

from dotenv import load_dotenv

BASE_DIR = Path(__file__).resolve().parent
load_dotenv(BASE_DIR / ".env")

# ---------------------------------------------------------------------------
# Paliers de frais Kraken Spot (volume 30 jours en USD -> maker, taker).
# Source : grille publique "Kraken Pro" spot (paires crypto/fiat).
# Vérifiez https://www.kraken.com/features/fee-schedule : Kraken peut les modifier.
# ---------------------------------------------------------------------------
KRAKEN_SPOT_FEE_TIERS: List[Tuple[float, float, float]] = [
    (0, 0.0025, 0.0040),
    (10_000, 0.0020, 0.0035),
    (50_000, 0.0014, 0.0024),
    (100_000, 0.0012, 0.0022),
    (250_000, 0.0010, 0.0020),
    (500_000, 0.0008, 0.0018),
    (1_000_000, 0.0006, 0.0016),
    (2_500_000, 0.0004, 0.0014),
    (5_000_000, 0.0002, 0.0012),
    (10_000_000, 0.0000, 0.0010),
]


def fee_for_volume(volume_30d_usd: float) -> Tuple[float, float]:
    """Retourne (maker, taker) applicables pour un volume 30 jours donné."""
    maker, taker = KRAKEN_SPOT_FEE_TIERS[0][1], KRAKEN_SPOT_FEE_TIERS[0][2]
    for threshold, m, t in KRAKEN_SPOT_FEE_TIERS:
        if volume_30d_usd >= threshold:
            maker, taker = m, t
        else:
            break
    return maker, taker


# ---------------------------------------------------------------------------
# Helpers de lecture des variables d'environnement
# ---------------------------------------------------------------------------
def _env_str(name: str, default: str) -> str:
    value = os.getenv(name)
    return default if value is None or value.strip() == "" else value.strip()


def _env_bool(name: str, default: bool) -> bool:
    value = os.getenv(name)
    if value is None or value.strip() == "":
        return default
    return value.strip().lower() in ("1", "true", "yes", "y", "on")


def _env_float(name: str, default: float) -> float:
    value = os.getenv(name)
    return default if value is None or value.strip() == "" else float(value)


def _env_int(name: str, default: int) -> int:
    value = os.getenv(name)
    return default if value is None or value.strip() == "" else int(value)


def _env_list(name: str, default: List[str]) -> List[str]:
    value = os.getenv(name)
    if value is None or value.strip() == "":
        return list(default)
    return [item.strip() for item in value.split(",") if item.strip()]


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
@dataclass
class BotConfig:
    # --- Accès API / modes -------------------------------------------------
    api_key: str = ""
    secret_key: str = ""
    dry_run: bool = True                    # simulation par défaut : aucun ordre réel
    api_tier: str = "starter"               # starter | intermediate | pro (call counter Kraken)

    # --- Marchés -----------------------------------------------------------
    symbols: List[str] = field(default_factory=lambda: ["BTC/USD", "ETH/USD", "SOL/USD"])
    quote_currency: str = "USD"
    timeframe: str = "15m"                  # 15m ou 1h recommandés
    ohlcv_limit: int = 300                  # bougies chargées à chaque clôture

    # --- Stratégie : Bollinger + Volume + ADX ------------------------------
    bb_period: int = 20
    bb_std: float = 2.0
    volume_sma_period: int = 20
    volume_multiplier: float = 1.8          # volume > 1.8 x moyenne = "explosion"
    adx_period: int = 14
    adx_threshold: float = 25.0
    atr_period: int = 14
    require_fresh_breakout: bool = True     # la bougie précédente était sous la bande
    require_di_confirmation: bool = True    # +DI > -DI
    require_bandwidth_expansion: bool = True  # largeur des bandes en hausse
    exit_on_mid_band: bool = False          # sortie si clôture sous la moyenne mobile

    # --- Gestion des stops -------------------------------------------------
    initial_stop_atr: float = 2.0           # stop initial = entrée - 2 ATR
    trailing_stop_atr: float = 3.0          # chandelier : plus haut - 3 ATR
    use_exchange_stop: bool = True          # stop-loss "catastrophe" posé chez Kraken
    stop_update_min_atr: float = 0.25       # ne déplace le stop exchange que si +0.25 ATR

    # --- Risque ------------------------------------------------------------
    sizing_mode: str = "fixed"              # fixed | kelly
    risk_per_trade: float = 0.02            # 2 % du capital risqué par trade
    fixed_stake: float = 0.0                # >0 : mise fixe par trade en devise de cotation (ex. 5 €)
    max_risk_per_trade: float = 0.03        # plafond absolu 3 %
    kelly_fraction: float = 0.5             # demi-Kelly
    kelly_lookback: int = 50
    kelly_min_trades: int = 20
    kelly_floor: float = 0.005              # risque minimal si Kelly <= 0
    max_portfolio_risk: float = 0.06        # risque ouvert cumulé max (somme des stops)
    max_open_positions: int = 3
    max_position_pct: float = 0.95          # notionnel max par position (pas de levier)
    compound: bool = True                   # réinvestissement des gains
    compound_ratio: float = 0.5             # part des gains réalisés réinvestie
    max_weekly_drawdown: float = 0.10       # coupure d'urgence à -10 % sur 7 jours glissants
    flatten_on_halt: bool = True            # liquider les positions lors du hard stop

    # --- Pyramiding --------------------------------------------------------
    pyramid_max_adds: int = 0               # 0 = désactivé
    pyramid_step_atr: float = 1.5           # ajout tous les +1.5 ATR depuis le dernier ajout
    pyramid_risk_scale: float = 0.5         # chaque ajout risque 50 % du risque initial

    # --- Exécution ---------------------------------------------------------
    post_only: bool = True
    order_timeout_sec: int = 45             # durée de vie d'un ordre limite avant repricing
    max_reprice: int = 3
    entry_chase_atr: float = 0.5            # on abandonne si le prix a fui de +0.5 ATR
    taker_fallback: bool = True             # market si le post-only n'est pas exécuté
    exit_post_only_first: bool = False      # sorties sur stop : market direct (sécurité)
    poll_interval_sec: int = 10
    ws_stale_sec: int = 15                  # au-delà, on repasse sur REST
    max_retries: int = 5

    # --- Simulation / backtest ---------------------------------------------
    sim_starting_balance: float = 10_000.0
    fee_volume_30d: float = 0.0             # volume 30j initial pour le palier de frais
    slippage_bps: float = 5.0               # glissement appliqué aux ordres market (backtest)

    # --- Divers ------------------------------------------------------------
    state_file: str = str(BASE_DIR / "bot_state.json")
    log_level: str = "INFO"
    log_file: str = str(BASE_DIR / "bot.log")

    # -----------------------------------------------------------------------
    @classmethod
    def from_env(cls) -> "BotConfig":
        d = cls()
        return cls(
            api_key=_env_str("API_KEY", d.api_key),
            secret_key=_env_str("SECRET_KEY", d.secret_key),
            dry_run=_env_bool("DRY_RUN", d.dry_run),
            api_tier=_env_str("API_TIER", d.api_tier).lower(),
            symbols=_env_list("SYMBOLS", d.symbols),
            quote_currency=_env_str("QUOTE_CURRENCY", d.quote_currency),
            timeframe=_env_str("TIMEFRAME", d.timeframe),
            ohlcv_limit=_env_int("OHLCV_LIMIT", d.ohlcv_limit),
            bb_period=_env_int("BB_PERIOD", d.bb_period),
            bb_std=_env_float("BB_STD", d.bb_std),
            volume_sma_period=_env_int("VOLUME_SMA_PERIOD", d.volume_sma_period),
            volume_multiplier=_env_float("VOLUME_MULTIPLIER", d.volume_multiplier),
            adx_period=_env_int("ADX_PERIOD", d.adx_period),
            adx_threshold=_env_float("ADX_THRESHOLD", d.adx_threshold),
            atr_period=_env_int("ATR_PERIOD", d.atr_period),
            require_fresh_breakout=_env_bool("REQUIRE_FRESH_BREAKOUT", d.require_fresh_breakout),
            require_di_confirmation=_env_bool("REQUIRE_DI_CONFIRMATION", d.require_di_confirmation),
            require_bandwidth_expansion=_env_bool("REQUIRE_BANDWIDTH_EXPANSION", d.require_bandwidth_expansion),
            exit_on_mid_band=_env_bool("EXIT_ON_MID_BAND", d.exit_on_mid_band),
            initial_stop_atr=_env_float("INITIAL_STOP_ATR", d.initial_stop_atr),
            trailing_stop_atr=_env_float("TRAILING_STOP_ATR", d.trailing_stop_atr),
            use_exchange_stop=_env_bool("USE_EXCHANGE_STOP", d.use_exchange_stop),
            stop_update_min_atr=_env_float("STOP_UPDATE_MIN_ATR", d.stop_update_min_atr),
            sizing_mode=_env_str("SIZING_MODE", d.sizing_mode).lower(),
            risk_per_trade=_env_float("RISK_PER_TRADE", d.risk_per_trade),
            fixed_stake=_env_float("FIXED_STAKE", d.fixed_stake),
            max_risk_per_trade=_env_float("MAX_RISK_PER_TRADE", d.max_risk_per_trade),
            kelly_fraction=_env_float("KELLY_FRACTION", d.kelly_fraction),
            kelly_lookback=_env_int("KELLY_LOOKBACK", d.kelly_lookback),
            kelly_min_trades=_env_int("KELLY_MIN_TRADES", d.kelly_min_trades),
            kelly_floor=_env_float("KELLY_FLOOR", d.kelly_floor),
            max_portfolio_risk=_env_float("MAX_PORTFOLIO_RISK", d.max_portfolio_risk),
            max_open_positions=_env_int("MAX_OPEN_POSITIONS", d.max_open_positions),
            max_position_pct=_env_float("MAX_POSITION_PCT", d.max_position_pct),
            compound=_env_bool("COMPOUND", d.compound),
            compound_ratio=_env_float("COMPOUND_RATIO", d.compound_ratio),
            max_weekly_drawdown=_env_float("MAX_WEEKLY_DRAWDOWN", d.max_weekly_drawdown),
            flatten_on_halt=_env_bool("FLATTEN_ON_HALT", d.flatten_on_halt),
            pyramid_max_adds=_env_int("PYRAMID_MAX_ADDS", d.pyramid_max_adds),
            pyramid_step_atr=_env_float("PYRAMID_STEP_ATR", d.pyramid_step_atr),
            pyramid_risk_scale=_env_float("PYRAMID_RISK_SCALE", d.pyramid_risk_scale),
            post_only=_env_bool("POST_ONLY", d.post_only),
            order_timeout_sec=_env_int("ORDER_TIMEOUT_SEC", d.order_timeout_sec),
            max_reprice=_env_int("MAX_REPRICE", d.max_reprice),
            entry_chase_atr=_env_float("ENTRY_CHASE_ATR", d.entry_chase_atr),
            taker_fallback=_env_bool("TAKER_FALLBACK", d.taker_fallback),
            exit_post_only_first=_env_bool("EXIT_POST_ONLY_FIRST", d.exit_post_only_first),
            poll_interval_sec=_env_int("POLL_INTERVAL_SEC", d.poll_interval_sec),
            ws_stale_sec=_env_int("WS_STALE_SEC", d.ws_stale_sec),
            max_retries=_env_int("MAX_RETRIES", d.max_retries),
            sim_starting_balance=_env_float("SIM_STARTING_BALANCE", d.sim_starting_balance),
            fee_volume_30d=_env_float("FEE_VOLUME_30D", d.fee_volume_30d),
            slippage_bps=_env_float("SLIPPAGE_BPS", d.slippage_bps),
            state_file=_env_str("STATE_FILE", d.state_file),
            log_level=_env_str("LOG_LEVEL", d.log_level).upper(),
            log_file=_env_str("LOG_FILE", d.log_file),
        )

    def validate(self) -> None:
        errors = []
        if not self.dry_run and (not self.api_key or not self.secret_key):
            errors.append("DRY_RUN=false exige API_KEY et SECRET_KEY dans .env")
        if self.api_tier not in ("starter", "intermediate", "pro"):
            errors.append("API_TIER doit valoir starter, intermediate ou pro")
        if self.timeframe not in ("5m", "15m", "30m", "1h", "4h"):
            errors.append("TIMEFRAME supporté : 5m, 15m, 30m, 1h, 4h")
        if not 0 < self.risk_per_trade <= self.max_risk_per_trade:
            errors.append("RISK_PER_TRADE doit être > 0 et <= MAX_RISK_PER_TRADE")
        if self.max_risk_per_trade > 0.03:
            errors.append("MAX_RISK_PER_TRADE est plafonné à 3 % (garde-fou anti-ruine)")
        if self.fixed_stake < 0:
            errors.append("FIXED_STAKE doit être >= 0 (0 = taille calculée sur le risque)")
        if self.sizing_mode not in ("fixed", "kelly"):
            errors.append("SIZING_MODE doit valoir fixed ou kelly")
        if not 0 < self.kelly_fraction <= 1:
            errors.append("KELLY_FRACTION doit être dans ]0, 1]")
        if not 0 < self.max_weekly_drawdown < 1:
            errors.append("MAX_WEEKLY_DRAWDOWN doit être dans ]0, 1[")
        if not 0 <= self.compound_ratio <= 1:
            errors.append("COMPOUND_RATIO doit être dans [0, 1]")
        if not 0 < self.max_position_pct <= 1:
            errors.append("MAX_POSITION_PCT doit être dans ]0, 1] (pas de levier)")
        if self.trailing_stop_atr <= 0 or self.initial_stop_atr <= 0:
            errors.append("Les multiplicateurs ATR des stops doivent être > 0")
        for s in self.symbols:
            if "/" in s and not s.upper().endswith("/" + self.quote_currency.upper()):
                errors.append(f"{s} : la devise de cotation doit être {self.quote_currency}")
        if errors:
            raise ValueError("Configuration invalide :\n  - " + "\n  - ".join(errors))

    def public_dict(self) -> dict:
        """Configuration sans les secrets (pour les logs)."""
        d = asdict(self)
        d["api_key"] = "***" if self.api_key else ""
        d["secret_key"] = "***" if self.secret_key else ""
        return d


def load_config() -> BotConfig:
    cfg = BotConfig.from_env()
    cfg.validate()
    return cfg


def setup_logging(cfg: BotConfig) -> None:
    handlers: List[logging.Handler] = [logging.StreamHandler()]
    if cfg.log_file:
        handlers.append(logging.FileHandler(cfg.log_file, encoding="utf-8"))
    logging.basicConfig(
        level=getattr(logging, cfg.log_level, logging.INFO),
        format="%(asctime)s | %(levelname)-7s | %(name)-12s | %(message)s",
        handlers=handlers,
        force=True,
    )
