"""Synthetic tests for the offline ambience analysis pilot.

The suite never invokes ffmpeg or ffprobe.  Tool versions, decoding and loudness
measurement are either injected into :func:`build_catalog` or mocked at the
subprocess boundary, so the tests stay deterministic on a developer machine.
"""

import copy
import json
import math
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import numpy as np

import analyze_ambience as ambience


SAMPLE_RATE = 48000
TOOLS = {
    "python": "3.12.0",
    "numpy": "2.0.2",
    "scipy": "1.14.1",
    "ffmpeg": "ffmpeg version synthetic",
    "ffprobe": "ffprobe version synthetic",
}
PROJECT_ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = PROJECT_ROOT / "assets" / "sfx" / "ambience_analysis_config.json"
ANNOTATIONS_PATH = PROJECT_ROOT / "assets" / "sfx" / "ambience_annotations.json"


def _write_json(path, value):
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n",
        encoding="utf-8",
    )


def _sine(frequency, seconds=1.0, rate=SAMPLE_RATE, amplitude=0.5):
    count = int(round(seconds * rate))
    time = np.arange(count, dtype=np.float64) / float(rate)
    return (amplitude * np.sin(2.0 * math.pi * frequency * time)).reshape(-1, 1)


def _fake_loudness(path, timeout):
    del path, timeout
    return {
        "integrated_lufs": -16.0,
        "true_peak_dbtp": -1.0,
        "method": "synthetic loudness",
        "units": {"integrated_lufs": "LUFS", "true_peak_dbtp": "dBTP"},
        "reasons": {},
    }


def _decode_factory(calls):
    def decode(path):
        name = Path(path).name
        calls.append(name)
        track_id = name[: -len("-loop.ogg")]
        index = ambience.TRACK_IDS.index(track_id)
        return _sine(300.0 + 100.0 * index, seconds=0.25, amplitude=0.2), SAMPLE_RATE, 1

    return decode


def _assert_json_safe(case, value):
    text = json.dumps(value, allow_nan=False, sort_keys=True)
    for token in ("NaN", "Infinity", "-Infinity"):
        case.assertNotIn(token, text)


def _assert_sorted_keys(case, node):
    if isinstance(node, dict):
        case.assertEqual(list(node), sorted(node))
        for item in node.values():
            _assert_sorted_keys(case, item)
    elif isinstance(node, list):
        for item in node:
            _assert_sorted_keys(case, item)


def _assert_rounded(case, node):
    if isinstance(node, dict):
        for item in node.values():
            _assert_rounded(case, item)
    elif isinstance(node, list):
        for item in node:
            _assert_rounded(case, item)
    elif isinstance(node, float):
        case.assertEqual(node, round(node, 6))


class ConfigTests(unittest.TestCase):
    def test_shipped_config_matches_default_policy(self):
        shipped = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        self.assertEqual(shipped, ambience.default_config())
        self.assertEqual(ambience.load_config(CONFIG_PATH), ambience.default_config())

    def test_shipped_annotations_start_empty(self):
        shipped = json.loads(ANNOTATIONS_PATH.read_text(encoding="utf-8"))
        self.assertEqual(shipped, {"schema_version": 1, "tracks": {}})

    def test_load_config_rounds_to_six_decimals(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "config.json"
            config = ambience.default_config()
            config["spectrum"]["epsilon"] = 0.0000014
            _write_json(path, config)
            loaded = ambience.load_config(path)
            self.assertEqual(loaded["spectrum"]["epsilon"], 0.000001)

    def test_rejects_invalid_policy_values(self):
        mutations = (
            ("unknown spectrum key", lambda c: c["spectrum"].__setitem__("mystery", 1)),
            ("non power of two window", lambda c: c["spectrum"].__setitem__("window_size", 2000)),
            ("hop larger than window", lambda c: c["spectrum"].__setitem__("hop_size", 4096)),
            ("rounding epsilon to zero", lambda c: c["spectrum"].__setitem__("epsilon", 1e-20)),
            ("wrong analysis rate", lambda c: c["spectrum"].__setitem__("sample_rate_hz", 44100)),
            (
                "band edge at Nyquist",
                lambda c: c["bands"].__setitem__("edges_hz", [0.0, 250.0, 2000.0, 24000.0]),
            ),
            ("unordered band edges", lambda c: c["bands"].__setitem__("edges_hz", [0.0, 2000.0, 250.0, 8000.0])),
            ("negative flux threshold", lambda c: c["flux"].__setitem__("peak_threshold", -0.1)),
            ("playback gain above one", lambda c: c["playback"].__setitem__("max_gain", 1.5)),
            ("empty note", lambda c: c["silence"].__setitem__("note", " ")),
            ("non finite threshold", lambda c: c["silence"].__setitem__("rms_dbfs", float("nan"))),
            ("disabled loudness with zero timeout", lambda c: c["loudness"].__setitem__("timeout_seconds", 0.0)),
        )
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "config.json"
            for label, mutate in mutations:
                with self.subTest(label=label):
                    config = ambience.default_config()
                    mutate(config)
                    # Plain json.dumps keeps NaN visible for the strict loader to reject.
                    path.write_text(json.dumps(config), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        ambience.load_config(path)

    def test_rejects_non_strict_json(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "config.json"
            path.write_text('{"config_version": 1, "config_version": 1}', encoding="utf-8")
            with self.assertRaises(ValueError):
                ambience.load_config(path)
            path.write_text('{"config_version": NaN}', encoding="utf-8")
            with self.assertRaises(ValueError):
                ambience.load_config(path)


class AnalysisTests(unittest.TestCase):
    def setUp(self):
        self.config = ambience.default_config()

    def test_silence_nulls_every_metric_with_reasons(self):
        analysis = ambience.analyze_pcm(
            np.zeros((SAMPLE_RATE, 2), dtype=np.float64), SAMPLE_RATE, self.config
        )
        self.assertTrue(analysis["silence"]["detected"])
        self.assertEqual(analysis["spectrum"]["frame_count"], 0)

        null_paths = (
            ("level", "rms_dbfs"),
            ("level", "peak_dbfs"),
            ("level", "crest_db"),
            ("spectrum", "centroid_hz"),
            ("spectrum", "rolloff85_hz"),
            ("spectrum", "flatness"),
            ("spectrum", "flux_mean_positive"),
            ("spectrum", "flux_peak_rate_hz"),
            ("dynamics", "spread_db"),
            ("stereo", "correlation"),
            ("stereo", "side_mid_ratio"),
            ("junction", "start_rms_dbfs"),
            ("junction", "end_rms_dbfs"),
            ("junction", "delta_db"),
        )
        for section, key in null_paths:
            with self.subTest(section=section, key=key):
                self.assertIsNone(analysis[section][key])
                self.assertIn(key, analysis[section]["reasons"])
        for value in analysis["dynamics"]["rms_percentiles_dbfs"].values():
            self.assertIsNone(value)
        for name in ambience.BAND_NAMES:
            self.assertIsNone(analysis["bands"]["shares"][name])
            self.assertIn(name, analysis["bands"]["reasons"])
        self.assertIsNone(analysis["bands"]["share_sum"])
        self.assertIn("share_sum", analysis["bands"]["reasons"])
        for key in ("integrated_lufs", "true_peak_dbtp"):
            self.assertIsNone(analysis["loudness"][key])
            self.assertIn(key, analysis["loudness"]["reasons"])
        _assert_json_safe(self, analysis)

    def test_signal_below_threshold_is_silent(self):
        quiet = np.full((SAMPLE_RATE, 1), 1e-5, dtype=np.float64)
        analysis = ambience.analyze_pcm(quiet, SAMPLE_RATE, self.config)
        self.assertTrue(analysis["silence"]["detected"])

    def test_empty_signal_is_silent(self):
        analysis = ambience.analyze_pcm(np.zeros((0, 1)), SAMPLE_RATE, self.config)
        self.assertTrue(analysis["silence"]["detected"])
        self.assertIn("empty signal", analysis["level"]["reasons"]["rms_dbfs"])
        _assert_json_safe(self, analysis)

    def test_known_sine_frequency_bands_and_levels(self):
        analysis = ambience.analyze_pcm(_sine(1000.0), SAMPLE_RATE, self.config)
        self.assertFalse(analysis["silence"]["detected"])

        expected_rms = 20.0 * math.log10(0.5 / math.sqrt(2.0))
        self.assertAlmostEqual(analysis["level"]["rms_dbfs"], expected_rms, delta=0.05)
        self.assertAlmostEqual(
            analysis["level"]["peak_dbfs"], 20.0 * math.log10(0.5), delta=0.01
        )
        self.assertAlmostEqual(analysis["level"]["crest_db"], 3.0103, delta=0.05)

        self.assertAlmostEqual(analysis["spectrum"]["centroid_hz"], 1000.0, delta=50.0)
        self.assertGreaterEqual(analysis["spectrum"]["rolloff85_hz"], 900.0)
        self.assertLessEqual(analysis["spectrum"]["rolloff85_hz"], 1100.0)
        self.assertLess(analysis["spectrum"]["flatness"], 0.1)

        shares = analysis["bands"]["shares"]
        self.assertGreater(shares["mid"], 0.95)
        self.assertLess(shares["low"], 0.02)
        self.assertLess(shares["high"], 0.02)
        self.assertAlmostEqual(analysis["bands"]["share_sum"], 1.0, delta=1e-9)
        self.assertAlmostEqual(analysis["junction"]["delta_db"], 0.0, delta=0.01)
        _assert_json_safe(self, analysis)

    def test_one_dimensional_input_is_mono(self):
        one_dimensional = _sine(1000.0)[:, 0]
        columns = one_dimensional.reshape(-1, 1)
        flat = ambience.analyze_pcm(one_dimensional, SAMPLE_RATE, self.config)
        framed = ambience.analyze_pcm(columns, SAMPLE_RATE, self.config)
        self.assertEqual(flat["stereo"]["channels"], 1)
        self.assertAlmostEqual(
            flat["level"]["rms_dbfs"], framed["level"]["rms_dbfs"], places=12
        )
        self.assertEqual(
            flat["spectrum"]["centroid_hz"], framed["spectrum"]["centroid_hz"]
        )


    def test_white_noise_is_diffuse(self):
        generator = np.random.default_rng(20261027)
        noise = (0.1 * generator.standard_normal((SAMPLE_RATE, 1))).astype(np.float64)
        analysis = ambience.analyze_pcm(noise, SAMPLE_RATE, self.config)
        self.assertGreater(analysis["spectrum"]["flatness"], 0.35)
        self.assertGreaterEqual(analysis["spectrum"]["centroid_hz"], 8000.0)
        self.assertLessEqual(analysis["spectrum"]["centroid_hz"], 16000.0)
        self.assertGreater(analysis["level"]["crest_db"], 6.0)
        for name in ambience.BAND_NAMES:
            self.assertIsNotNone(analysis["bands"]["shares"][name])
            self.assertGreater(analysis["bands"]["shares"][name], 0.0)
        self.assertAlmostEqual(analysis["bands"]["share_sum"], 1.0, delta=1e-9)
        _assert_json_safe(self, analysis)

    def test_impulse_train_has_marked_crest_flux_and_spread(self):
        impulses = np.zeros((SAMPLE_RATE, 1), dtype=np.float64)
        impulses[::6000, 0] = 0.9
        analysis = ambience.analyze_pcm(impulses, SAMPLE_RATE, self.config)
        self.assertAlmostEqual(
            analysis["level"]["peak_dbfs"], 20.0 * math.log10(0.9), delta=0.01
        )
        self.assertGreater(analysis["level"]["crest_db"], 20.0)
        self.assertGreater(analysis["dynamics"]["spread_db"], 40.0)
        self.assertGreater(analysis["spectrum"]["flux_peak_rate_hz"], 2.0)
        self.assertGreater(analysis["spectrum"]["flatness"], 0.2)
        _assert_json_safe(self, analysis)

    def test_amplitude_change_moves_percentiles_and_junction(self):
        loud = _sine(440.0, seconds=1.0, amplitude=0.5)
        quiet = _sine(440.0, seconds=1.0, amplitude=0.02)
        signal = np.vstack([loud, quiet])
        analysis = ambience.analyze_pcm(signal, SAMPLE_RATE, self.config)
        spread = analysis["dynamics"]["spread_db"]
        self.assertGreater(spread, 20.0)
        self.assertLess(spread, 40.0)
        self.assertLess(analysis["junction"]["delta_db"], -20.0)
        self.assertEqual(len(analysis["junction"]["channels"]), 1)
        self.assertLess(
            analysis["junction"]["channels"][0]["end_rms_dbfs"],
            analysis["junction"]["channels"][0]["start_rms_dbfs"],
        )
        _assert_json_safe(self, analysis)

    def test_mono_and_anti_phase_share_spectral_metrics(self):
        base = _sine(1000.0)[:, 0]
        mono = ambience.analyze_pcm(base.reshape(-1, 1), SAMPLE_RATE, self.config)
        anti_phase = ambience.analyze_pcm(
            np.column_stack([base, -base]), SAMPLE_RATE, self.config
        )
        for key in (
            "centroid_hz",
            "rolloff85_hz",
            "flatness",
            "flux_mean_positive",
            "flux_peak_rate_hz",
        ):
            with self.subTest(metric=key):
                self.assertAlmostEqual(
                    mono["spectrum"][key], anti_phase["spectrum"][key], places=9
                )
        for name in ambience.BAND_NAMES:
            with self.subTest(band=name):
                self.assertAlmostEqual(
                    mono["bands"]["shares"][name],
                    anti_phase["bands"]["shares"][name],
                    places=9,
                )
        self.assertAlmostEqual(
            mono["level"]["rms_dbfs"], anti_phase["level"]["rms_dbfs"], places=12
        )
        self.assertAlmostEqual(anti_phase["stereo"]["correlation"], -1.0, places=9)
        self.assertIsNone(anti_phase["stereo"]["side_mid_ratio"])
        self.assertIn("mid energy is zero", anti_phase["stereo"]["reasons"]["side_mid_ratio"])
        self.assertIsNone(mono["stereo"]["correlation"])
        self.assertIn("mono source", mono["stereo"]["reasons"]["correlation"])

    def test_correlated_stereo_ratio(self):
        base = _sine(1000.0)[:, 0]
        analysis = ambience.analyze_pcm(
            np.column_stack([base, 0.5 * base]), SAMPLE_RATE, self.config
        )
        self.assertAlmostEqual(analysis["stereo"]["correlation"], 1.0, places=9)
        self.assertAlmostEqual(analysis["stereo"]["side_mid_ratio"], 1.0 / 9.0, delta=1e-9)

    def test_low_sample_rate_marks_top_band_unavailable(self):
        generator = np.random.default_rng(4242)
        low_rate = 8000
        noise = (0.1 * generator.standard_normal((low_rate, 1))).astype(np.float64)
        analysis = ambience.analyze_pcm(noise, low_rate, self.config)
        self.assertEqual(analysis["spectrum"]["source_nyquist_hz"], 4000.0)
        self.assertEqual(analysis["spectrum"]["analysed_limit_hz"], 4000.0)
        self.assertIsNone(analysis["bands"]["shares"]["very_high"])
        self.assertIn("above the analysed limit", analysis["bands"]["reasons"]["very_high"])
        self.assertEqual(analysis["bands"]["coverage"]["very_high"], 0.0)
        self.assertAlmostEqual(
            analysis["bands"]["coverage"]["high"], (4000.0 - 2000.0) / 6000.0, delta=1e-9
        )
        self.assertIsNotNone(analysis["bands"]["shares"]["high"])
        self.assertAlmostEqual(analysis["bands"]["share_sum"], 1.0, delta=1e-9)
        _assert_json_safe(self, analysis)

    def test_rejects_bad_inputs(self):
        for bad in (float("nan"), float("inf"), float("-inf")):
            with self.subTest(value=bad), self.assertRaises(ValueError):
                ambience.analyze_pcm(np.array([[bad]]), SAMPLE_RATE, self.config)
        with self.assertRaises(ValueError):
            ambience.analyze_pcm(np.zeros((10, 1, 1)), SAMPLE_RATE, self.config)
        with self.assertRaises(ValueError):
            ambience.analyze_pcm(np.zeros((10, 0)), SAMPLE_RATE, self.config)
        with self.assertRaises(ValueError):
            ambience.analyze_pcm(np.zeros((10, 1)), 0, self.config)
        with self.assertRaises(ValueError):
            ambience.analyze_pcm(np.zeros((10, 1)), True, self.config)


class DescriptionTests(unittest.TestCase):
    def setUp(self):
        self.config = ambience.default_config()

    def test_description_is_deterministic_without_identifiers(self):
        analysis = ambience.analyze_pcm(_sine(1000.0), SAMPLE_RATE, self.config)
        first = ambience.describe(analysis, self.config)
        second = ambience.describe(copy.deepcopy(analysis), self.config)
        self.assertEqual(first, second)
        self.assertTrue(first.endswith("."))
        words = first.split()
        self.assertGreaterEqual(len(words), 12)
        self.assertLessEqual(len(words), 45)
        lowered = first.lower()
        for track_id in ambience.TRACK_IDS:
            self.assertNotIn(track_id, lowered)

    def test_silent_description_is_shorter(self):
        silent = ambience.analyze_pcm(np.zeros((SAMPLE_RATE, 1)), SAMPLE_RATE, self.config)
        audible = ambience.analyze_pcm(_sine(1000.0), SAMPLE_RATE, self.config)
        silent_text = ambience.describe(silent, self.config)
        audible_text = ambience.describe(audible, self.config)
        self.assertIn("Silence", silent_text)
        self.assertLess(len(silent_text.split()), len(audible_text.split()))

    def test_describe_does_not_concatenate_annotation(self):
        analysis = ambience.analyze_pcm(_sine(1000.0), SAMPLE_RATE, self.config)
        text = ambience.describe(analysis, self.config)
        self.assertNotIn("Souffle humain", text)


class AnnotationTests(unittest.TestCase):
    def test_missing_file_means_no_annotation(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(
                ambience.load_annotations(Path(folder) / "missing.json"), {}
            )

    def test_valid_entry_and_defaults(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "annotations.json"
            _write_json(
                path,
                {
                    "schema_version": 1,
                    "tracks": {
                        "blade": {"text": "Souffle humain.", "provenance": "human"},
                        "bug": {"trim_db": 0.0},
                    },
                },
            )
            loaded = ambience.load_annotations(path)
            self.assertEqual(
                loaded["blade"],
                {
                    "text": "Souffle humain.",
                    "provenance": "human",
                    "trim_db": 0.0,
                    "trim_validated": False,
                },
            )
            self.assertEqual(
                loaded["bug"],
                {
                    "text": None,
                    "provenance": None,
                    "trim_db": 0.0,
                    "trim_validated": False,
                },
            )

    def test_trim_bounds_and_validation(self):
        valid = (
            ({"text": "x", "trim_db": -24.0, "trim_validated": True}, -24.0),
            ({"text": "x", "trim_db": 6.0, "trim_validated": True}, 6.0),
            ({"text": "x", "trim_db": 0.0}, 0.0),
            ({"text": "x", "trim_db": 0.0, "trim_validated": True}, 0.0),
        )
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "annotations.json"
            for entry, expected in valid:
                with self.subTest(entry=entry):
                    _write_json(
                        path,
                        {"schema_version": 1, "tracks": {"blade": entry}},
                    )
                    loaded = ambience.load_annotations(path)
                    self.assertEqual(loaded["blade"]["trim_db"], expected)

            invalid = (
                {"text": "x", "trim_db": -3.0},
                {"text": "x", "trim_db": 3.0, "trim_validated": False},
                {"text": "x", "trim_db": -24.0001, "trim_validated": True},
                {"text": "x", "trim_db": 6.0001, "trim_validated": True},
                {"text": "x", "trim_db": "0"},
                {"text": "x", "trim_validated": "yes"},
            )
            for entry in invalid:
                with self.subTest(entry=entry):
                    _write_json(
                        path,
                        {"schema_version": 1, "tracks": {"blade": entry}},
                    )
                    with self.assertRaises(ValueError):
                        ambience.load_annotations(path)

    def test_rejects_invalid_documents(self):
        documents = (
            {"schema_version": 2, "tracks": {}},
            {"schema_version": 1},
            {"schema_version": 1, "tracks": []},
            {"schema_version": 1, "tracks": {"unknown": {"text": "x"}}},
            {"schema_version": 1, "tracks": {"blade": {"text": " "}}},
            {"schema_version": 1, "tracks": {"blade": {"text": "x", "provenance": "llm"}}},
            {"schema_version": 1, "tracks": {"blade": {"provenance": "human"}}},
            {"schema_version": 1, "tracks": {"blade": {"text": "x", "extra": 1}}},
        )
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "annotations.json"
            for document in documents:
                with self.subTest(document=document):
                    _write_json(path, document)
                    with self.assertRaises(ValueError):
                        ambience.load_annotations(path)

            path.write_text(
                '{"schema_version": 1, "schema_version": 1, "tracks": {}}',
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                ambience.load_annotations(path)

            path.write_text(
                '{"schema_version": 1, "tracks": {"blade": {"text": "x", "trim_db": NaN}}}',
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                ambience.load_annotations(path)


class _CatalogFixture(unittest.TestCase):
    def setUp(self):
        self._temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self._temporary.cleanup)
        self.root = Path(self._temporary.name)
        self.output = self.root / "ambience_features.json"
        self.config_path = self.root / "ambience_analysis_config.json"
        self.annotations_path = self.root / "ambience_annotations.json"
        self.calls = []
        self.source_bytes = {}
        for track_id in ambience.TRACK_IDS:
            payload = ("source bytes for " + track_id).encode("utf-8")
            (self.root / (track_id + "-loop.ogg")).write_bytes(payload)
            self.source_bytes[track_id] = payload
        _write_json(self.config_path, ambience.default_config())
        _write_json(self.annotations_path, {"schema_version": 1, "tracks": {}})

        for name in ("collect_tools", "decode_audio", "measure_loudness"):
            patcher = patch.object(
                ambience, name, side_effect=AssertionError(name + " must not run")
            )
            patcher.start()
            self.addCleanup(patcher.stop)

    def build(self, *, force=False, tools=None, decode=None, loudness=None):
        return ambience.build_catalog(
            self.root,
            self.output,
            self.config_path,
            self.annotations_path,
            force=force,
            tool_versions=TOOLS if tools is None else tools,
            decode=_decode_factory(self.calls) if decode is None else decode,
            loudness_measure=_fake_loudness if loudness is None else loudness,
        )


class CatalogTests(_CatalogFixture):
    def test_output_cannot_overwrite_sources_or_editorial_inputs(self):
        for path in (self.root / "blade-loop.ogg", self.config_path, self.annotations_path):
            before = path.read_bytes()
            self.output = path
            with self.assertRaisesRegex(ValueError, "must not overwrite"):
                self.build()
            self.assertEqual(path.read_bytes(), before)

    def test_root_and_track_schema(self):
        catalog = self.build()
        self.assertEqual(
            set(catalog),
            {
                "schema_version",
                "script_version",
                "tools",
                "config",
                "config_sha256",
                "catalog_version",
                "tracks",
            },
        )
        self.assertEqual(catalog["schema_version"], 1)
        self.assertEqual(catalog["script_version"], ambience.SCRIPT_VERSION)
        self.assertEqual(catalog["tools"], TOOLS)
        self.assertEqual(catalog["config"], ambience.load_config(self.config_path))
        self.assertEqual(len(catalog["config_sha256"]), 64)
        self.assertEqual(len(catalog["catalog_version"]), 64)
        self.assertEqual(
            [track["id"] for track in catalog["tracks"]], list(ambience.TRACK_IDS)
        )
        for index, track in enumerate(catalog["tracks"]):
            track_id = ambience.TRACK_IDS[index]
            with self.subTest(track=track_id):
                self.assertEqual(
                    set(track),
                    {
                        "id",
                        "path",
                        "sha256",
                        "duration_seconds",
                        "sample_rate",
                        "channels",
                        "analysis",
                        "description",
                        "rule_version",
                        "annotation",
                        "playback",
                    },
                )
                self.assertEqual(track["path"], "assets/sfx/" + track_id + "-loop.ogg")
                self.assertEqual(track["sample_rate"], SAMPLE_RATE)
                self.assertEqual(track["channels"], 1)
                self.assertEqual(track["duration_seconds"], 0.25)
                self.assertEqual(
                    set(track["analysis"]),
                    {
                        "silence",
                        "level",
                        "spectrum",
                        "bands",
                        "dynamics",
                        "stereo",
                        "junction",
                        "loudness",
                    },
                )
                self.assertFalse(track["analysis"]["silence"]["detected"])
                self.assertEqual(track["rule_version"], ambience.RULE_VERSION)
                self.assertIsNone(track["annotation"])
                self.assertEqual(
                    track["playback"],
                    {"trim_db": 0.0, "max_gain": 1.0, "trim_validated": False},
                )
                self.assertEqual(
                    track["sha256"],
                    ambience.sha256_file(
                        self.root / (track_id + "-loop.ogg")
                    ),
                )

    def test_loudness_values_are_included(self):
        catalog = self.build()
        for track in catalog["tracks"]:
            with self.subTest(track=track["id"]):
                self.assertEqual(track["analysis"]["loudness"]["integrated_lufs"], -16.0)
                self.assertEqual(track["analysis"]["loudness"]["true_peak_dbtp"], -1.0)

    def test_descriptions_are_acoustic_and_identifier_free(self):
        catalog = self.build()
        for track in catalog["tracks"]:
            description = track["description"]
            with self.subTest(track=track["id"]):
                self.assertTrue(description.endswith("."))
                self.assertGreaterEqual(len(description.split()), 12)
                self.assertLessEqual(len(description.split()), 45)
                lowered = description.lower()
                for track_id in ambience.TRACK_IDS:
                    self.assertNotIn(track_id, lowered)
                self.assertNotIn("source bytes", lowered)

    def test_written_json_is_sorted_rounded_and_strict(self):
        catalog = self.build()
        text = self.output.read_text(encoding="utf-8")
        self.assertTrue(text.endswith("\n"))
        for token in ("NaN", "Infinity", "-Infinity"):
            self.assertNotIn(token, text)
        self.assertNotIn("timestamp", text.lower())
        parsed = json.loads(text)
        self.assertEqual(parsed, catalog)
        _assert_sorted_keys(self, parsed)
        _assert_rounded(self, parsed)
        pairs = json.loads(text, object_pairs_hook=lambda items: items)
        keys = [key for key, _ in pairs]
        self.assertEqual(keys, sorted(keys))

    def test_sources_are_never_modified(self):
        self.build()
        for track_id, payload in self.source_bytes.items():
            with self.subTest(track=track_id):
                self.assertEqual(
                    (self.root / (track_id + "-loop.ogg")).read_bytes(), payload
                )


class CacheTests(_CatalogFixture):
    def test_cache_reuses_analysis_and_rereads_annotations(self):
        first = self.build()
        self.assertEqual(len(self.calls), 4)
        first_text = self.output.read_text(encoding="utf-8")

        second = self.build()
        self.assertEqual(len(self.calls), 4)
        self.assertEqual(second, first)
        self.assertEqual(self.output.read_text(encoding="utf-8"), first_text)

        _write_json(
            self.annotations_path,
            {
                "schema_version": 1,
                "tracks": {
                    "blade": {
                        "text": "Souffle humain.",
                        "provenance": "human",
                        "trim_db": -3.0,
                        "trim_validated": True,
                    }
                },
            },
        )
        annotations_text = self.annotations_path.read_text(encoding="utf-8")
        third = self.build()
        self.assertEqual(len(self.calls), 4)
        self.assertEqual(
            third["tracks"][0]["annotation"],
            {"text": "Souffle humain.", "provenance": "human"},
        )
        self.assertEqual(
            third["tracks"][0]["playback"],
            {"trim_db": -3.0, "max_gain": 1.0, "trim_validated": True},
        )
        self.assertIsNone(third["tracks"][1]["annotation"])
        for track in third["tracks"]:
            self.assertNotIn("Souffle humain", track["description"])
        self.assertEqual(
            self.annotations_path.read_text(encoding="utf-8"), annotations_text
        )
        self.assertEqual(
            third["tracks"][0]["sha256"], first["tracks"][0]["sha256"]
        )
        self.assertNotEqual(third["catalog_version"], first["catalog_version"])

    def test_cache_invalidated_by_config_force_tools_and_source(self):
        first = self.build()
        self.assertEqual(len(self.calls), 4)

        changed_config = ambience.default_config()
        changed_config["flux"]["peak_threshold"] = 0.03
        _write_json(self.config_path, changed_config)
        second = self.build()
        self.assertEqual(len(self.calls), 8)
        self.assertNotEqual(second["config_sha256"], first["config_sha256"])
        self.assertEqual(second["config"]["flux"]["peak_threshold"], 0.03)

        forced = self.build(force=True)
        self.assertEqual(len(self.calls), 12)
        self.assertEqual(forced, second)

        changed_tools = dict(TOOLS)
        changed_tools["ffmpeg"] = "ffmpeg version 9.9"
        third = self.build(tools=changed_tools)
        self.assertEqual(len(self.calls), 16)
        self.assertEqual(third["tools"]["ffmpeg"], "ffmpeg version 9.9")

        old_blade_sha = third["tracks"][0]["sha256"]
        (self.root / "blade-loop.ogg").write_bytes(b"changed blade source")
        fourth = self.build(tools=changed_tools)
        self.assertEqual(len(self.calls), 17)
        self.assertEqual(self.calls[-1], "blade-loop.ogg")
        self.assertNotEqual(fourth["tracks"][0]["sha256"], old_blade_sha)
        for index in (1, 2, 3):
            with self.subTest(track=ambience.TRACK_IDS[index]):
                self.assertEqual(
                    fourth["tracks"][index]["sha256"], third["tracks"][index]["sha256"]
                )
        self.assertTrue(self.output.is_file())

        with patch.object(ambience, "SCRIPT_VERSION", "0.0.0"):
            fifth = self.build(tools=changed_tools)
        self.assertEqual(len(self.calls), 21)
        self.assertEqual(fifth["script_version"], "0.0.0")

    def test_annotation_error_preserves_previous_catalogue(self):
        self.build()
        previous = self.output.read_text(encoding="utf-8")
        _write_json(
            self.annotations_path,
            {
                "schema_version": 1,
                "tracks": {"blade": {"text": "x", "trim_db": -3.0}},
            },
        )
        with self.assertRaisesRegex(ValueError, "trim_validated"):
            self.build()
        self.assertEqual(len(self.calls), 4)
        self.assertEqual(self.output.read_text(encoding="utf-8"), previous)

    def test_decode_failure_preserves_previous_catalogue(self):
        self.build()
        previous = self.output.read_text(encoding="utf-8")

        def failing_decode(path):
            raise RuntimeError("corrupt source " + Path(path).name)

        with self.assertRaisesRegex(RuntimeError, "corrupt source"):
            self.build(force=True, decode=failing_decode)
        self.assertEqual(self.output.read_text(encoding="utf-8"), previous)
        self.assertEqual(
            list(self.root.glob(self.output.name + ".*.tmp")), []
        )


class AtomicTests(_CatalogFixture):
    def test_successful_write_revalidates(self):
        catalog = self.build()
        ambience.write_catalog(self.output, catalog)
        self.assertEqual(json.loads(self.output.read_text(encoding="utf-8")), catalog)

    def test_invalid_catalogue_preserves_previous_file(self):
        catalog = self.build()
        previous = self.output.read_text(encoding="utf-8")

        stale = copy.deepcopy(catalog)
        stale["tracks"][0]["duration_seconds"] = 1.0
        with self.assertRaises(ValueError):
            ambience.write_catalog(self.output, stale)
        self.assertEqual(self.output.read_text(encoding="utf-8"), previous)
        self.assertEqual(list(self.root.glob(self.output.name + ".*.tmp")), [])

        non_finite = copy.deepcopy(catalog)
        non_finite["tracks"][0]["analysis"]["level"]["rms_dbfs"] = float("nan")
        with self.assertRaises(ValueError):
            ambience.write_catalog(self.output, non_finite)
        self.assertEqual(self.output.read_text(encoding="utf-8"), previous)
        self.assertEqual(list(self.root.glob(self.output.name + ".*.tmp")), [])

    def test_replace_failure_preserves_previous_file_and_cleans_temp(self):
        catalog = self.build()
        previous = self.output.read_text(encoding="utf-8")
        with patch.object(
            ambience.os, "replace", side_effect=OSError("replace failed")
        ):
            with self.assertRaises(OSError):
                ambience.write_catalog(self.output, catalog)
        self.assertEqual(self.output.read_text(encoding="utf-8"), previous)
        self.assertEqual(list(self.root.glob(self.output.name + ".*.tmp")), [])


class SourceTests(unittest.TestCase):
    def test_missing_sources_are_reported_explicitly(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for track_id in ("blade", "bug", "night"):
                (root / (track_id + "-loop.ogg")).write_bytes(b"x")
            with self.assertRaises(FileNotFoundError) as caught:
                ambience.resolve_sources(root)
            message = str(caught.exception)
            self.assertIn("static-loop.ogg", message)
            self.assertNotIn("blade-loop.ogg", message)

    def test_all_four_sources_are_resolved_in_documented_order(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            for track_id in ambience.TRACK_IDS:
                (root / (track_id + "-loop.ogg")).write_bytes(b"x")
            (root / "unrelated.txt").write_text("ignore", encoding="utf-8")
            sources = ambience.resolve_sources(root)
            self.assertEqual(list(sources), list(ambience.TRACK_IDS))
            for track_id in ambience.TRACK_IDS:
                self.assertEqual(sources[track_id].name, track_id + "-loop.ogg")

    def test_missing_source_aborts_build_without_writing(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            config_path = root / "config.json"
            _write_json(config_path, ambience.default_config())
            output = root / "features.json"
            with self.assertRaises(FileNotFoundError):
                ambience.build_catalog(
                    root,
                    output,
                    config_path,
                    None,
                    tool_versions=TOOLS,
                    decode=lambda path: (_ for _ in ()).throw(
                        AssertionError("decode must not run")
                    ),
                    loudness_measure=_fake_loudness,
                )
            self.assertFalse(output.exists())


class DecodeTests(unittest.TestCase):
    def test_decodes_native_float32_without_resampling(self):
        samples = np.array([[0.1, -0.1], [0.2, -0.2]], dtype="<f4")
        completed = subprocess.CompletedProcess(
            args=["ffmpeg"], returncode=0, stdout=samples.tobytes(), stderr=b""
        )
        with patch.object(
            ambience, "probe_audio", return_value=(44100, 2)
        ), patch.object(ambience.subprocess, "run", return_value=completed):
            decoded, rate, channels = ambience.decode_audio("fixture.ogg")
        self.assertEqual(rate, 44100)
        self.assertEqual(channels, 2)
        self.assertEqual(decoded.shape, (2, 2))
        np.testing.assert_allclose(decoded, samples.astype(np.float64))

    def test_rejects_partial_or_failed_decode(self):
        partial = subprocess.CompletedProcess(
            args=["ffmpeg"], returncode=0, stdout=b"\x00\x00\x00", stderr=b""
        )
        with patch.object(
            ambience, "probe_audio", return_value=(48000, 1)
        ), patch.object(ambience.subprocess, "run", return_value=partial):
            with self.assertRaisesRegex(RuntimeError, "partial"):
                ambience.decode_audio("fixture.ogg")

        failed = subprocess.CompletedProcess(
            args=["ffmpeg"], returncode=1, stdout=b"", stderr=b"bad file"
        )
        with patch.object(
            ambience, "probe_audio", return_value=(48000, 1)
        ), patch.object(ambience.subprocess, "run", return_value=failed):
            with self.assertRaisesRegex(RuntimeError, "failed to decode"):
                ambience.decode_audio("fixture.ogg")

        non_finite = np.array([[float("nan")]], dtype="<f4")
        corrupt = subprocess.CompletedProcess(
            args=["ffmpeg"], returncode=0, stdout=non_finite.tobytes(), stderr=b""
        )
        with patch.object(
            ambience, "probe_audio", return_value=(48000, 1)
        ), patch.object(ambience.subprocess, "run", return_value=corrupt):
            with self.assertRaisesRegex(RuntimeError, "non-finite"):
                ambience.decode_audio("fixture.ogg")


class LoudnessTests(unittest.TestCase):
    def test_parses_loudnorm_json(self):
        completed = subprocess.CompletedProcess(
            args=["ffmpeg"],
            returncode=0,
            stdout="",
            stderr='summary\n{\n  "input_i" : "-16.0",\n  "input_tp" : "-1.2"\n}\n',
        )
        with patch.object(ambience.subprocess, "run", return_value=completed):
            block = ambience.measure_loudness("fixture.ogg", 5.0)
        self.assertEqual(block["integrated_lufs"], -16.0)
        self.assertEqual(block["true_peak_dbtp"], -1.2)
        self.assertEqual(block["reasons"], {})

    def test_failure_is_null_with_reason(self):
        completed = subprocess.CompletedProcess(
            args=["ffmpeg"], returncode=1, stdout="", stderr="boom"
        )
        with patch.object(ambience.subprocess, "run", return_value=completed):
            block = ambience.measure_loudness("fixture.ogg", 5.0)
        self.assertIsNone(block["integrated_lufs"])
        self.assertIsNone(block["true_peak_dbtp"])
        self.assertIn("failed", block["reasons"]["integrated_lufs"])
        self.assertIn("failed", block["reasons"]["true_peak_dbtp"])

    def test_unparsable_output_is_null_with_reason(self):
        completed = subprocess.CompletedProcess(
            args=["ffmpeg"], returncode=0, stdout="", stderr="no json here"
        )
        with patch.object(ambience.subprocess, "run", return_value=completed):
            block = ambience.measure_loudness("fixture.ogg", 5.0)
        self.assertIsNone(block["integrated_lufs"])
        self.assertIn("did not return JSON", block["reasons"]["integrated_lufs"])


if __name__ == "__main__":
    unittest.main()
