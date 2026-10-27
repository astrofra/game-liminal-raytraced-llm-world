"""Regressions acoustiques bornees (DUO-043).

Cinq oracles physiques pour le contrat de scripts/analyze_ambience.py de la
branche principale. Aucun test n'a ete execute dans ce lot : Codex raccorde le
module et execute la suite.
"""

import json
import unittest

import numpy as np

from analyze_ambience import analyze_pcm, default_config, describe


def strict_json(payload):
    return json.dumps(payload, allow_nan=False)


class AmbienceRegressionsTest(unittest.TestCase):
    def setUp(self):
        self.config = default_config()

    def test_tone_keeps_centroid_and_mid_after_resampling(self):
        rate = 44100
        time = np.arange(rate, dtype=np.float64) / rate
        tone = (0.4 * np.sin(2.0 * np.pi * 1000.0 * time))[:, None]

        analysis = analyze_pcm(tone, rate, self.config)

        centroid = analysis["spectrum"]["centroid_hz"]
        self.assertIsNotNone(centroid)
        self.assertGreaterEqual(centroid, 950.0)
        self.assertLessEqual(centroid, 1050.0)

        bands = analysis["bands"]
        shares = bands["shares"]
        self.assertAlmostEqual(bands["share_sum"], 1.0, delta=1e-6)
        self.assertGreater(shares["mid"], 0.95)
        # Le masque de frequence doit survivre au reechelonnage : le 1 kHz
        # reste dans mid et ne fabrique ni grave ni aigu.
        self.assertLess(shares["low"], 0.01)
        self.assertLess(shares["high"], 0.01)
        strict_json(analysis)

    def test_noise_at_4k_sample_rate_drops_unavailable_bands(self):
        rate = 4000
        rng = np.random.default_rng(20261027)
        noise = (0.2 * rng.standard_normal(rate))[:, None]

        analysis = analyze_pcm(noise, rate, self.config)

        bands = analysis["bands"]
        shares = bands["shares"]
        self.assertIsNone(shares["high"])
        self.assertIsNone(shares["very_high"])
        self.assertIsNotNone(shares["low"])
        self.assertIsNotNone(shares["mid"])
        self.assertIn("coverage", bands)
        # Nyquist = 2000 Hz : le bin exact ne doit pas etre compte deux fois.
        self.assertAlmostEqual(bands["share_sum"], shares["low"] + shares["mid"], delta=1e-9)
        self.assertAlmostEqual(bands["share_sum"], 1.0, delta=1e-6)
        strict_json(analysis)

    def test_correlated_stereo_keeps_side_mid_ratio_at_low_level(self):
        rate = 44100
        count = int(round(rate * 0.25))
        time = np.arange(count, dtype=np.float64) / rate
        left = 0.0008 * np.sin(2.0 * np.pi * 1000.0 * time)
        stereo = np.stack([left, 0.5 * left], axis=1)

        analysis = analyze_pcm(stereo, rate, self.config)

        rms_dbfs = analysis["level"]["rms_dbfs"]
        self.assertIsNotNone(rms_dbfs)
        # ~ -64.9 dBFS attendu : signal faible mais non nul, pas du silence.
        self.assertGreater(rms_dbfs, -70.0)
        self.assertLess(rms_dbfs, -60.0)
        ratio = analysis["stereo"]["side_mid_ratio"]
        self.assertIsNotNone(ratio)
        self.assertAlmostEqual(ratio, 1.0 / 9.0, delta=1e-6)
        strict_json(analysis)

    def test_equal_rms_with_opposite_sign_is_not_a_clean_junction(self):
        rate = 48000
        count = int(round(rate * 0.25))
        half = count // 2
        signal = np.empty((count, 1), dtype=np.float64)
        signal[:half] = 0.25
        signal[half:] = -0.25

        analysis = analyze_pcm(signal, rate, self.config)

        channels = analysis["junction"]["channels"]
        self.assertEqual(len(channels), 1)
        channel = channels[0]
        self.assertAlmostEqual(channel["amplitude_delta"], 0.5, delta=1e-9)
        self.assertIsNotNone(channel["delta_db"])
        self.assertLess(abs(channel["delta_db"]), 0.1)

        text = describe(analysis, self.config)
        self.assertIsInstance(text, str)
        self.assertTrue(text.strip())
        self.assertNotIn("sans ecart notable", text.lower().replace("é", "e"))
        self.assertEqual(text, describe(analysis, self.config))
        strict_json(analysis)

    def test_flatness_is_gain_independent_and_describes_no_source(self):
        rate = 48000
        rng = np.random.default_rng(424242)
        base = rng.standard_normal(rate)
        loud = analyze_pcm((0.1 * base)[:, None], rate, self.config)
        quiet = analyze_pcm((0.001 * base)[:, None], rate, self.config)

        flat_loud = loud["spectrum"]["flatness"]
        flat_quiet = quiet["spectrum"]["flatness"]
        for value in (flat_loud, flat_quiet):
            self.assertIsNotNone(value)
            self.assertGreater(value, 0.35)
        self.assertAlmostEqual(flat_loud, flat_quiet, delta=0.01)

        for analysis in (loud, quiet):
            text = describe(analysis, self.config).lower()
            for word in ("insecte", "machine", "vent", "nuit"):
                self.assertNotIn(word, text)
            strict_json(analysis)


if __name__ == "__main__":
    unittest.main()
