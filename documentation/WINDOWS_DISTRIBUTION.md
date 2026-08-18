# Windows Distribution

Last updated: 2026-08-18

## Outcome

`build_installer.bat` creates a 64-bit Windows installer for *Within the Latent Walls* / *Entre les Murs Latents*. The setup executable contains the native game, assets, fonts, notices, and application-local runtime DLLs. It never contains AI model weights.

During setup, Inno Setup downloads the official text-only model directly from Mistral AI's Hugging Face repository:

- repository: `mistralai/Ministral-3-8B-Instruct-2512-GGUF`
- revision: `0102285ad796bd99af90f58de616092e5630e970`
- file: `Ministral-3-8B-Instruct-2512-Q4_K_M.gguf`
- exact size: `5,198,911,904` bytes
- SHA-256: `33e7a72cf5e6e2cfc2f2847075acc013d68bba023e35310cef86b5cf8fdca761`
- model license: Apache License 2.0

The multimodal projector is not downloaded because the game uses text inference only.

## Build prerequisites

The build machine needs:

- 64-bit Windows
- Visual Studio 2022 with the C++ desktop workload
- CMake available in `PATH`
- Python 3.10 or newer available in `PATH`
- a supported NVIDIA CUDA Toolkit for the CUDA-enabled build
- Internet access if the pinned Inno Setup compiler is not already cached

The player does not need Python, Visual Studio, the CUDA Toolkit, or Ollama. The installer places the application-local CUDA and Microsoft runtime libraries beside the executable. A current NVIDIA display driver remains a system prerequisite for this CUDA build.

## Build command

From the repository root:

```bat
build_installer.bat --version 0.1.0
```

Useful options:

```bat
build_installer.bat --version 0.1.0 --skip-build
build_installer.bat --version 0.1.0 --skip-source-archive
build_installer.bat --version 0.1.0 --iscc "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
build_installer.bat --version 0.1.0 --no-bootstrap-inno
```

The script performs these checks before reporting success:

1. builds or reuses the CMake `Release` executable;
2. copies assets, third-party redistribution notices, and recursively discovered non-system DLL imports to an isolated staging directory;
3. rejects the stage if any `.gguf` file is present;
4. writes `distribution-manifest.json` with file hashes and model metadata;
5. launches the staged executable with `--llama-info`;
6. downloads and SHA-256 verifies the pinned Inno Setup compiler if necessary;
7. compiles the setup and rejects an unexpectedly large result;
8. creates the corresponding-source ZIP by default, excluding model and vocabulary-fixture `.gguf` files.

## Outputs

Artifacts are written to `output/installer/dist/`:

- `Within-the-Latent-Walls-Setup-<version>.exe`
- `Within-the-Latent-Walls-Source-<version>.zip`

Intermediate files live under `output/installer/stage/` and `output/installer/tools/`. The whole `output/installer/` tree is ignored by Git.

## Installation behavior

The setup is per-user and does not require administrator privileges. Its default destination is:

```text
%LOCALAPPDATA%\Programs\Within the Latent Walls
```

It offers English and French installer languages, creates a Start Menu shortcut, optionally creates a desktop shortcut, downloads and verifies the model, and can launch the game when setup completes. Save data is written below `%LOCALAPPDATA%\WithinTheLatentWalls` and is deliberately retained during uninstall.

The installer needs an Internet connection and approximately 5.20 GB of model download. Allow at least 12 GB of free disk space for installation and temporary files. Once installation succeeds, normal play is local and does not need network access.

## Release checklist

- Build from the intended release commit with a semantic version.
- Archive and publish the setup SHA-256 and source ZIP SHA-256.
- Sign both the game executable and setup executable with Authenticode before public release.
- Test install, first launch, one LLM turn, save/load, and uninstall on clean Windows 10 and Windows 11 x64 machines.
- Test failure and retry after interrupting the model download.
- Confirm the machine has enough VRAM or system RAM for the 8B Q4_K_M model.
- Distribute the corresponding-source ZIP beside the GPL binary package.

The current build script validates the application payload and installer compilation without downloading the 5.20 GB model. A clean-machine end-to-end installation remains a release gate.
