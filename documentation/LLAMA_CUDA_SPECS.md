# Technical Specification: Ministral 3 8B with `llama.cpp` and CUDA

**Document version:** 1.0  
**Date:** 2026-07-30  
**Primary target:** Windows 10/11 x64  
**GPU target:** NVIDIA GeForce RTX 4060, 8 GB VRAM  
**Inference runtime:** `llama.cpp` compiled with CUDA  
**Model:** Ministral 3 8B Instruct 2512, GGUF `Q4_K_M`

---

## 1. Purpose

This document defines a reproducible setup for:

1. downloading the official Ministral 3 8B Instruct model in GGUF format;
2. compiling `llama.cpp` with NVIDIA CUDA support;
3. verifying that the CUDA backend is detected;
4. verifying that the model loads and generates text;
5. verifying that inference is actually offloaded to the RTX 4060;
6. optionally exposing the model through the `llama-server` HTTP API.

The resulting setup is intended as the first technical foundation for a native, offline interactive-fiction prototype.

---

## 2. Selected model

### 2.1 Repository

```text
mistralai/Ministral-3-8B-Instruct-2512-GGUF
```

### 2.2 Model file

```text
Ministral-3-8B-Instruct-2512-Q4_K_M.gguf
```

### 2.3 File properties

| Property | Value |
|---|---|
| Architecture | Ministral 3 |
| Language model size | Approximately 8.4B parameters |
| Quantization | `Q4_K_M` |
| File size | Approximately 5.2 GB |
| Format | GGUF |
| Licence | Apache 2.0 |
| SHA-256 | `33e7a72cf5e6e2cfc2f2847075acc013d68bba023e35310cef86b5cf8fdca761` |

This quantization is selected because it leaves enough of the RTX 4060's 8 GB VRAM for CUDA buffers and a practical KV cache.

### 2.4 Text-only deployment

Ministral 3 also supports vision through a separate multimodal projector. The projector is not required for this project.

Do **not** download:

```text
Ministral-3-8B-Instruct-2512-BF16-mmproj.gguf
```

The first prototype uses text input and structured text output only.

---

## 3. Expected project layout

```text
project-root/
├── external/
│   └── llama.cpp/
├── models/
│   └── ministral-3-8b/
│       └── Ministral-3-8B-Instruct-2512-Q4_K_M.gguf
├── scripts/
│   └── download_ministral.py
├── logs/
└── README.md
```

The model file must not be committed to Git.

Suggested `.gitignore` entries:

```gitignore
models/*.gguf
models/**/*.gguf
external/llama.cpp/build*/
logs/*.txt
```

---

## 4. Prerequisites

## 4.1 Hardware

- NVIDIA GeForce RTX 4060 with 8 GB VRAM
- At least 16 GB system RAM
- Approximately 10 GB free disk space for:
  - model file;
  - source checkout;
  - CUDA build artifacts.

32 GB of system RAM is recommended for development, but not mandatory for this model.

## 4.2 Windows software

Install the following components:

1. **Recent NVIDIA display driver**
2. **Visual Studio 2022 or Visual Studio Build Tools**
3. **Desktop development with C++** workload
4. **Windows SDK**
5. **CMake**
6. **Git**
7. **Python 3.10 or later**
8. **NVIDIA CUDA Toolkit**

Install Visual Studio and its C++ workload **before** installing the CUDA Toolkit.

Use a current production CUDA Toolkit supported by the installed Visual Studio version. Avoid a developer-preview CUDA release for the first reproducible build.

## 4.3 Preflight commands

Open **Developer PowerShell for Visual Studio** and run:

```powershell
nvidia-smi
nvcc --version
cmake --version
git --version
python --version
```

### Expected results

- `nvidia-smi` lists an NVIDIA GeForce RTX 4060.
- `nvcc --version` reports the installed CUDA Toolkit.
- `cmake`, `git`, and `python` are found in `PATH`.
- No command returns a “not recognized” error.

---

## 5. Downloading the model

The recommended method uses the official `huggingface_hub` Python library. A direct HTTP alternative is provided afterwards.

## 5.1 Install the download dependency

From the project root:

```powershell
python -m pip install --upgrade huggingface_hub
```

Recent versions of `huggingface_hub` install Xet support, which is useful for large Hugging Face files.

## 5.2 Python download script

Create:

```text
scripts/download_ministral.py
```

with the following content:

```python
from __future__ import annotations

import hashlib
import sys
from pathlib import Path

from huggingface_hub import hf_hub_download


REPO_ID = "mistralai/Ministral-3-8B-Instruct-2512-GGUF"
FILENAME = "Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
EXPECTED_SHA256 = (
    "33e7a72cf5e6e2cfc2f2847075acc013"
    "d68bba023e35310cef86b5cf8fdca761"
)


def sha256_file(path: Path, chunk_size: int = 16 * 1024 * 1024) -> str:
    """Return the SHA-256 digest of a file."""
    digest = hashlib.sha256()

    with path.open("rb") as stream:
        while chunk := stream.read(chunk_size):
            digest.update(chunk)

    return digest.hexdigest()


def validate_gguf(path: Path) -> None:
    """Validate the basic GGUF signature, file size, and SHA-256 digest."""
    if not path.is_file():
        raise FileNotFoundError(f"Model file not found: {path}")

    file_size = path.stat().st_size
    if file_size < 4_000_000_000:
        raise RuntimeError(
            f"Model file is unexpectedly small: {file_size:,} bytes"
        )

    with path.open("rb") as stream:
        magic = stream.read(4)

    if magic != b"GGUF":
        raise RuntimeError(
            f"Invalid GGUF signature: expected b'GGUF', received {magic!r}"
        )

    print("Computing SHA-256. This may take a few minutes...")
    actual_sha256 = sha256_file(path)

    if actual_sha256.lower() != EXPECTED_SHA256.lower():
        raise RuntimeError(
            "SHA-256 mismatch.\n"
            f"Expected: {EXPECTED_SHA256}\n"
            f"Actual:   {actual_sha256}"
        )


def main() -> int:
    """Download and validate the selected Ministral GGUF file."""
    project_root = Path(__file__).resolve().parents[1]
    output_dir = project_root / "models" / "ministral-3-8b"
    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Repository: {REPO_ID}")
    print(f"File:       {FILENAME}")
    print(f"Output:     {output_dir}")

    downloaded_path = Path(
        hf_hub_download(
            repo_id=REPO_ID,
            filename=FILENAME,
            local_dir=output_dir,
        )
    ).resolve()

    print(f"Downloaded: {downloaded_path}")
    validate_gguf(downloaded_path)
    print("Model validation successful.")

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1) from exc
```

Run it from the project root:

```powershell
python scripts\download_ministral.py
```

### Expected final message

```text
Model validation successful.
```

## 5.3 Direct download alternative with `curl`

Windows includes `curl.exe` on recent systems.

```powershell
New-Item -ItemType Directory -Force models\ministral-3-8b | Out-Null

curl.exe -L `
  --fail `
  --retry 5 `
  --output models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf `
  "https://huggingface.co/mistralai/Ministral-3-8B-Instruct-2512-GGUF/resolve/main/Ministral-3-8B-Instruct-2512-Q4_K_M.gguf?download=true"
```

Verify the hash:

```powershell
Get-FileHash `
  models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf `
  -Algorithm SHA256
```

Expected hash:

```text
33E7A72CF5E6E2CFC2F2847075ACC013D68BBA023E35310CEF86B5CF8FDCA761
```

## 5.4 Direct download alternative with `wget`

```bash
mkdir -p models/ministral-3-8b

wget \
  --continue \
  --output-document models/ministral-3-8b/Ministral-3-8B-Instruct-2512-Q4_K_M.gguf \
  "https://huggingface.co/mistralai/Ministral-3-8B-Instruct-2512-GGUF/resolve/main/Ministral-3-8B-Instruct-2512-Q4_K_M.gguf?download=true"
```

Verify the hash:

```bash
sha256sum models/ministral-3-8b/Ministral-3-8B-Instruct-2512-Q4_K_M.gguf
```

---

## 6. Obtaining `llama.cpp`

From the project root:

```powershell
New-Item -ItemType Directory -Force external | Out-Null

git clone `
  https://github.com/ggml-org/llama.cpp.git `
  external\llama.cpp
```

Enter the repository:

```powershell
Set-Location external\llama.cpp
```

Record the exact source revision:

```powershell
git rev-parse HEAD | Tee-Object ..\..\llama-cpp-commit.txt
```

This commit identifier must be kept with test logs so that the build can be reproduced later.

---

## 7. Compiling `llama.cpp` with CUDA

## 7.1 Configure the CUDA build

Run from:

```text
project-root/external/llama.cpp
```

in **Developer PowerShell for Visual Studio**:

```powershell
cmake `
  -S . `
  -B build-cuda `
  -DGGML_CUDA=ON
```

The important option is:

```text
-DGGML_CUDA=ON
```

It enables the CUDA backend for NVIDIA GPUs.

## 7.2 Build the required binaries

```powershell
cmake `
  --build build-cuda `
  --config Release `
  --target llama-cli llama-server llama-bench `
  --parallel
```

Expected Windows output directory:

```text
external/llama.cpp/build-cuda/bin/Release/
```

Expected binaries include:

```text
llama-cli.exe
llama-server.exe
llama-bench.exe
```

CUDA and GGML DLLs should be located in the same output directory.

## 7.3 Optional non-native build

The default configuration targets the GPU connected to the development machine.

For a more portable CUDA build intended to run on different NVIDIA GPU generations:

```powershell
cmake `
  -S . `
  -B build-cuda-portable `
  -DGGML_CUDA=ON `
  -DGGML_NATIVE=OFF

cmake `
  --build build-cuda-portable `
  --config Release `
  --target llama-cli llama-server llama-bench `
  --parallel
```

The portable build may take longer and produce larger binaries.

---

## 8. Technical validation

Define convenient PowerShell variables from the project root:

```powershell
$LlamaBin = Resolve-Path `
  "external\llama.cpp\build-cuda\bin\Release"

$Model = Resolve-Path `
  "models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
```

## 8.1 Test A — binary version

```powershell
& "$LlamaBin\llama-cli.exe" --version
```

### Pass condition

- The executable starts without a missing-DLL error.
- Build and version information are displayed.

Save the result:

```powershell
& "$LlamaBin\llama-cli.exe" --version |
  Tee-Object logs\llama-version.txt
```

## 8.2 Test B — CUDA device detection

```powershell
& "$LlamaBin\llama-cli.exe" --list-devices
```

### Pass condition

The output lists a CUDA device corresponding to the RTX 4060.

Typical output contains information similar to:

```text
CUDA0: NVIDIA GeForce RTX 4060
```

The exact formatting may vary between `llama.cpp` revisions.

Save the result:

```powershell
& "$LlamaBin\llama-cli.exe" --list-devices |
  Tee-Object logs\llama-devices.txt
```

## 8.3 Test C — single-turn inference

```powershell
& "$LlamaBin\llama-cli.exe" `
  --model $Model `
  --n-gpu-layers all `
  --ctx-size 4096 `
  --flash-attn on `
  --conversation `
  --single-turn `
  --no-mmproj `
  --system-prompt "You are running a deterministic technical validation." `
  --prompt "Reply with the exact token MINISTRAL_CUDA_OK and nothing else." `
  --n-predict 32 `
  --temperature 0 `
  --seed 42
```

### Pass conditions

1. The model loads without an error.
2. CUDA initialization is visible in the startup log.
3. All or nearly all model layers are offloaded to the GPU.
4. The model returns:

```text
MINISTRAL_CUDA_OK
```

Minor punctuation around the token does not invalidate the runtime test, but it indicates that prompt adherence should be evaluated separately.

## 8.4 Test D — confirm actual GPU use

Open a second PowerShell window while Test C is running:

```powershell
nvidia-smi --loop=1
```

### Pass conditions

- A `llama-cli.exe` or `llama-server.exe` process appears.
- GPU memory use increases substantially.
- GPU utilisation rises during prompt processing and token generation.

For the selected 5.2 GB Q4 model, total VRAM consumption will be higher than the model file size because runtime buffers and the KV cache also require memory.

## 8.5 Test E — benchmark

```powershell
& "$LlamaBin\llama-bench.exe" `
  --model $Model `
  --n-gpu-layers all
```

### Pass conditions

- The benchmark completes.
- The backend column mentions CUDA.
- Prompt-processing and token-generation throughput values are reported.

Save the result:

```powershell
& "$LlamaBin\llama-bench.exe" `
  --model $Model `
  --n-gpu-layers all |
  Tee-Object logs\llama-bench-cuda.txt
```

No fixed tokens-per-second threshold is imposed in this specification because performance also depends on:

- RTX 4060 desktop or laptop variant;
- GPU power limit;
- CPU;
- system memory;
- context size;
- `llama.cpp` revision;
- CUDA Toolkit and driver versions.

---

## 9. HTTP server validation

## 9.1 Start `llama-server`

```powershell
& "$LlamaBin\llama-server.exe" `
  --model $Model `
  --n-gpu-layers all `
  --ctx-size 8192 `
  --flash-attn on `
  --no-mmproj `
  --host 127.0.0.1 `
  --port 8080
```

The server may take several seconds to load the model.

## 9.2 Health check

In a second PowerShell window:

```powershell
Invoke-RestMethod `
  -Uri "http://127.0.0.1:8080/health" `
  -Method Get
```

Expected response:

```text
status
------
ok
```

A temporary HTTP `503` response means that the model is still loading.

## 9.3 Chat completion test

```powershell
$Body = @{
    model = "ministral-local"
    temperature = 0.1
    max_tokens = 128
    messages = @(
        @{
            role = "system"
            content = "You are a local runtime validation assistant."
        },
        @{
            role = "user"
            content = "Reply with a short sentence confirming that the local API works."
        }
    )
} | ConvertTo-Json -Depth 6

$Response = Invoke-RestMethod `
  -Uri "http://127.0.0.1:8080/v1/chat/completions" `
  -Method Post `
  -ContentType "application/json" `
  -Body $Body

$Response.choices[0].message.content
```

### Pass condition

A non-empty assistant response is returned without an HTTP or JSON error.

---

## 10. Recommended initial runtime profile

For the RTX 4060 8 GB:

```text
Model:             Q4_K_M
GPU layers:        all
Context size:      8192 tokens
Flash Attention:   on
Vision projector:  disabled
Parallel slots:    1
Temperature:       0.1 for structured tests
```

Example:

```powershell
& "$LlamaBin\llama-server.exe" `
  --model $Model `
  --n-gpu-layers all `
  --ctx-size 8192 `
  --flash-attn on `
  --parallel 1 `
  --no-mmproj `
  --host 127.0.0.1 `
  --port 8080
```

The model advertises a much larger theoretical context window, but the first prototype should not attempt to use it. Context length directly affects KV-cache memory consumption.

For the interactive-fiction project, session state should eventually be managed through:

- hard-state data stored outside the model;
- short rolling conversation history;
- periodic summaries;
- retrieval of only the facts relevant to the current turn.

---

## 11. Troubleshooting

## 11.1 `nvcc` is not found

Possible causes:

- CUDA Toolkit is not installed;
- the shell was opened before installation;
- CUDA `bin` directory is missing from `PATH`.

Actions:

1. open a new Developer PowerShell;
2. run `nvcc --version`;
3. repair or reinstall the CUDA Toolkit if necessary.

## 11.2 CMake cannot find CUDA

Actions:

1. verify `nvcc --version`;
2. verify that Visual Studio C++ tools are installed;
3. install Visual Studio before reinstalling CUDA;
4. delete the failed build directory;
5. rerun CMake configuration.

```powershell
Remove-Item -Recurse -Force build-cuda

cmake `
  -S . `
  -B build-cuda `
  -DGGML_CUDA=ON
```

## 11.3 Missing `ggml-cuda.dll` or another DLL

Run the executable from:

```text
external/llama.cpp/build-cuda/bin/Release/
```

Do not copy only `llama-cli.exe` to another folder without its runtime DLLs.

For distribution, the executable and all required `llama.cpp`, GGML, CUDA-runtime, and compiler-runtime DLLs must be packaged together according to their respective licences.

## 11.4 CUDA device is not listed

Actions:

1. run `nvidia-smi`;
2. update or reinstall the NVIDIA driver;
3. confirm that the CUDA build was used rather than a CPU-only build;
4. confirm that `GGML_CUDA=ON` appeared in the CMake configuration.

## 11.5 Out-of-memory error

Reduce memory consumption in this order:

1. close other GPU applications;
2. reduce context size from `8192` to `4096`;
3. let `llama.cpp` fit the model automatically;
4. offload fewer layers if required;
5. optionally quantize the KV cache.

Safer fallback:

```powershell
& "$LlamaBin\llama-cli.exe" `
  --model $Model `
  --n-gpu-layers auto `
  --ctx-size 4096 `
  --flash-attn on `
  --no-mmproj `
  --conversation
```

Optional KV-cache reduction:

```text
--cache-type-k q8_0
--cache-type-v q8_0
```

KV-cache quantization must be quality-tested before production use, especially for structured output.

## 11.6 Inference is unexpectedly slow

Check:

- `--n-gpu-layers all` is present;
- startup logs report CUDA;
- startup logs report GPU layer offloading;
- `nvidia-smi` shows active GPU utilisation;
- the process is not using an accidentally compiled CPU-only binary;
- the context size is not unnecessarily large.

## 11.7 The vision projector is loaded accidentally

When using a local `--model` path, do not pass an `--mmproj` file.

When using Hugging Face auto-download mode, add:

```text
--no-mmproj
```

This project does not require the vision encoder.

---

## 12. Reproducibility records

After a successful setup, save the following information:

```powershell
New-Item -ItemType Directory -Force logs | Out-Null

nvidia-smi |
  Out-File logs\nvidia-smi.txt

nvcc --version |
  Out-File logs\nvcc-version.txt

cmake --version |
  Out-File logs\cmake-version.txt

& "$LlamaBin\llama-cli.exe" --version |
  Out-File logs\llama-version.txt

& "$LlamaBin\llama-cli.exe" --list-devices |
  Out-File logs\llama-devices.txt

Get-FileHash $Model -Algorithm SHA256 |
  Format-List |
  Out-File logs\model-sha256.txt
```

Also preserve:

```text
llama-cpp-commit.txt
```

A reproducible test record should contain:

- operating-system version;
- GPU model;
- NVIDIA driver version;
- CUDA Toolkit version;
- Visual Studio compiler version;
- CMake version;
- `llama.cpp` Git commit;
- model repository;
- model filename;
- model SHA-256;
- full inference command;
- benchmark result.

---

## 13. Acceptance criteria

The setup is accepted when all the following conditions are met:

- [ ] The model file exists in the expected directory.
- [ ] The model file begins with the `GGUF` signature.
- [ ] The model SHA-256 matches the expected value.
- [ ] `llama-cli.exe --version` runs successfully.
- [ ] `llama-cli.exe --list-devices` lists the RTX 4060 through CUDA.
- [ ] The model loads with `--n-gpu-layers all`.
- [ ] Startup logs report CUDA layer offloading.
- [ ] `nvidia-smi` shows VRAM use by the process.
- [ ] A single-turn prompt produces a valid response.
- [ ] `llama-bench` completes and reports the CUDA backend.
- [ ] `llama-server` returns `{"status":"ok"}` from `/health`.
- [ ] `/v1/chat/completions` returns a non-empty assistant message.

---

## 14. Linux build appendix

Install the compiler, CMake, Git, Python, NVIDIA driver, and CUDA Toolkit using the distribution-specific instructions.

Clone and build:

```bash
git clone https://github.com/ggml-org/llama.cpp.git external/llama.cpp
cd external/llama.cpp

cmake \
  -S . \
  -B build-cuda \
  -DGGML_CUDA=ON \
  -DCMAKE_BUILD_TYPE=Release

cmake \
  --build build-cuda \
  --config Release \
  --target llama-cli llama-server llama-bench \
  --parallel
```

Expected binaries:

```text
external/llama.cpp/build-cuda/bin/llama-cli
external/llama.cpp/build-cuda/bin/llama-server
external/llama.cpp/build-cuda/bin/llama-bench
```

Example test:

```bash
./external/llama.cpp/build-cuda/bin/llama-cli \
  --model models/ministral-3-8b/Ministral-3-8B-Instruct-2512-Q4_K_M.gguf \
  --n-gpu-layers all \
  --ctx-size 4096 \
  --flash-attn on \
  --conversation \
  --single-turn \
  --no-mmproj \
  --system-prompt "You are running a deterministic technical validation." \
  --prompt "Reply with the exact token MINISTRAL_CUDA_OK and nothing else." \
  --n-predict 32 \
  --temperature 0 \
  --seed 42
```

---

## 15. Official references

- [Mistral AI — Ministral 3 8B Instruct 2512 GGUF](https://huggingface.co/mistralai/Ministral-3-8B-Instruct-2512-GGUF)
- [Hugging Face — Download files from the Hub](https://huggingface.co/docs/huggingface_hub/guides/download)
- [llama.cpp — Build documentation](https://github.com/ggml-org/llama.cpp/blob/master/docs/build.md)
- [llama.cpp — CLI documentation](https://github.com/ggml-org/llama.cpp/blob/master/tools/cli/README.md)
- [llama.cpp — HTTP server documentation](https://github.com/ggml-org/llama.cpp/blob/master/tools/server/README.md)
- [NVIDIA — CUDA Installation Guide for Microsoft Windows](https://docs.nvidia.com/cuda/cuda-installation-guide-microsoft-windows/index.html)
