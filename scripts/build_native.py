"""Build the Windows native runtime with an explicit MSVC/Ninja environment.

Use --msvc-root (or ignored build_settings.local.json) for a portable toolset.
The LLM engine's Vulkan backend needs the Vulkan SDK (VULKAN_SDK, for the
headers and glslc); the game itself only needs the GPU driver's vulkan-1.dll.
No installer or compiler-version override is invoked by this helper.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def version(path):
    return tuple(int(x) for x in re.findall(r"\d+", path.name))


def compiler_environment(msvc_root=None):
    env = os.environ.copy()
    settings = ROOT / "build_settings.local.json"
    if msvc_root is None:
        configured = env.get("LIMINAL_MSVC_ROOT")
        if not configured and settings.exists():
            configured = json.loads(settings.read_text(encoding="utf-8")).get("msvc_root")
        if configured:
            msvc_root = Path(configured)
    if msvc_root is None:
        base = Path(env.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio"
        candidates = [p for p in base.glob("*/*/VC/Tools/MSVC/*") if (p / "bin/Hostx64/x64/cl.exe").is_file()]
        compatible = [p for p in candidates if version(p) >= (14, 20)]
        if not compatible:
            raise RuntimeError("No MSVC toolset found. Install the VS 2022 (or newer) C++ tools or pass --msvc-root.")
        msvc_root = max(compatible, key=version)
    msvc_root = Path(msvc_root).resolve()
    compiler = msvc_root / "bin/Hostx64/x64/cl.exe"
    if not compiler.is_file():
        raise FileNotFoundError(compiler)
    sdk = Path(env.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Windows Kits/10"
    versions = [p for p in (sdk / "Lib").iterdir() if (sdk / "Include" / p.name / "ucrt").is_dir()]
    sdk_version = max(versions, key=version).name
    env["PATH"] = os.pathsep.join([str(compiler.parent), str(sdk / "bin" / sdk_version / "x64"), env["PATH"]])
    env["INCLUDE"] = os.pathsep.join([str(msvc_root / "include")] +
                                    [str(sdk / "Include" / sdk_version / part) for part in ("ucrt", "shared", "um", "winrt")])
    env["LIB"] = os.pathsep.join([str(msvc_root / "lib/x64")] +
                                [str(sdk / "Lib" / sdk_version / part / "x64") for part in ("ucrt", "um")])
    return env, compiler


def engine_commit():
    """Source revision, as CMake embeds it for --llm-info (LIMINAL_ENGINE_COMMIT)."""
    result = subprocess.run(["git", "rev-parse", "--short=12", "HEAD"], cwd=ROOT, capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else "unknown"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build_native")
    parser.add_argument("--msvc-root", type=Path)
    parser.add_argument("--parallel", type=int, default=min(8, os.cpu_count() or 4))
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument("--publish", action="store_true", help="Copy successful binaries to the conventional build/ launch paths")
    args = parser.parse_args()
    env, compiler = compiler_environment(args.msvc_root)
    if not shutil.which("ninja", path=env["PATH"]):
        raise RuntimeError("Ninja is required (winget install Ninja-build.Ninja).")
    if not env.get("VULKAN_SDK"):
        raise RuntimeError("The Vulkan SDK is required to build the LLM engine (https://vulkan.lunarg.com, sets VULKAN_SDK).")
    build = args.build_dir.resolve()
    command = ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja Multi-Config",
               f"-DCMAKE_C_COMPILER={compiler.as_posix()}", f"-DCMAKE_CXX_COMPILER={compiler.as_posix()}"]
    cache = build / "CMakeCache.txt"
    if cache.exists() and "CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config" not in cache.read_text(errors="replace"):
        command.insert(1, "--fresh")
    for name, directory in (("SDL3", "sdl3-src"), ("SDL3_TTF", "sdl3_ttf-src")):
        source = ROOT / "build/_deps" / directory
        if not source.is_dir():
            source = ROOT / "external-build-sources" / ("SDL" if name == "SDL3" else "SDL_ttf")
        if source.is_dir():
            command.append(f"-DFETCHCONTENT_SOURCE_DIR_{name}={source.as_posix()}")
    print(subprocess.list2cmdline(command), flush=True)
    subprocess.run(command, env=env, cwd=ROOT, check=True)
    if not args.configure_only:
        targets = ["liminal_cornell_renderer", "liminal_llm_channels_test", "liminal_distant_light_test", "liminal_dynamic_scene_test", "liminal_scene_normalization_test", "liminal_scene_visibility_test", "liminal_place_proposal_test", "liminal_inventory_test", "liminal_narrative_language_test"]
        targets.extend(["liminal_engine_cli", "liminal_engine_tokenizer_test", "liminal_engine_grammar_test"])
        targets.append("liminal_continuity_test")
        targets.extend(["liminal_audio_diagnostic", "liminal_ambience_test"])
        targets.append("liminal_exploration_policy_test")
        targets.extend(["liminal_glass_supports", "liminal_glass_supports_test", "liminal_glass_supports_geometry_test"])
        targets.extend(["liminal_coplanar_cleanup", "liminal_coplanar_cleanup_test", "liminal_coplanar_cleanup_geometry_test"])
        subprocess.run(["cmake", "--build", str(build), "--config", "Release", "--parallel", str(args.parallel),
                        "--target", *targets], env=env, cwd=ROOT, check=True)
        artifacts = [build / "Release/liminal_cornell_renderer.exe"]
        provenance = {
            "engine_commit": engine_commit(),
            "model_manifest_sha256": hashlib.sha256((ROOT / "scripts/models.json").read_bytes()).hexdigest(),
            "compiler": str(compiler),
            "artifacts": {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in artifacts},
        }
        for artifact in artifacts:
            (artifact.parent / "runtime-build.json").write_text(json.dumps(provenance, indent=2), encoding="utf-8")
            if args.publish:
                destination = ROOT / "build" / "Release"
                destination.mkdir(parents=True, exist_ok=True)
                if destination.resolve() != artifact.parent.resolve():
                    shutil.copy2(artifact, destination / artifact.name)
                    shutil.copy2(artifact.parent / "runtime-build.json", destination / "runtime-build.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
