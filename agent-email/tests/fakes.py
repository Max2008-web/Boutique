"""Faux objets pour tester l'agent sans réseau ni compte Gmail réel."""

from __future__ import annotations

import base64
from email.utils import format_datetime
from datetime import datetime, timezone


def b64(texte: str, charset: str = "utf-8") -> str:
    return base64.urlsafe_b64encode(texte.encode(charset)).decode().rstrip("=")


def fabriquer_message(
    msg_id: str,
    expediteur: str = "Jean Martin <jean.martin@client.example>",
    objet: str = "Question",
    texte: str | None = "Bonjour, quels sont vos horaires ?",
    html: str | None = None,
    destinataire: str = "contact@cabinet-durand.example",
    libelles: tuple[str, ...] = ("INBOX", "UNREAD"),
    entetes: dict[str, str] | None = None,
    date: datetime | None = None,
    thread_id: str | None = None,
    pieces_jointes: tuple[str, ...] = (),
) -> dict:
    """Construit un message au format JSON renvoyé par l'API Gmail (format='full')."""
    date = date or datetime(2026, 11, 13, 9, 0, tzinfo=timezone.utc)
    headers = [
        {"name": "From", "value": expediteur},
        {"name": "To", "value": destinataire},
        {"name": "Subject", "value": objet},
        {"name": "Date", "value": format_datetime(date)},
        {"name": "Message-ID", "value": f"<{msg_id}@client.example>"},
    ]
    for nom, valeur in (entetes or {}).items():
        headers.append({"name": nom, "value": valeur})

    parties = []
    if texte is not None:
        parties.append({"mimeType": "text/plain",
                        "headers": [{"name": "Content-Type", "value": 'text/plain; charset="UTF-8"'}],
                        "body": {"data": b64(texte)}})
    if html is not None:
        parties.append({"mimeType": "text/html",
                        "headers": [{"name": "Content-Type", "value": "text/html; charset=UTF-8"}],
                        "body": {"data": b64(html)}})
    for nom in pieces_jointes:
        parties.append({"mimeType": "application/pdf", "filename": nom,
                        "body": {"attachmentId": "pj-" + nom, "size": 1234}})

    payload = {"mimeType": "multipart/mixed", "headers": headers, "parts": parties}
    return {
        "id": msg_id,
        "threadId": thread_id or f"t-{msg_id}",
        "labelIds": list(libelles),
        "snippet": (texte or "")[:100],
        "payload": payload,
    }


# ---------------------------------------------------------------------------
# Faux service googleapiclient (reproduit la chaîne users().messages().get()…)
# ---------------------------------------------------------------------------

class ErreurHttpFactice(Exception):
    """Imite googleapiclient.errors.HttpError (attribut resp.status)."""

    def __init__(self, statut: int) -> None:
        super().__init__(f"HTTP {statut}")
        self.resp = type("Resp", (), {"status": statut})()


class _Requete:
    def __init__(self, fonction):
        self._fonction = fonction

    def execute(self):
        return self._fonction()


class FauxServiceGmail:
    """Boîte Gmail en mémoire."""

    def __init__(self, adresse: str = "contact@cabinet-durand.example", history_id: str = "1000"):
        self.adresse = adresse
        self.history_id = history_id
        self.boite: dict[str, dict] = {}
        self.historique: list[dict] = []          # entrées renvoyées par history.list
        self.historique_expire = False
        self.erreurs_a_lever: list[int] = []      # statuts HTTP à lever avant de répondre
        self.appels: list[str] = []

    # Utilitaires de test
    def ajouter(self, message: dict, dans_historique: bool = True) -> None:
        self.boite[message["id"]] = message
        if dans_historique:
            self.history_id = str(int(self.history_id) + 1)
            self.historique.append({
                "id": self.history_id,
                "messagesAdded": [{"message": {"id": message["id"], "labelIds": message["labelIds"]}}],
            })

    def _peut_echouer(self, fonction):
        def wrapper():
            if self.erreurs_a_lever:
                raise ErreurHttpFactice(self.erreurs_a_lever.pop(0))
            return fonction()
        return _Requete(wrapper)

    # Chaîne de l'API
    def users(self):
        return self

    def getProfile(self, userId):
        self.appels.append("getProfile")
        return self._peut_echouer(lambda: {"emailAddress": self.adresse, "historyId": self.history_id})

    def messages(self):
        return _FauxMessages(self)

    def history(self):
        return _FauxHistory(self)


class _FauxMessages:
    def __init__(self, svc: FauxServiceGmail):
        self.svc = svc

    def list(self, userId, q=None, pageToken=None, maxResults=100):
        self.svc.appels.append(f"messages.list:{q}")
        ids = [{"id": i, "threadId": m["threadId"]} for i, m in self.svc.boite.items()
               if "INBOX" in m["labelIds"]]
        debut = int(pageToken or 0)
        page = ids[debut: debut + maxResults]
        suivant = debut + maxResults
        reponse = {"messages": page} if page else {}
        if suivant < len(ids):
            reponse["nextPageToken"] = str(suivant)
        return self.svc._peut_echouer(lambda: reponse)

    def get(self, userId, id, format="full"):
        self.svc.appels.append(f"messages.get:{id}")
        return self.svc._peut_echouer(lambda: self.svc.boite[id])


class _FauxHistory:
    def __init__(self, svc: FauxServiceGmail):
        self.svc = svc

    def list(self, userId, startHistoryId, historyTypes=None, labelId=None, pageToken=None):
        self.svc.appels.append(f"history.list:{startHistoryId}")
        if self.svc.historique_expire:
            def expire():
                raise ErreurHttpFactice(404)
            return _Requete(expire)
        entrees = [h for h in self.svc.historique if int(h["id"]) > int(startHistoryId)]
        return self.svc._peut_echouer(lambda: {"history": entrees, "historyId": self.svc.history_id})
