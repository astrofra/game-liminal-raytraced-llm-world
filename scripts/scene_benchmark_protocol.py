"""Provenance and fail-closed extraction for the local llama-cli benchmark."""

import hashlib
import re


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(16 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def extract_final_response(transcript, prompt, stdout):
    # --file and the interactive loop both trim trailing newlines in some builds.
    prompt = prompt.replace("\r\n", "\n")
    prefix = "User:\n" + prompt.rstrip("\n")
    transcript = transcript.replace("\r\n", "\n")
    if not transcript.startswith(prefix):
        raise ValueError("Missing or incompatible llama-cli assistant transcript boundary")
    boundary = re.match(r"\n{2,}Assistant:\n", transcript[len(prefix):])
    if not boundary:
        raise ValueError("Missing or incompatible llama-cli assistant transcript boundary")
    response = transcript[len(prefix) + boundary.end():].strip()
    start, end = "[Start thinking]", "[End thinking]"
    # The output file closes thinking even when interrupted; the live output does not.
    if stdout.count(start) != stdout.count(end):
        raise ValueError("Incomplete reasoning channel in llama-cli stream")
    reasoning = ""
    if response.startswith(start):
        closing = response.find(end, len(start))
        if closing < 0:
            raise ValueError("Unclosed reasoning channel")
        reasoning = response[len(start):closing].strip()
        response = response[closing + len(end):].strip()
    forbidden = (start, end, "<|channel>", "<channel|>", "<think>", "</think>", "<|think|>", "[THINK]", "[/THINK]")
    if any(marker in response for marker in forbidden):
        raise ValueError("Unexpected reasoning marker in final response")
    if not response:
        raise ValueError("No final response")
    return response, len(reasoning)


def parse_generation_metrics(stderr, total_budget):
    metrics = {"stop_reason": "unknown", "generated_tokens": None, "prompt_tokens": None,
               "prompt_eval_ms": None, "decode_ms": None, "offloaded_layers": None}
    patterns = (
        ("prompt eval time", "prompt_eval_ms", "prompt_tokens"),
        ("eval time", "decode_ms", "generated_tokens"),
    )
    for label, duration, tokens in patterns:
        matches = re.findall(r"(?m)(?<!prompt )\b" + label + r"\s*=\s*([\d.]+) ms\s*/\s*(\d+) tokens", stderr)
        if matches:
            metrics[duration], metrics[tokens] = float(matches[-1][0]), int(matches[-1][1])
    offload = re.search(r"offloaded (\d+/\d+) layers to GPU", stderr)
    if offload:
        metrics["offloaded_layers"] = offload.group(1)
    stop_lines = "\n".join(line for line in stderr.splitlines()
                           if re.search(r"\bslot\s+process_toke\w*:\s+id\s+\d+\s*\|\s*task\s+\d+\s*\|", line))
    if "stopped by limit" in stop_lines or (
        metrics["generated_tokens"] is not None and metrics["generated_tokens"] >= total_budget
    ):
        metrics["stop_reason"] = "token_limit"
    elif "running out of context capacity" in stop_lines:
        metrics["stop_reason"] = "context_limit"
    elif "stopped by EOS" in stop_lines:
        metrics["stop_reason"] = "eos"
    memory_lines = [line.split("common_memory_breakdown_print:", 1)[-1].strip()
                    for line in stderr.splitlines() if "common_memory_breakdown_print:" in line]
    headers = [index for index, line in enumerate(memory_lines) if "memory breakdown" in line]
    if headers:
        memory_lines = memory_lines[headers[-1]:]
    metrics["memory_breakdown_at_exit"] = memory_lines
    return metrics
