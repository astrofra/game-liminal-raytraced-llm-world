"""Real native protocol checks; intentionally separate from the fast unit suite."""
import argparse
import json
import subprocess
from pathlib import Path
from model_profiles import PROJECT_ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    prompt = args.out / "prompt.json"
    prompt.write_text(json.dumps([
        {"role": "system", "content": "Return only a JSON object."},
        {"role": "user", "content": 'Return exactly {"ok":true}.'},
    ]), encoding="utf-8")
    cases = [
        ("q4-grammar", ["--llm-profile", "ministral-8b-q4", "--use-json-grammar"], "eos"),
        ("q5-grammar", ["--llm-profile", "ministral-8b-q5", "--use-json-grammar"], "eos"),
        ("reasoning", ["--llm-profile", "ministral-8b-reasoning-q4", "--use-json-grammar"], "eos"),
        ("token-limit", ["--llm-predict", "1"], "token_limit"),
        ("cancel-before", ["--llm-cancel-after", "0"], "cancelled"),
        ("cancel-load", ["--llm-cancel-during-load"], "cancelled"),
        ("cancel-stream", ["--llm-cancel-after", "2"], "cancelled"),
        ("cancel-thought", ["--llm-profile", "ministral-8b-reasoning-q4", "--llm-cancel-after-checks", "12"], "cancelled"),
        ("timeout", ["--llm-timeout-ms", "1"], "timeout"),
        ("context-limit", ["--llm-context", "128"], "context_limit"),
        ("thought-limit", ["--llm-profile", "ministral-8b-reasoning-q4", "--llm-reasoning-budget", "1"], "reasoning_limit"),
    ]
    results = []
    for name, flags, expected in cases:
        output = args.out / (name + ".json")
        command = [str(args.exe.resolve()), "--llm-probe", str(prompt), "--llm-probe-output", str(output),
                   "--llm-predict", "128", *flags]
        with (args.out / (name + ".log")).open("wb") as log:
            process = subprocess.run(command, stdout=log, stderr=log, timeout=210, cwd=PROJECT_ROOT)
        data = json.loads(output.read_text(encoding="utf-8")) if output.exists() else {}
        valid = data.get("stop_reason") == expected and not data.get("stream_leaked_marker", True)
        if expected == "eos":
            valid = valid and process.returncode == 0 and data.get("success")
            try:
                valid = valid and json.loads(data["final"]) == {"ok": True}
            except (KeyError, ValueError):
                valid = False
        else:
            valid = valid and process.returncode == 2 and not data.get("success") and data.get("final") == ""
        if name == "reasoning":
            valid = valid and data.get("reasoning_tokens", 0) > 0 and data.get("final_tokens", 0) > 0
        if name == "cancel-thought":
            valid = valid and data.get("reasoning_tokens", 0) > 0 and data.get("stream_final") == ""
        results.append({"test": name, "expected": expected, "observed": data.get("stop_reason"),
                        "passed": bool(valid), "returncode": process.returncode})
        print(f"{name}: {data.get('stop_reason')}, passed={bool(valid)}", flush=True)
        (args.out / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    return int(not all(result["passed"] for result in results))


if __name__ == "__main__":
    raise SystemExit(main())
