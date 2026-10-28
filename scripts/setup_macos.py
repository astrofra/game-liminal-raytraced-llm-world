"""Prepare a repository-local MLX environment and pinned Gemma weights."""
import argparse
import platform
from pathlib import Path
import subprocess
import sys
import venv

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--skip-model', action='store_true', help='Install dependencies only')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Native Apple Silicon macOS is required (no Rosetta).')
    if tuple(map(int, platform.mac_ver()[0].split('.')[:2])) < (14, 0):
        parser.error('macOS 14 or newer is required.')
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
