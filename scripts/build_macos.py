"""Build and optionally publish a local Apple Silicon .app using MLX inference.

The app uses the repository's MLX environment and model; it is not a standalone
redistributable. No signing identity, notarization or upload is performed.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def publish(build):
    app = ROOT / 'output/macos/Within the Latent Walls.app'
    contents = app / 'Contents'
    binary = contents / 'MacOS'
    resources = contents / 'Resources'
    binary.mkdir(parents=True, exist_ok=True)
    resources.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / 'liminal_cornell_renderer', binary / 'WithinTheLatentWalls')
    shutil.copytree(ROOT / 'assets', resources / 'assets', dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns('.DS_Store'))
    (resources / 'scripts').mkdir(exist_ok=True)
    for name in ('mlx_worker.py', 'models_mlx.json'):
        shutil.copy2(ROOT / 'scripts' / name, resources / 'scripts' / name)
    plist = dict(CFBundleName='Within the Latent Walls', CFBundleDisplayName='Within the Latent Walls',
                 CFBundleIdentifier='org.astrofra.within-the-latent-walls',
                 CFBundleExecutable='WithinTheLatentWalls', CFBundlePackageType='APPL',
                 CFBundleShortVersionString='3.0.1', CFBundleVersion='1',
                 LSMinimumSystemVersion='14.0', NSHighResolutionCapable=True)
    with (contents / 'Info.plist').open('wb') as output:
        plistlib.dump(plist, output)
    # Ad-hoc signing is local only; no developer account/credential is used.
    subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(app)], check=True)
    print(f'Local application: {app}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build_macos')
    parser.add_argument('--parallel', type=int, default=min(8, os.cpu_count() or 4))
    parser.add_argument('--publish', action='store_true')
    parser.add_argument('--test', action='store_true')
    args = parser.parse_args()
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        parser.error('Use native Apple Silicon macOS, not Rosetta.')
    if args.parallel < 1:
        parser.error('--parallel must be positive')
    for tool in ('cmake', 'ninja', 'clang++'):
        if not shutil.which(tool):
            parser.error(f'Missing {tool}; install Xcode Command Line Tools, CMake and Ninja.')
    python = ROOT / '.venv-mlx/bin/python'
    if not python.exists():
        parser.error('Run python3.12 scripts/setup_macos.py first.')
    build = args.build_dir.resolve()
    command = ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
               '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
               '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DLIMINAL_ENABLE_MLX=ON',
               '-DLIMINAL_ENABLE_LLAMA_CPP=OFF', '-DLIMINAL_ENABLE_LLAMA_CUDA=OFF',
               f'-DLIMINAL_MLX_PYTHON={python}',
               f'-DLIMINAL_MLX_MODEL_DIR={ROOT / "models/mlx/gemma-4-e4b-it-4bit"}']
    # Apple Clang needs explicit libomp discovery. Optional: builds remain valid without it.
    libomp = Path('/opt/homebrew/opt/libomp')
    if (libomp / 'lib/libomp.dylib').exists():
        command.extend(['-DOpenMP_CXX_FLAGS=-Xpreprocessor -fopenmp',
                        '-DOpenMP_CXX_LIB_NAMES=omp',
                        f'-DOpenMP_omp_LIBRARY={libomp / "lib/libomp.dylib"}',
                        f'-DOpenMP_CXX_INCLUDE_DIR={libomp / "include"}'])
    subprocess.run(command, check=True)
    subprocess.run(['cmake', '--build', str(build), '--parallel', str(args.parallel)], check=True)
    if args.test:
        subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True)
    provenance = {'architecture': platform.machine(), 'macos': platform.mac_ver()[0],
                  'backend': 'mlx', 'python': str(python),
                  'model_manifest': json.loads((ROOT / 'scripts/models_mlx.json').read_text())}
    (build / 'runtime-build.json').write_text(json.dumps(provenance, indent=2) + '\n')
    if args.publish:
        publish(build)


if __name__ == '__main__':
    main()
