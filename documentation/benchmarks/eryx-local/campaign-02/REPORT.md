# Rapport local — scènes Eryx

Synthèse technique produite uniquement à partir des essais fournis. Les compteurs d'acceptation et de premier passage relèvent de l'audit technique ; la qualité visuelle se juge séparément, à l'œil, sans score automatique.

## Synthèse par modèle

| Modèle | Essais | Acceptés | Premier passage | Médiane acceptés (ms) | p95 acceptés (ms) | Médiane tous (ms) | p95 tous (ms) |
|---|---:|---:|---:|---:|---:|---:|---:|
| gemma4:e4b | 10 | 8 | 6 | 4316.9 | 7852.1 | 4740.8 | 7864.3 |
| eryx-ministral8b:q4 | 10 | 4 | 3 | 19185.0 | 33933.2 | 32624.5 | 48648.3 |

### Lecture des métriques

- `Essais` compte tous les essais, échecs compris ; aucun échec n'est retiré du dénominateur.
- Les médianes et p95 d'acceptation ne portent que sur les essais acceptés dont la durée totale est mesurée. Les colonnes « tous » incluent aussi les essais rejetés mesurés.
- Une mesure absente ou invalide reste inconnue et s'affiche « — » ; aucun zéro n'est fabriqué.
- Le p95 suit le rang le plus proche, `ceil(0.95 × n)`. Avec peu d'essais, il correspond souvent au maximum observé : ce n'est pas une estimation robuste sur plusieurs sessions.

### Audit technique et revue visuelle

L'audit technique vérifie l'acceptation par le validateur, le premier passage et les durées mesurées. Il ne dit rien de la composition, de la lisibilité du brief. La couverture géométrique du repère est contrôlée, mais sa lisibilité artistique et les relations spatiales exigent une revue visuelle humaine, essai par essai, sans note artistique automatique.

## Détail par essai

| Modèle | Cas | Essai | Statut | Premier passage | Total (ms) | Rendu (ms) | Appels | Génération mesurée (ms) | Erreur | Image |
|---|---|---:|---|---|---:|---:|---:|---:|---|---|
| gemma4:e4b | eryx_under_mass_entry | 0 | Accepté | Oui | 5079.5 | 594.6 | 1 | 3680.7 | — | [voir](model-0/eryx_under_mass_entry-0/attempt-0.png) |
| gemma4:e4b | eryx_under_mass_entry | 1 | Accepté | Oui | 4124.9 | 583.6 | 1 | 3322.7 | — | [voir](model-0/eryx_under_mass_entry-1/attempt-0.png) |
| gemma4:e4b | eryx_bank_bridge | 0 | Rejeté | Non | 7864.3 | — | 2 | 7253.2 | landmark_not_visible span: Named solid must cover at least 3 of 2048 view probes; observed=0. right_bank_main blocks 12/12 rays towards the landmark's surface. Raise or move the landmark outside that solid and clear its sightline.; Native audit/render rejected (exit 1) | [voir](model-0/eryx_bank_bridge-0/rejected-diagnostic.png) |
| gemma4:e4b | eryx_bank_bridge | 1 | Rejeté | Non | 6788.7 | — | 2 | 6353.8 | landmark_not_visible span: Named solid must cover at least 3 of 2048 view probes; observed=0. left_bank_upper blocks 4/12 rays towards the landmark's surface. Raise or move the landmark outside that solid and clear its sightline.; Native audit/render rejected (exit 1) | [voir](model-0/eryx_bank_bridge-1/rejected-diagnostic.png) |
| gemma4:e4b | eryx_distant_marker | 0 | Accepté | Non | 7852.1 | 666.3 | 2 | 6565.4 | — | [voir](model-0/eryx_distant_marker-0/attempt-1.png) |
| gemma4:e4b | eryx_distant_marker | 1 | Accepté | Non | 7318.2 | 369.0 | 2 | 6504.8 | — | [voir](model-0/eryx_distant_marker-1/attempt-1.png) |
| gemma4:e4b | eryx_blade_oblique_break | 0 | Accepté | Oui | 3799.5 | 277.4 | 1 | 3100.2 | — | [voir](model-0/eryx_blade_oblique_break-0/attempt-0.png) |
| gemma4:e4b | eryx_blade_oblique_break | 1 | Accepté | Oui | 4231.5 | 377.1 | 1 | 3667.6 | — | [voir](model-0/eryx_blade_oblique_break-1/attempt-0.png) |
| gemma4:e4b | eryx_cantilever_terrace | 0 | Accepté | Oui | 4402.2 | 474.8 | 1 | 3524.2 | — | [voir](model-0/eryx_cantilever_terrace-0/attempt-0.png) |
| gemma4:e4b | eryx_cantilever_terrace | 1 | Accepté | Oui | 3945.0 | 408.4 | 1 | 3356.9 | — | [voir](model-0/eryx_cantilever_terrace-1/attempt-0.png) |
| eryx-ministral8b:q4 | eryx_under_mass_entry | 0 | Accepté | Oui | 19309.4 | 666.1 | 1 | 18165.6 | — | [voir](model-1/eryx_under_mass_entry-0/attempt-0.png) |
| eryx-ministral8b:q4 | eryx_under_mass_entry | 1 | Accepté | Oui | 16947.9 | 774.0 | 1 | 15972.1 | — | [voir](model-1/eryx_under_mass_entry-1/attempt-0.png) |
| eryx-ministral8b:q4 | eryx_bank_bridge | 0 | Rejeté | Non | 33577.8 | — | 2 | 33106.8 | Fixed header changed or missing; copy it exactly, without Markdown; Only named box directives without emit are allowed after the header | — |
| eryx-ministral8b:q4 | eryx_bank_bridge | 1 | Rejeté | Non | 38368.9 | — | 2 | 38143.4 | Fixed header changed or missing; copy it exactly, without Markdown; Only named box directives without emit are allowed after the header | — |
| eryx-ministral8b:q4 | eryx_distant_marker | 0 | Rejeté | Non | 48648.3 | — | 2 | 47516.8 | Expected 8..20 boxes, observed 25 body lines | [voir](model-1/eryx_distant_marker-0/rejected-diagnostic.png) |
| eryx-ministral8b:q4 | eryx_distant_marker | 1 | Rejeté | Non | 42374.1 | — | 2 | 41381.5 | landmark_not_visible far_marker: Named solid must cover at least 3 of 2048 view probes; observed=0. Surface sightlines are clear; move the landmark inside the fixed camera frame or enlarge it.; Native audit/render rejected (exit 1) | [voir](model-1/eryx_distant_marker-1/rejected-diagnostic.png) |
| eryx-ministral8b:q4 | eryx_blade_oblique_break | 0 | Accepté | Non | 33933.2 | 263.2 | 2 | 33025.4 | — | [voir](model-1/eryx_blade_oblique_break-0/attempt-1.png) |
| eryx-ministral8b:q4 | eryx_blade_oblique_break | 1 | Accepté | Oui | 19060.6 | 344.2 | 1 | 18434.2 | — | [voir](model-1/eryx_blade_oblique_break-1/attempt-0.png) |
| eryx-ministral8b:q4 | eryx_cantilever_terrace | 0 | Rejeté | Non | 27684.0 | — | 2 | 26696.3 | landmark_not_visible ledge: Named solid must cover at least 3 of 2048 view probes; observed=2. quarry_wall_back blocks 6/12 rays towards the landmark's surface. Raise or move the landmark outside that solid and clear its sightline.; Native audit/render rejected (exit 1) | [voir](model-1/eryx_cantilever_terrace-0/rejected-diagnostic.png) |
| eryx-ministral8b:q4 | eryx_cantilever_terrace | 1 | Rejeté | Non | 31671.2 | — | 2 | 30887.0 | landmark_not_visible ledge: Named solid must cover at least 3 of 2048 view probes; observed=2. support_post_2 blocks 1/12 rays towards the landmark's surface. Raise or move the landmark outside that solid and clear its sightline.; Native audit/render rejected (exit 1) | [voir](model-1/eryx_cantilever_terrace-1/rejected-diagnostic.png) |

La colonne « Génération mesurée » additionne les appels seulement si tous sont mesurés ; « — » signale une mesure absente, jamais un zéro. Les échecs restent visibles dans ce tableau avec leur erreur éventuelle.

## Mesures par appel

| Modèle | Cas | Essai | Appel | Chargement ms | Prompt ms | Génération ms | Tokens | Tokens/s |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| gemma4:e4b | eryx_under_mass_entry | 0 | 1 | 4.3 | 385.3 | 3680.7 | 565.0 | 153.5 |
| gemma4:e4b | eryx_under_mass_entry | 1 | 1 | 4.4 | 99.5 | 3322.7 | 528.0 | 158.9 |
| gemma4:e4b | eryx_bank_bridge | 0 | 1 | 4.4 | 255.6 | 3786.1 | 598.0 | 157.9 |
| gemma4:e4b | eryx_bank_bridge | 0 | 2 | 5.3 | 83.5 | 3467.1 | 598.0 | 172.5 |
| gemma4:e4b | eryx_bank_bridge | 1 | 1 | 4.9 | 66.2 | 3340.4 | 522.0 | 156.3 |
| gemma4:e4b | eryx_bank_bridge | 1 | 2 | 4.7 | 82.7 | 3013.4 | 522.0 | 173.2 |
| gemma4:e4b | eryx_distant_marker | 0 | 1 | 4.1 | 259.7 | 3401.2 | 539.0 | 158.5 |
| gemma4:e4b | eryx_distant_marker | 0 | 2 | 4.1 | 79.4 | 3164.2 | 539.0 | 170.3 |
| gemma4:e4b | eryx_distant_marker | 1 | 1 | 4.1 | 72.5 | 3408.5 | 537.0 | 157.5 |
| gemma4:e4b | eryx_distant_marker | 1 | 2 | 5.1 | 81.3 | 3096.3 | 531.0 | 171.5 |
| gemma4:e4b | eryx_blade_oblique_break | 0 | 1 | 4.1 | 260.3 | 3100.2 | 520.0 | 167.7 |
| gemma4:e4b | eryx_blade_oblique_break | 1 | 1 | 4.4 | 64.4 | 3667.6 | 594.0 | 162.0 |
| gemma4:e4b | eryx_cantilever_terrace | 0 | 1 | 4.7 | 253.5 | 3524.2 | 546.0 | 154.9 |
| gemma4:e4b | eryx_cantilever_terrace | 1 | 1 | 4.2 | 64.9 | 3356.9 | 511.0 | 152.2 |
| eryx-ministral8b:q4 | eryx_under_mass_entry | 0 | 1 | 3.9 | 320.3 | 18165.6 | 803.0 | 44.2 |
| eryx-ministral8b:q4 | eryx_under_mass_entry | 1 | 1 | 3.7 | 23.7 | 15972.1 | 710.0 | 44.5 |
| eryx-ministral8b:q4 | eryx_bank_bridge | 0 | 1 | 4.2 | 310.7 | 20857.4 | 924.0 | 44.3 |
| eryx-ministral8b:q4 | eryx_bank_bridge | 0 | 2 | 3.7 | 47.6 | 12249.4 | 532.0 | 43.4 |
| eryx-ministral8b:q4 | eryx_bank_bridge | 1 | 1 | 3.8 | 23.6 | 25448.2 | 1122.0 | 44.1 |
| eryx-ministral8b:q4 | eryx_bank_bridge | 1 | 2 | 3.9 | 47.7 | 12695.2 | 548.0 | 43.2 |
| eryx-ministral8b:q4 | eryx_distant_marker | 0 | 1 | 3.6 | 312.5 | 21233.0 | 940.0 | 44.3 |
| eryx-ministral8b:q4 | eryx_distant_marker | 0 | 2 | 3.8 | 488.8 | 26283.7 | 1129.0 | 43.0 |
| eryx-ministral8b:q4 | eryx_distant_marker | 1 | 1 | 3.4 | 23.5 | 27109.0 | 1192.0 | 44.0 |
| eryx-ministral8b:q4 | eryx_distant_marker | 1 | 2 | 3.7 | 580.6 | 14272.5 | 615.0 | 43.1 |
| eryx-ministral8b:q4 | eryx_blade_oblique_break | 0 | 1 | 3.4 | 312.0 | 20585.7 | 912.0 | 44.3 |
| eryx-ministral8b:q4 | eryx_blade_oblique_break | 0 | 2 | 3.7 | 36.5 | 12439.7 | 540.0 | 43.4 |
| eryx-ministral8b:q4 | eryx_blade_oblique_break | 1 | 1 | 4.2 | 23.4 | 18434.2 | 816.0 | 44.3 |
| eryx-ministral8b:q4 | eryx_cantilever_terrace | 0 | 1 | 3.7 | 310.4 | 12581.2 | 559.0 | 44.4 |
| eryx-ministral8b:q4 | eryx_cantilever_terrace | 0 | 2 | 3.7 | 291.2 | 14115.2 | 617.0 | 43.7 |
| eryx-ministral8b:q4 | eryx_cantilever_terrace | 1 | 1 | 3.6 | 24.2 | 14363.7 | 634.0 | 44.1 |
| eryx-ministral8b:q4 | eryx_cantilever_terrace | 1 | 2 | 3.9 | 345.2 | 16523.3 | 705.0 | 42.7 |

