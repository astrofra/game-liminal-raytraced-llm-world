# Scene Generation Benchmark

Genere le 2026-10-24 11:09:07.

Generation directe de `.scene` ; profil `ministral-8b-reasoning-q4`, modele `Ministral-3-8B-Reasoning-2512-Q4_K_M.gguf`.

Pipeline :
- le moteur fabrique le prompt exact via `--dump-scene-audit-prompt`
- `llama-cli` genere une reponse brute
- la reponse est normalisee en `.scene`
- le renderer audite le fichier et produit une image si la scene est valide

Parametres :
- rendu : `800x400`, `8` spp, `3` bounces, `1` direct samples
- LLM : `temperature=0.7`, `n_predict=1024`, `ctx=4096`, `seed=42`
- reflexion : `on`, budget `512` ; top_k `40`, top_p `0.95`
- seul le canal final termine par EOS est audite ; les sorties tronquees sont refusees

Resultat global : `8/10` scenes avec syntaxe et camera valides, `2` invalides. Syntaxe seule : `8/10`.

| Case | Status | Gen ms | Audit ms | Triangles | Materials |
| --- | --- | ---: | ---: | ---: | ---: |
| `entry_gate_dusk` | `valid` | 17222.87 | 566.74 | 454 | 38 |
| `sally_port_checkpoint` | `valid` | 20374.50 | 718.57 | 838 | 65 |
| `badge_vestibule` | `valid` | 20421.15 | 765.03 | 424 | 37 |
| `server_aisles_dense` | `invalid_scene` | 28516.32 | 87.48 | 0 | 0 |
| `control_hub` | `valid` | 19007.98 | 923.01 | 786 | 36 |
| `backup_vault` | `valid` | 20168.30 | 796.15 | 1066 | 54 |
| `cooling_exchange_bay` | `valid` | 24710.15 | 850.06 | 550 | 46 |
| `loading_dock_dust` | `invalid_scene` | 20874.40 | 88.14 | 0 | 0 |
| `roof_watch_dusk` | `valid` | 17353.68 | 238.86 | 366 | 32 |
| `stairhead_parapet` | `valid` | 18276.98 | 500.19 | 450 | 39 |

## Entry Gate

A concrete gate court faces the datacenter threshold. Dust clings to the bars and the badge reader blinks beside a service intercom.

Status : `valid`
Slug : `entry_gate_dusk`
Generation : `17222.87 ms`
Audit + render : `566.74 ms`
Scene lines : `11`
Scene chars : `908`
Triangles : `454`
Materials : `38`

State : `states/entry_gate_dusk.session.json`
Prompt : `prompts/entry_gate_dusk.prompt.txt`
Raw response : `responses/entry_gate_dusk.raw.txt`
Final response : `responses/entry_gate_dusk.final.txt`
Stop reason : `eos`
Scene : `scenes/entry_gate_dusk.scene`
Audit stdout : `logs/entry_gate_dusk.audit.stdout.txt`
Audit stderr : `logs/entry_gate_dusk.audit.stderr.txt`

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

![entry_gate_dusk](images/entry_gate_dusk.png)

## Sally Port Checkpoint

A narrow checkpoint sits between two heavy gates. A code keypad, a dead camera head, and a steel cabinet make the space feel procedural and exposed.

Status : `valid`
Slug : `sally_port_checkpoint`
Generation : `20374.50 ms`
Audit + render : `718.57 ms`
Scene lines : `14`
Scene chars : `1136`
Triangles : `838`
Materials : `65`

State : `states/sally_port_checkpoint.session.json`
Prompt : `prompts/sally_port_checkpoint.prompt.txt`
Raw response : `responses/sally_port_checkpoint.raw.txt`
Final response : `responses/sally_port_checkpoint.final.txt`
Stop reason : `eos`
Scene : `scenes/sally_port_checkpoint.scene`
Audit stdout : `logs/sally_port_checkpoint.audit.stdout.txt`
Audit stderr : `logs/sally_port_checkpoint.audit.stderr.txt`

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

![sally_port_checkpoint](images/sally_port_checkpoint.png)

## Badge Vestibule

A bare vestibule buffers the exterior from the datacenter core. The only relief is a reader column, a maintenance hatch, and a wall placard with faded rules.

Status : `valid`
Slug : `badge_vestibule`
Generation : `20421.15 ms`
Audit + render : `765.03 ms`
Scene lines : `13`
Scene chars : `1065`
Triangles : `424`
Materials : `37`

State : `states/badge_vestibule.session.json`
Prompt : `prompts/badge_vestibule.prompt.txt`
Raw response : `responses/badge_vestibule.raw.txt`
Final response : `responses/badge_vestibule.final.txt`
Stop reason : `eos`
Scene : `scenes/badge_vestibule.scene`
Audit stdout : `logs/badge_vestibule.audit.stdout.txt`
Audit stderr : `logs/badge_vestibule.audit.stderr.txt`

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

![badge_vestibule](images/badge_vestibule.png)

## Primary Server Aisles

Dense aisles of racks run into darkness. A service cart, a floor hatch, and a side console break the repetition under red LEDs.

Status : `invalid_scene`
Slug : `server_aisles_dense`
Generation : `28516.32 ms`
Audit + render : `87.48 ms`
Scene lines : `21`
Scene chars : `1699`
Triangles : `0`
Materials : `0`

State : `states/server_aisles_dense.session.json`
Prompt : `prompts/server_aisles_dense.prompt.txt`
Raw response : `responses/server_aisles_dense.raw.txt`
Final response : `responses/server_aisles_dense.final.txt`
Stop reason : `eos`
Scene : `scenes/server_aisles_dense.scene`
Audit stdout : `logs/server_aisles_dense.audit.stdout.txt`
Audit stderr : `logs/server_aisles_dense.audit.stderr.txt`

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

Erreur :

```text
C:\works\projects\game-liminal-raytraced-llm-world\documentation\generated\ministral_qualification\scene-DP-v070-reasoning\scenes\server_aisles_dense.scene:18: Plane requires name, pos(), normal(), size(), and gray()
```

## Sector Control Hub

A compact control room opens off the aisles. One main console dominates the center while a locked cabinet and an access hatch wait at the margins.

Status : `valid`
Slug : `control_hub`
Generation : `19007.98 ms`
Audit + render : `923.01 ms`
Scene lines : `12`
Scene chars : `1076`
Triangles : `786`
Materials : `36`

State : `states/control_hub.session.json`
Prompt : `prompts/control_hub.prompt.txt`
Raw response : `responses/control_hub.raw.txt`
Final response : `responses/control_hub.final.txt`
Stop reason : `eos`
Scene : `scenes/control_hub.scene`
Audit stdout : `logs/control_hub.audit.stdout.txt`
Audit stderr : `logs/control_hub.audit.stderr.txt`

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

![control_hub](images/control_hub.png)

## Backup Vault

A chilled storage vault holds sealed pods and backup racks. A biometric pad, a wheeled crate, and a suspended service panel suggest expensive access.

Status : `valid`
Slug : `backup_vault`
Generation : `20168.30 ms`
Audit + render : `796.15 ms`
Scene lines : `13`
Scene chars : `1143`
Triangles : `1066`
Materials : `54`

State : `states/backup_vault.session.json`
Prompt : `prompts/backup_vault.prompt.txt`
Raw response : `responses/backup_vault.raw.txt`
Final response : `responses/backup_vault.final.txt`
Stop reason : `eos`
Scene : `scenes/backup_vault.scene`
Audit stdout : `logs/backup_vault.audit.stdout.txt`
Audit stderr : `logs/backup_vault.audit.stderr.txt`

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

![backup_vault](images/backup_vault.png)

## Cooling Exchange Bay

Tall cooling blocks and pipe runs crowd an industrial chamber. A valve wheel, a drip tray, and a maintenance console provide the room's few obvious handles.

Status : `valid`
Slug : `cooling_exchange_bay`
Generation : `24710.15 ms`
Audit + render : `850.06 ms`
Scene lines : `16`
Scene chars : `1399`
Triangles : `550`
Materials : `46`

State : `states/cooling_exchange_bay.session.json`
Prompt : `prompts/cooling_exchange_bay.prompt.txt`
Raw response : `responses/cooling_exchange_bay.raw.txt`
Final response : `responses/cooling_exchange_bay.final.txt`
Stop reason : `eos`
Scene : `scenes/cooling_exchange_bay.scene`
Audit stdout : `logs/cooling_exchange_bay.audit.stdout.txt`
Audit stderr : `logs/cooling_exchange_bay.audit.stderr.txt`

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

![cooling_exchange_bay](images/cooling_exchange_bay.png)

## Dust Loading Dock

A side loading bay opens toward the desert through a half-raised shutter. Pallets, a cargo crate, and a bent lamp post stage a tense threshold between inside and outside.

Status : `invalid_scene`
Slug : `loading_dock_dust`
Generation : `20874.40 ms`
Audit + render : `88.14 ms`
Scene lines : `14`
Scene chars : `1142`
Triangles : `0`
Materials : `0`

State : `states/loading_dock_dust.session.json`
Prompt : `prompts/loading_dock_dust.prompt.txt`
Raw response : `responses/loading_dock_dust.raw.txt`
Final response : `responses/loading_dock_dust.final.txt`
Stop reason : `eos`
Scene : `scenes/loading_dock_dust.scene`
Audit stdout : `logs/loading_dock_dust.audit.stdout.txt`
Audit stderr : `logs/loading_dock_dust.audit.stderr.txt`

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

Erreur :

```text
C:\works\projects\game-liminal-raytraced-llm-world\documentation\generated\ministral_qualification\scene-DP-v070-reasoning\scenes\loading_dock_dust.scene:10: Unknown scene directive
```

## Roof Watch

The datacenter roof becomes a parapet walk above the desert. A beacon mast, a hatch door, and a field scope hold the line against the horizon.

Status : `valid`
Slug : `roof_watch_dusk`
Generation : `17353.68 ms`
Audit + render : `238.86 ms`
Scene lines : `11`
Scene chars : `898`
Triangles : `366`
Materials : `32`

State : `states/roof_watch_dusk.session.json`
Prompt : `prompts/roof_watch_dusk.prompt.txt`
Raw response : `responses/roof_watch_dusk.raw.txt`
Final response : `responses/roof_watch_dusk.final.txt`
Stop reason : `eos`
Scene : `scenes/roof_watch_dusk.scene`
Audit stdout : `logs/roof_watch_dusk.audit.stdout.txt`
Audit stderr : `logs/roof_watch_dusk.audit.stderr.txt`

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

![roof_watch_dusk](images/roof_watch_dusk.png)

## Parapet Stairhead

A concrete stairhead interrupts the roof line. The landing is tight, with a latch box, a cable spool, and a narrow gate looking over the darkening desert.

Status : `valid`
Slug : `stairhead_parapet`
Generation : `18276.98 ms`
Audit + render : `500.19 ms`
Scene lines : `12`
Scene chars : `975`
Triangles : `450`
Materials : `39`

State : `states/stairhead_parapet.session.json`
Prompt : `prompts/stairhead_parapet.prompt.txt`
Raw response : `responses/stairhead_parapet.raw.txt`
Final response : `responses/stairhead_parapet.final.txt`
Stop reason : `eos`
Scene : `scenes/stairhead_parapet.scene`
Audit stdout : `logs/stairhead_parapet.audit.stdout.txt`
Audit stderr : `logs/stairhead_parapet.audit.stderr.txt`

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

![stairhead_parapet](images/stairhead_parapet.png)
