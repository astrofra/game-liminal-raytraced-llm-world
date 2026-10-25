# Within the Latent Walls / Entre les Murs Latents

![alt text](img/liminal-room.png)

## About

*Within the Latent Walls* / *Entre les Murs Latents* is a native, local-first interactive fiction and raytracing research prototype. It asks what happens when most of a game's computational budget is spent on language inference, interpretation, and world structure while a deliberately constrained CPU raytracer produces the image.

The active artistic direction follows a four-stage lineage from Lovecraft and Kenneth J. Sterling's *In the Walls of Eryx*, through ASD's *Beyond the Walls of Eryx* and Mandarine's *Within the Mesh*, to an interactive generative labyrinth. The intended player explores Venusian extraction infrastructure while an invisible topology can change through LLM proposals validated by an authoritative engine.

The game now has one authored starting place: the quarry threshold, inspired by concept 12. Every place beyond it is generated on discovery by local Gemma/Ollama. Places remain stable during a game; each launch starts a new exploration. Automatic write-only research archives retain descriptions, scene programs, images and turn results. See the [current play and research guide](documentation/SINGLE_START_RESEARCH.md).

The [fiction and scene reliability delivery](documentation/BILAN_FICTION_ET_FIABILITE_SCENES.md) adds grounded narration, a notebook, fixed 256×128 depth/object-ID validation, conservative syntax recovery and at most three local composition calls.

Current state:

- normal launch: SDL at the quarry threshold, using local Ollama `gemma4:e4b`; east discovers the next place, west returns; a new game and generation seed at every launch
- [local-scene roadmap results and retained images](documentation/LOCAL_SCENE_ROADMAP_RESULTS.md): Gemma selected after a common-backend comparison; 9/10 native pilot discoveries accepted, with visual limitations documented
- vendored Cornell Box assets
- vendored `llama.cpp v0.7.0` (`620d2225cc436ab0b001e3200e590e3b286ef9ab`) in `vendor/llama.cpp`
- native C++17 CLI renderer
- proprietary scene format v1 bootstrap
- source-generated Eryx quarry prefab vocabulary and visual catalog
- first handcrafted liminal scene
- camera-attached parametric spotlight for proprietary scenes
- optional world-space distant light with color and angular radius, including shadows and diffuse bounce lighting
- grayscale material reduction from the Cornell MTL
- simple BVH acceleration
- diffuse path tracing with direct light sampling and low-bounce radiosity
- locked dielectric glass for crystal prefabs, with refraction, exact Fresnel reflection, total internal reflection, thickness-dependent RGB filtering, bounded “poor man's” dispersion, and a separate path-depth limit
- native PNG output via vendored `stb_image_write.h`
- optional `llama.cpp` runtime wiring in CMake
- optional OpenMP line-parallel rendering in CMake
- first SDL3 interactive frontend with streaming LLM output
- progressively generated eight-image raytraced viewport loops with deterministic breathing-camera motion and live HUD composition
- software viewport post-processing with a sharp central field, progressive peripheral blur, and low-cost radial RGB dispersion
- first generated-room graph for improvised cardinal navigation
- authoritative hard/soft/spatial state with deterministic and hybrid scene compilation
- experimental `--scene-generation-mode dynamic`: native spatial plan, free `.scene` generation, bounded validation/repair, immutable accepted scenes; real-model qualification has not passed ([French runtime guide](documentation/GENERATION_DYNAMIQUE_DECORS_RUNTIME.md), [observed results](documentation/GENERATION_DYNAMIQUE_DECORS_BILAN.md))
- one authored Eryx starting scene with a solid 25 cm glass slab; the obsolete six-location route has been removed
- generated exploration with authoritative links and immutable revisits within a game
- automatic research archives outside the repository; never used to initialize a game
- model-directed Venus heat loop linking external temperature, suit-buffered body temperature, and LLM sampling temperature
- six-language player interface selected at launch: English (`E`), Français (`F`), Norsk (`N`, Bokmål), Dansk (`D`), Deutsch (`G`), or Italiano (`I`)
- Unicode-tolerant localized navigation whose exits remain engine-owned
- symmetric procedural suit-visor mask around the 3D viewport
- green-to-pale-yellow Venus sky with doubled radiance on environment misses
- legacy datacenter/desert scenes retained as historical baselines
- pinned, hash-verified Ministral Q4/Q5/Reasoning profiles retained for native-backend diagnostics
- Jinja chat templates, separate private/final channels, bounded generation and cooperative cancellation
- first headless `Ministral` turn pipeline wired to the renderer
- explicit provenance for the reused 2003 raytracer ideas

Documentation index:

- `documentation/README.md`
- [`Ministral qualification and model decision`](documentation/MINISTRAL_QUALIFICATION_REPORT.md)

## Build

```powershell
python scripts/build_native.py --with-cli --publish
```

Windows helper:

The Windows helper uses Ninja, a CUDA-compatible MSVC toolset and the Windows SDK. See [`documentation/MINISTRAL_RUNTIME_GUIDE.md`](documentation/MINISTRAL_RUNTIME_GUIDE.md) for toolset selection, model profiles and qualification commands. Python generates the native model table from `scripts/ministral_models.json`.

```bat
build_release.bat
build_installer.bat --version 2.2.0
run_cornell_test.bat
download_ministral.bat
ask_ministral.bat
play_within_the_latent_walls.bat
```

## Windows distribution

The existing 2.2 distribution below predates the Ollama/Gemma default. This tranche publishes the development executable, not a new installer. For the current game, run Ollama locally with `gemma4:e4b` installed; see the [current launch guide](documentation/SINGLE_START_RESEARCH.md).

Build the distributable installer from a Windows development machine with:

```bat
build_installer.bat --version 2.2.0
```

The resulting setup executable and GPL corresponding-source archive are written to `output/installer/dist/`. The setup contains the native game, assets, CUDA runtime libraries, and Microsoft runtime libraries, but not the AI weights. During installation it downloads the pinned official `Ministral 3 8B Instruct 2512 Q4_K_M` GGUF (about 5.20 GB), verifies its SHA-256 digest, and installs it beside the game. Internet access is required only during setup; the installed game then runs locally.

See [`documentation/WINDOWS_DISTRIBUTION.md`](documentation/WINDOWS_DISTRIBUTION.md) for prerequisites, package contents, release checks, and signing notes.

If you want the vendorized `llama.cpp` build with CUDA enabled, keep the default CMake options:

```powershell
python scripts/build_native.py --with-cli --publish
```

SDL3 is now enabled by default too. If the library is not already installed on the machine, CMake fetches SDL `3.4.12` automatically and links it statically for the interactive frontend.

OpenMP is enabled by default when the local compiler supports it. After the initial build, a developer shell configured for the same compiler and SDK can select a mono-thread build:

```powershell
cmake -S . -B build_ministral_v070 -DLIMINAL_ENABLE_OPENMP=OFF
cmake --build build_ministral_v070 --config Release
```

Model download helper for the explicit native Ministral backend (the current game uses Ollama/Gemma):

```powershell
python -m pip install "huggingface_hub<1.0"
python scripts\download_ministral.py
```

Windows shortcut:

```bat
download_ministral.bat
download_ministral.bat --force-download
```

Quick local LLM question:

```bat
ask_ministral.bat "Quel est le sens de la vie ?"
ask_ministral.bat "Resume-moi ce projet en 3 phrases."
```

Launch the SDL3 frontend:

```bat
play_within_the_latent_walls.bat
```

Normal launch asks only for the interface language, then starts at the quarry threshold. Type **east** (or **est** in French) to discover a generated room, and continue east to explore further. Research archives are written incrementally to `%LOCALAPPDATA%\WithinTheLatentWalls\research\`, in a unique directory for each game. SDL does not resume a previous game. Ollama must be running on `127.0.0.1:11434`, with `gemma4:e4b` installed.

Animated-view diagnostics:

```powershell
.\build\Release\liminal_cornell_renderer.exe --animated-view-self-test
.\build\Release\liminal_cornell_renderer.exe --navigation-self-test
.\build\Release\liminal_cornell_renderer.exe --sdl --animated-view-debug --view-animation-fps 6
.\build\Release\liminal_cornell_renderer.exe --view-post-process-self-test
.\build\Release\liminal_cornell_renderer.exe --sdl --view-post-process-debug --view-blur-radius 3 --view-dispersion 6 --view-grain 7
```

## Run

```powershell
.\build\Release\liminal_cornell_renderer.exe
```

Inspect the compiled `llama.cpp` runtime wiring:

```powershell
.\build\Release\liminal_cornell_renderer.exe --llama-info
```

Running without arguments opens a new SDL game at the quarry threshold. Rendering defaults remain `800x400`, panorama, `16 spp`; the published latency comparison uses `8 spp`. Supply `--scene` explicitly for a standalone diagnostic render.

Useful overrides:

```powershell
.\build\Release\liminal_cornell_renderer.exe --scene assets\scenes\liminal_service_corridor.scene --samples 16 --width 256 --height 256
.\build\Release\liminal_cornell_renderer.exe --scene assets\cornell\cornell_box.obj --output output\cornell_box_512.png --samples 64 --width 512 --height 512
.\build\Release\liminal_cornell_renderer.exe --sdl --location quarry_threshold --save-state output\sdl_session_state.json
.\build\Release\liminal_cornell_renderer.exe --dump-turn-contract --location quarry_threshold --command "inspect the northern datum"
.\build\Release\liminal_cornell_renderer.exe --compile-location quarry_threshold --output output\compiled_eryx_threshold.png
.\build\Release\liminal_cornell_renderer.exe --run-session --location quarry_threshold --command east --command east --command west --save-state output\eryx_route.json --output output\eryx_route.png
.\build\Release\liminal_cornell_renderer.exe --run-turn --load-state output\eryx_route.json --command west --dump-session-history --output output\eryx_route_2.png
``` 

`--sdl` opens the first desktop loop:

- launch screen: the large French title `Entre les Murs Latents`, the smaller English title `Within the Latent Walls`, a two-column selector for `English (E)`, `Français (F)`, `Norsk (N)`, `Dansk (D)`, `Deutsch (G)`, and `Italiano (I)`, then `(c) Resistance 2026` at the bottom; player-facing text follows that choice while internal LLM contracts remain English
- top panel: raytraced scene with a software peripheral blur and radial R/G/B pixel dispersion; the central field remains sharp
- the 3D image is clipped by a symmetric procedural visor assembled from rectangular scanlines and elliptical boundaries; the exterior is black
- visor HUD: external temperature and body temperature, composed after post-processing so it remains sharp
- lower panel: transcript joueur avec la narration finale seulement
- input line: text entry with edition, history on arrow up/down, submit on Enter
- localized status line with an ASCII activity spinner
- raw LLM output and generated `.scene` stream to the terminal, not to the player GUI
- `Escape` while busy: cancellation request
- cardinal commands like `NORTH`, `EAST`, `SOUTH`, `WEST` can now generate and cache improvised rooms on first entry

Default output: `output/liminal_service_corridor.png`

Cornell Box test helper:

```bat
run_cornell_test.bat
```
