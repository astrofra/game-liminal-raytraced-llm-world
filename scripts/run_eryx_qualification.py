"""Run isolated native turns, retaining saves and sampled resource measurements.

Narrative assertions remain review items. A successful process is not a quality
verdict. Every session turn loads the preceding save in a new native process.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import subprocess
import threading
import time
from pathlib import Path

import psutil

from model_profiles import DEFAULT_PROFILE, PROJECT_ROOT, get_profile, resolve_model_path


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def fingerprint(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def sampled_run(command, folder, timeout):
    start = time.perf_counter()
    peak_ram = 0
    peak_vram = None
    samples = 0
    timed_out = False
    with (folder / "stdout.txt").open("wb") as out, (folder / "stderr.txt").open("wb") as err:
        process = subprocess.Popen(command, cwd=PROJECT_ROOT, stdout=out, stderr=err)
        native = psutil.Process(process.pid)
        ended = []
        def wait_for_exit():
            process.wait()
            ended.append(time.perf_counter())
        waiter = threading.Thread(target=wait_for_exit, daemon=True)
        waiter.start()
        while waiter.is_alive():
            try:
                peak_ram = max(peak_ram, native.memory_info().rss)
            except psutil.NoSuchProcess:
                pass
            try:
                gpu = subprocess.run(
                    ["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits"],
                    capture_output=True, text=True, timeout=3,
                )
                if gpu.returncode == 0:
                    used = int(gpu.stdout.splitlines()[0])
                    peak_vram = max(peak_vram or 0, used)
                    samples += 1
            except (OSError, ValueError, subprocess.TimeoutExpired):
                pass
            if time.perf_counter() - start > timeout:
                timed_out = True
                process.kill()
                break
            time.sleep(0.5)
        waiter.join()
        code = process.returncode
    return {"returncode": code, "timeout": timed_out,
            "wall_time_ms": (ended[0] - start) * 1000,
            "sampled_peak_process_rss_bytes": peak_ram,
            "sampled_peak_device_vram_mib": peak_vram, "vram_samples": samples}


def inspect_state(previous, current, expected, command=""):
    errors = []
    actual = current["hard_state"]["current_location_id"]
    if expected is not None and actual != expected:
        errors.append(f"location: expected {expected}, got {actual}")
    serialized = json.dumps(current, ensure_ascii=False)
    if any(marker in serialized for marker in ("[THINK]", "[/THINK]", "<think>", "</think>")):
        errors.append("Reasoning marker leaked into persisted state")
    if previous:
        directions = {"north": "north", "nord": "north", "south": "south", "sud": "south",
                      "east": "east", "est": "east", "west": "west", "ouest": "west"}
        direction = directions.get(command.strip().lower())
        def links(state):
            return {(link["from_place_id"], link["direction"], link["to_place_id"])
                    for link in state.get("room_links", [])}
        old_links, new_links = links(previous), links(current)
        if not old_links.issubset(new_links):
            errors.append("Existing directed links changed after save/reload")
        for source, bearing, destination in old_links:
            if source == previous.get("current_place_id") and bearing == direction:
                if current.get("current_place_id") != destination:
                    errors.append(f"Known traversal did not reach persisted destination {destination}")
        barriers = {(b["place_id"], b["direction"]): b for b in current.get("invisible_barriers", [])}
        for barrier in previous.get("invisible_barriers", []):
            key = (barrier["place_id"], barrier["direction"])
            if key not in barriers or (barrier.get("discovered") and not barriers[key].get("discovered")):
                errors.append("Invisible barrier or its discovery was lost")
            if key == (previous.get("current_place_id"), direction):
                if (current.get("current_place_id") != previous.get("current_place_id") or
                    current["hard_state"]["move_count"] != previous["hard_state"]["move_count"] or
                    not barriers.get(key, {}).get("discovered")):
                    errors.append("Direct barrier contact changed position/count or was not discovered")
        old_rooms = {room["room_id"]: room for room in previous.get("generated_rooms", [])}
        new_rooms = {room["room_id"]: room for room in current.get("generated_rooms", [])}
        for room_id, room in old_rooms.items():
            if room_id not in new_rooms:
                errors.append(f"Generated room lost: {room_id}")
            elif room.get("scene_text") != new_rooms[room_id].get("scene_text"):
                errors.append(f"Generated scene changed: {room_id}")
        opposite = {"north": "south", "south": "north", "east": "west", "west": "east"}
        for room_id in new_rooms.keys() - old_rooms.keys():
            entrances = [(src, bearing) for src, bearing, dest in new_links if dest == room_id]
            if not entrances or not any((room_id, opposite.get(bearing), src) in new_links for src, bearing in entrances):
                errors.append(f"Generated room has no reciprocal return: {room_id}")
    return errors


def run_turn(args, case, folder, previous_path=None):
    folder.mkdir()
    save = folder / "state.json"
    if previous_path is None and args.fixtures:
        initial = json.loads((args.fixtures / (case["location"] + ".json")).read_text(encoding="utf-8"))
        initial["language"] = case.get("language", "fr")
        initial["hard_state"].update(case.get("hard_state_patch", {}))
        previous_path = folder / "initial-state.json"
        write_json(previous_path, initial)
    command_file = folder / "player-command.txt"
    command_file.write_text(case["command"] + "\n", encoding="utf-8")
    command = [str(args.exe), "--run-session", "--command-file", str(command_file),
               "--model", str(resolve_model_path(args.profile)), "--llm-predict", str(args.predict),
               "--width", "160", "--height", "90", "--samples", "1", "--bounces", "1",
               "--save-state", str(save), "--output", str(folder / "view.png")]
    if args.adapted:
        command += ["--llm-profile", args.profile, "--llm-seed", str(args.seed),
                    "--llm-reasoning-budget", str(args.reasoning_budget)]
    if args.legacy_template:
        command += ["--llm-legacy-template"]
    if previous_path:
        command += ["--load-state", str(previous_path)]
    else:
        command += ["--location", case["location"]]
    write_json(folder / "command.json", command)
    measurement = sampled_run(command, folder, args.timeout)
    output = (folder / "stdout.txt").read_text(encoding="utf-8", errors="replace")
    measurement.update(id=case["id"], command=case["command"],
                       narrative_review="pending", assertions=case.get("assertions", []))
    errors = []
    if measurement["returncode"] or not save.exists():
        errors.append("Native turn failed or did not save")
    else:
        current = json.loads(save.read_text(encoding="utf-8"))
        previous = json.loads(previous_path.read_text(encoding="utf-8")) if previous_path else None
        errors += inspect_state(previous, current, case.get("expected_location"), case["command"])
        measurement["current_place_id"] = current.get("current_place_id")
        measurement["generated_room_count"] = len(current.get("generated_rooms", []))
        if case.get("category") == "generation" and not current.get("generated_rooms"):
            errors.append("Exploration aperture did not create a room")
        if case.get("category") == "barrier":
            before_count = previous["hard_state"]["move_count"] if previous else 0
            if current["hard_state"]["move_count"] != before_count:
                errors.append("Barrier contact incremented move_count")
    measurement["invariant_errors"] = errors
    for label, key in (("Turn JSON repair", "repair"), ("Turn fallback", "fallback"),
                       ("Generated room metadata source", "room_metadata_source"),
                       ("Generated room scene source", "room_scene_source"),
                       ("Candidate scene audit", "candidate_scene_audit"),
                       ("Rendered scene source", "rendered_scene_source"),
                       ("Intent", "intent"),
                       ("Effective LLM temperature", "effective_temperature")):
        match = re.search(r"(?m)^" + re.escape(label) + r": (.+)$", output)
        measurement[key] = match.group(1).strip() if match else None
    narration = re.search(r"(?ms)^Narration: (.*?)(?=^(?:Clarification|Initial location):)", output)
    measurement["narration"] = narration.group(1).strip() if narration else None
    calls = []
    for line in (folder / "stderr.txt").read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("LIMINAL_LLM_METRIC "):
            calls.append(json.loads(line[len("LIMINAL_LLM_METRIC "):]))
    measurement["llm_calls"] = calls
    write_json(folder / "result.json", measurement)
    print(f"{case['id']}: {measurement['wall_time_ms']/1000:.1f}s, invariant errors={len(errors)}", flush=True)
    return measurement, save if save.exists() else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--profile", default=DEFAULT_PROFILE)
    parser.add_argument("--corpus", type=Path, default=PROJECT_ROOT / "scripts/eryx_qualification_cases.json")
    parser.add_argument("--adapted", action="store_true")
    parser.add_argument("--legacy-template", action="store_true")
    parser.add_argument("--seed", type=int, default=42)
    parser.add_argument("--reasoning-budget", type=int, default=512)
    parser.add_argument("--predict", type=int, default=768)
    parser.add_argument("--timeout", type=int, default=300)
    parser.add_argument("--cases-only", action="store_true")
    parser.add_argument("--fixtures", type=Path, help="Canonical initial states exported by the native engine; language and stress fixtures applied per case")
    parser.add_argument("--limit", type=int)
    parser.add_argument("--case", action="append", default=[], help="Select case IDs for repeated-seed studies")
    args = parser.parse_args()
    args.exe = args.exe.resolve()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    corpus = json.loads(args.corpus.read_text(encoding="utf-8"))
    selected = set(args.case)
    if selected - {case["id"] for case in corpus["cases"]}:
        raise ValueError("Unknown case ID in --case")
    model = get_profile(args.profile)
    model_path = resolve_model_path(args.profile)
    model_hash = fingerprint(model_path)
    if model_hash != model["sha256"] or model_path.stat().st_size != model["size_bytes"]:
        raise ValueError("Selected weights do not match the pinned profile")
    exe_hash = fingerprint(args.exe)
    build_provenance = None
    for record_path in (args.exe.parent / "provenance.json", args.exe.parent / "runtime-build.json"):
        if record_path.exists():
            build_provenance = json.loads(record_path.read_text(encoding="utf-8"))
            recorded_hash = build_provenance.get("exe_sha256", build_provenance.get("artifacts", {}).get(args.exe.name))
            if recorded_hash != exe_hash:
                raise ValueError("Executable hash differs from its recorded build")
            break
    write_json(args.out / "provenance.json", {
        "exe": str(args.exe), "exe_sha256": exe_hash, "build": build_provenance, "profile": model,
        "model_sha256_verified": model_hash,
        "fixtures": {p.name: fingerprint(p) for p in args.fixtures.glob("*.json")} if args.fixtures else None,
        "corpus_sha256": fingerprint(args.corpus), "seed": args.seed,
        "adapted": args.adapted, "legacy_template": args.legacy_template,
        "reasoning_budget": args.reasoning_budget, "final_budget": args.predict,
        "selected_case_ids": args.case,
        "resource_measurement": "sampled every approximately 0.5s; device VRAM includes other applications",
        "native_metrics": "legacy native counters omit some auxiliary calls; use full wall time",
    })
    results = []
    cases = [case for case in corpus["cases"] if not selected or case["id"] in selected]
    for case in cases[:args.limit]:
        result, _ = run_turn(args, case, args.out / case["id"])
        results.append(result)
        write_json(args.out / "results.json", results)
    if not args.cases_only:
        for session in corpus["sessions"]:
            previous = None
            checkpoints = {x["after_turn"]: x["expected_location"] for x in session["checkpoints"]}
            for index, command in enumerate(session["commands"], 1):
                case = dict(id=f"{session['id']}-{index:02}", location=session["location"],
                            command=command, expected_location=checkpoints.get(index),
                            assertions=session["assertions"])
                result, previous = run_turn(args, case, args.out / case["id"], previous)
                results.append(result)
                write_json(args.out / "results.json", results)
                if previous is None:
                    break
    durations = sorted(x["wall_time_ms"] for x in results)
    write_json(args.out / "summary.json", {
        "turns": len(results), "invariant_failures": sum(bool(x["invariant_errors"]) for x in results),
        "p50_wall_ms": durations[len(durations)//2] if durations else None,
        "p95_wall_ms": durations[max(0, math.ceil(len(durations)*.95)-1)] if durations else None,
        "narrative_review": "pending: do not equate invariants with artistic or narrative quality",
    })
    return int(any(x["invariant_errors"] for x in results))


if __name__ == "__main__":
    raise SystemExit(main())
