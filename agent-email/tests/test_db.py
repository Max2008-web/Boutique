"""Tests de la base SQLite."""

from agent.db import Database


def test_etat_lecture_ecriture(db):
    assert db.lire_etat("cle") is None
    db.ecrire_etat("cle", "1")
    db.ecrire_etat("cle", "2")
    assert db.lire_etat("cle") == "2"


def test_messages_traites(db):
    assert not db.deja_traite("m1")
    db.marquer_traite("m1", "t1", "repondu")
    assert db.deja_traite("m1")
    db.marquer_traite("m1", "t1", "remonte")  # mise à jour sans erreur
    with db.connexion() as conn:
        statut = conn.execute("SELECT statut FROM messages_traites WHERE message_id='m1'").fetchone()[0]
    assert statut == "remonte"


def test_persistance_sur_disque(tmp_path):
    chemin = tmp_path / "sous" / "dossier" / "agent.db"
    Database(chemin).ecrire_etat("x", "y")
    assert Database(chemin).lire_etat("x") == "y"


def test_base_en_memoire():
    db = Database(":memory:")
    db.marquer_traite("m", "t", "ok")
    assert db.deja_traite("m")
