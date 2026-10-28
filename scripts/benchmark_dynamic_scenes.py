#!/usr/bin/env python3
"""Banc de scènes dynamiques : pilote les runs, conserve les preuves, ne note pas l'art."""
import argparse, datetime, hashlib, html, json, os, random, re, subprocess, sys, time
from pathlib import Path
from urllib.parse import quote
METHODS = ("local", "direct", "two-pass", "hybrid")
SIDES = {"north", "east", "south", "west"}
VISUAL_SCORES = ("silhouette", "profondeur", "vide", "focale", "lumiere", "coherence", "diversite")
ID_RE = re.compile(r"^[A-Za-z0-9_-]+$")
RUN_TIMEOUT = 1200
METRIC_KEYS = ("version", "method", "success", "cancelled", "timeout", "repairs", "calls",
               "seed", "profile", "elapsed_ms", "audit", "error")
def load_corpus(path):
    try:
        raw = path.read_bytes(); data = json.loads(raw.decode("utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError(f"corpus illisible: {exc}") from exc
    if not isinstance(data, dict) or data.get("version") != 1: raise ValueError("corpus: version 1 attendue")
    cases = data.get("cases")
    if not isinstance(cases, list): raise ValueError("corpus: cases doit être une liste")
    seen, clean = set(), []
    for index, case in enumerate(cases):
        where = f"cases[{index}]"
        if not isinstance(case, dict): raise ValueError(f"{where}: objet attendu")
        cid = case.get("id")
        if not isinstance(cid, str) or not ID_RE.fullmatch(cid): raise ValueError(f"{where}: id ASCII sûr attendu")
        if cid in seen: raise ValueError(f"{where}: id dupliqué: {cid}")
        seen.add(cid)
        if case.get("split") not in ("calibration", "holdout"): raise ValueError(f"{where}: split invalide")
        if not isinstance(case.get("entry_direction"), str) or case["entry_direction"] not in SIDES: raise ValueError(f"{where}: entry_direction invalide")
        if not isinstance(case.get("brief"), str) or not case["brief"].strip(): raise ValueError(f"{where}: brief attendu")
        if not isinstance(case.get("motif"), str): raise ValueError(f"{where}: motif attendu")
        expectations = case.get("expectations")
        if not isinstance(expectations, list) or len(expectations) != 3 or not all(isinstance(x, str) for x in expectations):
            raise ValueError(f"{where}: trois textes d'attendus requis")
        clean.append({key: case[key] for key in ("id", "split", "brief", "entry_direction", "motif", "expectations")})
    return hashlib.sha256(raw).hexdigest(), clean
def build_cmd(exe, case, method, seed, profile, outdir):
    return [str(exe), "--generate-dynamic-scene", str(outdir / "brief.txt"), "--scene-method", method,
            "--scene-entry", case["entry_direction"], "--scene-artifacts", str(outdir / "scene"), "--output", str(outdir / "image.png"),
            "--width", "512", "--height", "256", "--samples", "8", "--llm-profile", profile,
            "--llm-temperature", "0.6", "--llm-seed", str(seed), "--seed", str(seed), "--scene-timeout-ms", "900000"]
def metrics_issue(metrics, method, seed):
    if not isinstance(metrics, dict): return "métriques invalides: objet JSON attendu"
    missing = [key for key in METRIC_KEYS if key not in metrics]
    if missing: return "métriques invalides: clés absentes: " + ", ".join(missing)
    if any(not isinstance(metrics[key], bool) for key in ("success", "cancelled", "timeout")):
        return "métriques invalides: booléens success/cancelled/timeout attendus"
    if not isinstance(metrics["repairs"], int) or isinstance(metrics["repairs"], bool):
        return "métriques invalides: repairs entier attendu"
    if not isinstance(metrics["calls"], list) or not isinstance(metrics["audit"], dict):
        return "métriques invalides: calls/audit de type incorrect"
    if not isinstance(metrics["seed"], int) or isinstance(metrics["seed"], bool):
        return "métriques invalides: seed entier attendu"
    if metrics["seed"] != seed: return f"métriques incohérentes: seed {metrics['seed']} != {seed}"
    if metrics["method"] != ("local-fixed-v1" if method == "local" else method): return f"métriques incohérentes: method {metrics['method']!r} != {method!r}"
    return None
def execute(root, exe, case, method, seed, profile, record):
    outdir = root / case["id"] / method / str(seed)
    outdir.mkdir(parents=True, exist_ok=True)
    (outdir / "brief.txt").write_text(case["brief"], encoding="utf-8")
    start, returncode, timed_out = time.monotonic(), None, False
    try:
        with open(outdir / "stdout.log", "w", encoding="utf-8", errors="replace") as out, open(outdir / "stderr.log", "w", encoding="utf-8", errors="replace") as err:
            returncode = subprocess.run(build_cmd(exe, case, method, seed, profile, outdir), cwd=Path(__file__).resolve().parents[1], stdout=out, stderr=err, timeout=RUN_TIMEOUT, shell=False).returncode
    except subprocess.TimeoutExpired:
        timed_out, record["error"] = True, f"timeout après {RUN_TIMEOUT} s"
    except OSError as exc:
        record["error"] = f"exécution impossible: {exc}"
    record["returncode"], record["duration_s"] = returncode, round(time.monotonic() - start, 3)
    image = outdir / "image.png"
    record["image"] = image.relative_to(root).as_posix() if image.is_file() else None
    try:
        metrics = json.loads((outdir / "scene.metrics.json").read_text(encoding="utf-8-sig"))
    except FileNotFoundError:
        metrics, metrics_error = None, "métriques absentes: scene.metrics.json"
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        metrics, metrics_error = None, f"métriques illisibles: {exc}"
    else:
        metrics_error = metrics_issue(metrics, method, seed)
    record["metrics"], record["metrics_error"] = metrics, metrics_error
    errors = [record["error"]] if record.get("error") else []
    if returncode not in (None, 0): errors.append(f"code retour {returncode}")
    if metrics_error: errors.append(metrics_error)
    if isinstance(metrics, dict) and metrics_error is None and metrics["success"] is not True:
        errors.append("metrics.success n'est pas true")
    if not image.is_file(): errors.append("image absente")
    success = returncode == 0 and metrics_error is None and isinstance(metrics, dict) and metrics["success"] is True and image.is_file()
    record["success"], record["status"] = success, "success" if success else ("timeout" if timed_out else "failure")
    record["error"] = "; ".join(dict.fromkeys(errors)) if errors else None
def hybrid_info(metrics):
    if not isinstance(metrics, dict): return None
    found = {key: metrics[key] for key in ("fallback", "source") if key in metrics}
    audit = metrics.get("audit")
    if isinstance(audit, dict): found.update({f"audit.{key}": audit[key] for key in ("fallback", "source") if key in audit})
    return found or None
def write_text(path, text):
    tmp = path.with_name(path.name + ".tmp"); tmp.write_text(text, encoding="utf-8"); os.replace(tmp, path)

def write_blind_review(root, records):
    order = list(range(len(records)))
    random.Random(20261024).shuffle(order)
    keys, scores, cards = [], [], []
    for index, source in enumerate(order, 1):
        label, record = f"V{index:03d}", records[source]
        keys.append({"label": label, "case": record["id"], "method": record["method"], "seed": record["seed"]})
        scores.append({"label": label, "scores": {criterion: None for criterion in VISUAL_SCORES}, "notes": ""})
        picture = f'<img src="{esc(quote(record["image"], safe="/"))}" alt="{label}">' if record.get("image") else '<p>Échec : aucune vue acceptée.</p>'
        cards.append(f'<article><h2>{label}</h2><p>{esc(record["brief"])}</p>{picture}</article>')
    write_text(root / "review-key.json", json.dumps(keys, ensure_ascii=False, indent=2))
    write_text(root / "visual-scores-template.json", json.dumps(scores, ensure_ascii=False, indent=2))
    write_text(root / "blind.html", '<!doctype html><html lang="fr"><meta charset="utf-8"><title>Revue visuelle</title>'
        '<style>body{background:#20251f;color:#eee;font:16px system-ui;margin:2rem}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(480px,1fr));gap:2rem}img{width:100%}article{background:#30382d;padding:1rem}</style>'
        '<h1>Revue visuelle — méthodes masquées</h1><p>Noter les sept critères de 0 à 4 dans une copie du gabarit JSON. La provenance se trouve dans review-key.json.</p><main>' + ''.join(cards) + '</main></html>')
def summarize(records, interrupted):
    attempts = len(records); successes = sum(1 for r in records if r.get("success") is True)
    return {"attempts": attempts, "successes": successes, "acceptance_rate": successes / attempts if attempts else None, "interrupted": interrupted}
def esc(value): return html.escape("" if value is None else str(value), quote=True)
def json_html(value): return esc(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True))
def build_html(results):
    summary = results["summary"]
    rate = "n/a (aucun run)" if summary["acceptance_rate"] is None else f'{summary["acceptance_rate"] * 100:.1f} % ({summary["successes"]}/{summary["attempts"]})'
    parts = ['<!doctype html><html lang="fr"><head><meta charset="utf-8"><title>Banc de scènes</title>',
             '<style>body{font:14px system-ui;margin:2rem;max-width:1100px}article{border-top:1px solid #ccc;margin-top:2rem;padding-top:1rem}img{max-width:100%;image-rendering:pixelated}pre{white-space:pre-wrap}.todo{background:#ffc}.missing{color:#a00}</style></head><body>',
             '<h1>Banc de scènes dynamiques</h1>',
             f'<p><b>Taux d\'acceptation technique :</b> {esc(rate)}. Compte les runs avec code retour 0, metrics.success=true et image présente ; ne mesure pas la qualité artistique.</p>',
             f'<p>Profil : {esc(results["configuration"]["profile"])} · Seeds : {esc(results["configuration"]["seeds"])} · SHA-256 corpus : <code>{esc(results["corpus_sha256"])}</code> · Interrompu : {esc(summary["interrupted"])}</p>',
             '<h2>Scores visuels (à noter manuellement)</h2><table><thead><tr><th>Critère</th><th>Note</th></tr></thead><tbody>']
    parts += [f"<tr><td>{esc(name)}</td><td>à noter</td></tr>" for name in VISUAL_SCORES]
    parts.append('</tbody></table><p>Aucune note calculée, aucun gate de promotion automatique.</p>')
    for record in results["cases"]:
        metrics = record.get("metrics")
        audit = metrics.get("audit") if isinstance(metrics, dict) else None
        parts += [f'<article><h2>{esc(record["id"])} — {esc(record["method"])} — seed {esc(record["seed"])}</h2>',
                  f'<p>Split : {esc(record["split"])} · Entrée : {esc(record["entry_direction"])} · Motif : {esc(record["motif"])}</p>',
                  f'<p><b>Brief :</b> {esc(record["brief"])}</p><p><b>Critères :</b></p><ol>']
        parts += [f"<li>{esc(item)}</li>" for item in record["expectations"]]
        parts.append("</ol>")
        if record.get("image"):
            parts.append(f'<figure><img src="{esc(quote(record["image"], safe="/"))}" alt="Scène {esc(record["id"])}"><figcaption>{esc(record["image"])}</figcaption></figure>')
        else:
            parts.append('<p class="missing">Image absente.</p>')
        parts.append(f'<p><b>Statut :</b> {esc(record["status"])} · <b>Code retour :</b> {esc(record["returncode"] if record["returncode"] is not None else "non disponible")} · <b>Durée :</b> {esc(record["duration_s"] if record["duration_s"] is not None else "non disponible")} s</p>')
        if record.get("error"): parts.append(f'<p class="missing"><b>Erreur :</b> {esc(record["error"])}</p>')
        parts.append('<details><summary>Audit runtime</summary><pre>' + (json_html(audit) if audit is not None else "non disponible") + '</pre></details>')
        if record["method"] == "hybrid":
            source = hybrid_info(metrics)
            if source: parts.append('<details open><summary>Source hybride</summary><pre>' + json_html(source) + '</pre></details>')
        parts.append('<p class="todo"><b>Scores visuels :</b> ' + esc(", ".join(VISUAL_SCORES)) + ' — à noter. Aucune note calculée.</p></article>')
    parts.append("</body></html>")
    return "\n".join(parts)
def main():
    parser = argparse.ArgumentParser(description="Pilote du banc de scènes dynamiques")
    parser.add_argument("--exe", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--corpus", default="tests/data/dynamic_scene_briefs.json")
    parser.add_argument("--seeds", type=int, nargs="+", default=[77, 1337])
    parser.add_argument("--methods", nargs="+", choices=METHODS, default=list(METHODS))
    parser.add_argument("--profile", default="gemma-4-e4b-q4")
    parser.add_argument("--limit", type=int, default=3)
    parser.add_argument("--split", choices=("calibration", "holdout", "all"), default="calibration")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    if len(set(args.seeds)) != len(args.seeds) or len(set(args.methods)) != len(args.methods):
        parser.error("Graines et méthodes doivent être uniques")
    if any(seed < 0 or seed > 2147483647 for seed in args.seeds): parser.error("Graines hors plage 0..2147483647")
    if args.limit < 0: parser.error("--limit doit être >= 0")
    try:
        corpus_sha, corpus_cases = load_corpus(Path(args.corpus))
    except ValueError as exc:
        print(f"Erreur: {exc}", file=sys.stderr); return 2
    selected = [c for c in corpus_cases if args.split == "all" or c["split"] == args.split][:args.limit]
    exe, root = Path(args.exe).expanduser().resolve(), Path(args.output).expanduser().resolve()
    config = {"exe": str(exe), "output": str(root), "corpus": str(Path(args.corpus).expanduser().resolve()),
              "seeds": args.seeds, "methods": args.methods, "profile": args.profile, "limit": args.limit,
              "split": args.split, "dry_run": args.dry_run, "timeout_seconds": RUN_TIMEOUT,
              "width": 512, "height": 256, "samples": 8, "scene_timeout_ms": 900000, "temperature": 0.6}
    if args.dry_run:
        for case in selected:
            for method in args.methods:
                for seed in args.seeds:
                    outdir = root / case["id"] / method / str(seed)
                    print(f"[dry-run] output={outdir}"); print("  " + subprocess.list2cmdline(build_cmd(exe, case, method, seed, args.profile, outdir)))
        print("[dry-run] aucune exécution, aucun résultat, aucun score."); return 0
    if root.exists() and not root.is_dir(): print(f"Erreur: --output n'est pas un répertoire: {root}", file=sys.stderr); return 2
    for case in selected:
        case_dir = root / case["id"]
        if case_dir.exists() and (not case_dir.is_dir() or any(case_dir.iterdir())):
            print(f"Erreur: répertoire de cas déjà rempli: {case_dir}", file=sys.stderr); return 2
    root.mkdir(parents=True, exist_ok=True)
    records, interrupted = [], False
    try:
        for case in selected:
            for method in args.methods:
                for seed in args.seeds:
                    record = {**{k: case[k] for k in ("id", "split", "brief", "entry_direction", "motif", "expectations")},
                              "method": method, "seed": seed, "status": "interrupted", "success": False,
                              "returncode": None, "duration_s": None, "error": None, "image": None,
                              "metrics": None, "metrics_error": None}
                    records.append(record)
                    try: execute(root, exe, case, method, seed, args.profile, record)
                    except Exception as exc: record["status"], record["error"] = "failure", f"erreur du pilote: {exc!r}"
                    print(f"{record['status']}: {case['id']} / {method} / {seed}", file=sys.stderr)
    except KeyboardInterrupt:
        interrupted = True
        for record in records:
            if record["status"] == "interrupted": record["error"] = "interrompu (Ctrl+C)"
        print("Interruption: résultats acquis sauvegardés, succès non revendiqué.", file=sys.stderr)
    results = {"version": 1, "generated_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
               "configuration": config, "corpus_sha256": corpus_sha,
               "summary": summarize(records, interrupted), "cases": records}
    write_text(root / "results.json", json.dumps(results, ensure_ascii=False, indent=2) + "\n")
    write_text(root / "compare.html", build_html(results) + "\n")
    write_blind_review(root, records)
    print(f"Résultats: {root / 'results.json'} | Rapport: {root / 'compare.html'}", file=sys.stderr)
    return 130 if interrupted else (1 if any(not r["success"] for r in records) else 0)
if __name__ == "__main__":
    sys.exit(main())
