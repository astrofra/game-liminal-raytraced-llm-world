# game-liminal-raytraced-llm-world

Initial bootstrap for the rendering side of the project.

Current state:

- vendored Cornell Box assets
- clean C++11 CLI renderer
- proprietary scene format v1 bootstrap
- first handcrafted liminal scene
- camera-attached parametric spotlight for proprietary scenes
- grayscale material reduction from the Cornell MTL
- simple BVH acceleration
- diffuse path tracing with direct light sampling and low-bounce radiosity
- native PNG output via vendored `stb_image_write.h`
- explicit provenance for the reused 2003 raytracer ideas

Documentation index:

- `documentation/README.md`

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

Windows helper:

```bat
build_release.bat
run_cornell_test.bat
```

## Run

```powershell
.\build\Release\liminal_cornell_renderer.exe
```

The default run now renders:

- `assets/scenes/liminal_service_corridor.scene`
- en `800x400` par defaut, en cadrage panorama

Useful overrides:

```powershell
.\build\Release\liminal_cornell_renderer.exe --scene assets\scenes\liminal_service_corridor.scene --samples 16 --width 256 --height 256
.\build\Release\liminal_cornell_renderer.exe --scene assets\cornell\cornell_box.obj --output output\cornell_box_512.png --samples 64 --width 512 --height 512
```

Default output: `output/liminal_service_corridor.png`

Cornell Box test helper:

```bat
run_cornell_test.bat
```
