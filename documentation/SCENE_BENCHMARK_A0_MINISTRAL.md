# Scene Generation Benchmark

Genere le 2026-10-24 09:09:32.

Ce document audite la generation directe de `.scene` par `Ministral` a partir de briefs spatiaux fixes.

Pipeline :
- le moteur fabrique le prompt exact via `--dump-scene-audit-prompt`
- `llama-cli` genere une reponse brute
- la reponse est normalisee en `.scene`
- le renderer audite le fichier et produit une image si la scene est valide

Parametres :
- rendu : `800x400`, `8` spp, `3` bounces, `1` direct samples
- LLM : `temperature=0.0`, `n_predict=1024`, `ctx=4096`, `seed=42`

Resultat global : `10/10` scenes valides, `0` invalides.

| Case | Status | Gen ms | Audit ms | Triangles | Materials |
| --- | --- | ---: | ---: | ---: | ---: |
| `entry_gate_dusk` | `valid` | 23412.85 | 649.47 | 666 | 57 |
| `sally_port_checkpoint` | `valid` | 21281.99 | 727.39 | 582 | 49 |
| `badge_vestibule` | `valid` | 19530.34 | 801.36 | 508 | 44 |
| `server_aisles_dense` | `valid` | 22546.85 | 992.97 | 568 | 49 |
| `control_hub` | `valid` | 18349.54 | 687.66 | 866 | 43 |
| `backup_vault` | `valid` | 26176.58 | 1504.61 | 1332 | 67 |
| `cooling_exchange_bay` | `valid` | 23910.12 | 989.18 | 536 | 46 |
| `loading_dock_dust` | `valid` | 26487.22 | 700.48 | 992 | 57 |
| `roof_watch_dusk` | `valid` | 22349.74 | 543.83 | 974 | 52 |
| `stairhead_parapet` | `valid` | 22439.68 | 705.70 | 590 | 51 |

## Entry Gate

A concrete gate court faces the datacenter threshold. Dust clings to the bars and the badge reader blinks beside a service intercom.

Status : `valid`
Slug : `entry_gate_dusk`
Generation : `23412.85 ms`
Audit + render : `649.47 ms`
Scene lines : `15`
Scene chars : `1226`
Triangles : `666`
Materials : `57`

State : `generated/scene_model_comparison/A0-ministral/states/entry_gate_dusk.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/entry_gate_dusk.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/entry_gate_dusk.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/entry_gate_dusk.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/entry_gate_dusk.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/entry_gate_dusk.audit.stderr.txt`

Spatial brief :
- location_id: `gate`
- location_archetype: `threshold courtyard`
- time_of_day: `dusk`
- visibility_level: `clear`
- desert_state: `still`
- interior_density: `sparse`
- alert_level: `1`
- anchors: `gate frame, perimeter wall, badge reader, intercom plate`
- visible_objects: `badge reader, service intercom, warning placard, padlocked crate`
- blocked_exits: `west, south`
- spatial_anomalies: `(none)`

![entry_gate_dusk](generated/scene_model_comparison/A0-ministral/images/entry_gate_dusk.png)

## Sally Port Checkpoint

A narrow checkpoint sits between two heavy gates. A code keypad, a dead camera head, and a steel cabinet make the space feel procedural and exposed.

Status : `valid`
Slug : `sally_port_checkpoint`
Generation : `21281.99 ms`
Audit + render : `727.39 ms`
Scene lines : `15`
Scene chars : `1241`
Triangles : `582`
Materials : `49`

State : `generated/scene_model_comparison/A0-ministral/states/sally_port_checkpoint.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/sally_port_checkpoint.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/sally_port_checkpoint.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/sally_port_checkpoint.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/sally_port_checkpoint.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/sally_port_checkpoint.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `security checkpoint`
- time_of_day: `night`
- visibility_level: `low`
- desert_state: `windy`
- interior_density: `sparse`
- alert_level: `2`
- anchors: `inner gate, outer gate, checkpoint alcove`
- visible_objects: `code keypad, steel cabinet, dead camera head, inspection hatch`
- blocked_exits: `south`
- spatial_anomalies: `camera loop stuck on static`

![sally_port_checkpoint](generated/scene_model_comparison/A0-ministral/images/sally_port_checkpoint.png)

## Badge Vestibule

A bare vestibule buffers the exterior from the datacenter core. The only relief is a reader column, a maintenance hatch, and a wall placard with faded rules.

Status : `valid`
Slug : `badge_vestibule`
Generation : `19530.34 ms`
Audit + render : `801.36 ms`
Scene lines : `13`
Scene chars : `1071`
Triangles : `508`
Materials : `44`

State : `generated/scene_model_comparison/A0-ministral/states/badge_vestibule.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/badge_vestibule.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/badge_vestibule.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/badge_vestibule.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/badge_vestibule.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/badge_vestibule.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `airlock vestibule`
- time_of_day: `night`
- visibility_level: `clear`
- desert_state: `still`
- interior_density: `sparse`
- alert_level: `1`
- anchors: `reader column, inner door, outer door`
- visible_objects: `badge reader, maintenance hatch, wall placard, ceiling speaker`
- blocked_exits: `west`
- spatial_anomalies: `(none)`

![badge_vestibule](generated/scene_model_comparison/A0-ministral/images/badge_vestibule.png)

## Primary Server Aisles

Dense aisles of racks run into darkness. A service cart, a floor hatch, and a side console break the repetition under red LEDs.

Status : `valid`
Slug : `server_aisles_dense`
Generation : `22546.85 ms`
Audit + render : `992.97 ms`
Scene lines : `15`
Scene chars : `1287`
Triangles : `568`
Materials : `49`

State : `generated/scene_model_comparison/A0-ministral/states/server_aisles_dense.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/server_aisles_dense.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/server_aisles_dense.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/server_aisles_dense.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/server_aisles_dense.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/server_aisles_dense.audit.stderr.txt`

Spatial brief :
- location_id: `server_aisles`
- location_archetype: `server aisles`
- time_of_day: `night`
- visibility_level: `low`
- desert_state: `still`
- interior_density: `dense`
- alert_level: `2`
- anchors: `rack corridor, service lane, ceiling cable tray`
- visible_objects: `service cart, floor hatch, side console, sealed lockbox`
- blocked_exits: `south`
- spatial_anomalies: `cooling hum pulses unevenly`

![server_aisles_dense](generated/scene_model_comparison/A0-ministral/images/server_aisles_dense.png)

## Sector Control Hub

A compact control room opens off the aisles. One main console dominates the center while a locked cabinet and an access hatch wait at the margins.

Status : `valid`
Slug : `control_hub`
Generation : `18349.54 ms`
Audit + render : `687.66 ms`
Scene lines : `12`
Scene chars : `1015`
Triangles : `866`
Materials : `43`

State : `generated/scene_model_comparison/A0-ministral/states/control_hub.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/control_hub.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/control_hub.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/control_hub.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/control_hub.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/control_hub.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `control room`
- time_of_day: `night`
- visibility_level: `clear`
- desert_state: `still`
- interior_density: `sparse`
- alert_level: `2`
- anchors: `main console, wall cabinet, floor hatch`
- visible_objects: `main console, locked cabinet, floor hatch, status placard`
- blocked_exits: `east, south`
- spatial_anomalies: `(none)`

![control_hub](generated/scene_model_comparison/A0-ministral/images/control_hub.png)

## Backup Vault

A chilled storage vault holds sealed pods and backup racks. A biometric pad, a wheeled crate, and a suspended service panel suggest expensive access.

Status : `valid`
Slug : `backup_vault`
Generation : `26176.58 ms`
Audit + render : `1504.61 ms`
Scene lines : `18`
Scene chars : `1566`
Triangles : `1332`
Materials : `67`

State : `generated/scene_model_comparison/A0-ministral/states/backup_vault.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/backup_vault.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/backup_vault.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/backup_vault.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/backup_vault.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/backup_vault.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `backup vault`
- time_of_day: `night`
- visibility_level: `low`
- desert_state: `still`
- interior_density: `dense`
- alert_level: `3`
- anchors: `sealed data pods, vault door, service panel`
- visible_objects: `biometric pad, wheeled crate, service panel, emergency switch`
- blocked_exits: `north, west`
- spatial_anomalies: `cold vapor drifts near the floor`

![backup_vault](generated/scene_model_comparison/A0-ministral/images/backup_vault.png)

## Cooling Exchange Bay

Tall cooling blocks and pipe runs crowd an industrial chamber. A valve wheel, a drip tray, and a maintenance console provide the room's few obvious handles.

Status : `valid`
Slug : `cooling_exchange_bay`
Generation : `23910.12 ms`
Audit + render : `989.18 ms`
Scene lines : `17`
Scene chars : `1387`
Triangles : `536`
Materials : `46`

State : `generated/scene_model_comparison/A0-ministral/states/cooling_exchange_bay.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/cooling_exchange_bay.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/cooling_exchange_bay.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/cooling_exchange_bay.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/cooling_exchange_bay.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/cooling_exchange_bay.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `cooling plant bay`
- time_of_day: `night`
- visibility_level: `dusty`
- desert_state: `still`
- interior_density: `dense`
- alert_level: `2`
- anchors: `cooling blocks, pipe gallery, drain trench`
- visible_objects: `valve wheel, maintenance console, drip tray, tool locker`
- blocked_exits: `west`
- spatial_anomalies: `one pipe bank rattles under strain`

![cooling_exchange_bay](generated/scene_model_comparison/A0-ministral/images/cooling_exchange_bay.png)

## Dust Loading Dock

A side loading bay opens toward the desert through a half-raised shutter. Pallets, a cargo crate, and a bent lamp post stage a tense threshold between inside and outside.

Status : `valid`
Slug : `loading_dock_dust`
Generation : `26487.22 ms`
Audit + render : `700.48 ms`
Scene lines : `19`
Scene chars : `1575`
Triangles : `992`
Materials : `57`

State : `generated/scene_model_comparison/A0-ministral/states/loading_dock_dust.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/loading_dock_dust.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/loading_dock_dust.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/loading_dock_dust.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/loading_dock_dust.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/loading_dock_dust.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `loading dock`
- time_of_day: `dusk`
- visibility_level: `dusty`
- desert_state: `dusty`
- interior_density: `sparse`
- alert_level: `2`
- anchors: `half-raised shutter, dock lip, desert threshold`
- visible_objects: `cargo crate, shipping placard, control switch, lamp post`
- blocked_exits: `north`
- spatial_anomalies: `dust slips under the shutter in thin sheets`

![loading_dock_dust](generated/scene_model_comparison/A0-ministral/images/loading_dock_dust.png)

## Roof Watch

The datacenter roof becomes a parapet walk above the desert. A beacon mast, a hatch door, and a field scope hold the line against the horizon.

Status : `valid`
Slug : `roof_watch_dusk`
Generation : `22349.74 ms`
Audit + render : `543.83 ms`
Scene lines : `16`
Scene chars : `1292`
Triangles : `974`
Materials : `52`

State : `generated/scene_model_comparison/A0-ministral/states/roof_watch_dusk.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/roof_watch_dusk.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/roof_watch_dusk.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/roof_watch_dusk.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/roof_watch_dusk.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/roof_watch_dusk.audit.stderr.txt`

Spatial brief :
- location_id: `roof_watch`
- location_archetype: `roof parapet`
- time_of_day: `dusk`
- visibility_level: `clear`
- desert_state: `windy`
- interior_density: `sparse`
- alert_level: `1`
- anchors: `parapet, horizon, beacon mast, roof hatch`
- visible_objects: `roof hatch, field scope, beacon mast switch, warning sign`
- blocked_exits: `south`
- spatial_anomalies: `(none)`

![roof_watch_dusk](generated/scene_model_comparison/A0-ministral/images/roof_watch_dusk.png)

## Parapet Stairhead

A concrete stairhead interrupts the roof line. The landing is tight, with a latch box, a cable spool, and a narrow gate looking over the darkening desert.

Status : `valid`
Slug : `stairhead_parapet`
Generation : `22439.68 ms`
Audit + render : `705.70 ms`
Scene lines : `16`
Scene chars : `1309`
Triangles : `590`
Materials : `51`

State : `generated/scene_model_comparison/A0-ministral/states/stairhead_parapet.session.json`
Prompt : `generated/scene_model_comparison/A0-ministral/prompts/stairhead_parapet.prompt.txt`
Raw response : `generated/scene_model_comparison/A0-ministral/responses/stairhead_parapet.raw.txt`
Scene : `generated/scene_model_comparison/A0-ministral/scenes/stairhead_parapet.scene`
Audit stdout : `generated/scene_model_comparison/A0-ministral/logs/stairhead_parapet.audit.stdout.txt`
Audit stderr : `generated/scene_model_comparison/A0-ministral/logs/stairhead_parapet.audit.stderr.txt`

Spatial brief :
- location_id: `unknown`
- location_archetype: `roof stairhead`
- time_of_day: `dusk`
- visibility_level: `low`
- desert_state: `windy`
- interior_density: `sparse`
- alert_level: `1`
- anchors: `stairhead bulk, roof gate, parapet edge`
- visible_objects: `latch box, cable spool, roof gate handle, inspection placard`
- blocked_exits: `east, south`
- spatial_anomalies: `wind makes the gate vibrate softly`

![stairhead_parapet](generated/scene_model_comparison/A0-ministral/images/stairhead_parapet.png)
