"""Fixtures pytest partagées."""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

RACINE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(RACINE))

from agent.config import charger_config  # noqa: E402
from agent.db import Database  # noqa: E402


@pytest.fixture
def dossier_config() -> Path:
    """Dossier de config de démonstration fourni avec le projet."""
    return RACINE / "config"


@pytest.fixture
def config(dossier_config):
    return charger_config(dossier_config)


@pytest.fixture
def db(tmp_path) -> Database:
    return Database(tmp_path / "test.db")
