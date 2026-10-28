"""Build native C++/Metal and publish a standalone Apple Silicon application.

Use --backend mlx-python only to rebuild the historical development reference.
Python runs during the build, never in the native game.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def publish_mlx_reference(build):
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
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--backend', choices=('native', 'mlx-python'), default='native')
    parser.add_argument('--skip-build', action='store_true', help='Publish an already built binary')
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
    if args.backend == 'mlx-python' and not python.exists():
        parser.error('Run python3.12 scripts/setup_macos.py --backend mlx-python first.')
    build = (args.build_dir or ROOT / ('build_macos_native' if args.backend == 'native' else 'build_macos')).resolve()
    command = ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
               '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
               '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
               f'-DLIMINAL_ENABLE_MLX={"OFF" if args.backend == "native" else "ON"}',
               f'-DLIMINAL_ENABLE_LLAMA_CPP={"ON" if args.backend == "native" else "OFF"}',
               '-DLIMINAL_ENABLE_LLAMA_CUDA=OFF', '-DGGML_METAL=ON',
               '-DGGML_METAL_EMBED_LIBRARY=ON', '-DGGML_ACCELERATE=ON',
               f'-DLIMINAL_MLX_PYTHON={python}',
               f'-DLIMINAL_MLX_MODEL_DIR={ROOT / "models/mlx/gemma-4-e4b-it-4bit"}']
    # Apple Clang needs explicit libomp discovery. Optional: builds remain valid without it.
    libomp = Path('/opt/homebrew/opt/libomp')
    if (libomp / 'lib/libomp.dylib').exists():
        command.extend(['-DOpenMP_CXX_FLAGS=-Xpreprocessor -fopenmp',
                        '-DOpenMP_CXX_LIB_NAMES=omp',
                        f'-DOpenMP_omp_LIBRARY={libomp / "lib/libomp.dylib"}',
                        f'-DOpenMP_CXX_INCLUDE_DIR={libomp / "include"}'])
    if not args.skip_build:
        subprocess.run(command, check=True)
        subprocess.run(['cmake', '--build', str(build), '--parallel', str(args.parallel)], check=True)
    if args.test:
        subprocess.run(['ctest', '--test-dir', str(build), '--output-on-failure'], check=True)
    provenance_path = build / 'runtime-build.json'
    binary_sha256 = hashlib.sha256((build / 'liminal_cornell_renderer').read_bytes()).hexdigest()
    backend = 'llama.cpp-metal' if args.backend == 'native' else 'mlx-python'
    if args.skip_build:
        provenance = json.loads(provenance_path.read_text())
        if provenance.get('source_binary_sha256') != binary_sha256 or provenance.get('backend') != backend:
            parser.error('Binary provenance does not match; rerun without --skip-build.')
    else:
        provenance = {'architecture': platform.machine(), 'macos': platform.mac_ver()[0],
                      'backend': 'llama.cpp-metal' if args.backend == 'native' else 'mlx-python',
                      'source_binary_sha256': binary_sha256,
                      'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
                      'llama_cpp_commit': (ROOT / 'vendor/llama.cpp/VENDORED_COMMIT.txt').read_text().strip(),
                      'model_manifest': json.loads((ROOT / 'scripts' / ('models.json' if args.backend == 'native' else 'models_mlx.json')).read_text())}
        provenance_path.write_text(json.dumps(provenance, indent=2) + '\n')
    if args.publish:
        if args.backend == 'native':
            from package_macos import package
            package(build, ROOT / 'output/macos-native/Within the Latent Walls.app')
        else:
            publish_mlx_reference(build)


if __name__ == '__main__':
    main()
