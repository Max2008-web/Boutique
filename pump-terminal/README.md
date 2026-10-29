# Pump Terminal — trading Pump.fun / PumpSwap sur Solana

Terminal de trading connecté à **votre vrai portefeuille Phantom**, alimenté en temps réel par la blockchain Solana, avec analyse **Grok (xAI)** et automatisations **sous votre contrôle**.

> ⚠️ **Avertissement.** Les tokens Pump.fun sont extrêmement spéculatifs : la grande majorité perd l'essentiel de sa valeur, beaucoup sont des arnaques (rug pulls, bundles, honeypots). Aucun outil, y compris celui-ci ou une IA, ne garantit de gain. N'engagez que des montants que vous acceptez de perdre entièrement.

---

## Ce que fait l'application (et d'où viennent les données)

| Fonction | Source réelle | Clé requise |
|---|---|---|
| Nouveaux tokens Pump.fun, trades en direct, migrations | WebSocket PumpPortal `wss://pumpportal.fun/api/data` | Non (gratuit). Une clé PumpPortal payante ajoute les trades PumpSwap après migration |
| Solde SOL, tokens, historique, P&L | Votre RPC Solana (`getBalance`, `getParsedTokenAccountsByOwner`, `getTransaction`…) | **Oui** : `SOLANA_RPC_URL` (Helius, QuickNode…) |
| Autorités du mint, top détenteurs | RPC Solana | idem |
| Prix, liquidité et volume des tokens migrés | API DexScreener | Non |
| Bougies historiques (≥ 1 min) | API GeckoTerminal | Non |
| Bougies en direct | Construites à partir des trades réellement observés | — |
| Prix SOL/USD | Jupiter Price API, repli CoinGecko | Non |
| Construction des transactions | API locale PumpPortal (`/api/trade-local`, **0,5 %** de frais) ou Jupiter (0 % de frais de plateforme) | Non |
| Analyse IA + recherche sur X | xAI Grok, API Responses avec l'outil `x_search` | **Oui** : `XAI_API_KEY` (payant à l'usage) |
| Alertes sur téléphone 24 h/24 | Bot Telegram | Optionnel |

Rien n'est simulé ni inventé : si une source est indisponible, l'interface affiche « — » ou un message d'erreur, jamais une valeur fictive.

### Terminal
- **Découverte** : trois colonnes en direct (*Nouveaux*, *Bientôt migrés*, *Migrés*), comme sur les terminaux Solana connus. Chaque carte montre la capitalisation, le volume, les acheteurs uniques, la progression de la courbe, la part du dev, les ventes du dev, les snipers, le top 10 et un score de risque. Achat rapide en un clic, toujours avec aperçu et signature.
- **Page token** : graphique en chandeliers (1 s → 1 h, prix ou capitalisation), statistiques, transactions en direct (dev et vous identifiés), top détenteurs on-chain, contrôles de sécurité (autorité de mint, autorité de gel, extensions Token-2022, concentration), analyse Grok, alertes personnalisées.
- **Achat / vente** : montants prédéfinis en SOL et boutons **25 %, 50 %, 75 %, 100 %** pour la vente.
- **Portefeuille** : solde réel en SOL et en dollars, tokens détenus et leur valeur, transactions récentes, **P&L réalisé et latent** calculé à partir de vos transactions on-chain (méthode du coût moyen, frais inclus).

### Sécurité des transactions
1. Le serveur construit la transaction pour **votre** adresse publique et vérifie que vous êtes bien le payeur.
2. Il la **simule sur Solana** (`simulateTransaction`) et lit l'état de votre compte après simulation : SOL dépensés ou reçus, tokens reçus, prix effectif, écart avec le prix spot.
3. L'aperçu affiche les **frais réseau** (base + priorité, lus dans les instructions Compute Budget), les **frais de plateforme**, le **slippage**, l'éventuelle **rente** de création de compte et les **risques**. Vous devez cocher une case de confirmation.
4. **Phantom signe** la transaction. Ni la phrase de récupération ni la clé privée ne sont jamais demandées ni stockées.
5. L'état reste « en attente » jusqu'à ce que `getSignatureStatuses` renvoie `confirmed` ou `finalized` **sans erreur**. Ce n'est qu'à ce moment que la transaction est affichée comme réussie. Le serveur relit ensuite la transaction on-chain pour l'enregistrer.

### Grok
- Analyse d'un token : Grok reçoit les données réelles (flux, on-chain, DexScreener, détenteurs, analyse de risque déterministe) et consulte X via `x_search`. Il répond en français : verdict, score d'opportunité et de risque, signes de manipulation (bundles, wash trading, ventes du dev), sentiment observé sur X et sources.
- Analyse du marché : tendances, narratifs sur X, opportunités spéculatives et alertes de risque.
- Analyse automatique optionnelle (`AI_AUTOSCAN_MINUTES`) avec alertes. Chaque appel est facturé par xAI.
- Si l'outil `x_search` n'est pas disponible pour votre modèle ou votre compte, l'analyse se fait sans X, et c'est **affiché explicitement** (« sans recherche X »).

### Automatisation
- Le serveur tourne **24 h/24** et surveille tous les nouveaux tokens.
- **Stratégies** à filtres (âge, capitalisation, acheteurs, volume, courbe, dev, snipers, top 10, réseaux sociaux, score de risque…) en mode *Alerte* ou *Proposer un achat*.
- **Take-profit / stop-loss** sur les positions ouvertes depuis le terminal.
- **Alertes personnalisées** par token : capitalisation au-dessus ou en dessous d'un seuil, gros achat, vente du dev, migration.
- **Limites** : montant maximum par transaction, dépense maximale par jour, perte réalisée maximale par jour, nombre maximum de positions. Ces limites peuvent aussi s'appliquer aux achats manuels.
- **Arrêt d'urgence** : désactive toutes les stratégies et annule les propositions en attente.
- **Exécution uniquement après votre autorisation explicite.** Le serveur ne détient aucune clé : il crée une *proposition* (notification navigateur, son, Telegram). Vous l'examinez, voyez l'aperçu complet, puis signez dans Phantom. Une proposition expire au bout de 2 minutes.

> Pourquoi pas d'exécution 100 % automatique ? Un serveur ne peut signer seul qu'en détenant une clé privée, ce que vous avez explicitement exclu, à raison. Phantom n'offre pas de délégation de signature à un serveur. Le modèle « proposition puis signature » est donc le seul compatible avec vos exigences de sécurité.

---

## Installation locale

Prérequis : Node.js 20 ou plus récent.

```bash
cd pump-terminal
npm run install:all
cp server/.env.example server/.env   # puis remplissez SOLANA_RPC_URL (et XAI_API_KEY, etc.)
npm run build
npm start                            # http://localhost:8080
```

Mode développement (rechargement à chaud) : `npm run dev:server` et, dans un autre terminal, `npm run dev:client` (http://localhost:5173, proxy vers le serveur).

## Déploiement (accès depuis Android, tablette et ordinateur)

Le serveur doit tourner **en continu** pour la surveillance 24 h/24. Les plateformes « serverless » comme Vercel ne conviennent pas. Phantom exige **HTTPS** en production.

### Option A — Render (le plus simple)
1. Poussez ce dépôt sur GitHub.
2. Sur render.com : **New → Blueprint**, puis sélectionnez le dépôt. Le fichier `render.yaml` à la racine est détecté automatiquement.
3. Renseignez `SOLANA_RPC_URL`, `XAI_API_KEY` et, si vous le souhaitez, `TELEGRAM_*`. `ACCESS_TOKEN` est généré automatiquement : copiez-le depuis le tableau de bord pour le saisir dans **Réglages**.
4. Le plan *Starter* (payant) est nécessaire : l'offre gratuite se met en veille et n'inclut pas de disque persistant.

### Option B — Docker sur un VPS (Hetzner, OVH, DigitalOcean…)
```bash
cd pump-terminal
docker build -t pump-terminal .
docker run -d --name pump-terminal --restart unless-stopped \
  -p 8080:8080 -v pump-data:/data --env-file server/.env pump-terminal
```
Placez ensuite un reverse proxy HTTPS devant (Caddy : `votre-domaine.fr { reverse_proxy localhost:8080 }`).

### Option C — Railway / Fly.io
Déployez le dossier `pump-terminal` avec son `Dockerfile`, ajoutez un volume monté sur `/data` et les mêmes variables d'environnement.

## Utilisation sur Android
- **Recommandé** : ouvrez l'URL du terminal dans le **navigateur intégré de l'application Phantom** (onglet Explorer, puis saisissez l'URL). Phantom est alors détecté directement.
- Ou dans Chrome Android : le bouton de connexion propose « Mobile Wallet Adapter », qui ouvre Phantom pour signer.
- Menu Chrome → « Ajouter à l'écran d'accueil » : le terminal s'installe comme une application (PWA).
- Pour recevoir les alertes application fermée, configurez Telegram.

## Clés, abonnements et autorisations nécessaires

| Service | Nécessaire ? | Coût indicatif | Où l'obtenir |
|---|---|---|---|
| RPC Solana (`SOLANA_RPC_URL`) | **Obligatoire** | Helius propose une offre gratuite pour démarrer ; offre payante conseillée en usage intensif | helius.dev, quicknode.com |
| xAI Grok (`XAI_API_KEY`) | Pour l'IA | Payant au token et par recherche X | console.x.ai |
| Hébergement 24 h/24 | Pour l'automatisation | Environ 5 à 10 $/mois | Render, VPS… |
| Telegram | Optionnel | Gratuit | @BotFather |
| PumpPortal API key | Optionnel (trades PumpSwap en direct) | Facturé par PumpPortal | pumpportal.fun |
| Jupiter API key | Optionnel (si `lite-api` est limité ou fermé) | Offre gratuite disponible | portal.jup.ag |
| Frais de trading | À chaque transaction | Frais réseau + priorité ; 0,5 % PumpPortal (route par défaut) ; frais du protocole Pump.fun/PumpSwap inclus dans le prix | — |

## Limites connues (en toute transparence)
- Les métriques « dev », « snipers » et « top 10 vus » proviennent du flux de trades **observé depuis le démarrage du serveur**. Pour un token créé avant, elles sont indisponibles (« — ») ; les détenteurs on-chain restent affichés.
- Les snipers sont détectés par heuristique (achats dans les 3 s suivant la création, hors dev).
- Le P&L du portefeuille porte sur les 150 dernières transactions et ne compte pas les tokens reçus par transfert comme des achats.
- La progression de la courbe utilise les paramètres standards de Pump.fun (1 073 M tokens virtuels / 30 SOL virtuels). Si Pump.fun modifie sa courbe, cette valeur deviendra approximative.
- Les API tierces (PumpPortal, Jupiter, xAI, DexScreener) peuvent changer. Leurs URL sont configurables dans `.env`.

## Architecture
```
pump-terminal/
├── server/src/
│   ├── index.ts        API REST + fichiers statiques
│   ├── pumpportal.ts   flux WebSocket PumpPortal (une connexion partagée, reconnexion auto)
│   ├── tokens.ts       état temps réel des tokens, statistiques, bougies
│   ├── onchain.ts      mint, détenteurs, portefeuille, historique, P&L
│   ├── trade.ts        construction, simulation et vérification des transactions
│   ├── risk.ts         analyse de risque déterministe
│   ├── ai.ts           Grok (Responses API + x_search)
│   ├── automation.ts   stratégies, alertes, TP/SL, propositions, arrêt d'urgence
│   ├── limits.ts       limites de dépenses et de pertes
│   ├── hub.ts          WebSocket vers les navigateurs
│   ├── notify.ts       alertes + Telegram
│   └── rpc.ts          RPC Solana + proxy en liste blanche (la clé reste côté serveur)
└── client/src/         React + Vite + Tailwind + Solana Wallet Adapter + lightweight-charts
```
