"""Compare saved runs without regenerating or altering their evidence."""

import argparse
import json
import math
import os
import pathlib
import statistics

from scene_benchmark_quality import audit_camera
from scene_benchmark_protocol import sha256_file


def percentile95(values):
    return sorted(values)[math.ceil(len(values) * 0.95) - 1] if values else 0


def analyze_run(directory):
    data = json.loads((directory / 'results.json').read_text(encoding='utf-8'))
    rows = []
    for result in data['results']:
        # Resolve artifacts relative to the run, so archived runs remain portable.
        scene = directory / 'scenes' / (result['slug'] + '.scene')
        camera_errors = audit_camera(scene.read_text(encoding='utf-8')) if scene.is_file() else ['No scene delivered']
        syntax_valid = result.get('syntax_valid_scene', result['valid_scene'])
        rows.append({**result, 'camera_errors': camera_errors,
                     'syntax_valid': syntax_valid,
                     'prompt_sha256': sha256_file(directory / 'prompts' / (result['slug'] + '.prompt.txt')),
                     'usable_camera': syntax_valid and not camera_errors})
    times = [row['generation_time_ms'] / 1000 for row in rows]
    return {
        'label': directory.name, 'directory': str(directory),
        'generated_at': data['generated_at'],
        'model': data['model'], 'reasoning': data.get('reasoning', 'legacy default'),
        'reasoning_budget': data.get('reasoning_budget'),
        'syntax_valid': sum(row['syntax_valid'] for row in rows),
        'usable_camera': sum(row['usable_camera'] for row in rows),
        'total': len(rows), 'generation_median_s': statistics.median(times) if times else 0,
        'generation_p95_s': percentile95(times), 'results': rows,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='append', required=True, type=pathlib.Path)
    parser.add_argument('--markdown', required=True, type=pathlib.Path)
    parser.add_argument('--json', required=True, type=pathlib.Path)
    args = parser.parse_args()
    if args.markdown.exists() or args.json.exists():
        parser.error('Comparison outputs already exist; choose new paths')
    runs = [analyze_run(path.resolve()) for path in args.run]
    reference_slugs = {row['slug'] for row in runs[0]['results']}
    if any({row['slug'] for row in run['results']} != reference_slugs for run in runs):
        parser.error('Runs must contain the same case identifiers')
    reference_prompts = {row['slug']: row['prompt_sha256'] for row in runs[0]['results']}
    for run in runs:
        run['changed_prompts_vs_first_run'] = [row['slug'] for row in run['results']
                                             if row['prompt_sha256'] != reference_prompts[row['slug']]]
    lines = [
        '# Comparaison des modèles — génération directe de scènes', '',
        f"Première exécution : {runs[0]['generated_at']}. {len(reference_slugs)} briefs identiques par variante ; détails des profils dans les preuves.", '',
        'La validité syntaxique correspond à l’acceptation par le renderer. Le contrôle de caméra rejette une base dégénérée ; il ne mesure ni la composition artistique, ni la fidélité au brief.', '',
        '| Variante | Syntaxe | Caméra non dégénérée et syntaxe valide | Génération médiane | p95 empirique |',
        '|---|---:|---:|---:|---:|',
    ]
    for run in runs:
        lines.append(f"| {run['label']} | {run['syntax_valid']}/{run['total']} | {run['usable_camera']}/{run['total']} | {run['generation_median_s']:.2f} s | {run['generation_p95_s']:.2f} s |")
    lines.extend(['',
        'Le temps de génération inclut le chargement et le processus CLI, mais exclut le hachage initial, la création du prompt et le rendu. Avec dix observations, le p95 par rang correspond au maximum ; ce n’est pas une estimation sur plusieurs sessions.', '',
        'Les journaux détaillés renseignent les tokens, la fin EOS, le traitement du prompt, le décodage et les allocations à la fermeture. Ces allocations ne sont pas un pic RAM/VRAM. Le premier texte utile n’est pas chronométré.', '',
        '## Détail et images', '',
    ])

    def link(path):
        return os.path.relpath(path, args.markdown.resolve().parent).replace('\\', '/')

    for run in runs:
        root = pathlib.Path(run['directory'])
        lines.extend([f"### {run['label']}", '',
                      f"Modèle : `{pathlib.Path(run['model']).name}`. Réflexion : `{run['reasoning']}`, budget `{run['reasoning_budget']}`.", '',
                      f"Prompts utilisateur différents de la première variante : {len(run['changed_prompts_vs_first_run'])}.", '',
                      f"[Résultats bruts]({link(root / 'results.json')})", '',
                      '| Cas | Statut initial | Contrôle caméra | Génération | Image |',
                      '|---|---|---|---:|---|'])
        for row in run['results']:
            errors = '; '.join(row['camera_errors']) or 'OK'
            image = root / 'images' / (row['slug'] + '.png')
            image_link = f"[voir]({link(image)})" if image.is_file() else '—'
            lines.append(f"| {row['slug']} | {row['status']} | {errors} | {row['generation_time_ms']/1000:.2f} s | {image_link} |")
        lines.append('')
    first_slug = runs[0]['results'][0]['slug']
    lines.extend(['## Premier cas : comparaison visuelle', ''])
    for run in runs:
        image = pathlib.Path(run['directory']) / 'images' / (first_slug + '.png')
        if image.is_file():
            lines.extend([f"### {run['label']}", '', f"![{run['label']}]({link(image)})", ''])
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.write_text('\n'.join(lines), encoding='utf-8')
    args.json.write_text(json.dumps(runs, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    for run in runs:
        print(f"{run['label']}: syntax {run['syntax_valid']}/{run['total']}, camera {run['usable_camera']}/{run['total']}, median {run['generation_median_s']:.2f}s")


if __name__ == '__main__':
    main()
