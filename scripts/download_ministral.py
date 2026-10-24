from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path
from typing import Any

try:  # imported as part of the scripts package
    from .model_profiles import (
        DEFAULT_PROFILE,
        PROJECT_ROOT,
        get_profile,
        load_profiles,
        resolve_model_path,
    )
except ImportError:  # executed when the script is run directly
    from model_profiles import (
        DEFAULT_PROFILE,
        PROJECT_ROOT,
        get_profile,
        load_profiles,
        resolve_model_path,
    )


_DEFAULT_PROFILE_DATA = get_profile(DEFAULT_PROFILE)

# Compatibility aliases for the default profile; the manifest stays authoritative.
REPO_ID = _DEFAULT_PROFILE_DATA["repo_id"]
FILENAME = _DEFAULT_PROFILE_DATA["filename"]
EXPECTED_SHA256 = _DEFAULT_PROFILE_DATA["sha256"]
EXPECTED_SIZE = _DEFAULT_PROFILE_DATA["size_bytes"]


def sha256_file(path: Path, chunk_size: int = 16 * 1024 * 1024) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as stream:
        while True:
            chunk = stream.read(chunk_size)
            if not chunk:
                break
            digest.update(chunk)

    return digest.hexdigest()


def verify_model_file(path: Path, profile: dict[str, Any]) -> None:
    expected_size = profile["size_bytes"]
    expected_sha256 = profile["sha256"]

    if not path.is_file():
        raise FileNotFoundError(f"Model file not found: {path}")

    file_size = path.stat().st_size
    if file_size != expected_size:
        raise RuntimeError(
            f"Model file size mismatch: expected {expected_size:,} bytes, got {file_size:,} bytes"
        )

    with path.open("rb") as stream:
        magic = stream.read(4)

    if magic != b"GGUF":
        raise RuntimeError(f"Invalid GGUF signature: expected b'GGUF', got {magic!r}")

    print("Computing SHA-256. This may take a few minutes...")
    actual_sha256 = sha256_file(path)

    if actual_sha256.lower() != expected_sha256.lower():
        raise RuntimeError(
            "SHA-256 mismatch.\n"
            f"Expected: {expected_sha256}\n"
            f"Actual:   {actual_sha256}"
        )


def validate_gguf(path: Path) -> None:
    """Validate a model file against the default profile (compatibility helper)."""
    verify_model_file(Path(path), _DEFAULT_PROFILE_DATA)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    profiles = load_profiles()["profiles"]
    parser = argparse.ArgumentParser(
        description="Download and validate a Ministral 3 8B GGUF model profile."
    )
    parser.add_argument(
        "--profile",
        choices=sorted(profiles),
        default=DEFAULT_PROFILE,
        help="Model profile declared in scripts/ministral_models.json (default: %(default)s).",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help=(
            "Directory where the GGUF file will be stored. Defaults to the directory "
            "of the selected profile's relative_path in the project."
        ),
    )
    parser.add_argument(
        "--force-download",
        action="store_true",
        help="Force a fresh download even if the file already exists locally.",
    )
    parser.add_argument(
        "--verify-only",
        action="store_true",
        help="Validate an existing local file without any network access.",
    )
    return parser.parse_args(argv)


def _import_hf_hub_download():
    try:
        from huggingface_hub import hf_hub_download
    except ImportError as exc:  # pragma: no cover - dependency error path
        raise RuntimeError(
            "huggingface_hub is not installed. Run:\n"
            "  python -m pip install --upgrade huggingface_hub"
        ) from exc
    return hf_hub_download


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    profile = get_profile(args.profile)

    if args.output_dir is not None:
        output_dir = (PROJECT_ROOT / args.output_dir).resolve()
        model_path = output_dir / profile["filename"]
    else:
        model_path = resolve_model_path(args.profile)
        output_dir = model_path.parent

    print(f"Profile:    {args.profile}")
    print(f"Repository: {profile['repo_id']}")
    print(f"Revision:   {profile['revision']}")
    print(f"File:       {profile['filename']}")
    print(f"Size:       {profile['size_bytes']:,} bytes")
    print(f"Output:     {output_dir}")

    if args.verify_only:
        print("Verifying the local model file (no network access)...")
        verify_model_file(model_path, profile)
        print("Model validation successful.")
        return 0

    hf_hub_download = _import_hf_hub_download()
    output_dir.mkdir(parents=True, exist_ok=True)

    downloaded_path = Path(
        hf_hub_download(
            repo_id=profile["repo_id"],
            filename=profile["filename"],
            revision=profile["revision"],
            local_dir=str(output_dir),
            force_download=args.force_download,
        )
    ).resolve()

    print(f"Downloaded: {downloaded_path}")
    verify_model_file(downloaded_path, profile)
    print("Model validation successful.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
