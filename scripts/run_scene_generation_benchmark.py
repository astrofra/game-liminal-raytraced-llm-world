#!/usr/bin/env python3

import argparse
import datetime as dt
import json
import os
import pathlib
import re
import subprocess
import sys
import time

from scene_benchmark_protocol import extract_final_response, parse_generation_metrics, sha256_file
from scene_benchmark_quality import audit_camera
from model_profiles import DEFAULT_PROFILE, load_profiles, resolve_model_path


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_RENDERER = REPO_ROOT / "build" / "Release" / "liminal_cornell_renderer.exe"
DEFAULT_LLAMA_CLI = REPO_ROOT / "build" / "bin" / "Release" / "llama-cli.exe"
DEFAULT_MODEL = resolve_model_path()
MODEL_PROFILES = {name: resolve_model_path(name) for name in load_profiles()["profiles"]}
DEFAULT_CASES = REPO_ROOT / "scripts" / "scene_generation_benchmark_cases.json"

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
CAMERA_GUIDANCE = (
    "\nCamera validity check for the final scene:\n"
    "- eye and target must be distinct; the viewing direction must not be parallel to up.\n"
    "- With up(0,1,0), eye and target MUST differ in x or z. Never use the same x AND z for both.\n"
    "- A safe example is camera eye(0,1.75,-7.9) target(0,1.18,8.8) up(0,1,0) fov(46).\n"
    "- Frame the named anchors in front of the camera. Keep the eye outside walls and solid prefabs.\n"
)


def apply_profile_defaults(options):
    manifest = load_profiles()
    name = options.profile
    profile = manifest["profiles"].get(name, {})
    defaults = {"temperature": profile.get("temperature", DEFAULT_TEMPERATURE),
                "top_k": profile.get("top_k", 40), "top_p": profile.get("top_p", .95),
                "reasoning": "on" if profile.get("reasoning") else "off"}
    for key, value in defaults.items():
        if getattr(options, key) is None:
            setattr(options, key, value)
    if options.reasoning_budget is None:
        options.reasoning_budget = 512 if options.reasoning == "on" else 0


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
    return os.path.relpath(target_path, markdown_path.parent).replace("\\", "/")


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


def run_command(command, cwd, timeout=300):
    start = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=cwd,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
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
            r"(?m)^(room|camera|spotlight|sky|plane|box|prefab_gate|prefab_rack|prefab_crate|prefab_cooling_unit|prefab_ai_server|prefab_survey_beacon|prefab_crystal_scanner|prefab_crystal_cluster|prefab_extraction_rig|prefab_prospect_shelter|prefab_quarry_pylon|prefab_atmospheric_processor)\b",
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
        r"(?m)^(room|camera|spotlight|sky|plane|box|prefab_gate|prefab_rack|prefab_crate|prefab_cooling_unit|prefab_ai_server|prefab_survey_beacon|prefab_crystal_scanner|prefab_crystal_cluster|prefab_extraction_rig|prefab_prospect_shelter|prefab_quarry_pylon|prefab_atmospheric_processor)\b",
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
    options,
    transcript_path,
):
    system_prompt = SYSTEM_PROMPT
    if options.reasoning == "on":
        system_prompt = system_prompt.replace("Return only", "Your final answer must contain only")
        profile = load_profiles()["profiles"].get(getattr(options, "profile", ""), {})
        if profile.get("reasoning"):
            system_prompt += ("\nReason privately inside [THINK] and [/THINK]. All output-format "
                              "requirements apply only to the final answer after [/THINK].")
    command = [
        str(llama_cli_path),
        "--model",
        str(model_path),
        "--n-gpu-layers",
        options.gpu_layers,
        "--ctx-size",
        str(ctx_size),
        "--flash-attn",
        "on",
        "--single-turn",
        "--jinja",
        "--reasoning", options.reasoning,
        "--reasoning-format", "deepseek",
        "--reasoning-budget", str(options.reasoning_budget),
        "--batch-size", str(options.batch_size),
        "--ubatch-size", str(options.ubatch_size),
        "--top-k", str(options.top_k),
        "--top-p", str(options.top_p),
        "--output-file", str(transcript_path),
        "--no-mmproj",
        "--system-prompt",
        system_prompt,
        "--file",
        str(prompt_path),
        "--n-predict",
        str(n_predict + options.reasoning_budget + (64 if options.reasoning == "on" else 0)),
        "--temperature",
        str(temperature),
        "--seed",
        str(seed),
        "--no-display-prompt",
        "--no-show-timings",
        "--simple-io",
        "--log-verbosity", "5",
        "--log-colors", "off",
        "--color",
        "off",
    ]
    if getattr(options, "cli_supports_conversation", True):
        command.append("--conversation")
    transcript_path.parent.mkdir(parents=True, exist_ok=True)
    return run_command(command, REPO_ROOT, options.generation_timeout)


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
    options,
):
    slug = case["slug"]
    state_path = output_root / "states" / f"{slug}.session.json"
    prompt_path = output_root / "prompts" / f"{slug}.prompt.txt"
    raw_response_path = output_root / "responses" / f"{slug}.raw.txt"
    llama_stderr_path = output_root / "responses" / f"{slug}.llama.stderr.txt"
    transcript_path = output_root / "responses" / f"{slug}.transcript.txt"
    final_path = output_root / "responses" / f"{slug}.final.txt"
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
        "syntax_valid_scene": False,
        "camera_errors": [],
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
        "stop_reason": "not_started",
        "final_response_path": str(final_path),
        "transcript_path": str(transcript_path),
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
    if options.camera_guidance:
        write_text(output_root / "prompts" / f"{slug}.engine.prompt.txt", prompt_text)
        prompt_text += CAMERA_GUIDANCE
    write_text(prompt_path, prompt_text)

    generation_start = time.perf_counter()
    try:
        generation_completed, generation_duration_ms = generate_scene_with_llama(
            llama_cli_path, model_path, prompt_path, ctx_size, n_predict,
            temperature, seed, options, transcript_path,
        )
    except subprocess.TimeoutExpired as exc:
        result.update(status="timeout", stop_reason="timeout", error="Generation timeout",
                      generation_time_ms=(time.perf_counter() - generation_start) * 1000)
        for path, content in ((raw_response_path, exc.stdout), (llama_stderr_path, exc.stderr)):
            write_text(path, content.decode("utf-8", errors="replace") if isinstance(content, bytes) else (content or ""))
        return result
    result["generation_time_ms"] = generation_duration_ms
    result["generation_command"] = generation_completed.args
    result["prompt_sha256"] = sha256_file(prompt_path)
    write_text(raw_response_path, generation_completed.stdout)
    write_text(llama_stderr_path, generation_completed.stderr)
    total_budget = n_predict + options.reasoning_budget + (64 if options.reasoning == "on" else 0)
    result.update(parse_generation_metrics(generation_completed.stderr, total_budget))
    if generation_completed.returncode != 0:
        result["status"] = "llm_failed"
        result["error"] = (generation_completed.stderr or generation_completed.stdout).strip()
        return result

    if result["stop_reason"] != "eos":
        result["status"] = "incomplete_generation"
        result["error"] = "No proven EOS; refusing to audit potentially truncated output"
        return result
    try:
        final_text, reasoning_chars = extract_final_response(
            transcript_path.read_text(encoding="utf-8"), prompt_text, generation_completed.stdout)
    except (ValueError, OSError) as exc:
        result.update(status="invalid_protocol", error=str(exc))
        return result
    write_text(final_path, final_text + "\n")
    result["reasoning_characters"] = reasoning_chars
    normalized_scene_text = extract_scene_candidate_text(final_text)
    if normalized_scene_text:
        normalized_scene_text += "\n"
    write_text(scene_path, normalized_scene_text)
    result["scene_characters"] = len(normalized_scene_text)
    result["scene_lines"] = len([line for line in normalized_scene_text.splitlines() if line.strip()])
    result["camera_errors"] = audit_camera(normalized_scene_text)

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
    result["syntax_valid_scene"] = True
    if result["camera_errors"]:
        result["status"] = "invalid_camera"
        result["error"] = "; ".join(result["camera_errors"])
        return result
    result["valid_scene"] = True
    result["status"] = "valid"
    return result


def write_markdown(markdown_path, output_root, cases, results, args):
    generated_at = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total_cases = len(results)
    valid_cases = sum(1 for item in results if item["valid_scene"])
    invalid_cases = total_cases - valid_cases
    syntax_valid_cases = sum(1 for item in results if item["syntax_valid_scene"])

    case_lookup = {case["slug"]: case for case in cases}

    lines = [
        "# Scene Generation Benchmark",
        "",
        f"Genere le {generated_at}.",
        "",
        f"Generation directe de `.scene` ; profil `{args.profile}`, modele `{args.model.name}`.",
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
        f"- reflexion : `{args.reasoning}`, budget `{args.reasoning_budget}` ; top_k `{args.top_k}`, top_p `{args.top_p}`",
        "- seul le canal final termine par EOS est audite ; les sorties tronquees sont refusees",
        "",
        f"Resultat global : `{valid_cases}/{total_cases}` scenes avec syntaxe et camera valides, `{invalid_cases}` invalides. Syntaxe seule : `{syntax_valid_cases}/{total_cases}`.",
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
                f"Final response : `{relative_to_markdown(markdown_path, pathlib.Path(item['final_response_path']))}`",
                f"Stop reason : `{item['stop_reason']}`",
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

        if pathlib.Path(item["image_path"]).is_file():
            image_rel = relative_to_markdown(markdown_path, pathlib.Path(item["image_path"]))
            lines.extend(
                [
                    f"![{item['slug']}]({image_rel})",
                    "",
                ]
            )
        if not item["valid_scene"]:
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
    parser.add_argument("--profile", choices=sorted(MODEL_PROFILES), default=DEFAULT_PROFILE)
    parser.add_argument("--model", type=pathlib.Path, help="Override the selected profile's GGUF path")
    parser.add_argument("--cases", type=pathlib.Path, default=DEFAULT_CASES)
    parser.add_argument("--output-root", type=pathlib.Path, help="New directory; existing non-empty directories are refused")
    parser.add_argument("--markdown", type=pathlib.Path, help="New report path (default: OUTPUT_ROOT/report.md)")
    parser.add_argument("--width", type=int, default=DEFAULT_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT)
    parser.add_argument("--samples", type=int, default=DEFAULT_SAMPLES)
    parser.add_argument("--bounces", type=int, default=DEFAULT_BOUNCES)
    parser.add_argument("--direct-samples", type=int, default=DEFAULT_DIRECT_SAMPLES)
    parser.add_argument("--render-seed", type=int, default=DEFAULT_RENDER_SEED)
    parser.add_argument("--exposure", type=float, default=DEFAULT_EXPOSURE)
    parser.add_argument("--ctx-size", type=int, default=DEFAULT_CTX)
    parser.add_argument("--n-predict", type=int, default=DEFAULT_PREDICT)
    parser.add_argument("--temperature", type=float, help="Override the profile's sampling temperature")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--reasoning", choices=("off", "on"), help="Override profile default")
    parser.add_argument("--reasoning-budget", type=int, help="Thinking tokens; default 512 when reasoning is on, otherwise 0")
    parser.add_argument("--top-k", type=int)
    parser.add_argument("--top-p", type=float)
    parser.add_argument("--batch-size", type=int, default=512)
    parser.add_argument("--ubatch-size", type=int, default=128)
    parser.add_argument("--gpu-layers", default="auto")
    parser.add_argument("--generation-timeout", type=float, default=300)
    parser.add_argument("--camera-guidance", action="store_true", help="Append explicit camera-basis rules to the engine prompt (separate experiment)")
    parser.add_argument("--case", action="append", default=[], help="Run only the specified case slug. May be repeated.")
    args = parser.parse_args()
    apply_profile_defaults(args)

    if args.reasoning_budget < 0 or (args.reasoning == "on") != (args.reasoning_budget > 0):
        parser.error("Use --reasoning off --reasoning-budget 0, or on with a positive budget")
    if min(args.n_predict, args.ctx_size, args.batch_size, args.ubatch_size, args.generation_timeout) <= 0:
        parser.error("Token, context, batch and timeout limits must be positive")
    if args.ubatch_size > args.batch_size or not 0 < args.top_p <= 1 or args.top_k < 0:
        parser.error("Invalid micro-batch or sampling limits")
    total_budget = args.n_predict + args.reasoning_budget + (64 if args.reasoning == "on" else 0)
    if total_budget >= args.ctx_size:
        parser.error("Context must exceed the total generation budget and leave room for the prompt")
    args.model = args.model or MODEL_PROFILES[args.profile]
    run_name = dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f") + f"-{args.profile}-{args.reasoning}"
    args.output_root = args.output_root or REPO_ROOT / "documentation/generated/scene_generation_benchmark_runs" / run_name
    args.markdown = args.markdown or args.output_root / "report.md"

    renderer_path = args.renderer.resolve()
    llama_cli_path = args.llama_cli.resolve()
    model_path = args.model.resolve()
    cases_path = args.cases.resolve()
    output_root = args.output_root.resolve()
    markdown_path = args.markdown.resolve()

    if output_root.exists() and (not output_root.is_dir() or any(output_root.iterdir())):
        parser.error("Output directory must be new or empty; previous results are preserved")
    if markdown_path.exists():
        parser.error("Markdown report already exists; choose a new path")

    ensure_file(renderer_path, "Renderer")
    ensure_file(llama_cli_path, "llama-cli")
    ensure_file(model_path, "Model")
    ensure_file(cases_path, "Cases file")

    cases = load_cases(cases_path)
    if args.case:
        selected = set(args.case)
        unknown = selected - {case.get("slug") for case in cases}
        if unknown:
            parser.error("Unknown case(s): " + ", ".join(sorted(unknown)))
        cases = [case for case in cases if case.get("slug") in selected]
        if not cases:
            print("No benchmark case matched the requested slug filter.", file=sys.stderr)
            return 1

    if not cases:
        parser.error("No benchmark cases")
    slugs = [case["slug"] for case in cases]
    if len(set(slugs)) != len(slugs) or any(not re.fullmatch(r"[A-Za-z0-9_-]+", slug) for slug in slugs):
        parser.error("Case slugs must be unique simple filenames")
    help_run, _ = run_command([str(llama_cli_path), "--help"], REPO_ROOT, 30)
    args.cli_supports_conversation = "--conversation" in help_run.stdout + help_run.stderr
    required = ("--jinja", "--reasoning", "--reasoning-budget", "--output-file", "--log-verbosity")
    if help_run.returncode or any(flag not in help_run.stdout + help_run.stderr for flag in required):
        parser.error("llama-cli does not expose the required protocol/telemetry options")
    version, _ = run_command([str(llama_cli_path), "--version"], REPO_ROOT, 30)
    print("[benchmark] hashing model and executables for provenance", flush=True)
    provenance = {
        "model_sha256": sha256_file(model_path), "model_size": model_path.stat().st_size,
        "llama_cli_sha256": sha256_file(llama_cli_path),
        "renderer_sha256": sha256_file(renderer_path),
        "llama_cli_version": (version.stdout + version.stderr).strip(),
        "vendor_source_commit": (REPO_ROOT / "vendor/llama.cpp/VENDORED_COMMIT.txt").read_text().strip(),
        "cases_sha256": sha256_file(cases_path),
        "benchmark_script_sha256": sha256_file(pathlib.Path(__file__)),
        "protocol_script_sha256": sha256_file(pathlib.Path(__file__).with_name("scene_benchmark_protocol.py")),
        "quality_script_sha256": sha256_file(pathlib.Path(__file__).with_name("scene_benchmark_quality.py")),
        "driver_gpu": None,
        "note": "Binary version/hash and vendored source revision recorded separately; no equivalence assumed.",
    }
    build_record = llama_cli_path.parent / "runtime-build.json"
    if build_record.is_file():
        record = json.loads(build_record.read_text(encoding="utf-8"))
        if record.get("artifacts", {}).get(llama_cli_path.name) != provenance["llama_cli_sha256"]:
            parser.error("llama-cli does not match its runtime-build.json provenance")
        provenance["runtime_build"] = record
    try:
        gpu, _ = run_command(["nvidia-smi", "--query-gpu=name,memory.total,memory.used,driver_version", "--format=csv"], REPO_ROOT, 10)
        provenance["driver_gpu"] = gpu.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        pass
    write_json(output_root / "provenance.json", provenance)
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
            args,
        )
        results.append(result)
        write_json(output_root / "partial_results.json", results)
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
        "profile": args.profile,
        "camera_guidance": args.camera_guidance,
        "reasoning": args.reasoning,
        "reasoning_budget": args.reasoning_budget,
        "total_generation_budget": total_budget,
        "top_k": args.top_k, "top_p": args.top_p,
        "batch_size": args.batch_size, "ubatch_size": args.ubatch_size,
        "gpu_layers": args.gpu_layers, "generation_timeout_seconds": args.generation_timeout,
        "render_seed": args.render_seed, "exposure": args.exposure,
        "provenance": provenance,
        "valid_scenes": sum(1 for item in results if item["valid_scene"]),
        "syntax_valid_scenes": sum(1 for item in results if item["syntax_valid_scene"]),
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
    return 0 if valid_scenes == total_cases else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(f"[benchmark] ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
