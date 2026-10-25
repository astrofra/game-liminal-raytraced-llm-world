"""Compare two/three native composition calls on four historical failure seeds.

Artifacts must be written outside the repository. Runs sequentially against the
same local model; no statistical reliability claim is inferred from four cases.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--exe", type=Path, default=ROOT / "build/Release/liminal_cornell_renderer.exe")
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or output.exists():
        parser.error("--output must be a new directory outside the repository")
    output.mkdir(parents=True)
    contract = json.loads((ROOT / "assets/scene_generation/local_v1.json").read_text(encoding="utf-8"))
    rows = []
    for index, seed in enumerate((99491150, 99595879, 99813256, 99917985)):
        brief = output / f"brief-{index}.txt"
        brief.write_text(contract["exploration_briefs"][2 if index < 2 else 3], encoding="utf-8")
        for limit in (2, 3):
            prefix = output / f"case-{index}-calls-{limit}"
            with prefix.with_suffix(".log").open("wb") as log:
                completed = subprocess.run([
                    str(args.exe.resolve()), "--generate-dynamic-scene", str(brief),
                    "--scene-method", "local", "--scene-max-calls", str(limit),
                    "--llm-seed", str(seed), "--scene-artifacts", str(prefix),
                    "--width", "256", "--height", "128", "--samples", "1",
                    "--output", str(prefix.with_suffix(".png"))], cwd=ROOT,
                    stdout=log, stderr=subprocess.STDOUT, timeout=150)
            metrics = json.loads(prefix.with_suffix(".metrics.json").read_text(encoding="utf-8"))
            attempts = sorted(output.glob(prefix.name + ".attempt-*.raw.txt"))
            row = {"case": index, "seed": seed, "limit": limit, "exit": completed.returncode,
                   "success": metrics["success"], "calls": len(metrics["calls"]),
                   "elapsed_ms": metrics["elapsed_ms"], "error": metrics["error"],
                   "candidate_hashes": [hashlib.sha256(p.read_bytes()).hexdigest() for p in attempts]}
            rows.append(row)
            print(json.dumps(row, ensure_ascii=False), flush=True)
            (output / "results.json").write_text(json.dumps(rows, indent=2, ensure_ascii=False), encoding="utf-8")
    pairs = []
    for index in range(4):
        two, three = rows[2 * index:2 * index + 2]
        same = two["candidate_hashes"] == three["candidate_hashes"][:len(two["candidate_hashes"])]
        pairs.append({"case": index, "same_first_candidates": same,
                      "success_difference": not two["success"] and three["success"],
                      "gain_attributable_to_third": same and three["calls"] == 3 and not two["success"] and three["success"],
                      "additional_ms": three["elapsed_ms"] - two["elapsed_ms"]})
    (output / "comparison.json").write_text(json.dumps(pairs, indent=2), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
