# Bot Kraken : momentum agressif et cassure de volatilité

Bot de trading spot pour Kraken (BTC/USD, ETH/USD, SOL/USD…), écrit en Python avec `ccxt`, `pandas` et `numpy`.
Il cherche à capter les fortes impulsions haussières tout en limitant le risque de ruine par plusieurs garde-fous.

> ⚠️ Le trading de crypto-actifs peut faire perdre tout le capital engagé. Ce code est fourni à titre éducatif, sans aucune garantie de résultat.
> Lancez-le d'abord en simulation (`DRY_RUN=true`, le réglage par défaut), puis passez au backtest, et seulement ensuite au live avec un petit capital.

---

## 1. La stratégie

### Le principe
Les marchés crypto alternent de longues phases de compression (faible volatilité, range) et de courtes phases d'expansion violentes.
Une grande partie du rendement annuel se concentre dans ces quelques expansions.
La stratégie reste hors du marché la plupart du temps, entre dès qu'une expansion démarre, puis laisse courir la position aussi longtemps que l'impulsion dure.

### Le signal d'entrée (calculé sur une bougie clôturée de 15m ou 1h)
| Filtre | Condition | Rôle |
|---|---|---|
| Cassure | Clôture au-dessus de la bande de Bollinger haute (20, 2σ), alors que la bougie précédente clôturait encore dessous | Le prix sort de son enveloppe statistique normale |
| Expansion | La largeur des bandes augmente | La volatilité explose au lieu de rester comprimée |
| Volume | Volume supérieur à 1,8 × la moyenne des 20 bougies précédentes | La cassure est financée par de vrais flux, ce qui écarte une partie des fausses cassures |
| Tendance | ADX > 25 et +DI > −DI | La tendance est forte et orientée à la hausse |

Ces quatre filtres réunis sélectionnent les cassures qui ont le plus de chances de se transformer en vague d'impulsion.

### Pourquoi c'est conçu pour maximiser le gain
1. **Asymétrie des gains.** Le stop initial est placé à 2 ATR, donc la perte est connue et limitée à 1R. Un trailing stop chandelier (plus haut atteint − 3 ATR) laisse courir les gagnants. Un seul run de +8R ou +10R compense une série de petites pertes : c'est le profil d'un suiveur de tendance. Le taux de réussite est souvent de 30 à 45 %, mais les gains moyens sont bien plus grands que les pertes moyennes.
2. **Entrée rapide.** L'ordre part dès la clôture de la bougie de cassure, au début de l'impulsion.
3. **Intérêts composés.** Avec `COMPOUND=true`, une fraction des gains réalisés (`COMPOUND_RATIO`) augmente le capital sur lequel la taille des positions est calculée. Les pertes, elles, le réduisent toujours à 100 %.
4. **Pyramiding (optionnel).** Avec `PYRAMID_MAX_ADDS>0`, le bot renforce une position gagnante tous les +1,5 ATR. Il ne le fait que lorsque le stop protège déjà le prix moyen d'entrée : le trade initial ne peut plus perdre.
5. **Frais réduits.** Les entrées passent en limite post-only (frais maker de 0,25 % contre 0,40 % en taker au premier palier). Sur des centaines de trades, cet écart représente une part importante du rendement net.
6. **Demi-Kelly (optionnel).** Avec `SIZING_MODE=kelly`, le risque par trade suit l'avantage statistique mesuré sur les 50 derniers trades : f* = p − (1−p)/b, en multiples de R. On en applique la moitié, avec un plafond de 3 %.

### Les garde-fous contre la ruine
- Le risque par trade est plafonné à 3 % du capital. Ce plafond est vérifié au démarrage et ne peut pas être dépassé par configuration.
- Le risque ouvert cumulé est limité à 6 %, avec au plus 3 positions simultanées. Il n'y a aucun levier : le notionnel d'une position ne dépasse jamais 95 % du capital.
- **Hard stop.** Si le drawdown sur 7 jours glissants atteint 10 %, le bot annule ses ordres, liquide ses positions (`FLATTEN_ON_HALT`) et s'arrête avec le code de sortie 2. Il refuse ensuite de redémarrer tant que vous n'avez pas lancé `python bot.py --reset-halt`.
- Un stop-loss est posé directement chez Kraken et remonté au fil du trailing. Les positions restent protégées si le bot, le VPS ou la connexion tombe.
- Si le prix s'est déjà éloigné de plus de 0,5 ATR au-dessus du signal, le bot n'entre pas.

---

## 2. Architecture

```
kraken_bot/
├── kraken_config.py   # .env, paramètres, validation, paliers de frais Kraken
├── kraken_feed.py     # ccxt.kraken, synchro horloge et nonce, call counter, symboles, WebSocket v2
├── strategy.py        # Bollinger, ATR, ADX, volume SMA, signaux (vectorisé)
├── risk_engine.py     # sizing fixe ou demi-Kelly, compound, trailing ATR, hard stop, état persistant
├── execution.py       # post-only, repricing, idempotence userref, retries, stops Kraken, broker papier
├── backtester.py      # backtest portefeuille avec frais maker/taker par palier
├── bot.py             # orchestrateur live / paper
├── check_setup.py     # vérification de l'intégration Kraken avant le live (aucun ordre envoyé)
├── kraken-bot.service # unité systemd pour VPS
├── requirements.txt
├── .env.example
└── .env.petit-capital.example  # profil prêt à l'emploi pour démarrer avec ~50 €
```

### Spécificités Kraken prises en charge
- **Symboles.** `resolve_symbol()` accepte `BTC/USD`, `XBT/USD`, `XXBTZUSD`, `XBTUSD` ou `btcusd` et renvoie le symbole unifié ccxt. ccxt traduit ensuite vers l'identifiant Kraken (`XXBTZUSD`).
- **Nonce.** Le nonce est strictement croissant (protégé par un verrou), en millisecondes, et calé sur l'heure serveur Kraken mesurée au démarrage. Sur `EAPI:Invalid nonce`, le bot resynchronise l'horloge et réessaie.
- **Call counter REST.** Le bot réplique localement le compteur Kraken : maximum 15/20/20 avec une décroissance de 0,33/0,5/1 par seconde selon le palier. Un appel au registre ou à l'historique des trades coûte 2. En cas de `Rate limit exceeded`, le compteur local est considéré plein et le bot applique un backoff.
- **Compteur du moteur de trading (par paire).** AddOrder coûte +1. Annuler un ordre de moins de 5 s coûte +8, de moins de 10 s +6, et ainsi de suite. Le bot laisse donc vivre ses ordres limites avant de les annuler.
- **Post-only.** L'ordre est placé au bid + 1 tick avec `oflags=post`. Si Kraken le rejette (il aurait été exécuté en taker), le bot recalcule le prix. Il fait jusqu'à 3 tentatives, puis peut basculer sur un ordre market (`TAKER_FALLBACK`).
- **Frais en devise de cotation** (`oflags=fciq`). Les quantités de BTC, ETH ou SOL achetées restent exactes.
- **Idempotence.** Chaque ordre porte un `userref` aléatoire. Après un timeout réseau, le bot vérifie chez Kraken si l'ordre a été accepté avant de le renvoyer, ce qui évite les doublons.

---

## 3. Lancement pas à pas (terminal local ou VPS Ubuntu/Debian)

### 3.1 Préparer la machine
```bash
sudo apt update && sudo apt install -y python3 python3-venv python3-pip git
sudo timedatectl set-ntp true          # horloge synchronisée, indispensable pour Kraken
timedatectl status                      # vérifier "System clock synchronized: yes"
```
Pour un VPS, choisissez de préférence un datacenter proche de ceux de Kraken (Europe de l'Ouest ou côte Est des États-Unis). Créez un utilisateur dédié (`sudo adduser trader`).

### 3.2 Installer le bot
```bash
git clone <url-du-repo> && cd <repo>/kraken_bot
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
cp .env.example .env
chmod 600 .env
```

### 3.3 Créer la clé API Kraken
Dans Kraken : **Settings → API → Create API key**.
- Permissions à cocher : *Query Funds*, *Query Open Orders & Trades*, *Query Closed Orders & Trades*, *Create & Modify Orders*, *Cancel/Close Orders*.
- Ne cochez **jamais** *Withdraw Funds*.
- Restreignez la clé à l'adresse IP du VPS.
- Utilisez une clé dédiée au bot : le nonce d'une clé partagée avec un autre outil entrerait en conflit.

Collez ensuite `API_KEY` et `SECRET_KEY` dans `.env`, et réglez `API_TIER` sur votre niveau de vérification.

### 3.4 Backtester
```bash
# Rapide : les 720 dernières bougies 1h via l'API OHLC (environ 30 jours)
python backtester.py --source ohlc --timeframe 1h

# Historique long : bougies reconstruites depuis les trades (lent, mis en cache dans data/)
python backtester.py --source trades --days 180 --timeframe 1h --symbols BTC/USD,ETH/USD

# Archives OHLCVT officielles de Kraken (support.kraken.com, "Downloadable historical OHLCVT data")
python backtester.py --source csv --csv BTC/USD=data/XBTUSD_60.csv --csv ETH/USD=data/ETHUSD_60.csv --timeframe 1h

# Variantes
python backtester.py --source ohlc --sizing kelly --pyramid 2 --save-trades trades.csv --save-equity equity.csv
```
Regardez en priorité le profit factor (> 1,3), le drawdown max, le poids des frais et la part du volume exécutée en maker.
Testez plusieurs périodes : un réglage qui ne gagne que sur un seul mois est sur-optimisé.

### 3.5 Simulation en conditions réelles (paper trading)
```bash
python bot.py            # DRY_RUN=true : prix réels, ordres simulés, soldes dans bot_state.json.paper.json
python bot.py --status   # état : positions, PnL, drawdown, risque courant
```
Laissez tourner au moins 2 à 4 semaines.

### 3.6 Passage en live
1. Lancez `python check_setup.py`. Il vérifie la connexion, l'horloge, les permissions de la clé, les soldes, les frais réels du compte, la faisabilité des trades avec votre capital et le WebSocket. Il n'envoie **aucun** ordre : le droit de trading est testé avec un ordre en mode `validate`, que Kraken contrôle sans l'exécuter. Corrigez chaque `[ÉCHEC]` avant d'aller plus loin.
2. Dans `.env` : `DRY_RUN=false` et `STATE_FILE=bot_state_live.json`. Un fichier d'état distinct est obligatoire, et le bot refuse de mélanger simulation et live.
3. Commencez avec un petit capital sur le compte.
4. Lancez `python bot.py`, puis surveillez `tail -f bot.log`.

### 3.7 Faire tourner 24/7 sur un VPS (systemd)
```bash
sudo cp kraken-bot.service /etc/systemd/system/
sudo nano /etc/systemd/system/kraken-bot.service   # ajuster User et les chemins
sudo systemctl daemon-reload
sudo systemctl enable --now kraken-bot
journalctl -u kraken-bot -f                        # logs en direct
sudo systemctl stop kraken-bot                     # arrêt propre (les stops Kraken restent en place)
```
Le service redémarre automatiquement après un crash. Il ne redémarre **pas** après un hard stop (code de sortie 2).
Dans ce cas, analysez la situation, puis lancez :
```bash
.venv/bin/python bot.py --reset-halt   # réarme, puis Ctrl+C
sudo systemctl start kraken-bot
```
Alternative sans systemd : `tmux new -s bot`, puis `python bot.py`, puis `Ctrl+B D` pour détacher la session.

---

## 4. Conseils et limites
- **Les frais sont l'ennemi principal en 15m.** Au premier palier, un aller-retour coûte de 0,5 % (maker + maker) à 0,8 % (taker + taker). Si le backtest 15m est mangé par les frais, passez en 1h ou montez le palier de volume.
- Les sorties sur stop passent volontairement en market (taker). La sécurité prime sur 0,15 % de frais.
- Une cassure peut se refermer aussitôt (faux signal). Elle est alors coupée au stop initial : c'est le coût normal de la stratégie.
- Le backtest suppose une exécution maker quand la bougie suivante traverse strictement le prix limite. La réalité (file d'attente du carnet) peut être moins favorable : prévoyez une marge.
- L'API OHLC de Kraken ne renvoie que 720 bougies. Pour un historique fiable, utilisez `--source trades` ou les archives CSV.
- Spot uniquement, positions longues seulement : pas de vente à découvert ni de levier, ce qui supprime le risque de liquidation.

---

## 5. Démarrer avec 50 €

### Ce que 50 € permettent (et ne permettent pas)
- **Techniquement, ça passe.** Avec 2 % de risque, chaque trade risque environ 1 €. Selon la volatilité, la position fait 15 à 45 €, au-dessus des minimums Kraken (environ 0,00005 BTC, soit quelques euros). `check_setup.py` fait le calcul exact avec les prix du jour.
- **Les frais pèsent lourd en proportion.** Un aller-retour sur une position de 30 € coûte environ 0,15 à 0,25 €, soit 15 à 25 % du risque de 1 €. Chaque trade démarre donc avec environ −0,2R. C'est pour ça que le profil utilise du 1h (stops plus larges, moins de trades) et des ordres post-only.
- **Les gains en euros resteront petits.** Même une excellente année à +30 % ferait 15 €. Avec 50 €, le vrai objectif est de valider le système en conditions réelles (exécutions maker, stops, frais, stabilité) avant d'y mettre plus d'argent. Ce n'est pas un revenu.

### Le plan, étape par étape
1. **Aujourd'hui, 0 € engagé.** Copiez le profil (`cp .env.petit-capital.example .env`), puis lancez `python backtester.py --source ohlc --timeframe 1h` et `python bot.py` en simulation. Laissez tourner 2 à 4 semaines sur votre ordinateur.
2. **Compte Kraken.** Inscription et vérification d'identité, nécessaire pour déposer des euros. Faites un dépôt de 50 € par virement SEPA, et vérifiez sur la page de dépôt de Kraken les frais et le minimum en vigueur.
3. **Clé API.** Créez une clé dédiée au bot, avec les permissions de la section 3.3 et sans *Withdraw Funds*, puis collez-la dans `.env`. Ne la partagez jamais, y compris dans une conversation avec une IA.
4. **Vérification.** Lancez `python check_setup.py` et attendez zéro `[ÉCHEC]`.
5. **Live.** Passez à `DRY_RUN=false` et `STATE_FILE=bot_state_live.json`, puis `python bot.py`. Contrôlez chaque trade dans l'historique Kraken pendant les premières semaines.
6. **Bilan après 2 à 3 mois.** N'augmentez le capital que si les résultats live ressemblent au backtest : taux de réussite, R moyen, frais réels.

### Où faire tourner le bot sans manger le capital
| Option | Coût | Verdict pour 50 € |
|---|---|---|
| Votre ordinateur, allumé en continu (veille désactivée) | 0 € | ✅ Idéal pour commencer. Le stop-loss posé chez Kraken protège la position si le PC s'éteint. |
| Offre cloud gratuite (ex. VM « Always Free » d'Oracle Cloud, région Paris ou Francfort) | 0 € (conditions à vérifier chez l'hébergeur) | ✅ Le meilleur choix pour du 24/7. Suivez la section 3.7 (systemd). |
| VPS payant (environ 4 à 6 €/mois) | 50 à 70 €/an | ❌ Coûterait plus que le capital chaque année. À envisager à partir d'environ 1 000 à 2 000 €. |
| Raspberry Pi | 60 à 100 € une fois | ⚠️ Plus cher que le capital de départ. |

### Fiscalité (résident fiscal français)
- Un compte ouvert sur une plateforme étrangère comme Kraken se déclare chaque année avec les revenus (formulaire 3916-bis).
- Les ventes de crypto contre des euros sont imposables : chaque sortie de position BTC/EUR en est une.
- Le bot enregistre tous les trades dans le fichier d'état, et Kraken permet d'exporter l'historique pour la déclaration. Vérifiez les règles en vigueur sur impots.gouv.fr.
