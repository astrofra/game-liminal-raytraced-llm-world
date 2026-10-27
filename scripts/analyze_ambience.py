#!/usr/bin/env python3
"""Offline acoustic analysis and catalogue for the four ambience loops.

AUDIO-01/AUDIO-02 pilot.  The game runtime never imports this module and the
NumPy/SciPy dependencies stay development-only.  The source OGG files are only
read; they are never rewritten, normalised or replaced.
"""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import math
import os
import platform
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import scipy
from scipy import signal

SCHEMA_VERSION = 1
SCRIPT_VERSION = "1.1.0"
RULE_VERSION = "acoustic-description-2"

TRACK_IDS = ("blade", "bug", "night", "static")
TRACK_PATH_TEMPLATE = "assets/sfx/{id}-loop.ogg"

BAND_NAMES = ("low", "mid", "high", "very_high")
BAND_LABELS = {
    "low": "grave",
    "mid": "médium",
    "high": "aiguë",
    "very_high": "très aiguë",
}

TRIM_DB_MIN = -24.0
TRIM_DB_MAX = 6.0

FFMPEG_BINARY = "ffmpeg"
FFPROBE_BINARY = "ffprobe"

_ROOT_KEYS = frozenset(
    {
        "schema_version",
        "script_version",
        "tools",
        "config",
        "config_sha256",
        "catalog_version",
        "tracks",
    }
)
_TRACK_KEYS = frozenset(
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
    }
)
_PLAYBACK_KEYS = frozenset({"trim_db", "max_gain", "trim_validated"})
_ANALYSIS_KEYS = frozenset(
    {"silence", "level", "spectrum", "bands", "dynamics", "stereo", "junction", "loudness"}
)
_ANNOTATION_ENTRY_KEYS = frozenset({"text", "provenance", "trim_db", "trim_validated"})


def default_config() -> dict:
    """Return the versioned analysis policy used when no config file is given."""

    return {
        "config_version": 1,
        "silence": {
            "rms_dbfs": -70.0,
            "note": (
                "A source is silent when its full-signal native RMS is at or below this "
                "threshold. Silence is detected before any spectral or temporal "
                "qualification; every derived measurement is then null with a reason."
            ),
        },
        "spectrum": {
            "sample_rate_hz": 48000,
            "window": "hann_periodic",
            "window_size": 2048,
            "hop_size": 512,
            "block_frames": 64,
            "rolloff_fraction": 0.85,
            "epsilon": 0.000001,
            "rms_floor_dbfs": -120.0,
            "note": (
                "Native float32 samples are decoded once. Levels use the native rate; "
                "the spectrum uses resample_poly at sample_rate_hz. Hann is periodic "
                "(0.5 - 0.5*cos(2*pi*n/N)). The final frame is zero padded and FFT work "
                "is bounded to block_frames frames. Spectral statistics only use bins "
                "up to the lower of the source Nyquist and the analysed Nyquist. "
                "Channel powers are averaged before magnitude is derived, so anti-phase "
                "channels cannot cancel. Frames at or below the silence threshold are excluded "
                "from spectral statistics. Flatness uses a relative epsilon times mean bin power. "
                "Zero-energy frames use rms_floor_dbfs in the "
                "RMS percentiles. Config numbers are consumed at six-decimal precision."
            ),
        },
        "bands": {
            "edges_hz": [0.0, 250.0, 2000.0, 8000.0],
            "note": (
                "Band shares use summed power over half-open ranges low [0,250), "
                "mid [250,2000), high [2000,8000), very_high [8000, analysed Nyquist]. "
                "The denominator is restricted to the source Nyquist. A band is null "
                "when it has no bins below that limit; coverage records the fraction of "
                "the nominal band width that was actually analysed. The exact source Nyquist "
                "bin belongs to the last available band only."
            ),
        },
        "flux": {
            "peak_threshold": 0.02,
            "note": (
                "Per-frame flux is the sum of positive changes of the L1-normalised "
                "magnitude spectrum. Flux of the first frame is zero. A peak is counted "
                "on an interior frame when flux is at least peak_threshold, strictly "
                "greater than the previous frame and at least the next frame. Rate is "
                "peaks divided by analysed duration in seconds."
            ),
        },
        "description": {
            "dominant_band_share": 0.5,
            "stable_spread_db": 3.0,
            "moderate_spread_db": 9.0,
            "diffuse_flatness": 0.4,
            "tonal_flatness": 0.1,
            "busy_peak_rate_hz": 2.0,
            "quiet_peak_rate_hz": 0.2,
            "mono_correlation": 0.5,
            "opposition_correlation": -0.1,
            "junction_delta_db": 1.0,
            "marked_crest_db": 12.0,
            "note": (
                "Absolute thresholds for the deterministic acoustic sheet. No track is "
                "ranked against another and no source identifier contributes vocabulary. "
                "When a measurement is unavailable the corresponding qualifier is "
                "omitted or stated as not measurable."
            ),
        },
        "playback": {
            "max_gain": 1.0,
            "trim_db_min": -24.0,
            "trim_db_max": 6.0,
            "note": (
                "Playback metadata copied to every track. trim_db is authored in the "
                "annotations file and must be finite and within [-24, 6]; a non-zero "
                "trim also requires trim_validated true. max_gain caps any future "
                "target gain; this is not automatic loudness compensation."
            ),
        },
        "loudness": {
            "enabled": True,
            "timeout_seconds": 120.0,
            "note": (
                "Optional integrated loudness (LUFS) and true peak (dBTP) measured by "
                "ffmpeg loudnorm input_i/input_tp. Failures and silence leave both "
                "values null with a reason; they never abort extraction."
            ),
        },
    }


# ---------------------------------------------------------------------------
# JSON, numbers and validation


def _reject_constant(token: str):
    raise ValueError(f"non-finite JSON number {token!r} is not allowed")


def _no_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _loads_strict(text: str):
    return json.loads(text, parse_constant=_reject_constant, object_pairs_hook=_no_duplicate_keys)


def _load_json_file(path) -> object:
    path = Path(path)
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        raise FileNotFoundError(f"cannot read JSON file {path}: {exc}") from exc
    try:
        return _loads_strict(text)
    except ValueError as exc:
        raise ValueError(f"invalid JSON in {path}: {exc}") from exc


def _round_numbers(value, digits: int = 6):
    """Recursively round JSON numbers, rejecting non-finite values."""

    if value is None or isinstance(value, (str, bool)):
        return value
    if isinstance(value, dict):
        return {key: _round_numbers(item, digits) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_round_numbers(item, digits) for item in value]
    if isinstance(value, (int, np.integer)):
        return int(value)
    if isinstance(value, (float, np.floating)):
        number = float(value)
        if not math.isfinite(number):
            raise ValueError("non-finite number is not allowed in the catalogue")
        rounded = round(number, digits)
        return 0.0 if rounded == 0 else rounded
    raise TypeError(f"unsupported catalogue value type: {type(value).__name__}")


def _canonical_json(value) -> str:
    return json.dumps(
        value,
        sort_keys=True,
        ensure_ascii=False,
        separators=(",", ":"),
        allow_nan=False,
    )


def _content_hash(value) -> str:
    return hashlib.sha256(_canonical_json(value).encode("utf-8")).hexdigest()


def _number(value, where: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError(f"{where} must be a number")
    number = float(value)
    if not math.isfinite(number):
        raise ValueError(f"{where} must be finite")
    return number


def _integer(value, where: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError(f"{where} must be an integer")
    return int(value)


def _mapping(value, where: str, required, optional=()) -> dict:
    if not isinstance(value, dict):
        raise ValueError(f"{where} must be an object")
    required = set(required)
    allowed = required | set(optional)
    missing = sorted(required - set(value))
    extra = sorted(set(value) - allowed)
    if missing:
        raise ValueError(f"{where} is missing keys: {', '.join(missing)}")
    if extra:
        raise ValueError(f"{where} has unknown keys: {', '.join(extra)}")
    return value


def _note(value, where: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{where} must be a non-empty string")
    return value


def _boolean(value, where: str) -> bool:
    if not isinstance(value, bool):
        raise ValueError(f"{where} must be a boolean")
    return value


def validate_config(config) -> dict:
    """Validate the complete, explicit analysis policy. Unknown keys are errors."""

    _mapping(
        config,
        "config",
        {
            "config_version",
            "silence",
            "spectrum",
            "bands",
            "flux",
            "description",
            "playback",
            "loudness",
        },
    )
    if _integer(config["config_version"], "config.config_version") != 1:
        raise ValueError("config.config_version must be 1")

    silence = _mapping(config["silence"], "config.silence", {"rms_dbfs", "note"})
    silence_threshold = _number(silence["rms_dbfs"], "config.silence.rms_dbfs")
    if silence_threshold >= 0.0:
        raise ValueError("config.silence.rms_dbfs must be negative")
    _note(silence["note"], "config.silence.note")

    spectrum = _mapping(
        config["spectrum"],
        "config.spectrum",
        {
            "sample_rate_hz",
            "window",
            "window_size",
            "hop_size",
            "block_frames",
            "rolloff_fraction",
            "epsilon",
            "rms_floor_dbfs",
            "note",
        },
    )
    sample_rate = _integer(spectrum["sample_rate_hz"], "config.spectrum.sample_rate_hz")
    if sample_rate != 48000:
        raise ValueError("config.spectrum.sample_rate_hz must be 48000")
    if spectrum["window"] != "hann_periodic":
        raise ValueError("config.spectrum.window must be 'hann_periodic'")
    window_size = _integer(spectrum["window_size"], "config.spectrum.window_size")
    if window_size < 256 or window_size & (window_size - 1):
        raise ValueError("config.spectrum.window_size must be a power of two >= 256")
    hop_size = _integer(spectrum["hop_size"], "config.spectrum.hop_size")
    if hop_size <= 0 or hop_size > window_size:
        raise ValueError("config.spectrum.hop_size must be in [1, window_size]")
    block_frames = _integer(spectrum["block_frames"], "config.spectrum.block_frames")
    if block_frames <= 0 or block_frames > 1024:
        raise ValueError("config.spectrum.block_frames must be in [1, 1024]")
    rolloff = _number(spectrum["rolloff_fraction"], "config.spectrum.rolloff_fraction")
    if not 0.0 < rolloff < 1.0:
        raise ValueError("config.spectrum.rolloff_fraction must be in (0, 1)")
    epsilon = _number(spectrum["epsilon"], "config.spectrum.epsilon")
    if epsilon <= 0.0:
        raise ValueError("config.spectrum.epsilon must be positive")
    floor_db = _number(spectrum["rms_floor_dbfs"], "config.spectrum.rms_floor_dbfs")
    if floor_db >= silence_threshold:
        raise ValueError("config.spectrum.rms_floor_dbfs must be below the silence threshold")
    _note(spectrum["note"], "config.spectrum.note")

    bands = _mapping(config["bands"], "config.bands", {"edges_hz", "note"})
    edges = bands["edges_hz"]
    if not isinstance(edges, list) or len(edges) != 4:
        raise ValueError("config.bands.edges_hz must be a list of four numbers")
    values = [_number(item, "config.bands.edges_hz") for item in edges]
    if values[0] != 0.0:
        raise ValueError("config.bands.edges_hz must start at 0")
    if any(later <= earlier for earlier, later in zip(values, values[1:])):
        raise ValueError("config.bands.edges_hz must be strictly increasing")
    if values[-1] >= sample_rate / 2.0:
        raise ValueError("config.bands.edges_hz must stay below the analysed Nyquist")
    _note(bands["note"], "config.bands.note")

    flux = _mapping(config["flux"], "config.flux", {"peak_threshold", "note"})
    if _number(flux["peak_threshold"], "config.flux.peak_threshold") < 0.0:
        raise ValueError("config.flux.peak_threshold must be non-negative")
    _note(flux["note"], "config.flux.note")

    description = _mapping(
        config["description"],
        "config.description",
        {
            "dominant_band_share",
            "stable_spread_db",
            "moderate_spread_db",
            "diffuse_flatness",
            "tonal_flatness",
            "busy_peak_rate_hz",
            "quiet_peak_rate_hz",
            "mono_correlation",
            "opposition_correlation",
            "junction_delta_db",
            "marked_crest_db",
            "note",
        },
    )
    dominant_share = _number(
        description["dominant_band_share"], "config.description.dominant_band_share"
    )
    if not 0.0 <= dominant_share <= 1.0:
        raise ValueError("config.description.dominant_band_share must be in [0, 1]")
    stable_spread = _number(description["stable_spread_db"], "config.description.stable_spread_db")
    moderate_spread = _number(
        description["moderate_spread_db"], "config.description.moderate_spread_db"
    )
    if stable_spread < 0.0 or moderate_spread <= stable_spread:
        raise ValueError("config.description spread thresholds must satisfy 0 <= stable < moderate")
    diffuse = _number(description["diffuse_flatness"], "config.description.diffuse_flatness")
    tonal = _number(description["tonal_flatness"], "config.description.tonal_flatness")
    if not 0.0 <= tonal < diffuse <= 1.0:
        raise ValueError("config.description flatness thresholds must satisfy 0 <= tonal < diffuse <= 1")
    busy_rate = _number(description["busy_peak_rate_hz"], "config.description.busy_peak_rate_hz")
    quiet_rate = _number(description["quiet_peak_rate_hz"], "config.description.quiet_peak_rate_hz")
    if busy_rate <= quiet_rate or quiet_rate < 0.0:
        raise ValueError("config.description peak-rate thresholds must satisfy 0 <= quiet < busy")
    mono_correlation = _number(
        description["mono_correlation"], "config.description.mono_correlation"
    )
    opposition_correlation = _number(
        description["opposition_correlation"], "config.description.opposition_correlation"
    )
    if not -1.0 <= opposition_correlation < mono_correlation <= 1.0:
        raise ValueError("config.description correlation thresholds must be ordered in [-1, 1]")
    if _number(description["junction_delta_db"], "config.description.junction_delta_db") < 0.0:
        raise ValueError("config.description.junction_delta_db must be non-negative")
    if _number(description["marked_crest_db"], "config.description.marked_crest_db") < 0.0:
        raise ValueError("config.description.marked_crest_db must be non-negative")
    _note(description["note"], "config.description.note")

    playback = _mapping(
        config["playback"],
        "config.playback",
        {"max_gain", "trim_db_min", "trim_db_max", "note"},
    )
    max_gain = _number(playback["max_gain"], "config.playback.max_gain")
    if not 0.0 < max_gain <= 1.0:
        raise ValueError("config.playback.max_gain must be in (0, 1]")
    trim_min = _number(playback["trim_db_min"], "config.playback.trim_db_min")
    trim_max = _number(playback["trim_db_max"], "config.playback.trim_db_max")
    if trim_min > TRIM_DB_MIN or trim_max < TRIM_DB_MAX or trim_min >= trim_max:
        raise ValueError("config.playback trim range must cover [-24, 6]")
    _note(playback["note"], "config.playback.note")

    loudness = _mapping(
        config["loudness"], "config.loudness", {"enabled", "timeout_seconds", "note"}
    )
    _boolean(loudness["enabled"], "config.loudness.enabled")
    if _number(loudness["timeout_seconds"], "config.loudness.timeout_seconds") <= 0.0:
        raise ValueError("config.loudness.timeout_seconds must be positive")
    _note(loudness["note"], "config.loudness.note")
    return config


def load_config(path) -> dict:
    config = _round_numbers(_load_json_file(path))
    return validate_config(config)


# ---------------------------------------------------------------------------
# Measurement helpers


def _hann_periodic(size: int) -> np.ndarray:
    return 0.5 - 0.5 * np.cos(2.0 * math.pi * np.arange(size) / float(size))


def _db_power(power: float):
    if power <= 0.0 or not math.isfinite(power):
        return None
    return 10.0 * math.log10(power)


def _unavailable_loudness(reason: str) -> dict:
    return {
        "integrated_lufs": None,
        "true_peak_dbtp": None,
        "method": "ffmpeg loudnorm input_i/input_tp",
        "units": {"integrated_lufs": "LUFS", "true_peak_dbtp": "dBTP"},
        "reasons": {"integrated_lufs": reason, "true_peak_dbtp": reason},
    }


def _spectrum_and_dynamics(samples: np.ndarray, sample_rate: int, config: dict):
    cfg = config["spectrum"]
    target_rate = int(cfg["sample_rate_hz"])
    window_size = int(cfg["window_size"])
    hop_size = int(cfg["hop_size"])
    block_frames = int(cfg["block_frames"])
    epsilon = float(cfg["epsilon"])
    floor_db = float(cfg["rms_floor_dbfs"])
    rolloff_fraction = float(cfg["rolloff_fraction"])

    if sample_rate == target_rate:
        analysis_signal = samples
    else:
        divisor = math.gcd(int(sample_rate), target_rate)
        analysis_signal = signal.resample_poly(
            samples, target_rate // divisor, int(sample_rate) // divisor, axis=0
        )

    frame_total = analysis_signal.shape[0]
    channels = analysis_signal.shape[1]
    frame_count = (frame_total + hop_size - 1) // hop_size

    frequencies_all = np.fft.rfftfreq(window_size, d=1.0 / target_rate)
    source_nyquist = sample_rate / 2.0
    analysed_limit = min(source_nyquist, target_rate / 2.0)
    bin_mask = frequencies_all <= analysed_limit + 1e-9
    frequencies = frequencies_all[bin_mask]

    edges = [float(item) for item in config["bands"]["edges_hz"]]
    nominal_bands = (
        ("low", edges[0], edges[1]),
        ("mid", edges[1], edges[2]),
        ("high", edges[2], edges[3]),
        ("very_high", edges[3], target_rate / 2.0),
    )
    band_masks = {}
    band_coverage = {}
    for name, low, high in nominal_bands:
        top = min(high, analysed_limit)
        width = high - low
        coverage = max(0.0, top - low) / width if width > 0.0 else 0.0
        band_coverage[name] = coverage
        if coverage <= 0.0:
            continue
        if high < analysed_limit:
            mask = (frequencies_all >= low) & (frequencies_all < high)
        else:
            mask = (frequencies_all >= low) & (frequencies_all <= analysed_limit)
        mask &= bin_mask
        if np.any(mask):
            band_masks[name] = mask[bin_mask]

    window = _hann_periodic(window_size)
    offsets = np.arange(window_size)
    mag_sum = np.zeros(frame_count, dtype=np.float64)
    centroid_numerator = np.zeros(frame_count, dtype=np.float64)
    power_total = np.zeros(frame_count, dtype=np.float64)
    flatness = np.zeros(frame_count, dtype=np.float64)
    rolloff = np.zeros(frame_count, dtype=np.float64)
    flux = np.zeros(frame_count, dtype=np.float64)
    frame_power = np.zeros(frame_count, dtype=np.float64)
    band_power = {name: np.zeros(frame_count, dtype=np.float64) for name in BAND_NAMES}
    previous_normalised = np.zeros(frequencies.size, dtype=np.float64)

    for block_start in range(0, frame_count, block_frames):
        block_end = min(frame_count, block_start + block_frames)
        starts = np.arange(block_start, block_end) * hop_size
        local = starts.size
        indices = starts[:, None] + offsets[None, :]
        valid = indices < frame_total
        power_sum = np.zeros((local, frequencies.size), dtype=np.float64)
        sum_squares = np.zeros(local, dtype=np.float64)
        for channel in range(channels):
            raw = np.zeros((local, window_size), dtype=np.float64)
            raw[valid] = analysis_signal[indices[valid], channel]
            sum_squares += np.einsum("ij,ij->i", raw, raw)
            spectrum_block = np.fft.rfft(raw * window, axis=1)
            power = spectrum_block.real * spectrum_block.real + spectrum_block.imag * spectrum_block.imag
            power_sum += power[:, bin_mask]
        counts = np.minimum(window_size, frame_total - starts).astype(np.float64)
        frame_power[block_start:block_end] = sum_squares / (channels * counts)
        power_sum /= channels
        audible = frame_power[block_start:block_end] > 10.0 ** (config["silence"]["rms_dbfs"] / 10.0)
        power_sum[~audible] = 0.0
        total = power_sum.sum(axis=1)
        power_total[block_start:block_end] = total
        magnitude = np.sqrt(power_sum)
        magnitude_total = magnitude.sum(axis=1)
        mag_sum[block_start:block_end] = magnitude_total
        centroid_numerator[block_start:block_end] = magnitude @ frequencies
        power_mean = total / frequencies.size
        safe_mean = np.maximum(power_mean, np.finfo(float).tiny)
        flatness[block_start:block_end] = np.exp(
            np.mean(np.log(np.maximum(power_sum / safe_mean[:, None], epsilon)), axis=1)
        )
        cumulative = np.cumsum(power_sum, axis=1)
        target_power = rolloff_fraction * total
        rolloff_index = np.argmax(cumulative >= target_power[:, None], axis=1)
        rolloff[block_start:block_end] = frequencies[rolloff_index]
        normalised = np.zeros_like(magnitude)
        np.divide(
            magnitude,
            magnitude_total[:, None],
            out=normalised,
            where=magnitude_total[:, None] > epsilon,
        )
        previous = np.empty_like(normalised)
        previous[0] = previous_normalised
        previous[1:] = normalised[:-1]
        flux[block_start:block_end] = np.maximum(normalised - previous, 0.0).sum(axis=1)
        previous_normalised = normalised[-1].copy()
        for name, mask in band_masks.items():
            band_power[name][block_start:block_end] = power_sum[:, mask].sum(axis=1)

    if frame_count:
        flux[0] = 0.0

    spectrum_reasons = {}
    magnitude_denominator = float(mag_sum.sum())
    if magnitude_denominator > epsilon:
        centroid = float(centroid_numerator.sum() / magnitude_denominator)
    else:
        centroid = None
        spectrum_reasons["centroid_hz"] = "no spectral magnitude above the analysis floor"

    active_power = power_total > epsilon
    active_frames = int(np.count_nonzero(active_power))
    if active_frames:
        rolloff_value = float(rolloff[active_power].mean())
        flatness_value = float(flatness[active_power].mean())
    else:
        rolloff_value = None
        flatness_value = None
        spectrum_reasons["rolloff85_hz"] = "no spectral power above the analysis floor"
        spectrum_reasons["flatness"] = "no spectral power above the analysis floor"

    if frame_count > 1:
        flux_mean = float(flux[1:].mean())
    elif frame_count == 1:
        flux_mean = 0.0
    else:
        flux_mean = None
        spectrum_reasons["flux_mean_positive"] = "no spectral frame was analysed"

    analysed_seconds = frame_total / float(target_rate)
    if frame_count >= 3 and analysed_seconds > 0.0:
        threshold = float(config["flux"]["peak_threshold"])
        inner = flux[1:-1]
        peaks = (
            (inner >= threshold)
            & (inner > flux[:-2])
            & (inner >= flux[2:])
        )
        peak_count = int(np.count_nonzero(peaks))
        peak_rate = peak_count / analysed_seconds
    elif frame_count >= 1 and analysed_seconds > 0.0:
        peak_rate = 0.0
    else:
        peak_rate = None
        spectrum_reasons["flux_peak_rate_hz"] = "no spectral frame was analysed"

    spectrum = {
        "source_nyquist_hz": source_nyquist,
        "analysed_sample_rate_hz": target_rate,
        "analysed_limit_hz": analysed_limit,
        "window": "hann_periodic",
        "window_size": window_size,
        "hop_size": hop_size,
        "frame_count": frame_count,
        "fft_bins_analysed": int(frequencies.size),
        "centroid_hz": centroid,
        "rolloff85_hz": rolloff_value,
        "flatness": flatness_value,
        "flux_mean_positive": flux_mean,
        "flux_peak_rate_hz": peak_rate,
        "flux_peak_threshold": float(config["flux"]["peak_threshold"]),
        "rolloff_fraction": rolloff_fraction,
        "units": {
            "source_nyquist_hz": "Hz",
            "analysed_sample_rate_hz": "Hz",
            "analysed_limit_hz": "Hz",
            "centroid_hz": "Hz",
            "rolloff85_hz": "Hz",
            "flatness": "ratio in [0, 1]",
            "flux_mean_positive": "mean L1-normalised positive magnitude change per frame",
            "flux_peak_rate_hz": "detected peaks per second",
            "flux_peak_threshold": "L1-normalised magnitude change",
            "rolloff_fraction": "cumulative power fraction",
        },
        "method": (
            "Native PCM is resampled once with scipy.signal.resample_poly; Hann periodic "
            "window and rfft in bounded blocks; per-channel powers summed before "
            "magnitude; statistics restricted to the lower of source and analysed Nyquist."
        ),
        "reasons": spectrum_reasons,
    }

    total_power = float(power_total.sum())
    shares = {}
    coverage = {}
    band_reasons = {}
    for name, low, high in nominal_bands:
        coverage[name] = band_coverage[name]
        if name not in band_masks or total_power <= epsilon:
            shares[name] = None
            if name not in band_masks:
                band_reasons[name] = (
                    f"band lies above the analysed limit of {analysed_limit:.3f} Hz"
                )
            else:
                band_reasons[name] = "no spectral power above the analysis floor"
        else:
            shares[name] = float(band_power[name].sum() / total_power)
    available_shares = [value for value in shares.values() if value is not None]
    if available_shares:
        share_sum = float(sum(available_shares))
    else:
        share_sum = None
        band_reasons["share_sum"] = "no band is available for this source"

    bands = {
        "edges_hz": edges,
        "source_nyquist_hz": source_nyquist,
        "analysed_limit_hz": analysed_limit,
        "shares": shares,
        "coverage": coverage,
        "share_sum": share_sum,
        "units": {
            "edges_hz": "Hz",
            "source_nyquist_hz": "Hz",
            "analysed_limit_hz": "Hz",
            "shares": "fraction of summed power below the analysed limit",
            "coverage": "fraction of nominal band width actually analysed",
            "share_sum": "sum of available shares, expected 1",
        },
        "method": (
            "Summed FFT power over all frames. Each band is restricted to bins below "
            "the source Nyquist; unavailable bands stay null with a reason."
        ),
        "reasons": band_reasons,
    }

    floor_power = 10.0 ** (floor_db / 10.0)
    if frame_count:
        frame_db = 10.0 * np.log10(np.maximum(frame_power, floor_power))
        percentiles = np.percentile(frame_db, [10.0, 50.0, 90.0])
        p10, p50, p90 = (float(item) for item in percentiles)
        spread = p90 - p10
        dynamics_reasons = {}
    else:
        p10 = p50 = p90 = spread = None
        dynamics_reasons = {
            "p10_dbfs": "no spectral frame was analysed",
            "p50_dbfs": "no spectral frame was analysed",
            "p90_dbfs": "no spectral frame was analysed",
            "spread_db": "no spectral frame was analysed",
        }

    dynamics = {
        "rms_percentiles_dbfs": {"p10": p10, "p50": p50, "p90": p90},
        "spread_db": spread,
        "frame_rms_floor_dbfs": floor_db,
        "units": {
            "rms_percentiles_dbfs": "dBFS",
            "spread_db": "dB (p90 - p10)",
            "frame_rms_floor_dbfs": "dBFS",
        },
        "method": (
            "Per-frame RMS of the 48 kHz analysis signal, channels pooled by mean "
            "square, including the zero-padded final frame divided by its valid sample "
            "count. Zero-energy frames use the documented floor before percentiles."
        ),
        "reasons": dynamics_reasons,
    }
    return spectrum, bands, dynamics


def _stereo_analysis(samples: np.ndarray, config: dict) -> dict:
    channels = int(samples.shape[1])
    result = {
        "channels": channels,
        "correlation": None,
        "side_mid_ratio": None,
        "units": {
            "correlation": "Pearson r in [-1, 1]",
            "side_mid_ratio": "side energy divided by mid energy",
        },
        "method": (
            "Pearson correlation and side/mid energy measured on native samples. "
            "Side=(L-R)/2 and mid=(L+R)/2; null when the denominator is zero."
        ),
        "reasons": {},
    }
    if channels == 1:
        reason = "mono source"
        result["reasons"] = {"correlation": reason, "side_mid_ratio": reason}
        return result
    if channels != 2:
        reason = "stereo metrics require one or two channels"
        result["reasons"] = {"correlation": reason, "side_mid_ratio": reason}
        return result

    left = samples[:, 0]
    right = samples[:, 1]
    count = left.size
    sum_left = float(left.sum())
    sum_right = float(right.sum())
    sum_left_sq = float(np.dot(left, left))
    sum_right_sq = float(np.dot(right, right))
    sum_product = float(np.dot(left, right))
    denominator_sq = (count * sum_left_sq - sum_left * sum_left) * (
        count * sum_right_sq - sum_right * sum_right
    )
    if count < 2 or denominator_sq <= 0.0:
        result["reasons"]["correlation"] = "zero variance in one or both channels"
    else:
        correlation = (count * sum_product - sum_left * sum_right) / math.sqrt(denominator_sq)
        result["correlation"] = max(-1.0, min(1.0, float(correlation)))

    mid = (left + right) * 0.5
    side = (left - right) * 0.5
    mid_power = float(np.mean(mid * mid))
    side_power = float(np.mean(side * side))
    if mid_power <= np.finfo(float).tiny:
        result["reasons"]["side_mid_ratio"] = "mid energy is zero"
    else:
        result["side_mid_ratio"] = side_power / mid_power
    return result


def _junction_analysis(samples: np.ndarray, sample_rate: int) -> dict:
    frame_total = int(samples.shape[0])
    window = min(frame_total, max(1, int(round(sample_rate * 0.05))))
    start = samples[:window]
    end = samples[-window:]
    channels_out = []
    for channel in range(int(samples.shape[1])):
        start_power = float(np.mean(start[:, channel] * start[:, channel]))
        end_power = float(np.mean(end[:, channel] * end[:, channel]))
        start_db = _db_power(start_power)
        end_db = _db_power(end_power)
        reasons = {}
        if start_db is None:
            reasons["start_rms_dbfs"] = "zero energy in the first junction window"
        if end_db is None:
            reasons["end_rms_dbfs"] = "zero energy in the last junction window"
        if start_db is None or end_db is None:
            delta = None
            reasons["delta_db"] = "a junction window has zero energy"
        else:
            delta = end_db - start_db
        channels_out.append(
            {
                "amplitude_delta": float(samples[0, channel] - samples[-1, channel]),
                "start_rms_dbfs": start_db,
                "end_rms_dbfs": end_db,
                "delta_db": delta,
                "reasons": reasons,
            }
        )

    pooled_start = _db_power(float(np.mean(start * start)))
    pooled_end = _db_power(float(np.mean(end * end)))
    pooled_reasons = {}
    if pooled_start is None:
        pooled_reasons["start_rms_dbfs"] = "zero energy in the first junction window"
    if pooled_end is None:
        pooled_reasons["end_rms_dbfs"] = "zero energy in the last junction window"
    if pooled_start is None or pooled_end is None:
        pooled_delta = None
        pooled_reasons["delta_db"] = "a junction window has zero energy"
    else:
        pooled_delta = pooled_end - pooled_start

    return {
        "window_ms": 50.0,
        "channels": channels_out,
        "start_rms_dbfs": pooled_start,
        "end_rms_dbfs": pooled_end,
        "delta_db": pooled_delta,
        "units": {
            "amplitude_delta": "linear full-scale amplitude, first sample minus last, per channel",
            "window_ms": "ms",
            "start_rms_dbfs": "dBFS",
            "end_rms_dbfs": "dBFS",
            "delta_db": "dB (end - start)",
        },
        "method": (
            "Unweighted RMS of the first and last 50 ms at the native rate, per channel "
            "and pooled over channels. The window is clamped to the signal length for "
            "sources shorter than 50 ms."
        ),
        "reasons": pooled_reasons,
    }


def _silent_analysis(config: dict, reason: str, sample_rate: int, channels: int) -> dict:
    target_rate = int(config["spectrum"]["sample_rate_hz"])
    edges = [float(item) for item in config["bands"]["edges_hz"]]
    shares = {name: None for name in BAND_NAMES}
    coverage = {name: 0.0 for name in BAND_NAMES}
    band_reasons = {name: reason for name in BAND_NAMES}
    band_reasons["share_sum"] = reason
    return {
        "silence": {
            "detected": True,
            "threshold_rms_dbfs": float(config["silence"]["rms_dbfs"]),
            "method": "full-signal native RMS, channels pooled; detected before qualifications",
        },
        "level": {
            "rms_dbfs": None,
            "peak_dbfs": None,
            "crest_db": None,
            "units": {"rms_dbfs": "dBFS", "peak_dbfs": "dBFS", "crest_db": "dB"},
            "method": "native sample domain, unweighted, channels pooled",
            "reasons": {"rms_dbfs": reason, "peak_dbfs": reason, "crest_db": reason},
        },
        "spectrum": {
            "source_nyquist_hz": sample_rate / 2.0,
            "analysed_sample_rate_hz": target_rate,
            "analysed_limit_hz": min(sample_rate / 2.0, target_rate / 2.0),
            "window": "hann_periodic",
            "window_size": int(config["spectrum"]["window_size"]),
            "hop_size": int(config["spectrum"]["hop_size"]),
            "frame_count": 0,
            "fft_bins_analysed": 0,
            "centroid_hz": None,
            "rolloff85_hz": None,
            "flatness": None,
            "flux_mean_positive": None,
            "flux_peak_rate_hz": None,
            "flux_peak_threshold": float(config["flux"]["peak_threshold"]),
            "rolloff_fraction": float(config["spectrum"]["rolloff_fraction"]),
            "units": {
                "source_nyquist_hz": "Hz",
                "analysed_sample_rate_hz": "Hz",
                "analysed_limit_hz": "Hz",
                "centroid_hz": "Hz",
                "rolloff85_hz": "Hz",
                "flatness": "ratio in [0, 1]",
                "flux_mean_positive": "mean L1-normalised positive magnitude change per frame",
                "flux_peak_rate_hz": "detected peaks per second",
                "flux_peak_threshold": "L1-normalised magnitude change",
                "rolloff_fraction": "cumulative power fraction",
            },
            "method": "spectral frames are not computed when silence is detected first",
            "reasons": {
                "centroid_hz": reason,
                "rolloff85_hz": reason,
                "flatness": reason,
                "flux_mean_positive": reason,
                "flux_peak_rate_hz": reason,
            },
        },
        "bands": {
            "edges_hz": edges,
            "source_nyquist_hz": sample_rate / 2.0,
            "analysed_limit_hz": min(sample_rate / 2.0, target_rate / 2.0),
            "shares": shares,
            "coverage": coverage,
            "share_sum": None,
            "units": {
                "edges_hz": "Hz",
                "source_nyquist_hz": "Hz",
                "analysed_limit_hz": "Hz",
                "shares": "fraction of summed power below the analysed limit",
                "coverage": "fraction of nominal band width actually analysed",
                "share_sum": "sum of available shares, expected 1",
            },
            "method": "band powers are not computed when silence is detected first",
            "reasons": band_reasons,
        },
        "dynamics": {
            "rms_percentiles_dbfs": {"p10": None, "p50": None, "p90": None},
            "spread_db": None,
            "frame_rms_floor_dbfs": float(config["spectrum"]["rms_floor_dbfs"]),
            "units": {
                "rms_percentiles_dbfs": "dBFS",
                "spread_db": "dB (p90 - p10)",
                "frame_rms_floor_dbfs": "dBFS",
            },
            "method": "temporal statistics are not computed when silence is detected first",
            "reasons": {
                "p10_dbfs": reason,
                "p50_dbfs": reason,
                "p90_dbfs": reason,
                "spread_db": reason,
            },
        },
        "stereo": {
            "channels": channels,
            "correlation": None,
            "side_mid_ratio": None,
            "units": {
                "correlation": "Pearson r in [-1, 1]",
                "side_mid_ratio": "side energy divided by mid energy",
            },
            "method": "stereo metrics are not computed when silence is detected first",
            "reasons": {"correlation": reason, "side_mid_ratio": reason},
        },
        "junction": {
            "window_ms": 50.0,
            "channels": [],
            "start_rms_dbfs": None,
            "end_rms_dbfs": None,
            "delta_db": None,
            "units": {
                "window_ms": "ms",
                "start_rms_dbfs": "dBFS",
                "end_rms_dbfs": "dBFS",
                "delta_db": "dB (end - start)",
            },
            "method": "junction metrics are not computed when silence is detected first",
            "reasons": {
                "start_rms_dbfs": reason,
                "end_rms_dbfs": reason,
                "delta_db": reason,
            },
        },
        "loudness": _unavailable_loudness(reason),
    }


def analyze_pcm(samples, sample_rate, config) -> dict:
    """Analyse frames x channels float samples and return a JSON-safe analysis object."""

    validate_config(config)
    if isinstance(sample_rate, bool) or not isinstance(sample_rate, (int, np.integer)):
        raise ValueError("sample_rate must be an integer")
    sample_rate = int(sample_rate)
    if sample_rate <= 0:
        raise ValueError("sample_rate must be positive")

    array = np.asarray(samples, dtype=np.float64)
    if array.ndim == 1:
        array = array.reshape(-1, 1)
    if array.ndim != 2:
        raise ValueError("samples must have shape frames x channels")
    channels = int(array.shape[1])
    if channels <= 0:
        raise ValueError("samples must contain at least one channel")
    if not np.all(np.isfinite(array)):
        raise ValueError("samples must contain only finite values")
    if array.shape[0] == 0:
        return _silent_analysis(config, "empty signal", sample_rate, channels)

    mean_square = float(np.mean(array * array))
    peak = float(np.max(np.abs(array)))
    rms_db = _db_power(mean_square)
    silence_threshold = float(config["silence"]["rms_dbfs"])
    if mean_square <= 0.0 or (rms_db is not None and rms_db <= silence_threshold):
        return _silent_analysis(config, "silence", sample_rate, channels)

    peak_db = 20.0 * math.log10(peak) if peak > 0.0 else None
    level = {
        "rms_dbfs": rms_db,
        "peak_dbfs": peak_db,
        "crest_db": (peak_db - rms_db) if peak_db is not None and rms_db is not None else None,
        "units": {"rms_dbfs": "dBFS", "peak_dbfs": "dBFS", "crest_db": "dB"},
        "method": (
            "Native sample domain, unweighted; RMS pools mean square across channels "
            "and samples; peak is the largest absolute sample."
        ),
        "reasons": {},
    }
    if peak_db is None:
        level["reasons"]["peak_dbfs"] = "digital silence"
        level["reasons"]["crest_db"] = "digital silence"

    spectrum, bands, dynamics = _spectrum_and_dynamics(array, sample_rate, config)
    return {
        "silence": {
            "detected": False,
            "threshold_rms_dbfs": silence_threshold,
            "method": "full-signal native RMS, channels pooled; detected before qualifications",
        },
        "level": level,
        "spectrum": spectrum,
        "bands": bands,
        "dynamics": dynamics,
        "stereo": _stereo_analysis(array, config),
        "junction": _junction_analysis(array, sample_rate),
        "loudness": _unavailable_loudness(
            "loudness is measured from the encoded source by the extraction pipeline"
        ),
    }


# ---------------------------------------------------------------------------
# Deterministic acoustic sheet


def describe(analysis: dict, config: dict) -> str:
    """Return a compact deterministic acoustic sheet, without source names or emotion."""

    thresholds = config["description"]
    if analysis.get("silence", {}).get("detected"):
        return (
            "Silence mesuré sous le seuil configuré ; aucune qualification spectrale, "
            "temporelle ou stéréo n'est disponible."
        )

    shares = analysis.get("bands", {}).get("shares", {})
    available = [
        (name, float(shares[name]))
        for name in BAND_NAMES
        if isinstance(shares.get(name), (int, float))
        and not isinstance(shares.get(name), bool)
    ]
    if not available:
        first = "Répartition spectrale non disponible"
    else:
        band_name, band_share = max(available, key=lambda item: item[1])
        if band_share >= thresholds["dominant_band_share"]:
            first = f"Dominante {BAND_LABELS[band_name]}"
        else:
            first = "Énergie répartie sur plusieurs bandes"

    spread = analysis.get("dynamics", {}).get("spread_db")
    if not isinstance(spread, (int, float)) or isinstance(spread, bool):
        stability = "variations d'énergie non mesurables"
    elif spread <= thresholds["stable_spread_db"]:
        stability = "énergie très stable"
    elif spread <= thresholds["moderate_spread_db"]:
        stability = "énergie assez stable"
    else:
        stability = "fortes variations d'énergie"

    flatness = analysis.get("spectrum", {}).get("flatness")
    if not isinstance(flatness, (int, float)) or isinstance(flatness, bool):
        texture = "texture spectrale non mesurable"
    elif flatness >= thresholds["diffuse_flatness"]:
        texture = "spectre diffus"
    elif flatness <= thresholds["tonal_flatness"]:
        texture = "spectre concentré"
    else:
        texture = "texture spectrale intermédiaire"

    peak_rate = analysis.get("spectrum", {}).get("flux_peak_rate_hz")
    if not isinstance(peak_rate, (int, float)) or isinstance(peak_rate, bool):
        activity = "activité temporelle non mesurable"
    elif peak_rate <= thresholds["quiet_peak_rate_hz"]:
        activity = "peu de variations rapides détectées"
    elif peak_rate >= thresholds["busy_peak_rate_hz"]:
        activity = "variations rapides fréquentes"
    else:
        activity = "variations rapides modérées"

    correlation = analysis.get("stereo", {}).get("correlation")
    if not isinstance(correlation, (int, float)) or isinstance(correlation, bool):
        stereo = "métriques stéréo non applicables"
    elif correlation >= thresholds["mono_correlation"]:
        stereo = "canaux proches du mono"
    elif correlation <= thresholds["opposition_correlation"]:
        stereo = "canaux partiellement en opposition"
    else:
        stereo = "canaux partiellement décorrélés"

    junction_delta = analysis.get("junction", {}).get("delta_db")
    if not isinstance(junction_delta, (int, float)) or isinstance(junction_delta, bool):
        junction = "jonction de boucle non mesurable"
    elif abs(junction_delta) <= thresholds["junction_delta_db"]:
        junction = "énergie proche aux extrémités ; jonction à écouter"
    else:
        junction = "énergies différentes aux extrémités ; jonction à écouter"

    parts = [first, stability, texture, activity, stereo, junction]
    crest = analysis.get("level", {}).get("crest_db")
    if (
        isinstance(crest, (int, float))
        and not isinstance(crest, bool)
        and crest >= thresholds["marked_crest_db"]
    ):
        parts.append("crêtes marquées")
    return "; ".join(parts) + "."


# ---------------------------------------------------------------------------
# Annotations and playback metadata


def load_annotations(path) -> dict:
    """Read the optional human annotations. Missing file means no annotation."""

    if path is None:
        return {}
    path = Path(path)
    if not path.exists():
        return {}
    data = _load_json_file(path)
    _mapping(data, "annotations", {"schema_version", "tracks"})
    if _integer(data["schema_version"], "annotations.schema_version") != 1:
        raise ValueError("annotations.schema_version must be 1")
    tracks = data["tracks"]
    if not isinstance(tracks, dict):
        raise ValueError("annotations.tracks must be an object")
    normalised = {}
    for track_id, entry in tracks.items():
        if track_id not in TRACK_IDS:
            raise ValueError(f"annotations.tracks has unknown track id {track_id!r}")
        where = f"annotations.tracks.{track_id}"
        entry = _mapping(entry, where, set(), _ANNOTATION_ENTRY_KEYS)
        text = None
        provenance = None
        if "text" in entry:
            text = entry["text"]
            if not isinstance(text, str) or not text.strip():
                raise ValueError(f"{where}.text must be a non-empty string")
            provenance = entry.get("provenance", "human")
            if provenance != "human":
                raise ValueError(f"{where}.provenance must be 'human'")
        elif "provenance" in entry:
            raise ValueError(f"{where}.provenance requires a text")
        trim_db = _number(entry.get("trim_db", 0.0), f"{where}.trim_db")
        if not TRIM_DB_MIN <= trim_db <= TRIM_DB_MAX:
            raise ValueError(f"{where}.trim_db must be within [{TRIM_DB_MIN}, {TRIM_DB_MAX}]")
        trim_validated = _boolean(
            entry.get("trim_validated", False), f"{where}.trim_validated"
        )
        if trim_db != 0.0 and not trim_validated:
            raise ValueError(f"{where}.trim_db requires trim_validated true")
        normalised[track_id] = {
            "text": text,
            "provenance": provenance,
            "trim_db": trim_db,
            "trim_validated": trim_validated,
        }
    return normalised


def _annotation_and_playback(track_id: str, annotations: dict, config: dict):
    entry = annotations.get(track_id)
    if entry is None:
        return None, {
            "trim_db": 0.0,
            "max_gain": float(config["playback"]["max_gain"]),
            "trim_validated": False,
        }
    annotation = None
    if entry.get("text") is not None:
        annotation = {"text": entry["text"], "provenance": entry["provenance"]}
    return annotation, {
        "trim_db": float(entry["trim_db"]),
        "max_gain": float(config["playback"]["max_gain"]),
        "trim_validated": bool(entry["trim_validated"]),
    }


# ---------------------------------------------------------------------------
# Encoded sources, external tools and loudness


def resolve_sources(input_dir) -> dict:
    input_dir = Path(input_dir)
    if not input_dir.is_dir():
        raise FileNotFoundError(f"input directory does not exist: {input_dir}")
    sources = {}
    missing = []
    for track_id in TRACK_IDS:
        path = input_dir / f"{track_id}-loop.ogg"
        if path.is_file():
            sources[track_id] = path
        else:
            missing.append(str(path))
    if missing:
        raise FileNotFoundError(
            "missing required ambience source file(s): " + ", ".join(missing)
        )
    return sources


def sha256_file(path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def probe_audio(path, ffprobe: str = FFPROBE_BINARY):
    command = [
        ffprobe,
        "-v",
        "error",
        "-select_streams",
        "a:0",
        "-show_entries",
        "stream=sample_rate,channels",
        "-of",
        "json",
        str(path),
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError) as exc:
        raise RuntimeError(f"ffprobe is required but is unavailable: {exc}") from exc
    if result.returncode != 0:
        detail = (result.stderr or "").strip()[:400]
        raise RuntimeError(f"ffprobe failed for {path}: {detail}")
    try:
        payload = json.loads(result.stdout)
    except ValueError as exc:
        raise RuntimeError(f"ffprobe returned invalid JSON for {path}") from exc
    streams = payload.get("streams") if isinstance(payload, dict) else None
    if not isinstance(streams, list) or not streams:
        raise RuntimeError(f"no audio stream found in {path}")
    stream = streams[0]
    try:
        sample_rate = int(stream["sample_rate"])
        channels = int(stream["channels"])
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError(f"ffprobe did not report sample rate and channels for {path}") from exc
    if sample_rate <= 0 or channels <= 0:
        raise RuntimeError(f"invalid audio stream metadata for {path}")
    return sample_rate, channels


def decode_audio(path, ffmpeg: str = FFMPEG_BINARY, ffprobe: str = FFPROBE_BINARY):
    """Decode once at the native rate and channel count into float32 samples."""

    sample_rate, channels = probe_audio(path, ffprobe)
    command = [
        ffmpeg,
        "-v",
        "error",
        "-nostdin",
        "-xerror",
        "-err_detect",
        "explode",
        "-i",
        str(path),
        "-map",
        "0:a:0",
        "-f",
        "f32le",
        "-acodec",
        "pcm_f32le",
        "-",
    ]
    try:
        result = subprocess.run(command, capture_output=True, timeout=600)
    except (OSError, subprocess.SubprocessError) as exc:
        raise RuntimeError(f"ffmpeg is required but is unavailable: {exc}") from exc
    if result.returncode != 0:
        detail = result.stderr.decode("utf-8", "replace").strip()[:400]
        raise RuntimeError(f"ffmpeg failed to decode {path}: {detail}")
    raw = result.stdout
    if not raw:
        raise RuntimeError(f"ffmpeg decoded no samples from {path}")
    if len(raw) % 4:
        raise RuntimeError(f"ffmpeg returned a partial float32 sample for {path}")
    samples = np.frombuffer(raw, dtype="<f4").astype(np.float64)
    if samples.size % channels:
        raise RuntimeError(f"decoded sample count is not aligned on {channels} channels for {path}")
    if not np.all(np.isfinite(samples)):
        raise RuntimeError(f"decoded samples contain non-finite values for {path}")
    return samples.reshape(-1, channels), sample_rate, channels


def _tool_version(command: str) -> str:
    try:
        result = subprocess.run(
            [command, "-version"], capture_output=True, text=True, timeout=30
        )
    except (OSError, subprocess.SubprocessError) as exc:
        raise RuntimeError(f"{command} is required for extraction but is unavailable: {exc}") from exc
    if result.returncode != 0:
        raise RuntimeError(f"{command} -version failed with exit code {result.returncode}")
    text = result.stdout or result.stderr or ""
    for line in text.splitlines():
        if line.strip():
            return line.strip()
    raise RuntimeError(f"{command} -version returned no version line")


def collect_tools() -> dict:
    return {
        "python": platform.python_version(),
        "numpy": np.__version__,
        "scipy": scipy.__version__,
        "ffmpeg": _tool_version(FFMPEG_BINARY),
        "ffprobe": _tool_version(FFPROBE_BINARY),
    }


def _loudnorm_value(value):
    if not isinstance(value, str):
        return None
    try:
        number = float(value)
    except ValueError:
        return None
    return number if math.isfinite(number) else None


def measure_loudness(path, timeout_seconds: float, ffmpeg: str = FFMPEG_BINARY) -> dict:
    command = [
        ffmpeg,
        "-hide_banner",
        "-nostats",
        "-v",
        "info",
        "-i",
        str(path),
        "-af",
        "loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json",
        "-f",
        "null",
        "-",
    ]
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, timeout=float(timeout_seconds)
        )
    except (OSError, subprocess.SubprocessError) as exc:
        return _unavailable_loudness(f"ffmpeg loudnorm is unavailable: {exc}")
    if result.returncode != 0:
        return _unavailable_loudness(
            f"ffmpeg loudnorm failed with exit code {result.returncode}"
        )
    text = result.stderr or ""
    start = text.rfind("{")
    end = text.rfind("}")
    if start < 0 or end <= start:
        return _unavailable_loudness("ffmpeg loudnorm did not return JSON")
    try:
        payload = json.loads(text[start : end + 1])
    except ValueError:
        return _unavailable_loudness("ffmpeg loudnorm returned invalid JSON")
    integrated = _loudnorm_value(payload.get("input_i")) if isinstance(payload, dict) else None
    true_peak = _loudnorm_value(payload.get("input_tp")) if isinstance(payload, dict) else None
    reasons = {}
    if integrated is None:
        reasons["integrated_lufs"] = "loudnorm input_i is not a finite number"
    if true_peak is None:
        reasons["true_peak_dbtp"] = "loudnorm input_tp is not a finite number"
    return {
        "integrated_lufs": integrated,
        "true_peak_dbtp": true_peak,
        "method": "ffmpeg loudnorm input_i/input_tp, target -16 LUFS / -1.5 dBTP",
        "units": {"integrated_lufs": "LUFS", "true_peak_dbtp": "dBTP"},
        "reasons": reasons,
    }


# ---------------------------------------------------------------------------
# Catalogue validation, cache and atomic publication


def _assert_json_tree(value, where: str = "catalog") -> None:
    if value is None or isinstance(value, (str, bool)):
        return
    if isinstance(value, (int, np.integer)):
        return
    if isinstance(value, (float, np.floating)):
        if not math.isfinite(float(value)):
            raise ValueError(f"{where} contains a non-finite number")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            _assert_json_tree(item, f"{where}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            if not isinstance(key, str):
                raise ValueError(f"{where} contains a non-string key")
            _assert_json_tree(item, f"{where}.{key}")
        return
    raise ValueError(f"{where} contains an unsupported value of type {type(value).__name__}")


def _is_hex_digest(value) -> bool:
    return (
        isinstance(value, str)
        and len(value) == 64
        and all(character in "0123456789abcdef" for character in value)
    )


def _valid_analysis(analysis) -> bool:
    return isinstance(analysis, dict) and _ANALYSIS_KEYS <= set(analysis)


def validate_catalog(catalog) -> dict:
    _mapping(catalog, "catalog", _ROOT_KEYS)
    _assert_json_tree(catalog)
    if _integer(catalog["schema_version"], "catalog.schema_version") != SCHEMA_VERSION:
        raise ValueError("catalog.schema_version must be 1")
    if not isinstance(catalog["script_version"], str) or not catalog["script_version"]:
        raise ValueError("catalog.script_version must be a non-empty string")
    tools = catalog["tools"]
    if not isinstance(tools, dict):
        raise ValueError("catalog.tools must be an object")
    required_tools = {"python", "numpy", "scipy", "ffmpeg", "ffprobe"}
    if set(tools) != required_tools:
        raise ValueError("catalog.tools must contain python, numpy, scipy, ffmpeg and ffprobe")
    for name, value in tools.items():
        if not isinstance(value, str) or not value:
            raise ValueError(f"catalog.tools.{name} must be a non-empty string")
    config = catalog["config"]
    validate_config(config)
    if not _is_hex_digest(catalog["config_sha256"]):
        raise ValueError("catalog.config_sha256 must be a lowercase SHA-256 digest")
    if catalog["config_sha256"] != _content_hash(config):
        raise ValueError("catalog.config_sha256 does not match catalog.config")
    tracks = catalog["tracks"]
    if not isinstance(tracks, list) or len(tracks) != len(TRACK_IDS):
        raise ValueError("catalog.tracks must contain exactly four tracks")
    for index, track_id in enumerate(TRACK_IDS):
        where = f"catalog.tracks[{index}]"
        track = _mapping(tracks[index], where, _TRACK_KEYS)
        if track["id"] != track_id:
            raise ValueError(f"{where}.id must be {track_id!r} in the documented order")
        expected_path = TRACK_PATH_TEMPLATE.format(id=track_id)
        if track["path"] != expected_path:
            raise ValueError(f"{where}.path must be {expected_path!r}")
        if not _is_hex_digest(track["sha256"]):
            raise ValueError(f"{where}.sha256 must be a lowercase SHA-256 digest")
        if _number(track["duration_seconds"], f"{where}.duration_seconds") <= 0.0:
            raise ValueError(f"{where}.duration_seconds must be positive")
        if _integer(track["sample_rate"], f"{where}.sample_rate") <= 0:
            raise ValueError(f"{where}.sample_rate must be positive")
        if _integer(track["channels"], f"{where}.channels") <= 0:
            raise ValueError(f"{where}.channels must be positive")
        if not _valid_analysis(track["analysis"]):
            raise ValueError(f"{where}.analysis is missing required sections")
        if not isinstance(track["description"], str) or not track["description"].strip():
            raise ValueError(f"{where}.description must be a non-empty string")
        if track["rule_version"] != RULE_VERSION:
            raise ValueError(f"{where}.rule_version must be {RULE_VERSION!r}")
        annotation = track["annotation"]
        if annotation is not None:
            _mapping(annotation, f"{where}.annotation", {"text", "provenance"})
            if not isinstance(annotation["text"], str) or not annotation["text"].strip():
                raise ValueError(f"{where}.annotation.text must be a non-empty string")
            if annotation["provenance"] != "human":
                raise ValueError(f"{where}.annotation.provenance must be 'human'")
        playback = _mapping(tracks[index]["playback"], f"{where}.playback", _PLAYBACK_KEYS)
        trim_db = _number(playback["trim_db"], f"{where}.playback.trim_db")
        if not TRIM_DB_MIN <= trim_db <= TRIM_DB_MAX:
            raise ValueError(f"{where}.playback.trim_db must be within [-24, 6]")
        max_gain = _number(playback["max_gain"], f"{where}.playback.max_gain")
        if not 0.0 < max_gain <= 1.0:
            raise ValueError(f"{where}.playback.max_gain must be in (0, 1]")
        _boolean(playback["trim_validated"], f"{where}.playback.trim_validated")
        if trim_db != 0 and not playback["trim_validated"]:
            raise ValueError(f"{where}: nonzero trim requires human validation")
    expected_version = _content_hash(
        {key: value for key, value in catalog.items() if key != "catalog_version"}
    )
    if catalog["catalog_version"] != expected_version:
        raise ValueError("catalog.catalog_version does not match the catalogue content")
    if not _is_hex_digest(catalog["catalog_version"]):
        raise ValueError("catalog.catalog_version must be a lowercase SHA-256 digest")
    return catalog


def _cache_hit(previous, track_id: str, source_sha: str, config_sha: str, tools: dict):
    if not isinstance(previous, dict):
        return None
    if previous.get("schema_version") != SCHEMA_VERSION:
        return None
    if previous.get("script_version") != SCRIPT_VERSION:
        return None
    if previous.get("config_sha256") != config_sha:
        return None
    if previous.get("tools") != tools:
        return None
    tracks = previous.get("tracks")
    if not isinstance(tracks, list):
        return None
    for track in tracks:
        if not isinstance(track, dict) or track.get("id") != track_id:
            continue
        if track.get("sha256") != source_sha:
            return None
        if track.get("path") != TRACK_PATH_TEMPLATE.format(id=track_id):
            return None
        if not _valid_analysis(track.get("analysis")):
            return None
        return track
    return None


def _load_previous_catalog(path):
    path = Path(path)
    if not path.is_file():
        return None
    try:
        catalog = _load_json_file(path)
        validate_catalog(catalog)
        return catalog
    except (OSError, TypeError, ValueError):
        return None


def build_catalog(
    input_dir,
    output_path,
    config_path=None,
    annotations_path=None,
    force: bool = False,
    *,
    tool_versions=None,
    decode=None,
    loudness_measure=None,
) -> dict:
    input_dir = Path(input_dir)
    config_path = (
        Path(config_path)
        if config_path is not None
        else input_dir / "ambience_analysis_config.json"
    )
    annotations_path = (
        Path(annotations_path)
        if annotations_path is not None
        else input_dir / "ambience_annotations.json"
    )
    config = load_config(config_path)
    config_sha = _content_hash(config)
    tools = collect_tools() if tool_versions is None else _round_numbers(tool_versions)
    if not isinstance(tools, dict) or not tools:
        raise ValueError("tool versions must be a non-empty object")
    decoder = decode_audio if decode is None else decode
    loudness_fn = measure_loudness if loudness_measure is None else loudness_measure
    sources = resolve_sources(input_dir)
    protected = [*sources.values(), config_path, annotations_path]
    if Path(output_path).resolve() in [Path(path).resolve() for path in protected]:
        raise ValueError("output must not overwrite a source, config or annotation file")
    annotations = load_annotations(annotations_path)
    previous = None if force else _load_previous_catalog(output_path)

    tracks = []
    for track_id in TRACK_IDS:
        source = sources[track_id]
        source_sha = sha256_file(source)
        cached = (
            None
            if force
            else _cache_hit(previous, track_id, source_sha, config_sha, tools)
        )
        if cached is not None:
            analysis = copy.deepcopy(cached["analysis"])
            duration = float(cached["duration_seconds"])
            sample_rate = int(cached["sample_rate"])
            channels = int(cached["channels"])
        else:
            samples, sample_rate, channels = decoder(source)
            samples = np.asarray(samples, dtype=np.float64)
            sample_rate = int(sample_rate)
            channels = int(channels)
            if sample_rate <= 0 or channels <= 0:
                raise ValueError(f"decoded audio metadata is invalid for {track_id}")
            if samples.ndim != 2 or samples.shape[0] == 0 or samples.shape[1] != channels:
                raise ValueError(f"decoded samples for {track_id} have an unexpected shape")
            duration = samples.shape[0] / float(sample_rate)
            analysis = analyze_pcm(samples, sample_rate, config)
            if analysis["silence"]["detected"]:
                loudness_block = _unavailable_loudness("silence")
            elif config["loudness"]["enabled"]:
                loudness_block = loudness_fn(
                    str(source), float(config["loudness"]["timeout_seconds"])
                )
            else:
                loudness_block = _unavailable_loudness(
                    "loudness analysis is disabled in configuration"
                )
            analysis["loudness"] = loudness_block

        analysis = _round_numbers(analysis)
        description = describe(analysis, config)
        annotation, playback = _annotation_and_playback(track_id, annotations, config)
        tracks.append(
            {
                "id": track_id,
                "path": TRACK_PATH_TEMPLATE.format(id=track_id),
                "sha256": source_sha,
                "duration_seconds": duration,
                "sample_rate": sample_rate,
                "channels": channels,
                "analysis": analysis,
                "description": description,
                "rule_version": RULE_VERSION,
                "annotation": annotation,
                "playback": playback,
            }
        )

    catalog = {
        "schema_version": SCHEMA_VERSION,
        "script_version": SCRIPT_VERSION,
        "tools": tools,
        "config": config,
        "config_sha256": config_sha,
        "tracks": tracks,
    }
    catalog = _round_numbers(catalog)
    catalog["catalog_version"] = _content_hash(catalog)
    validate_catalog(catalog)
    write_catalog(output_path, catalog)
    return catalog


def write_catalog(output_path, catalog) -> None:
    """Validate, write to a sibling temporary file, re-read it, then replace atomically."""

    output_path = Path(output_path)
    parent = output_path.parent
    if not parent.is_dir():
        raise FileNotFoundError(f"output directory does not exist: {parent}")
    text = (
        json.dumps(
            catalog,
            ensure_ascii=False,
            sort_keys=True,
            indent=2,
            allow_nan=False,
        )
        + "\n"
    )
    descriptor, temporary_name = tempfile.mkstemp(
        prefix=output_path.name + ".", suffix=".tmp", dir=str(parent)
    )
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
            handle.flush()
            os.fsync(handle.fileno())
        reread = Path(temporary_name).read_text(encoding="utf-8")
        parsed = _loads_strict(reread)
        validate_catalog(parsed)
        if parsed != catalog:
            raise ValueError("catalogue re-read does not match the written value")
        os.replace(str(temporary_name), str(output_path))
    except BaseException:
        try:
            os.unlink(temporary_name)
        except OSError:
            pass
        raise


# ---------------------------------------------------------------------------
# Command line


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Measure the four ambience loops and write a deterministic catalogue."
    )
    parser.add_argument("--input", required=True, help="directory containing <id>-loop.ogg")
    parser.add_argument("--output", required=True, help="catalogue JSON path")
    parser.add_argument(
        "--config",
        default=None,
        help="analysis config (default: <input>/ambience_analysis_config.json)",
    )
    parser.add_argument(
        "--annotations",
        default=None,
        help="human annotations (default: <input>/ambience_annotations.json)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="ignore a compatible previous catalogue and analyse every source again",
    )
    arguments = parser.parse_args(argv)
    try:
        catalog = build_catalog(
            arguments.input,
            arguments.output,
            arguments.config,
            arguments.annotations,
            force=arguments.force,
        )
    except (OSError, TypeError, ValueError, RuntimeError) as exc:
        print(f"analyze_ambience: error: {exc}", file=sys.stderr)
        return 1
    print(
        f"analyze_ambience: wrote {arguments.output} "
        f"for {len(catalog['tracks'])} tracks"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
