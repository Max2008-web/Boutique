"""Tests de la relève incrémentale des nouveaux mails."""

from datetime import datetime, timezone

import pytest

from agent.gmail_client import GmailClient
from agent.releve import CLE_CURSEUR, Releve
from tests.fakes import FauxServiceGmail, fabriquer_message


@pytest.fixture
def svc():
    return FauxServiceGmail(history_id="1000")


@pytest.fixture
def releve(svc, db, config):
    return Releve(GmailClient(svc, dormir=lambda s: None), db, config.reglages)


def test_premiere_releve_par_requete(svc, releve, db):
    svc.ajouter(fabriquer_message("a"))
    svc.ajouter(fabriquer_message("b"))
    lot = releve.relever()
    assert {m.id for m in lot.mails} == {"a", "b"}
    assert any(a.startswith("messages.list:in:inbox newer_than:1d") for a in svc.appels)
    # Le curseur n'est PAS avancé avant confirmation.
    assert db.lire_etat(CLE_CURSEUR) is None
    releve.confirmer(lot)
    assert db.lire_etat(CLE_CURSEUR) == "1002"


def test_releve_suivante_via_historique(svc, releve, db):
    svc.ajouter(fabriquer_message("a"))
    releve.confirmer(releve.relever())
    for m in ("a",):
        db.marquer_traite(m, "t", "traite")

    svc.ajouter(fabriquer_message("c"))
    lot = releve.relever()
    assert [m.id for m in lot.mails] == ["c"]
    assert "history.list:1001" in svc.appels


def test_aucun_mail_traite_deux_fois(svc, releve, db):
    svc.ajouter(fabriquer_message("a"))
    lot = releve.relever()
    db.marquer_traite("a", "t-a", "traite")
    # Crash simulé : curseur non confirmé, on relève à nouveau.
    lot2 = releve.relever()
    assert lot.mails and not lot2.mails


def test_mails_envoyes_et_brouillons_ignores(svc, releve):
    svc.ajouter(fabriquer_message("envoye", expediteur="contact@cabinet-durand.example"))
    svc.ajouter(fabriquer_message("brouillon", libelles=("INBOX", "DRAFT")))
    svc.ajouter(fabriquer_message("ok"))
    lot = releve.relever()
    assert [m.id for m in lot.mails] == ["ok"]
    assert set(lot.ignores) == {"envoye", "brouillon"}


def test_ignores_marques_apres_confirmation(svc, releve, db):
    svc.ajouter(fabriquer_message("brouillon", libelles=("INBOX", "DRAFT")))
    releve.confirmer(releve.relever())
    assert db.deja_traite("brouillon")


def test_curseur_expire_repli_par_date(svc, releve, db):
    db.ecrire_etat(CLE_CURSEUR, "1")
    svc.historique_expire = True
    svc.ajouter(fabriquer_message("x"))
    lot = releve.relever()
    assert [m.id for m in lot.mails] == ["x"]
    assert lot.curseur == svc.history_id


def test_ordre_chronologique(svc, releve):
    svc.ajouter(fabriquer_message("tard", date=datetime(2026, 11, 13, 15, tzinfo=timezone.utc)))
    svc.ajouter(fabriquer_message("tot", date=datetime(2026, 11, 13, 8, tzinfo=timezone.utc)))
    assert [m.id for m in releve.relever().mails] == ["tot", "tard"]
