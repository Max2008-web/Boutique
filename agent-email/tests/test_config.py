"""Tests du chargement et de la validation de la configuration."""

import shutil
from decimal import Decimal

import pytest
import yaml

from agent.config import ErreurConfig, charger_config, charger_environnement


def _copier_config(dossier_config, tmp_path):
    cible = tmp_path / "config"
    shutil.copytree(dossier_config, cible)
    return cible


def _modifier(dossier, fichier, fonction):
    chemin = dossier / fichier
    donnees = yaml.safe_load(chemin.read_text(encoding="utf-8"))
    fonction(donnees)
    chemin.write_text(yaml.safe_dump(donnees, allow_unicode=True), encoding="utf-8")


def test_config_demo_valide(config):
    assert config.entreprise.entreprise.nom == "Cabinet Durand & Associés"
    assert len(config.entreprise.faq) >= 5
    assert config.reglages.mode == "test"          # mode sûr par défaut
    assert config.offres.produit("pack_tpe").prix_ht == Decimal("190")
    assert config.offres.produit("inexistant") is None
    assert config.offres.remise("parrainage").pourcentage == Decimal("10")


def test_confirmation_envoi_active_par_defaut(config):
    assert config.reglages.notifications.confirmation_envoi is True


def test_fichier_manquant(tmp_path):
    with pytest.raises(ErreurConfig, match="introuvable"):
        charger_config(tmp_path)


def test_mode_inconnu_refuse(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "reglages.yaml", lambda d: d.update(mode="turbo"))
    with pytest.raises(ErreurConfig, match="reglages.yaml.*mode"):
        charger_config(cible)


def test_seuil_hors_bornes_refuse(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "reglages.yaml", lambda d: d.update(seuil_confiance=1.5))
    with pytest.raises(ErreurConfig, match="seuil_confiance"):
        charger_config(cible)


def test_heure_resume_invalide(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "reglages.yaml", lambda d: d["resume_quotidien"].update(heure="25:00"))
    with pytest.raises(ErreurConfig, match="heure"):
        charger_config(cible)


def test_email_responsable_invalide(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "reglages.yaml", lambda d: d["responsable"].update(email="pas-un-email"))
    with pytest.raises(ErreurConfig, match="email"):
        charger_config(cible)


def test_prix_negatif_refuse(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "offres.yaml", lambda d: d["produits"][0].update(prix_ht=-5))
    with pytest.raises(ErreurConfig, match="prix_ht"):
        charger_config(cible)


def test_remise_au_dessus_du_plafond_refusee(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "offres.yaml", lambda d: d["remises"]["autorisees"][0].update(pourcentage=25))
    with pytest.raises(ErreurConfig, match="dépasse le plafond"):
        charger_config(cible)


def test_identifiants_produits_en_double(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "offres.yaml", lambda d: d["produits"].append(dict(d["produits"][0])))
    with pytest.raises(ErreurConfig, match="double"):
        charger_config(cible)


def test_fuseau_horaire_inconnu(dossier_config, tmp_path):
    cible = _copier_config(dossier_config, tmp_path)
    _modifier(cible, "reglages.yaml", lambda d: d.update(fuseau_horaire="Mars/Olympus"))
    with pytest.raises(ErreurConfig, match="Fuseau"):
        charger_config(cible)


def test_environnement_chemins_relatifs(tmp_path, monkeypatch):
    monkeypatch.setenv("DB_PATH", "data/x.db")
    monkeypatch.setenv("MODEL", "modele-test")
    monkeypatch.delenv("ANTHROPIC_API_KEY", raising=False)
    env = charger_environnement(tmp_path)
    assert env.db_path == tmp_path / "data" / "x.db"
    assert env.model == "modele-test"
    assert env.anthropic_api_key is None


def test_aucun_secret_dans_la_config(dossier_config):
    """Garde-fou : les YAML ne doivent pas contenir de clé API."""
    for fichier in dossier_config.glob("*.yaml"):
        contenu = fichier.read_text(encoding="utf-8")
        assert "sk-ant-" not in contenu
        assert "client_secret" not in contenu
