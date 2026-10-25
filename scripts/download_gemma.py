from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


from model_profiles import get_profile

_PROFILE = get_profile()
REPO_ID = _PROFILE["repo_id"]
REVISION = _PROFILE["revision"]
FILENAME = _PROFILE["filename"]
EXPECTED_SIZE = _PROFILE["size_bytes"]
EXPECTED_SHA256 = _PROFILE["sha256"]
PIP_INSTALL_COMMAND = "python -m pip install --upgrade huggingface_hub"


def sha256_file(path: Path, chunk_size: int = 16 * 1024 * 1024) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as stream:
        while True:
            chunk = stream.read(chunk_size)
            if not chunk:
                break
            digest.update(chunk)

    return digest.hexdigest()


def validate_gguf(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(
            f"Model file not found: {path}\n"
            "Run this script without --verify-only to download it."
        )

    file_size = path.stat().st_size
    if file_size != EXPECTED_SIZE:
        raise RuntimeError(
            f"Model file size mismatch: expected {EXPECTED_SIZE:,} bytes, got {file_size:,} bytes"
        )

    with path.open("rb") as stream:
        magic = stream.read(4)

    if magic != b"GGUF":
        raise RuntimeError(f"Invalid GGUF signature: expected b'GGUF', got {magic!r}")

    print("Computing SHA-256. This may take a few minutes...")
    actual_sha256 = sha256_file(path)

    if actual_sha256.lower() != EXPECTED_SHA256.lower():
        raise RuntimeError(
            "SHA-256 mismatch.\n"
            f"Expected: {EXPECTED_SHA256}\n"
            f"Actual:   {actual_sha256}"
        )


def import_hf_hub_download():
    try:
        from huggingface_hub import hf_hub_download
    except ImportError as exc:
        print(
            "ERROR: huggingface_hub is not installed, so the model cannot be downloaded.\n"
            "Install it with:\n"
            f"  {PIP_INSTALL_COMMAND}",
            file=sys.stderr,
        )
        raise SystemExit(1) from exc

    return hf_hub_download


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download and validate the selected Gemma 4 E4B GGUF model."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("models") / "gemma-4-e4b",
        help=(
            "Directory where the GGUF file will be stored. "
            "Relative paths are resolved against the project root."
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
        help=(
            "Only validate the already downloaded file (size, GGUF magic, SHA-256) "
            "without any network access."
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_root = Path(__file__).resolve().parents[1]
    output_dir = (project_root / args.output_dir).resolve()
    model_path = output_dir / FILENAME

    print(f"Repository: {REPO_ID}")
    print(f"Revision:   {REVISION}")
    print(f"File:       {FILENAME}")
    print(f"Output:     {output_dir}")

    if args.verify_only:
        print("Mode:       verify only (no network access)")
        validate_gguf(model_path)
        print("Model validation successful.")
        return 0

    hf_hub_download = import_hf_hub_download()
    output_dir.mkdir(parents=True, exist_ok=True)

    downloaded_path = Path(
        hf_hub_download(
            repo_id=REPO_ID,
            filename=FILENAME,
            revision=REVISION,
            local_dir=output_dir,
            force_download=args.force_download,
        )
    ).resolve()

    print(f"Downloaded: {downloaded_path}")
    validate_gguf(downloaded_path)
    print("Model validation successful.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
