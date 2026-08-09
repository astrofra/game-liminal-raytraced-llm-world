# Eryx-Based Generative Labyrinth

## Current story direction

The project does not yet have a final public title. *Le Désert des tokens* is the previous working title and now designates an earlier narrative branch documented in [`NOTES.md`](./NOTES.md) and [`PROJECT_JOURNAL.md`](./PROJECT_JOURNAL.md).

The active fiction places the player in a Venusian extraction zone. The player is a prospector or survey operative moving among a quarry, sparse industrial shelters, extraction machinery, navigation beacons, sample containers, and other human attempts to measure and exploit a hostile environment.

Within this visible infrastructure lies another architecture: a labyrinth whose walls cannot ordinarily be seen. An apparently open route may reject movement. A corridor may reconnect to a different place. A path followed in reverse may not return from the expected direction. A familiar site may preserve its local appearance while acquiring different connections.

The player is therefore not only exploring territory. They are trying to establish whether space can still be trusted.

## Fictional situation

The player enters the extraction field with practical objectives: survey a sector, recover or inspect a crystal specimen, reactivate an instrument, locate another prospector, return to a shelter, or mark a safe route. These goals make the world actionable without requiring a fixed retelling of *In the Walls of Eryx*.

The human layer should remain materially legible:

- quarry floors, cuts, ramps, trenches, and retaining slabs
- prospecting stations and pressure shelters
- drilling or extraction equipment
- atmospheric-processing and oxygen-service units
- gates, pylons, cargo, scanners, and survey beacons
- sparse terrain extending beyond the range of the player's instrumented view

The alien layer should not become a conventional set of visible corridors. It exists primarily through traversal constraints, contradictions, and indirect evidence. The text may report contact with a surface where the image shows empty air. Dust may stop against a plane, a scanner may trace a contour, or an object may appear to rest against nothing. These signs are optional and incremental; the perceptual mismatch is fundamental.

The setting is a mediated, speculative Venus rather than an exercise in historical or scientific reconstruction. Its visual synthesis is:

> human brutalist extraction infrastructure + sparse hostile planetary environment + invisible alien topology

## The labyrinth changes under controlled rules

The system maintains an authoritative world state outside the language model. Inventory, collected specimens, survival resources if used, named discoveries, and the player's committed current position must not disappear because a model improvises.

The LLM receives controlled authority over selected topological relations. It may propose that:

- a known exit is now blocked by an unseen barrier
- a barrier previously encountered is absent
- a corridor reaches a different known location
- a return path arrives from an impossible direction
- an open quarry contains a newly inferred hidden division
- a revisited site retains recognizable anchors but changes connectivity
- the player's map conflicts with an otherwise actionable local scene

The engine validates each proposal before committing it. It must preserve immediate playability, protect hard state, and keep at least one useful course of action available. Random sampling alone is not the labyrinth; the artistic mechanism is the deliberate negotiation between generative proposals and deterministic validation.

The central design question is:

> What must remain stable for a changing maze to feel uncanny rather than merely broken?

## Four-stage lineage

The project treats Eryx as a motif transformed across four media and computational regimes.

```text
*In the Walls of Eryx*
        ↓
ASD, *Beyond the Walls of Eryx*
        ↓
Mandarine, *Within the Mesh*
        ↓
current interactive LLM work
```

### 1. Literary invisible space

H. P. Lovecraft and Kenneth J. Sterling's [*In the Walls of Eryx*](./IN_THE_WALLS_OF_ERYX.md) provides the conceptual matrix: Venus as an extractive frontier, valuable crystals, a prospector trapped by invisible architecture, failed mapping, and the progressive loss of spatial certainty.

The project credits and reinterprets these motifs. It does not reproduce the story scene by scene or imitate its prose.

### 2. Demoscene transformation

ASD's [*Beyond the Walls of Eryx*](https://www.pouet.net/prod.php?which=31088) (2007) transformed the motif into a demoscene audiovisual work concerned with lines, linked worlds, changing projection, collapse, and the breakdown of computed space. The reference establishes that Eryx already has a history as an aesthetic and computational reinterpretation within the demoscene.

### 3. Personal demoscene continuation

Mandarine's [*Within the Mesh*](https://www.pouet.net/prod.php?which=61730) (2013), co-created by the present project's author, explicitly continued that relation. The Eryx lineage is therefore historical and personal: it already passed through the author's demoscene practice before this project.

### 4. Interactive generative transformation

The current work makes the labyrinth inhabitable and mutable. The player no longer watches unstable computed space unfold in a fixed audiovisual sequence. A local model interprets actions and proposes controlled changes to the navigable graph while a deterministic engine protects the conditions of play.

This is the new contribution to the lineage:

> performed computed space becomes generatively reconfigured interactive space.

## Text, image, and incomplete perception

The LLM is not merely a prose generator. It interprets commands, narrates local consequences, updates selected facts, and proposes changes to soft topology. A separate spatial state and deterministic scene compiler mediate those proposals into geometry.

The renderer remains deliberately austere. Simple slabs, boxes, planes, a constrained palette, camera-linked lighting, low sample counts, and visible noise make the image feel like an incomplete instrument reading rather than an exhaustive depiction.

The text and image do different work:

- the text can name a wall the image cannot show
- the image can preserve a familiar local landmark while the text reports changed access
- the scene can orient the player without proving the global map
- the gap between both channels can become evidence of the labyrinth

The world should stay locally legible and globally uncertain. The image is not a failed illustration of the prose; it is one partial sensor among others.

## Pressure, orientation, and optional temperature

The fiction may use oxygen, suit stress, exposure time, confidence in the map, distance from shelter, or proximity to the labyrinth as pressures on exploration. No such resource should be added merely to imitate survival-game convention.

The temperature mechanism developed for *Le Désert des tokens* remains an optional research idea, not the central mechanic. If retained, model sampling variation could respond to environmental stress or spatial entropy. It must remain subordinate to structured topological mutation, because randomness and meaningful spatial instability are not equivalent.

## Autonomous presentation

An exhibition or demoparty version may use a scripted ghost player. Its command sequence should provide dramatic structure while leaving narration and accepted topology changes live.

A suitable progression is:

```text
survey
    → mark a route
    → move through it
    → retrace it
    → discover a contradiction
    → compare instruments and memory
    → remap
    → continue
```

This is preferable to an unconstrained model talking to itself: authored actions establish rhythm, while the generative system preserves uncertainty.

## Computational allocation and mediation

The original counterfactual remains central:

> What if video games had spent their computational power on narrative plasticity rather than graphical sophistication?

The GPU is used primarily for local LLM inference and world interpretation. The CPU produces a deliberately constrained raytraced image. AI does not remove computational constraint; it relocates it.

The WinUAE DMA visualizer remains a complementary mediation device. It can show how CPU, Copper, Blitter, bitplane DMA, sprites, audio, and other accesses occupy Amiga memory-bus time. This should be compared conceptually, not architecturally, with the contemporary allocation:

```text
Amiga-era demo:
hardware resources → coordinated audiovisual output

current project:
GPU compute → inference, interpretation, topological proposals
CPU compute → constrained raytraced image
```

In both cases, aesthetic form is inseparable from where the machine spends its resources.

## Research proposition

The project is not an experiment in whether a model can write literature better than a human author. It is an executable research instrument concerned with computational allocation, image/text mediation, real-time world generation, and controlled spatial instability.

Its central question is:

> How can an AI-driven interactive work extend a demoscene lineage by turning the spatial inconsistency of generative models into an expressive, navigable constraint?
