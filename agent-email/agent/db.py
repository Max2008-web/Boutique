"""Base SQLite de l'agent.

À l'étape 1, deux tables :
- ``etat``             : paires clé/valeur (ex. le curseur Gmail ``history_id``) ;
- ``messages_traites`` : un enregistrement par message déjà pris en charge,
                         pour ne jamais traiter (ni répondre à) un mail deux fois.

Le journal détaillé et la file de validation seront ajoutés aux étapes 5 et 6.
"""

from __future__ import annotations

import sqlite3
from contextlib import contextmanager
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterator

SCHEMA = """
CREATE TABLE IF NOT EXISTS etat (
    cle    TEXT PRIMARY KEY,
    valeur TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS messages_traites (
    message_id TEXT PRIMARY KEY,
    thread_id  TEXT NOT NULL,
    statut     TEXT NOT NULL,
    traite_le  TEXT NOT NULL
);
"""


def maintenant_utc() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


class Database:
    """Accès à la base. Une connexion courte par opération (sûr et simple)."""

    def __init__(self, chemin: Path | str) -> None:
        self.chemin = str(chemin)
        if self.chemin != ":memory:":
            Path(self.chemin).parent.mkdir(parents=True, exist_ok=True)
        # En mémoire, on garde une connexion unique (sinon la base disparaît).
        self._memoire = sqlite3.connect(":memory:") if self.chemin == ":memory:" else None
        self.initialiser()

    @contextmanager
    def connexion(self) -> Iterator[sqlite3.Connection]:
        conn = self._memoire or sqlite3.connect(self.chemin, timeout=30)
        conn.row_factory = sqlite3.Row
        try:
            yield conn
            conn.commit()
        except Exception:
            conn.rollback()
            raise
        finally:
            if self._memoire is None:
                conn.close()

    def initialiser(self) -> None:
        with self.connexion() as conn:
            if self._memoire is None:
                conn.execute("PRAGMA journal_mode=WAL")
            conn.executescript(SCHEMA)

    # -- État clé/valeur ------------------------------------------------------

    def lire_etat(self, cle: str) -> str | None:
        with self.connexion() as conn:
            ligne = conn.execute("SELECT valeur FROM etat WHERE cle = ?", (cle,)).fetchone()
        return ligne["valeur"] if ligne else None

    def ecrire_etat(self, cle: str, valeur: str) -> None:
        with self.connexion() as conn:
            conn.execute(
                "INSERT INTO etat (cle, valeur) VALUES (?, ?) "
                "ON CONFLICT(cle) DO UPDATE SET valeur = excluded.valeur",
                (cle, valeur),
            )

    # -- Messages traités -----------------------------------------------------

    def deja_traite(self, message_id: str) -> bool:
        with self.connexion() as conn:
            ligne = conn.execute(
                "SELECT 1 FROM messages_traites WHERE message_id = ?", (message_id,)
            ).fetchone()
        return ligne is not None

    def marquer_traite(self, message_id: str, thread_id: str, statut: str) -> None:
        with self.connexion() as conn:
            conn.execute(
                "INSERT INTO messages_traites (message_id, thread_id, statut, traite_le) "
                "VALUES (?, ?, ?, ?) "
                "ON CONFLICT(message_id) DO UPDATE SET statut = excluded.statut",
                (message_id, thread_id, statut, maintenant_utc()),
            )
