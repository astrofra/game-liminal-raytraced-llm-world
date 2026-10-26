"""Short native continuity journey and paired open-instruction comparison.

Evidence stays outside the repository. Art quality requires review of the PNGs,
accepted programs and prose; hashes alone are not an artistic acceptance test.
"""
import argparse
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OPPOSITE = {"north": "south", "south": "north", "east": "west", "west": "east"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build/Release/liminal_cornell_renderer.exe")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seed", type=int, default=4242)
    args = parser.parse_args()
    out = args.out.resolve()
    if out.is_relative_to(ROOT) or out.exists():
        parser.error("--out must be a new directory outside the repository")
    out.mkdir(parents=True)
    exe = str(args.exe.resolve())
    results = []

    def run(name, flags):
        started = time.monotonic()
        with (out / f"{name}.log").open("wb") as log:
            process = subprocess.run([exe, *map(str, flags)], cwd=ROOT, stdout=log, stderr=log, timeout=240)
        row = {"name": name, "returncode": process.returncode, "wall_ms": 1000 * (time.monotonic() - started)}
        results.append(row)
        (out / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
        print(f"{name}: exit={process.returncode}, wall={row['wall_ms']:.0f}ms", flush=True)
        return row

    initial = out / "initial.json"
    run("initial", ["--write-initial-state", initial])
    state = json.loads(initial.read_text(encoding="utf-8"))
    state["language"] = "fr"
    initial.write_text(json.dumps(state, ensure_ascii=False), encoding="utf-8")
    state_path = initial
    paired = None

    def turn(command):
        nonlocal state_path, state, paired
        index = len([row for row in results if row.get("command")]) + 1
        name = f"turn-{index:02d}"
        prefix = out / name
        next_state = out / f"{name}.state.json"
        row = run(name, ["--run-turn", "--load-state", state_path, "--command", command,
                         "--llm-seed", args.seed, "--scene-artifacts", prefix, "--save-state", next_state,
                         "--output", out / f"{name}.png", "--width", 512, "--height", 256, "--samples", 8])
        row["command"] = command
        trace_path = out / f"{name}.turn.json"
        if trace_path.exists():
            trace = json.loads(trace_path.read_text(encoding="utf-8"))
            row.update({key: trace[key] for key in ("from", "to", "llm_calls", "accepted")})
            if trace["narrative_metrics"]:
                row["narration"] = json.loads(trace["narrative_metrics"])
                narrative = row["narration"]
                if "continuity_selection" in narrative:
                    selection = narrative["continuity_selection"]
                    assert selection["success"]
                    assert narrative["prompt_tokens"] == selection["prompt_tokens_after"]
                    assert selection["prompt_tokens_after"] + selection["output_reserved"] + selection["technical_margin"] <= selection["context_tokens"]
                    request = json.loads(trace["request"])
                    assert request["continuity"]["origin"]["id"] == state["current_place_id"]
        metrics_path = out / f"{name}.metrics.json"
        if metrics_path.exists():
            metrics = json.loads(metrics_path.read_text(encoding="utf-8"))
            row["geometry"] = metrics
            for call in metrics["calls"]:
                selection = call["continuity_selection"]
                if call.get("submitted", True):
                    assert call["prompt_tokens"] == selection["prompt_tokens_after"]
                    assert selection["memory_tokens"] <= selection["memory_cap"]
                    assert selection["prompt_tokens_after"] + selection["output_reserved"] + selection["technical_margin"] <= selection["context_tokens"]
            if metrics["success"] and len(state.get("history", [])) >= 2:
                paired = metrics["calls"][0]
        if row["returncode"] == 0:
            state_path = next_state
            state = json.loads(next_state.read_text(encoding="utf-8"))
        (out / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
        return row["returncode"] == 0

    turn("prendre le bloc")
    if not turn("east"):
        return 1
    a = state["current_place_id"]
    rooms = {room["room_id"]: room for room in state["generated_rooms"]}
    geometry_a = rooms[a]["scene_text"]
    description_a = rooms[a]["spatial_state"]["room_summary"]
    # Follow actual engine exits. Never invent a branch solely to make the recipe pass.
    blocked = state["spatial_state"]["blocked_exits"]
    departures = [d for d in OPPOSITE if d not in blocked and d != "west"]
    if departures:
        direction = departures[0]
        if turn(direction) and turn(OPPOSITE[direction]):
            assert state["current_place_id"] == a
            assert next(room for room in state["generated_rooms"] if room["room_id"] == a)["scene_text"] == geometry_a
            assert next(room for room in state["generated_rooms"] if room["room_id"] == a)["spatial_state"]["room_summary"] == description_a
            if len(departures) > 1:
                turn(departures[1])
            else:
                turn(direction)  # A real revisit when there is no second branch.
    if paired:
        # Same messages/instructions/seed; only remove the continuity DATA block.
        # Native probe isolates composition; it does not run local repairs or publish a room.
        for mode in ("with-continuity", "open-alone"):
            messages = [dict(message) for message in paired["messages"]]
            if mode == "open-alone":
                messages[1]["content"] = messages[1]["content"].split("\nCONTINUITY_V1:\n")[0]
            prompt = out / f"{mode}.messages.json"
            prompt.write_text(json.dumps(messages, ensure_ascii=False), encoding="utf-8")
            response = out / f"{mode}.response.json"
            row = run(mode, ["--llm-probe", prompt, "--llm-probe-output", response,
                             "--llm-seed", paired["seed"], "--llm-temperature", paired["temperature"],
                             "--llm-predict", 2048])
            if row["returncode"] == 0:
                data = json.loads(response.read_text(encoding="utf-8"))
                program = out / f"{mode}.scene"
                program.write_text(data["final"], encoding="utf-8")
                run(mode + "-render", ["--audit-scene-text", program, "--output", out / f"{mode}.png",
                                       "--width", 512, "--height", 256, "--samples", 8])
    (out / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    return int(any(row["returncode"] for row in results))


if __name__ == "__main__":
    raise SystemExit(main())
