"""Point d'entrée en ligne de commande : ``python -m agent <commande>``.

Commandes disponibles à l'étape 1 :
    auth             connecte la boîte Gmail (OAuth 2.0) et enregistre le jeton
    verifier-config  valide les fichiers YAML et le .env
    lire             affiche les derniers mails de la boîte (lecture seule)

Les commandes ``run`` (boucle continue) et ``once`` (un seul passage)
arriveront à l'étape 5.
"""

from __future__ import annotations

import argparse
import logging
import sys

from .config import ErreurConfig, charger_config, charger_environnement
from .gmail_client import (
    ErreurAuthGmail,
    GmailClient,
    construire_service,
    obtenir_identifiants,
)

log = logging.getLogger("agent")


def _configurer_logs(niveau: str) -> None:
    logging.basicConfig(
        level=getattr(logging, niveau, logging.INFO),
        format="%(asctime)s %(levelname)-7s %(name)s : %(message)s",
    )
    # Les bibliothèques Google sont très bavardes en DEBUG.
    logging.getLogger("googleapiclient").setLevel(logging.WARNING)


def cmd_auth(args, env) -> int:
    creds = obtenir_identifiants(
        env.google_credentials_file, env.google_token_file,
        interactif=True, port=args.port, ouvrir_navigateur=not args.sans_navigateur,
    )
    gmail = GmailClient(construire_service(creds))
    print(f"Connexion réussie à la boîte : {gmail.adresse_boite()}")
    print(f"Jeton enregistré dans : {env.google_token_file}")
    return 0


def cmd_verifier_config(args, env) -> int:
    cfg = charger_config(env.config_dir)
    print("Configuration valide.")
    print(f"  Entreprise      : {cfg.entreprise.entreprise.nom}")
    print(f"  FAQ             : {len(cfg.entreprise.faq)} entrées")
    print(f"  Produits        : {len(cfg.offres.produits)}")
    print(f"  Remises         : {len(cfg.offres.remises.autorisees)} (max {cfg.offres.remises.remise_max_pourcentage} %)")
    print(f"  Mode            : {cfg.reglages.mode}")
    print(f"  Seuil confiance : {cfg.reglages.seuil_confiance}")
    print(f"  Responsable     : {cfg.reglages.responsable.nom} <{cfg.reglages.responsable.email}>")
    print(f"  Modèle IA       : {env.model}")
    print(f"  Clé Anthropic   : {'présente' if env.anthropic_api_key else 'ABSENTE'}")
    print(f"  Jeton Gmail     : {'présent' if env.google_token_file.exists() else 'absent (python -m agent auth)'}")
    return 0


def cmd_lire(args, env) -> int:
    creds = obtenir_identifiants(env.google_credentials_file, env.google_token_file)
    gmail = GmailClient(construire_service(creds))
    ids = gmail.lister_ids_par_requete(f"in:inbox newer_than:{args.jours}d", maximum=args.max)
    print(f"Boîte {gmail.adresse_boite()} : {len(ids)} mail(s) sur {args.jours} jour(s).\n")
    for message_id in ids:
        m = gmail.lire_message(message_id)
        date = m.date.strftime("%d/%m/%Y %H:%M") if m.date else "?"
        print(f"[{date}] {m.nom_expediteur or ''} <{m.expediteur}>")
        print(f"  Objet : {m.objet or '(sans objet)'}")
        apercu = " ".join(m.corps.split())[:160]
        print(f"  {apercu}{'…' if len(m.corps) > 160 else ''}")
        if m.pieces_jointes:
            print(f"  Pièces jointes : {', '.join(m.pieces_jointes)}")
        print()
    return 0


def construire_parseur() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="python -m agent", description="Agent IA de gestion des emails")
    sous = p.add_subparsers(dest="commande", required=True)

    a = sous.add_parser("auth", help="Connecter la boîte Gmail (OAuth 2.0)")
    a.add_argument("--port", type=int, default=8080, help="Port local de retour OAuth (défaut 8080)")
    a.add_argument("--sans-navigateur", action="store_true",
                   help="Affiche l'URL au lieu d'ouvrir le navigateur (serveur sans écran)")
    a.set_defaults(func=cmd_auth)

    v = sous.add_parser("verifier-config", help="Valider la configuration")
    v.set_defaults(func=cmd_verifier_config)

    l = sous.add_parser("lire", help="Afficher les derniers mails (lecture seule)")
    l.add_argument("--jours", type=int, default=1)
    l.add_argument("--max", type=int, default=10)
    l.set_defaults(func=cmd_lire)
    return p


def main(argv: list[str] | None = None) -> int:
    args = construire_parseur().parse_args(argv)
    env = charger_environnement()
    _configurer_logs(env.log_level)
    try:
        return args.func(args, env)
    except (ErreurConfig, ErreurAuthGmail) as exc:
        print(f"ERREUR : {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
