"""Agent de service client : répond aux questions fréquentes d'une entreprise.

Utilisation :
    python agent.py                       # mode conversation dans le terminal
    python agent.py "Quels sont vos délais de livraison ?"   # une seule question

La clé API est lue dans la variable d'environnement ANTHROPIC_API_KEY.
"""

import asyncio
import json
import os
import sys
import unicodedata
from pathlib import Path
from typing import Any

from claude_agent_sdk import (
    AssistantMessage,
    ClaudeAgentOptions,
    ClaudeSDKClient,
    TextBlock,
    create_sdk_mcp_server,
    tool,
)

DOSSIER = Path(__file__).resolve().parent
FAQ_PATH = Path(os.environ.get("FAQ_PATH", DOSSIER / "faq.json"))
MODELE = os.environ.get("CLAUDE_MODEL", "claude-sonnet-5-5")


# --------------------------------------------------------------------------
# Base de connaissances
# --------------------------------------------------------------------------

def charger_faq(path: Path = FAQ_PATH) -> dict[str, Any]:
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _normaliser(texte: str) -> str:
    """Minuscules sans accents, pour une recherche tolérante."""
    texte = unicodedata.normalize("NFD", texte.lower())
    return "".join(c for c in texte if unicodedata.category(c) != "Mn")


def rechercher(faq: dict[str, Any], requete: str, limite: int = 3) -> list[dict[str, Any]]:
    """Retourne les entrées de FAQ les plus pertinentes pour la requête."""
    mots = set(_normaliser(requete).replace("?", " ").replace("'", " ").split())
    resultats = []
    for entree in faq["questions"]:
        score = 0
        for cle in entree["mots_cles"]:
            cle_n = _normaliser(cle)
            if " " in cle_n:
                score += 2 if cle_n in _normaliser(requete) else 0
            # « casse » trouve aussi « cassee », « retour » trouve « retours »…
            elif any(m == cle_n or (len(cle_n) >= 4 and m.startswith(cle_n)) for m in mots):
                score += 1
        if score:
            resultats.append((score, entree))
    resultats.sort(key=lambda r: r[0], reverse=True)
    return [e for _, e in resultats[:limite]]


FAQ = charger_faq()


# --------------------------------------------------------------------------
# Outils exposés à l'agent
# --------------------------------------------------------------------------

@tool(
    "rechercher_faq",
    "Cherche dans la FAQ officielle de l'entreprise les réponses liées à la question du client. "
    "Toujours utiliser cet outil avant de répondre à une question sur l'entreprise.",
    {"requete": str},
)
async def rechercher_faq(args: dict[str, Any]) -> dict[str, Any]:
    trouves = rechercher(FAQ, args["requete"])
    if not trouves:
        texte = "Aucune réponse trouvée dans la FAQ pour cette question."
    else:
        texte = "\n\n".join(f"Q : {e['question']}\nR : {e['reponse']}" for e in trouves)
    return {"content": [{"type": "text", "text": texte}]}


@tool(
    "transmettre_a_un_conseiller",
    "Transmet la demande à un conseiller humain quand la FAQ ne permet pas de répondre, "
    "quand le client est mécontent, ou quand il faut accéder à son dossier personnel.",
    {"resume": str},
)
async def transmettre_a_un_conseiller(args: dict[str, Any]) -> dict[str, Any]:
    # Version de test : en production, créer ici un ticket (e-mail, CRM, Zendesk…).
    print(f"\n[ticket créé pour un conseiller] {args['resume']}\n", file=sys.stderr)
    return {
        "content": [{
            "type": "text",
            "text": f"Ticket créé. Contact humain : {FAQ['contact_humain']}",
        }]
    }


SERVEUR = create_sdk_mcp_server(
    name="faq", version="1.0.0", tools=[rechercher_faq, transmettre_a_un_conseiller]
)


SYSTEM_PROMPT = f"""Tu es l'assistant du service client de {FAQ['entreprise']}.

Règles :
- Réponds en français, avec un ton chaleureux, poli et concis (3 à 5 phrases maximum).
- Avant de répondre à une question sur l'entreprise, utilise toujours l'outil rechercher_faq.
- Ne réponds qu'à partir des informations de la FAQ. N'invente jamais de prix, délai,
  politique ou information absente de la FAQ.
- Si la FAQ ne contient pas la réponse, si le client est mécontent ou si la demande concerne
  son dossier personnel (une commande précise, un remboursement en cours…), utilise
  transmettre_a_un_conseiller puis donne au client le contact humain.
- Ne demande jamais de mot de passe ni de numéro de carte bancaire.
- Si la question n'a rien à voir avec l'entreprise, ramène poliment la conversation
  vers le service client.
"""


def options() -> ClaudeAgentOptions:
    return ClaudeAgentOptions(
        system_prompt=SYSTEM_PROMPT,
        model=MODELE,
        tools=[],  # aucun outil intégré (fichiers, shell…) : uniquement nos outils FAQ
        mcp_servers={"faq": SERVEUR},
        allowed_tools=[
            "mcp__faq__rechercher_faq",
            "mcp__faq__transmettre_a_un_conseiller",
        ],
        max_turns=6,
    )


# --------------------------------------------------------------------------
# Exécution
# --------------------------------------------------------------------------

async def repondre(client: ClaudeSDKClient, question: str) -> str:
    await client.query(question)
    morceaux = []
    async for message in client.receive_response():
        if isinstance(message, AssistantMessage):
            morceaux += [b.text for b in message.content if isinstance(b, TextBlock)]
    return "\n".join(morceaux).strip()


async def main() -> None:
    if not os.environ.get("ANTHROPIC_API_KEY"):
        sys.exit("Erreur : définissez la variable d'environnement ANTHROPIC_API_KEY.")

    async with ClaudeSDKClient(options=options()) as client:
        if len(sys.argv) > 1:
            print(await repondre(client, " ".join(sys.argv[1:])))
            return

        print(f"Service client — {FAQ['entreprise']}\n(tapez « quitter » pour sortir)\n")
        while True:
            try:
                question = input("Vous : ").strip()
            except (EOFError, KeyboardInterrupt):
                break
            if question.lower() in {"quitter", "exit", "q"}:
                break
            if question:
                print(f"\nAssistant : {await repondre(client, question)}\n")


if __name__ == "__main__":
    asyncio.run(main())
