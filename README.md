# game-liminal-raytraced-llm-world

Initial bootstrap for the rendering side of the project.

Current state:

- vendored Cornell Box assets
- clean C++11 CLI renderer
- grayscale material reduction from the Cornell MTL
- simple BVH acceleration
- diffuse path tracing with direct light sampling and low-bounce radiosity
- explicit provenance for the reused 2003 raytracer ideas

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Run

```powershell
.\build\Release\liminal_cornell_renderer.exe
```

Useful overrides:

```powershell
.\build\Release\liminal_cornell_renderer.exe --samples 16 --width 256 --height 256
.\build\Release\liminal_cornell_renderer.exe --output output\cornell_box_512.pgm --samples 64 --width 512 --height 512
```

Default output: `output/cornell_box.pgm`
