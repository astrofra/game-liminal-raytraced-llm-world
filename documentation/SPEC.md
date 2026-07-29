# Liminal Raytraced LLM World

## Product and Technical Specification

## 1. Premise

This project is an interactive fiction game that inverts the usual computational priority of video games.

Instead of spending most of the machine budget on high-fidelity graphics and using a fixed, pre-authored narrative, the game spends most of its computational intensity on local language-model inference. Visual rendering is intentionally austere: low-tech, grayscale, brutalist, liminal, and visibly synthetic.

The player interacts through a keyboard-first parser interface in the spirit of *Zork*, but command interpretation is handled by an LLM rather than by a closed deterministic verb parser. The result should feel like a dysfunctional but compelling game master: suggestive, unstable, locally coherent, and globally strange.

Each turn produces:

- a short narrative response
- an interpretation of the player's action
- an updated local world state
- a rendered grayscale image of the current place

The world is expected to feel only relatively stable. Places may reoccur, mutate, contradict themselves, or connect with weak topology. This is a feature, not a defect, as long as short-term interaction remains legible.

## 2. Core Vision

The project should deliver a playable prototype where:

- the fiction is generated locally by an LLM
- the game is distributable as a native application
- no Python runtime is required
- no Ollama dependency is required
- `llama.cpp` is the preferred inference backend
- each location can be rendered from a minimal scene description or generated geometry
- the visual identity is grayscale raytraced found-footage horror rather than modern real-time realism

## 3. Design Pillars

### 3.1 Narrative First

The narrative layer is the main computational center of the game. The visual layer exists to materialize the fiction, not to dominate it.

### 3.2 Low-Tech Visuals

The rendering should look constrained, noisy, geometric, and deliberate. It should not attempt photorealism.

### 3.3 Liminal Instability

The world should feel plausible at the sentence level and unstable at the map level. Continuity is local, not global.

### 3.4 Distributable Local Runtime

The game should run as a self-contained desktop application with local inference. Any install flow must avoid requiring the user to manually set up Python or Ollama.

### 3.5 Open Language Interaction

Player input should remain open-ended. The system should absorb phrasing variations, ambiguity, and unconventional wording without relying on a strict verb whitelist.

## 4. Non-Goals

The v1 prototype is not trying to provide:

- photorealistic rendering
- a fully deterministic simulation
- a large handcrafted map
- a conventional puzzle-logic parser game
- online multiplayer
- cloud inference as a requirement

## 5. Target Experience

The intended player experience is:

1. The player types a command such as `open the metal door`, `listen`, `go back`, or `ask the woman what she lost`.
2. The LLM interprets the intention and decides what happens.
3. The game returns short prose, updates the local fiction state, and shows a grayscale render of the place.
4. The player continues exploring a chain of spaces that feel haunted, partial, and statistically improvised.

Tone targets:

- uncanny
- austere
- oppressive
- dreamlike but readable
- occasionally contradictory

The game should avoid classic parser dead ends such as repeated `I don't understand that verb`. Even failure should feel diegetic, for example: the action is ignored by the world, misunderstood by the narrator, or transformed into another plausible action.

## 6. Functional Requirements

### 6.1 Input and Interaction

- The game must support free-text keyboard input.
- The game must preserve a parser-like cadence: one player command, one world response.
- The LLM must interpret intent, not just surface verbs.
- The system must support basic IF actions such as movement, inspection, manipulation, dialogue, and inventory-related behavior.
- The system must be able to ask for clarification when an input is too ambiguous.

### 6.2 Narrative Generation

- The game must use `Ministral 3:14b` as the target model.
- The game must generate short-form narrative output per turn.
- The game must maintain enough state for local continuity across turns.
- The game must support recurring locations, recurring objects, and recurring NPC references.
- The game should favor concise prose over long paragraphs in moment-to-moment play.

### 6.3 Scene Generation

For each location or major state change, the system must produce a renderable scene description.

Two content-generation modes are allowed:

- Preferred mode for v1: a simple proprietary scene format describing primitives, transforms, and basic material properties.
- Experimental mode: `.OBJ` generation directly from the LLM.

### 6.4 Rendering

- The renderer must produce grayscale output.
- Lighting must use a single light source attached to the camera, like a harsh found-footage lamp.
- The renderer must include raytraced radiosity or diffuse bounce, but at intentionally low sample counts so the result remains very noisy.
- The final image should preserve noise rather than denoise it away.
- The renderer should prioritize mood, legibility, and speed over accuracy.

### 6.5 Runtime and Distribution

- The game must be distributable as a native desktop build.
- The runtime should use `llama.cpp` rather than Python-based stacks.
- The game should not require Ollama.
- The game should run offline after installation.
- Save/load support is required.

## 7. Recommended Technical Direction

### 7.1 Application Stack

Preferred direction:

- native desktop executable
- `llama.cpp` embedded or linked directly
- renderer implemented in the same native runtime
- simple UI layer for text input, transcript view, and image display

Pragmatic implementation preference:

- C++ is the most direct fit because of `llama.cpp` integration and renderer ownership.
- Rust is acceptable if the project wants safer application code and is willing to manage FFI boundaries cleanly.

The main requirement is not the language itself, but avoiding a runtime architecture that depends on Python services or external model daemons.

### 7.2 Model Runtime

The system should assume:

- a `llama.cpp`-compatible model artifact is available for `Ministral 3:14b`
- quantized local inference is required for practical distribution
- hardware acceleration should be optional but supported when available

Practical backend expectations:

- CPU-only execution must remain possible, even if slower
- Metal, CUDA, or Vulkan acceleration should be used opportunistically
- the application must handle model loading, prompt assembly, and inference directly

## 8. World Model

The world model should be intentionally asymmetric:

- Some facts are hard state and must persist reliably.
- Some facts are soft state and may drift.

### 8.1 Hard State

Hard state should include:

- current turn number
- current location identifier
- inventory
- persistent named entities
- unresolved promises or threats
- recently established facts that directly affect current interaction
- save metadata and model metadata

### 8.2 Soft State

Soft state may include:

- broader geography
- exact architectural continuity
- atmospheric details
- minor object placement
- historical explanations that are not mechanically relevant

This split is essential. The game should preserve what the player can act upon, while allowing the larger world to remain unstable.

## 9. LLM Turn Contract

The LLM should not return raw prose alone. It should return structured turn data that can be validated before being accepted by the engine.

Minimum turn payload:

- player intent interpretation
- narrative response
- world-state deltas
- visible entities or salient objects
- optional clarification request
- scene description for rendering

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

The exact schema is an implementation detail, but structured output is a product requirement because it is the only realistic way to keep the game playable and debuggable.

## 10. Scene Representation

### 10.1 Preferred v1 Format: Proprietary Primitive Scene Format

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
box "wall_left"    pos(-4,1,0)  size(0.2,2.5,8) gray(0.35)
box "wall_right"   pos( 4,1,0)  size(0.2,2.5,8) gray(0.38)
box "ceiling"      pos(0,2.6,0) size(8,0.2,8)   gray(0.42)
plane "floor"      pos(0,0,0)   normal(0,1,0)   gray(0.18)
box "door"         pos(0,1,-3.6) size(1.1,2.1,0.15) gray(0.52)
cylinder "bucket"  pos(-2.8,0.3,1.2) radius(0.25) height(0.6) gray(0.48)
```

This format should remain intentionally narrow. If the LLM cannot describe a place with this vocabulary, the scene should be simplified rather than expanded into a general-purpose 3D standard.

### 10.2 Experimental v2 Format: LLM-Generated `.OBJ`

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

## 11. Rendering Specification

The renderer should implement a deliberately rough grayscale raytraced aesthetic.

### 11.1 Visual Rules

- grayscale only
- no color as a core lighting signal
- one light source only
- light source locked to the camera
- high contrast where useful
- visible shadow noise
- low sample count by design
- geometry should read as stark and primitive

### 11.2 Lighting Model

The camera acts like a carried lamp or flashlight, evoking found-footage horror. This is not a realistic cinematographic setup; it is a stylistic one.

The renderer should support:

- direct illumination from the camera-mounted light
- raytraced indirect bounce or crude radiosity
- intentionally noisy sampling
- no aggressive temporal smoothing that would erase the aesthetic

### 11.3 Performance Direction

The renderer should aim for a low internal resolution and should prefer stable responsiveness over expensive accuracy.

Acceptable strategies:

- low fixed internal render resolution
- image upscaling in the UI
- progressive refinement only when idle
- hard caps on bounce depth and sample count

## 12. UI and Presentation

The UI should remain minimal.

Required elements:

- text transcript
- command input line
- current rendered image
- save/load access
- settings for model path and performance profile

Optional elements:

- command history
- transcript export
- debug panel for scene data and state inspection

The interface should feel closer to a tool, terminal, or surveillance device than to a glossy game HUD.

## 13. Save/Load Requirements

Save files should preserve enough information to continue play coherently.

They should include at least:

- transcript or transcript summary
- hard world state
- current scene description
- persistent entity state
- model identifier and relevant generation settings
- seed or sampling metadata if reproducibility is desired

Because LLM output is non-deterministic, save files are especially important. The system cannot rely on perfect regeneration from a prompt alone.

## 14. Distribution Requirements

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

## 15. Roadmap

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

### Phase 1: Text-Only Vertical Slice

Goals:

- implement parser-style turn loop without graphics
- establish hard vs soft state rules
- define structured LLM turn contract
- validate that the model can sustain short-term continuity

Exit criteria:

- the player can type commands and receive coherent multi-turn responses
- saves can restore a session
- the game avoids repeated parser-style dead ends

### Phase 2: Primitive Scene Pipeline

Goals:

- introduce the proprietary primitive scene format
- generate one scene description per turn
- build a grayscale raytracer with camera-mounted light
- render noisy indirect light in a controllable way

Exit criteria:

- most turns can produce a valid render
- the renderer has a recognizable aesthetic identity
- invalid scene outputs can be recovered safely

### Phase 3: Coherence and Authorial Control

Goals:

- improve revisiting behavior for locations
- improve recurring entities and inventory consistency
- add prompt controls for tone, pacing, and prose length
- create author tools for scenario framing and constraints

Exit criteria:

- the game holds together across medium-length sessions
- narrative drift remains artistically interesting rather than merely broken

### Phase 4: Distribution Build

Goals:

- package the game as a native app
- integrate model management
- support multiple performance profiles
- test offline installation and first launch flow

Exit criteria:

- a non-technical player can install and run the game locally
- no Python or Ollama setup is required

### Phase 5: Experimental Geometry Expansion

Goals:

- test `.OBJ` generation as an optional path
- compare direct mesh generation against primitive scenes
- evaluate whether richer geometry improves the experience enough to justify the added instability

Exit criteria:

- either `.OBJ` remains experimental, or it proves sufficiently robust for limited use

## 16. Major Friction Points

### 16.1 Model Availability and Licensing

The biggest non-creative risk is whether `Ministral 3:14b` is actually available in a form that can be used with `llama.cpp` and legally redistributed with the game.

If this cannot be guaranteed, the whole distribution strategy changes.

### 16.2 Local Performance

A 14B-class local model may still be heavy for a distributable art game, especially on lower-end machines. Latency, RAM pressure, load time, and package size are major concerns.

### 16.3 LLM as Parser

An LLM-driven parser is expressive, but it is also difficult to test. The system may misread commands, invent hidden affordances, or contradict earlier decisions.

### 16.4 Coherence vs Hallucination

The concept benefits from instability, but too much instability breaks playability. The design challenge is deciding which layers may drift and which layers must remain hard.

### 16.5 Geometry Reliability

If the LLM generates visual data directly, malformed output is inevitable. The renderer pipeline must be defensive, validating or simplifying content rather than assuming correctness.

### 16.6 Debuggability

A combined system of inference, state summarization, structured output, and rendering can fail in subtle ways. The project will need internal debugging views very early.

### 16.7 Distribution Size

Bundling a native app plus local model weights may produce a large download. This affects accessibility, platform support, and update strategy.

## 17. Recommended v1 Decisions

To keep the project achievable, v1 should commit to the following:

- use `llama.cpp`
- avoid Python and Ollama entirely
- use `Ministral 3:14b` if licensing and compatibility allow it
- treat the proprietary primitive scene format as the default rendering input
- keep `.OBJ` generation experimental
- preserve local continuity, not global map consistency
- design the renderer around grayscale, camera-light, and noisy radiosity from the start

## 18. Open Questions

- Is `Ministral 3:14b` confirmed as a redistributable and `llama.cpp`-compatible target, or is that still a working assumption?
- Should the world be framed as a pure wandering machine, or should it have a stronger authored premise and win/lose conditions?
- How much of inventory and object interaction should be deterministic at the engine level versus entrusted to the LLM?
- Should revisiting a location reproduce a stable scene fingerprint, or should it visibly mutate?
- Is the shipping target a gallery/exhibition prototype, a downloadable art game, or a more conventional consumer release?

## 19. Summary

The project should be treated as a local-first interactive fiction engine where the LLM is the main stage and the renderer is a stark, low-tech witness. The most pragmatic v1 is not a general 3D content generator; it is a tightly constrained narrative system that emits simple geometry, renders it in noisy grayscale, and uses instability as part of the fiction.
