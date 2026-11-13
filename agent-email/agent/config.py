"""Chargement et validation de la configuration.

Deux sources :
- le fichier ``.env`` (secrets et paramètres techniques), lu avec python-dotenv ;
- le dossier ``config/`` (fichiers YAML propres à chaque client).

Chaque fichier YAML est validé par un modèle Pydantic : une faute de frappe
dans la config (prix négatif, mode inconnu, heure invalide…) arrête l'agent au
démarrage avec un message clair, plutôt que de produire une réponse fausse.
"""

from __future__ import annotations

import os
import re
from dataclasses import dataclass
from decimal import Decimal
from pathlib import Path
from typing import Literal
from zoneinfo import ZoneInfo, ZoneInfoNotFoundError

import yaml
from dotenv import load_dotenv
from pydantic import BaseModel, Field, ValidationError, field_validator, model_validator

# Racine du projet (dossier qui contient agent/, config/, .env).
RACINE_PROJET = Path(__file__).resolve().parent.parent

_REGEX_EMAIL = re.compile(r"^[^@\s]+@[^@\s]+\.[^@\s]+$")


class ErreurConfig(Exception):
    """Configuration absente ou invalide."""


# ---------------------------------------------------------------------------
# entreprise.yaml
# ---------------------------------------------------------------------------

class InfosEntreprise(BaseModel):
    nom: str
    activite: str
    adresse: str
    telephone: str
    email: str
    site_web: str | None = None
    siret: str | None = None
    inscription: str | None = None


class Service(BaseModel):
    id: str
    nom: str
    description: str


class EntreeFAQ(BaseModel):
    id: str
    question: str
    reponse: str


class ConfigEntreprise(BaseModel):
    entreprise: InfosEntreprise
    signature: str
    horaires: dict[str, str]
    services: list[Service] = Field(default_factory=list)
    faq: list[EntreeFAQ] = Field(default_factory=list)

    @model_validator(mode="after")
    def _ids_uniques(self) -> "ConfigEntreprise":
        for nom, elements in (("services", self.services), ("faq", self.faq)):
            ids = [e.id for e in elements]
            doublons = {i for i in ids if ids.count(i) > 1}
            if doublons:
                raise ValueError(f"Identifiants en double dans '{nom}' : {sorted(doublons)}")
        return self


# ---------------------------------------------------------------------------
# offres.yaml
# ---------------------------------------------------------------------------

class Produit(BaseModel):
    id: str
    nom: str
    description: str
    prix_ht: Decimal = Field(ge=0)
    unite: str
    delai: str | None = None


class RemiseAutorisee(BaseModel):
    id: str
    libelle: str
    pourcentage: Decimal = Field(gt=0, le=100)
    condition: str


class Remises(BaseModel):
    remise_max_pourcentage: Decimal = Field(ge=0, le=100)
    autorisees: list[RemiseAutorisee] = Field(default_factory=list)

    @model_validator(mode="after")
    def _remises_sous_le_plafond(self) -> "Remises":
        for r in self.autorisees:
            if r.pourcentage > self.remise_max_pourcentage:
                raise ValueError(
                    f"La remise '{r.id}' ({r.pourcentage} %) dépasse le plafond "
                    f"remise_max_pourcentage ({self.remise_max_pourcentage} %)."
                )
        return self


class ConfigOffres(BaseModel):
    devise: str = "EUR"
    tva_pourcentage: Decimal = Field(ge=0, le=100)
    validite_devis_jours: int = Field(gt=0)
    produits: list[Produit]
    remises: Remises
    conditions: dict[str, str] = Field(default_factory=dict)

    @model_validator(mode="after")
    def _ids_uniques(self) -> "ConfigOffres":
        ids = [p.id for p in self.produits] + [r.id for r in self.remises.autorisees]
        doublons = {i for i in ids if ids.count(i) > 1}
        if doublons:
            raise ValueError(f"Identifiants en double dans offres.yaml : {sorted(doublons)}")
        return self

    def produit(self, produit_id: str) -> Produit | None:
        """Retourne le produit portant cet identifiant, ou None."""
        return next((p for p in self.produits if p.id == produit_id), None)

    def remise(self, remise_id: str) -> RemiseAutorisee | None:
        """Retourne la remise autorisée portant cet identifiant, ou None."""
        return next((r for r in self.remises.autorisees if r.id == remise_id), None)


# ---------------------------------------------------------------------------
# reglages.yaml
# ---------------------------------------------------------------------------

Mode = Literal["test", "approbation", "autonome"]


class Responsable(BaseModel):
    nom: str
    email: str

    @field_validator("email")
    @classmethod
    def _email_valide(cls, v: str) -> str:
        if not _REGEX_EMAIL.match(v):
            raise ValueError(f"Adresse email invalide : {v!r}")
        return v.lower()


class Notifications(BaseModel):
    confirmation_envoi: bool = True
    alerte_brouillon: bool = False


class ResumeQuotidien(BaseModel):
    actif: bool = True
    heure: str = "18:00"

    @field_validator("heure")
    @classmethod
    def _heure_valide(cls, v: str) -> str:
        if not re.fullmatch(r"([01]\d|2[0-3]):[0-5]\d", v):
            raise ValueError(f"Heure invalide (format attendu HH:MM) : {v!r}")
        return v


class Reglages(BaseModel):
    mode: Mode = "test"
    seuil_confiance: float = Field(default=0.85, ge=0, le=1)
    intervalle_minutes: int = Field(default=5, ge=1)
    premiere_releve_jours: int = Field(default=1, ge=1)
    fuseau_horaire: str = "Europe/Paris"
    responsable: Responsable
    notifications: Notifications = Field(default_factory=Notifications)
    resume_quotidien: ResumeQuotidien = Field(default_factory=ResumeQuotidien)
    prefixe_libelles: str = "Agent"
    expediteurs_ignores: list[str] = Field(default_factory=list)

    @field_validator("fuseau_horaire")
    @classmethod
    def _fuseau_valide(cls, v: str) -> str:
        try:
            ZoneInfo(v)
        except (ZoneInfoNotFoundError, ValueError) as exc:
            raise ValueError(f"Fuseau horaire inconnu : {v!r}") from exc
        return v

    @field_validator("expediteurs_ignores")
    @classmethod
    def _minuscules(cls, v: list[str]) -> list[str]:
        return [e.strip().lower() for e in v if e.strip()]

    @property
    def zone(self) -> ZoneInfo:
        return ZoneInfo(self.fuseau_horaire)


# ---------------------------------------------------------------------------
# .env
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class Environnement:
    """Paramètres techniques et secrets lus depuis l'environnement / .env."""

    anthropic_api_key: str | None
    model: str
    google_credentials_file: Path
    google_token_file: Path
    db_path: Path
    config_dir: Path
    log_level: str


def _chemin(valeur: str, racine: Path) -> Path:
    """Les chemins relatifs du .env sont résolus depuis la racine du projet."""
    p = Path(valeur).expanduser()
    return p if p.is_absolute() else (racine / p)


def charger_environnement(racine: Path = RACINE_PROJET) -> Environnement:
    """Lit le fichier .env (s'il existe) puis les variables d'environnement.

    Les variables déjà présentes dans l'environnement ont priorité sur le .env,
    ce qui permet de les surcharger sur un serveur (systemd, Docker…).
    """
    load_dotenv(racine / ".env", override=False)
    return Environnement(
        anthropic_api_key=os.environ.get("ANTHROPIC_API_KEY") or None,
        model=os.environ.get("MODEL", "").strip() or "claude-sonnet-5-5",
        google_credentials_file=_chemin(
            os.environ.get("GOOGLE_CREDENTIALS_FILE", "secrets/credentials.json"), racine
        ),
        google_token_file=_chemin(os.environ.get("GOOGLE_TOKEN_FILE", "secrets/token.json"), racine),
        db_path=_chemin(os.environ.get("DB_PATH", "data/agent.db"), racine),
        config_dir=_chemin(os.environ.get("CONFIG_DIR", "config"), racine),
        log_level=os.environ.get("LOG_LEVEL", "INFO").upper(),
    )


# ---------------------------------------------------------------------------
# Chargement global
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class ConfigClient:
    """Ensemble de la configuration d'un client (les trois fichiers YAML)."""

    entreprise: ConfigEntreprise
    offres: ConfigOffres
    reglages: Reglages


def _lire_yaml(chemin: Path) -> dict:
    if not chemin.exists():
        raise ErreurConfig(f"Fichier de configuration introuvable : {chemin}")
    try:
        with chemin.open(encoding="utf-8") as f:
            donnees = yaml.safe_load(f)
    except yaml.YAMLError as exc:
        raise ErreurConfig(f"YAML invalide dans {chemin.name} : {exc}") from exc
    if not isinstance(donnees, dict):
        raise ErreurConfig(f"{chemin.name} doit contenir un dictionnaire YAML.")
    return donnees


def _valider(modele: type[BaseModel], donnees: dict, nom_fichier: str):
    try:
        return modele.model_validate(donnees)
    except ValidationError as exc:
        details = "; ".join(
            f"{'.'.join(str(x) for x in err['loc']) or '(racine)'} : {err['msg']}"
            for err in exc.errors()
        )
        raise ErreurConfig(f"Erreur dans {nom_fichier} -> {details}") from exc


def charger_config(config_dir: Path) -> ConfigClient:
    """Charge et valide entreprise.yaml, offres.yaml et reglages.yaml."""
    config_dir = Path(config_dir)
    return ConfigClient(
        entreprise=_valider(ConfigEntreprise, _lire_yaml(config_dir / "entreprise.yaml"), "entreprise.yaml"),
        offres=_valider(ConfigOffres, _lire_yaml(config_dir / "offres.yaml"), "offres.yaml"),
        reglages=_valider(Reglages, _lire_yaml(config_dir / "reglages.yaml"), "reglages.yaml"),
    )
