"""The benchmark must never turn missing, stale or failed output into a success."""
import importlib.util
import json
import pathlib
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("bench", ROOT / "scripts/benchmark_dynamic_scenes.py")
bench = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bench)


class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="banc scènes ")
        self.root = pathlib.Path(self.temp.name)
        self.case = bench.load_corpus(ROOT / "tests/data/dynamic_scene_briefs.json")[1][0]

    def tearDown(self):
        self.temp.cleanup()

    def run_case(self, fake):
        record = {}
        with patch.object(bench.subprocess, "run", side_effect=fake):
            bench.execute(self.root, ROOT / "fake executable.exe", self.case, "two-pass", 77, "ministral-8b-q4", record)
        return record

    def test_missing_metrics_is_failure(self):
        result = self.run_case(lambda *a, **k: subprocess.CompletedProcess(a[0], 0))
        self.assertFalse(result["success"])
        self.assertIn("absentes", result["metrics_error"])

    def test_timeout_is_failure(self):
        def timeout(*a, **k):
            raise subprocess.TimeoutExpired(a[0], 1200)
        self.assertEqual(self.run_case(timeout)["status"], "timeout")

    def test_paths_and_observed_success(self):
        def fake(cmd, **kwargs):
            self.assertEqual(kwargs["cwd"], ROOT)
            self.assertFalse(kwargs["shell"])
            self.assertTrue(pathlib.Path(cmd[2]).is_absolute())
            prefix = pathlib.Path(cmd[cmd.index("--scene-artifacts") + 1])
            data = {key: None for key in bench.METRIC_KEYS}
            data.update(version=1, method="two-pass", seed=77, success=True, timeout=False,
                        cancelled=False, repairs=0, calls=[], audit={"accepted": True})
            prefix.with_suffix(".metrics.json").write_text(json.dumps(data), encoding="utf-8")
            pathlib.Path(cmd[cmd.index("--output") + 1]).write_bytes(b"PNG test stub")
            return subprocess.CompletedProcess(cmd, 0)
        self.assertTrue(self.run_case(fake)["success"])

    def test_bad_method_metrics_and_unsafe_corpus(self):
        self.assertIsNotNone(bench.metrics_issue({}, "two-pass", 77))
        bad = self.root / "bad.json"
        bad.write_text(json.dumps({"version": 1, "cases": [{**self.case, "id": "../escape"}]}))
        with self.assertRaises(ValueError):
            bench.load_corpus(bad)


if __name__ == "__main__":
    unittest.main()
