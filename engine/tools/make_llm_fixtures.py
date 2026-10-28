"""Froze the llama.cpp (620d2225) goldens into tests/data/llm before llama.cpp was
removed; kept as the record of how the fixtures were produced. The goldens came
from the former engine/tools/llama_golden.cpp.

- vocab, tokenizer and chat-template cases, one prompt per purpose (prompts.json);
- golden/<record>.json: prompt tokens and the first 64 greedy steps with llama's
  top-8 logits, for `liminal_engine_cli parity` (per-layer dumps are left out).

Usage: python engine/tools/make_llm_fixtures.py <golden_dir> <capture_dir>
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tests" / "data" / "llm"


def main():
    golden, capture = Path(sys.argv[1]), Path(sys.argv[2])
    OUT.mkdir(parents=True, exist_ok=True)
    vocab = json.loads((golden / "vocab.json").read_text(encoding="utf-8"))
    runs, previous, count = [], None, 0
    for attr in vocab["attrs"]:
        if attr == previous:
            count += 1
            continue
        if previous is not None:
            runs.append([previous, count])
        previous, count = attr, 1
    runs.append([previous, count])
    summary = {k: vocab[k] for k in ("n_tokens", "bos", "eos", "eog")}
    summary["attr_runs"] = runs
    (OUT / "vocab.json").write_text(json.dumps(summary), encoding="utf-8")
    for name in ("tokenizer_cases.json", "chat_cases.json"):
        (OUT / name).write_text((golden / name).read_text(encoding="utf-8"), encoding="utf-8")
    prompts, seen = [], set()
    for path in sorted(golden.glob("*-*.json")):
        record = json.loads(path.read_text(encoding="utf-8"))
        if record["purpose"] in seen:
            continue
        seen.add(record["purpose"])
        source = json.loads((capture / record["record"]).read_text(encoding="utf-8"))
        prompts.append({"purpose": record["purpose"], "messages": source["messages"],
                        "prompt_text": record["prompt_text"], "tokens": record["tokens"],
                        "use_json_grammar": source.get("use_json_grammar", False),
                        "json_schema": source.get("json_schema", ""),
                        "greedy": record["greedy"]["tokens"], "greedy_stop": record["greedy"]["stop"]})
    (OUT / "prompts.json").write_text(json.dumps(prompts, ensure_ascii=False), encoding="utf-8")
    (OUT / "golden").mkdir(exist_ok=True)
    for path in sorted(golden.glob("*-*.json")):
        record = json.loads(path.read_text(encoding="utf-8"))
        greedy = record["greedy"]
        compact = {"purpose": record["purpose"], "record": record["record"], "tokens": record["tokens"],
                   "greedy": {"tokens": greedy["tokens"][:64], "top": greedy["top"][:64],
                              "stop": greedy["stop"], "constrained": greedy["constrained"]}}
        (OUT / "golden" / path.name).write_text(json.dumps(compact, separators=(",", ":")), encoding="utf-8")
    print(f"wrote {len(prompts)} prompts, {len(runs)} attr runs to {OUT}")


if __name__ == "__main__":
    main()
