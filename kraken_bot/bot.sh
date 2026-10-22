#!/usr/bin/env bash
# Commandes courtes pour piloter le bot depuis un téléphone (installé sous le nom "kraken").
set -euo pipefail

DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
PY="$DIR/.venv/bin/python"
cd "$DIR"

usage() {
  cat <<'EOF'
Utilisation : kraken <commande>
  config    saisir les clés API Kraken et choisir réel / simulation
  check     vérifier la connexion au compte Kraken (aucun ordre envoyé)
  start     démarrer le bot (redémarre seul après un crash ou un reboot)
  stop      arrêter le bot (les stop-loss restent posés chez Kraken)
  restart   redémarrer le bot
  status    état du bot : en marche ?, positions, gains/pertes
  logs      suivre l'activité en direct (Ctrl+C pour quitter)
  reset     réarmer le bot après un arrêt d'urgence (drawdown)
  update    installer la dernière version du bot
  profil X  changer de profil : petit-capital ou explosif (garde tes clés)
EOF
}

config() {
  echo "Clés à créer sur Kraken : Paramètres > API (sans la permission de retrait)."
  read -rp "API_KEY (clé publique) : " key
  read -rsp "SECRET_KEY (clé privée, rien ne s'affiche, c'est normal) : " secret
  echo
  read -rp "Trader en RÉEL avec ton argent ? (oui/non) : " answer
  case "$answer" in
    oui|o|OUI|Oui) dry=false ;;
    *) dry=true ;;
  esac
  KEY="$key" SECRET="$secret" DRY="$dry" "$PY" - <<'PYEOF'
import os, pathlib
p = pathlib.Path(".env")
dry = os.environ["DRY"]
values = {
    "API_KEY": os.environ["KEY"].strip(),
    "SECRET_KEY": os.environ["SECRET"].strip(),
    "DRY_RUN": dry,
    "STATE_FILE": "bot_state_paper.json" if dry == "true" else "bot_state_live.json",
}
lines, seen = [], set()
for line in p.read_text(encoding="utf-8").splitlines():
    name = line.split("=", 1)[0].strip()
    if name in values and not line.lstrip().startswith("#"):
        lines.append(f"{name}={values[name]}")
        seen.add(name)
    else:
        lines.append(line)
lines += [f"{k}={v}" for k, v in values.items() if k not in seen]
p.write_text("\n".join(lines) + "\n", encoding="utf-8")
os.chmod(p, 0o600)
PYEOF
  if [ "$dry" = "false" ]; then
    echo "Mode RÉEL enregistré. Lance maintenant : kraken check"
  else
    echo "Mode SIMULATION enregistré. Lance maintenant : kraken check"
  fi
  if systemctl is-active -q kraken-bot 2>/dev/null; then
    sudo systemctl restart kraken-bot && echo "Bot redémarré avec la nouvelle configuration."
  fi
}

profil() {
  src=".env.${1:-}.example"
  if [ ! -f "$src" ]; then
    echo "Profils disponibles : petit-capital, explosif"
    exit 1
  fi
  SRC="$src" "$PY" - <<'PYEOF'
import os, pathlib
keep = ("API_KEY", "SECRET_KEY", "DRY_RUN", "STATE_FILE")
old = {}
env = pathlib.Path(".env")
if env.exists():
    for line in env.read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.lstrip().startswith("#"):
            k, v = line.split("=", 1)
            if k.strip() in keep:
                old[k.strip()] = v.strip()
lines = []
for line in pathlib.Path(os.environ["SRC"]).read_text(encoding="utf-8").splitlines():
    k = line.split("=", 1)[0].strip()
    if k in old and not line.lstrip().startswith("#"):
        line = f"{k}={old[k]}"
    lines.append(line)
env.write_text("\n".join(lines) + "\n", encoding="utf-8")
os.chmod(env, 0o600)
PYEOF
  echo "Profil '$1' activé (clés et mode conservés)."
  if systemctl is-active -q kraken-bot 2>/dev/null; then
    sudo systemctl restart kraken-bot && echo "Bot redémarré avec le nouveau profil."
  fi
}

case "${1:-}" in
  config)  config ;;
  profil)  profil "${2:-}" ;;
  check)   "$PY" check_setup.py ;;
  start)   sudo systemctl start kraken-bot && echo "Bot démarré. Suis-le avec : kraken logs" ;;
  stop)    sudo systemctl stop kraken-bot && echo "Bot arrêté." ;;
  restart) sudo systemctl restart kraken-bot && echo "Bot redémarré." ;;
  status)
    if systemctl is-active -q kraken-bot 2>/dev/null; then echo "Bot : EN MARCHE"; else echo "Bot : ARRÊTÉ"; fi
    "$PY" bot.py --status 2>/dev/null ;;
  logs)    sudo journalctl -u kraken-bot -n 50 -f ;;
  reset)   "$PY" bot.py --reset-halt --status >/dev/null && echo "Arrêt d'urgence réarmé. Relance : kraken start" ;;
  update)
    git -C "$DIR/.." pull -q --ff-only
    .venv/bin/pip install -q -r requirements.txt
    sudo systemctl restart kraken-bot || true
    echo "Bot mis à jour." ;;
  *) usage ;;
esac
