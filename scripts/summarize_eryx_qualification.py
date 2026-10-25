"""Summarize mechanical evidence; narrative judgments are deliberately not scored."""
import argparse
import collections
import json
import math
import statistics
from pathlib import Path


def percentile(values, quantile):
    ordered = sorted(values)
    return ordered[max(0, math.ceil(len(ordered) * quantile) - 1)] if ordered else None


def summarize(label, folder):
    rows = json.loads((folder / "results.json").read_text(encoding="utf-8"))
    provenance = json.loads((folder / "provenance.json").read_text(encoding="utf-8"))
    cases = [row for row in rows if not row["id"].startswith("session-")]
    sessions = [row for row in rows if row["id"].startswith("session-")]
    calls = [call for row in rows for call in row.get("llm_calls", [])]
    generations = [row for row in cases if row["id"].startswith("generation-")]
    generation_times = [row["wall_time_ms"] / 1000 for row in generations if row["returncode"] == 0]
    successful_times = [row["wall_time_ms"] / 1000 for row in rows if row["returncode"] == 0]
    def passed(row):
        return row["returncode"] == 0 and not row["invariant_errors"]
    first_pass = [row for row in rows if passed(row) and row.get("repair") == "not needed"
                  and row.get("fallback") == "not needed" and row.get("room_metadata_source") != "fallback"
                  and row.get("room_scene_source") != "fallback" and row.get("candidate_scene_audit") != "invalid"]
    summary = {
        "label": label, "directory": str(folder), "provenance": provenance,
        "cases": len(cases), "cases_passed": sum(passed(row) for row in cases),
        "session_turns": len(sessions), "session_turns_passed": sum(passed(row) for row in sessions),
        "native_failures": sum(row["returncode"] != 0 for row in rows),
        "without_observed_repair_or_fallback": len(first_pass),
        "auxiliary_metrics_available": bool(calls),
        "turns_with_all_observed_calls_successful": sum(bool(row.get("llm_calls")) and all(call["success"] for call in row["llm_calls"]) for row in rows) if calls else None,
        "p50_success_wall_s": statistics.median(successful_times) if successful_times else None,
        "p95_success_wall_s": percentile(successful_times, .95),
        "generation_cases": len(generations),
        "generation_cases_passed": sum(passed(row) for row in generations),
        "generation_metadata_from_model": sum(row.get("room_metadata_source") == "llm" for row in generations),
        "generation_metadata_fallback": sum(row.get("room_metadata_source") == "fallback" for row in generations),
        "generation_p50_success_wall_s": statistics.median(generation_times) if generation_times else None,
        "generation_p95_success_wall_s": percentile(generation_times, .95),
        "sampled_peak_device_vram_mib": max((row.get("sampled_peak_device_vram_mib") or 0 for row in rows), default=0),
        "sampled_peak_rss_mib": max((row.get("sampled_peak_process_rss_bytes", 0) / 2**20 for row in rows), default=0),
        "call_stops": dict(collections.Counter(call["stop_reason"] for call in calls)),
        "calls_by_purpose": dict(collections.Counter(call["purpose"] for call in calls)),
        "call_wall_s_by_purpose": {
            purpose: sum(call["wall_time_ms"] / 1000 for call in calls if call["purpose"] == purpose)
            for purpose in sorted({call["purpose"] for call in calls})
        },
        "effective_temperature_range": [min(call["temperature"] for call in calls), max(call["temperature"] for call in calls)] if calls else None,
        "reasoning_tokens": sum(call["reasoning_tokens"] for call in calls) if calls else None,
        "reasoning_enabled_calls": sum(bool(call.get("reasoning")) for call in calls) if calls else None,
        "calls_with_reasoning_tokens": sum(call["reasoning_tokens"] > 0 for call in calls) if calls else None,
        "final_tokens": sum(call["final_tokens"] for call in calls) if calls else None,
        "offload": sorted({f"{call['offloaded_layers']}/{call['total_layers']}" for call in calls}),
        "failures": [{"id": row["id"], "errors": row["invariant_errors"], "returncode": row["returncode"]}
                     for row in rows if not passed(row)],
        "narrative_review": "pending",
    }
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="append", required=True, help="LABEL=DIRECTORY")
    parser.add_argument("--json", type=Path, required=True)
    parser.add_argument("--markdown", type=Path, required=True)
    args = parser.parse_args()
    if args.json.exists() or args.markdown.exists():
        parser.error("Choose new output paths; prior reports are preserved")
    runs = [summarize(label, Path(folder)) for label, folder in (arg.split("=", 1) for arg in args.run)]
    lines = ["# Qualification native Eryx : mesures", "",
             "Les assertions mécaniques ne constituent pas un score artistique. Les latences ci-dessous portent sur les tours dont le processus a réussi, y compris leurs replis éventuels. Les échecs sont comptés séparément. Les anciens binaires n'exposent pas les métriques complètes des auxiliaires.", "",
             "Les scores ci-dessous couvrent les positions attendues, liens persistants, barrières, scènes sauvegardées, retours des branches et absence de balises de réflexion dans l'état. Ils ne valident pas automatiquement la justification d'un changement de ressource, les valeurs thermiques annoncées ou la cohérence narrative ; ces points nécessitent une revue séparée des sauvegardes et du texte.", "",
             "| Variante | Cas sans erreur mécanique | Tours de session sans erreur | Échecs natifs | Sans réparation/repli observé | p50 / p95 succès | Pic VRAM échantillonné |",
             "|---|---:|---:|---:|---:|---:|---:|"]
    for run in runs:
        latency = (f"{run['p50_success_wall_s']:.2f} / {run['p95_success_wall_s']:.2f} s"
                   if run["p50_success_wall_s"] is not None else "n/a")
        lines.append(f"| {run['label']} | {run['cases_passed']}/{run['cases']} | "
                     f"{run['session_turns_passed']}/{run['session_turns']} | {run['native_failures']} | "
                     f"{run['without_observed_repair_or_fallback']} | {latency} | {run['sampled_peak_device_vram_mib']} MiB |")
    lines.extend(["", "Les commandes cardinales connues sont majoritairement déterministes et peuvent ne solliciter que l'auxiliaire thermique. Pour isoler le coût d'une nouvelle salle :", "",
                  "| Variante | Branches sans erreur mécanique | Métadonnées modèle / repli | Génération de salle : p50 / p95 du tour complet |",
                  "|---|---:|---:|---:|"])
    for run in runs:
        latency = (f"{run['generation_p50_success_wall_s']:.2f} / {run['generation_p95_success_wall_s']:.2f} s"
                   if run["generation_p50_success_wall_s"] is not None else "n/a")
        lines.append(f"| {run['label']} | {run['generation_cases_passed']}/{run['generation_cases']} | "
                     f"{run['generation_metadata_from_model']} / {run['generation_metadata_fallback']} | {latency} |")
    for run in runs:
        lines.extend(["", f"## {run['label']}", "", f"Preuves : `{run['directory']}`.", "",
                      f"Arrêts des appels : `{run['call_stops']}`. Offload observé : `{run['offload']}`.", ""])
        for failure in run["failures"]:
            lines.append(f"- `{failure['id']}` : {'; '.join(failure['errors'])} (code {failure['returncode']}).")
    for path in (args.json, args.markdown):
        path.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(runs, ensure_ascii=False, indent=2), encoding="utf-8")
    args.markdown.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
