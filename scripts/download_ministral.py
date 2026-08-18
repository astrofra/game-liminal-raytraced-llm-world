from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

try:
    from huggingface_hub import hf_hub_download
except ImportError as exc:  # pragma: no cover - dependency error path
    print(
        "ERROR: huggingface_hub is not installed. Run:\n"
        "  python -m pip install --upgrade huggingface_hub",
        file=sys.stderr,
    )
    raise SystemExit(1) from exc


REPO_ID = "mistralai/Ministral-3-8B-Instruct-2512-GGUF"
FILENAME = "Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
EXPECTED_SHA256 = (
    "33e7a72cf5e6e2cfc2f2847075acc013"
    "d68bba023e35310cef86b5cf8fdca761"
)
EXPECTED_SIZE = 5_198_911_904


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
        raise FileNotFoundError(f"Model file not found: {path}")

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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Download and validate the selected Ministral 3 8B GGUF model."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("models") / "ministral-3-8b",
        help="Directory where the GGUF file will be stored.",
    )
    parser.add_argument(
        "--force-download",
        action="store_true",
        help="Force a fresh download even if the file already exists locally.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    project_root = Path(__file__).resolve().parents[1]
    output_dir = (project_root / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Repository: {REPO_ID}")
    print(f"File:       {FILENAME}")
    print(f"Output:     {output_dir}")

    downloaded_path = Path(
        hf_hub_download(
            repo_id=REPO_ID,
            filename=FILENAME,
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
