# Comparaison Ministral / Gemma — génération directe de scènes

Première exécution : 2026-10-24T09:20:13. 10 briefs identiques par variante ; détails des profils dans les preuves.

La validité syntaxique correspond à l’acceptation par le renderer. Le contrôle de caméra rejette une base dégénérée ; il ne mesure ni la composition artistique, ni la fidélité au brief.

| Variante | Syntaxe | Caméra non dégénérée et syntaxe valide | Génération médiane | p95 empirique |
|---|---:|---:|---:|---:|
| B-gemma | 10/10 | 2/10 | 17.01 s | 19.70 s |
| D-gemma-guided-full | 10/10 | 10/10 | 16.15 s | 20.44 s |

Le temps de génération inclut le chargement et le processus CLI, mais exclut le hachage initial, la création du prompt et le rendu. Avec dix observations, le p95 par rang correspond au maximum ; ce n’est pas une estimation sur plusieurs sessions.

Les journaux détaillés renseignent les tokens, la fin EOS, le traitement du prompt, le décodage et les allocations à la fermeture. Ces allocations ne sont pas un pic RAM/VRAM. Le premier texte utile n’est pas chronométré.

## Détail et images

### B-gemma

Modèle : `gemma-4-E4B-it-Q4_K_M.gguf`. Réflexion : `off`, budget `0`.

Prompts utilisateur différents de la première variante : 0.

[Résultats bruts](generated/scene_model_comparison/B-gemma/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 15.04 s | [voir](generated/scene_model_comparison/B-gemma/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 14.96 s | [voir](generated/scene_model_comparison/B-gemma/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 17.51 s | [voir](generated/scene_model_comparison/B-gemma/images/badge_vestibule.png) |
| server_aisles_dense | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 19.70 s | [voir](generated/scene_model_comparison/B-gemma/images/server_aisles_dense.png) |
| control_hub | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 14.17 s | [voir](generated/scene_model_comparison/B-gemma/images/control_hub.png) |
| backup_vault | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 17.29 s | [voir](generated/scene_model_comparison/B-gemma/images/backup_vault.png) |
| cooling_exchange_bay | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 16.72 s | [voir](generated/scene_model_comparison/B-gemma/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 14.57 s | [voir](generated/scene_model_comparison/B-gemma/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 18.58 s | [voir](generated/scene_model_comparison/B-gemma/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | camera view direction is parallel to its up vector (normalized cross product 0 <= 1e-4) | 17.37 s | [voir](generated/scene_model_comparison/B-gemma/images/stairhead_parapet.png) |

### D-gemma-guided-full

Modèle : `gemma-4-E4B-it-Q4_K_M.gguf`. Réflexion : `off`, budget `0`.

Prompts utilisateur différents de la première variante : 10.

[Résultats bruts](generated/scene_model_comparison/D-gemma-guided-full/results.json)

| Cas | Statut initial | Contrôle caméra | Génération | Image |
|---|---|---|---:|---|
| entry_gate_dusk | valid | OK | 14.27 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/entry_gate_dusk.png) |
| sally_port_checkpoint | valid | OK | 15.51 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/sally_port_checkpoint.png) |
| badge_vestibule | valid | OK | 14.95 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/badge_vestibule.png) |
| server_aisles_dense | valid | OK | 20.44 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/server_aisles_dense.png) |
| control_hub | valid | OK | 16.95 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/control_hub.png) |
| backup_vault | valid | OK | 16.63 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/backup_vault.png) |
| cooling_exchange_bay | valid | OK | 18.50 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/cooling_exchange_bay.png) |
| loading_dock_dust | valid | OK | 15.67 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/loading_dock_dust.png) |
| roof_watch_dusk | valid | OK | 18.23 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/roof_watch_dusk.png) |
| stairhead_parapet | valid | OK | 15.31 s | [voir](generated/scene_model_comparison/D-gemma-guided-full/images/stairhead_parapet.png) |

## Premier cas : comparaison visuelle

### B-gemma

![B-gemma](generated/scene_model_comparison/B-gemma/images/entry_gate_dusk.png)

### D-gemma-guided-full

![D-gemma-guided-full](generated/scene_model_comparison/D-gemma-guided-full/images/entry_gate_dusk.png)
