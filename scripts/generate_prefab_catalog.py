#!/usr/bin/env python3

import argparse
import datetime as dt
import os
import pathlib
import subprocess
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_RENDERER = REPO_ROOT / "build" / "Release" / "liminal_cornell_renderer.exe"
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "documentation" / "generated" / "prefab_catalog"
DEFAULT_MARKDOWN = REPO_ROOT / "documentation" / "PREFAB_CATALOG.md"


PREFABS = [
    {
        "slug": "prefab_gate",
        "title": "prefab_gate",
        "role": "Portail de seuil controle, utile pour les entrees, grilles et sas.",
        "camera_eye": (-7.2, 2.7, -7.8),
        "camera_target": (0.0, 1.6, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_gate "catalog_gate" pos(0.0,1.55,0.0) size(6.15,3.10,0.40) gray(0.46) detail(0.58) bars(5)',
    },
    {
        "slug": "prefab_rack",
        "title": "prefab_rack",
        "role": "Baie serveur ou rack technique. Le renderer y injecte automatiquement des LEDs rouges.",
        "camera_eye": (-4.8, 2.4, -5.8),
        "camera_target": (0.0, 1.3, 0.0),
        "fov": 42.0,
        "extra_geometry": [],
        "directive": 'prefab_rack "catalog_rack" pos(0.0,1.25,0.0) size(1.30,2.50,1.60) gray(0.19) detail(0.35)',
    },
    {
        "slug": "prefab_crate",
        "title": "prefab_crate",
        "role": "Caisse, bloc de service ou console basse pour meubler une salle et ajouter des prises d'action.",
        "camera_eye": (-3.9, 1.8, -4.8),
        "camera_target": (0.0, 0.9, 0.0),
        "fov": 40.0,
        "extra_geometry": [
            'box "catalog_pedestal" pos(0.0,0.20,0.0) size(3.2,0.4,2.4) gray(0.16)',
        ],
        "directive": 'prefab_crate "catalog_crate" pos(0.0,0.95,0.0) size(1.70,1.10,1.40) gray(0.20) detail(0.31)',
    },
    {
        "slug": "prefab_cooling_unit",
        "title": "prefab_cooling_unit",
        "role": "Bloc de climatisation ou masse technique pour fixer une silhouette industrielle stable.",
        "camera_eye": (-5.5, 2.5, -6.4),
        "camera_target": (0.0, 1.4, 0.0),
        "fov": 42.0,
        "extra_geometry": [],
        "directive": 'prefab_cooling_unit "catalog_cooling_unit" pos(0.0,1.35,0.0) size(1.20,2.70,3.10) gray(0.25) detail(0.37)',
    },
]


def format_vec3(values):
    return f"{values[0]:.2f},{values[1]:.2f},{values[2]:.2f}"


def build_scene_text(entry):
    lines = [
        f'room "{entry["title"]} catalog"',
        (
            "camera "
            f'eye({format_vec3(entry["camera_eye"])}) '
            f'target({format_vec3(entry["camera_target"])}) '
            "up(0.00,1.00,0.00) "
            f'fov({entry["fov"]:.1f})'
        ),
        "spotlight panel(1.00,1.00) offset(0.00,0.10,0.35) range(18.0) cone(16.0,34.0) intensity(220.0)",
        'plane "catalog_floor" pos(0.0,0.0,0.0) normal(0.0,1.0,0.0) size(18.0,18.0) gray(0.20)',
        'box "catalog_backdrop" pos(0.0,5.6,7.2) size(20.0,11.2,0.4) gray(0.30)',
        'box "catalog_left_wall" pos(-9.8,4.0,0.0) size(0.4,8.0,18.0) gray(0.26)',
        'box "catalog_right_wall" pos(9.8,4.0,0.0) size(0.4,8.0,18.0) gray(0.26)',
        'box "catalog_ceiling" pos(0.0,8.0,0.0) size(20.0,0.4,18.0) gray(0.24)',
    ]
    lines.extend(entry["extra_geometry"])
    lines.append(entry["directive"])
    return "\n".join(lines) + "\n"


def render_prefab(renderer_path, scene_path, image_path, width, height, samples):
    command = [
        str(renderer_path),
        "--scene",
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
        "3",
        "--direct-samples",
        "1",
        "--seed",
        "77",
        "--exposure",
        "1.0",
    ]
    subprocess.run(command, check=True, cwd=REPO_ROOT)


def write_markdown(markdown_path, output_root, entries, width, height, samples):
    generated_at = dt.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    lines = [
        "# Prefab Catalog",
        "",
        f"Genere le {generated_at}.",
        "",
        "Ce document rassemble le premier catalogue visuel des prefabs `scene v1` dans un seul Markdown.",
        "",
        "Notes :",
        "- chaque visuel est rendu par `liminal_cornell_renderer` a partir du `.scene` adjacent",
        "- fond volontairement neutre en niveaux de gris",
        "- le renderer n'expose pas encore de `color()` libre dans `scene v1`, donc pas de vrai fond vert catalogable a ce stade",
        f"- parametres de rendu utilises : `{width}x{height}`, `{samples}` samples par pixel",
        "",
    ]

    for entry in entries:
        scene_path = output_root / "scenes" / f'{entry["slug"]}.scene'
        image_path = output_root / "images" / f'{entry["slug"]}.png'
        relative_image = os.path.relpath(image_path, markdown_path.parent).replace("\\", "/")
        relative_scene = os.path.relpath(scene_path, markdown_path.parent).replace("\\", "/")

        lines.extend(
            [
                f"## {entry['title']}",
                "",
                entry["role"],
                "",
                f"Source scene : `{relative_scene}`",
                "",
                f"![{entry['title']}]({relative_image})",
                "",
                "Directive rendue :",
                "",
                "```text",
                entry["directive"],
                "```",
                "",
            ]
        )

    markdown_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description="Generate a visual prefab catalog and one Markdown document.")
    parser.add_argument("--renderer", type=pathlib.Path, default=DEFAULT_RENDERER)
    parser.add_argument("--output-root", type=pathlib.Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--markdown", type=pathlib.Path, default=DEFAULT_MARKDOWN)
    parser.add_argument("--width", type=int, default=960)
    parser.add_argument("--height", type=int, default=540)
    parser.add_argument("--samples", type=int, default=8)
    args = parser.parse_args()

    renderer_path = args.renderer.resolve()
    output_root = args.output_root.resolve()
    markdown_path = args.markdown.resolve()

    if not renderer_path.is_file():
        print(f"Renderer not found: {renderer_path}", file=sys.stderr)
        return 1

    scenes_dir = output_root / "scenes"
    images_dir = output_root / "images"
    scenes_dir.mkdir(parents=True, exist_ok=True)
    images_dir.mkdir(parents=True, exist_ok=True)
    markdown_path.parent.mkdir(parents=True, exist_ok=True)

    for entry in PREFABS:
        scene_path = scenes_dir / f'{entry["slug"]}.scene'
        image_path = images_dir / f'{entry["slug"]}.png'
        scene_text = build_scene_text(entry)
        scene_path.write_text(scene_text, encoding="utf-8")
        print(f"[catalog] rendering {entry['slug']} -> {image_path}")
        render_prefab(renderer_path, scene_path, image_path, args.width, args.height, args.samples)

    write_markdown(markdown_path, output_root, PREFABS, args.width, args.height, args.samples)
    print(f"[catalog] wrote {markdown_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
