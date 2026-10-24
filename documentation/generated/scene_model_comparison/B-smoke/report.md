# Scene Generation Benchmark

Genere le 2026-10-24 09:11:47.

Generation directe de `.scene` ; profil `gemma-e4b`, modele `gemma-4-E4B-it-Q4_K_M.gguf`.

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

Resultat global : `1/1` scenes valides, `0` invalides.

| Case | Status | Gen ms | Audit ms | Triangles | Materials |
| --- | --- | ---: | ---: | ---: | ---: |
| `entry_gate_dusk` | `valid` | 15090.98 | 779.75 | 630 | 37 |

## Entry Gate

A concrete gate court faces the datacenter threshold. Dust clings to the bars and the badge reader blinks beside a service intercom.

Status : `valid`
Slug : `entry_gate_dusk`
Generation : `15090.98 ms`
Audit + render : `779.75 ms`
Scene lines : `13`
Scene chars : `1103`
Triangles : `630`
Materials : `37`

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
