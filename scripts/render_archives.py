"""Render every logged game and place, with resumable PNG output and an offline gallery."""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import zlib

from archive_gallery import write_gallery

ROOT = Path(__file__).resolve().parents[1]
PRIVATE_RUNS = Path("C:/works/duo-runs/game-liminal-raytraced-llm-world")


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def valid_png(path: Path, size: int) -> bool:
    """Check all chunk CRCs and IEND, not just the signature of a partial render."""
    try:
        data = path.read_bytes()
        if data[:8] != b"\x89PNG\r\n\x1a\n":
            return False
        offset, dimensions, image_data = 8, None, False
        while offset + 12 <= len(data):
            length = struct.unpack_from(">I", data, offset)[0]
            end = offset + length + 12
            if end > len(data):
                return False
            kind = data[offset + 4:offset + 8]
            payload = data[offset + 8:offset + 8 + length]
            crc = struct.unpack_from(">I", data, offset + 8 + length)[0]
            if zlib.crc32(kind + payload) & 0xffffffff != crc:
                return False
            if kind == b"IHDR":
                if offset != 8 or length != 13:
                    return False
                dimensions = struct.unpack_from(">II", payload)
            elif kind == b"IDAT":
                image_data = True
            elif kind == b"IEND":
                return length == 0 and end == len(data) and dimensions == (size, size) and image_data
            offset = end
    except (OSError, ValueError, struct.error):
        pass
    return False


def source_fingerprint(archive: Path) -> str:
    result = hashlib.sha256()
    paths = [archive / "manifest.json", archive / "state.json"]
    # Include the complete accepted-place set: the C++ reader validates all of
    # it even when rendering one place. Rejected attempts never enter this set.
    paths += sorted(p for p in (archive / "places").glob("place-*")
                    if p.suffix in (".json", ".scene") and "." not in p.stem)
    for path in paths:
        result.update(path.name.encode("utf-8"))
        result.update(b"\0")
        result.update(path.read_bytes())
        result.update(b"\0")
    return result.hexdigest()


def collect_jobs(research: Path, mode: str) -> list[dict]:
    jobs = []
    for archive in sorted(research.glob("run-*"), reverse=True):
        if not archive.is_dir():
            continue
        base = {"archive": archive.name, "archive_path": archive}
        if mode in ("all", "atlases"):
            jobs.append({**base, "kind": "atlas", "title": "Vue d'ensemble", "place": None,
                         "image": f"{archive.name}/atlas.png"})
        if mode in ("all", "places"):
            for path in sorted((archive / "places").glob("place-*.json")):
                if "." in path.stem:
                    continue
                job = {**base, "kind": "place", "title": path.stem, "place": None,
                       "image": f"{archive.name}/{path.stem}.png"}
                try:
                    info = read_json(path)
                    job["place"] = info["place_id"]
                    job["title"] = info["title"]
                    if not isinstance(job["place"], str) or not isinstance(job["title"], str):
                        raise ValueError("Place ID/title must be strings")
                except (OSError, ValueError, KeyError, TypeError) as error:
                    job["discovery_error"] = str(error)
                jobs.append(job)
    return jobs


def fingerprint(job: dict, source: str, renderer_hash: str, size: int, samples: int) -> str:
    value = {"source": source, "renderer": renderer_hash, "size": size, "samples": samples,
             "place": job["place"], "kind": job["kind"], "archive": job["archive"], "version": 1}
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def cache_valid(record: dict, expected: str, output: Path, image: str, size: int) -> bool:
    path = output / image
    try:
        # An interrupted catalog may mark a previously verified entry pending.
        # The hashes and request identity, not its display status, prove reuse.
        return (bool(expected) and record.get("fingerprint") == expected and valid_png(path, size) and
                record.get("png_sha256") == digest(path) and
                record.get("metadata_sha256") == digest(path.with_suffix(".json")))
    except OSError:
        return False


def publish_catalog(output: Path, records: list[dict], size: int, samples: int):
    catalog = {"schema": 1, "size": size, "samples": samples,
               "counts": dict(Counter(r["status"] for r in records)), "images": records}
    temporary = output / "catalog.json.tmp"
    text = json.dumps(catalog, ensure_ascii=False, indent=2) + "\n"
    def retry_locked(action):
        for attempt in range(8):
            try:
                action()
                return
            except PermissionError:
                if attempt == 7:
                    raise
                time.sleep(min(.05 * 2**attempt, .4))
    retry_locked(lambda: temporary.write_text(text, encoding="utf-8"))
    retry_locked(lambda: temporary.replace(output / "catalog.json"))
    retry_locked(lambda: write_gallery(output, records))


def render_job(job: dict, renderer: Path, output: Path, logs: Path,
               size: int, samples: int, timeout: float, threads: int, annotate=False) -> dict:
    record = {k: job[k] for k in ("archive", "title", "kind", "image")}
    record["status"] = "failed"
    destination = output / job["image"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    log_path = logs / job["image"]
    log_path = log_path.with_suffix(".txt")
    log_path.parent.mkdir(parents=True, exist_ok=True)
    # TemporaryDirectory only cleans its own newly-created directory. An old
    # successful image survives a failed rerender or interruption.
    with tempfile.TemporaryDirectory(prefix=".render-", dir=destination.parent) as stage:
        temporary = Path(stage) / "image.png"
        command = [str(renderer), "--archive", str(job["archive_path"]), "--output", str(temporary),
                   "--size", str(size), "--samples", str(samples)]
        if job["kind"] == "place":
            command += ["--place", job["place"]]
        env = os.environ.copy()
        env["OMP_NUM_THREADS"] = str(threads)
        started = time.monotonic()
        try:
            with log_path.open("w", encoding="utf-8") as log:
                if annotate:
                    # C++ checks legacy camera, geometry, graph and quality before
                    # reusing pixels. A refusal falls back to a complete render.
                    result = subprocess.run(command + ["--annotate", str(destination)],
                                            stdout=log, stderr=subprocess.STDOUT,
                                            env=env, timeout=timeout, check=False)
                    if result.returncode == 0:
                        record["method"] = "titles_only"
                    else:
                        log.write("\nLegacy annotation unavailable; rendering the scene.\n")
                        log.flush()
                if not annotate or result.returncode:
                    result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                            env=env, timeout=timeout, check=False)
                    record["method"] = "raytrace"
            if result.returncode:
                record["failure"] = "renderer_error"
            elif not valid_png(temporary, size):
                record["failure"] = "invalid_png"
            else:
                metadata = read_json(temporary.with_suffix(".json"))
                if (metadata.get("size") != size or metadata.get("samples") != samples or
                        metadata.get("archive") != job["archive"]):
                    raise ValueError("Renderer provenance does not match the request")
                record["png_sha256"] = digest(temporary)
                record["metadata_sha256"] = digest(temporary.with_suffix(".json"))
                temporary.replace(destination)
                temporary.with_suffix(".json").replace(destination.with_suffix(".json"))
                record["status"] = "rendered"
        except subprocess.TimeoutExpired:
            record["failure"] = "timeout"
        except (OSError, ValueError) as error:
            record["failure"] = "io_or_metadata_error"
            with log_path.open("a", encoding="utf-8") as log:
                log.write("\n" + str(error) + "\n")
        record["seconds"] = round(time.monotonic() - started, 3)
    return record


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    default_research = (Path(os.environ["LOCALAPPDATA"]) / "WithinTheLatentWalls/research"
                        if os.environ.get("LOCALAPPDATA") else None)
    parser.add_argument("--research-dir", type=Path, default=default_research)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "documentation/rendus-scenes/toutes")
    parser.add_argument("--renderer", type=Path, default=ROOT / "build_archive_render/liminal_archive_render.exe")
    parser.add_argument("--mode", choices=("all", "atlases", "places"), default="all")
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--samples", type=int, default=32)
    parser.add_argument("--threads", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument("--timeout", type=float, default=300)
    parser.add_argument("--logs-dir", type=Path, help="New private directory outside the repository")
    parser.add_argument("--force", action="store_true", help="Recompute existing images")
    args = parser.parse_args(argv)
    if not args.research_dir or not args.research_dir.is_dir():
        parser.error("Set --research-dir to an existing research archive root")
    renderer = args.renderer.resolve()
    if not renderer.is_file():
        parser.error("Renderer missing; run python scripts/build_archive_render.py first")
    if not 256 <= args.size <= 4096 or not 1 <= args.samples <= 4096:
        parser.error("Expected size 256..4096 and samples 1..4096")
    if args.threads < 1 or not 0 < args.timeout < float("inf"):
        parser.error("Threads and finite timeout must be positive")
    research, output = args.research_dir.resolve(), args.output_dir.resolve()
    if output.is_relative_to(research) or research.is_relative_to(output):
        parser.error("Output and research directories must be disjoint")
    if args.logs_dir:
        logs = args.logs_dir.resolve()
    else:
        private_root = PRIVATE_RUNS if os.name == "nt" else Path(tempfile.gettempdir()) / "liminal-renders"
        logs = private_root / ("archive-batch-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
    if logs.is_relative_to(ROOT) or logs.is_relative_to(research) or logs.is_relative_to(output):
        parser.error("Logs must stay outside the repository, source archives and images")
    logs.mkdir(parents=True, exist_ok=False)
    output.mkdir(parents=True, exist_ok=True)
    jobs = collect_jobs(research, args.mode)
    if not jobs:
        parser.error("No run-* archives or accepted places found")
    try:
        prior_catalog = read_json(output / "catalog.json")
        previous = {r["image"]: r for r in prior_catalog["images"]}
    except (OSError, ValueError, KeyError, TypeError):
        previous = {}
    if previous:
        (logs / "catalog-before.json").write_text(json.dumps(prior_catalog, ensure_ascii=False), encoding="utf-8")
    # Retain prior proofs for ALL jobs before publishing the pending list, so
    # interruption during a cache-only pass never discards the remaining cache.
    records = [{**previous.get(j["image"], {}),
                **{k: j[k] for k in ("archive", "title", "kind", "image")}, "status": "pending"} for j in jobs]
    source_cache = {}
    renderer_hash = digest(renderer)
    print(f"{len(jobs)} images / {args.size} x {args.size} / {args.samples} samples", flush=True)
    print(f"Private logs: {logs}", flush=True)
    publish_catalog(output, records, args.size, args.samples)
    try:
        for index, job in enumerate(jobs):
            try:
                if job.get("discovery_error"):
                    raise ValueError(job["discovery_error"])
                if job["archive"] not in source_cache:
                    source_cache[job["archive"]] = source_fingerprint(job["archive_path"])
                expected = fingerprint(job, source_cache[job["archive"]], renderer_hash, args.size, args.samples)
                old = previous.get(job["image"], {})
                if not args.force and cache_valid(old, expected, output, job["image"], args.size):
                    records[index] = {**old, "status": "cached"}
                else:
                    legacy = False
                    if not args.force and cache_valid(old, old.get("fingerprint", ""), output, job["image"], args.size):
                        legacy = "labels_version" not in read_json((output / job["image"]).with_suffix(".json"))
                    records[index] = render_job(job, renderer, output, logs, args.size,
                                                args.samples, args.timeout, args.threads, annotate=legacy)
                    records[index]["fingerprint"] = expected
            except (OSError, ValueError) as error:
                records[index]["status"] = "failed"
                records[index]["failure"] = "archive_error"
                with (logs / "archive-errors.txt").open("a", encoding="utf-8") as log:
                    log.write(f"{job['image']}: {error}\n")
            print(f"[{index+1}/{len(jobs)}] {records[index]['status']}: {job['image']}", flush=True)
            publish_catalog(output, records, args.size, args.samples)
    except KeyboardInterrupt:
        publish_catalog(output, records, args.size, args.samples)
        print("Interrupted. Run the same command to resume.", flush=True)
        return 130
    counts = dict(Counter(r["status"] for r in records))
    print(json.dumps(counts), flush=True)
    print(f"Gallery: {output / 'index.html'}", flush=True)
    return 1 if counts.get("failed") else 0


if __name__ == "__main__":
    raise SystemExit(main())
