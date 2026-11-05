"""Tests de l'agent service client.

- Tests hors ligne (sans clé API) : la recherche dans la FAQ trouve la bonne réponse.
- Scénarios réalistes (avec ANTHROPIC_API_KEY) : on pose de vraies questions à l'agent.

Lancer :  python -m pytest -v        (depuis agents/service-client/)
"""

import os

import pytest

from agent import ClaudeSDKClient, FAQ, options, rechercher, repondre


# ---- Tests hors ligne : la recherche FAQ ----------------------------------

@pytest.mark.parametrize(
    "question, id_attendu",
    [
        ("Quels sont vos délais de livraison ?", "livraison-delais"),
        ("La livraison est gratuite à partir de combien ?", "livraison-frais"),
        ("Je veux retourner ma lampe, c'est possible ?", "retours"),
        ("Ma suspension est arrivée cassée", "casse"),
        ("Est-ce que je peux payer avec PayPal ?", "paiement"),
        ("Vous avez un magasin à Lyon ?", "boutique-physique"),
        ("Combien de temps dure la garantie ?", "garantie"),
        ("Comment suivre ma commande ?", "suivi-commande"),
    ],
)
def test_recherche_trouve_la_bonne_reponse(question, id_attendu):
    resultats = rechercher(FAQ, question)
    assert resultats, f"aucun résultat pour : {question}"
    assert resultats[0]["id"] == id_attendu


def test_recherche_sans_resultat():
    assert rechercher(FAQ, "Quelle est la capitale du Pérou ?") == []


# ---- Scénarios réalistes avec l'API (nécessitent ANTHROPIC_API_KEY) --------

avec_api = pytest.mark.skipif(
    not os.environ.get("ANTHROPIC_API_KEY"), reason="ANTHROPIC_API_KEY non définie"
)

SCENARIOS = [
    # (question du client, mots qui doivent apparaître dans la réponse)
    ("Bonjour, je commande aujourd'hui, je reçois quand ?", ["2 à 4"]),
    ("La livraison est offerte à partir de quel montant ?", ["80"]),
    ("J'ai reçu ma lampe il y a 10 jours, je peux la renvoyer ?", ["30 jours"]),
    ("Vous avez un showroom ?", ["Lyon"]),
    # Hors FAQ : doit rediriger vers un humain au lieu d'inventer
    ("Vous livrez au Japon ?", ["support@maison-lumen.example"]),
]


@avec_api
@pytest.mark.parametrize("question, attendus", SCENARIOS)
async def test_scenario_reel(question, attendus):
    async with ClaudeSDKClient(options=options()) as client:
        reponse = await repondre(client, question)
    print(f"\nQ : {question}\nR : {reponse}")
    for mot in attendus:
        assert mot.lower() in reponse.lower(), f"« {mot} » absent de : {reponse}"
