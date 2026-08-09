#!/usr/bin/env python3

import argparse
import datetime as dt
import os
import pathlib
import subprocess
import sys


REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
DEFAULT_RENDERER = (
    REPO_ROOT / "build" / "Release" / "liminal_cornell_renderer.exe"
    if os.name == "nt"
    else REPO_ROOT / "build" / "liminal_cornell_renderer"
)
DEFAULT_OUTPUT_ROOT = REPO_ROOT / "documentation" / "generated" / "prefab_catalog"
DEFAULT_MARKDOWN = REPO_ROOT / "documentation" / "PREFAB_CATALOG.md"
DEFAULT_CATALOG_WIDTH = 1024
DEFAULT_CATALOG_HEIGHT = 1024
DEFAULT_CATALOG_SAMPLES = 24
SKY_DIRECTIVE = (
    "sky zenith(0.025) horizon(0.30) nadir(0.015) "
    "band(0.42) curve(1.55) noise(0.16) stars(0.0,0.0,0.0) seed(77)"
)


PREFABS = [
    {
        "slug": "prefab_gate",
        "title": "prefab_gate",
        "role": "Seuil de carrière asymétrique : deux piles massives, un linteau en porte-à-faux et une grille de contrôle légère. Il marque une limite humaine sans représenter les murs invisibles du labyrinthe.",
        "camera_eye": (-10.0, 3.8, -11.2),
        "camera_target": (0.0, 1.70, 0.0),
        "fov": 40.0,
        "extra_geometry": [],
        "directive": 'prefab_gate "catalog_quarry_gate" pos(0.0,1.90,0.0) size(6.60,3.80,0.65) gray(0.31) detail(0.48) bars(6)',
    },
    {
        "slug": "prefab_crate",
        "title": "prefab_crate",
        "role": "Container de prélèvement, réserve d'oxygène ou caisse d'outillage. Le bandeau frontal et le sceau clair en font une prise d'action lisible dans une scène pauvre.",
        "camera_eye": (-4.9, 1.8, -5.9),
        "camera_target": (0.0, 0.62, 0.0),
        "fov": 40.0,
        "extra_geometry": [],
        "directive": 'prefab_crate "catalog_sample_case" pos(0.0,0.60,0.0) size(1.90,1.20,1.50) gray(0.20) detail(0.36)',
    },
    {
        "slug": "prefab_survey_beacon",
        "title": "prefab_survey_beacon",
        "role": "Balise de route et mât de relèvement. Sa fourche ouverte, sa barre de visée et son unique lampe fournissent un repère fragile lorsque les relations spatiales dérivent.",
        "camera_eye": (-5.8, 3.0, -7.2),
        "camera_target": (0.0, 1.75, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_survey_beacon "catalog_route_datum" pos(0.0,1.90,0.0) size(1.20,3.80,1.10) gray(0.24) detail(0.44)',
    },
    {
        "slug": "prefab_crystal_scanner",
        "title": "prefab_crystal_scanner",
        "role": "Instrument d'affinité pour localiser et examiner un échantillon. Deux piles épaisses encadrent un petit cristal diélectrique filtré par son épaisseur et une ligne de mesure suspendue.",
        "camera_eye": (-6.4, 2.7, -7.4),
        "camera_target": (0.0, 1.15, 0.0),
        "fov": 40.0,
        "extra_geometry": [],
        "directive": 'prefab_crystal_scanner "catalog_affinity_scanner" pos(0.0,1.25,0.0) size(2.60,2.50,1.80) gray(0.25) detail(0.45) glow(0.28)',
    },
    {
        "slug": "prefab_crystal_cluster",
        "title": "prefab_crystal_cluster",
        "role": "Veine cristalline affleurante, à la fois ressource, balise lumineuse et appât. Les cinq prismes utilisent un verre diélectrique verrouillé : réfraction, Fresnel, dispersion RGB et filtre d'absorption dépendant de l'épaisseur.",
        "camera_eye": (-5.8, 2.8, -6.8),
        "camera_target": (0.0, 1.15, 0.0),
        "fov": 40.0,
        "extra_geometry": [],
        "directive": 'prefab_crystal_cluster "catalog_exposed_vein" pos(0.0,1.30,0.0) size(2.00,2.60,1.80) gray(0.66) glow(0.34)',
    },
    {
        "slug": "prefab_extraction_rig",
        "title": "prefab_extraction_rig",
        "role": "Portique de forage compact : jambes inclinées, tête suspendue, colonne et pointe de coupe. Il exprime l'extraction sans devenir une machine décorative complexe.",
        "camera_eye": (-7.2, 3.4, -8.6),
        "camera_target": (0.0, 1.55, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_extraction_rig "catalog_diamond_drill" pos(0.0,1.80,0.0) size(3.00,3.60,2.40) gray(0.23) detail(0.43)',
    },
    {
        "slug": "prefab_prospect_shelter",
        "title": "prefab_prospect_shelter",
        "role": "Abri de prospection brutaliste : socle, masses décalées, entrée profondément en retrait et toiture en porte-à-faux. Une architecture humaine, visible et lourde.",
        "camera_eye": (-9.4, 4.2, -10.6),
        "camera_target": (0.0, 1.55, 0.0),
        "fov": 42.0,
        "extra_geometry": [],
        "directive": 'prefab_prospect_shelter "catalog_field_shelter" pos(0.0,1.70,0.0) size(5.60,3.40,4.20) gray(0.28) detail(0.41)',
    },
    {
        "slug": "prefab_quarry_pylon",
        "title": "prefab_quarry_pylon",
        "role": "Pylône de cote implanté sur un gradin de carrière. Sa couronne fendue reprend les volumes brutalistes emboîtés et distingue un repère industriel d'une simple lampe.",
        "camera_eye": (-6.0, 3.3, -7.4),
        "camera_target": (0.0, 1.85, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_quarry_pylon "catalog_quarry_datum" pos(0.0,2.00,0.0) size(1.60,4.00,1.40) gray(0.27) detail(0.45)',
    },
    {
        "slug": "prefab_atmospheric_processor",
        "title": "prefab_atmospheric_processor",
        "role": "Unité de service atmosphérique et d'oxygène : masse basse, double cheminée et prises d'air frontales. Elle remplace la climatisation de datacenter par un besoin propre à Vénus.",
        "camera_eye": (-6.7, 3.0, -8.0),
        "camera_target": (0.0, 1.45, 0.0),
        "fov": 41.0,
        "extra_geometry": [],
        "directive": 'prefab_atmospheric_processor "catalog_oxygen_service" pos(0.0,1.60,0.0) size(2.50,3.20,2.00) gray(0.24) detail(0.41)',
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
        "spotlight panel(1.00,1.00) offset(0.00,0.10,0.35) range(24.0) cone(16.0,36.0) intensity(250.0)",
        'plane "quarry_ground" pos(0.0,-0.02,0.0) normal(0.0,1.0,0.0) size(14.0,14.0) gray(0.11)',
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
        "--glass-bounces",
        "9",
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
        "# Catalogue des prefabs — carrière d’Eryx",
        "",
        f"Généré le {generated_at}.",
        "",
        "Ce document est généré depuis les géométries C++ réellement prises en charge par le format `scene v1`. Il fixe le vocabulaire visuel actif de la carrière de cristal vénusienne : infrastructure humaine brutaliste, instruments de prospection lisibles, cristaux facettés et repères de navigation.",
        "",
        "La structure alien reste volontairement absente du catalogue : ses murs sont une propriété de collision et de topologie, pas un prefab visible. Les anciennes directives de rack, refroidissement et serveur IA restent acceptées pour la compatibilité des scènes historiques, mais ne font plus partie de ce vocabulaire actif.",
        "",
        "Sources de direction : [brief de réorientation](ERYX_PROJECT_REORIENTATION_CODEX_BRIEF.md), [nouvelle](IN_THE_WALLS_OF_ERYX.md), [origines visuelles](moodboard/origins/) et [brutalisme](moodboard/brutalism/).",
        "",
        "Notes :",
        "- chaque visuel est rendu par `liminal_cornell_renderer` à partir du `.scene` adjacent",
        "- rendu carré sans changement d’architecture du raytraceur : seul le buffer de sortie change",
        "- chaque objet repose sur un plan de carrière neutre sous un ciel brumeux sans étoiles",
        "- deux vues par prefab : 3/4 gauche et 3/4 droite",
        "- les cristaux utilisent un verre diélectrique interne à IOR central 1,52, dispersion RGB et filtre d’épaisseur ; le format `.scene` n’expose pas de matériau libre",
        "- profondeur de rendu : 3 événements diffus et 9 événements diélectriques au maximum par chemin",
        f"- paramètres de rendu utilisés : `{width}x{height}`, `{samples}` samples par pixel",
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

    expected_scene_names = {
        f'{entry["slug"]}_{view}.scene'
        for entry in PREFABS
        for view in ("left", "right")
    }
    expected_image_names = {
        f'{entry["slug"]}_{view}.png'
        for entry in PREFABS
        for view in ("left", "right")
    }
    for stale_scene in scenes_dir.glob("*.scene"):
        if stale_scene.name not in expected_scene_names:
            stale_scene.unlink()
    for stale_image in images_dir.glob("*.png"):
        if stale_image.name not in expected_image_names:
            stale_image.unlink()

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
