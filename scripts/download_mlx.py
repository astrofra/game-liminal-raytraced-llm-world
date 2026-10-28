"""Download the pinned MLX model; verify every file before atomic replacement."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = Path(__file__).with_name('models_mlx.json')


def verified(path, spec):
    if not path.is_file() or path.stat().st_size != spec['size_bytes']:
        return False
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest() == spec['sha256']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    parser.add_argument('--destination', type=Path)
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    destination = args.destination or ROOT / manifest['relative_path']
    for name, spec in manifest['files'].items():
        target = destination / name
        if verified(target, spec):
            print(f'Verified {name}', flush=True)
            continue
        if args.verify_only:
            raise SystemExit(f'Missing or corrupt model file: {target}')
        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_name(target.name + '.partial')
        url = f"https://huggingface.co/{manifest['repo_id']}/resolve/{manifest['revision']}/{name}"
        print(f'Downloading {name} ({spec["size_bytes"]:,} bytes)', flush=True)
        try:
            with urllib.request.urlopen(url, timeout=60) as source, partial.open('wb') as output:
                while chunk := source.read(4 * 1024 * 1024):
                    output.write(chunk)
            if not verified(partial, spec):
                raise RuntimeError(f'SHA-256/size mismatch: {name}')
            partial.replace(target)
        finally:
            partial.unlink(missing_ok=True)
    print(f'MLX model ready: {destination}')


if __name__ == '__main__':
    main()
