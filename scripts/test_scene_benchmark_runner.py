"""Failure-path tests; no model loading or external process required."""

import pathlib
import subprocess
import tempfile
import types
import unittest
from unittest import mock

import run_scene_generation_benchmark as benchmark


PROMPT = 'Compile a scene.\n\n'
NORMAL = 'room "test"\ncamera eye(0,1,-8) target(0,1,4) up(0,1,0) fov(46)'
DEGENERATE = 'room "test"\ncamera eye(0,1.75,0) target(0,1.18,0) up(0,1,0) fov(46)'
EOS = 'D slot process_toke: id 0 | task 0 | stopped by EOS\n'


class RunnerTests(unittest.TestCase):
    def run_case(self, scene=NORMAL, log=EOS, timeout=False, live=None):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)

            def generate(*args):
                if timeout:
                    raise subprocess.TimeoutExpired(['llama-cli'], 0.01, output=b'partial', stderr=b'timeout log')
                args[-1].parent.mkdir(parents=True, exist_ok=True)
                args[-1].write_text('User:\nCompile a scene.\n\nAssistant:\n' + scene, encoding='utf-8')
                return subprocess.CompletedProcess(['llama-cli'], 0, stdout=scene if live is None else live, stderr=log), 100

            with mock.patch.object(benchmark, 'dump_scene_audit_prompt', return_value=(
                subprocess.CompletedProcess([], 0, stdout=PROMPT, stderr=''), 1)), \
                mock.patch.object(benchmark, 'generate_scene_with_llama', side_effect=generate), \
                mock.patch.object(benchmark, 'audit_and_render_scene', return_value=(
                    subprocess.CompletedProcess([], 0, stdout='Loaded 12 triangles, 1 materials, 0 emissive triangles', stderr=''), 2)) as audit:
                result = benchmark.render_case(
                    case={'slug': 'test'}, renderer_path=root/'renderer', llama_cli_path=root/'cli',
                    model_path=root/'model', output_root=root, width=80, height=40, samples=1,
                    bounces=1, direct_samples=1, render_seed=77, exposure=1, ctx_size=4096,
                    n_predict=1024, temperature=0, seed=42,
                    options=types.SimpleNamespace(reasoning_budget=0, reasoning='off', camera_guidance=False))
                return result, audit.call_count

    def test_complete_scene_passes_both_checks(self):
        result, calls = self.run_case()
        self.assertTrue(result['valid_scene'])
        self.assertTrue(result['syntax_valid_scene'])
        self.assertEqual(calls, 1)

    def test_renderer_acceptance_does_not_hide_invalid_camera(self):
        result, calls = self.run_case(scene=DEGENERATE)
        self.assertEqual(result['status'], 'invalid_camera')
        self.assertTrue(result['syntax_valid_scene'])
        self.assertFalse(result['valid_scene'])
        self.assertEqual(calls, 1)  # Keep the diagnostic image.

    def test_token_limit_never_reaches_renderer(self):
        result, calls = self.run_case(log='D slot process_toke: id 0 | task 0 | stopped by limit')
        self.assertEqual(result['status'], 'incomplete_generation')
        self.assertEqual(calls, 0)

    def test_unknown_stop_never_reaches_renderer(self):
        result, calls = self.run_case(log='')
        self.assertEqual(result['status'], 'incomplete_generation')
        self.assertEqual(calls, 0)

    def test_incomplete_thinking_never_reaches_renderer(self):
        result, calls = self.run_case(scene='[Start thinking]draft[End thinking]' + NORMAL,
                                      live='[Start thinking]draft')
        self.assertEqual(result['status'], 'invalid_protocol')
        self.assertEqual(calls, 0)

    def test_timeout_never_reaches_renderer(self):
        result, calls = self.run_case(timeout=True)
        self.assertEqual(result['status'], 'timeout')
        self.assertEqual(result['stop_reason'], 'timeout')
        self.assertEqual(calls, 0)


class ProfileDefaultsTests(unittest.TestCase):
    def options(self, profile, **overrides):
        values = dict(profile=profile, temperature=None, top_k=None, top_p=None,
                      reasoning=None, reasoning_budget=None)
        values.update(overrides)
        options = types.SimpleNamespace(**values)
        benchmark.apply_profile_defaults(options)
        return options

    def test_gemma_profile_defaults(self):
        options = self.options('gemma-4-e4b-q4')
        self.assertEqual((options.reasoning, options.reasoning_budget, options.temperature), ('off', 0, 0))

    def test_explicit_zero_temperature_and_reasoning_off_are_preserved(self):
        options = self.options('gemma-4-e4b-q4', temperature=0, reasoning='off', top_k=1)
        self.assertEqual((options.temperature, options.reasoning_budget, options.top_k), (0, 0, 1))

    def test_profile_has_no_reasoning(self):
        for profile in ('gemma-4-e4b-q4',):
            options = self.options(profile)
            self.assertEqual((options.reasoning, options.reasoning_budget, options.temperature), ('off', 0, 0))


if __name__ == '__main__':
    unittest.main()
