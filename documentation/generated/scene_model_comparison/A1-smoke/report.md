# Scene Generation Benchmark

Genere le 2026-10-24 09:10:38.

Generation directe de `.scene` ; profil `ministral`, modele `Ministral-3-8B-Instruct-2512-Q4_K_M.gguf`.

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

Resultat global : `0/1` scenes valides, `1` invalides.

| Case | Status | Gen ms | Audit ms | Triangles | Materials |
| --- | --- | ---: | ---: | ---: | ---: |
| `entry_gate_dusk` | `invalid_protocol` | 22710.05 | 0.00 | 0 | 0 |

## Entry Gate

A concrete gate court faces the datacenter threshold. Dust clings to the bars and the badge reader blinks beside a service intercom.

Status : `invalid_protocol`
Slug : `entry_gate_dusk`
Generation : `22710.05 ms`
Audit + render : `0.00 ms`
Scene lines : `0`
Scene chars : `0`
Triangles : `0`
Materials : `0`

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

Erreur :

```text
Missing or incompatible llama-cli assistant transcript boundary
```
