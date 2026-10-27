"""Packaging regressions: Setup must build without local weights and exclude them."""
import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import build_installer as installer


class InstallerPayloadTest(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        root = Path(temp.name)
        self.root = root
        self.stage = root / "output/installer/stage"
        self.model = root / installer.MODEL_RELATIVE_PATH
        self.weights = b"GGUFfixture weights"
        files = [
            "build/Release/liminal_cornell_renderer.exe", "scripts/models.json",
            "vendor/llama.cpp/VENDORED_COMMIT.txt", "vendor/llama.cpp/LICENSE", "LICENSE", "README.md",
            "packaging/MODEL_NOTICE.txt", "packaging/INSTALLATION_README.txt", "packaging/Gemma-LICENSE.txt",
            "build/_deps/sdl3-src/LICENSE.txt", "build/_deps/sdl3_ttf-src/LICENSE.txt",
            "build/_deps/sdl3_ttf-src/external/freetype/LICENSE.TXT",
            "build/_deps/sdl3_ttf-src/external/harfbuzz/COPYING",
            "build/_deps/sdl3_ttf-src/external/plutosvg/LICENSE",
            "build/_deps/sdl3_ttf-src/external/plutovg/LICENSE",
            "assets/fonts/Zilla_Slab/OFL.txt", "assets/fonts/Zilla_Slab_Highlight/OFL.txt",
            "cuda/LICENSE", "cuda/EULA.txt", "vendor/stb/VORBIS-LICENSE",
        ]
        for name in files:
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture", encoding="utf-8")
        exe = root / files[0]
        provenance = {"artifacts": {exe.name: installer.sha256_file(exe)},
                      "model_manifest_sha256": installer.sha256_file(root / "scripts/models.json"),
                      "llama_cpp_commit": "fixture"}
        (exe.parent / "runtime-build.json").write_text(json.dumps(provenance), encoding="utf-8")
        patches = [
            patch.multiple(installer, PROJECT_ROOT=root, BUILD_DIR=root / "build", SOURCE_EXE=exe,
                           INSTALLER_ROOT=root / "output/installer", STAGE_DIR=self.stage,
                           DIST_DIR=root / "output/installer/dist", PACKAGING_DIR=root / "packaging",
                           MODEL_SIZE=len(self.weights), MODEL_SHA256=hashlib.sha256(self.weights).hexdigest()),
            patch.dict(os.environ, {"CUDA_PATH": str(root / "cuda")}),
            patch.object(installer, "copy_runtime_dependencies", return_value=[]),
        ]
        for item in patches:
            item.start()
            self.addCleanup(item.stop)

    def manifest(self):
        return json.loads((self.stage / "distribution-manifest.json").read_text(encoding="utf-8"))

    def test_setup_without_local_model_and_restage_after_portable(self):
        installer.copy_distribution_payload("3.0.1")
        self.assertFalse(self.model.exists())
        self.assertEqual(list(self.stage.rglob("*.gguf")), [])
        self.assertFalse(self.manifest()["model_embedded"])
        self.assertTrue((self.stage / "licenses/third-party/stb-vorbis-LICENSE.txt").is_file())
        self.assertTrue(self.manifest()["model"]["downloaded_during_installation"])

        self.model.parent.mkdir(parents=True)
        self.model.write_bytes(self.weights)
        installer.copy_distribution_payload("3.0.1", include_model=True)
        self.assertEqual((self.stage / installer.MODEL_RELATIVE_PATH).read_bytes(), self.weights)
        self.assertTrue(self.manifest()["model_embedded"])
        self.assertFalse(self.manifest()["model"]["downloaded_during_installation"])
        with self.assertRaisesRegex(RuntimeError, "must not contain model weights"):
            installer.build_inno_installer("3.0.1", Path("unused-compiler.exe"))

        # A stale portable stage and corrupt local GGUF must not affect Setup.
        self.model.write_bytes(b"X" * len(self.weights))
        installer.copy_distribution_payload("3.0.1")
        self.assertEqual(list(self.stage.rglob("*.gguf")), [])
        self.assertFalse(self.manifest()["model_embedded"])
        self.assertFalse(any(item["path"].endswith(".gguf") for item in self.manifest()["payload_files"]))

    def test_portable_rejects_missing_or_corrupt_model(self):
        with self.assertRaisesRegex(RuntimeError, "model is missing or invalid"):
            installer.copy_distribution_payload("3.0.1", include_model=True)
        self.model.parent.mkdir(parents=True)
        self.model.write_bytes(b"X" * len(self.weights))
        with self.assertRaisesRegex(RuntimeError, "model is missing or invalid"):
            installer.copy_distribution_payload("3.0.1", include_model=True)

    def test_weights_in_assets_cannot_leak_into_setup(self):
        (self.root / "assets/accidental.gguf").write_bytes(self.weights)
        with self.assertRaisesRegex(RuntimeError, "Unexpected model payload"):
            installer.copy_distribution_payload("3.0.1")


if __name__ == "__main__":
    unittest.main()
