#!/usr/bin/env bash
# Installation du bot Kraken sur un serveur Ubuntu (ex. Oracle Cloud "Always Free").
# Une seule commande depuis le serveur :
#   curl -fsSL https://raw.githubusercontent.com/Max2008-web/Boutique/ccr-4d7b60f1-vuvg52/kraken_bot/install.sh | bash
set -euo pipefail

REPO_URL="${REPO_URL:-https://github.com/Max2008-web/Boutique.git}"
BRANCH="${BRANCH:-ccr-4d7b60f1-vuvg52}"
INSTALL_DIR="${INSTALL_DIR:-$HOME/Boutique}"
BOT_DIR="$INSTALL_DIR/kraken_bot"
RUN_USER="$(id -un)"

echo "==> Installation des paquets système"
sudo apt-get update -y -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y -qq python3 python3-venv python3-pip git
sudo timedatectl set-ntp true || true

echo "==> Téléchargement du bot"
if [ -d "$INSTALL_DIR/.git" ]; then
  git -C "$INSTALL_DIR" fetch -q origin "$BRANCH"
  git -C "$INSTALL_DIR" checkout -q "$BRANCH"
  git -C "$INSTALL_DIR" pull -q --ff-only origin "$BRANCH"
else
  git clone -q --branch "$BRANCH" "$REPO_URL" "$INSTALL_DIR"
fi

echo "==> Installation des dépendances Python (1 à 3 minutes)"
cd "$BOT_DIR"
python3 -m venv .venv
.venv/bin/pip install -q --upgrade pip
.venv/bin/pip install -q -r requirements.txt

if [ ! -f .env ]; then
  cp .env.petit-capital.example .env
  chmod 600 .env
fi
chmod +x bot.sh install.sh

echo "==> Service de démarrage automatique"
sudo tee /etc/systemd/system/kraken-bot.service >/dev/null <<EOF
[Unit]
Description=Kraken momentum breakout bot
After=network-online.target time-sync.target
Wants=network-online.target

[Service]
Type=simple
User=$RUN_USER
WorkingDirectory=$BOT_DIR
ExecStart=$BOT_DIR/.venv/bin/python bot.py
Restart=on-failure
RestartPreventExitStatus=2
RestartSec=30
KillSignal=SIGTERM
TimeoutStopSec=60

[Install]
WantedBy=multi-user.target
EOF
sudo systemctl daemon-reload
sudo systemctl enable -q kraken-bot
sudo ln -sf "$BOT_DIR/bot.sh" /usr/local/bin/kraken

cat <<'EOF'

==========================================================
 Installation terminée.
 Étapes suivantes :
   kraken config   -> colle tes clés API Kraken, choisis le mode
   kraken check    -> vérifie la connexion à ton compte Kraken
   kraken start    -> démarre le bot (il tourne 24h/24)
   kraken logs     -> regarde ce que fait le bot
==========================================================
EOF
