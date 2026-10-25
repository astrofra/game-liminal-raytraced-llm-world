"""Unit tests for the local Eryx report aggregation and Markdown rendering."""

import copy
import pathlib
import sys
import tempfile
import unittest

SCRIPT_DIR = pathlib.Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from scene_local_report import summarize, write_report  # noqa: E402


def make_call(**overrides):
    data = {
        "load_ms": 120.0,
        "prompt_ms": 30.0,
        "generation_ms": 800.0,
        "eval_count": 100,
        "tokens_per_second": 12.5,
    }
    data.update(overrides)
    return data


def make_trial(**overrides):
    data = {
        "model": "gemma",
        "case_id": "case_a",
        "trial": 1,
        "accepted": True,
        "first_pass": True,
        "total_ms": 1000.0,
        "render_ms": 40.0,
        "calls": [make_call()],
    }
    data.update(overrides)
    return data


class SummarizeTests(unittest.TestCase):
    def test_mixed_successes_and_failures(self):
        records = [
            make_trial(model="gemma", total_ms=1000.0),
            make_trial(model="gemma", trial=2, accepted=False, first_pass=False,
                       total_ms=3000.0, render_ms=None, calls=[],
                       error="validation"),
            make_trial(model="gemma", case_id="case_b", trial=1, accepted=False,
                       first_pass=True, total_ms=None,
                       calls=[make_call(generation_ms=None)]),
            make_trial(model="ministral", accepted=True, first_pass=False,
                       total_ms=500.0),
        ]

        summary = summarize(records)

        self.assertEqual(list(summary), ["gemma", "ministral"])
        self.assertEqual(summary["gemma"], {
            "trials": 3,
            "accepted": 1,
            "first_pass": 1,
            "accepted_median_ms": 1000.0,
            "accepted_p95_ms": 1000.0,
            "all_median_ms": 2000.0,
            "all_p95_ms": 3000.0,
        })
        self.assertEqual(summary["ministral"], {
            "trials": 1,
            "accepted": 1,
            "first_pass": 0,
            "accepted_median_ms": 500.0,
            "accepted_p95_ms": 500.0,
            "all_median_ms": 500.0,
            "all_p95_ms": 500.0,
        })

    def test_null_missing_and_invalid_metrics_stay_unknown(self):
        records = [
            make_trial(total_ms=None),
            {key: value for key, value in make_trial().items() if key != "total_ms"},
            make_trial(accepted=False, first_pass=False, total_ms=float("nan")),
            make_trial(accepted=False, first_pass=False, total_ms=float("inf")),
            make_trial(total_ms=0.0),
            make_trial(total_ms=800.0),
        ]

        group = summarize(records)["gemma"]

        self.assertEqual(group["trials"], 6)
        self.assertEqual(group["accepted"], 4)
        self.assertEqual(group["first_pass"], 4)
        self.assertEqual(group["accepted_median_ms"], 800.0)
        self.assertEqual(group["accepted_p95_ms"], 800.0)
        self.assertEqual(group["all_median_ms"], 800.0)
        self.assertEqual(group["all_p95_ms"], 800.0)
        self.assertNotIn(0.0, [
            group["accepted_median_ms"],
            group["accepted_p95_ms"],
            group["all_median_ms"],
            group["all_p95_ms"],
        ])

    def test_nearest_rank_p95_on_small_samples(self):
        records = [make_trial(trial=index, total_ms=float(index))
                   for index in range(1, 21)]

        group = summarize(records)["gemma"]

        self.assertEqual(group["trials"], 20)
        self.assertEqual(group["accepted_median_ms"], 10.5)
        self.assertEqual(group["accepted_p95_ms"], 19.0)
        self.assertEqual(group["all_median_ms"], 10.5)
        self.assertEqual(group["all_p95_ms"], 19.0)

        single = summarize([make_trial(total_ms=42.0)])["gemma"]
        self.assertEqual(single["accepted_p95_ms"], 42.0)

    def test_summarize_does_not_mutate_records(self):
        records = [
            make_trial(),
            make_trial(model="ministral", accepted=False, total_ms=None,
                       calls=[make_call(load_ms=None)]),
        ]
        snapshot = copy.deepcopy(records)

        summarize(records)

        self.assertEqual(records, snapshot)


class WriteReportTests(unittest.TestCase):
    def test_report_escapes_cells_and_links_images_relatively(self):
        with tempfile.TemporaryDirectory() as temp:
            root = pathlib.Path(temp)
            output = root / "reports" / "report.md"
            records = [
                make_trial(case_id="case|pipe", error="ligne 1\nligne | 2",
                           image_path=root / "images" / "case_a.png"),
                make_trial(trial=2, accepted=False, first_pass=False,
                           total_ms=None, case_id="case_b",
                           error=None, image_path=None),
            ]

            result = write_report(records, output)

            self.assertIsNone(result)
            self.assertTrue(output.is_file())
            text = output.read_text(encoding="utf-8")
            self.assertIn("case\\|pipe", text)
            self.assertIn("ligne 1<br>ligne \\| 2", text)
            self.assertIn("[voir](../images/case_a.png)", text)
            self.assertIn("Rejeté", text)
            self.assertIn(
                "| gemma | 2 | 1 | 1 | 1000.0 | 1000.0 | 1000.0 | 1000.0 |",
                text,
            )
            self.assertIn(
                "| gemma | case_b | 2 | Rejeté | Non | — | 40.0 | 1 | 800.0 | — | — |",
                text,
            )
            self.assertIn("audit technique", text)
            self.assertIn("revue visuelle", text)
            self.assertIn("p95", text)

    def test_report_without_any_accepted_trial(self):
        records = [
            make_trial(trial=1, accepted=False, first_pass=False, total_ms=1500.0),
            make_trial(trial=2, accepted=False, first_pass=False, total_ms=2500.0),
        ]

        group = summarize(records)["gemma"]
        self.assertEqual(group["accepted"], 0)
        self.assertEqual(group["first_pass"], 0)
        self.assertIsNone(group["accepted_median_ms"])
        self.assertIsNone(group["accepted_p95_ms"])
        self.assertEqual(group["all_median_ms"], 2000.0)
        self.assertEqual(group["all_p95_ms"], 2500.0)

        with tempfile.TemporaryDirectory() as temp:
            output = pathlib.Path(temp) / "report.md"
            write_report(records, output)
            text = output.read_text(encoding="utf-8")
            self.assertIn(
                "| gemma | 2 | 0 | 0 | — | — | 2000.0 | 2500.0 |",
                text,
            )
            self.assertEqual(text.count("Rejeté"), 2)

    def test_empty_records_are_reported_without_fabricated_values(self):
        self.assertEqual(summarize([]), {})
        with tempfile.TemporaryDirectory() as temp:
            output = pathlib.Path(temp) / "report.md"
            write_report([], output)
            text = output.read_text(encoding="utf-8")
            self.assertIn("Aucun essai fourni.", text)
            self.assertTrue(text.startswith("# Rapport local"))


if __name__ == "__main__":
    unittest.main()
