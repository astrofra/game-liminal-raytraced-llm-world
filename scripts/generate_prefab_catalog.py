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
DEFAULT_CATALOG_WIDTH = 1536
DEFAULT_CATALOG_HEIGHT = 1536
DEFAULT_CATALOG_SAMPLES = 32
SKY_DIRECTIVE = (
    "sky zenith(0.01) horizon(0.24) nadir(0.00) "
    "band(0.32) curve(1.95) noise(0.12) stars(0.0036,1.55,0.100) seed(77)"
)


PREFABS = [
    {
        "slug": "prefab_gate",
        "title": "prefab_gate",
        "role": "Portail de seuil controle, utile pour les entrees, grilles et sas.",
        "camera_eye": (-10.0, 2.8, -11.2),
        "camera_target": (0.0, 1.55, 0.0),
        "fov": 40.0,
        "extra_geometry": [],
        "directive": 'prefab_gate "catalog_gate" pos(0.0,1.55,0.0) size(6.15,3.10,0.40) gray(0.46) detail(0.58) bars(5)',
    },
    {
        "slug": "prefab_rack",
        "title": "prefab_rack",
        "role": "Baie serveur ou rack technique. Le renderer y injecte automatiquement des LEDs rouges.",
        "camera_eye": (-5.8, 2.4, -7.0),
        "camera_target": (0.0, 1.3, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_rack "catalog_rack" pos(0.0,1.25,0.0) size(1.30,2.50,1.60) gray(0.19) detail(0.35)',
    },
    {
        "slug": "prefab_crate",
        "title": "prefab_crate",
        "role": "Caisse, bloc de service ou console basse pour meubler une salle et ajouter des prises d'action.",
        "camera_eye": (-4.9, 1.8, -5.9),
        "camera_target": (0.0, 0.55, 0.0),
        "fov": 40.0,
        "extra_geometry": [],
        "directive": 'prefab_crate "catalog_crate" pos(0.0,0.55,0.0) size(1.70,1.10,1.40) gray(0.20) detail(0.31)',
    },
    {
        "slug": "prefab_cooling_unit",
        "title": "prefab_cooling_unit",
        "role": "Bloc de climatisation ou masse technique pour fixer une silhouette industrielle stable.",
        "camera_eye": (-6.2, 2.5, -7.8),
        "camera_target": (0.0, 1.4, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_cooling_unit "catalog_cooling_unit" pos(0.0,1.35,0.0) size(1.20,2.70,3.10) gray(0.25) detail(0.37)',
    },
]


def format_vec3(values):
    return f"{values[0]:.2f},{values[1]:.2f},{values[2]:.2f}"


def mirror_eye(values):
    return (-values[0], values[1], values[2])


def build_scene_text(entry, camera_eye, view_slug):
    lines = [
        f'room "{entry["title"]} catalog {view_slug}"',
        (
            "camera "
            f'eye({format_vec3(camera_eye)}) '
            f'target({format_vec3(entry["camera_target"])}) '
            "up(0.00,1.00,0.00) "
            f'fov({entry["fov"]:.1f})'
        ),
        SKY_DIRECTIVE,
        "spotlight panel(1.00,1.00) offset(0.00,0.10,0.35) range(20.0) cone(16.0,34.0) intensity(220.0)",
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
        "- rendu carre sans changement d'architecture du raytraceur : seul le buffer de sortie change",
        "- studio ouvert avec `sky` visible et sans sol : les prefabs flottent volontairement dans le vide",
        "- deux vues par prefab : 3/4 gauche et 3/4 droite",
        f"- parametres de rendu utilises : `{width}x{height}`, `{samples}` samples par pixel",
        "",
    ]

    for entry in entries:
        left_scene_path = output_root / "scenes" / f'{entry["slug"]}_left.scene'
        right_scene_path = output_root / "scenes" / f'{entry["slug"]}_right.scene'
        left_image_path = output_root / "images" / f'{entry["slug"]}_left.png'
        right_image_path = output_root / "images" / f'{entry["slug"]}_right.png'
        relative_left_image = os.path.relpath(left_image_path, markdown_path.parent).replace("\\", "/")
        relative_right_image = os.path.relpath(right_image_path, markdown_path.parent).replace("\\", "/")
        relative_left_scene = os.path.relpath(left_scene_path, markdown_path.parent).replace("\\", "/")
        relative_right_scene = os.path.relpath(right_scene_path, markdown_path.parent).replace("\\", "/")

        lines.extend(
            [
                f"## {entry['title']}",
                "",
                entry["role"],
                "",
                f"Scene gauche : `{relative_left_scene}`",
                f"Scene droite : `{relative_right_scene}`",
                "",
                f"Vue 3/4 gauche : ![{entry['title']} left]({relative_left_image})",
                "",
                f"Vue 3/4 droite : ![{entry['title']} right]({relative_right_image})",
                "",
                "Directive rendue :",
                "",
                "```text",
                entry["directive"],
                "```",
                "",
            ]
        )

    markdown_path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description="Generate a visual prefab catalog and one Markdown document.")
    parser.add_argument("--renderer", type=pathlib.Path, default=DEFAULT_RENDERER)
    parser.add_argument("--output-root", type=pathlib.Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--markdown", type=pathlib.Path, default=DEFAULT_MARKDOWN)
    parser.add_argument("--width", type=int, default=DEFAULT_CATALOG_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_CATALOG_HEIGHT)
    parser.add_argument("--samples", type=int, default=DEFAULT_CATALOG_SAMPLES)
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
        view_eyes = {
            "left": entry["camera_eye"],
            "right": mirror_eye(entry["camera_eye"]),
        }
        for view_slug, camera_eye in view_eyes.items():
            scene_path = scenes_dir / f'{entry["slug"]}_{view_slug}.scene'
            image_path = images_dir / f'{entry["slug"]}_{view_slug}.png'
            scene_text = build_scene_text(entry, camera_eye, view_slug)
            scene_path.write_text(scene_text, encoding="utf-8")
            print(f"[catalog] rendering {entry['slug']} {view_slug} -> {image_path}")
            render_prefab(renderer_path, scene_path, image_path, args.width, args.height, args.samples)

    write_markdown(markdown_path, output_root, PREFABS, args.width, args.height, args.samples)
    print(f"[catalog] wrote {markdown_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
