"""Render the four hand-authored concept studies with the existing native renderer.
Raw logs and exact input snapshots stay outside the repository.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time

STUDY = Path(__file__).resolve().parent
ROOT = STUDY.parents[2]
CASES = {
    "01": "01-la-chambre-du-ciel",
    "02": "02-le-champ-des-diapasons",
    "03": "03-la-faille-en-chicane",
    "09": "09-la-veille-des-lumieres",
}

def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True, help="New run directory name; never overwritten")
    parser.add_argument("--cases", nargs="+", choices=CASES, default=list(CASES))
    parser.add_argument("--width", type=int, default=640)
    parser.add_argument("--height", type=int, default=320)
    parser.add_argument("--samples", type=int, default=24)
    parser.add_argument("--bounces", type=int, default=5)
    parser.add_argument("--direct-samples", type=int, default=4)
    parser.add_argument("--threads", type=int, default=12)
    parser.add_argument("--exposure", type=float, default=1.0)
    parser.add_argument("--scene-root", type=Path, default=STUDY / "scenes")
    parser.add_argument("--output-root", type=Path, default=STUDY / "renders")
    parser.add_argument("--logs-root", type=Path, default=Path("C:/works/duo-runs/game-liminal-raytraced-llm-world/CONCEPT-RENDER-01"))
    parser.add_argument("--renderer", type=Path, default=ROOT / "build/Release/liminal_cornell_renderer.exe")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9_-]+", args.label):
        parser.error("--label must be a simple directory name")
    if min(args.width, args.height, args.samples, args.bounces, args.direct_samples, args.threads) <= 0:
        parser.error("Render counts must be positive")
    renderer = args.renderer.resolve()
    if not renderer.is_file():
        parser.error(f"Missing renderer: {renderer}")
    out = args.output_root.resolve() / args.label
    logs = args.logs_root.resolve() / args.label
    if out.exists() or logs.exists():
        parser.error("Run/output directory already exists; choose a new label")
    out.mkdir(parents=True)
    logs.mkdir(parents=True)
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = str(args.threads)
    manifest = {
        "kind": "hand_authored_renderer_capability_study",
        "renderer_sha256": sha256(renderer),
        "source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "settings": {"width": args.width, "height": args.height, "spp": args.samples,
                     "diffuse_bounces": args.bounces, "glass_bounces": 9,
                     "direct_samples": args.direct_samples, "threads": args.threads,
                     "exposure": args.exposure, "seed": 1337},
        "cases": [],
    }
    for case in args.cases:
        name = CASES[case]
        scene = (args.scene_root / f"{name}.scene").resolve()
        scene_bytes = scene.read_bytes()
        (logs / scene.name).write_bytes(scene_bytes)
        output = out / f"{name}.png"
        command = [str(renderer), "--scene", str(scene), "--output", str(output),
                   "--width", str(args.width), "--height", str(args.height),
                   "--samples", str(args.samples), "--bounces", str(args.bounces),
                   "--glass-bounces", "9", "--direct-samples", str(args.direct_samples),
                   "--seed", "1337", "--exposure", str(args.exposure)]
        started = time.perf_counter()
        result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True,
                                encoding="utf-8", errors="replace", timeout=900)
        elapsed = time.perf_counter() - started
        log = result.stdout + "\n" + result.stderr
        (logs / f"{name}.log").write_text(log, encoding="utf-8")
        (logs / f"{name}.command.json").write_text(json.dumps(command, indent=2), encoding="utf-8")
        record = {"id": case, "scene": scene.name,
                  "scene_sha256": hashlib.sha256(scene_bytes).hexdigest(),
                  "image": output.name, "exit_code": result.returncode,
                  "wall_seconds": round(elapsed, 3)}
        for label, pattern in [
            ("render_ms", r"Render(?: time)?:\s*([0-9.]+)\s*ms"),
            ("triangles", r"Loaded\s+(\d+)\s+triangles"),
            ("materials", r"triangles,\s*(\d+)\s+materials"),
            ("emissive_triangles", r"materials,\s*(\d+)\s+emissive triangles"),
        ]:
            match = re.search(pattern, log, flags=re.I)
            if match:
                record[label] = float(match.group(1)) if label == "render_ms" else int(match.group(1))
        if result.returncode == 0 and output.is_file():
            record["image_sha256"] = sha256(output)
        manifest["cases"].append(record)
        (out / "results.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(json.dumps(record, ensure_ascii=False), flush=True)
        if result.returncode != 0:
            raise SystemExit(f"Renderer failed; see {logs / (name + '.log')}")
    print(f"Results: {out}", flush=True)

if __name__ == "__main__":
    main()

