# Scene Generation Benchmark

Genere le 2026-10-24 10:57:00.

Generation directe de `.scene` ; profil `ministral-8b-q4`, modele `Ministral-3-8B-Instruct-2512-Q4_K_M.gguf`.

Pipeline :
- le moteur fabrique le prompt exact via `--dump-scene-audit-prompt`
- `llama-cli` genere une reponse brute
- la reponse est normalisee en `.scene`
- le renderer audite le fichier et produit une image si la scene est valide

Parametres :
- rendu : `800x400`, `8` spp, `3` bounces, `1` direct samples
- LLM : `temperature=0.0`, `n_predict=1024`, `ctx=4096`, `seed=42`
- reflexion : `off`, budget `0` ; top_k `40`, top_p `0.95`
- seul le canal final termine par EOS est audite ; les sorties tronquees sont refusees

Resultat global : `10/10` scenes avec syntaxe et camera valides, `0` invalides. Syntaxe seule : `10/10`.

| Case | Status | Gen ms | Audit ms | Triangles | Materials |
| --- | --- | ---: | ---: | ---: | ---: |
| `entry_gate_dusk` | `valid` | 22843.01 | 769.15 | 774 | 65 |
| `sally_port_checkpoint` | `valid` | 21530.99 | 783.09 | 582 | 49 |
| `badge_vestibule` | `valid` | 19274.85 | 847.83 | 616 | 53 |
| `server_aisles_dense` | `valid` | 24878.12 | 1453.32 | 1032 | 58 |
| `control_hub` | `valid` | 18208.33 | 1060.52 | 866 | 43 |
| `backup_vault` | `valid` | 25722.23 | 1164.62 | 1054 | 62 |
| `cooling_exchange_bay` | `valid` | 23106.66 | 567.61 | 546 | 46 |
| `loading_dock_dust` | `valid` | 22908.44 | 914.45 | 478 | 43 |
| `roof_watch_dusk` | `valid` | 21467.19 | 565.03 | 966 | 51 |
| `stairhead_parapet` | `valid` | 21518.45 | 682.23 | 582 | 50 |

## Entry Gate

A concrete gate court faces the datacenter threshold. Dust clings to the bars and the badge reader blinks beside a service intercom.

Status : `valid`
Slug : `entry_gate_dusk`
Generation : `22843.01 ms`
Audit + render : `769.15 ms`
Scene lines : `16`
Scene chars : `1345`
Triangles : `774`
Materials : `65`

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
Generation : `21530.99 ms`
Audit + render : `783.09 ms`
Scene lines : `15`
Scene chars : `1243`
Triangles : `582`
Materials : `49`

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
Generation : `19274.85 ms`
Audit + render : `847.83 ms`
Scene lines : `13`
Scene chars : `1070`
Triangles : `616`
Materials : `53`

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

Status : `valid`
Slug : `server_aisles_dense`
Generation : `24878.12 ms`
Audit + render : `1453.32 ms`
Scene lines : `17`
Scene chars : `1482`
Triangles : `1032`
Materials : `58`

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

![server_aisles_dense](images/server_aisles_dense.png)

## Sector Control Hub

A compact control room opens off the aisles. One main console dominates the center while a locked cabinet and an access hatch wait at the margins.

Status : `valid`
Slug : `control_hub`
Generation : `18208.33 ms`
Audit + render : `1060.52 ms`
Scene lines : `12`
Scene chars : `1015`
Triangles : `866`
Materials : `43`

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
Generation : `25722.23 ms`
Audit + render : `1164.62 ms`
Scene lines : `18`
Scene chars : `1541`
Triangles : `1054`
Materials : `62`

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
Generation : `23106.66 ms`
Audit + render : `567.61 ms`
Scene lines : `17`
Scene chars : `1376`
Triangles : `546`
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

Status : `valid`
Slug : `loading_dock_dust`
Generation : `22908.44 ms`
Audit + render : `914.45 ms`
Scene lines : `16`
Scene chars : `1309`
Triangles : `478`
Materials : `43`

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

![loading_dock_dust](images/loading_dock_dust.png)

## Roof Watch

The datacenter roof becomes a parapet walk above the desert. A beacon mast, a hatch door, and a field scope hold the line against the horizon.

Status : `valid`
Slug : `roof_watch_dusk`
Generation : `21467.19 ms`
Audit + render : `565.03 ms`
Scene lines : `15`
Scene chars : `1221`
Triangles : `966`
Materials : `51`

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
Generation : `21518.45 ms`
Audit + render : `682.23 ms`
Scene lines : `15`
Scene chars : `1230`
Triangles : `582`
Materials : `50`

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
