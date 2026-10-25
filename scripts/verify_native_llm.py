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
    schema = args.out / "constraint.json"
    schema.write_text(json.dumps({"type":"object", "properties":{"ok":{"const":True}}, "required":["ok"], "additionalProperties":False}), encoding="utf-8")
    conflict = args.out / "conflict.json"
    conflict.write_text(json.dumps([
        {"role":"system","content":"Return only a JSON object."},
        {"role":"user","content":'Return exactly {"ok":false,"extra":123}.'}
    ]),encoding="utf-8")
    broken = args.out / "broken.gguf"
    broken.write_bytes(b"broken model")
    cases = [
        ("schema", ["--llm-probe-schema",str(schema)], "eos"),
        ("resident-20", ["--llm-probe-repeat","20","--llm-probe-schema",str(schema)], "eos"),
        ("missing-model", ["--model",str(args.out / "missing.gguf")], "model_missing"),
        ("broken-model", ["--model",str(broken)], "model_load_error"),
        ("gemma-grammar", ["--use-json-grammar"], "eos"),
        ("token-limit", ["--llm-predict", "1"], "token_limit"),
        ("cancel-before", ["--llm-cancel-after", "0"], "cancelled"),
        ("cancel-load", ["--llm-cancel-during-load"], "cancelled"),
        ("cancel-stream", ["--llm-cancel-after", "2"], "cancelled"),
        ("timeout", ["--llm-timeout-ms", "1"], "timeout"),
        ("context-limit", ["--llm-context", "128"], "context_limit"),
    ]
    results = []
    for name, flags, expected in cases:
        output = args.out / (name + ".json")
        command = [str(args.exe.resolve()), "--llm-probe", str(conflict if name in ("schema","resident-20") else prompt), "--llm-probe-output", str(output),
                   "--llm-predict", "128", *flags]
        with (args.out / (name + ".log")).open("wb") as log:
            process = subprocess.run(command, stdout=log, stderr=log, timeout=300, cwd=PROJECT_ROOT)
        data = json.loads(output.read_text(encoding="utf-8")) if output.exists() else {}
        resident_valid = True
        if name == "resident-20":
            resident_valid = isinstance(data,list) and len(data)==20 and all(
                row.get("success") and json.loads(row["final"])=={"ok":True} and
                row.get("model_reused")==bool(i) and (i==0 or row.get("load_time_ms")==0)
                for i,row in enumerate(data))
            data = data[-1] if isinstance(data,list) and data else {}
        valid = resident_valid and data.get("stop_reason") == expected and not data.get("stream_leaked_marker", True)
        if expected == "eos":
            valid = valid and process.returncode == 0 and data.get("success")
            try:
                valid = valid and json.loads(data["final"]) == {"ok": True}
            except (KeyError, ValueError):
                valid = False
        else:
            valid = valid and process.returncode == 2 and not data.get("success") and data.get("final") == ""
        results.append({"test": name, "expected": expected, "observed": data.get("stop_reason"),
                        "passed": bool(valid), "returncode": process.returncode})
        print(f"{name}: {data.get('stop_reason')}, passed={bool(valid)}", flush=True)
        (args.out / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    return int(not all(result["passed"] for result in results))


if __name__ == "__main__":
    raise SystemExit(main())
