# Liminal Raytraced-World LLM-Built Game

![alt text](img/liminal-room.png)

Initial bootstrap for the rendering side of the project.

Current state:

- vendored Cornell Box assets
- vendored `llama.cpp` tree in `vendor/llama.cpp`
- clean C++11 CLI renderer
- proprietary scene format v1 bootstrap
- first handcrafted liminal scene
- camera-attached parametric spotlight for proprietary scenes
- grayscale material reduction from the Cornell MTL
- simple BVH acceleration
- diffuse path tracing with direct light sampling and low-bounce radiosity
- native PNG output via vendored `stb_image_write.h`
- optional `llama.cpp` runtime wiring in CMake
- Python helper to download `Ministral 3 8B Instruct 2512` GGUF
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
download_ministral.bat
```

If you want the vendorized `llama.cpp` build with CUDA enabled, keep the default CMake options:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DLIMINAL_ENABLE_LLAMA_CPP=ON -DLIMINAL_ENABLE_LLAMA_CUDA=ON
cmake --build build --config Release
```

Model download helper:

```powershell
python -m pip install "huggingface_hub<1.0"
python scripts\download_ministral.py
```

Windows shortcut:

```bat
download_ministral.bat
download_ministral.bat --force-download
```

## Run

```powershell
.\build\Release\liminal_cornell_renderer.exe
```

Inspect the compiled `llama.cpp` runtime wiring:

```powershell
.\build\Release\liminal_cornell_renderer.exe --llama-info
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
