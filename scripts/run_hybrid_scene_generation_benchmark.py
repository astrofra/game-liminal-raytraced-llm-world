#!/usr/bin/env python3

import argparse
import datetime as dt
import json
import pathlib
import re
import subprocess
import sys
import time


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_RENDERER = REPO_ROOT / "build" / "Release" / "liminal_cornell_renderer.exe"
DEFAULT_MODEL = REPO_ROOT / "models" / "ministral-3-8b" / "Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
DEFAULT_CASES = REPO_ROOT / "scripts" / "scene_generation_benchmark_cases.json"
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "documentation" / "generated" / "hybrid_scene_generation_benchmark"
DEFAULT_MARKDOWN = REPO_ROOT / "documentation" / "HYBRID_SCENE_GENERATION_BENCHMARK.md"

DEFAULT_WIDTH = 800
DEFAULT_HEIGHT = 400
DEFAULT_SAMPLES = 8
DEFAULT_BOUNCES = 3
DEFAULT_DIRECT_SAMPLES = 1
DEFAULT_RENDER_SEED = 77
DEFAULT_EXPOSURE = 1.0

DEFAULT_PREDICT = 1024
DEFAULT_TEMPERATURE = 0.0

CANONICAL_LOCATIONS = {"gate", "server_aisles", "roof_watch"}
CARDINAL_ORDER = ("north", "east", "south", "west")


def load_cases(path):
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if not isinstance(payload, list):
        raise ValueError("Benchmark cases file must contain a JSON array.")
    return payload


def ensure_file(path, label):
    if not path.is_file():
        raise FileNotFoundError(f"{label} not found: {path}")


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def write_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8", newline="\n")


def relative_to_markdown(markdown_path, target_path):
    return str(target_path.relative_to(markdown_path.parent)).replace("\\", "/")


def run_command(command, cwd):
    start = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    duration_ms = (time.perf_counter() - start) * 1000.0
    return completed, duration_ms


def build_spatial_state(case):
    return {
        "location_id": case.get("location_id", "unknown"),
        "room_title": case.get("room_title", ""),
        "room_summary": case.get("room_summary", ""),
        "location_archetype": case.get("location_archetype", ""),
        "canonical_fixture": case.get("canonical_fixture", ""),
        "time_of_day": case.get("time_of_day", "unknown"),
        "visibility_level": case.get("visibility_level", "unknown"),
        "desert_state": case.get("desert_state", "unknown"),
        "interior_density": case.get("interior_density", "unknown"),
        "alert_level": int(case.get("alert_level", 1)),
        "anchors": case.get("anchors", []),
        "visible_objects": case.get("visible_objects", []),
        "blocked_exits": case.get("blocked_exits", []),
        "spatial_anomalies": case.get("spatial_anomalies", []),
    }


def build_origin_session_state(case):
    spatial_state = build_spatial_state(case)
    location_id = spatial_state["location_id"]
    is_canonical = location_id in CANONICAL_LOCATIONS

    current_place_id = f"canonical:{location_id}" if is_canonical else f"generated:origin_{case['slug']}"
    generated_rooms = []
    if not is_canonical:
        generated_rooms.append(
            {
                "room_id": current_place_id,
                "spatial_state": spatial_state,
                "scene_text": "",
                "scene_source": "benchmark_origin",
                "metadata_fallback_used": False,
                "scene_fallback_used": False,
            }
        )

    return {
        "hard_state": {
            "turn_number": 1,
            "move_count": 0,
            "score": 0,
            "current_location_id": location_id if is_canonical else "unknown",
            "alert_level": spatial_state["alert_level"],
            "cooling_state": "stable",
            "water_state": "stable",
            "power_state": "stable",
            "inventory_items": [],
            "named_entities": [],
            "unresolved_threats": [],
        },
        "soft_state": {
            "rolling_summary": case.get("room_summary", ""),
            "atmosphere": case.get("location_archetype", ""),
            "active_hypotheses": [],
            "tolerated_incoherences": [],
        },
        "spatial_state": spatial_state,
        "history": [],
        "current_place_id": current_place_id,
        "next_generated_room_index": 1,
        "generated_rooms": generated_rooms,
        "room_links": [],
    }


def choose_generation_direction(case):
    explicit = case.get("generation_direction", "").strip().lower()
    if explicit:
        if explicit not in CARDINAL_ORDER:
            raise ValueError(f"Invalid generation_direction for case {case['slug']}: {explicit}")
        return explicit

    blocked = {value.strip().lower() for value in case.get("blocked_exits", []) if isinstance(value, str)}
    for direction in CARDINAL_ORDER:
        if direction not in blocked:
            return direction

    raise ValueError(f"No open traversal direction found for case {case['slug']}.")


def dump_generated_room_prompt(renderer_path, state_path, direction):
    command = [
        str(renderer_path),
        "--dump-generated-room-prompt",
        "--generated-room-direction",
        direction,
        "--load-state",
        str(state_path),
    ]
    return run_command(command, REPO_ROOT)


def run_hybrid_turn(
    renderer_path,
    model_path,
    initial_state_path,
    final_state_path,
    direction,
    image_path,
    width,
    height,
    samples,
    bounces,
    direct_samples,
    render_seed,
    exposure,
    n_predict,
    temperature,
    use_json_grammar,
):
    image_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(renderer_path),
        "--run-turn",
        "--load-state",
        str(initial_state_path),
        "--save-state",
        str(final_state_path),
        "--model",
        str(model_path),
        "--command",
        direction,
        "--output",
        str(image_path),
        "--width",
        str(width),
        "--height",
        str(height),
        "--samples",
        str(samples),
        "--bounces",
        str(bounces),
        "--direct-samples",
        str(direct_samples),
        "--seed",
        str(render_seed),
        "--exposure",
        str(exposure),
        "--llm-predict",
        str(n_predict),
        "--llm-temperature",
        str(temperature),
        "--dump-raw-turn",
    ]
    command.append("--use-json-grammar" if use_json_grammar else "--no-json-grammar")
    return run_command(command, REPO_ROOT)


def extract_section(text, start_header, end_headers):
    start = text.find(start_header)
    if start == -1:
        return ""
    start += len(start_header)
    end = len(text)
    for header in end_headers:
        position = text.find(header, start)
        if position != -1 and position < end:
            end = position
    return text[start:end].strip()


def extract_first_json_object(text):
    start = text.find("{")
    if start == -1:
        return ""

    depth = 0
    in_string = False
    escaping = False
    for index in range(start, len(text)):
        current = text[index]
        if escaping:
            escaping = False
            continue
        if current == "\\" and in_string:
            escaping = True
            continue
        if current == '"':
            in_string = not in_string
            continue
        if in_string:
            continue
        if current == "{":
            depth += 1
        elif current == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    return ""


def parse_key_value_lines(stdout_text):
    parsed = {}
    patterns = {
        "prompt_tokens": r"Prompt tokens:\s*(\d+)",
        "generated_tokens": r"Generated tokens:\s*(\d+)",
        "inference_time_ms": r"Inference time:\s*([0-9.]+)\s*ms",
        "preparation_time_ms": r"Preparation time:\s*([0-9.]+)\s*ms",
        "render_time_ms": r"Render time:\s*([0-9.]+)\s*ms",
        "turn_fallback": r"Turn fallback:\s*(.+)",
        "intent": r"Intent:\s*(.+)",
        "narration": r"Narration:\s*(.+)",
        "clarification": r"Clarification:\s*(.+)",
        "updated_place": r"Updated place:\s*(.+)",
        "rendered_scene_source": r"Rendered scene source:\s*(.+)",
        "metadata_source": r"Generated room metadata source:\s*(.+)",
        "scene_source": r"Generated room scene source:\s*(.+)",
        "cache_refresh_source": r"Generated room cache refresh source:\s*(.+)",
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, stdout_text)
        if not match:
            continue
        value = match.group(1).strip()
        if key in {"prompt_tokens", "generated_tokens"}:
            parsed[key] = int(value)
        elif key in {"inference_time_ms", "preparation_time_ms", "render_time_ms"}:
            parsed[key] = float(value)
        else:
            parsed[key] = value

    geometry_match = re.search(
        r"Loaded (\d+) triangles, (\d+) materials, (\d+) emissive triangles",
        stdout_text,
    )
    if geometry_match:
        parsed["triangles"] = int(geometry_match.group(1))
        parsed["materials"] = int(geometry_match.group(2))
        parsed["emissive_triangles"] = int(geometry_match.group(3))

    return parsed


def load_json_if_present(path):
    if not path.is_file():
        return None
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def extract_current_generated_room(session_state):
    if not isinstance(session_state, dict):
        return None
    current_place_id = session_state.get("current_place_id", "")
    generated_rooms = session_state.get("generated_rooms", [])
    if not isinstance(generated_rooms, list):
        return None
    for room in generated_rooms:
        if isinstance(room, dict) and room.get("room_id") == current_place_id:
            return room
    return None


def render_case(
    case,
    renderer_path,
    model_path,
    output_root,
    width,
    height,
    samples,
    bounces,
    direct_samples,
    render_seed,
    exposure,
    n_predict,
    temperature,
    use_json_grammar,
):
    slug = case["slug"]
    direction = choose_generation_direction(case)

    state_path = output_root / "states" / f"{slug}.origin.session.json"
    final_state_path = output_root / "states" / f"{slug}.result.session.json"
    prompt_path = output_root / "prompts" / f"{slug}.prompt.txt"
    prompt_stderr_path = output_root / "prompts" / f"{slug}.prompt.stderr.txt"
    stdout_path = output_root / "logs" / f"{slug}.stdout.txt"
    stderr_path = output_root / "logs" / f"{slug}.stderr.txt"
    raw_response_path = output_root / "responses" / f"{slug}.raw.txt"
    generated_room_json_path = output_root / "responses" / f"{slug}.generated_room.json"
    scene_path = output_root / "scenes" / f"{slug}.scene"
    image_path = output_root / "images" / f"{slug}.png"
    generated_room_state_path = output_root / "states" / f"{slug}.generated_room.json"

    result = {
        "slug": slug,
        "title": case.get("room_title", slug),
        "direction": direction,
        "status": "unknown",
        "valid_scene": False,
        "prompt_time_ms": 0.0,
        "command_time_ms": 0.0,
        "prompt_tokens": None,
        "generated_tokens": None,
        "inference_time_ms": None,
        "preparation_time_ms": None,
        "render_time_ms": None,
        "compile_time_ms_estimate": None,
        "triangles": None,
        "materials": None,
        "turn_fallback": "",
        "intent": "",
        "updated_place": "",
        "rendered_scene_source": "",
        "metadata_source": "",
        "scene_source": "",
        "scene_lines": 0,
        "scene_characters": 0,
        "error": "",
        "state_path": str(state_path),
        "final_state_path": str(final_state_path),
        "generated_room_state_path": str(generated_room_state_path),
        "prompt_path": str(prompt_path),
        "stdout_path": str(stdout_path),
        "stderr_path": str(stderr_path),
        "raw_response_path": str(raw_response_path),
        "generated_room_json_path": str(generated_room_json_path),
        "scene_path": str(scene_path),
        "image_path": str(image_path),
    }

    write_json(state_path, build_origin_session_state(case))

    prompt_completed, prompt_duration_ms = dump_generated_room_prompt(renderer_path, state_path, direction)
    result["prompt_time_ms"] = prompt_duration_ms
    write_text(prompt_path, prompt_completed.stdout)
    write_text(prompt_stderr_path, prompt_completed.stderr)
    if prompt_completed.returncode != 0:
        result["status"] = "prompt_failed"
        result["error"] = (prompt_completed.stderr or prompt_completed.stdout).strip()
        return result

    command_completed, command_duration_ms = run_hybrid_turn(
        renderer_path,
        model_path,
        state_path,
        final_state_path,
        direction,
        image_path,
        width,
        height,
        samples,
        bounces,
        direct_samples,
        render_seed,
        exposure,
        n_predict,
        temperature,
        use_json_grammar,
    )
    result["command_time_ms"] = command_duration_ms
    write_text(stdout_path, command_completed.stdout)
    write_text(stderr_path, command_completed.stderr)

    raw_turn_section = extract_section(
        command_completed.stdout,
        "=== Raw Turn Response ===\n",
        (
            "\n=== Repair Turn Response ===",
            "\n=== Raw Scene Audit Response ===",
            "\nLoaded scene ",
        ),
    )
    write_text(raw_response_path, raw_turn_section + ("\n" if raw_turn_section else ""))

    raw_scene_section = extract_section(
        command_completed.stdout,
        "=== Raw Scene Audit Response ===\n",
        (
            "\nLoaded scene ",
            "\nPreparation time:",
            "\nRender time:",
        ),
    )
    if raw_scene_section:
        if not raw_scene_section.endswith("\n"):
            raw_scene_section += "\n"
        write_text(scene_path, raw_scene_section)
        result["scene_characters"] = len(raw_scene_section)
        result["scene_lines"] = len([line for line in raw_scene_section.splitlines() if line.strip()])

    generated_room_json_text = extract_first_json_object(raw_turn_section)
    if generated_room_json_text:
        try:
            write_json(generated_room_json_path, json.loads(generated_room_json_text))
        except json.JSONDecodeError:
            write_text(generated_room_json_path, generated_room_json_text + "\n")

    stats = parse_key_value_lines(command_completed.stdout)
    result["prompt_tokens"] = stats.get("prompt_tokens")
    result["generated_tokens"] = stats.get("generated_tokens")
    result["inference_time_ms"] = stats.get("inference_time_ms")
    result["preparation_time_ms"] = stats.get("preparation_time_ms")
    result["render_time_ms"] = stats.get("render_time_ms")
    result["triangles"] = stats.get("triangles")
    result["materials"] = stats.get("materials")
    result["turn_fallback"] = stats.get("turn_fallback", "")
    result["intent"] = stats.get("intent", "")
    result["updated_place"] = stats.get("updated_place", "")
    result["rendered_scene_source"] = stats.get("rendered_scene_source", "")
    result["metadata_source"] = stats.get("metadata_source", "")
    result["scene_source"] = stats.get("scene_source", stats.get("cache_refresh_source", ""))

    if result["preparation_time_ms"] is not None and result["inference_time_ms"] is not None:
        result["compile_time_ms_estimate"] = max(result["preparation_time_ms"] - result["inference_time_ms"], 0.0)

    final_state = load_json_if_present(final_state_path)
    current_generated_room = extract_current_generated_room(final_state)
    if current_generated_room is not None:
        write_json(generated_room_state_path, current_generated_room)

    if command_completed.returncode != 0:
        result["status"] = "turn_failed"
        result["error"] = (command_completed.stderr or command_completed.stdout).strip()
        return result

    result["valid_scene"] = True
    if result["turn_fallback"].lower().startswith("applied") or result["metadata_source"] == "fallback" or result["scene_source"] == "fallback":
        result["status"] = "valid_with_fallback"
    else:
        result["status"] = "valid"
    return result


def write_markdown(markdown_path, cases, results, args):
    generated_at = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total_cases = len(results)
    valid_cases = sum(1 for item in results if item["valid_scene"])
    fallback_cases = sum(1 for item in results if item["status"] == "valid_with_fallback")
    case_lookup = {case["slug"]: case for case in cases}

    lines = [
        "# Hybrid Scene Generation Benchmark",
        "",
        f"Genere le {generated_at}.",
        "",
        "Ce document audite la nouvelle chaine runtime :",
        "- room brief source",
        "- commande cardinale",
        "- metadata JSON produite par le LLM local",
        "- compilation hybride deterministe `SpatialState -> .scene`",
        "- audit du `.scene` compile et rendu PNG",
        "",
        "Comparaison :",
        "- le benchmark historique `SCENE_GENERATION_BENCHMARK.md` mesure la voie directe `brief -> .scene brut par Ministral`",
        "- ce benchmark mesure la voie runtime `brief source -> room JSON -> compilateur hybride -> rendu`",
        "",
        "Parametres :",
        f"- rendu : `{args.width}x{args.height}`, `{args.samples}` spp, `{args.bounces}` bounces, `{args.direct_samples}` direct samples",
        f"- runtime LLM : `temperature={args.temperature}`, `n_predict={args.n_predict}`, `json_grammar={'on' if args.use_json_grammar else 'off'}`",
        "",
        f"Resultat global : `{valid_cases}/{total_cases}` rendus valides, dont `{fallback_cases}` avec fallback.",
        "",
        "| Case | Dir | Status | Metadata | Scene | Infer ms | Prep ms | Render ms | Triangles |",
        "| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: |",
    ]

    for item in results:
        lines.append(
            f"| `{item['slug']}` | `{item['direction'].upper()}` | `{item['status']}` | "
            f"`{item['metadata_source'] or '-'}` | `{item['scene_source'] or '-'}` | "
            f"{item['inference_time_ms'] or 0:.2f} | {item['preparation_time_ms'] or 0:.2f} | "
            f"{item['render_time_ms'] or 0:.2f} | {item['triangles'] or 0} |"
        )

    lines.append("")

    for item in results:
        case = case_lookup[item["slug"]]
        lines.extend(
            [
                f"## {item['title']}",
                "",
                case.get("room_summary", ""),
                "",
                f"Direction benchmarkee : `{item['direction']}`",
                f"Status : `{item['status']}`",
                f"Updated place : `{item['updated_place'] or '(unknown)'}`",
                f"Intent : `{item['intent'] or '(unknown)'}`",
                f"Metadata source : `{item['metadata_source'] or '(unknown)'}`",
                f"Scene source : `{item['scene_source'] or '(unknown)'}`",
                f"Rendered scene source : `{item['rendered_scene_source'] or '(unknown)'}`",
                f"Turn fallback : `{item['turn_fallback'] or '(unknown)'}`",
                f"Prompt dump : `{item['prompt_time_ms']:.2f} ms`",
                f"Command wall time : `{item['command_time_ms']:.2f} ms`",
                f"Inference time : `{(item['inference_time_ms'] or 0):.2f} ms`",
                f"Preparation time : `{(item['preparation_time_ms'] or 0):.2f} ms`",
                f"Compile estimate : `{(item['compile_time_ms_estimate'] or 0):.2f} ms`",
                f"Render time : `{(item['render_time_ms'] or 0):.2f} ms`",
                f"Prompt tokens : `{item['prompt_tokens'] or 0}`",
                f"Generated tokens : `{item['generated_tokens'] or 0}`",
                f"Triangles : `{item['triangles'] or 0}`",
                f"Materials : `{item['materials'] or 0}`",
                f"Scene lines : `{item['scene_lines']}`",
                f"Scene chars : `{item['scene_characters']}`",
                "",
                f"Origin state : `{relative_to_markdown(markdown_path, pathlib.Path(item['state_path']))}`",
                f"Result state : `{relative_to_markdown(markdown_path, pathlib.Path(item['final_state_path']))}`",
                f"Current generated room : `{relative_to_markdown(markdown_path, pathlib.Path(item['generated_room_state_path']))}`",
                f"Prompt : `{relative_to_markdown(markdown_path, pathlib.Path(item['prompt_path']))}`",
                f"Run stdout : `{relative_to_markdown(markdown_path, pathlib.Path(item['stdout_path']))}`",
                f"Run stderr : `{relative_to_markdown(markdown_path, pathlib.Path(item['stderr_path']))}`",
                f"Raw room JSON : `{relative_to_markdown(markdown_path, pathlib.Path(item['raw_response_path']))}`",
                f"Normalized room JSON : `{relative_to_markdown(markdown_path, pathlib.Path(item['generated_room_json_path']))}`",
                f"Compiled scene : `{relative_to_markdown(markdown_path, pathlib.Path(item['scene_path']))}`",
                "",
                "Source room brief :",
                f"- location_id: `{case.get('location_id', 'unknown')}`",
                f"- location_archetype: `{case.get('location_archetype', '')}`",
                f"- time_of_day: `{case.get('time_of_day', 'unknown')}`",
                f"- visibility_level: `{case.get('visibility_level', 'unknown')}`",
                f"- desert_state: `{case.get('desert_state', 'unknown')}`",
                f"- interior_density: `{case.get('interior_density', 'unknown')}`",
                f"- alert_level: `{case.get('alert_level', 1)}`",
                f"- blocked_exits: `{', '.join(case.get('blocked_exits', [])) or '(none)'}`",
                "",
            ]
        )

        if item["valid_scene"] and pathlib.Path(item["image_path"]).is_file():
            lines.extend(
                [
                    f"![{item['slug']}]({relative_to_markdown(markdown_path, pathlib.Path(item['image_path']))})",
                    "",
                ]
            )
        else:
            lines.extend(
                [
                    "Erreur :",
                    "",
                    "```text",
                    item["error"] or "(no error text)",
                    "```",
                    "",
                ]
            )

    write_text(markdown_path, "\n".join(lines))


def main():
    parser = argparse.ArgumentParser(description="Run a reproducible benchmark for the hybrid generated-room runtime chain.")
    parser.add_argument("--renderer", type=pathlib.Path, default=DEFAULT_RENDERER)
    parser.add_argument("--model", type=pathlib.Path, default=DEFAULT_MODEL)
    parser.add_argument("--cases", type=pathlib.Path, default=DEFAULT_CASES)
    parser.add_argument("--output-root", type=pathlib.Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--markdown", type=pathlib.Path, default=DEFAULT_MARKDOWN)
    parser.add_argument("--width", type=int, default=DEFAULT_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT)
    parser.add_argument("--samples", type=int, default=DEFAULT_SAMPLES)
    parser.add_argument("--bounces", type=int, default=DEFAULT_BOUNCES)
    parser.add_argument("--direct-samples", type=int, default=DEFAULT_DIRECT_SAMPLES)
    parser.add_argument("--render-seed", type=int, default=DEFAULT_RENDER_SEED)
    parser.add_argument("--exposure", type=float, default=DEFAULT_EXPOSURE)
    parser.add_argument("--n-predict", type=int, default=DEFAULT_PREDICT)
    parser.add_argument("--temperature", type=float, default=DEFAULT_TEMPERATURE)
    parser.add_argument("--use-json-grammar", action="store_true")
    parser.add_argument("--case", action="append", default=[], help="Run only the specified case slug. May be repeated.")
    args = parser.parse_args()

    renderer_path = args.renderer.resolve()
    model_path = args.model.resolve()
    cases_path = args.cases.resolve()
    output_root = args.output_root.resolve()
    markdown_path = args.markdown.resolve()

    ensure_file(renderer_path, "Renderer")
    ensure_file(model_path, "Model")
    ensure_file(cases_path, "Cases file")

    cases = load_cases(cases_path)
    if args.case:
        selected = set(args.case)
        cases = [case for case in cases if case.get("slug") in selected]
        if not cases:
            print("No benchmark case matched the requested slug filter.", file=sys.stderr)
            return 1

    results = []
    for case in cases:
        print(f"[hybrid-benchmark] {case['slug']} -> prompt -> run-turn -> render")
        result = render_case(
            case,
            renderer_path,
            model_path,
            output_root,
            args.width,
            args.height,
            args.samples,
            args.bounces,
            args.direct_samples,
            args.render_seed,
            args.exposure,
            args.n_predict,
            args.temperature,
            args.use_json_grammar,
        )
        results.append(result)
        print(
            f"[hybrid-benchmark] {case['slug']} status={result['status']} "
            f"scene={result['scene_source'] or '-'} infer={result['inference_time_ms'] or 0:.2f}ms "
            f"render={result['render_time_ms'] or 0:.2f}ms"
        )

    summary = {
        "generated_at": dt.datetime.now().isoformat(timespec="seconds"),
        "renderer": str(renderer_path),
        "model": str(model_path),
        "cases_file": str(cases_path),
        "width": args.width,
        "height": args.height,
        "samples": args.samples,
        "bounces": args.bounces,
        "direct_samples": args.direct_samples,
        "n_predict": args.n_predict,
        "temperature": args.temperature,
        "use_json_grammar": args.use_json_grammar,
        "valid_scenes": sum(1 for item in results if item["valid_scene"]),
        "total_cases": len(results),
        "fallback_cases": sum(1 for item in results if item["status"] == "valid_with_fallback"),
        "results": results,
    }
    write_json(output_root / "results.json", summary)
    write_markdown(markdown_path, cases, results, args)

    print(f"[hybrid-benchmark] valid scenes: {summary['valid_scenes']}/{summary['total_cases']}")
    print(f"[hybrid-benchmark] fallback scenes: {summary['fallback_cases']}")
    print(f"[hybrid-benchmark] markdown: {markdown_path}")
    print(f"[hybrid-benchmark] summary: {output_root / 'results.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
