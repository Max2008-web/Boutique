# Lancer le bot Kraken avec seulement un téléphone

Le bot tourne sur un petit serveur gratuit dans le cloud, 24 h/24.
Toi, tu le pilotes depuis ton téléphone. Compte environ 1 heure la première fois.

Ce qu'il te faut :
- un téléphone (Android ou iPhone) ;
- une carte bancaire, demandée par Oracle pour vérifier ton identité (l'offre gratuite n'est pas débitée) ;
- ton compte Kraken avec tes 50 €.

---

## Étape 1 : installer Termius et créer ta clé de connexion (5 min)
1. Installe l'appli **Termius** (Play Store ou App Store). La version gratuite suffit.
2. Dans Termius, ouvre **Keychain**, touche **+**, puis **Generate Key**. Choisis le type **RSA**, donne-lui le nom `oracle`, puis touche **Save**.
3. Ouvre la clé et touche **Copy public key** (« copier la clé publique »). Elle commence par `ssh-rsa`. Garde-la dans ton presse-papier.

## Étape 2 : créer ton serveur gratuit chez Oracle Cloud (20 à 30 min)
1. Va sur **oracle.com/cloud/free** depuis ton navigateur, puis touche **Start for free**.
2. Remplis l'inscription. Pour la **Home Region**, choisis **France Central (Paris)**, **France South (Marseille)** ou **Germany Central (Frankfurt)**. Ce choix est définitif.
3. Une fois connecté au tableau de bord : ouvre le menu **☰**, puis **Compute**, **Instances**, **Create instance**.
4. **Image** : touche *Edit*, puis *Change image*, puis **Canonical Ubuntu** (24.04).
5. **Shape** : *Change shape*, puis **Ampere**, puis **VM.Standard.A1.Flex** avec 1 OCPU et 6 GB. Vérifie l'étiquette « Always Free-eligible ».
   - Si Oracle affiche « Out of capacity », prends plutôt **AMD**, puis **VM.Standard.E2.1.Micro** (également gratuit).
6. **Add SSH keys** : choisis **Paste public keys**, puis colle la clé copiée à l'étape 1.
7. Touche **Create**. Attends que le statut passe à **Running** (vert).
8. Note l'adresse **Public IP address** (exemple : `141.145.xx.xx`).

## Étape 3 : te connecter au serveur depuis Termius (2 min)
1. Dans Termius, ouvre **Hosts**, puis **+**, puis **New Host**.
2. Remplis les champs :
   - **Address** : l'adresse IP notée à l'étape 2 ;
   - **Username** : `ubuntu` ;
   - **Key** : la clé `oracle`.
3. Enregistre, puis touche l'hôte pour te connecter. Accepte l'empreinte (*Continue* / *Yes*).
4. Une ligne du type `ubuntu@...:~$` apparaît : tu es sur ton serveur.

## Étape 4 : installer le bot en une seule commande (3 min)
Copie-colle cette ligne dans Termius, puis valide :
```bash
curl -fsSL https://raw.githubusercontent.com/Max2008-web/Boutique/ccr-4d7b60f1-vuvg52/kraken_bot/install.sh | bash
```
Attends le message **« Installation terminée »**.

## Étape 5 : créer ta clé API Kraken (5 min)
1. Dans le navigateur du téléphone, va sur **pro.kraken.com** (active « Version ordinateur » si besoin) et connecte-toi.
2. Ouvre ton profil, puis **Settings**, **API**, **Create API key**.
3. Coche uniquement :
   - Query Funds
   - Query Open Orders & Trades
   - Query Closed Orders & Trades
   - Create & Modify Orders
   - Cancel/Close Orders
4. **Ne coche jamais « Withdraw Funds »** : comme ça, même si ta clé fuitait, personne ne pourrait retirer ton argent.
5. Si Kraken propose une restriction par adresse IP, mets l'IP de ton serveur Oracle.
6. Valide. Kraken affiche une **API Key** et une **Private Key**. Garde cette page ouverte pour l'étape suivante.
   **Ne les envoie à personne, pas même à une IA ni à un « support ».**

## Étape 6 : configurer et lancer le bot (5 min)
Dans Termius :
```bash
kraken config
```
- Colle l'**API Key**, puis valide.
- Colle la **Private Key**, puis valide. Rien ne s'affiche quand tu colles, c'est normal.
- Réponds `oui` pour trader en réel.

Ensuite :
```bash
kraken check
```
Il ne doit y avoir **aucun `[ÉCHEC]`**. Si tu en vois un, copie le message (jamais tes clés) et demande de l'aide.

Puis :
```bash
kraken start
kraken logs
```
Le bot tourne. Tu peux fermer Termius et éteindre ton téléphone : le serveur continue tout seul.

---

## Les commandes du quotidien
| Commande | Effet |
|---|---|
| `kraken status` | Le bot est-il en marche ? Positions ouvertes, gains et pertes |
| `kraken logs` | Activité en direct (pour sortir : Ctrl+C, touche **Ctrl** du clavier Termius puis **C**) |
| `kraken stop` | Arrête le bot (les stop-loss restent posés chez Kraken) |
| `kraken start` | Relance le bot |
| `kraken config` | Change les clés ou le mode réel / simulation |
| `kraken reset` | Réarme le bot après un arrêt d'urgence |
| `kraken update` | Installe la dernière version du bot |
| `kraken profil explosif` | Mises fixes de 5 €, jusqu'à 8 paris sur 13 cryptos volatiles, trailing large |
| `kraken profil petit-capital` | Retour au profil prudent (1 position, BTC/ETH/SOL) |

Tu peux aussi suivre tous les ordres et trades du bot dans ton appli Kraken habituelle.

## Ce qu'il faut savoir
- **Le bot n'achète pas tout de suite.** Il attend une vraie cassure validée par ses quatre filtres. Cela peut prendre plusieurs heures, voire plusieurs jours. Dans `kraken logs`, chaque heure, une ligne par paire indique `signal=False` ou `signal=True`.
- **Chaque trade risque environ 1 €** (2 % du capital). Si le compte perd 10 % sur 7 jours, le bot vend tout et s'arrête. Il ne repart qu'après `kraken reset`.
- **Avec 50 €, attends-toi à des gains et des pertes de l'ordre de quelques euros, pas de centaines.** Personne ne peut gagner des centaines d'euros par jour avec 50 € : quelqu'un qui te le promet cherche à t'arnaquer.
