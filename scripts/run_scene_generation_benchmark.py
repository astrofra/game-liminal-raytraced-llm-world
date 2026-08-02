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
DEFAULT_LLAMA_CLI = REPO_ROOT / "vendor" / "llama.cpp" / "build-cuda" / "bin" / "Release" / "llama-cli.exe"
DEFAULT_MODEL = REPO_ROOT / "models" / "ministral-3-8b" / "Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
DEFAULT_CASES = REPO_ROOT / "scripts" / "scene_generation_benchmark_cases.json"
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "documentation" / "generated" / "scene_generation_benchmark"
DEFAULT_MARKDOWN = REPO_ROOT / "documentation" / "SCENE_GENERATION_BENCHMARK.md"

DEFAULT_WIDTH = 800
DEFAULT_HEIGHT = 400
DEFAULT_SAMPLES = 8
DEFAULT_BOUNCES = 3
DEFAULT_DIRECT_SAMPLES = 1
DEFAULT_RENDER_SEED = 77
DEFAULT_EXPOSURE = 1.0

DEFAULT_CTX = 4096
DEFAULT_PREDICT = 1024
DEFAULT_TEMPERATURE = 0.0
DEFAULT_SEED = 42
SYSTEM_PROMPT = (
    "You are a deterministic scene compiler. "
    "Return only a valid .scene program. "
    "Do not use markdown fences. Stop after the last scene directive."
)


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


def build_session_state(case):
    location_id = case.get("location_id", "unknown")
    alert_level = int(case.get("alert_level", 1))
    return {
        "hard_state": {
            "turn_number": 1,
            "move_count": 0,
            "score": 0,
            "current_location_id": location_id,
            "alert_level": alert_level,
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
        "spatial_state": {
            "location_id": location_id,
            "room_title": case.get("room_title", ""),
            "room_summary": case.get("room_summary", ""),
            "location_archetype": case.get("location_archetype", ""),
            "canonical_fixture": case.get("canonical_fixture", ""),
            "time_of_day": case.get("time_of_day", "unknown"),
            "visibility_level": case.get("visibility_level", "unknown"),
            "desert_state": case.get("desert_state", "unknown"),
            "interior_density": case.get("interior_density", "unknown"),
            "alert_level": alert_level,
            "anchors": case.get("anchors", []),
            "visible_objects": case.get("visible_objects", []),
            "blocked_exits": case.get("blocked_exits", []),
            "spatial_anomalies": case.get("spatial_anomalies", []),
        },
        "history": [],
        "current_place_id": f"benchmark_{case['slug']}",
        "next_generated_room_index": 1,
        "generated_rooms": [],
        "room_links": [],
    }


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


def extract_scene_candidate_text(text):
    candidate = text.replace("\r\n", "\n").replace("\r", "\n")

    fence_index = candidate.find("```")
    if fence_index != -1:
        first_newline = candidate.find("\n", fence_index)
        if first_newline != -1:
            candidate = candidate[first_newline + 1 :]
            closing_fence = candidate.rfind("```")
            if closing_fence != -1:
                candidate = candidate[:closing_fence]
    else:
        directive_match = re.search(
            r"(?m)^(room|camera|spotlight|sky|plane|box|prefab_gate|prefab_rack|prefab_crate|prefab_cooling_unit|prefab_ai_server)\b",
            candidate,
        )
        if directive_match:
            candidate = candidate[directive_match.start() :]

    cleaned_lines = []
    for raw_line in candidate.splitlines():
        stripped = raw_line.rstrip()
        if stripped == "Exiting...":
            break
        if stripped.startswith("[ Prompt:"):
            continue
        cleaned_lines.append(stripped)

    cleaned = "\n".join(cleaned_lines).strip()
    directive_match = re.search(
        r"(?m)^(room|camera|spotlight|sky|plane|box|prefab_gate|prefab_rack|prefab_crate|prefab_cooling_unit|prefab_ai_server)\b",
        cleaned,
    )
    if directive_match:
        cleaned = cleaned[directive_match.start() :]

    return cleaned.strip()


def dump_scene_audit_prompt(renderer_path, session_state_path):
    command = [
        str(renderer_path),
        "--dump-scene-audit-prompt",
        "--load-state",
        str(session_state_path),
    ]
    return run_command(command, REPO_ROOT)


def generate_scene_with_llama(
    llama_cli_path,
    model_path,
    prompt_path,
    ctx_size,
    n_predict,
    temperature,
    seed,
):
    command = [
        str(llama_cli_path),
        "--model",
        str(model_path),
        "--n-gpu-layers",
        "auto",
        "--ctx-size",
        str(ctx_size),
        "--flash-attn",
        "on",
        "--conversation",
        "--single-turn",
        "--no-mmproj",
        "--system-prompt",
        SYSTEM_PROMPT,
        "--file",
        str(prompt_path),
        "--n-predict",
        str(n_predict),
        "--temperature",
        str(temperature),
        "--seed",
        str(seed),
        "--no-display-prompt",
        "--no-show-timings",
        "--simple-io",
        "--log-disable",
        "--color",
        "off",
    ]
    return run_command(command, REPO_ROOT)


def audit_and_render_scene(
    renderer_path,
    scene_path,
    image_path,
    width,
    height,
    samples,
    bounces,
    direct_samples,
    seed,
    exposure,
):
    image_path.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(renderer_path),
        "--audit-scene-text",
        str(scene_path),
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
        str(seed),
        "--exposure",
        str(exposure),
    ]
    return run_command(command, REPO_ROOT)


def parse_render_stats(stdout_text):
    stats = {}

    loaded_match = re.search(
        r"Loaded (\d+) triangles, (\d+) materials, (\d+) emissive triangles",
        stdout_text,
    )
    if loaded_match:
        stats["triangles"] = int(loaded_match.group(1))
        stats["materials"] = int(loaded_match.group(2))
        stats["emissive_triangles"] = int(loaded_match.group(3))

    render_match = re.search(r"Render time:\s*([0-9.]+)\s*ms", stdout_text)
    if render_match:
        stats["render_time_ms"] = float(render_match.group(1))

    prep_match = re.search(r"(Load time|Preparation time):\s*([0-9.]+)\s*ms", stdout_text)
    if prep_match:
        stats["load_time_ms"] = float(prep_match.group(2))

    return stats


def slug_title(case):
    return case.get("room_title", case["slug"])


def render_case(
    case,
    renderer_path,
    llama_cli_path,
    model_path,
    output_root,
    width,
    height,
    samples,
    bounces,
    direct_samples,
    render_seed,
    exposure,
    ctx_size,
    n_predict,
    temperature,
    seed,
):
    slug = case["slug"]
    state_path = output_root / "states" / f"{slug}.session.json"
    prompt_path = output_root / "prompts" / f"{slug}.prompt.txt"
    raw_response_path = output_root / "responses" / f"{slug}.raw.txt"
    llama_stderr_path = output_root / "responses" / f"{slug}.llama.stderr.txt"
    scene_path = output_root / "scenes" / f"{slug}.scene"
    image_path = output_root / "images" / f"{slug}.png"
    audit_stdout_path = output_root / "logs" / f"{slug}.audit.stdout.txt"
    audit_stderr_path = output_root / "logs" / f"{slug}.audit.stderr.txt"

    result = {
        "slug": slug,
        "title": slug_title(case),
        "room_summary": case.get("room_summary", ""),
        "status": "unknown",
        "valid_scene": False,
        "prompt_time_ms": 0.0,
        "generation_time_ms": 0.0,
        "audit_time_ms": 0.0,
        "prompt_path": str(prompt_path),
        "state_path": str(state_path),
        "raw_response_path": str(raw_response_path),
        "scene_path": str(scene_path),
        "image_path": str(image_path),
        "llama_stderr_path": str(llama_stderr_path),
        "audit_stdout_path": str(audit_stdout_path),
        "audit_stderr_path": str(audit_stderr_path),
        "scene_characters": 0,
        "scene_lines": 0,
        "triangles": None,
        "materials": None,
        "render_time_ms": None,
        "error": "",
    }

    session_state = build_session_state(case)
    write_json(state_path, session_state)

    prompt_completed, prompt_duration_ms = dump_scene_audit_prompt(renderer_path, state_path)
    result["prompt_time_ms"] = prompt_duration_ms
    if prompt_completed.returncode != 0:
        result["status"] = "prompt_failed"
        result["error"] = (prompt_completed.stderr or prompt_completed.stdout).strip()
        write_text(prompt_path, prompt_completed.stdout)
        write_text(audit_stderr_path, prompt_completed.stderr)
        return result

    prompt_text = prompt_completed.stdout
    write_text(prompt_path, prompt_text)

    generation_completed, generation_duration_ms = generate_scene_with_llama(
        llama_cli_path,
        model_path,
        prompt_path,
        ctx_size,
        n_predict,
        temperature,
        seed,
    )
    result["generation_time_ms"] = generation_duration_ms
    write_text(raw_response_path, generation_completed.stdout)
    write_text(llama_stderr_path, generation_completed.stderr)
    if generation_completed.returncode != 0:
        result["status"] = "llm_failed"
        result["error"] = (generation_completed.stderr or generation_completed.stdout).strip()
        return result

    normalized_scene_text = extract_scene_candidate_text(generation_completed.stdout)
    if normalized_scene_text:
        normalized_scene_text += "\n"
    write_text(scene_path, normalized_scene_text)
    result["scene_characters"] = len(normalized_scene_text)
    result["scene_lines"] = len([line for line in normalized_scene_text.splitlines() if line.strip()])

    audit_completed, audit_duration_ms = audit_and_render_scene(
        renderer_path,
        scene_path,
        image_path,
        width,
        height,
        samples,
        bounces,
        direct_samples,
        render_seed,
        exposure,
    )
    result["audit_time_ms"] = audit_duration_ms
    write_text(audit_stdout_path, audit_completed.stdout)
    write_text(audit_stderr_path, audit_completed.stderr)

    if audit_completed.returncode != 0:
        result["status"] = "invalid_scene"
        result["error"] = (audit_completed.stderr or audit_completed.stdout).strip()
        return result

    stats = parse_render_stats(audit_completed.stdout)
    result["triangles"] = stats.get("triangles")
    result["materials"] = stats.get("materials")
    result["render_time_ms"] = stats.get("render_time_ms")
    result["valid_scene"] = True
    result["status"] = "valid"
    return result


def write_markdown(markdown_path, output_root, cases, results, args):
    generated_at = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total_cases = len(results)
    valid_cases = sum(1 for item in results if item["valid_scene"])
    invalid_cases = total_cases - valid_cases

    case_lookup = {case["slug"]: case for case in cases}

    lines = [
        "# Scene Generation Benchmark",
        "",
        f"Genere le {generated_at}.",
        "",
        "Ce document audite la generation directe de `.scene` par `Ministral` a partir de briefs spatiaux fixes.",
        "",
        "Pipeline :",
        "- le moteur fabrique le prompt exact via `--dump-scene-audit-prompt`",
        "- `llama-cli` genere une reponse brute",
        "- la reponse est normalisee en `.scene`",
        "- le renderer audite le fichier et produit une image si la scene est valide",
        "",
        "Parametres :",
        f"- rendu : `{args.width}x{args.height}`, `{args.samples}` spp, `{args.bounces}` bounces, `{args.direct_samples}` direct samples",
        f"- LLM : `temperature={args.temperature}`, `n_predict={args.n_predict}`, `ctx={args.ctx_size}`, `seed={args.seed}`",
        "",
        f"Resultat global : `{valid_cases}/{total_cases}` scenes valides, `{invalid_cases}` invalides.",
        "",
        "| Case | Status | Gen ms | Audit ms | Triangles | Materials |",
        "| --- | --- | ---: | ---: | ---: | ---: |",
    ]

    for item in results:
        lines.append(
            f"| `{item['slug']}` | `{item['status']}` | {item['generation_time_ms']:.2f} | "
            f"{item['audit_time_ms']:.2f} | {item['triangles'] or 0} | {item['materials'] or 0} |"
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
                f"Status : `{item['status']}`",
                f"Slug : `{item['slug']}`",
                f"Generation : `{item['generation_time_ms']:.2f} ms`",
                f"Audit + render : `{item['audit_time_ms']:.2f} ms`",
                f"Scene lines : `{item['scene_lines']}`",
                f"Scene chars : `{item['scene_characters']}`",
                f"Triangles : `{item['triangles'] or 0}`",
                f"Materials : `{item['materials'] or 0}`",
                "",
                f"State : `{relative_to_markdown(markdown_path, pathlib.Path(item['state_path']))}`",
                f"Prompt : `{relative_to_markdown(markdown_path, pathlib.Path(item['prompt_path']))}`",
                f"Raw response : `{relative_to_markdown(markdown_path, pathlib.Path(item['raw_response_path']))}`",
                f"Scene : `{relative_to_markdown(markdown_path, pathlib.Path(item['scene_path']))}`",
                f"Audit stdout : `{relative_to_markdown(markdown_path, pathlib.Path(item['audit_stdout_path']))}`",
                f"Audit stderr : `{relative_to_markdown(markdown_path, pathlib.Path(item['audit_stderr_path']))}`",
                "",
                "Spatial brief :",
                f"- location_id: `{case.get('location_id', 'unknown')}`",
                f"- location_archetype: `{case.get('location_archetype', '')}`",
                f"- time_of_day: `{case.get('time_of_day', 'unknown')}`",
                f"- visibility_level: `{case.get('visibility_level', 'unknown')}`",
                f"- desert_state: `{case.get('desert_state', 'unknown')}`",
                f"- interior_density: `{case.get('interior_density', 'unknown')}`",
                f"- alert_level: `{case.get('alert_level', 1)}`",
                f"- anchors: `{', '.join(case.get('anchors', [])) or '(none)'}`",
                f"- visible_objects: `{', '.join(case.get('visible_objects', [])) or '(none)'}`",
                f"- blocked_exits: `{', '.join(case.get('blocked_exits', [])) or '(none)'}`",
                f"- spatial_anomalies: `{', '.join(case.get('spatial_anomalies', [])) or '(none)'}`",
                "",
            ]
        )

        if item["valid_scene"] and pathlib.Path(item["image_path"]).is_file():
            image_rel = relative_to_markdown(markdown_path, pathlib.Path(item["image_path"]))
            lines.extend(
                [
                    f"![{item['slug']}]({image_rel})",
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
    parser = argparse.ArgumentParser(description="Run a reproducible local benchmark for direct .scene generation.")
    parser.add_argument("--renderer", type=pathlib.Path, default=DEFAULT_RENDERER)
    parser.add_argument("--llama-cli", type=pathlib.Path, default=DEFAULT_LLAMA_CLI)
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
    parser.add_argument("--ctx-size", type=int, default=DEFAULT_CTX)
    parser.add_argument("--n-predict", type=int, default=DEFAULT_PREDICT)
    parser.add_argument("--temperature", type=float, default=DEFAULT_TEMPERATURE)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--case", action="append", default=[], help="Run only the specified case slug. May be repeated.")
    args = parser.parse_args()

    renderer_path = args.renderer.resolve()
    llama_cli_path = args.llama_cli.resolve()
    model_path = args.model.resolve()
    cases_path = args.cases.resolve()
    output_root = args.output_root.resolve()
    markdown_path = args.markdown.resolve()

    ensure_file(renderer_path, "Renderer")
    ensure_file(llama_cli_path, "llama-cli")
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
        print(f"[benchmark] {case['slug']} -> prompt -> llm -> audit")
        result = render_case(
            case,
            renderer_path,
            llama_cli_path,
            model_path,
            output_root,
            args.width,
            args.height,
            args.samples,
            args.bounces,
            args.direct_samples,
            args.render_seed,
            args.exposure,
            args.ctx_size,
            args.n_predict,
            args.temperature,
            args.seed,
        )
        results.append(result)
        print(
            f"[benchmark] {case['slug']} status={result['status']} "
            f"gen={result['generation_time_ms']:.2f}ms audit={result['audit_time_ms']:.2f}ms"
        )

    summary = {
        "generated_at": dt.datetime.now().isoformat(timespec="seconds"),
        "renderer": str(renderer_path),
        "llama_cli": str(llama_cli_path),
        "model": str(model_path),
        "cases_file": str(cases_path),
        "width": args.width,
        "height": args.height,
        "samples": args.samples,
        "bounces": args.bounces,
        "direct_samples": args.direct_samples,
        "ctx_size": args.ctx_size,
        "n_predict": args.n_predict,
        "temperature": args.temperature,
        "seed": args.seed,
        "valid_scenes": sum(1 for item in results if item["valid_scene"]),
        "total_cases": len(results),
        "results": results,
    }
    write_json(output_root / "results.json", summary)
    write_markdown(markdown_path, output_root, cases, results, args)

    valid_scenes = summary["valid_scenes"]
    total_cases = summary["total_cases"]
    print(f"[benchmark] valid scenes: {valid_scenes}/{total_cases}")
    print(f"[benchmark] markdown: {markdown_path}")
    print(f"[benchmark] summary: {output_root / 'results.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
