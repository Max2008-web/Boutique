"""Tests de la lecture Gmail : parsing des messages et appels à l'API (faux service)."""

import pytest

from agent.gmail_client import (
    GmailClient,
    HistoriqueExpire,
    analyser_message,
    avec_relances,
    html_vers_texte,
)
from tests.fakes import ErreurHttpFactice, FauxServiceGmail, b64, fabriquer_message


# --- Parsing ----------------------------------------------------------------

def test_analyse_message_texte():
    m = analyser_message(fabriquer_message("m1", objet="Horaires ?", texte="Bonjour,\r\nvous ouvrez quand ?"))
    assert m.id == "m1"
    assert m.thread_id == "t-m1"
    assert m.expediteur == "jean.martin@client.example"
    assert m.nom_expediteur == "Jean Martin"
    assert m.objet == "Horaires ?"
    assert m.corps == "Bonjour,\nvous ouvrez quand ?"
    assert m.message_id == "<m1@client.example>"
    assert m.destinataires == ("contact@cabinet-durand.example",)
    assert m.date is not None and m.date.year == 2026
    assert m.domaine_expediteur == "client.example"


def test_analyse_accents_et_emojis():
    m = analyser_message(fabriquer_message("m2", texte="Délai pour le bilan ? Merci 🙏 — Hélène"))
    assert "Délai" in m.corps and "Hélène" in m.corps and "🙏" in m.corps


def test_html_seul_converti_en_texte():
    html = "<html><head><style>p{color:red}</style></head><body><p>Bonjour</p><p>Votre <b>tarif</b> ?</p><script>x()</script></body></html>"
    m = analyser_message(fabriquer_message("m3", texte=None, html=html))
    assert m.corps == "Bonjour\nVotre tarif ?"
    assert "color" not in m.corps and "x()" not in m.corps


def test_texte_prefere_au_html():
    m = analyser_message(fabriquer_message("m4", texte="version texte", html="<p>version html</p>"))
    assert m.corps == "version texte"


def test_pieces_jointes_listees_sans_contenu():
    m = analyser_message(fabriquer_message("m5", pieces_jointes=("facture.pdf", "kbis.pdf")))
    assert m.pieces_jointes == ("facture.pdf", "kbis.pdf")


def test_entetes_utiles_conserves():
    m = analyser_message(fabriquer_message("m6", entetes={
        "List-Unsubscribe": "<mailto:unsub@news.example>",
        "Auto-Submitted": "auto-replied",
        "X-Inutile": "abc",
    }))
    assert m.entetes["list-unsubscribe"] == "<mailto:unsub@news.example>"
    assert m.entetes["auto-submitted"] == "auto-replied"
    assert "x-inutile" not in m.entetes


def test_message_simple_non_multipart_latin1():
    message = {
        "id": "m7", "threadId": "t7", "labelIds": ["INBOX"], "snippet": "",
        "payload": {
            "mimeType": "text/plain",
            "headers": [{"name": "From", "value": "a@b.example"},
                        {"name": "Subject", "value": "Test"},
                        {"name": "Content-Type", "value": "text/plain; charset=ISO-8859-1"}],
            "body": {"data": b64("Café crème", "latin-1")},
        },
    }
    assert analyser_message(message).corps == "Café crème"


def test_date_invalide_ne_plante_pas():
    msg = fabriquer_message("m8")
    for h in msg["payload"]["headers"]:
        if h["name"] == "Date":
            h["value"] = "pas une date"
    assert analyser_message(msg).date is None


def test_html_vers_texte_listes():
    assert html_vers_texte("<ul><li>Un</li><li>Deux</li></ul>") == "Un\nDeux"


# --- Client -----------------------------------------------------------------

def test_client_profil_et_lecture():
    svc = FauxServiceGmail()
    svc.ajouter(fabriquer_message("a1"))
    client = GmailClient(svc, dormir=lambda s: None)
    assert client.adresse_boite() == "contact@cabinet-durand.example"
    assert client.lire_message("a1").id == "a1"


def test_client_pagination():
    svc = FauxServiceGmail()
    for i in range(250):
        svc.ajouter(fabriquer_message(f"p{i}"), dans_historique=False)
    client = GmailClient(svc, dormir=lambda s: None)
    assert len(client.lister_ids_par_requete("in:inbox", maximum=500)) == 250
    assert len(client.lister_ids_par_requete("in:inbox", maximum=120)) == 120


def test_client_historique():
    svc = FauxServiceGmail(history_id="1000")
    svc.ajouter(fabriquer_message("h1"))
    svc.ajouter(fabriquer_message("h2"))
    client = GmailClient(svc, dormir=lambda s: None)
    ids, curseur = client.lister_ids_depuis_historique("1001")
    assert ids == ["h2"]
    assert curseur == "1002"


def test_client_historique_expire():
    svc = FauxServiceGmail()
    svc.historique_expire = True
    client = GmailClient(svc, dormir=lambda s: None)
    with pytest.raises(HistoriqueExpire):
        client.lister_ids_depuis_historique("1")


def test_relance_sur_erreur_temporaire():
    svc = FauxServiceGmail()
    svc.erreurs_a_lever = [429, 503]
    attentes = []
    client = GmailClient(svc, dormir=attentes.append)
    assert client.adresse_boite() == "contact@cabinet-durand.example"
    assert len(attentes) == 2


def test_pas_de_relance_sur_erreur_definitive():
    appels = []

    def appel():
        appels.append(1)
        raise ErreurHttpFactice(403)

    with pytest.raises(ErreurHttpFactice):
        avec_relances(appel, dormir=lambda s: None)
    assert len(appels) == 1


def test_abandon_apres_trop_de_relances():
    def appel():
        raise ErreurHttpFactice(500)

    with pytest.raises(ErreurHttpFactice):
        avec_relances(appel, tentatives=3, dormir=lambda s: None)
