from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import urllib.request
import zipfile
from pathlib import Path

if __package__:
    from .model_profiles import DEFAULT_PROFILE, get_profile
else:
    # Direct execution (``python scripts/build_installer.py``) puts scripts/ on sys.path.
    from model_profiles import DEFAULT_PROFILE, get_profile


PROJECT_ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = PROJECT_ROOT / "build"
RELEASE_DIR = BUILD_DIR / "Release"
SOURCE_EXE = RELEASE_DIR / "liminal_cornell_renderer.exe"
INSTALLER_ROOT = PROJECT_ROOT / "output" / "installer"
STAGE_DIR = INSTALLER_ROOT / "stage"
DIST_DIR = INSTALLER_ROOT / "dist"
TOOLS_DIR = INSTALLER_ROOT / "tools"
PACKAGING_DIR = PROJECT_ROOT / "packaging"
INNO_SCRIPT = PACKAGING_DIR / "within_the_latent_walls.iss"
STAGED_EXE_NAME = "WithinTheLatentWalls.exe"

MODEL_PROFILE = get_profile()
MODEL_PROFILE_NAME = DEFAULT_PROFILE
MODEL_REPOSITORY = MODEL_PROFILE["repo_id"]
MODEL_REVISION = MODEL_PROFILE["revision"]
MODEL_FILENAME = MODEL_PROFILE["filename"]
MODEL_SIZE = MODEL_PROFILE["size_bytes"]
MODEL_SHA256 = MODEL_PROFILE["sha256"]
MODEL_RELATIVE_PATH = MODEL_PROFILE["relative_path"]

INNO_VERSION = "6.7.3"
INNO_URL = (
    "https://github.com/jrsoftware/issrc/releases/download/"
    "is-6_7_3/innosetup-6.7.3.exe"
)
INNO_SHA256 = "9c73c3bae7ed48d44112a0f48e66742c00090bdb5bef71d9d3c056c66e97b732"

SYSTEM_DLLS = {
    "advapi32.dll",
    "bcrypt.dll",
    "cfgmgr32.dll",
    "combase.dll",
    "crypt32.dll",
    "gdi32.dll",
    "imm32.dll",
    "kernel32.dll",
    "kernelbase.dll",
    "ncrypt.dll",
    "ntdll.dll",
    "ole32.dll",
    "oleaut32.dll",
    "powrprof.dll",
    "rpcrt4.dll",
    "secur32.dll",
    "setupapi.dll",
    "shell32.dll",
    "shlwapi.dll",
    "user32.dll",
    "userenv.dll",
    "usp10.dll",
    "version.dll",
    "winmm.dll",
    "ws2_32.dll",
    # NVIDIA's driver DLL is supplied by the installed graphics driver.
    "nvcuda.dll",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Stage and build the Windows installer for Within the Latent Walls. "
            "The verified Gemma model is included for offline installation."
        )
    )
    parser.add_argument("--version", default="3.0.0", help="Installer application version.")
    parser.add_argument("--skip-build", action="store_true", help="Reuse the current Release executable.")
    parser.add_argument(
        "--skip-source-archive",
        action="store_true",
        help="Do not create the GPL corresponding-source ZIP beside the installer.",
    )
    parser.add_argument("--portable-only", action="store_true", help="Stage the complete offline folder without compiling Setup.")
    parser.add_argument("--iscc", type=Path, help="Explicit path to Inno Setup's ISCC.exe.")
    parser.add_argument(
        "--no-bootstrap-inno",
        action="store_true",
        help="Fail instead of downloading a pinned portable Inno Setup compiler when ISCC is absent.",
    )
    return parser.parse_args()


def run(command: list[str], *, cwd: Path = PROJECT_ROOT) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, cwd=str(cwd), check=True)


def sha256_file(path: Path, chunk_size: int = 16 * 1024 * 1024) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(chunk_size)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def ensure_inside_installer_root(path: Path) -> Path:
    resolved_root = INSTALLER_ROOT.resolve()
    resolved = path.resolve()
    if resolved == resolved_root or resolved_root not in resolved.parents:
        raise RuntimeError(f"Refusing to modify a path outside {resolved_root}: {resolved}")
    return resolved


def reset_directory(path: Path) -> None:
    safe_path = ensure_inside_installer_root(path)
    if safe_path.exists():
        shutil.rmtree(safe_path)
    safe_path.mkdir(parents=True, exist_ok=True)


def version_key(path: Path) -> tuple[int, ...]:
    values = [int(value) for value in re.findall(r"\d+", path.name)]
    return tuple(values)


def find_dumpbin() -> Path:
    from_path = shutil.which("dumpbin.exe") or shutil.which("dumpbin")
    if from_path:
        return Path(from_path).resolve()

    roots = [
        Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Microsoft Visual Studio",
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio",
    ]
    candidates: list[Path] = []
    for root in roots:
        if root.exists():
            candidates.extend(root.glob("*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe"))
    if not candidates:
        raise RuntimeError("dumpbin.exe was not found. Install the Visual Studio C++ x64 tools.")
    return sorted(candidates, key=lambda path: version_key(path.parents[3]))[-1].resolve()


def find_runtime_directories() -> list[tuple[str, Path]]:
    directories: list[tuple[str, Path]] = []

    cuda_path = os.environ.get("CUDA_PATH")
    if cuda_path:
        cuda_bin = Path(cuda_path) / "bin"
        if cuda_bin.is_dir():
            directories.append(("NVIDIA CUDA runtime", cuda_bin.resolve()))

    vs_roots = [
        Path(os.environ.get("ProgramFiles", r"C:\Program Files"))
        / "Microsoft Visual Studio",
        Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio",
    ]
    redist_versions: list[Path] = []
    for vs_root in vs_roots:
        if vs_root.exists():
            redist_versions.extend(vs_root.glob("*/*/VC/Redist/MSVC/*"))
    for version_dir in sorted(redist_versions, key=version_key, reverse=True):
        for runtime_dir in sorted((version_dir / "x64").glob("Microsoft.VC*.CRT"), reverse=True):
            if runtime_dir.is_dir():
                directories.append(("Microsoft Visual C++ runtime", runtime_dir.resolve()))
        for runtime_dir in sorted((version_dir / "x64").glob("Microsoft.VC*.OpenMP"), reverse=True):
            if runtime_dir.is_dir():
                directories.append(("Microsoft OpenMP runtime", runtime_dir.resolve()))

    for raw_path in os.environ.get("PATH", "").split(os.pathsep):
        if raw_path:
            candidate = Path(raw_path)
            if candidate.is_dir():
                directories.append(("PATH runtime", candidate.resolve()))

    unique: list[tuple[str, Path]] = []
    seen: set[str] = set()
    for kind, directory in directories:
        key = str(directory).lower()
        if key not in seen:
            seen.add(key)
            unique.append((kind, directory))
    return unique


def dumpbin_dependencies(dumpbin: Path, binary: Path) -> list[str]:
    completed = subprocess.run(
        [str(dumpbin), "/nologo", "/dependents", str(binary)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
    )
    dependencies: list[str] = []
    for line in completed.stdout.splitlines():
        candidate = line.strip()
        if re.fullmatch(r"[A-Za-z0-9_.+-]+\.dll", candidate, flags=re.IGNORECASE):
            dependencies.append(candidate)
    return dependencies


def is_system_dependency(name: str) -> bool:
    lower = name.lower()
    return lower in SYSTEM_DLLS or lower.startswith("api-ms-win-") or lower.startswith("ext-ms-")


def resolve_runtime_dependency(name: str, directories: list[tuple[str, Path]]) -> tuple[str, Path] | None:
    for kind, directory in directories:
        candidate = directory / name
        if candidate.is_file():
            return kind, candidate.resolve()
        # DLL imports are case-insensitive; preserve this on case-sensitive build hosts too.
        matches = [path for path in directory.glob("*.dll") if path.name.lower() == name.lower()]
        if matches:
            return kind, matches[0].resolve()
    return None


def copy_runtime_dependencies(source_exe: Path, staged_exe: Path) -> list[dict[str, object]]:
    dumpbin = find_dumpbin()
    runtime_directories = find_runtime_directories()
    queued: list[Path] = [source_exe]
    inspected: set[str] = set()
    copied_names: set[str] = set()
    records: list[dict[str, object]] = []

    while queued:
        binary = queued.pop(0)
        binary_key = str(binary).lower()
        if binary_key in inspected:
            continue
        inspected.add(binary_key)

        for dependency in dumpbin_dependencies(dumpbin, binary):
            dependency_lower = dependency.lower()
            if is_system_dependency(dependency_lower) or dependency_lower in copied_names:
                continue

            resolved = resolve_runtime_dependency(dependency, runtime_directories)
            if not resolved:
                raise RuntimeError(
                    f"Required non-system dependency {dependency} imported by {binary.name} was not found."
                )

            source_kind, source_path = resolved
            destination = staged_exe.parent / source_path.name
            shutil.copy2(source_path, destination)
            copied_names.add(dependency_lower)
            queued.append(source_path)
            records.append(
                {
                    "name": destination.name,
                    "bytes": destination.stat().st_size,
                    "sha256": sha256_file(destination),
                    "source_kind": source_kind,
                }
            )
            print(f"Runtime: {dependency} <- {source_kind}")

    return sorted(records, key=lambda item: str(item["name"]).lower())


def copy_distribution_payload(version: str) -> None:
    if not SOURCE_EXE.is_file():
        raise FileNotFoundError(f"Release executable not found: {SOURCE_EXE}")
    provenance_path = SOURCE_EXE.parent / "runtime-build.json"
    if not provenance_path.is_file():
        raise RuntimeError("Build provenance missing; run scripts/build_native.py --publish first")
    provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    if provenance.get("artifacts", {}).get(SOURCE_EXE.name) != sha256_file(SOURCE_EXE):
        raise RuntimeError("Release executable differs from its recorded build")
    if provenance.get("model_manifest_sha256") != sha256_file(PROJECT_ROOT / "scripts/models.json"):
        raise RuntimeError("Model manifest differs from the native build; rebuild before packaging")
    expected_commit = (PROJECT_ROOT / "vendor/llama.cpp/VENDORED_COMMIT.txt").read_text().strip()
    if provenance.get("llama_cpp_commit") != expected_commit:
        raise RuntimeError("Native build and packaged vendor revision differ")
    model_source = PROJECT_ROOT / MODEL_RELATIVE_PATH
    if not model_source.is_file() or model_source.stat().st_size != MODEL_SIZE or sha256_file(model_source) != MODEL_SHA256:
        raise RuntimeError("The pinned Gemma model is missing or invalid; run download_gemma.bat before packaging")
    reset_directory(STAGE_DIR)
    DIST_DIR.mkdir(parents=True, exist_ok=True)

    staged_exe = STAGE_DIR / STAGED_EXE_NAME
    shutil.copy2(SOURCE_EXE, staged_exe)
    shutil.copy2(provenance_path, STAGE_DIR / "runtime-build.json")
    shutil.copytree(PROJECT_ROOT / "assets", STAGE_DIR / "assets")
    shutil.copy2(PROJECT_ROOT / "LICENSE", STAGE_DIR / "LICENSE.txt")
    shutil.copy2(PROJECT_ROOT / "README.md", STAGE_DIR / "README.md")
    shutil.copy2(PACKAGING_DIR / "MODEL_NOTICE.txt", STAGE_DIR / "MODEL_NOTICE.txt")
    shutil.copy2(PACKAGING_DIR / "INSTALLATION_README.txt", STAGE_DIR / "INSTALLATION_README.txt")
    (STAGE_DIR / "output").mkdir()
    (STAGE_DIR / Path(MODEL_RELATIVE_PATH).parent).mkdir(parents=True)
    shutil.copy2(model_source, STAGE_DIR / MODEL_RELATIVE_PATH)

    license_dir = STAGE_DIR / "licenses" / "third-party"
    license_dir.mkdir(parents=True)
    cuda_path_value = os.environ.get("CUDA_PATH", "").strip()
    if not cuda_path_value:
        raise RuntimeError("CUDA_PATH is required to package the CUDA redistribution notices.")
    cuda_root = Path(cuda_path_value)
    third_party_licenses = [
        (PACKAGING_DIR / "Gemma-LICENSE.txt", "Gemma-LICENSE.txt"),
        (PROJECT_ROOT / "vendor" / "llama.cpp" / "LICENSE", "llama.cpp-LICENSE.txt"),
        (BUILD_DIR / "_deps" / "sdl3-src" / "LICENSE.txt", "SDL3-LICENSE.txt"),
        (BUILD_DIR / "_deps" / "sdl3_ttf-src" / "LICENSE.txt", "SDL3_ttf-LICENSE.txt"),
        (
            BUILD_DIR / "_deps" / "sdl3_ttf-src" / "external" / "freetype" / "LICENSE.TXT",
            "FreeType-LICENSE.txt",
        ),
        (
            BUILD_DIR / "_deps" / "sdl3_ttf-src" / "external" / "harfbuzz" / "COPYING",
            "HarfBuzz-COPYING.txt",
        ),
        (
            BUILD_DIR / "_deps" / "sdl3_ttf-src" / "external" / "plutosvg" / "LICENSE",
            "PlutoSVG-LICENSE.txt",
        ),
        (
            BUILD_DIR / "_deps" / "sdl3_ttf-src" / "external" / "plutovg" / "LICENSE",
            "PlutoVG-LICENSE.txt",
        ),
        (PROJECT_ROOT / "assets" / "fonts" / "Zilla_Slab" / "OFL.txt", "Zilla-Slab-OFL.txt"),
        (
            PROJECT_ROOT / "assets" / "fonts" / "Zilla_Slab_Highlight" / "OFL.txt",
            "Zilla-Slab-Highlight-OFL.txt",
        ),
        (cuda_root / "LICENSE", "NVIDIA-CUDA-LICENSE.txt"),
        (cuda_root / "EULA.txt", "NVIDIA-CUDA-EULA.txt"),
    ]
    for source, destination_name in third_party_licenses:
        if not source.is_file():
            raise FileNotFoundError(f"Required third-party license not found: {source}")
        shutil.copy2(source, license_dir / destination_name)

    runtime_records = copy_runtime_dependencies(SOURCE_EXE, staged_exe)

    gguf_files = list(STAGE_DIR.rglob("*.gguf"))
    if gguf_files != [STAGE_DIR / MODEL_RELATIVE_PATH]:
        raise RuntimeError(f"Unexpected model payload: {gguf_files}")

    payload_files: list[dict[str, object]] = []
    for path in sorted(STAGE_DIR.rglob("*")):
        if path.is_file() and path.name != "distribution-manifest.json":
            payload_files.append(
                {
                    "path": path.relative_to(STAGE_DIR).as_posix(),
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )

    manifest = {
        "application": "Within the Latent Walls / Entre les Murs Latents",
        "version": version,
        "executable": STAGED_EXE_NAME,
        "model_embedded": True,
        "llama_cpp_commit": (PROJECT_ROOT / "vendor/llama.cpp/VENDORED_COMMIT.txt").read_text().strip(),
        "model": {
            "profile": MODEL_PROFILE_NAME,
            "repository": MODEL_REPOSITORY,
            "revision": MODEL_REVISION,
            "filename": MODEL_FILENAME,
            "relative_path": MODEL_RELATIVE_PATH,
            "bytes": MODEL_SIZE,
            "sha256": MODEL_SHA256,
            "downloaded_during_installation": False,
        },
        "runtime_dependencies": runtime_records,
        "payload_files": payload_files,
    }
    (STAGE_DIR / "distribution-manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    print(f"Staged {len(payload_files) + 1} files in {STAGE_DIR}")
    print(f"Staged size: {sum(path.stat().st_size for path in STAGE_DIR.rglob('*') if path.is_file()):,} bytes")


def smoke_test_staged_executable() -> None:
    executable = STAGE_DIR / STAGED_EXE_NAME
    print("Running staged executable smoke test...")
    result = subprocess.run([str(executable), "--llama-info"], cwd=INSTALLER_ROOT,
                            capture_output=True, text=True, check=True)
    print(result.stdout, end="")
    expected_commit = (PROJECT_ROOT / "vendor/llama.cpp/VENDORED_COMMIT.txt").read_text().strip()
    if f"llama.cpp vendored commit: {expected_commit}" not in result.stdout:
        raise RuntimeError("Staged binary and packaged llama.cpp source revision differ")


def download_verified(url: str, expected_sha256: str, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".part")
    if temporary.exists():
        temporary.unlink()

    print(f"Downloading {url}")
    request = urllib.request.Request(url, headers={"User-Agent": "WithinTheLatentWalls-InstallerBuilder/1"})
    digest = hashlib.sha256()
    with urllib.request.urlopen(request) as response, temporary.open("wb") as output:
        while True:
            block = response.read(1024 * 1024)
            if not block:
                break
            output.write(block)
            digest.update(block)

    actual_sha256 = digest.hexdigest()
    if actual_sha256.lower() != expected_sha256.lower():
        temporary.unlink(missing_ok=True)
        raise RuntimeError(
            f"Downloaded file hash mismatch for {destination.name}: "
            f"expected {expected_sha256}, got {actual_sha256}"
        )
    temporary.replace(destination)


def find_iscc(explicit_path: Path | None) -> Path | None:
    candidates: list[Path] = []
    if explicit_path:
        candidates.append(explicit_path)
    from_path = shutil.which("ISCC.exe") or shutil.which("ISCC")
    if from_path:
        candidates.append(Path(from_path))

    candidates.extend(
        [
            TOOLS_DIR / f"Inno Setup {INNO_VERSION}" / "ISCC.exe",
            Path(os.environ.get("LOCALAPPDATA", "")) / "Programs" / "Inno Setup 6" / "ISCC.exe",
            Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Inno Setup 6" / "ISCC.exe",
            Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Inno Setup 6" / "ISCC.exe",
        ]
    )
    for candidate in candidates:
        if candidate and candidate.is_file():
            return candidate.resolve()
    return None


def bootstrap_inno_setup() -> Path:
    installer = TOOLS_DIR / f"innosetup-{INNO_VERSION}.exe"
    install_dir = TOOLS_DIR / f"Inno Setup {INNO_VERSION}"
    iscc = install_dir / "ISCC.exe"
    if iscc.is_file():
        return iscc.resolve()

    if not installer.is_file() or sha256_file(installer).lower() != INNO_SHA256.lower():
        download_verified(INNO_URL, INNO_SHA256, installer)

    install_dir.mkdir(parents=True, exist_ok=True)
    run(
        [
            str(installer),
            "/VERYSILENT",
            "/SUPPRESSMSGBOXES",
            "/NORESTART",
            "/CURRENTUSER",
            "/PORTABLE=1",
            f"/DIR={install_dir}",
        ]
    )
    if not iscc.is_file():
        discovered = list(install_dir.rglob("ISCC.exe"))
        if not discovered:
            raise RuntimeError(f"Inno Setup bootstrap completed but ISCC.exe was not found in {install_dir}")
        iscc = discovered[0]
    return iscc.resolve()


def build_inno_installer(version: str, iscc: Path) -> Path:
    expected_name = f"Within-the-Latent-Walls-Setup-{version}.exe"
    expected_output = DIST_DIR / expected_name
    if expected_output.exists():
        expected_output.unlink()

    run(
        [
            str(iscc),
            f"/DStageDir={STAGE_DIR}",
            f"/DDistDir={DIST_DIR}",
            f"/DAppVersion={version}",
            str(INNO_SCRIPT),
        ]
    )
    if not expected_output.is_file():
        raise RuntimeError(f"Inno Setup did not create the expected installer: {expected_output}")
    slices = list(DIST_DIR.glob(f"Within-the-Latent-Walls-Setup-{version}-*.bin"))
    if not slices or sum(path.stat().st_size for path in slices) < MODEL_SIZE:
        raise RuntimeError("Offline installer data volumes are incomplete")
    return expected_output


def iter_source_files() -> list[tuple[Path, Path]]:
    pairs: list[tuple[Path, Path]] = []
    root_files = [
        PROJECT_ROOT / ".gitignore",
        PROJECT_ROOT / "CMakeLists.txt",
        PROJECT_ROOT / "LICENSE",
        PROJECT_ROOT / "README.md",
    ]
    root_files.extend(PROJECT_ROOT.glob("*.bat"))
    # Personal commit automation is not required to build or run the game.
    for source in root_files:
        if source.is_file():
            pairs.append((source, source.relative_to(PROJECT_ROOT)))

    source_trees = [
        (PROJECT_ROOT / "src", Path("src")),
        (PROJECT_ROOT / "assets", Path("assets")),
        (PROJECT_ROOT / "scripts", Path("scripts")),
        (PROJECT_ROOT / "tests", Path("tests")),
        (PROJECT_ROOT / "packaging", Path("packaging")),
        (PROJECT_ROOT / "vendor" / "stb", Path("vendor/stb")),
        (PROJECT_ROOT / "vendor" / "llama.cpp", Path("vendor/llama.cpp")),
        (BUILD_DIR / "_deps" / "sdl3-src", Path("external-build-sources/SDL")),
        (BUILD_DIR / "_deps" / "sdl3_ttf-src", Path("external-build-sources/SDL_ttf")),
    ]
    excluded_parts = {".git", "__pycache__", ".cache", "CMakeFiles", "node_modules", "build", "build-cuda"}
    for source_root, archive_root in source_trees:
        if not source_root.is_dir():
            raise FileNotFoundError(f"Corresponding-source directory is missing: {source_root}")
        for source in source_root.rglob("*"):
            parts = source.relative_to(source_root).parts
            if not source.is_file() or any(part in excluded_parts for part in parts[:-1]):
                continue
            # llama.cpp ships vocabulary fixtures as GGUF. They are not source code,
            # are not required to build the game, and must not blur the no-weights rule.
            if source.suffix.lower() == ".gguf":
                continue
            relative = archive_root / source.relative_to(source_root)
            pairs.append((source, relative))

    for source in (PROJECT_ROOT / "documentation").glob("*.md"):
        pairs.append((source, Path("documentation") / source.name))

    return sorted(pairs, key=lambda pair: pair[1].as_posix().lower())


def build_source_archive(version: str) -> Path:
    archive_path = DIST_DIR / f"Within-the-Latent-Walls-Source-{version}.zip"
    if archive_path.exists():
        archive_path.unlink()
    files = iter_source_files()
    gguf_sources = [relative for _, relative in files if relative.suffix.lower() == ".gguf"]
    if gguf_sources:
        raise RuntimeError(f"The corresponding-source archive must not contain GGUF files: {gguf_sources}")
    print(f"Creating corresponding-source archive with {len(files)} files...")
    with zipfile.ZipFile(archive_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for source, relative in files:
            archive.write(source, Path("Within-the-Latent-Walls-Source") / relative)
    return archive_path


def main() -> int:
    args = parse_args()
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){1,3}(?:[-+][A-Za-z0-9.-]+)?", args.version):
        raise ValueError(f"Unsupported version format: {args.version}")

    if sys.platform != "win32":
        raise RuntimeError("The current installer builder targets 64-bit Windows only.")

    if not args.skip_build:
        run([sys.executable, str(PROJECT_ROOT / "scripts/build_native.py"), "--publish"])

    copy_distribution_payload(args.version)
    smoke_test_staged_executable()

    if args.portable_only:
        if not args.skip_source_archive:
            build_source_archive(args.version)
        print(f"Complete offline portable folder: {STAGE_DIR}")
        return 0

    iscc = find_iscc(args.iscc)
    if not iscc:
        if args.no_bootstrap_inno:
            raise FileNotFoundError("ISCC.exe was not found and Inno Setup bootstrap is disabled.")
        iscc = bootstrap_inno_setup()
    print(f"Inno Setup compiler: {iscc}")

    installer = build_inno_installer(args.version, iscc)
    outputs = [installer, *sorted(DIST_DIR.glob(f"Within-the-Latent-Walls-Setup-{args.version}-*.bin"))]
    if not args.skip_source_archive:
        outputs.append(build_source_archive(args.version))

    print("\nDistribution artifacts:")
    for output in outputs:
        print(f"  {output} ({output.stat().st_size:,} bytes, SHA-256 {sha256_file(output)})")
    print("\nGemma is included. Keep Setup.exe and every .bin volume together; installation needs no network.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
