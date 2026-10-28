"""Build a relocatable native .app with verified GGUF and recursive dylib closure.

Python is a build tool only. The bundle contains no interpreter, worker or venv.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SYSTEM_PREFIXES = ('/System/Library/', '/usr/lib/')


def output(command):
    return subprocess.check_output([str(arg) for arg in command], text=True)


def dependencies(binary):
    return [line.strip().split(' (compatibility version', 1)[0]
            for line in output(['otool', '-L', binary]).splitlines()[1:]]


def rpaths(binary):
    result = []
    lines = output(['otool', '-l', binary]).splitlines()
    for index, line in enumerate(lines):
        if line.strip() == 'cmd LC_RPATH':
            result.append(lines[index + 2].strip().split(' ', 1)[1].rsplit(' (offset', 1)[0])
    return result


def resolve_dependency(name, loader, executable, search_paths):
    def expand(value):
        return value.replace('@loader_path', str(loader.parent)).replace('@executable_path', str(executable.parent))
    if name.startswith('@rpath/'):
        candidates = [Path(expand(path)) / name[len('@rpath/'):] for path in search_paths]
    else:
        candidates = [Path(expand(name))]
    for path in candidates:
        if path.is_file():
            return path.resolve()
    raise RuntimeError(f'Unresolved non-system dependency {name} in {loader}')


def bundle_libraries(source_executable, executable, frameworks):
    """Copy each non-system library once, rewrite every edge, then audit closure."""
    frameworks.mkdir(parents=True, exist_ok=True)
    source_executable = source_executable.resolve()
    copied = {source_executable: executable}
    basenames = {}
    queue = [source_executable]
    executable_rpaths = rpaths(source_executable)
    while queue:
        source = queue.pop(0)
        destination = copied[source]
        search = rpaths(source) + executable_rpaths
        for dependency in dependencies(source):
            if dependency.startswith(SYSTEM_PREFIXES):
                continue
            resolved = resolve_dependency(dependency, source, source_executable, search)
            if resolved == source:  # dylib's own LC_ID_DYLIB
                continue
            if resolved not in copied:
                name = resolved.name
                if name in basenames and basenames[name] != resolved:
                    raise RuntimeError(f'Conflicting dylib names: {name}')
                basenames[name] = resolved
                copied[resolved] = frameworks / name
                shutil.copy2(resolved, copied[resolved])
                copied[resolved].chmod(0o755)
                queue.append(resolved)
            target = '@executable_path/../Frameworks/' + copied[resolved].name
            subprocess.run(['install_name_tool', '-change', dependency, target, str(destination)], check=True)
        if source != source_executable:
            subprocess.run(['install_name_tool', '-id', '@executable_path/../Frameworks/' + destination.name,
                            str(destination)], check=True)
        for path in rpaths(destination):
            subprocess.run(['install_name_tool', '-delete_rpath', path, str(destination)], check=True)
    audit_libraries(executable, frameworks)
    return copied


def audit_libraries(executable, frameworks):
    allowed_prefix = '@executable_path/../Frameworks/'
    for binary in [executable, *frameworks.glob('*.dylib')]:
        if 'arm64' not in output(['lipo', '-archs', binary]).split():
            raise RuntimeError(f'Missing arm64 architecture: {binary}')
        if rpaths(binary):
            raise RuntimeError(f'Unexpected rpath in bundle: {binary}')
        for dependency in dependencies(binary):
            if dependency.startswith(SYSTEM_PREFIXES):
                continue
            if not dependency.startswith(allowed_prefix):
                raise RuntimeError(f'External dependency remains in {binary}: {dependency}')
            if not (frameworks / dependency.removeprefix(allowed_prefix)).is_file():
                raise RuntimeError(f'Bundled dependency missing: {dependency}')


def copy_notices(resources, build, libraries):
    notices = resources / 'licenses'
    notices.mkdir()
    shutil.copy2(ROOT / 'LICENSE', resources / 'LICENSE.txt')
    notice = (ROOT / 'packaging/MODEL_NOTICE.txt').read_text().replace('licenses/third-party/', 'licenses/')
    (resources / 'MODEL_NOTICE.txt').write_text(notice)
    # CMake records the actual SDL_ttf source, including a supplied FetchContent source.
    cache = (build / 'CMakeCache.txt').read_text()
    match = re.search(r'^SDL3_ttf_SOURCE_DIR:STATIC=(.+)$', cache, re.M)
    ttf = Path(match.group(1)) if match else build / '_deps/sdl3_ttf-src'
    files = {
        'Gemma-LICENSE.txt': ROOT / 'packaging/Gemma-LICENSE.txt',
        'llama.cpp-LICENSE.txt': ROOT / 'vendor/llama.cpp/LICENSE',
        'stb-vorbis-LICENSE.txt': ROOT / 'vendor/stb/VORBIS-LICENSE',
        'SDL3_ttf-LICENSE.txt': ttf / 'LICENSE.txt',
        'FreeType-LICENSE.txt': ttf / 'external/freetype/LICENSE.TXT',
        'FreeType-FTL.txt': ttf / 'external/freetype/docs/FTL.TXT',
        'HarfBuzz-COPYING.txt': ttf / 'external/harfbuzz/COPYING',
        'PlutoSVG-LICENSE.txt': ttf / 'external/plutosvg/LICENSE',
        'PlutoVG-LICENSE.txt': ttf / 'external/plutovg/LICENSE',
        'Zilla-Slab-OFL.txt': ROOT / 'assets/fonts/Zilla_Slab/OFL.txt',
        'Zilla-Slab-Highlight-OFL.txt': ROOT / 'assets/fonts/Zilla_Slab_Highlight/OFL.txt',
    }
    for library in libraries:
        if library.name.startswith('libSDL3'):
            files['SDL3-LICENSE.txt'] = library.parent.parent / 'LICENSE.txt'
        if library.name.startswith('libomp'):
            files['libomp-LICENSE.txt'] = library.parent.parent / 'LICENSE.TXT'
    if 'SDL3-LICENSE.txt' not in files:
        files['SDL3-LICENSE.txt'] = build / '_deps/sdl3-src/LICENSE.txt'
    for name, source in files.items():
        if not source.is_file():
            raise FileNotFoundError(f'Required notice not found: {source}')
        shutil.copy2(source, notices / name)


def package(build, destination, backend='native'):
    from model_profiles import get_profile
    from download_gemma import validate_gguf
    native_mlx = backend == 'mlx-cpp'
    profile = json.loads((ROOT / 'scripts/models_mlx_cpp.json').read_text()) if native_mlx else get_profile()
    source_model = ROOT / profile['relative_path']
    if native_mlx:
        from prepare_mlx_cpp import verified
        for name, spec in profile['files'].items():
            if not verified(source_model / name, spec):
                raise RuntimeError(f'Native MLX artifact missing or corrupt: {name}')
    else:
        validate_gguf(source_model)
    source_executable = build / 'liminal_cornell_renderer'
    info = output([source_executable, '--llama-info'])
    expected = 'MLX C++ 0.32.4' if native_mlx else 'MLX compiled in: no'
    if 'llama.cpp compiled in: yes' not in info or expected not in info or 'GPU offload available: yes' not in info:
        raise RuntimeError('Publishing requires the selected native C++/Metal build')
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Assemble beside the destination; a failed package never replaces the previous app.
    with tempfile.TemporaryDirectory(prefix='.native-stage-', dir=destination.parent) as temporary:
        app = Path(temporary) / destination.name
        contents = app / 'Contents'
        resources = contents / 'Resources'
        executable = contents / 'MacOS/WithinTheLatentWalls'
        executable.parent.mkdir(parents=True)
        resources.mkdir()
        shutil.copy2(source_executable, executable)
        libraries = bundle_libraries(source_executable, executable, contents / 'Frameworks')
        shutil.copytree(ROOT / 'assets', resources / 'assets', ignore=shutil.ignore_patterns('.DS_Store'))
        model = resources / profile['relative_path']
        model.parent.mkdir(parents=True)
        if native_mlx:
            model.mkdir()
            for name, spec in profile['files'].items():
                shutil.copy2(source_model / name, model / name)
                if not verified(model / name, spec):
                    raise RuntimeError(f'Packaged MLX artifact failed verification: {name}')
            mlx_library = next(source for source in libraries if source.name == 'libmlx.dylib')
            shutil.copy2(mlx_library.parent / 'mlx.metallib', resources / 'mlx.metallib')
        else:
            shutil.copy2(source_model, model)
            validate_gguf(model)
        copy_notices(resources, build, libraries)
        if native_mlx:
            shutil.copy2(ROOT / 'packaging/MODEL_NOTICE_MLX_CPP.txt', resources / 'MODEL_NOTICE.txt')
            for name in ('MLX-LICENSE.txt', 'MLX-LM-LICENSE.txt'):
                shutil.copy2(ROOT / 'packaging' / name, resources / 'licenses' / name)
        provenance = json.loads((build / 'runtime-build.json').read_text())
        provenance['model'] = profile
        if native_mlx:
            provenance['mlx_metallib_sha256'] = hashlib.sha256((resources / 'mlx.metallib').read_bytes()).hexdigest()
        plist = dict(CFBundleName='Within the Latent Walls', CFBundleDisplayName='Within the Latent Walls',
                     CFBundleIdentifier='org.astrofra.within-the-latent-walls',
                     CFBundleExecutable='WithinTheLatentWalls', CFBundlePackageType='APPL',
                     CFBundleShortVersionString='3.0.2', CFBundleVersion='2',
                     LSMinimumSystemVersion='14.0', NSHighResolutionCapable=True)
        with (contents / 'Info.plist').open('wb') as stream:
            plistlib.dump(plist, stream)
        for library in (contents / 'Frameworks').glob('*.dylib'):
            subprocess.run(['codesign', '--force', '--sign', '-', str(library)], check=True)
        provenance['libraries'] = {target.name: hashlib.sha256(target.read_bytes()).hexdigest()
                                   for source, target in libraries.items() if source != source_executable.resolve()}
        (resources / 'runtime-build.json').write_text(json.dumps(provenance, indent=2) + '\n')
        subprocess.run(['codesign', '--force', '--sign', '-', str(app)], check=True)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
        backup = destination.with_name(destination.name + '.previous')
        if backup.exists():
            raise FileExistsError(f'Inspect previous package before replacing: {backup}')
        if destination.exists():
            destination.rename(backup)
        try:
            app.rename(destination)
        except Exception:
            if backup.exists():
                backup.rename(destination)
            raise
        if backup.exists():
            shutil.rmtree(backup)
    print(f'Standalone native application: {destination}', flush=True)
