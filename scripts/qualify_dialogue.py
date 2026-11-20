"""Run SDL dialogue recipes and check the interaction archive against engine state."""

import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SHORTCUTS = ["N", "S", "Nord", "Sud", "O", "Ouest", "i", "inventaire", "inventory"]
EXPANDED_SHORTCUTS = ["NORD", "SUD", "NORD", "SUD", "OUEST", "OUEST", "INVENTAIRE", "INVENTAIRE", "INVENTAIRE"]


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def inspect_archive(run, commands, dialogue):
    interactions = sorted((run / "interactions").glob("[0-9]*"))
    problems = []
    rows = []
    startup_generation = None
    if len(interactions) != len(commands) + 1:
        problems.append(f"Expected {len(commands) + 1} interaction folders, found {len(interactions)}")
    for index, folder in enumerate(interactions):
        try:
            row = {}
            completion = read_json(folder / "completion.json")
            assert completion["complete"], completion["error"]
            presentation = read_json(folder / "presentation.json")
            assert presentation["presented"] and presentation["width"] > 0
            assert (folder / "screen.png").read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
            scene = read_json(folder / "scene.json")
            assert (run / scene["base"]).is_file()
            assert hashlib.sha256((run / scene["effective"]).read_bytes()).hexdigest() == scene["effective_sha256"]
            assert read_json(folder / "audio.json")["published_targets"] is not None
            before = read_json(folder / "state-before.json")
            after = read_json(folder / "state-after.json")
            calls = []
            for request_path in sorted((folder / "calls").glob("*-request.json")):
                stem = request_path.name.removesuffix("-request.json")
                request = read_json(request_path)
                response = read_json(request_path.with_name(stem + "-response.json"))
                assert request_path.with_name(stem + "-raw.txt").is_file()
                assert response["purpose"] == request["purpose"]
                if response["success"]:
                    assert request["formatted_prompt"]
                    assert request["prompt_tokens"] + request["n_predict"] <= request["n_ctx"]
                calls.append({"purpose": request["purpose"], "wall_ms": response["wall_ms"],
                              "load_ms": response["load_ms"], "prompt_tokens": request["prompt_tokens"],
                              "generated_tokens": response["generated_tokens"]})
            assert len(calls) == completion["llm_calls"]
            if index == 0:
                assert not calls and before == after
                startup_generation = presentation["generation"]
                continue
            turn = read_json(run / completion["turn"])
            assert turn["command"].casefold() == commands[index - 1].casefold(), "command differs after shortcut expansion"
            if turn["status"] == "accepted":
                assert presentation["text"] == turn["description"]
            row = {"command": turn["command"], "intent": turn["intent"], "response": presentation["text"],
                   "calls": calls, "generation": presentation["generation"],
                   "view_reused": turn["view_reused"], "ms": presentation["submission_to_present_ms"],
                   "archive_ms": completion["presentation_archive_ms"]}
            rows.append(row)
            if dialogue:
                for state in (before, after):
                    state.pop("history")
                assert before == after, "simulation state changed"
                assert turn["intent"] == "conversation", turn["intent"]
                assert turn["view_reused"] and presentation["generation"] == startup_generation
                assert not turn["effects"]["raytrace_required"]
                assert [call["purpose"] for call in calls] == ["player_interpretation_v4", "game_master_dialogue_v2"]
                metrics = json.loads(turn["narrative_metrics"])
                assert not metrics["fallback"], "fallback used"
                for subset in (json.loads(turn["interaction"]), metrics):
                    budget = subset["continuity_selection"]
                    assert budget["prompt_tokens_after"] + budget["output_reserved"] + budget["technical_margin"] <= budget["context_tokens"]
        except (AssertionError, OSError, ValueError, KeyError, IndexError) as error:
            problems.append(f"{folder.name}: {type(error).__name__} {error}")
    return rows, problems


def inspect_journey(run, rows):
    if len(rows) != 7:
        return ["The journey did not finish all seven interactions"]
    try:
        def state(index):
            return read_json(run / "interactions" / f"{index:05d}" / "state-after.json")

        assert rows[0]["intent"] == "read_inventory" and not rows[0]["calls"] and rows[0]["view_reused"]
        assert rows[1]["intent"] == "move_west_blocked" and not rows[1]["calls"] and rows[1]["view_reused"]
        assert rows[2]["intent"] == "object_drop" and not rows[2]["view_reused"]
        origin = state(3)["current_place_id"]
        assert state(3)["objects"]["deposits"][origin][0]["object_id"] == "boundary chalk"
        assert rows[3]["intent"] == "conversation" and rows[3]["view_reused"]
        assert state(5)["current_place_id"] != origin
        assert state(6)["current_place_id"] == origin
        assert state(6)["objects"]["deposits"][origin] == state(3)["objects"]["deposits"][origin]
        assert state(6)["generated_rooms"] == state(5)["generated_rooms"]
        assert rows[6]["intent"] == "object_take" and not rows[6]["view_reused"]
        assert "boundary chalk" in state(7)["hard_state"]["inventory_items"]
        assert not state(7)["objects"]["deposits"].get(origin)
        return []
    except (AssertionError, KeyError, OSError, ValueError) as error:
        return [f"Journey check: {error}"]


def qualify_objects(exe, out, seed):
    initial = out / "initial.json"
    subprocess.run([exe, "--write-initial-state", str(initial)], cwd=ROOT, check=True)
    start = read_json(initial)
    start["language"] = "fr"
    place = start["current_place_id"]
    cases = {
        "synonym": (["Je ramasse le couteau.", "Je le repose."], ["object_take", "object_drop"], 1),
        "ambiguity": (["Je ramasse un couteau.", "Celui au manche sombre."], ["clarify_object", "object_take"], 2),
    }
    report = {}
    for label, (commands, expected, number) in cases.items():
        state = json.loads(json.dumps(start))
        deposits = []
        for index, suffix in enumerate(("sombre", "clair")[:number]):
            object_id = f"fixture:dague-{suffix}"
            state["objects"]["instances"][object_id] = {
                "id": object_id, "label": f"dague à manche {suffix}", "aliases": [],
                "description": f"Une dague à manche {suffix}.", "created_turn": 0,
                "origin_place": "", "portable": True, "source_quote": "", "visual_id": "",
            }
            deposits.append({"object_id": object_id, "x": index * 0.5, "y": 0.0, "z": 0.6})
        state["objects"]["deposits"][place] = deposits
        fixture = out / f"{label}.before.json"
        fixture.write_text(json.dumps(state, ensure_ascii=False), encoding="utf-8")
        inputs = out / f"{label}.txt"
        inputs.write_text("\n".join(commands) + "\n", encoding="utf-8")
        final = out / f"{label}.after.json"
        with (out / f"{label}.log").open("wb") as log:
            proc = subprocess.run([exe, "--run-session", "--load-state", str(fixture), "--command-file", str(inputs),
                "--llm-seed", str(seed), "--save-state", str(final),
                "--output", str(out / f"{label}.png"), "--width", "320", "--height", "160", "--samples", "1"],
                cwd=ROOT, stdout=log, stderr=log, timeout=240)
        entry = {"problems": []}
        if proc.returncode or not final.is_file():
            entry["problems"].append(f"Game returned {proc.returncode} or final fixture is missing")
        else:
            result = read_json(final)
            intents = [row["intent"] for row in result["history"]]
            entry["intents"] = intents
            entry["replies"] = [row["narration"] for row in result["history"]]
            if intents != expected:
                entry["problems"].append(f"Expected {expected}, got {intents}")
            if set(result["objects"]["instances"]) != set(state["objects"]["instances"]):
                entry["problems"].append("Instance identities changed")
            if label == "synonym" and result["objects"]["revision"] != 2:
                entry["problems"].append("Expected exactly two transfers")
            if label == "ambiguity" and (
                set(result["history"][0].get("clarification_candidates", [])) != {obj["object_id"] for obj in deposits}
                or "fixture:dague-sombre" not in result["hard_state"]["inventory_items"]
                or result["objects"]["revision"] != 1
            ):
                entry["problems"].append("Clarification did not preserve candidates and select the dark-handled dagger")
        report[label] = entry
        print(f"{label}: {len(entry['problems'])} issue(s)", flush=True)
        (out / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    return int(any(entry["problems"] for entry in report.values()))


def inspect_shortcuts(run, rows):
    expected = ["move_north_generated_room", "move_south_known_room", "move_north_known_room",
                "move_south_known_room", "move_west_blocked", "move_west_blocked",
                "read_inventory", "read_inventory", "read_inventory"]
    if [row["intent"] for row in rows] != expected:
        return [f"Unexpected shortcut outcomes: {[row['intent'] for row in rows]}"]
    try:
        assert not any(call["purpose"].startswith("player_interpretation") for row in rows for call in row["calls"])
        assert not any(call["purpose"].startswith("scene_program") for row in rows[1:] for call in row["calls"])
        assert all(row["view_reused"] and not row["calls"] for row in rows[4:])
        states = [read_json(run / "interactions" / f"{i:05d}" / "state-after.json") for i in range(10)]
        place = [state["current_place_id"] for state in states]
        assert place[0] == place[2] == place[4] == place[-1]
        assert place[1] == place[3] and place[1] != place[0]
        assert states[-1]["hard_state"]["move_count"] == 4
        assert states[-1]["hard_state"]["inventory_items"] == states[0]["hard_state"]["inventory_items"]
        assert states[-1]["objects"] == states[4]["objects"]
        assert len(states[-1]["generated_rooms"]) == 1
    except (AssertionError, KeyError, OSError, ValueError) as error:
        return [f"Shortcut check: {error}"]
    return []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT / "build_native/Release/liminal_cornell_renderer.exe")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--suite", choices=("questions", "followups", "languages", "journey", "objects", "shortcuts"), required=True)
    parser.add_argument("--seed", type=int, default=4242)
    args = parser.parse_args()
    out = args.out.resolve()
    if out.is_relative_to(ROOT) or out.exists():
        parser.error("--out must be a new directory outside the repository")
    out.mkdir(parents=True)
    if args.suite == "objects":
        return qualify_objects(str(args.exe.resolve()), out, args.seed)
    corpus = read_json(ROOT / "tests/data/dialogue_cases.json")
    if args.suite == "questions":
        runs = {"fr": [row["input"] for row in corpus["questions_fr"]]}
    elif args.suite == "followups":
        runs = {"fr": corpus["followups_fr"]}
    elif args.suite == "languages":
        runs = corpus["questions_other_languages"]
    elif args.suite == "shortcuts":
        runs = {"fr": SHORTCUTS}
    else:
        runs = {"fr": ["inventaire", "ouest", "Je pose ma craie.", "Qui est passé ici ?",
                       "est", "ouest", "Je reprends ce que je viens de poser."]}
    report = {}
    for language, commands in runs.items():
        command_file = out / f"{language}.txt"
        command_file.write_text("\n".join(commands) + "\n", encoding="utf-8")
        archive_root = out / language
        with (out / f"{language}.log").open("wb") as log:
            proc = subprocess.run([str(args.exe.resolve()), "--sdl", "--sdl-smoke-language", language,
                "--sdl-smoke-test-ms", "840000", "--command-file", str(command_file),
                "--research-dir", str(archive_root), "--llm-seed", str(args.seed),
                "--width", "640", "--height", "320", "--samples", "2"], cwd=ROOT,
                stdout=log, stderr=log, timeout=900)
        runs_found = list(archive_root.glob("run-*"))
        if len(runs_found) != 1:
            report[language] = {"problems": [f"Expected one archive, found {len(runs_found)}"]}
        else:
            effective_commands = EXPANDED_SHORTCUTS if args.suite == "shortcuts" else commands
            rows, problems = inspect_archive(runs_found[0], effective_commands, args.suite not in ("journey", "shortcuts"))
            if args.suite == "journey":
                problems.extend(inspect_journey(runs_found[0], rows))
            elif args.suite == "shortcuts":
                problems.extend(inspect_shortcuts(runs_found[0], rows))
            if proc.returncode:
                problems.append(f"Game returned {proc.returncode}")
            warm = sorted(row["ms"] for row in rows[1:])
            inference_wall = [sum(call["wall_ms"] for call in row["calls"]) for row in rows[1:]]
            report[language] = {"archive": str(runs_found[0]), "interactions": rows, "problems": problems,
                                "warm_median_ms": statistics.median(warm) if warm else None,
                                "warm_p95_ms": warm[math.ceil(0.95 * len(warm)) - 1] if warm else None,
                                "warm_llm_median_ms": statistics.median(inference_wall) if inference_wall else None,
                                "archive_median_ms": statistics.median(row["archive_ms"] for row in rows) if rows else None}
        print(f"{language}: {len(report[language]['problems'])} issue(s)", flush=True)
        (out / "results.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    return int(any(entry["problems"] for entry in report.values()))


if __name__ == "__main__":
    raise SystemExit(main())
