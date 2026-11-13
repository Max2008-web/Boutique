"""Relève des nouveaux mails.

Fonctionnement :
1. Premier lancement (aucun curseur en base) : on récupère les mails de la
   boîte de réception des N derniers jours (``premiere_releve_jours``), puis on
   mémorise le ``historyId`` courant de Gmail.
2. Lancements suivants : on demande à Gmail uniquement les messages ajoutés à
   INBOX depuis ce curseur (API ``history``), ce qui est rapide et économe.
3. Si le curseur a expiré (agent arrêté plus d'une semaine environ), on
   repasse par une requête par date.

Le curseur n'est avancé qu'APRÈS le traitement (``confirmer``) : si l'agent
plante au milieu d'un lot, les mails seront repris au prochain passage. La
table ``messages_traites`` empêche de traiter deux fois le même message.
"""

from __future__ import annotations

import logging
from dataclasses import dataclass, field
from datetime import datetime, timezone

from .config import Reglages
from .db import Database
from .gmail_client import GmailClient, HistoriqueExpire
from .models import Email

log = logging.getLogger(__name__)

CLE_CURSEUR = "gmail_history_id"


@dataclass
class Lot:
    """Résultat d'une relève : les mails à traiter et le curseur à valider ensuite."""

    mails: list[Email] = field(default_factory=list)
    curseur: str | None = None
    ignores: dict[str, str] = field(default_factory=dict)  # message_id -> raison


class Releve:
    def __init__(self, gmail: GmailClient, db: Database, reglages: Reglages) -> None:
        self.gmail = gmail
        self.db = db
        self.reglages = reglages

    def _requete_initiale(self) -> str:
        return f"in:inbox newer_than:{self.reglages.premiere_releve_jours}d"

    def relever(self) -> Lot:
        """Retourne les nouveaux mails à traiter (sans modifier le curseur)."""
        curseur = self.db.lire_etat(CLE_CURSEUR)
        if curseur is None:
            log.info("Première relève : mails des %d derniers jours.", self.reglages.premiere_releve_jours)
            nouveau_curseur = self.gmail.history_id_courant()
            ids = self.gmail.lister_ids_par_requete(self._requete_initiale())
        else:
            try:
                ids, nouveau_curseur = self.gmail.lister_ids_depuis_historique(curseur)
            except HistoriqueExpire:
                log.warning("Curseur Gmail expiré : relève par date.")
                nouveau_curseur = self.gmail.history_id_courant()
                ids = self.gmail.lister_ids_par_requete(self._requete_initiale())

        lot = Lot(curseur=nouveau_curseur)
        adresse_boite = self.gmail.adresse_boite()
        vus: set[str] = set()
        for message_id in ids:
            if message_id in vus or self.db.deja_traite(message_id):
                continue
            vus.add(message_id)
            mail = self.gmail.lire_message(message_id)
            raison = self._raison_d_ignorer(mail, adresse_boite)
            if raison:
                lot.ignores[mail.id] = raison
                continue
            lot.mails.append(mail)

        # Traitement dans l'ordre chronologique.
        lot.mails.sort(key=lambda m: m.date or datetime.min.replace(tzinfo=timezone.utc))
        log.info("Relève : %d mail(s) à traiter, %d ignoré(s).", len(lot.mails), len(lot.ignores))
        return lot

    @staticmethod
    def _raison_d_ignorer(mail: Email, adresse_boite: str) -> str | None:
        """Messages qui ne sont pas des mails entrants à traiter."""
        if "DRAFT" in mail.libelles:
            return "brouillon"
        if "SENT" in mail.libelles or mail.expediteur == adresse_boite:
            return "envoyé par la boîte elle-même"
        if "INBOX" not in mail.libelles:
            return "plus dans la boîte de réception"
        return None

    def confirmer(self, lot: Lot) -> None:
        """À appeler une fois le lot traité : marque les ignorés et avance le curseur."""
        for message_id, raison in lot.ignores.items():
            self.db.marquer_traite(message_id, "", f"ignore:{raison}")
        if lot.curseur:
            self.db.ecrire_etat(CLE_CURSEUR, lot.curseur)
