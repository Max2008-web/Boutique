"""Connexion à l'API Gmail officielle (OAuth 2.0) et lecture des messages.

Ce module ne contient aucune logique métier : il sait seulement
- obtenir et rafraîchir le jeton OAuth ;
- lister les nouveaux messages (requête Gmail ou historique incrémental) ;
- lire un message et le convertir en objet ``Email``.

La création de brouillons, l'envoi et les libellés seront ajoutés à l'étape 5.

Le parsing (``analyser_message``) est une fonction pure, testable sans réseau.
"""

from __future__ import annotations

import base64
import logging
import random
import time
from email.utils import getaddresses, parseaddr, parsedate_to_datetime
from html.parser import HTMLParser
from pathlib import Path
from typing import Any, Callable, TypeVar

from .models import Email

log = logging.getLogger(__name__)

# gmail.modify = lecture, libellés, brouillons et envoi (pas de suppression
# définitive). C'est le périmètre minimal pour toutes les fonctions de l'agent.
SCOPES = ["https://www.googleapis.com/auth/gmail.modify"]

# En-têtes conservés pour le pré-filtre (newsletters, mails automatiques…).
ENTETES_UTILES = {
    "list-unsubscribe",
    "list-id",
    "auto-submitted",
    "precedence",
    "x-autoreply",
    "x-autorespond",
    "x-auto-response-suppress",
    "x-mailer",
    "return-path",
    "reply-to",
    "x-spam-flag",
    "content-type",
}


class ErreurAuthGmail(Exception):
    """Jeton absent, expiré ou révoqué : il faut relancer ``python -m agent auth``."""


class HistoriqueExpire(Exception):
    """Le curseur historyId est trop ancien (Gmail ne garde l'historique que ~1 semaine)."""


# ---------------------------------------------------------------------------
# Authentification OAuth 2.0
# ---------------------------------------------------------------------------

def obtenir_identifiants(
    credentials_file: Path,
    token_file: Path,
    interactif: bool = False,
    port: int = 8080,
    ouvrir_navigateur: bool = True,
):
    """Retourne des identifiants Google valides.

    - Si ``token_file`` existe, il est chargé et rafraîchi si besoin.
    - Sinon, en mode interactif, le parcours OAuth est lancé (navigateur).
    - Sinon, une ``ErreurAuthGmail`` explique quoi faire.
    """
    # Imports locaux : les tests hors ligne n'en ont pas besoin.
    from google.auth.exceptions import RefreshError
    from google.auth.transport.requests import Request
    from google.oauth2.credentials import Credentials
    from google_auth_oauthlib.flow import InstalledAppFlow

    creds = None
    if token_file.exists():
        creds = Credentials.from_authorized_user_file(str(token_file), SCOPES)

    if creds and creds.valid:
        return creds

    if creds and creds.expired and creds.refresh_token:
        try:
            creds.refresh(Request())
            _sauver_jeton(creds, token_file)
            return creds
        except RefreshError as exc:
            log.warning("Rafraîchissement du jeton impossible (%s).", exc)
            if not interactif:
                raise ErreurAuthGmail(
                    "Le jeton Gmail a été révoqué ou a expiré. Relancez : python -m agent auth"
                ) from exc

    if not interactif:
        raise ErreurAuthGmail(
            f"Aucun jeton Gmail valide ({token_file}). Lancez d'abord : python -m agent auth"
        )

    if not credentials_file.exists():
        raise ErreurAuthGmail(
            f"Fichier OAuth introuvable : {credentials_file}. "
            "Téléchargez-le depuis Google Cloud (voir README)."
        )

    flow = InstalledAppFlow.from_client_secrets_file(str(credentials_file), SCOPES)
    creds = flow.run_local_server(port=port, open_browser=ouvrir_navigateur, prompt="consent")
    _sauver_jeton(creds, token_file)
    return creds


def _sauver_jeton(creds, token_file: Path) -> None:
    token_file.parent.mkdir(parents=True, exist_ok=True)
    token_file.write_text(creds.to_json(), encoding="utf-8")
    try:
        token_file.chmod(0o600)  # lisible uniquement par l'utilisateur
    except OSError:
        pass


def construire_service(creds):
    """Construit le client googleapiclient pour Gmail v1."""
    from googleapiclient.discovery import build

    return build("gmail", "v1", credentials=creds, cache_discovery=False)


# ---------------------------------------------------------------------------
# Relances automatiques en cas d'erreur temporaire
# ---------------------------------------------------------------------------

T = TypeVar("T")


def _statut_http(exc: Exception) -> int | None:
    resp = getattr(exc, "resp", None)
    statut = getattr(resp, "status", None)
    try:
        return int(statut) if statut is not None else None
    except (TypeError, ValueError):
        return None


def avec_relances(appel: Callable[[], T], tentatives: int = 5, attente_base: float = 1.0,
                  dormir: Callable[[float], None] = time.sleep) -> T:
    """Exécute ``appel`` en relançant sur les erreurs 429 / 5xx (backoff exponentiel)."""
    for essai in range(1, tentatives + 1):
        try:
            return appel()
        except Exception as exc:  # noqa: BLE001 - on filtre juste après
            statut = _statut_http(exc)
            temporaire = statut == 429 or (statut is not None and 500 <= statut < 600)
            if not temporaire or essai == tentatives:
                raise
            delai = attente_base * (2 ** (essai - 1)) + random.uniform(0, 0.5)
            log.warning("Erreur Gmail %s, nouvelle tentative dans %.1fs (%d/%d).",
                        statut, delai, essai, tentatives)
            dormir(delai)
    raise RuntimeError("inaccessible")  # pragma: no cover


# ---------------------------------------------------------------------------
# Conversion d'un message Gmail (JSON de l'API) en objet Email
# ---------------------------------------------------------------------------

class _ExtracteurTexte(HTMLParser):
    """Convertit du HTML en texte lisible (sans dépendance externe)."""

    _BLOCS = {"p", "div", "br", "li", "tr", "h1", "h2", "h3", "h4", "h5", "h6", "table", "blockquote"}

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self._morceaux: list[str] = []
        self._ignore = 0

    def handle_starttag(self, tag, attrs):
        if tag in ("script", "style", "head"):
            self._ignore += 1
        elif tag in self._BLOCS:
            self._morceaux.append("\n")

    def handle_endtag(self, tag):
        if tag in ("script", "style", "head") and self._ignore:
            self._ignore -= 1
        elif tag in self._BLOCS:
            self._morceaux.append("\n")

    def handle_data(self, data):
        if not self._ignore:
            self._morceaux.append(data)

    def texte(self) -> str:
        brut = "".join(self._morceaux)
        # Normalise les espaces et supprime les lignes vides dues aux balises.
        lignes = (" ".join(l.split()) for l in brut.splitlines())
        return "\n".join(l for l in lignes if l)


def html_vers_texte(html: str) -> str:
    parseur = _ExtracteurTexte()
    parseur.feed(html)
    parseur.close()
    return parseur.texte()


def _decoder_base64url(donnees: str, charset: str = "utf-8") -> str:
    brut = base64.urlsafe_b64decode(donnees + "=" * (-len(donnees) % 4))
    try:
        return brut.decode(charset or "utf-8", errors="replace")
    except LookupError:  # charset inconnu
        return brut.decode("utf-8", errors="replace")


def _charset(partie: dict) -> str:
    for h in partie.get("headers", []) or []:
        if h.get("name", "").lower() == "content-type":
            for morceau in h.get("value", "").split(";"):
                morceau = morceau.strip()
                if morceau.lower().startswith("charset="):
                    return morceau.split("=", 1)[1].strip().strip('"')
    return "utf-8"


def _parcourir(partie: dict, textes: list[str], htmls: list[str], pj: list[str]) -> None:
    """Parcourt récursivement l'arbre MIME d'un message Gmail."""
    mime = (partie.get("mimeType") or "").lower()
    nom_fichier = partie.get("filename") or ""
    corps = partie.get("body") or {}

    if nom_fichier:  # pièce jointe : on garde seulement le nom
        pj.append(nom_fichier)
        return

    if mime.startswith("multipart/"):
        for sous_partie in partie.get("parts", []) or []:
            _parcourir(sous_partie, textes, htmls, pj)
        return

    donnees = corps.get("data")
    if not donnees:
        return
    if mime == "text/plain":
        textes.append(_decoder_base64url(donnees, _charset(partie)))
    elif mime == "text/html":
        htmls.append(_decoder_base64url(donnees, _charset(partie)))


def _adresses(valeur: str) -> tuple[str, ...]:
    return tuple(a.lower() for _, a in getaddresses([valeur]) if a)


def analyser_message(message: dict[str, Any]) -> Email:
    """Transforme la réponse JSON de ``users.messages.get(format='full')`` en ``Email``."""
    payload = message.get("payload", {}) or {}
    entetes_bruts = {h["name"].lower(): h.get("value", "") for h in payload.get("headers", []) or []}

    nom, adresse = parseaddr(entetes_bruts.get("from", ""))

    date = None
    if entetes_bruts.get("date"):
        try:
            date = parsedate_to_datetime(entetes_bruts["date"])
        except (TypeError, ValueError):
            date = None

    textes: list[str] = []
    htmls: list[str] = []
    pieces_jointes: list[str] = []
    _parcourir(payload, textes, htmls, pieces_jointes)

    if textes:
        corps = "\n\n".join(t.strip() for t in textes if t.strip())
    elif htmls:
        corps = "\n\n".join(html_vers_texte(h) for h in htmls)
    else:
        corps = message.get("snippet", "") or ""

    return Email(
        id=message["id"],
        thread_id=message.get("threadId", message["id"]),
        expediteur=adresse.lower(),
        nom_expediteur=nom,
        objet=entetes_bruts.get("subject", "").strip(),
        corps=corps.replace("\r\n", "\n").strip(),
        date=date,
        destinataires=_adresses(entetes_bruts.get("to", "")),
        cc=_adresses(entetes_bruts.get("cc", "")),
        message_id=entetes_bruts.get("message-id", ""),
        references=entetes_bruts.get("references", ""),
        extrait=message.get("snippet", "") or "",
        libelles=tuple(message.get("labelIds", []) or []),
        entetes={k: v for k, v in entetes_bruts.items() if k in ENTETES_UTILES},
        pieces_jointes=tuple(pieces_jointes),
    )


# ---------------------------------------------------------------------------
# Client Gmail
# ---------------------------------------------------------------------------

class GmailClient:
    """Fine surcouche autour du service googleapiclient.

    ``service`` est l'objet renvoyé par ``construire_service`` (ou un faux
    service dans les tests).
    """

    def __init__(self, service, user_id: str = "me",
                 dormir: Callable[[float], None] = time.sleep) -> None:
        self._service = service
        self._user = user_id
        self._dormir = dormir
        self._adresse: str | None = None

    def _executer(self, requete):
        return avec_relances(requete.execute, dormir=self._dormir)

    # -- Profil ---------------------------------------------------------------

    def profil(self) -> dict:
        return self._executer(self._service.users().getProfile(userId=self._user))

    def adresse_boite(self) -> str:
        """Adresse email de la boîte surveillée (mise en cache)."""
        if self._adresse is None:
            self._adresse = self.profil()["emailAddress"].lower()
        return self._adresse

    def history_id_courant(self) -> str:
        return str(self.profil()["historyId"])

    # -- Listage --------------------------------------------------------------

    def lister_ids_par_requete(self, requete: str, maximum: int = 500) -> list[str]:
        """Identifiants des messages correspondant à une requête Gmail (ex. 'in:inbox newer_than:1d')."""
        ids: list[str] = []
        jeton = None
        while len(ids) < maximum:
            reponse = self._executer(
                self._service.users().messages().list(
                    userId=self._user, q=requete, pageToken=jeton,
                    maxResults=min(100, maximum - len(ids)),
                )
            )
            ids.extend(m["id"] for m in reponse.get("messages", []) or [])
            jeton = reponse.get("nextPageToken")
            if not jeton:
                break
        return ids

    def lister_ids_depuis_historique(self, start_history_id: str) -> tuple[list[str], str]:
        """Messages arrivés dans INBOX depuis ``start_history_id``.

        Retourne ``(ids, nouveau_history_id)``. Lève ``HistoriqueExpire`` si
        Gmail ne connaît plus ce curseur (erreur 404).
        """
        ids: list[str] = []
        jeton = None
        dernier_history_id = start_history_id
        while True:
            try:
                reponse = self._executer(
                    self._service.users().history().list(
                        userId=self._user, startHistoryId=start_history_id,
                        historyTypes=["messageAdded"], labelId="INBOX", pageToken=jeton,
                    )
                )
            except Exception as exc:  # noqa: BLE001
                if _statut_http(exc) == 404:
                    raise HistoriqueExpire(start_history_id) from exc
                raise
            for entree in reponse.get("history", []) or []:
                for ajout in entree.get("messagesAdded", []) or []:
                    msg_id = ajout.get("message", {}).get("id")
                    if msg_id and msg_id not in ids:
                        ids.append(msg_id)
            dernier_history_id = str(reponse.get("historyId", dernier_history_id))
            jeton = reponse.get("nextPageToken")
            if not jeton:
                break
        return ids, dernier_history_id

    # -- Lecture --------------------------------------------------------------

    def lire_message_brut(self, message_id: str) -> dict:
        return self._executer(
            self._service.users().messages().get(userId=self._user, id=message_id, format="full")
        )

    def lire_message(self, message_id: str) -> Email:
        return analyser_message(self.lire_message_brut(message_id))
