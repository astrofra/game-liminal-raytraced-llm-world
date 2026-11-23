"""Build the C++ archive atlas without SDL, CUDA or the inference runtime."""
import argparse
import subprocess
import os
from pathlib import Path
from build_native import compiler_environment, version, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build_archive_render")
    parser.add_argument("--msvc-root", type=Path)
    args = parser.parse_args()
    # This tool has no CUDA constraint. Select an installed compiler explicitly
    # instead of inheriting the game's possibly stale CUDA toolset setting.
    toolset = args.msvc_root
    if toolset is None:
        base = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Microsoft Visual Studio"
        candidates = [p for p in base.glob("*/*/VC/Tools/MSVC/*")
                      if (p / "bin/Hostx64/x64/cl.exe").is_file()]
        if not candidates:
            raise RuntimeError("Install MSVC C++ tools or pass --msvc-root.")
        toolset = max(candidates, key=version)
    env, compiler = compiler_environment(toolset)
    build = args.build_dir.resolve()
    subprocess.run(["cmake", "-S", str(ROOT / "tools/archive_render"), "-B", str(build),
                    "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
                    f"-DCMAKE_CXX_COMPILER={compiler.as_posix()}"], env=env, check=True)
    subprocess.run(["cmake", "--build", str(build), "--parallel", "4"], env=env, check=True)
    subprocess.run(["ctest", "--test-dir", str(build), "--output-on-failure"], env=env, check=True)


if __name__ == "__main__":
    main()
