# CLAUDE.md

## Rôle du projet

Je crée des **agents IA sur mesure pour des entreprises** avec le **Claude Agent SDK**
(Python, paquet `claude-agent-sdk`). Chaque fois que je te le demande, tu dois être
capable de créer un nouvel agent en suivant la méthode ci-dessous.

(Le dépôt contient aussi une petite app React « générateur de devis pour artisans » :
`src/`, `api/`, `README.md`. Elle est indépendante des agents Python.)

## Méthode pour chaque nouvel agent

1. **Me poser des questions avant de coder** — sur le client et son problème :
   - Qui est le client (secteur, taille, clientèle) ?
   - Quel problème précis l'agent doit-il résoudre ? Qui va l'utiliser ?
   - Quelles données / documents l'agent doit-il utiliser (FAQ, catalogue, tarifs…) ?
   - Quelles actions peut-il faire (outils : rechercher, créer un ticket, envoyer un e-mail…) ?
     Et ce qu'il ne doit **jamais** faire.
   - Ton, langue, cas où il doit passer la main à un humain.
   - Où il sera utilisé (terminal, site web, e-mail, Slack…).
2. **Créer l'agent dans son propre dossier** `agents/nom-de-l-agent/`
   (nom en minuscules avec tirets), en s'inspirant de `agents/service-client/` :
   - `agent.py` : prompt système, outils (`@tool` + `create_sdk_mcp_server`),
     `ClaudeAgentOptions` avec `tools=[]` et `allowed_tools` limités au strict nécessaire ;
   - les données du client dans des fichiers séparés (ex. `faq.json`), pas en dur dans le code ;
   - les dépendances nouvelles ajoutées dans `requirements.txt` à la racine.
3. **Le tester avec des exemples réalistes** dans `test_agent.py` :
   - tests hors ligne des outils (sans clé API) ;
   - scénarios réalistes posés à l'agent (vraies questions de clients, y compris des
     cas pièges : question hors sujet, info absente, client mécontent) — ignorés
     automatiquement si `ANTHROPIC_API_KEY` n'est pas définie ;
   - lancer les tests et me dire honnêtement ce qui passe, échoue ou n'a pas pu tourner.
4. **Écrire un petit `README.md`** dans le dossier de l'agent : à quoi il sert, pour quel
   client, comment le lancer, comment l'adapter, comment le tester.
5. **Mettre à jour la liste des agents** ci-dessous.

## Agents déjà créés

| Nom (dossier)                      | Rôle                                                                                   | Client                                            |
|------------------------------------|----------------------------------------------------------------------------------------|---------------------------------------------------|
| `agents/service-client/`           | Répond aux questions fréquentes (livraison, retours, paiement…) à partir d'une FAQ, transmet à un humain sinon | Agent de test — entreprise fictive « Maison Lumen » |

> À mettre à jour à chaque nouvel agent (nom, rôle, client).

## Installer et lancer les agents

Pré-requis : **Python 3.10+**. Le SDK embarque Claude Code, pas besoin de Node.js.

```bash
# Une seule fois, à la racine du projet
python3 -m venv .venv
source .venv/bin/activate          # Windows : .venv\Scripts\activate
pip install -r requirements.txt

# Clé API (voir section suivante)
export ANTHROPIC_API_KEY=...       # Windows : set ANTHROPIC_API_KEY=...

# Lancer un agent
python agents/<nom-de-l-agent>/agent.py

# Tester un agent
cd agents/<nom-de-l-agent> && python -m pytest -v
```

Variable optionnelle : `CLAUDE_MODEL` pour changer de modèle (par défaut `claude-sonnet-5-5`).

## Clé API — règle absolue

- La clé API se trouve dans la variable d'environnement **`ANTHROPIC_API_KEY`**.
- Elle ne doit **jamais** être écrite dans le code, dans un README, dans un test ni dans
  un fichier commité. Le code la lit uniquement via l'environnement.
- Le fichier `.env` (ignoré par git) peut la contenir en local ; `.env.example` ne
  contient qu'un exemple factice.
