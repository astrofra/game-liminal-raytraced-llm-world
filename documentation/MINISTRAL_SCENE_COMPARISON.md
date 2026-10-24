# Comparaison des modèles — génération directe de scènes

Première exécution : 2026-10-24T10:41:39. 10 briefs identiques par variante ; détails des profils dans les preuves.

La validité syntaxique correspond à l’acceptation par le renderer. Le contrôle de caméra rejette une base dégénérée ; il ne mesure ni la composition artistique, ni la fidélité au brief.

| Variante | Syntaxe | Caméra non dégénérée et syntaxe valide | Génération médiane | p95 empirique |
|---|---:|---:|---:|---:|
| scene-A-old-cli-q4 | 10/10 | 10/10 | 21.95 s | 25.62 s |
| scene-B-v070-q4 | 10/10 | 10/10 | 22.19 s | 25.72 s |
| scene-C-v070-q5 | 8/10 | 8/10 | 25.97 s | 30.23 s |
| scene-D-v070-reasoning | 8/10 | 8/10 | 19.97 s | 22.02 s |
| scene-DP-v070-reasoning | 8/10 | 8/10 | 20.27 s | 28.52 s |

Le temps de génération inclut le chargement et le processus CLI, mais exclut le hachage initial, la création du prompt et le rendu. Avec dix observations, le p95 par rang correspond au maximum ; ce n’est pas une estimation sur plusieurs sessions.

Les journaux détaillés renseignent les tokens, la fin EOS, le traitement du prompt, le décodage et les allocations à la fermeture. Ces allocations ne sont pas un pic RAM/VRAM. Le premier texte utile n’est pas chronométré.

## Détail et images

### scene-A-old-cli-q4

Modèle : `Ministral-3-8B-Instruct-2512-Q4_K_M.gguf`. Réflexion : `off`, budget `0`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-A-old-cli-q4/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 22.53 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 21.26 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 18.93 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/badge_vestibule.png) |
| server_aisles_dense | valid | OK | 24.73 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/server_aisles_dense.png) |
| control_hub | valid | OK | 18.13 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/control_hub.png) |
| backup_vault | valid | OK | 25.62 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 22.97 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 22.80 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 21.34 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 21.37 s | [voir](generated/ministral_qualification/scene-A-old-cli-q4/images/stairhead_parapet.png) |

### scene-B-v070-q4

Modèle : `Ministral-3-8B-Instruct-2512-Q4_K_M.gguf`. Réflexion : `off`, budget `0`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-B-v070-q4/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 22.84 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 21.53 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 19.27 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/badge_vestibule.png) |
| server_aisles_dense | valid | OK | 24.88 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/server_aisles_dense.png) |
| control_hub | valid | OK | 18.21 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/control_hub.png) |
| backup_vault | valid | OK | 25.72 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 23.11 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 22.91 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 21.47 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 21.52 s | [voir](generated/ministral_qualification/scene-B-v070-q4/images/stairhead_parapet.png) |

### scene-C-v070-q5

Modèle : `Ministral-3-8B-Instruct-2512-Q5_K_M.gguf`. Réflexion : `off`, budget `0`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-C-v070-q5/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 25.78 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 23.22 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/sally_port_checkpoint.png) |
| badge_vestibule | invalid_scene | OK | 23.09 s | — |
| server_aisles_dense | valid | OK | 27.32 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/server_aisles_dense.png) |
| control_hub | invalid_scene | OK | 23.98 s | — |
| backup_vault | valid | OK | 29.79 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 30.23 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 27.29 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 26.15 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 24.60 s | [voir](generated/ministral_qualification/scene-C-v070-q5/images/stairhead_parapet.png) |

### scene-D-v070-reasoning

Modèle : `Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf`. Réflexion : `on`, budget `512`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-D-v070-reasoning/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 17.34 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 19.34 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 17.61 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/badge_vestibule.png) |
| server_aisles_dense | valid | OK | 18.60 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/server_aisles_dense.png) |
| control_hub | invalid_scene | OK | 20.93 s | — |
| backup_vault | valid | OK | 20.84 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 20.59 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 22.02 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/loading_dock_dust.png) |
| roof_watch_dusk | invalid_scene | OK | 17.81 s | — |
| stairhead_parapet | valid | OK | 20.77 s | [voir](generated/ministral_qualification/scene-D-v070-reasoning/images/stairhead_parapet.png) |

### scene-DP-v070-reasoning

Modèle : `Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf`. Réflexion : `on`, budget `512`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-DP-v070-reasoning/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 17.22 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 20.37 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 20.42 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/badge_vestibule.png) |
| server_aisles_dense | invalid_scene | OK | 28.52 s | — |
| control_hub | valid | OK | 19.01 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/control_hub.png) |
| backup_vault | valid | OK | 20.17 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 24.71 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/cooling_exchange_bay.png) |
| loading_dock_dust | invalid_scene | OK | 20.87 s | — |
| roof_watch_dusk | valid | OK | 17.35 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 18.28 s | [voir](generated/ministral_qualification/scene-DP-v070-reasoning/images/stairhead_parapet.png) |

## Premier cas : comparaison visuelle

### scene-A-old-cli-q4

![scene-A-old-cli-q4](generated/ministral_qualification/scene-A-old-cli-q4/images/entry_gate_dusk.png)

### scene-B-v070-q4

![scene-B-v070-q4](generated/ministral_qualification/scene-B-v070-q4/images/entry_gate_dusk.png)

### scene-C-v070-q5

![scene-C-v070-q5](generated/ministral_qualification/scene-C-v070-q5/images/entry_gate_dusk.png)

### scene-D-v070-reasoning

![scene-D-v070-reasoning](generated/ministral_qualification/scene-D-v070-reasoning/images/entry_gate_dusk.png)

### scene-DP-v070-reasoning

![scene-DP-v070-reasoning](generated/ministral_qualification/scene-DP-v070-reasoning/images/entry_gate_dusk.png)
