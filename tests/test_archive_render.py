"""Integration checks for accepted archive selection and immutable sources."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

EXE = (Path(sys.argv.pop(1)).resolve()
       if __name__ == "__main__" and len(sys.argv) > 1 and not sys.argv[1].startswith('-') else None)


@unittest.skipUnless(EXE and EXE.is_file(), "Pass the archive renderer executable")
class ArchiveRenderTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="liminal-atlas-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.research = self.root / "research"
        self.output = self.root / "sortie-épreuve" / "atlas.png"
        self.archive = self.make_archive("run-new", "20261120T232130Z")
        # Created later on disk, but it is an older game.
        self.make_archive("run-old", "20261119T120000Z")

    def write(self, path, value):
        path.write_text(json.dumps(value), encoding="utf-8")

    def make_archive(self, name, started):
        archive = self.research / name
        (archive / "places").mkdir(parents=True)
        self.write(archive / "manifest.json", {"started_utc": started})
        state = {"origin_place_id": "canonical:quarry_threshold", "current_place_id": "generated:room_0001",
                 "generated_rooms": [{"room_id": "generated:room_0001"}],
                 "room_links": [
                     {"from_place_id": "canonical:quarry_threshold", "to_place_id": "generated:room_0001", "direction": "north"},
                     {"from_place_id": "generated:room_0001", "to_place_id": "canonical:quarry_threshold", "direction": "south"}]}
        self.write(archive / "state.json", state)
        scene = ('camera eye(0,2,-10) target(0,2,5) up(0,1,0) fov(55)\n'
                 'plane "floor" pos(0,0,0) normal(0,1,0) size(100,100) gray(0.3)\n'
                 'box "rock" pos(0,3,2) size(6,6,8) rot(0,20,0) gray(0.5)\n'
                 'distant_light direction(-1,-1,0) intensity(2)\n')
        for index, place in enumerate([state["origin_place_id"], state["current_place_id"]]):
            stem = archive / "places" / f"place-{index:05d}"
            stem.with_suffix(".scene").write_bytes(scene.encode())
            self.write(stem.with_suffix(".json"), {"place_id": place, "title": "Test place",
                       "scene_sha256": hashlib.sha256(scene.encode()).hexdigest(),
                       "provenance": {"brief_and_landmark": json.dumps({"forward_direction": "north"})}})
        (archive / "turns").mkdir()
        (archive / "turns" / "turn-99999.candidate.scene").write_text("rejected invalid scene", encoding="utf-8")
        return archive

    def invoke(self, *extra, success=True):
        command = [str(EXE), "--research-dir", str(self.research), "--output", str(self.output),
                   "--size", "256", "--samples", "1", *extra]
        result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=60)
        if success:
            self.assertEqual(result.returncode, 0, result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def test_latest_accepted_complete_and_deterministic(self):
        before = {p: p.read_bytes() for p in self.research.rglob("*") if p.is_file()}
        self.invoke()
        data = self.output.read_bytes()
        self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", data[16:24]), (256, 256))
        report = json.loads(self.output.with_suffix(".json").read_text(encoding="utf-8"))
        self.assertEqual(report["archive"], "run-new")
        self.assertEqual(len(report["places"]), 2)
        self.assertEqual(report["places"][1]["grid"], [0, 1])
        self.invoke()
        self.assertEqual(data, self.output.read_bytes())
        self.assertEqual(before, {p: p.read_bytes() for p in self.research.rglob("*") if p.is_file()})

    def test_place_selection(self):
        self.invoke("--place", "generated:room_0001")
        report = json.loads(self.output.with_suffix(".json").read_text(encoding="utf-8"))
        self.assertEqual(report["mode"], "single_place")
        self.assertEqual(len(report["places"]), 1)

    def test_unicode_titles_and_small_atlas_labels(self):
        title = "Le Seuil des Géométries — Éclats & Ombres"
        path = self.archive / "places/place-00001.json"
        metadata = json.loads(path.read_text(encoding="utf-8"))
        metadata["title"] = title
        self.write(path, metadata)
        self.invoke()
        atlas = json.loads(self.output.with_suffix(".json").read_text(encoding="utf-8"))
        self.assertEqual(atlas["labels_version"], 2)
        self.assertEqual(atlas["captions"][1]["font_pixels"], 16)
        self.assertEqual(atlas["captions"][1]["text"], title)
        for caption in atlas["captions"]:
            if "bounds" in caption:
                x, y, width, height = caption["bounds"]
                self.assertGreaterEqual(x, 0)
                self.assertGreaterEqual(y, 0)
                self.assertLessEqual(x + width, 256)
                self.assertLessEqual(y + height, 256)
        self.assertIn("already labelled", self.invoke("--annotate", str(self.output), success=False).stderr)
        self.invoke("--place", "generated:room_0001")
        single = json.loads(self.output.with_suffix(".json").read_text(encoding="utf-8"))
        self.assertEqual(single["captions"][0]["font_pixels"], 12)
        self.assertEqual(single["labels_version"], 1)
        self.assertEqual(single["captions"][0]["text"], title)

    def test_hash_mismatch(self):
        path = self.archive / "places/place-00001.scene"
        path.write_bytes(path.read_bytes() + b"\n# changed\n")
        self.assertIn("hash mismatch", self.invoke(success=False).stderr)
        self.assertFalse(self.output.exists())

    def test_legacy_return_direction(self):
        path = self.archive / "places/place-00001.json"
        metadata = json.loads(path.read_text(encoding="utf-8"))
        metadata["provenance"]["brief_and_landmark"] = json.dumps({"return_direction": "south"})
        self.write(path, metadata)
        self.invoke()
        report = json.loads(self.output.with_suffix(".json").read_text(encoding="utf-8"))
        self.assertEqual(report["places"][1]["yaw_degrees"], 0)

    def test_inconsistent_graph(self):
        path = self.archive / "state.json"
        state = json.loads(path.read_text())
        state["room_links"][1]["direction"] = "east"
        self.write(path, state)
        self.assertIn("inconsistent", self.invoke(success=False).stderr)

    def test_missing_place(self):
        (self.archive / "places/place-00001.json").unlink()
        self.assertIn("do not match", self.invoke(success=False).stderr)

    def test_invalid_arguments_and_archive_protection(self):
        for option, value in [("--size", "256junk"), ("--elevation", "nan"), ("--samples", "0"),
                              ("--output", str(self.archive / "atlas.png"))]:
            with self.subTest(option=option):
                self.invoke(option, value, success=False)
        self.assertFalse((self.archive / "atlas.png").exists())


if __name__ == "__main__":
    unittest.main()
