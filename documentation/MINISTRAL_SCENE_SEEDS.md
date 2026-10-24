# Comparaison des modèles — génération directe de scènes

Première exécution : 2026-10-24T11:09:07. 10 briefs identiques par variante ; détails des profils dans les preuves.

La validité syntaxique correspond à l’acceptation par le renderer. Le contrôle de caméra rejette une base dégénérée ; il ne mesure ni la composition artistique, ni la fidélité au brief.

| Variante | Syntaxe | Caméra non dégénérée et syntaxe valide | Génération médiane | p95 empirique |
|---|---:|---:|---:|---:|
| scene-DP-v070-reasoning | 8/10 | 8/10 | 20.27 s | 28.52 s |
| scene-DP-seed43 | 10/10 | 10/10 | 18.47 s | 22.11 s |
| scene-DP-seed44 | 10/10 | 10/10 | 20.31 s | 25.46 s |

Le temps de génération inclut le chargement et le processus CLI, mais exclut le hachage initial, la création du prompt et le rendu. Avec dix observations, le p95 par rang correspond au maximum ; ce n’est pas une estimation sur plusieurs sessions.

Les journaux détaillés renseignent les tokens, la fin EOS, le traitement du prompt, le décodage et les allocations à la fermeture. Ces allocations ne sont pas un pic RAM/VRAM. Le premier texte utile n’est pas chronométré.

## Détail et images

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

### scene-DP-seed43

Modèle : `Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf`. Réflexion : `on`, budget `512`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-DP-seed43/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 18.42 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 17.54 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 18.87 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/badge_vestibule.png) |
| server_aisles_dense | valid | OK | 18.51 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/server_aisles_dense.png) |
| control_hub | valid | OK | 15.45 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/control_hub.png) |
| backup_vault | valid | OK | 20.87 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 22.11 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 17.94 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 18.58 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 18.19 s | [voir](generated/ministral_qualification/scene-DP-seed43/images/stairhead_parapet.png) |

### scene-DP-seed44

Modèle : `Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf`. Réflexion : `on`, budget `512`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/ministral_qualification/scene-DP-seed44/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 18.52 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 19.64 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 22.03 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/badge_vestibule.png) |
| server_aisles_dense | valid | OK | 25.46 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/server_aisles_dense.png) |
| control_hub | valid | OK | 17.27 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/control_hub.png) |
| backup_vault | valid | OK | 24.26 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 22.65 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 20.98 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 17.10 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 18.35 s | [voir](generated/ministral_qualification/scene-DP-seed44/images/stairhead_parapet.png) |

## Premier cas : comparaison visuelle

### scene-DP-v070-reasoning

![scene-DP-v070-reasoning](generated/ministral_qualification/scene-DP-v070-reasoning/images/entry_gate_dusk.png)

### scene-DP-seed43

![scene-DP-seed43](generated/ministral_qualification/scene-DP-seed43/images/entry_gate_dusk.png)

### scene-DP-seed44

![scene-DP-seed44](generated/ministral_qualification/scene-DP-seed44/images/entry_gate_dusk.png)
