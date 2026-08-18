# Project Reorientation Brief for Codex
## From *Le Désert des tokens* to an Eryx-based generative labyrinth

**Purpose:** This document is an editorial and conceptual instruction for reconfiguring the documentary material of `astrofra/game-liminal-raytraced-llm-world`.

**Primary task:** Rewrite the project documentation so that the current datacenter/desert narrative is replaced by a new artistic and research direction built around the lineage of *In the Walls of Eryx*, ASD's *Beyond the Walls of Eryx*, Mandarine's *Within the Mesh*, and the present LLM-driven interactive work.

**Important:** This is primarily a **documentation-first reorientation**. Do not use this brief as an excuse to redesign working code indiscriminately. Preserve implemented technical facts, file paths, test results, performance measurements, rendering architecture, and runtime behavior unless a later code task explicitly changes them.

---

# 1. Why the project is being reoriented

The current project grew out of a strong technical and artistic premise:

> What if video games had spent their computational power on narrative plasticity rather than on graphical sophistication?

That premise remains valid and should remain central.

The current implementation already has several strong elements: local Ministral inference through `llama.cpp`, a native C++ runtime, a deliberately simple grayscale CPU raytracer, a player-facing parser interface, an authoritative state outside the model, an intermediate spatial state, a deterministic compilation path from spatial state to renderable scene, intentionally weak global topology, room generation and caching, and a visual language based on simple geometry, low information density, and visible technical limits.

The problem is not the technical architecture. The problem is the **narrative and research anchoring**.

The previous story, *Le Désert des tokens*, imagined an officer posted in an autonomous datacenter in the desert, waiting for an undefined massive cyberattack. The datacenter heated its own environment, while the fictional desert temperature influenced the LLM sampling temperature. This created an elegant loop between computational activity, energy, heat, ecological absurdity, and increasingly unstable language.

That concept remains artistically interesting, but it has one major weakness in the context of the author's PhD research: the connection to the demoscene is indirect. It depends mostly on the visual language of constraint and on the broader idea of expressive computational allocation.

The Eryx reorientation solves this problem by making the demoscene lineage **historical, personal, explicit, and structurally embedded in the work**.

---

# 2. The new four-stage lineage

## Stage 1 — Literature: *In the Walls of Eryx*

H. P. Lovecraft and Kenneth J. Sterling's *In the Walls of Eryx* is a science-fiction story set on Venus. Its protagonist is a prospector working for an extractive company. He encounters a mysterious structure whose walls are invisible, enters it, becomes trapped in a maze, and gradually loses his ability to determine how the space is organized.

The important motifs for this project are Venus as an extractive frontier, valuable energy-bearing crystals, exploration and prospecting, an invisible architectural structure, a maze that resists mapping, uncertainty over whether the labyrinth's configuration is stable, progressive collapse of spatial certainty, and the gap between what the protagonist believes, what he can perceive, and what the environment actually is.

The story should be treated as a **literary source and conceptual matrix**, not as a screenplay to reproduce scene by scene.

## Stage 2 — Demoscene: ASD, *Beyond the Walls of Eryx* (2007)

ASD's *Beyond the Walls of Eryx* is the first crucial demoscene transformation of this material.

Navis explicitly stated that the Lovecraft/Sterling story was one of the inspirations for the demo. He connected the invisible maze and its sense of hopelessness to another formative reference, the C64 game *Scarabaeus*. The resulting work translated this literary and game-derived feeling into a demoscene audiovisual language based on lines, parallel-projection space, linked worlds, transformation, collapse, and the breakdown of space.

This is not a superficial reference. It is evidence that the motif of Eryx had already migrated from literature into the demoscene as an **aesthetic and computational reinterpretation**.

Reference: https://www.pouet.net/prod.php?which=31088

The production was released by Andromeda Software Development in June 2007, ranked third in the Intel Demo Competition, and was later nominated for Scene.org Awards in direction and original concept.

## Stage 3 — Personal demoscene lineage: Mandarine, *Within the Mesh* (2013)

The project's author, François / `fra`, later co-created *Within the Mesh* with Mandarine.

Pouet explicitly records *Within the Mesh* as related to *Beyond the Walls of Eryx*. The demo was released at Evoke 2013, ranked third in the PC demo competition, and credits `fra` with design and high-level scripting.

Reference: https://www.pouet.net/prod.php?which=61730

This is essential to the new project positioning.

The Eryx lineage is not being introduced opportunistically in 2026. It already passed through the author's own demoscene practice more than a decade ago.

The new work therefore emerges from a **lived artistic genealogy**:

```text
In the Walls of Eryx
        ↓
Beyond the Walls of Eryx
        ↓
Within the Mesh
        ↓
current interactive LLM work
```

## Stage 4 — 2026: the labyrinth becomes interactive and generative

The present work introduces a new transformation that the previous stages could not provide.

The player does not merely watch a representation of the labyrinth.

The player **inhabits a computational labyrinth whose topology can actually change**.

The LLM is no longer only a narrator. It participates in the production of spatial uncertainty.

The central conceptual move is:

> The invisible and possibly changing labyrinth of Eryx becomes a generative spatial system in which the LLM can continually reconfigure the player's field of movement.

This makes the known weakness of LLM-based world simulation — imperfect global spatial continuity — artistically productive.

The model's difficulty in maintaining a perfectly stable global topology is not merely tolerated. It becomes part of the ontology of the fictional world.

---

# 3. The new central artistic proposition

The revised project should no longer be primarily framed as an ecological datacenter allegory.

Its new core proposition is:

> Build an interactive, locally coherent but globally unstable world in which the player explores a Venusian extraction zone and encounters an invisible labyrinth whose spatial organization is continually re-negotiated by the generative system.

The work should preserve the project's original counterfactual question about computational allocation:

> What becomes of the video-game image when computational expense moves away from graphical sophistication and toward inference, interpretation, and narrative/world generation?

The answer is now made concrete through Eryx. The GPU is used primarily to run the LLM, the CPU produces a deliberately austere raytraced image, the text can imply a world larger than the renderer shows, the image remains partial and instrumental, and spatial instability becomes fiction rather than a purely technical defect.

The new work should be understood as an **interactive continuation of a demoscene lineage**, not as a retro game that happens to reference a demo.

---

# 4. What should be preserved from *Le Désert des tokens*

Do not erase the intellectual history of the previous concept from the project journal and research notes. It was a legitimate stage of the research.

Preserve the following principles: inversion of computational load; local LLM inference as the expensive computational center; deliberately inexpensive image generation; visual poverty as a consequence of constraint rather than a retro filter; explicit visibility of computation and latency; local coherence combined with global drift; the project as a research instrument rather than a thesis illustration; a native executable and direct technical ownership; the possibility of an autonomous ghost-player version for exhibition or demoparty presentation; the idea that the machine's own computation can become aesthetically meaningful; and the relationship between text and image as partial, asymmetric mediation.

Supersede as main fiction: the autonomous military datacenter, the officer waiting for a cyberattack, cyber-threat interpretation as the main narrative engine, desert heat caused by server infrastructure, water consumption of the datacenter as the primary dramatic resource, institutional paranoia as the main theme, and the direct fictional equation "desert temperature → LLM temperature" as the central mechanic.

Do not delete all traces of these ideas from historical notes. Mark them as an earlier narrative branch where appropriate.

The title *Le Désert des tokens* should be treated as a **previous working title**, not silently rewritten out of the project history.

Do not invent a final new title unless one has been explicitly chosen by the author.

---

# 5. What can be retained from the temperature idea

The old temperature mechanism should not automatically be imported wholesale into Eryx.

Its conceptual strength was the coupling of physical temperature, machine temperature, and model sampling temperature. That remains an interesting research mechanism, but it should now be considered **optional and subordinate**.

If retained, it must serve Eryx rather than distort Eryx into the old datacenter story.

Possible reinterpretations include environmental stress on Venus, oxygen depletion, suit stress, exposure time, psychological disorientation, proximity to the labyrinth's interior, or a "spatial entropy" / confidence-loss value mapped to sampling variation.

Do not make LLM temperature the sole source of spatial change. Randomness is not the same thing as meaningful instability.

The authoritative world state should remain outside the model. The LLM may **propose structured spatial mutations**, which are then validated and committed by the engine.

---

# 6. The invisible labyrinth as the new spatial model

This is the most important conceptual and technical consequence of the reorientation.

The current project already separates:

```text
game state
    ↓
LLM turn result
    ↓
intermediate spatial state
    ↓
deterministic scene compilation
    ↓
render
```

Keep this architecture.

Do not return to unconstrained `.scene` generation.

Instead, reinterpret the spatial layer so it can express an invisible and mutable labyrinth.

The LLM should be allowed to propose changes such as an exit that existed previously becoming blocked, a corridor reconnecting to a different known location, a path returning to a location from an impossible direction, a newly discovered invisible barrier dividing an apparently open area, a known barrier disappearing, a room or open quarry acquiring a hidden substructure, a previously mapped path contradicting the player's map, or the player returning to a location whose local geometry remains recognizable while its connectivity has changed.

The engine should decide which mutations are accepted.

This preserves debuggability while giving the LLM genuine authorship over the labyrinth.

## Hard state versus soft topology

Continue to distinguish between facts that must remain stable and facts that may drift.

Hard state can include player inventory, survival resources if used, collected objects, key discoveries, persistent named entities, major narrative commitments, and the player's current physical position in the authoritative graph.

Soft or mutable spatial state can include adjacency relationships, corridor continuity, unseen barriers, route length, orientation, inferred geometry beyond the current local view, and whether two locally coherent spaces remain globally compatible.

The research question becomes sharper:

> What must remain stable for a changing maze to feel uncanny rather than merely broken?

---

# 7. The walls should not simply become visible geometry

Do not solve the invisible labyrinth by introducing an ordinary visible `prefab_invisible_wall` rendered as a wall.

The whole point is the mismatch between perception and collision/topology.

Invisible walls belong primarily to spatial semantics, collision or traversal constraints, narrative feedback, and optional diagnostic visualization.

The player may encounter a wall because an attempted movement fails even though the raytraced viewport appears open.

Possible indirect visual evidence includes dust or vapor stopping at an invisible plane, a scanner trace, a lighting discontinuity, a faint diagnostic contour, an object apparently suspended against empty space, or a shadow/reflection implying a surface the camera cannot directly see.

These effects can be added incrementally. Do not require them all in v1.

The text/image relationship is crucial:

> The text may know that there is a wall where the image shows nothing.

This is not a failure of illustration. It is the work's central perceptual tension.

---

# 8. New environmental direction: Venus, extraction, quarry, industrial brutalism

One reason for the reorientation is to give the brutalist visual language a stronger fictional justification.

The Eryx setting gives the visual language a more concrete world: Venusian extraction zone, human prospecting infrastructure, quarry or mining field, survey installations, industrial shelters, excavation pits, drilling machinery, cargo handling, navigation beacons, and alien architecture that may be invisible.

Do **not** interpret this as a demand for a historically or scientifically realistic Venus.

The literary Eryx is itself a speculative Venus. The project may construct its own mediated Venus.

The intended visual synthesis is:

> human brutalist extraction infrastructure + sparse hostile planetary environment + invisible alien topology.

This allows the current renderer's strengths to become purposeful. Boxes and slabs become extraction architecture, planes become quarry floors and plateaus, narrow openings become service passages, repeated modules become industrial systems, grayscale becomes instrumental vision, raytracing noise becomes incomplete sensing, and sparse scenes create a strong relation between visible and invisible space.

---

# 9. Prefab redesign

The current prefab catalog is strongly tied to the datacenter story, with elements such as racks and cooling units.

Rework the semantic catalog around the new setting.

Do not assume every existing prefab must be deleted. Reuse geometry when it can be reinterpreted convincingly.

Suggested direction:

- keep `prefab_gate`, reinterpreted as a mining perimeter gate, pressure threshold, survey checkpoint, or labyrinth access marker;
- keep `prefab_crate`, reinterpreted as prospecting cargo, sample container, oxygen supply case, or mining equipment;
- de-emphasize or repurpose `prefab_rack` as field instrumentation / survey electronics;
- replace or reinterpret `prefab_cooling_unit` as atmospheric processing, oxygen service, drilling support, pump, or extraction equipment;
- add a survey beacon / navigation mast;
- add a crystal scanner / sample instrument;
- add a crystal cluster or specimen;
- add a mining drill / extraction rig;
- add a shelter / prospecting station module;
- add a quarry marker / industrial pylon;
- optionally add a dead or abandoned prospector proxy only if it can be represented coherently with the renderer's primitive vocabulary.

Do not create large numbers of decorative assets.

The prefab system must remain consistent with the project's aesthetic of **high semantic value per geometric element**.

After real prefab geometry is implemented, regenerate the visual catalog from source. Do not manually fake generated catalog images.

---

# 10. The visual lineage must become more specific

The existing moodboard remains valuable and should not be discarded.

It already identifies the important principle:

> low-fi worlds are incomplete but operational.

Keep the references to early games and demoscene works that establish the author's situated visual education: *Driller*, *The Sentinel*, *Tau Ceti*, *Midwinter*, *The Pawn*, *Captain Blood*, *Drakkhen*, *Enigma* by Phenomena, *Origin Complex*, *Substance*, *Hardwired*, and *Amiga Desert Dreams*.

However, reorganize the moodboard so the Eryx lineage becomes a first-class section rather than an incidental demoscene-adjacency argument.

The new moodboard logic should distinguish three layers:

**Foundational microcomputer perception:** a world can be suggested through sparse geometry, interface, text, and omission.

**Demoscene transformation:** the demo does not only depict space; it makes computed space itself an event. *Beyond the Walls of Eryx* must become a major reference here. *Within the Mesh* must be documented as the author's own later continuation of that reference.

**Present generative transformation:** the new work turns unstable space into something the player can inhabit.

The visual lineage therefore moves from:

```text
suggested space
→ performed computed space
→ navigated demoscene space
→ generatively reconfigured interactive space
```

Do not describe the project as "retro". Prefer formulations such as contemporary translation of constraint-based expressivity, situated computational aesthetic, inherited visual grammar of incomplete space, low-information but high-tension image, machine-mediated perception, and partial visibility as active design.

---

# 11. The demoscene connection is now a central research axis

The documentation should stop treating the project as merely "demoscene-adjacent."

The new work has a direct demoscene genealogy.

Do not claim that the current interactive work is automatically a canonical demo.

Instead, make the more defensible claim:

> The work is a new transformation of a motif that has already circulated through demoscene history and through the author's own demoscene practice.

The PhD relevance now comes from several simultaneous layers.

**Historical lineage:** a literary motif enters the demoscene through ASD.

**Personal lineage:** the author previously reinterpreted that demoscene work in *Within the Mesh*.

**Technical lineage:** the current project preserves demoscene values of direct control of the technical stack, real-time execution, visible computation, platform-conscious design, voluntary constraint, and transformation of technical limits into aesthetics.

**Mediation:** the project can show how the same motif changes as it crosses media and computational regimes.

The central mediation question becomes:

> What is preserved, lost, or newly made possible when the same spatial motif moves from literature, to demo, to another demo, to interactive LLM-driven world generation?

This is much stronger than merely placing demoscene screenshots next to a contemporary AI artwork.

---

# 12. The role of AI becomes more precise

The AI must not be described merely as a content generator.

Its most important role in the new project is **spatial mediation and destabilization**.

The LLM interprets free-form player commands, narrates local consequences, updates selected world-state facts, proposes spatial changes, creates local descriptions that remain actionable, and may introduce contradictions at the level of the global map.

This provides a strong conceptual correspondence between the source motif and the model's actual computational behavior.

A weakness of generative models becomes an artistic property:

> incomplete long-range consistency becomes a changing labyrinth.

Avoid presenting this as "the AI hallucinates, therefore the maze changes."

Prefer:

> The system deliberately grants the generative model controlled authority over selected topological relations, while preserving an authoritative external state for playability.

This distinction matters technically and academically.

---

# 13. The project is not a literary-quality experiment

Retain the existing position.

The work is not intended to prove that a language model writes better fiction than a human writer.

The literary source gives the project a recognizable narrative and conceptual lineage, but the research focus remains on computational allocation, generated world structure, image/text mediation, spatial instability, real-time execution, the transformation of technical limits into aesthetic form, and the continuation of demoscene practices in an AI-era work.

The prose should remain concise enough that the player is not forced to read a novel between rendered frames.

---

# 14. Reframing the brutalist aesthetic

Keep grayscale, noise, severity, and liminality.

Refine "brutalist" so it is no longer a vague stylistic adjective.

It should now derive from extraction infrastructure, quarry geometry, slabs, retaining walls, trenches, ramps, service shelters, industrial repetition, instrument housings, sparse planetary terrain, and an alien architecture that cannot be directly seen.

The visual world should feel **constructed, excavated, surveyed, and partially unreadable**.

Do not turn the game into generic cyberpunk, datacenter horror, or NASA realism.

---

# 15. The autonomous / ghost-player version remains relevant

Keep the idea of a self-running version.

For an exhibition or demoparty presentation, a ghost player can execute a controlled series of commands while the LLM generates the resulting world state and narrative live.

For Eryx, the ghost player can repeatedly survey, mark, move, retrace, compare, attempt to return, discover a contradiction, remap, and continue.

The performance can make spatial failure increasingly visible.

This is preferable to a naive mode in which the LLM talks endlessly to itself.

The ghost-player script provides dramatic structure, while the generated topology preserves uncertainty.

---

# 16. Keep the computational-constraint mediation work

The current research on computational constraint and Amiga DMA visualization remains fully relevant.

Do not remove it from `NOTES.md` or the broader research positioning.

The WinUAE DMA visualizer provides a concrete way to explain that "old machines were limited" is an insufficient description.

On Amiga, the visualizer can expose how CPU, Copper, Blitter, bitplane DMA, sprites, audio, and other accesses compete for or occupy memory-bus time.

The mediation can compare a relatively idle Workbench, a simple window operation, and a carefully selected demo sequence using multiple hardware subsystems.

This makes constraint visible as **allocation**, not merely lack of performance.

The contemporary project then proposes another allocation regime:

```text
Amiga-era demo:
hardware resources → coordinated audiovisual output

current project:
GPU compute → LLM inference and world interpretation
CPU compute → deliberately constrained raytraced image
```

Do not claim architectural equivalence between an Amiga bus and a modern CPU/GPU system.

The comparison is conceptual:

> In both cases, aesthetic form is inseparable from how computational resources are allocated.

This remains one of the strongest mediation devices connecting the artwork to the PhD research.

---

# 17. Documentation files to update

Perform a coherent pass over the whole documentation tree.

## `documentation/STORY.md`

This file requires the largest rewrite.

Replace the datacenter/desert story with the Eryx-based project direction. Explain the Venus/extraction context, invisible labyrinth, possibility of changing topology, player/prospector role, relationship between visible human infrastructure and invisible alien space, resource or orientation pressure if useful, and the four-stage lineage from story to ASD to Mandarine to the current work.

Do not reproduce the original short story's prose or copy its plot mechanically.

## `documentation/SPEC.md`

Preserve the core counterfactual premise and technical architecture.

Rewrite the situated artistic lineage, demoscene positioning, target experience, story-facing examples, and any sections that make "demoscene adjacency" sound weak or incidental.

The new spec should explicitly state that the LLM-driven topology is the contemporary transformation of the Eryx labyrinth.

Change "Demoscene Adjacency" to a stronger but still precise framing such as **Demoscene Lineage and Continuation**.

## `documentation/AMBIANCE_MOODBOARD.md`

Do not throw away the existing analysis.

Reframe it around Venusian prospecting/extraction, invisible topology, quarry/mining brutalism, sparse operational space, and the direct visual lineage of *Beyond the Walls of Eryx* and *Within the Mesh*.

Retain the argument that low-fi aesthetics are not a retro skin.

## `documentation/NOTES.md`

Preserve historical thought rather than rewriting it as if the datacenter phase never existed.

Add a new dated section explaining the reorientation.

Clearly label the *Désert des tokens* branch as a previous conceptual stage.

Add the four-stage genealogy and the reasoning behind the shift.

Keep the computational-constraint/DMA mediation notes.

## `documentation/FUNCTIONAL_PIPELINE_V1.md`

Do not rewrite the architecture.

Update semantic examples and clarify that the spatial layer may represent invisible barriers, the LLM may propose topological mutations, mutations must be validated, authoritative state remains outside the model, local renderability/playability remain hard requirements, and global topology is intentionally allowed to drift within controlled limits.

## `documentation/SPATIAL_VALIDATION_PLAN.md`

Make this a major document in the new concept.

Validation should explicitly test whether invisible barriers remain actionable, whether the player can distinguish intentional spatial contradiction from parser failure, how often topology can mutate before frustration dominates, whether revisited places remain locally recognizable, whether the system can generate impossible return paths without corrupting hard state, and how mapping attempts reveal or fail to reveal the labyrinth.

## `documentation/HYBRID_SCENE_LAYOUT_PLAN.md`

Reinterpret layouts for quarry, extraction field, Venusian plateau, industrial service zone, prospecting shelter, scanner station, and labyrinth threshold.

Do not turn the invisible maze into visible corridors everywhere.

## `documentation/PREFAB_CATALOG.md`

This is generated documentation.

Update semantic descriptions only when source prefabs have actually changed.

Plan the new catalog and record required prefab work, but do not fabricate render outputs.

After new prefabs exist, regenerate the catalog using the existing generation pipeline.

## `documentation/DECISIONS.md`

Append a dated decision documenting the narrative reorientation, including the reason for abandoning the datacenter as main fiction, the direct demoscene lineage, Eryx labyrinth as model for controlled topological instability, the decision to preserve technical architecture, and the decision to reinterpret rather than erase prior research.

## `documentation/PROJECT_JOURNAL.md`

Append a chronological entry.

Do not rewrite history.

Describe the datacenter concept as the immediately preceding phase and explain why the Eryx lineage is a stronger fit for the thesis.

## `documentation/TECHNICAL_STATE.md`

Only change factual statements if necessary.

Do not transform conceptual intent into an implemented feature.

If invisible barriers are not implemented yet, list them as planned rather than current.

## `documentation/KNOWN_ISSUES.md`

Add transitional issues such as datacenter-specific prefab semantics remaining in the current build, runtime names/scripts still referencing `desert_des_tokens`, current prompts still encoding datacenter concepts, current spatial state not yet explicitly distinguishing invisible barriers from ordinary blocked exits, and the prefab catalog not yet being Eryx-specific.

Remove an issue only when actually resolved in code.

## `documentation/SCENE_FORMAT_V1.md`

Do not change this file merely for narrative reasons.

Only update it if a real technical representation for invisible barriers, diagnostic overlays, or new prefab directives is implemented.

## `documentation/README.md`

Update index descriptions where they become misleading. Preserve maintenance rules.

## Root `README.md`

Update the high-level "About" and project description after the documentation has converged.

Do not claim features not yet implemented.

Superseded on 2026-08-18: the dedicated title refactor renamed the active helper to `play_within_the_latent_walls.bat`. The former filename belongs only to repository history.

## Generated PDFs

Do not hand-edit PDF exports.

Update Markdown source first, then regenerate PDFs using the project's existing workflow if available.

---

# 18. Documentation editing rules

1. Do not falsify project history. Earlier research branches belong in notes and journal history.
2. Separate implemented facts from intentions. If an Eryx feature is only planned, say so.
3. Do not invent benchmark results.
4. Do not silently change technical architecture.
5. Do not remove useful generic research because its examples came from the datacenter phase. Reframe it.
6. Do not overuse Lovecraftian vocabulary or generic cosmic-horror clichés. The project is about unstable computed space, not fan-fiction atmosphere.
7. Do not turn the Eryx references into decorative lore. The lineage must explain the work structurally.
8. Do not make the invisible maze a conventional visible maze.
9. Do not reduce the AI contribution to prose generation. Its controlled influence on topology is central.
10. Do not reduce the demoscene connection to visual nostalgia. It is historical, personal, technical, and medial.
11. Keep code comments in English.
12. Prefer concise, technical English for new documentation unless an existing file is intentionally maintained in French. Do not randomly mix languages inside the same section.

---

# 19. Copyright / derivative-work stance for this redesign

Do not turn the documentation migration into a copyright-avoidance exercise.

For the purposes of this project direction, the author considers the work a unique artistic derivative / reinterpretative installation rather than a mass-market textual adaptation, and accepts that artistic positioning as part of the project.

Codex should therefore **not weaken the conceptual lineage merely to avoid naming the source works**.

At the same time, do not reproduce substantial passages from *In the Walls of Eryx*, do not copy prose from the short story, do not claim that rights have been legally cleared unless that has actually happened, and do not make legal conclusions inside the artistic specification.

Treat any formal rights review as a separate administrative question.

The documentation should openly credit the literary and demoscene sources.

---

# 20. Suggested new high-level project statement

Use the following as a conceptual basis, not necessarily verbatim:

> This project explores a four-stage lineage of unstable computed space. Lovecraft and Kenneth Sterling's *In the Walls of Eryx* imagines a Venusian prospector trapped in an invisible labyrinth whose topology may itself be changing. ASD's *Beyond the Walls of Eryx* transformed that motif into a demoscene audiovisual work in 2007. Mandarine's *Within the Mesh*, co-created by the present project's author in 2013, explicitly continued that demoscene lineage. The current work makes a further transformation: the labyrinth becomes interactive and generative. A local language model interprets the player's actions and is granted controlled authority over selected spatial relationships, allowing the world to remain locally navigable while its global topology drifts. The GPU therefore spends much of its computational budget on inference and world interpretation, while a deliberately constrained CPU raytracer produces sparse grayscale images. The work uses AI not to escape computational constraint, but to relocate it.

---

# 21. Suggested research framing

The revised documentation should converge toward this question:

> How can an AI-driven interactive work extend a demoscene lineage by turning the spatial inconsistency of generative models into an expressive, navigable constraint?

A broader PhD-facing formulation is:

> How can artificial intelligence mediate and reactivate a demoscene aesthetic founded on real-time computation, platform awareness, technical constraint, and incomplete but suggestive images?

A key subsidiary question is:

> What has to remain stable for a generatively changing world to be experienced as an uncanny labyrinth rather than as a broken game?

---

# 22. Suggested mediation framing

The work should be documented as a chain of media transformations:

```text
literary invisible labyrinth
        ↓
demoscene audiovisual interpretation
        ↓
personal demoscene reinterpretation
        ↓
interactive generative labyrinth
```

For public presentation, this can be combined with the Amiga DMA visualization work.

The historical mediation demonstrates that hardware constraint is not just "old computers were slow." It can be visualized as resource allocation across CPU, Copper, Blitter, bitplanes, sprites, audio, and memory access.

The contemporary project then shows a different but comparable artistic problem:

> Where does the machine spend its computation, and how does that allocation become visible in the work?

This gives the project a coherent triangle:

**Demoscene** — historical lineage and constraint-based real-time aesthetics.

**AI** — inference, interpretation, and controlled topological instability.

**Mediation** — making both historical and contemporary computational regimes perceptible to an audience.

---

# 23. Acceptance criteria for the documentation pass

The documentation reorientation is complete when:

- `STORY.md` no longer presents the datacenter as the active main fiction;
- `SPEC.md` explicitly establishes the four-stage Eryx/demoscene lineage;
- the LLM's controlled role in topological mutation is clearly stated;
- the moodboard explains how Eryx, ASD, and *Within the Mesh* connect to the current visual language;
- spatial-validation documentation treats invisible/mutable topology as a primary design problem;
- prefab documentation clearly identifies the migration from datacenter assets to Venusian extraction/prospecting assets;
- `DECISIONS.md` and `PROJECT_JOURNAL.md` preserve the history of the reorientation;
- technical documents do not claim unimplemented Eryx features;
- datacenter-specific language remains only where it documents historical development or current legacy implementation;
- the demoscene connection is no longer justified merely as "adjacency";
- the project still preserves the original computational-inversion thesis;
- the WinUAE/DMA mediation research remains visible;
- no generated PDF or prefab catalog is manually falsified;
- no unnecessary code refactor is performed during the documentation-only pass.

---

# 24. Source references for the reorientation

Project repository:

- https://github.com/astrofra/game-liminal-raytraced-llm-world

Literary source overview:

- https://en.wikipedia.org/wiki/In_the_Walls_of_Eryx

ASD — *Beyond the Walls of Eryx*:

- https://www.pouet.net/prod.php?which=31088

Mandarine — *Within the Mesh*:

- https://www.pouet.net/prod.php?which=61730

Current project documentation of particular importance:

- `documentation/SPEC.md`
- `documentation/STORY.md`
- `documentation/AMBIANCE_MOODBOARD.md`
- `documentation/NOTES.md`
- `documentation/FUNCTIONAL_PIPELINE_V1.md`
- `documentation/SPATIAL_VALIDATION_PLAN.md`
- `documentation/HYBRID_SCENE_LAYOUT_PLAN.md`
- `documentation/PREFAB_CATALOG.md`
- `documentation/DECISIONS.md`
- `documentation/PROJECT_JOURNAL.md`
- `documentation/TECHNICAL_STATE.md`
- `documentation/KNOWN_ISSUES.md`

---

# 25. Final instruction to Codex

Treat this reorientation as a **semantic migration of the project, not a reset**.

The existing engine, LLM integration, rendering research, state separation, computational-inversion thesis, constraint aesthetics, and mediation work are valuable and should remain visible.

The goal is to make all of those elements converge around a stronger lineage:

> a literary invisible maze becomes a demoscene work, becomes the author's own demoscene reinterpretation, and now becomes a playable generative system whose space itself is unstable.

The new project should feel as though the existing technical architecture had been waiting for this subject.

Do the documentation pass coherently, preserve history, flag code-level follow-up work, and do not invent implementation status.
