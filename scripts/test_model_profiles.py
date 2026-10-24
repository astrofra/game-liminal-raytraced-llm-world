import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import download_ministral as download
import model_profiles as profiles


class ProfilesTest(unittest.TestCase):
    def test_rejects_invalid_manifest(self):
        base = profiles.load_profiles()
        mutations = [
            ("relative_path", "../escape.gguf"),
            ("relative_path", "C:/escape.gguf"),
            ("relative_path", "\\\\host\\share\\escape.gguf"),
            ("size_bytes", True), ("size_bytes", 0),
            ("revision", "main"), ("sha256", "123"),
            ("temperature", float("nan")), ("top_p", float("inf")),
        ]
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "profiles.json"
            for key, value in mutations:
                with self.subTest(key=key, value=value):
                    manifest = copy.deepcopy(base)
                    manifest["profiles"][profiles.DEFAULT_PROFILE][key] = value
                    path.write_text(json.dumps(manifest), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        profiles.load_profiles(path)

    def test_download_pins_revision_and_verifies_bytes(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "test.gguf"
            data = b"GGUFtest model fixture"
            profile = dict(profiles.get_profile(), filename=path.name,
                           size_bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
            def fetch(**kwargs):
                self.assertEqual(kwargs["revision"], profile["revision"])
                self.assertEqual(kwargs["repo_id"], profile["repo_id"])
                self.assertEqual(kwargs["local_dir"], str(path.parent))
                path.write_bytes(data)
                return str(path)
            with patch.object(download, "get_profile", return_value=profile), patch.object(
                download, "_import_hf_hub_download", return_value=fetch
            ):
                self.assertEqual(download.main(["--output-dir", folder]), 0)
            path.write_bytes(b"GGUF" + b"x" * (len(data) - 4))
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                download.verify_model_file(path, profile)

    def test_verify_only_never_imports_network_library(self):
        with tempfile.TemporaryDirectory() as folder:
            with patch.object(download, "_import_hf_hub_download", side_effect=AssertionError("network")):
                with self.assertRaises(FileNotFoundError):
                    download.main(["--verify-only", "--output-dir", folder])


if __name__ == "__main__":
    unittest.main()
