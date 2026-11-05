# Agent « service client »

Assistant qui répond aux questions fréquentes des clients d'une entreprise
(livraison, retours, paiement, garantie, boutique…), uniquement à partir d'une FAQ
validée. S'il ne connaît pas la réponse, il n'invente rien : il transmet la demande
à un conseiller humain et donne le contact.

- **Client :** agent de test / démonstration (entreprise fictive « Maison Lumen »)
- **Outils de l'agent :** `rechercher_faq`, `transmettre_a_un_conseiller`

## Fichiers

| Fichier         | Rôle                                                          |
|-----------------|---------------------------------------------------------------|
| `agent.py`      | L'agent (prompt, outils, mode conversation)                   |
| `faq.json`      | Les questions/réponses de l'entreprise — **à adapter par client** |
| `test_agent.py` | Tests hors ligne + scénarios réalistes avec l'API             |

## Utilisation

Depuis la racine du projet, après l'installation (voir `CLAUDE.md`) :

```bash
export ANTHROPIC_API_KEY=...        # jamais dans le code

# Conversation dans le terminal
python agents/service-client/agent.py

# Une seule question
python agents/service-client/agent.py "Quels sont vos délais de livraison ?"
```

## Adapter à un nouveau client

1. Remplacer le contenu de `faq.json` (nom de l'entreprise, contact humain, questions,
   réponses et mots-clés). On peut aussi pointer vers un autre fichier avec
   `FAQ_PATH=/chemin/faq.json`.
2. Brancher `transmettre_a_un_conseiller` sur l'outil réel du client
   (e-mail, CRM, Zendesk…) — aujourd'hui il affiche simplement le ticket.
3. Mettre à jour les questions de `test_agent.py` et relancer les tests.

Le modèle se change avec la variable `CLAUDE_MODEL` (par défaut `claude-sonnet-5-5`).

## Tests

```bash
cd agents/service-client
python -m pytest -v
```

Sans clé API, seuls les tests de recherche dans la FAQ s'exécutent ; les scénarios
réalistes (vraies questions posées à l'agent) sont ignorés.
