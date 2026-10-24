"""Versioned Ministral model profiles: loading, validation and path resolution.

The module uses the standard library only and performs no network access, neither
at import time nor when its functions are called.
"""

from __future__ import annotations

import json
import math
import re
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "scripts" / "ministral_models.json"
SCHEMA_VERSION = 1

_REVISION_RE = re.compile(r"\A[0-9a-fA-F]{40}\Z")
_SHA256_RE = re.compile(r"\A[0-9a-fA-F]{64}\Z")
_WINDOWS_DRIVE_RE = re.compile(r"\A[A-Za-z]:")


class ModelProfileError(ValueError):
    """Raised when the manifest or one of its profiles fails validation."""


def _as_mapping(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ModelProfileError(f"{label} must be a JSON object, got {type(value).__name__}")
    return value


def _as_text(mapping: dict[str, Any], key: str, label: str) -> str:
    value = mapping.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ModelProfileError(f"{label}.{key} must be a non-empty string")
    return value


def _as_int(mapping: dict[str, Any], key: str, label: str, *, positive: bool = False) -> int:
    value = mapping.get(key)
    if isinstance(value, bool) or not isinstance(value, int):
        raise ModelProfileError(f"{label}.{key} must be an integer")
    if positive and value <= 0:
        raise ModelProfileError(f"{label}.{key} must be strictly positive, got {value}")
    return value


def _as_number(mapping: dict[str, Any], key: str, label: str) -> float:
    value = mapping.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ModelProfileError(f"{label}.{key} must be a number")
    number = float(value)
    if not math.isfinite(number):
        raise ModelProfileError(f"{label}.{key} must be finite")
    return number


def _as_bool(mapping: dict[str, Any], key: str, label: str) -> bool:
    value = mapping.get(key)
    if not isinstance(value, bool):
        raise ModelProfileError(f"{label}.{key} must be a boolean")
    return value


def _validate_relative_path(relative_path: str, label: str) -> None:
    if (
        relative_path.startswith("/")
        or "\\" in relative_path
        or ":" in relative_path
        or _WINDOWS_DRIVE_RE.match(relative_path)
    ):
        raise ModelProfileError(
            f"{label}.relative_path must be a portable project-relative path using '/', "
            f"got {relative_path!r}"
        )

    for part in relative_path.split("/"):
        if part in {"", ".", ".."}:
            raise ModelProfileError(
                f"{label}.relative_path must not contain empty, '.' or '..' segments, "
                f"got {relative_path!r}"
            )


def _validate_profile(name: str, profile: Any) -> None:
    label = f"profiles[{name!r}]"
    data = _as_mapping(profile, label)

    _as_text(data, "repo_id", label)

    revision = _as_text(data, "revision", label)
    if not _REVISION_RE.match(revision):
        raise ModelProfileError(
            f"{label}.revision must be a 40-character hexadecimal commit id, got {revision!r}"
        )

    filename = _as_text(data, "filename", label)
    if (
        _WINDOWS_DRIVE_RE.match(filename)
        or "/" in filename
        or "\\" in filename
        or filename in {".", ".."}
    ):
        raise ModelProfileError(f"{label}.filename must be a bare file name, got {filename!r}")

    _as_int(data, "size_bytes", label, positive=True)

    sha256 = _as_text(data, "sha256", label)
    if not _SHA256_RE.match(sha256):
        raise ModelProfileError(
            f"{label}.sha256 must be a 64-character hexadecimal digest, got {sha256!r}"
        )

    relative_path = _as_text(data, "relative_path", label)
    _validate_relative_path(relative_path, label)
    if relative_path.rsplit("/", 1)[-1] != filename:
        raise ModelProfileError(
            f"{label}.relative_path must end with filename {filename!r}, got {relative_path!r}"
        )

    _as_bool(data, "reasoning", label)

    temperature = _as_number(data, "temperature", label)
    if temperature < 0:
        raise ModelProfileError(f"{label}.temperature must be >= 0, got {temperature}")

    _as_int(data, "top_k", label, positive=True)

    top_p = _as_number(data, "top_p", label)
    if not 0.0 <= top_p <= 1.0:
        raise ModelProfileError(f"{label}.top_p must be within [0, 1], got {top_p}")


def load_profiles(path: str | Path | None = None) -> dict[str, Any]:
    """Load and validate the manifest, returning its full contents.

    ``path`` defaults to ``scripts/ministral_models.json``; a relative path is
    resolved against :data:`PROJECT_ROOT`.
    """
    manifest_path = Path(path) if path is not None else MANIFEST_PATH
    manifest_path = manifest_path.expanduser()
    if not manifest_path.is_absolute():
        manifest_path = PROJECT_ROOT / manifest_path
    manifest_path = manifest_path.resolve()

    if not manifest_path.is_file():
        raise FileNotFoundError(f"Model profile manifest not found: {manifest_path}")

    try:
        raw = json.loads(manifest_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ModelProfileError(
            f"Model profile manifest is not valid JSON: {manifest_path}: {exc}"
        ) from exc

    manifest = _as_mapping(raw, "manifest")

    schema_version = manifest.get("schema_version")
    if isinstance(schema_version, bool) or not isinstance(schema_version, int):
        raise ModelProfileError("manifest.schema_version must be an integer")
    if schema_version != SCHEMA_VERSION:
        raise ModelProfileError(
            f"Unsupported manifest schema_version {schema_version}; expected {SCHEMA_VERSION}"
        )

    default_profile = _as_text(manifest, "default_profile", "manifest")

    profiles = _as_mapping(manifest.get("profiles"), "manifest.profiles")
    if not profiles:
        raise ModelProfileError("manifest.profiles must declare at least one profile")

    for name, profile in profiles.items():
        if not isinstance(name, str) or not name.strip():
            raise ModelProfileError("manifest.profiles keys must be non-empty profile identifiers")
        _validate_profile(name, profile)

    if default_profile not in profiles:
        raise ModelProfileError(
            f"manifest.default_profile {default_profile!r} is not declared in manifest.profiles"
        )

    return manifest


DEFAULT_PROFILE = load_profiles()["default_profile"]


def get_profile(name: str = DEFAULT_PROFILE) -> dict[str, Any]:
    """Return the profile entry declared as ``name`` in the manifest."""
    if not isinstance(name, str) or not name:
        raise ValueError(f"Model profile name must be a non-empty string, got {name!r}")

    profiles = load_profiles()["profiles"]
    try:
        return profiles[name]
    except KeyError:
        available = ", ".join(sorted(profiles))
        raise ValueError(
            f"Unknown model profile {name!r}. Available profiles: {available}"
        ) from None


def resolve_model_path(name: str = DEFAULT_PROFILE, root: str | Path = PROJECT_ROOT) -> Path:
    """Return the absolute path of the model file for profile ``name`` under ``root``."""
    profile = get_profile(name)
    root_path = Path(root).expanduser().resolve()
    return root_path.joinpath(*profile["relative_path"].split("/")).resolve()
