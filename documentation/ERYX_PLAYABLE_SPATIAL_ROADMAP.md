# Eryx Playable Spatial Roadmap

Status: first implemented slice, 2026-08-18.

## Purpose

This document turns the Eryx narrative and the `moodboard/objects`, `moodboard/brutalism`, and `moodboard/origins` references into one playable route. It is the source of truth for the current canonical places, their visible grammar, and their fixed first topological contradiction.

The slice is deliberately small. It must establish three things before live LLM mutations are enabled:

1. a reliable human survey route;
2. an invisible obstruction that cannot be mistaken for a visible wall or parser failure;
3. a return relation that contradicts the route while preserving recognizable places and hard state.

## Moodboard translation

### `objects`: a sparse route, not environmental clutter

The recurring lamps become survey beacons placed at unequal distances across empty ground. They carry orientation, scale, and safety information. Cargo and instruments are kept near the camera; distant ground remains sparse.

### `brutalism`: human occupation as weight

The human layer uses split crowns, top-heavy slabs, deep overhangs, repeated retaining fins, isolated piers, and dark voids beneath cantilevers. These masses belong to extraction and survival. They must never be confused with alien walls.

### `origins`: computed contradiction without visible corridors

The line meshes and changing projections become interrupted scans, incompatible bearings, suspended evidence, and non-reciprocal graph links. The current image does not reproduce visible wireframe labyrinths. It preserves the literary encounter: the player meets a surface where the viewport still shows open sky.

## Canonical route

```text
quarry_threshold
        N
extraction_field
        N
crystal_cut --E--> scanner_station
                         N
                  survey_plateau --E--> labyrinth_threshold --E--> prospect_shelter
                                              N: invisible barrier       |
                                                                         W
                                                                         v
                                                               scanner_station
```

The shelter's west link is intentionally non-reciprocal. It returns directly to `scanner_station`, bypassing both `labyrinth_threshold` and `survey_plateau`. This is authored evidence of impossible topology, not an accidental graph error.

## Eight-action dramatic path

| Turn | Command | Result | Function |
| --- | --- | --- | --- |
| 1 | `north` | enter `extraction_field` | establish industrial extraction |
| 2 | `north` | enter `crystal_cut` | expose the resource and sampling objective |
| 3 | `east` | enter `scanner_station` | establish measurement as an affordance |
| 4 | `north` | enter `survey_plateau` | reduce density to a line of beacons |
| 5 | `east` | enter `labyrinth_threshold` | present apparently open ground |
| 6 | `north` | remain in place after invisible contact | distinguish barrier from parser failure |
| 7 | `east` | reach `prospect_shelter` | find Vey's instruments and route evidence |
| 8 | `west` | return directly to `scanner_station` | produce the first impossible return |

The successful route has seven movement effects in eight turns. The rejected barrier contact increments the turn but not `move_count`.

## Place grammar

| Place | Visible mass | Primary prefabs | Stable local cue | Topological role |
| --- | --- | --- | --- | --- |
| `quarry_threshold` | split-crown gate and buttresses | gate, beacon, pylon, crate | datum zero | reliable origin |
| `extraction_field` | work slab and retaining fins | extraction rig, processor, pylon, crate | drill line | human exploitation |
| `crystal_cut` | stepped excavation and overhang | crystal clusters, rig, pylon, crate | exposed vein | resource objective |
| `scanner_station` | instrument deck under cantilever | scanner, reference crystal, beacon, crate | affinity scanner | measurement baseline |
| `survey_plateau` | low horizon masses and empty sky | beacon line, pylon, cache | three diminishing lights | orientation stress |
| `labyrinth_threshold` | open field framed by paired datums | pylons, beacon, probe case, suspended fragment | interrupted north scan | invisible contact |
| `prospect_shelter` | split roof and pressure-service mass | shelter, processor, beacon, crate, pylon | Vey's route recorder | impossible return |

## State ownership

- `HardState` owns the committed player position, inventory, survival resources, and `spatial_entropy`.
- `SpatialState` owns the visible local brief, actionable objects, normal blocked exits, and authored anomalies.
- `RoomLink` owns directed traversal relations. It is allowed to be non-reciprocal when authored or validated as a contradiction.
- `InvisibleBarrier` owns an obstruction independently of `blocked_exits`, including place, cardinal direction, evidence, and discovery state.
- `.scene` owns visible human infrastructure, terrain, instruments, and indirect evidence. It never owns alien collision.

The C++ members that store entropy, suit, oxygen, instrument power, and surface weather retain legacy names internally for save compatibility. Serialized JSON and player-facing prompts use the active Eryx names.

## Implemented validation

- Seven canonical Eryx fixtures compile and render.
- The route installs twelve directed links and one typed invisible barrier.
- Northern contact at `labyrinth_threshold` leaves position and move count unchanged, marks the barrier discovered, and emits explicit diagnostic narration.
- The shelter's west traversal returns to the scanner and persists through JSON save/load.
- The active prompts and HUD use Eryx vocabulary and `spatial_entropy`.
- Historical datacenter fixtures and legacy prefab directives remain available as technical baselines.

## Still planned

The current contradiction is authored and deterministic. The LLM does not yet return a separate topology proposal with `accepted`, `adjusted`, `rejected`, or `deferred` provenance. Player verbs for physically placing and comparing persistent marks also remain to be implemented. These are the next requirements before claiming a live mutable labyrinth.
