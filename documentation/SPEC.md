# Liminal Raytraced LLM World

## Artistic, Research, and Technical Specification

Status: active direction as of 2026-08-09. The project has no final public title. *Le Désert des tokens* is the previous working title.

## 1. Premise

This project starts from a counterfactual question about the history of video games:

> What if video games had spent their computational power on narrative plasticity rather than on graphical sophistication?

The project inverts the dominant allocation of computation. Expensive work happens primarily in local language-model inference: interpreting player intent, updating a partial world model, and proposing selected changes to the player's field of movement. A deliberately inexpensive CPU raytracer produces sparse, noisy, low-information images.

The active fiction makes this inversion spatially concrete. The player explores a Venusian extraction zone containing human quarry and survey infrastructure and an alien labyrinth whose walls are ordinarily invisible. Local places remain readable; selected global connections, orientations, and traversal constraints can change.

The result should be both a playable interactive system and an executable counterfactual machine. It should not merely describe an alternate media history; it should make a fragment of that history run.

## 2. Artistic and research positioning

### 2.1 Central research questions

The project is not primarily an argument about literary quality. Its principal questions are:

> How can an AI-driven interactive work extend a demoscene lineage by turning the spatial inconsistency of generative models into an expressive, navigable constraint?

> What becomes of the video-game image when computational expense, technical innovation, and cultural prestige no longer concentrate on it?

> What must remain stable for a changing maze to feel uncanny rather than merely broken?

The prototype should help observe:

- how generated language and structured state become visible space
- what is lost between player command, interpretation, topology, geometry, rendering, and perception
- how a poor image activates projection rather than merely withholding information
- how players distinguish an invisible barrier or deliberate contradiction from parser or engine failure
- how inference latency changes the rhythm of parser-like play
- how computational allocation becomes perceptible as an aesthetic decision

### 2.2 Four-stage Eryx lineage

The active work continues a motif that has already crossed several media and computational regimes:

```text
Lovecraft and Kenneth J. Sterling, *In the Walls of Eryx*
        ↓
ASD, *Beyond the Walls of Eryx* (2007)
        ↓
Mandarine, *Within the Mesh* (2013)
        ↓
current interactive LLM work
```

The literary source provides a conceptual matrix: Venus as an extractive frontier, valuable crystals, invisible architecture, failed mapping, and the progressive loss of spatial certainty. It is a source to reinterpret, not a plot or prose style to reproduce.

ASD's [*Beyond the Walls of Eryx*](https://www.pouet.net/prod.php?which=31088) transforms the motif into a demoscene audiovisual event of lines, linked spaces, changes of projection, transformation, and collapse.

Mandarine's [*Within the Mesh*](https://www.pouet.net/prod.php?which=61730), co-created by the present project's author, continues that relation. The lineage is therefore not an opportunistic reference added to an AI artwork; it is historical and personal.

The present work adds a transformation unavailable to the earlier stages: the player inhabits a computational labyrinth whose selected topological relations can actually change during play.

### 2.3 Demoscene lineage and continuation

The project does not claim that an interactive artwork is automatically a canonical demo. It makes the more defensible claim that the work is a new transformation of a motif already present in demoscene history and in the author's own practice.

The relationship is:

- historical: a literary motif enters the demoscene through ASD
- personal: the author later co-created *Within the Mesh*
- technical: the project retains direct control of a native stack, real-time execution, platform awareness, voluntary constraint, and visible computation
- medial: the same motif is changed by prose, fixed audiovisual sequence, personal demoscene reinterpretation, and interactive generation

The broader situated visual lineage includes Atari ST and Amiga culture, crack intros, *Driller*, *The Sentinel*, *Tau Ceti*, *Midwinter*, *The Pawn*, *Captain Blood*, *Drakkhen*, *Enigma*, *Origin Complex*, *Substance*, *Hardwired*, and *Amiga Desert Dreams*.

These references establish an inherited visual grammar of incomplete but operational space. The project is a contemporary translation of constraint-based expressivity, not a retro simulation.

### 2.4 Prototype as research instrument

The prototype must not merely illustrate a predetermined thesis. It must be able to reveal where the concept resists implementation:

- which contradictions are fertile and which destroy playability
- how often topology can mutate before frustration dominates
- what players need in order to map, retrace, and diagnose a route
- when image poverty strengthens projection and when it collapses legibility
- whether the intended computational inversion is borne out by measurements

## 3. Core vision

The project should deliver a local-first native experience in which:

- the player uses open text commands in a parser-like cadence
- a local LLM interprets intent, narrates concise consequences, and proposes structured state changes
- the engine owns authoritative state and validates all changes
- a distinct spatial layer represents local geometry, adjacency, exits, and invisible traversal constraints
- a deterministic or hybrid compiler turns accepted spatial state into the narrow `scene v1` format
- the image is raytraced, noisy, austere, and dominated by grayscale
- the visible world consists of sparse Venusian extraction and prospecting infrastructure
- the invisible labyrinth is encountered through topology and perception rather than rendered as conventional walls
- the artifact can support play, exhibition, demoparty presentation, and research analysis

## 4. Design pillars

### 4.1 Inference and world interpretation as the computational center

The LLM is not merely a content generator. It interprets player language, produces concise narration, updates selected soft facts, and proposes topological mutations. The image is a partial consequence of this world pipeline.

### 4.2 Poor image as consequence, not skin

The visual austerity must emerge from real constraints:

- a small primitive vocabulary
- limited material control
- camera-linked instrumental lighting
- low sample counts and visible raytracing noise
- low information density
- deterministic simplification between spatial meaning and geometry

The implemented renderer currently uses a locked semantic RGB palette rather than strict monochrome. This limited color policy remains compatible with an image dominated by grayscale; the LLM must not gain arbitrary color control.

### 4.3 Extraction brutalism

“Brutalist” refers to the construction logic of the setting, not a vague mood label. The visual world should feel constructed, excavated, surveyed, and partially unreadable through:

- quarry cuts and plateaus
- slabs, retaining walls, ramps, trenches, and narrow service passages
- sparse shelters and instrument housings
- repeated extraction or atmospheric-processing modules
- harsh open terrain and incomplete sensing

The project must not drift into generic cyberpunk, datacenter horror, or NASA realism.

### 4.4 Local coherence, controlled global drift

The player must always be able to understand the immediate situation and attempt a useful action. Hard state and local landmarks persist. Selected soft topology may drift:

- an exit becomes blocked or unblocked by an unseen barrier
- a connection reaches another known location
- a return arrives from an impossible direction
- a locally recognizable place has changed adjacency
- a mapping claim conflicts with the traversable graph

Instability is authored through a bounded proposal-and-validation mechanism. “The AI hallucinates” is not a design specification.

### 4.5 Open language interaction

The system should absorb paraphrase, ambiguity, and unconventional wording instead of relying on a rigid verb whitelist. It may ask for clarification. Parser failure, invisible collision, and topology change must remain distinguishable enough to support a next action.

### 4.6 Direct technical ownership

The native runtime should own inference, validation, rendering, save data, and UI instead of outsourcing core behavior to external services.

### 4.7 Measurable computational inversion

The system must make inference time, scene compilation and validation, and render time observable. The allocation of computation is an empirical research surface, not only a metaphor.

## 5. Non-goals

The v1 prototype is not trying to provide:

- photorealistic rendering
- a scientifically or historically realistic Venus
- a scene-by-scene adaptation of *In the Walls of Eryx*
- a conventional visible alien maze
- generic cosmic-horror prose
- a fully deterministic simulation or a large handcrafted map
- a conventional puzzle-logic parser game
- online multiplayer or mandatory cloud inference
- proof that an LLM writes better fiction than a human author
- spatial change driven only by random sampling temperature
- a definitive answer about whether the project is a “true demo”

## 6. Target experience

A representative interaction is:

1. The player types `survey the barrier`, `mark this route`, `return to the beacon`, or `inspect the crystal sample`.
2. The LLM interprets the command and produces concise narrative and structured deltas.
3. The result may include a proposal to alter a selected topological relation.
4. The engine validates the proposal against hard state, local actionability, and safety rules.
5. The accepted spatial state is compiled deterministically into a renderable scene.
6. The viewport shows an instrumental image that may confirm, reduce, or partially contradict the text.
7. The player continues by moving, comparing, mapping, retracing, or testing the apparent contradiction.

Tone targets:

- uncanny and austere
- oppressive without generic horror ornament
- dreamlike but actionable
- sparse, constructed, and surveyed
- occasionally contradictory at the level of global space

Inference latency may be presented as visible machine activity rather than hidden, but the prose should remain concise enough to preserve play rhythm.

## 7. Functional requirements

### 7.1 Input and interaction

- Support free-text keyboard input and a one-command/one-response cadence.
- Interpret intent rather than only surface verbs.
- Support movement, inspection, manipulation, inventory, marking, mapping, surveying, and optional dialogue.
- Ask for clarification when ambiguity would otherwise threaten hard state.
- Provide actionable feedback when movement meets an invisible barrier.
- Preserve a safe next action or recovery route after accepted topology mutations.

### 7.2 Narrative generation

- Use `Ministral 3 8B Instruct 2512` in GGUF format through `llama.cpp` as the current target.
- Generate short-form prose during active play.
- Maintain recurring locations, objects, named entities, and discoveries through external state.
- Optimize for playable coherence rather than maximal literary flourish.
- Treat environmental or model temperature as optional and subordinate. If used, couple it to Eryx-relevant stress or spatial entropy, not to the superseded datacenter premise.

### 7.3 Spatial mediation and mutation

The LLM may propose, but not directly commit:

- exit state changes
- adjacency changes
- route-length or orientation contradictions
- newly discovered invisible barriers
- disappearance of known barriers
- local substructures within open areas

The engine must validate proposals. At minimum it must protect hard state, avoid trapping the player without intended recovery, preserve local renderability, log the decision, and keep the result inspectable.

### 7.4 Scene generation

The production path is:

```text
structured LLM result
    -> validated world/spatial state
    -> deterministic or hybrid scene compiler
    -> scene v1
    -> renderer
```

Direct LLM generation of a complete `.scene` remains an audit and stress-test path, not the authoritative production path. Direct `.OBJ` generation remains experimental.

Invisible barriers belong to traversal and collision semantics. Optional diagnostic visualization or indirect evidence may reveal them, but an ordinary opaque `prefab_invisible_wall` would defeat the concept.

### 7.5 Rendering

- Preserve the current locked palette and an image dominated by grayscale.
- Use camera-linked, instrument-like lighting.
- Include raytraced diffuse bounce or crude radiosity at intentionally low sample counts.
- Reserve bounded dielectric refraction, Fresnel reflection, thickness-dependent absorption, and deliberately approximate three-band RGB dispersion for crystals; do not expose a general material language to the LLM.
- Preserve visible noise instead of denoising it away.
- Prioritize mood, legibility, and speed over physical accuracy.
- Keep rendering computationally secondary to inference in the intended experience, or make any exception measurable and explicit.

### 7.6 Runtime and distribution

- Distribute a native desktop build.
- Use `llama.cpp`; do not require Ollama.
- Run offline after setup.
- Support save/load.
- Keep CPU-only inference possible, even if slower, while supporting available hardware acceleration.

### 7.7 Instrumentation and logging

Record at least:

- inference time, prompt size, and output size per turn
- scene compilation, validation, and render time
- state and topological proposals, validator decisions, and fallbacks
- memory use and CPU/GPU activity when available
- parser clarifications and failed traversal feedback
- location revisions and local-scene fingerprints useful for playtest analysis

## 8. Technical direction

### 8.1 Application stack

Preferred direction:

- native C++ desktop executable
- `llama.cpp` embedded or linked directly
- renderer in the same runtime
- `SDL3` for windowing, input, audio, and framebuffer presentation
- project-specific UI for transcript, command input, viewport, state, and diagnostics

Avoid Python services or external model daemons in the shipped runtime. Repository scripts may still support development, download, benchmarking, and documentation generation.

### 8.2 Model runtime

Assume:

- model: `Ministral 3 8B Instruct 2512`
- preferred artifact: GGUF `Q4_K_M`
- runtime: `llama.cpp`
- primary development acceleration: CUDA
- optional CPU and other supported hardware backends where practical

The application owns model loading, prompt assembly, generation, cancellation, and output validation.

### 8.3 Turn pipeline

```text
player command
    -> context projection
    -> constrained LLM result
    -> schema validation / repair
    -> hard-state validation
    -> topological proposal validation
    -> committed hard, soft, and spatial state
    -> deterministic scene compilation
    -> scene audit / safe fallback
    -> raytraced rendering
    -> player-facing text and image
```

Each boundary must be inspectable and independently debuggable.

### 8.4 C++ guidelines

Use a conservative Orthodox C++-style subset:

- keep C++11 as the baseline unless a later feature has a concrete, documented benefit
- prefer plain structs, free functions, and narrow classes with explicit ownership
- do not use exceptions, RTTI, modules, or abstraction-heavy inheritance
- do not use `iostream` or `stringstream`; prefer explicit logging and `printf`-style formatting
- avoid hidden allocations in runtime-critical paths
- avoid template metaprogramming unless it clearly reduces complexity
- keep subsystem APIs, allocator boundaries, and ownership transfer obvious
- prefer dependencies compatible with no-exceptions/no-RTTI builds

## 9. World model

The model is intentionally asymmetric.

### 9.1 Hard state

Hard state must persist reliably:

- turn number and authoritative current graph node
- inventory, samples, tools, and survival resources if used
- persistent named entities and major discoveries
- resolved and unresolved commitments that affect action
- accepted barriers or topology facts the design marks immutable
- session, save, model, and validation metadata

Hard state must never depend only on the prose of the last turn.

### 9.2 Soft state

Soft state may be summarized or drift:

- atmosphere and local interpretations
- non-actionable history or explanations
- psychological confidence and inferred meaning
- minor object placement

### 9.3 Mutable spatial state

Spatial state sits between fiction and render geometry. It may include:

- stable location identity and local visual anchors
- current adjacency and exits
- invisible barriers and traversal feedback
- orientation, route length, and mapping claims
- locally visible extraction structures and instruments
- diagnostic evidence and scene constraints
- accepted mutation history

The LLM should change this state through bounded deltas, not by inventing an unconstrained final scene.

## 10. LLM contracts

The turn result must be structured. A target payload should cover:

- interpreted intent
- concise narration
- optional clarification
- hard-state deltas
- soft-state deltas
- spatial deltas
- optional topological mutation proposal
- continuity notes

Room generation should likewise return semantic metadata and qualitative constraints rather than exact arbitrary coordinates. The engine owns placement, collision avoidance, camera choice, and final scene syntax.

Grammar-constrained output is desirable where stable, but validation, repair, and safe no-op fallback remain mandatory.

## 11. Scene representation

### 11.1 Production format

Use the narrow proprietary primitive format documented in [`SCENE_FORMAT_V1.md`](./SCENE_FORMAT_V1.md). It is compact, auditable, deterministic to compile, and easier to validate than general mesh formats.

The implementation currently supports only its documented subset and prefabs. Planned Eryx semantics must not be claimed as scene directives until they are implemented.

### 11.2 Direct scene and mesh generation

Direct LLM `.scene` output remains valuable as an audit benchmark because it reveals truncation, invented properties, and semantic drift. It is not the normal runtime authority.

Direct `.OBJ` generation may be explored for isolated research cases, but its token cost, malformed geometry, scale drift, and validation burden make it unsuitable as the v1 production path.

## 12. UI and presentation

The interface should feel like a prospecting, mapping, or sensing apparatus rather than a glossy game HUD.

Required elements:

- current rendered image
- transcript and command input
- save/load access
- clear busy/cancellation state
- settings for model path and performance profile

Recommended research and debug elements:

- command history and transcript export
- current exits, map claims, or survey marks where fictionally appropriate
- raw structured result and validator decision
- scene provenance, state diff, and timing telemetry
- optional invisible-topology diagnostic view unavailable in ordinary player presentation

### 12.1 Autonomous or ghost-player presentation

An exhibition or demoparty mode may run an authored sequence of survey, movement, marking, retracing, comparison, contradiction, and remapping commands. The script supplies dramatic rhythm; the LLM and topology validator preserve live uncertainty. It should not become an unconstrained model conversation.

## 13. Save, load, and session logs

Save data should preserve:

- transcript or transcript summary
- hard, soft, and spatial state
- current scene or enough committed state to reproduce it
- discovered locations, connections, barriers, and mutation history
- persistent entities and player marks
- model identifier and relevant generation settings
- seeds or sampling metadata when reproducibility is desired

Logs should preserve turn timings, validator outcomes, fallbacks, scene provenance, and structured deltas. Perfect regeneration from prompts alone is not assumed.

## 14. Distribution

Goals:

- Windows and macOS native builds
- Linux if practical
- no Python or Ollama requirement for players
- no mandatory network dependency after setup

Model weights may be bundled, downloaded on first run, or shipped separately. The final choice depends on redistribution terms, artifact size, and installation testing.

## 15. Prototype priorities and roadmap

### Phase A — Preserve and baseline the current runtime

- retain the working `llama.cpp`, SDL3, state, compiler, raytracer, save/load, and benchmark paths
- keep datacenter fixtures and benchmarks as historical technical baselines
- verify factual documentation without renaming runtime identifiers prematurely

### Phase B — Eryx semantic migration

- replace active datacenter prompts and default narrative vocabulary
- define extraction/prospecting location archetypes and object semantics
- distinguish invisible barriers from ordinary blocked exits in spatial state
- define a structured topological mutation proposal and validator
- repurpose existing prefab geometry where convincing; implement a very small high-value set of new prefabs
- regenerate source-derived prefab and benchmark documentation only after implementation

### Phase C — Playable labyrinth slice

- provide a five-to-ten-minute parser-style session
- support survey, marking, movement, retracing, and contradiction feedback
- keep revisited locations locally recognizable
- preserve hard state through impossible return paths and accepted topology changes
- log enough evidence to compare intentional instability with failure

### Phase D — Research readability and presentation

- run the spatial validation protocol
- calibrate mutation frequency and recovery affordances
- expose useful telemetry and diagnostic views
- prepare a controlled ghost-player sequence
- test the work as colloquium artifact, exhibition installation, and demoparty presentation

### Phase E — Distribution and optional geometry research

- package native builds and model management
- test offline installation and performance profiles
- retain direct `.scene` and `.OBJ` generation as bounded experiments

## 16. Major friction points

### 16.1 Uncanny topology versus broken interaction

The design must make a changed route feel like an event in the world, not lost input or corrupt state. Clear local feedback, persistent anchors, mutation limits, and recovery paths are central.

### 16.2 Invisible but actionable barriers

If a wall is neither visible nor explained through text, collision reads as a bug. If it is rendered as an ordinary wall, the central perceptual tension disappears. Indirect evidence and diagnostic tooling require careful separation.

### 16.3 Authoritative state versus generative agency

Over-constraining the model makes topology decorative; under-constraining it destroys playability. The proposal/validation contract must identify exactly which relations the model can influence.

### 16.4 Model availability, licensing, and local performance

An 8B-class local model remains heavy. Latency, RAM/VRAM, load time, package size, and redistribution conditions remain practical risks.

### 16.5 Geometry and contract reliability

Structured model output can still be malformed, truncated, or semantically vague. Repair and safe fallback are required, while direct scene generation must remain non-authoritative.

### 16.6 Style becoming decoration

Sparse quarry brutalism, limited color, and visible noise must remain consequences of the system and fictional setting. Adding decorative detail or expensive rendering can weaken both the computational inversion and the perceptual premise.

### 16.7 Debuggability and telemetry

Inference, state projection, topology validation, compilation, and rendering can fail separately. Each stage needs provenance, diagnostics, and measured timings.

## 17. Evaluation questions

1. Can players identify an intentional invisible barrier and respond usefully?
2. How much topology change is tolerable in a short session?
3. Do revisited places remain recognizable when their connectivity changes?
4. Can an impossible return path occur without corrupting inventory, discoveries, or current position?
5. How do player-made maps expose or fail to expose the labyrinth?
6. How do text, image, movement, and diagnostic evidence distribute knowledge differently?
7. Does the constrained image stimulate projection while remaining actionable?
8. Does measured inference remain the dominant computational cost?
9. What is preserved, lost, or newly possible across the Eryx literary and demoscene lineage?
10. How can the work mediate demoscene values of platform awareness and expressive allocation for a contemporary audience?

Evaluation should combine direct playtesting, telemetry, state/scene inspection, mapping artifacts, and qualitative reports.

## 18. Computational-constraint mediation

The WinUAE DMA visualizer remains an important complementary research device. It can show that historical constraint is allocation rather than simply “old machines were slow”: CPU, Copper, Blitter, bitplanes, sprites, audio, and other accesses compete for Amiga memory-bus time.

The comparison with the current project is conceptual, not architectural:

```text
Amiga-era demo:
hardware resources -> coordinated audiovisual output

current project:
GPU compute -> inference, interpretation, topological proposals
CPU compute -> deliberately constrained raytraced image
```

In both cases, aesthetic form depends on where the machine spends its resources.

## 19. Summary

The project is a local-first interactive fiction engine, a constrained raytraced executable artwork, and a research apparatus for unstable computed space. It extends a specific literary and demoscene lineage by making the Eryx labyrinth playable and generative.

Its pragmatic form is not a general 3D world generator. It is a bounded system in which a local LLM interprets action and proposes selected spatial changes, an authoritative engine protects playability, and a poor but material image witnesses only part of the world.

AI is used not to escape computational constraint, but to relocate it.
