"""Structures de données partagées par les modules de l'agent."""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime


@dataclass(frozen=True)
class Email:
    """Un email reçu, sous une forme indépendante de l'API Gmail.

    Attributs principaux :
        id             identifiant Gmail du message
        thread_id      identifiant Gmail du fil de discussion
        message_id     en-tête RFC 5322 "Message-ID" (sert à répondre dans le fil)
        expediteur     adresse email de l'expéditeur, en minuscules
        nom_expediteur nom affiché de l'expéditeur (peut être vide)
        objet          objet du mail
        corps          texte brut du mail (le HTML est converti en texte)
        entetes        en-têtes utiles au pré-filtre, clés en minuscules
        libelles       libellés Gmail (INBOX, SPAM, CATEGORY_PROMOTIONS…)
    """

    id: str
    thread_id: str
    expediteur: str
    nom_expediteur: str
    objet: str
    corps: str
    date: datetime | None = None
    destinataires: tuple[str, ...] = ()
    cc: tuple[str, ...] = ()
    message_id: str = ""
    references: str = ""
    extrait: str = ""
    libelles: tuple[str, ...] = ()
    entetes: dict[str, str] = field(default_factory=dict)
    pieces_jointes: tuple[str, ...] = ()

    @property
    def domaine_expediteur(self) -> str:
        return self.expediteur.rsplit("@", 1)[-1] if "@" in self.expediteur else ""

    def resume_court(self, longueur: int = 80) -> str:
        """Ligne lisible pour les logs : expéditeur + objet tronqué."""
        objet = self.objet if len(self.objet) <= longueur else self.objet[: longueur - 1] + "…"
        return f"{self.expediteur} — {objet}"
