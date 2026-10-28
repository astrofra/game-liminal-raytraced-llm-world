"""Prepare the native MLX C++ SDK and pinned Gemma weights (build tools only)."""
import argparse
import platform
from pathlib import Path
import subprocess
import sys
import venv
import urllib.request
import shutil

ROOT = Path(__file__).resolve().parents[1]


def prepare_native_model():
    from model_profiles import get_profile
    from download_gemma import validate_gguf
    profile = get_profile()
    model = ROOT / profile['relative_path']
    if model.exists():
        validate_gguf(model)
        print('Native model ready:', model)
        return
    model.parent.mkdir(parents=True, exist_ok=True)
    url = f"https://huggingface.co/{profile['repo_id']}/resolve/{profile['revision']}/{profile['filename']}"
    partial = model.with_suffix(model.suffix + '.partial')
    try:
        print(f'Downloading {profile["size_bytes"]:,} bytes from pinned revision…', flush=True)
        with urllib.request.urlopen(url, timeout=120) as response, partial.open('wb') as stream:
            shutil.copyfileobj(response, stream, 4 * 1024 * 1024)
        validate_gguf(partial)
        partial.replace(model)
    finally:
        partial.unlink(missing_ok=True)
    print('Native model ready:', model)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--backend', choices=('mlx-cpp', 'native', 'mlx-python'), default='mlx-cpp')
    parser.add_argument('--skip-model', action='store_true', help='Install dependencies only')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Native Apple Silicon macOS is required (no Rosetta).')
    if tuple(map(int, platform.mac_ver()[0].split('.')[:2])) < (14, 0):
        parser.error('macOS 14 or newer is required.')
    if args.backend == 'mlx-cpp':
        environment = ROOT / '.venv-mlx-cpp'
        if not (environment / 'bin/python').exists():
            venv.create(environment, with_pip=True)
        subprocess.run([str(environment / 'bin/python'), '-m', 'pip', 'install', 'mlx==0.32.4'], check=True)
        if not args.skip_model:
            subprocess.run([sys.executable, str(ROOT / 'scripts/download_mlx.py')], check=True)
            # GGUF is a build-time source for the small vocabulary-only artifact.
            from prepare_mlx_cpp import main as prepare_vocabulary, verified
            import json
            native_manifest = json.loads((ROOT / 'scripts/models_mlx_cpp.json').read_text())
            vocabulary = ROOT / native_manifest['relative_path'] / 'tokenizer.gguf'
            if not verified(vocabulary, native_manifest['files']['tokenizer.gguf']):
                prepare_native_model()
            prepare_vocabulary()
        return
    if args.backend == 'native':
        if not args.skip_model:
            prepare_native_model()
        return
    if sys.version_info[:2] != (3, 12):
        parser.error('Use python3.12 for the qualified MLX environment.')
    environment = ROOT / '.venv-mlx'
    if not (environment / 'bin/python').exists():
        venv.create(environment, with_pip=True)
    python = str(environment / 'bin/python')
    subprocess.run([python, '-m', 'pip', 'install', '-r', str(ROOT / 'scripts/requirements_mlx.txt')], check=True)
    subprocess.run([python, '-c', 'import mlx.core as mx; assert mx.metal.is_available(); print("MLX Metal ready:", mx.__version__)'], check=True)
    if not args.skip_model:
        subprocess.run([python, str(ROOT / 'scripts/download_mlx.py')], check=True)


if __name__ == '__main__':
    main()
