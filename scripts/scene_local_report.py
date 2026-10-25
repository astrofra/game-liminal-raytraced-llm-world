"""Aggregate local Eryx trial records and render one Markdown report.

The module is standard-library only and never runs a benchmark. It reads
already collected records, which must expose the following fields per trial:
``model``, ``case_id``, ``trial``, ``accepted``, ``first_pass``, ``total_ms``,
``render_ms`` and ``calls``. Each call may provide ``load_ms``, ``prompt_ms``,
``generation_ms``, ``eval_count`` and ``tokens_per_second``.

Unknown measurements stay unknown: absent or invalid values are rendered as a
dash and never replaced by a fabricated zero.
"""

from __future__ import annotations

import math
import os
import pathlib
import statistics
from collections.abc import Mapping
from typing import Any


def _finite_number(value: Any) -> float | None:
    """Return a finite number, or None when the value is not measurable."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    number = float(value)
    return number if math.isfinite(number) else None


def _non_negative_number(value: Any) -> float | None:
    """Return a finite non-negative number, or None when it is not usable."""
    number = _finite_number(value)
    return number if number is not None and number >= 0.0 else None


def _positive_duration(value: Any) -> float | None:
    """Return a finite strictly positive duration, as required for total_ms."""
    number = _finite_number(value)
    return number if number is not None and number > 0.0 else None


def _nearest_rank_p95(values: list[float]) -> float | None:
    if not values:
        return None
    ordered = sorted(values)
    rank = -(-95 * len(ordered) // 100)
    return ordered[rank - 1]


def summarize(records: list[dict]) -> dict:
    """Group one-trial records by model without mutating the inputs.

    Every record counts in ``trials``, including failures. Acceptance
    statistics only use accepted trials; ``all_*`` statistics use every trial
    with a measured, strictly positive ``total_ms``. Missing or invalid
    measurements produce ``None`` rather than zero. Models keep their first
    appearance order.
    """
    groups: dict[str, dict[str, Any]] = {}
    for record in records:
        model = record.get("model")
        if not isinstance(model, str) or not model:
            raise ValueError("Each record must carry a non-empty string 'model'")
        group = groups.setdefault(
            model,
            {"trials": 0, "accepted": 0, "first_pass": 0,
             "accepted_ms": [], "all_ms": []},
        )
        accepted = bool(record.get("accepted", False))
        group["trials"] += 1
        group["accepted"] += int(accepted)
        group["first_pass"] += int(accepted and bool(record.get("first_pass", False)))
        total_ms = _positive_duration(record.get("total_ms"))
        if total_ms is not None:
            group["all_ms"].append(total_ms)
            if accepted:
                group["accepted_ms"].append(total_ms)

    summary: dict[str, dict[str, Any]] = {}
    for model, group in groups.items():
        accepted_ms = group["accepted_ms"]
        all_ms = group["all_ms"]
        summary[model] = {
            "trials": group["trials"],
            "accepted": group["accepted"],
            "first_pass": group["first_pass"],
            "accepted_median_ms": statistics.median(accepted_ms) if accepted_ms else None,
            "accepted_p95_ms": _nearest_rank_p95(accepted_ms),
            "all_median_ms": statistics.median(all_ms) if all_ms else None,
            "all_p95_ms": _nearest_rank_p95(all_ms),
        }
    return summary


def _escape_cell(value: Any) -> str:
    """Escape a Markdown table cell, including pipes and line breaks."""
    if value is None:
        return "—"
    text = str(value)
    text = text.replace("\\", "\\\\").replace("|", "\\|")
    text = text.replace("\r\n", "\n").replace("\r", "\n").replace("\n", "<br>")
    return text


def _format_ms(value: float | None) -> str:
    return "—" if value is None else f"{value:.1f}"


def _sum_measured(calls: Any, key: str) -> float | None:
    """Sum one call metric, ignoring calls where the metric is unknown."""
    if not isinstance(calls, (list, tuple)):
        return None
    values = []
    for call in calls:
        if not isinstance(call, Mapping):
            continue
        value = _non_negative_number(call.get(key))
        if value is None:
            return None
        values.append(value)
    return sum(values) if values else None


def _relative_image_target(image_path: Any, base_dir: pathlib.Path) -> str:
    target = os.path.abspath(os.fspath(image_path))
    base = os.path.abspath(os.fspath(base_dir))
    return os.path.relpath(target, base).replace("\\", "/")


def write_report(records: list[dict], output_path: pathlib.Path) -> None:
    """Write the French Markdown report to ``output_path`` only."""
    output_path = pathlib.Path(output_path)
    rows = list(records)
    summary = summarize(rows)

    lines = [
        "# Rapport local — scènes Eryx",
        "",
        "Synthèse technique produite uniquement à partir des essais fournis. "
        "Les compteurs d'acceptation et de premier passage relèvent de l'audit "
        "technique ; la qualité visuelle se juge séparément, à l'œil, sans score "
        "automatique.",
        "",
        "## Synthèse par modèle",
        "",
        "| Modèle | Essais | Acceptés | Premier passage | Médiane acceptés (ms) | "
        "p95 acceptés (ms) | Médiane tous (ms) | p95 tous (ms) |",
        "|---|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for model, group in summary.items():
        lines.append("| " + " | ".join([
            _escape_cell(model),
            str(group["trials"]),
            str(group["accepted"]),
            str(group["first_pass"]),
            _format_ms(group["accepted_median_ms"]),
            _format_ms(group["accepted_p95_ms"]),
            _format_ms(group["all_median_ms"]),
            _format_ms(group["all_p95_ms"]),
        ]) + " |")
    if not summary:
        lines.extend(["", "Aucun essai fourni.", ""])

    lines.extend([
        "",
        "### Lecture des métriques",
        "",
        "- `Essais` compte tous les essais, échecs compris ; aucun échec n'est "
        "retiré du dénominateur.",
        "- Les médianes et p95 d'acceptation ne portent que sur les essais "
        "acceptés dont la durée totale est mesurée. Les colonnes « tous » "
        "incluent aussi les essais rejetés mesurés.",
        "- Une mesure absente ou invalide reste inconnue et s'affiche « — » ; "
        "aucun zéro n'est fabriqué.",
        "- Le p95 suit le rang le plus proche, `ceil(0.95 × n)`. Avec peu "
        "d'essais, il correspond souvent au maximum observé : ce n'est pas une "
        "estimation robuste sur plusieurs sessions.",
        "",
        "### Audit technique et revue visuelle",
        "",
        "L'audit technique vérifie l'acceptation par le validateur, le premier "
        "passage et les durées mesurées. Il ne dit rien de la composition, de la "
        "lisibilité du brief. La couverture géométrique du repère est contrôlée, "
        "mais sa lisibilité artistique et les relations spatiales "
        "exigent une revue visuelle humaine, essai par essai, sans note "
        "artistique automatique.",
        "",
        "## Détail par essai",
        "",
        "| Modèle | Cas | Essai | Statut | Premier passage | Total (ms) | "
        "Rendu (ms) | Appels | Génération mesurée (ms) | Erreur | Image |",
        "|---|---|---:|---|---|---:|---:|---:|---:|---|---|",
    ])
    for record in rows:
        calls = record.get("calls")
        calls_count = len(calls) if isinstance(calls, (list, tuple)) else None
        generation_ms = _sum_measured(calls, "generation_ms")
        image_path = record.get("image_path")
        if image_path:
            image_cell = f"[voir]({_relative_image_target(image_path, output_path.parent)})"
        else:
            image_cell = "—"
        error = record.get("error")
        lines.append("| " + " | ".join([
            _escape_cell(record.get("model", "—")),
            _escape_cell(record.get("case_id", "—")),
            _escape_cell(record.get("trial", "—")),
            "Accepté" if record.get("accepted") else "Rejeté",
            "Oui" if record.get("first_pass") else "Non",
            _format_ms(_positive_duration(record.get("total_ms"))),
            _format_ms(_non_negative_number(record.get("render_ms"))),
            "—" if calls_count is None else str(calls_count),
            _format_ms(generation_ms),
            "—" if error in (None, "") else _escape_cell(error),
            image_cell,
        ]) + " |")

    lines.extend([
        "",
        "La colonne « Génération mesurée » additionne les appels seulement si "
        "tous sont mesurés ; « — » signale une mesure absente, jamais "
        "un zéro. Les échecs restent visibles dans ce tableau avec leur erreur "
        "éventuelle.",
        "",
    ])

    lines += ["## Mesures par appel", "",
              "| Modèle | Cas | Essai | Appel | Chargement ms | Prompt ms | Génération ms | Tokens | Tokens/s |",
              "|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for record in rows:
        for index, call in enumerate(record.get("calls", [])):
            lines.append("| " + " | ".join([
                _escape_cell(record['model']), _escape_cell(record['case_id']), str(record['trial']), str(index+1),
                *[_format_ms(_non_negative_number(call.get(key))) for key in
                  ('load_ms','prompt_ms','generation_ms','eval_count','tokens_per_second')]]) + " |")
    lines.append("")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
