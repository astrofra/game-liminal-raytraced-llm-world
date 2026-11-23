import contextlib
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zlib

import render_archives as batch
from archive_gallery import write_gallery


def png_bytes(size):
    def chunk(name, payload):
        return (struct.pack(">I", len(payload)) + name + payload +
                struct.pack(">I", zlib.crc32(name + payload) & 0xffffffff))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress((b"\0" + b"\x44\x88\xaa" * size) * size)) +
            chunk(b"IEND", b""))


class BatchTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.research = self.root / "research"
        self.output = self.root / "images"
        self.renderer = self.root / "fake.exe"
        self.renderer.write_bytes(b"renderer v1")
        self.calls = []
        self.serial = 0
        self.fail = set()
        self.timeout = set()
        for name in ("run-a", "run-b"):
            archive = self.research / name
            (archive / "places").mkdir(parents=True)
            (archive / "manifest.json").write_text('{}')
            (archive / "state.json").write_text('{}')
            for index in range(2):
                (archive / f"places/place-{index:05d}.json").write_text(json.dumps({
                    "place_id": f"generated:room_{index}", "title": f"Lieu {index}"}))
                (archive / f"places/place-{index:05d}.scene").write_text("accepted geometry")
            (archive / "places/place-00000.supports.json").write_text("not a place")

    def fake_render(self, command, **kwargs):
        self.calls.append(command)
        archive = Path(command[command.index("--archive") + 1]).name
        place = command[command.index("--place") + 1] if "--place" in command else None
        if (archive, place) in self.timeout:
            raise subprocess.TimeoutExpired(command, 1)
        if (archive, place) in self.fail:
            return subprocess.CompletedProcess(command, 1)
        destination = Path(command[command.index("--output") + 1])
        size = int(command[command.index("--size") + 1])
        samples = int(command[command.index("--samples") + 1])
        destination.write_bytes(png_bytes(size))
        destination.with_suffix(".json").write_text(json.dumps({"archive": archive, "size": size, "samples": samples,
                                                                "labels_version": 1}))
        self.assertEqual(kwargs["env"]["OMP_NUM_THREADS"], "2")
        return subprocess.CompletedProcess(command, 0)

    def run_batch(self, *extra):
        self.serial += 1
        args = ["--research-dir", str(self.research), "--output-dir", str(self.output),
                "--renderer", str(self.renderer), "--size", "256", "--samples", "1",
                "--threads", "2", "--logs-dir", str(self.root / f"logs-{self.serial}"), *extra]
        with patch.object(batch.subprocess, "run", side_effect=self.fake_render), contextlib.redirect_stdout(io.StringIO()):
            return batch.main(args)

    def catalog(self):
        return json.loads((self.output / "catalog.json").read_text())

    def test_failures_continue_and_resume(self):
        before = {p: p.read_bytes() for p in self.research.rglob('*') if p.is_file()}
        self.fail.add(("run-b", None))
        self.assertEqual(self.run_batch(), 1)
        self.assertEqual(self.catalog()["counts"], {"failed": 1, "rendered": 5})
        self.fail.clear()
        self.calls.clear()
        self.assertEqual(self.run_batch(), 0)
        self.assertEqual(len(self.calls), 1)
        self.assertEqual(self.catalog()["counts"], {"rendered": 1, "cached": 5})
        self.assertEqual(before, {p: p.read_bytes() for p in self.research.rglob('*') if p.is_file()})

    def test_source_and_quality_invalidate_cache(self):
        self.run_batch()
        (self.research / "run-a/state.json").write_text('{"revision":1}')
        self.calls.clear()
        self.run_batch()
        self.assertEqual(len(self.calls), 3)
        self.calls.clear()
        self.run_batch("--samples", "2")
        self.assertEqual(len(self.calls), 6)

    def test_truncated_png_is_recomputed(self):
        self.run_batch()
        image = self.output / "run-a/atlas.png"
        image.write_bytes(image.read_bytes()[:-8])
        self.calls.clear()
        self.run_batch()
        self.assertEqual(len(self.calls), 1)
        self.assertTrue(batch.valid_png(image, 256))

    def test_legacy_titles_upgrade_without_raytrace(self):
        self.run_batch()
        catalog = self.catalog()
        for record in catalog["images"]:
            path = (self.output / record["image"]).with_suffix(".json")
            metadata = json.loads(path.read_text())
            metadata.pop("labels_version")
            path.write_text(json.dumps(metadata))
            record["metadata_sha256"] = batch.digest(path)
        (self.output / "catalog.json").write_text(json.dumps(catalog))
        self.renderer.write_bytes(b"renderer v2 with labels")
        self.calls.clear()
        self.run_batch()
        self.assertEqual(len(self.calls), 6)
        self.assertTrue(all("--annotate" in command for command in self.calls))
        self.assertTrue(all(r["method"] == "titles_only" for r in self.catalog()["images"]))

    def test_timeout_keeps_old_image_and_continues(self):
        self.run_batch()
        image = self.output / "run-b/atlas.png"
        before = image.read_bytes()
        self.timeout.add(("run-b", None))
        self.assertEqual(self.run_batch("--force"), 1)
        self.assertEqual(image.read_bytes(), before)
        self.assertEqual(self.catalog()["images"][0]["failure"], "timeout")
        self.assertEqual(self.catalog()["counts"]["rendered"], 5)
        self.assertFalse(list(self.output.rglob('.render-*')))

    def test_modes_and_malformed_metadata(self):
        self.assertEqual(len(batch.collect_jobs(self.research, "atlases")), 2)
        self.assertEqual(len(batch.collect_jobs(self.research, "places")), 4)
        (self.research / "run-a/places/place-00001.json").write_text('bad json')
        self.assertEqual(self.run_batch(), 1)
        self.assertEqual(self.catalog()["counts"]["rendered"], 5)

    def test_gallery_escaping_links_and_failed_images(self):
        records = [{"archive": '<A & "B">', "title": '<script>alert(1)</script>', "kind": "atlas",
                    "image": 'partie é/a"&.png', "status": "rendered"},
                   {"archive": '<A & "B">', "title": "Failed", "kind": "place",
                    "image": "missing.png", "status": "failed"}]
        write_gallery(self.root, records)
        html = (self.root / 'index.html').read_text(encoding='utf-8')
        self.assertNotIn('<script>', html)
        self.assertIn('&lt;script&gt;', html)
        self.assertIn('partie%20%C3%A9/a%22%26.png', html)
        self.assertNotIn('missing.png', html)
        self.assertEqual(html.count('<img '), 1)

    def test_interrupted_cache_pass_preserves_remaining_proofs(self):
        self.run_batch()
        original = batch.cache_valid
        checks = 0
        def interrupt(*args, **kwargs):
            nonlocal checks
            checks += 1
            if checks == 3:
                raise KeyboardInterrupt
            return original(*args, **kwargs)
        with patch.object(batch, 'cache_valid', side_effect=interrupt):
            self.assertEqual(self.run_batch(), 130)
        self.assertEqual(self.catalog()['counts'], {'cached': 2, 'pending': 4})
        self.calls.clear()
        self.assertEqual(self.run_batch(), 0)
        self.assertEqual(self.calls, [])
        self.assertEqual(self.catalog()['counts'], {'cached': 6})

    def test_transient_windows_catalog_lock_is_retried(self):
        self.output.mkdir()
        original = Path.replace
        attempts = 0
        def locked(path, target):
            nonlocal attempts
            attempts += 1
            if attempts <= 2:
                raise PermissionError('transient reader lock')
            return original(path, target)
        with patch.object(Path, 'replace', locked), patch.object(batch.time, 'sleep'):
            batch.publish_catalog(self.output, [], 256, 1)
        self.assertEqual(attempts, 3)
        self.assertEqual(self.catalog()['images'], [])


if __name__ == '__main__':
    unittest.main()
