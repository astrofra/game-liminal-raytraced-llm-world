#!/usr/bin/env python3
"""Galerie HTML autonome pour les rendus d'archives d'un traitement par lot."""

import html
import pathlib
import urllib.parse

STATUS_LABELS = {
    "rendered": "Rendu",
    "cached": "En cache",
    "failed": "Échec",
    "pending": "En attente",
}
KIND_LABELS = {"atlas": "Atlas", "place": "Lieu"}
COUNT_LABELS = (
    ("rendered", "Rendus"),
    ("cached", "En cache"),
    ("failed", "Échecs"),
    ("pending", "En attente"),
)
IMAGE_STATUSES = frozenset({"rendered", "cached"})

STYLE = """
* { box-sizing: border-box; }
body { margin: 0 auto; max-width: 1100px; padding: 2rem 1.25rem 3rem; font-family: system-ui, sans-serif; line-height: 1.5; color: #1f2328; background: #f6f6f4; }
h1 { margin: 0 0 0.5rem; font-size: 1.6rem; }
h2 { margin: 0 0 0.75rem; padding-bottom: 0.4rem; font-size: 1.15rem; border-bottom: 1px solid #d7d7d2; }
.counters { margin: 0 0 1.5rem; color: #4a4f55; }
.counters span { margin-right: 1rem; white-space: nowrap; }
.archive { margin-bottom: 2rem; }
.cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(180px, 1fr)); gap: 1rem; margin: 0; padding: 0; list-style: none; }
.card { display: flex; flex-direction: column; gap: 0.4rem; padding: 0.75rem; background: #fff; border: 1px solid #dcdcd6; border-radius: 6px; }
.thumb { display: block; line-height: 0; }
.thumb img { display: block; width: 100%; height: auto; border-radius: 4px; }
.card-title { margin: 0; font-weight: 600; overflow-wrap: anywhere; }
.meta { margin: 0; font-size: 0.85rem; color: #4a4f55; }
.status-failed { color: #a4262c; }
.status-pending { color: #8a6d00; }
.empty, footer { color: #4a4f55; font-size: 0.9rem; }
"""


def _label(table, value):
    if isinstance(value, str) and value in table:
        return table[value]
    return value if isinstance(value, str) else str(value)


def _render_card(record):
    title = html.escape(str(record.get("title", "")))
    kind = html.escape(_label(KIND_LABELS, record.get("kind", "")))
    status = record.get("status")
    status_key = status if isinstance(status, str) and status in STATUS_LABELS else "unknown"
    status_text = html.escape(_label(STATUS_LABELS, status if status is not None else ""))
    image = record.get("image")
    parts = ['<li class="card">']
    if isinstance(status, str) and status in IMAGE_STATUSES and image not in (None, ""):
        url = html.escape(urllib.parse.quote(str(image), safe="/"))
        parts.append(
            f'<a class="thumb" href="{url}">'
            f'<img src="{url}" alt="{title}" loading="lazy" decoding="async">'
            "</a>"
        )
    parts.append(f'<p class="card-title">{title}</p>')
    parts.append(
        f'<p class="meta">{kind} · '
        f'<span class="status status-{status_key}">{status_text}</span></p>'
    )
    parts.append("</li>")
    return "\n".join(parts)


def _render_counters(records):
    counts = {status: 0 for status in STATUS_LABELS}
    for record in records:
        status = record.get("status")
        if isinstance(status, str) and status in counts:
            counts[status] += 1
    parts = [f"<span>{len(records)} entrées</span>"]
    for status, label in COUNT_LABELS:
        parts.append(f"<span>{label} : {counts[status]}</span>")
    return '<p class="counters">' + " · ".join(parts) + "</p>"


def write_gallery(output_dir: pathlib.Path, records: list[dict]) -> None:
    groups = {}
    for record in records:
        groups.setdefault(str(record.get("archive", "")), []).append(record)

    parts = [
        "<!DOCTYPE html>",
        '<html lang="fr">',
        "<head>",
        '<meta charset="utf-8">',
        '<meta name="viewport" content="width=device-width, initial-scale=1">',
        "<title>Galerie des rendus d'archives</title>",
        "<style>",
        STYLE.strip(),
        "</style>",
        "</head>",
        "<body>",
        "<header>",
        "<h1>Galerie des rendus d'archives</h1>",
        _render_counters(records),
        "</header>",
        "<main>",
    ]
    if groups:
        for archive, items in groups.items():
            parts.append('<section class="archive">')
            parts.append(f"<h2>{html.escape(archive)}</h2>")
            parts.append('<ul class="cards">')
            parts.extend(_render_card(record) for record in items)
            parts.append("</ul>")
            parts.append("</section>")
    else:
        parts.append('<p class="empty">Aucun rendu à afficher.</p>')
    parts.extend(
        [
            "</main>",
            "<footer><p>Page locale générée sans ressource externe.</p></footer>",
            "</body>",
            "</html>",
            "",
        ]
    )
    (output_dir / "index.html").write_text(
        "\n".join(parts), encoding="utf-8", newline="\n"
    )
