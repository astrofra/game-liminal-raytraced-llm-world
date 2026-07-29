# Liminal Raytraced LLM World

## Artistic, Research, and Technical Specification

## 1. Premise

This project starts from a counterfactual question about the history of video games:

> What if video games had spent their computational power on narrative plasticity rather than on graphical sophistication?

Instead of dedicating most of the machine budget to high-fidelity rendering while keeping narrative logic relatively fixed, the project deliberately inverts the allocation of computation. Most of the expensive work should happen in local language-model inference: interpreting player intent, updating a partial world model, and producing short-form narrative responses. Visual rendering should remain intentionally poor, geometric, grayscale, noisy, and visibly synthetic.

The resulting prototype is both a playable interactive fiction system and an executable counterfactual machine. It should not merely describe an alternate media history; it should make a fragment of that alternate history run.

The player interacts through a keyboard-first parser interface in the tradition of *Zork* and Infocom, but command interpretation is delegated to a local LLM rather than a deterministic verb parser. The result should feel like a compelling but unstable game master: suggestive, locally coherent, globally fragile, and sometimes contradictory in productive ways.

Each turn should produce:

- a short narrative response
- an interpretation of the player's action
- an updated local world state
- a renderable scene description
- a rendered grayscale image of the current place

The world is expected to remain only relatively stable. Places may recur, mutate, contradict themselves, or connect through weak topology. This instability is not a defect as long as short-term play remains legible and actionable.

## 2. Artistic and Research Positioning

### 2.1 Image-Centered Research Question

The project is not primarily an argument about literary quality. Its central research question is instead:

> What becomes of the video-game image when computational expense, technical innovation, and cultural prestige no longer concentrate on it?

The prototype should help observe:

- what an image becomes when it is no longer the main site of calculation
- how generated text is translated into visible space
- what is lost between narrative description, world state, geometry, and rendering
- how a poor image activates the player's imagination
- how players reconcile contradictions between text and image

### 2.2 Situated Artistic Lineage

The project emerges from a situated artistic lineage in which early video games, crack intros, and demos were part of the same lived microcomputer culture. The relevant references are not only technical but experiential:

- Atari ST and Amiga domestic computing culture
- geometric 3D game spaces such as *Driller*
- demoscene works such as *Enigma* by Phenomena
- low polygon counts, abstraction, harsh limitations, and real-time surprise

This matters because the project does not treat poor graphics as irony or retro decoration. The intended visual poverty should reactivate a historically and personally grounded relation to computed images.

### 2.3 Demoscene Adjacency

The strongest connection to the demoscene is not a genre claim. It is a shared attitude toward expressive allocation of computational resources.

In that sense, the project should inherit several demoscene-adjacent traits:

- a specific or minimal engine
- direct ownership of the technical stack
- real-time execution
- voluntary limitation
- a tight bond between architecture and aesthetics
- visible material consequences of computational choices

The prototype may be treated as a demoscene-adjacent executable work without needing to resolve whether it counts as a "true demo".

### 2.4 Prototype as Research Instrument

The prototype must not function as a mere illustration of a predetermined thesis. It should be built as an instrument of inquiry capable of producing resistance, ambiguity, and unexpected outcomes.

It should help reveal:

- which incoherences are fertile and which destroy playability
- how waiting and latency reshape interactive fiction
- when image poverty strengthens projection and when it collapses legibility
- which facts must remain stable for instability to remain aesthetic rather than frustrating

## 3. Core Vision

The project should deliver a playable local-first prototype where:

- the fiction is generated locally by an LLM
- the game is distributable as a native application
- no Python runtime is required
- no Ollama dependency is required
- `llama.cpp` is the preferred inference backend
- each location can be rendered from a minimal scene description
- the visual identity is grayscale, raytraced, noisy, brutalist, and liminal
- the system can be used both as a playable artifact and as a research demonstrator

## 4. Design Pillars

### 4.1 Narrative-First Compute

The narrative layer is the main computational center of the project. The image is not the main spectacle; it is a consequence of the fiction pipeline.

### 4.2 Poor Image as Consequence, Not Skin

The brutalist monochrome look must emerge from actual constraints:

- simple primitives
- one light source
- low resolution
- low sample counts
- incomplete spatial continuity
- preserved raytracing noise

The aesthetic should not feel like a decorative filter pasted over a conventional renderer.

### 4.3 Local Coherence, Global Drift

The world should remain actionable at the sentence and turn level while allowing broader geography, architecture, and atmosphere to drift.

### 4.4 Open Language Interaction

Player input should remain open-ended. The system should absorb paraphrase, ambiguity, and unconventional wording instead of relying on a rigid verb whitelist.

### 4.5 Direct Technical Ownership

The runtime should be self-contained and directly own inference, validation, rendering, save data, and UI rather than outsourcing core behavior to external services.

### 4.6 Measurable Computational Inversion

The inversion of computational emphasis must be observable, not rhetorical. The system should make it possible to compare inference cost, scene-generation cost, and rendering cost.

## 5. Non-Goals

The v1 prototype is not trying to provide:

- photorealistic rendering
- a fully deterministic simulation
- a large handcrafted map
- a conventional puzzle-logic parser game
- online multiplayer
- cloud inference as a requirement
- proof that an LLM writes better fiction than a human author
- a definitive answer about whether the project belongs inside demoscene genre boundaries

## 6. Target Experience

The intended player experience is:

1. The player types a command such as `open the metal door`, `listen`, `go back`, or `ask the woman what she lost`.
2. The LLM interprets the intention and decides what happens.
3. The game returns short prose, updates the local fiction state, and emits a constrained scene description.
4. The renderer shows a grayscale image of the place that may confirm, reduce, distort, or partially contradict the prose.
5. The player continues through a chain of spaces that feel haunted, partial, and statistically improvised.

Tone targets:

- uncanny
- austere
- oppressive
- dreamlike but readable
- occasionally contradictory

The game should avoid classic parser dead ends such as repeated `I don't understand that verb`. Even failure should feel diegetic: the world ignores the action, misreads it, or transforms it into another plausible event.

The player should also feel the temporal structure of the system. Inference latency is not just a technical issue; it is part of the rhythm of play and may be used aesthetically.

## 7. Functional Requirements

### 7.1 Input and Interaction

- The game must support free-text keyboard input.
- The game must preserve a parser-like cadence: one player command, one world response.
- The LLM must interpret intent, not just surface verbs.
- The system must support basic IF actions such as movement, inspection, manipulation, dialogue, and inventory-related behavior.
- The system must be able to ask for clarification when input is too ambiguous.

### 7.2 Narrative Generation

- The current target model is `Ministral 3:14b`.
- The game must generate short-form narrative output per turn.
- The system must maintain enough state for local continuity across turns.
- The system must support recurring locations, recurring objects, and recurring NPC references.
- The system should favor concise prose over long paragraphs during active play.
- The project should optimize for playable coherence, not maximal literary flourish.

### 7.3 Scene Generation

For each location or major state change, the system must produce a renderable scene description.

Two content-generation modes are allowed:

- Preferred mode for v1: a simple proprietary scene format describing primitives, transforms, and basic material properties.
- Experimental mode: `.OBJ` generation directly from the LLM.

### 7.4 Rendering

- The renderer must produce grayscale output.
- Lighting must use a single light source attached to the camera, like a harsh found-footage lamp.
- The renderer must include raytraced diffuse bounce or crude radiosity at intentionally low sample counts.
- The final image should preserve visible noise rather than denoise it away.
- The renderer should prioritize mood, legibility, and speed over accuracy.
- The renderer should remain computationally cheaper than, or at least clearly secondary to, model inference in the intended experience.

### 7.5 Runtime and Distribution

- The game must be distributable as a native desktop build.
- The runtime should use `llama.cpp` rather than Python-based stacks.
- The game should not require Ollama.
- The game should run offline after installation.
- Save/load support is required.

### 7.6 Instrumentation and Logging

The prototype must expose enough telemetry to evaluate the central hypothesis.

It should record at least:

- model inference time per turn
- prompt and output size per turn
- scene-generation and scene-validation time
- render time
- memory use
- CPU and GPU activity when available
- validation failures and fallback behavior

## 8. Recommended Technical Direction

### 8.1 Application Stack

Preferred direction:

- native desktop executable
- `llama.cpp` embedded or linked directly
- renderer implemented in the same native runtime
- simple UI layer for text input, transcript view, image display, and debug access

Pragmatic implementation preference:

- C++ is the most direct fit because of `llama.cpp` integration and renderer ownership.
- Rust is acceptable if the project wants safer application code and is willing to manage FFI boundaries cleanly.

The main requirement is not the language itself, but avoiding a runtime architecture that depends on Python services or external model daemons.

### 8.2 Model Runtime

The system should assume:

- a `llama.cpp`-compatible model artifact is available for `Ministral 3:14b`
- quantized local inference is required for practical distribution
- hardware acceleration should be optional but supported when available

Practical backend expectations:

- CPU-only execution must remain possible, even if slower
- Metal, CUDA, or Vulkan acceleration should be used opportunistically
- the application must handle model loading, prompt assembly, and inference directly

### 8.3 Turn Pipeline

The core runtime should be organized as a chain of constrained transformations:

```text
player command
    ->
intent interpretation
    ->
fiction state update
    ->
constrained scene description
    ->
scene validation or simplification
    ->
raytraced rendering
    ->
image shown to the player
```

Each stage is both an implementation boundary and a research surface. The system should be able to inspect and debug failures at every transition.

## 9. World Model

The world model should be intentionally asymmetric:

- Some facts are hard state and must persist reliably.
- Some facts are soft state and may drift.

### 9.1 Hard State

Hard state should include:

- current turn number
- current location identifier
- inventory
- manipulable objects relevant to immediate action
- persistent named entities
- unresolved promises or threats
- recently established facts that directly affect interaction
- save metadata and model metadata

### 9.2 Soft State

Soft state may include:

- broader geography
- exact architectural continuity
- atmospheric details
- minor object placement
- historical explanations that are not mechanically relevant

This split is essential. The game should preserve what the player can act upon while allowing the larger world to remain unstable.

## 10. LLM Turn Contract

The LLM should not return raw prose alone. It should return structured turn data that can be validated before being accepted by the engine.

Minimum turn payload:

- player intent interpretation
- narrative response
- world-state deltas
- visible entities or salient objects
- optional clarification request
- scene description for rendering
- continuity notes or constraints when useful

An example target shape could be JSON or another grammar-constrained structure with fields such as:

- `intent`
- `narration`
- `result`
- `inventory_delta`
- `facts_added`
- `facts_removed`
- `entities_visible`
- `scene`
- `continuity_notes`

The exact schema is an implementation detail, but structured output is a product requirement because it is the only realistic way to keep the game playable, inspectable, and debuggable.

## 11. Scene Representation

### 11.1 Preferred v1 Format: Proprietary Primitive Scene Format

The preferred v1 approach is a very small proprietary scene description language built for LLM reliability and renderer simplicity.

Reasons:

- fewer tokens than `.OBJ`
- easier to validate
- easier to constrain with a grammar
- easier to keep scale and orientation consistent
- easier to render directly with analytic primitives
- easier to recover from malformed output

The format should support a minimal set of primitives:

- plane
- box
- sphere
- cylinder
- cone
- optional mesh reference

The format should support:

- position
- rotation
- scale or size
- grayscale material value
- roughness or diffuse category
- object name or tag

Illustrative example:

```text
room "service corridor"
box "wall_left"    pos(-4,1,0)   size(0.2,2.5,8)    gray(0.35)
box "wall_right"   pos(4,1,0)    size(0.2,2.5,8)    gray(0.38)
box "ceiling"      pos(0,2.6,0)  size(8,0.2,8)      gray(0.42)
plane "floor"      pos(0,0,0)    normal(0,1,0)      gray(0.18)
box "door"         pos(0,1,-3.6) size(1.1,2.1,0.15) gray(0.52)
cylinder "bucket"  pos(-2.8,0.3,1.2) radius(0.25) height(0.6) gray(0.48)
```

This format should remain intentionally narrow. If the LLM cannot describe a place with this vocabulary, the scene should be simplified rather than expanded into a general-purpose 3D standard.

### 11.2 Experimental v2 Format: LLM-Generated `.OBJ`

Direct `.OBJ` generation may be explored later for stranger or richer forms, but it should not be the primary production path for v1.

Risks of `.OBJ` as the main path:

- malformed geometry
- token-heavy output
- unstable scale
- broken normals or topology
- high validation cost
- poor determinism across similar prompts

Recommended role for `.OBJ`:

- optional research branch
- special-case hero scenes
- offline pre-generation experiments

## 12. Rendering Specification

The renderer should implement a deliberately rough grayscale raytraced aesthetic.

### 12.1 Visual Rules

- grayscale only
- no color as a core lighting signal
- one light source only
- light source locked to the camera
- high contrast where useful
- visible shadow noise
- low sample count by design
- geometry should read as stark and primitive

### 12.2 Lighting Model

The camera acts like a carried lamp or flashlight, evoking found-footage horror. This is not a realistic cinematographic setup; it is a stylistic one.

The renderer should support:

- direct illumination from the camera-mounted light
- raytraced indirect bounce or crude radiosity
- intentionally noisy sampling
- no aggressive temporal smoothing that would erase the aesthetic

### 12.3 Performance Direction

The renderer should aim for a low internal resolution and should prefer stable responsiveness over expensive accuracy.

Acceptable strategies:

- low fixed internal render resolution
- image upscaling in the UI
- progressive refinement only when idle
- hard caps on bounce depth and sample count

### 12.4 Inference-Time Refinement

A strong optional direction is to let the renderer accumulate samples while the language model is preparing its response. In that mode, the image would become a material trace of narrative computation time rather than a separate decorative output.

## 13. UI, Debug, and Presentation

The UI should remain minimal.

Required elements:

- text transcript
- command input line
- current rendered image
- save/load access
- settings for model path and performance profile

Optional but strongly recommended elements:

- command history
- transcript export
- debug panel for scene data and state inspection
- timing and telemetry panel
- raw structured turn output view

The interface should feel closer to a tool, terminal, or surveillance device than to a glossy game HUD.

## 14. Save, Load, and Session Logging

Save files should preserve enough information to continue play coherently.

They should include at least:

- transcript or transcript summary
- hard world state
- current scene description
- persistent entity state
- model identifier and relevant generation settings
- seed or sampling metadata if reproducibility is desired

The system should also support session logs suitable for later analysis, including:

- turn-by-turn timings
- validation errors
- fallback decisions
- scene outputs
- structured state deltas

Because LLM output is non-deterministic, save files and logs are both important. The system cannot rely on perfect regeneration from prompts alone.

## 15. Distribution Requirements

The project must be distributable without requiring the user to assemble a local AI stack manually.

Distribution goals:

- macOS build
- Windows build
- Linux build if practical
- no Python installation step
- no Ollama installation step
- no mandatory network dependency after setup

Possible delivery models:

- bundle executable plus model weights
- bundle executable plus first-run model downloader
- separate installer for executable and licensed model package

The final packaging strategy depends on model licensing and artifact size.

## 16. Prototype Scope Priorities

The full specification is broader than the first deliverable needs to be. For an initial public or research-facing slice, the project should prioritize a compact but stable vertical slice.

Minimum first-slice goals:

1. a playable session of roughly five to ten minutes
2. a local model integrated through `llama.cpp`
3. an open-text command loop
4. a minimal hard-state model
5. a primitive scene language
6. an identifiable grayscale raytracer
7. logging of responses, state changes, errors, and timing data

The priority is not a finished consumer product. The priority is an experience stable enough to test the hypothesis in practice.

## 17. Roadmap

### Phase 0: Feasibility and Constraints

Goals:

- confirm availability of `Ministral 3:14b` in a `llama.cpp`-compatible format
- confirm license terms for redistribution
- estimate practical memory and latency targets
- choose implementation language and packaging strategy

Exit criteria:

- model/runtime feasibility is confirmed
- distribution constraints are understood
- a v1 scene format is selected

### Phase 1: Vertical Slice

Goals:

- implement the parser-style turn loop
- establish hard versus soft state rules
- define the structured LLM turn contract
- implement primitive-scene generation and validation
- render a recognizable grayscale scene per turn
- log timing and failure data

Exit criteria:

- the player can type commands and receive coherent multi-turn responses
- most turns produce a valid render or a safe fallback
- timings and system behavior can be inspected after play

### Phase 2: Coherence and Research Readability

Goals:

- improve revisiting behavior for locations
- improve recurring entities and inventory consistency
- improve the relation between text, state, and image
- refine latency presentation and debug tooling
- evaluate which incoherences are productive

Exit criteria:

- the experience holds together across medium-length sessions
- contradictions feel interpretable rather than arbitrary
- the project can support analysis as well as play

### Phase 3: Distribution Build

Goals:

- package the game as a native app
- integrate model management
- support multiple performance profiles
- test offline installation and first-launch flow

Exit criteria:

- a non-technical player can install and run the game locally
- no Python or Ollama setup is required

### Phase 4: Experimental Geometry Expansion

Goals:

- test `.OBJ` generation as an optional path
- compare direct mesh generation against primitive scenes
- evaluate whether richer geometry improves the experience enough to justify the added instability

Exit criteria:

- either `.OBJ` remains experimental, or it proves sufficiently robust for limited use

## 18. Major Friction Points

### 18.1 Model Availability and Licensing

The biggest non-creative risk is whether `Ministral 3:14b` is actually available in a form that can be used with `llama.cpp` and legally redistributed with the game.

### 18.2 Local Performance

A 14B-class local model may still be heavy for a distributable art game, especially on lower-end machines. Latency, RAM pressure, load time, and package size are major concerns.

### 18.3 LLM as Parser

An LLM-driven parser is expressive, but it is also difficult to test. The system may misread commands, invent hidden affordances, or contradict earlier decisions.

### 18.4 Coherence Versus Hallucination

The concept benefits from instability, but too much instability breaks playability. The design challenge is deciding which layers may drift and which layers must remain hard.

### 18.5 Geometry Reliability

If the LLM generates visual data directly, malformed output is inevitable. The renderer pipeline must be defensive, validating or simplifying content rather than assuming correctness.

### 18.6 Style Becoming Mere Decoration

If the monochrome brutalist look is implemented as a superficial art direction rather than as the consequence of constrained computation, the conceptual core of the project weakens.

### 18.7 Debuggability

A combined system of inference, state summarization, structured output, validation, and rendering can fail in subtle ways. The project will need internal debugging views very early.

### 18.8 Distribution Size

Bundling a native app plus local model weights may produce a large download. This affects accessibility, platform support, and update strategy.

## 19. Research Questions and Evaluation

The prototype can be used to investigate questions such as:

1. What becomes of the game image when it is no longer the main site of computational expense?
2. Can an interactive fiction remain playable when its operational coherence is stable but its representational coherence drifts?
3. How do players interpret contradictions between narration, world state, and rendered image?
4. Does a poor geometric image stimulate stronger mental projection than a richer image?
5. How does inference latency reshape rhythm and expectation in parser-like play?
6. To what extent does the project reactivate a historical and affective continuity between video games, cracking culture, and the demoscene?
7. Can allocation of computation itself be treated as an aesthetic gesture?
8. Which parts of a world must remain stable for instability to be experienced as uncanny rather than broken?

Evaluation should combine:

- direct playtesting
- telemetry and timing analysis
- inspection of text/image mismatches
- qualitative notes about player interpretation

## 20. Recommended v1 Decisions

To keep the project achievable, v1 should commit to the following:

- use `llama.cpp`
- avoid Python and Ollama entirely
- use `Ministral 3:14b` if licensing and compatibility allow it
- treat the proprietary primitive scene format as the default rendering input
- keep `.OBJ` generation experimental
- preserve local continuity rather than global map consistency
- expose timing, validation, and debug data from the start
- design the renderer around grayscale, camera-light, and noisy radiosity from the start
- prioritize a research-grade vertical slice before full product ambitions

## 21. Open Questions

- Is `Ministral 3:14b` confirmed as a redistributable and `llama.cpp`-compatible target, or is that still a working assumption?
- Should the world be framed as a pure wandering machine, or should it have a stronger authored premise and win/lose conditions?
- How much of inventory and object interaction should be deterministic at the engine level versus entrusted to the LLM?
- Should revisiting a location reproduce a stable scene fingerprint, or should it visibly mutate?
- Should the first public target be a colloquium artifact, an exhibition prototype, or a downloadable art game?

## 22. Summary

The project should be treated as a local-first interactive fiction engine, a grayscale raytraced executable artwork, and a research apparatus for studying what happens when language becomes the expensive center of the machine and the image becomes its poor but material witness.

The most pragmatic v1 is not a general 3D world generator. It is a tightly constrained narrative system that emits simple geometry, renders it in noisy grayscale, exposes its computational distribution, and uses instability as part of the fiction rather than as an implementation accident.
