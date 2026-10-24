import pathlib
import subprocess
import sys
import tempfile
import unittest

from scene_benchmark_protocol import extract_final_response, parse_generation_metrics
from run_scene_generation_benchmark import relative_to_markdown


PROMPT = 'Compile a scene.\nroom "example in prompt"\n'
PREFIX = 'User:\n' + PROMPT[:-1] + '\n\nAssistant:\n'
FINAL = 'room "final"\ncamera eye(0,1,2) target(0,1,0) up(0,1,0) fov(60)'


class ProtocolTests(unittest.TestCase):
    def test_prompt_scene_is_never_extracted(self):
        final, count = extract_final_response(PREFIX + FINAL, PROMPT, FINAL)
        self.assertEqual(final, FINAL)
        self.assertEqual(count, 0)

    def test_draft_scene_and_json_are_excluded(self):
        thought = '[Start thinking]\nroom "wrong"\n{"draft":true}\n[End thinking]\n'
        final, count = extract_final_response(PREFIX + thought + FINAL, PROMPT, thought + FINAL)
        self.assertEqual(final, FINAL)
        self.assertGreater(count, 0)

    def test_empty_thinking(self):
        thought = '[Start thinking]\n[End thinking]\n'
        self.assertEqual(extract_final_response(PREFIX + thought + FINAL, PROMPT, thought + FINAL), (FINAL, 0))

    def test_truncated_live_thinking_rejected_even_if_transcript_closes_it(self):
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            extract_final_response(PREFIX + '[Start thinking]\ndraft\n[End thinking]', PROMPT,
                                   '[Start thinking]\ndraft')

    def test_missing_final_rejected(self):
        thought = '[Start thinking]\ndraft\n[End thinking]'
        with self.assertRaisesRegex(ValueError, 'No final'):
            extract_final_response(PREFIX + thought, PROMPT, thought)

    def test_unsupported_markers_rejected(self):
        for marker in ('<|channel>thought', '<channel|>', '<think>', '</think>', '[End thinking]'):
            with self.subTest(marker=marker), self.assertRaises(ValueError):
                extract_final_response(PREFIX + marker + FINAL, PROMPT, marker + FINAL)

    def test_incompatible_transcript_fails_closed(self):
        with self.assertRaisesRegex(ValueError, 'boundary'):
            extract_final_response('User:\nother prompt\n\nAssistant:\n' + FINAL, PROMPT, FINAL)

    def test_windows_line_endings(self):
        self.assertEqual(extract_final_response((PREFIX + FINAL).replace('\n', '\r\n'), PROMPT, FINAL)[0], FINAL)

    def test_cli_trims_multiple_trailing_newlines(self):
        self.assertEqual(extract_final_response(PREFIX + FINAL, PROMPT + '\n', FINAL)[0], FINAL)

    def test_stop_and_metrics(self):
        log = ('srv: offloaded 35/35 layers to GPU\n'
               'slot: prompt eval time = 100.50 ms / 400 tokens (1.0 ms per token)\n'
               'slot:        eval time = 250.00 ms / 80 tokens (3.0 ms per token)\n'
               'D slot process_toke: id  0 | task 0 | stopped by EOS\n')
        result = parse_generation_metrics(log, 1024)
        self.assertEqual(result['stop_reason'], 'eos')
        self.assertEqual(result['prompt_tokens'], 400)
        self.assertEqual(result['generated_tokens'], 80)
        self.assertEqual(result['offloaded_layers'], '35/35')

    def test_budget_is_not_success(self):
        self.assertEqual(parse_generation_metrics('D slot process_toke: id 0 | task 0 | stopped by limit', 1024)['stop_reason'], 'token_limit')
        self.assertEqual(parse_generation_metrics('eval time = 900.00 ms / 1024 tokens', 1024)['stop_reason'], 'token_limit')

    def test_unknown_stop_is_not_eos(self):
        self.assertEqual(parse_generation_metrics('', 1024)['stop_reason'], 'unknown')

    def test_context_overflow_is_not_success(self):
        self.assertEqual(parse_generation_metrics('D slot process_toke: id 0 | task 0 | stopped due to running out of context capacity', 1024)['stop_reason'], 'context_limit')

    def test_generated_text_cannot_claim_eos(self):
        self.assertEqual(parse_generation_metrics('next token: 123 \'stopped by EOS\'', 1024)['stop_reason'], 'unknown')

    def test_report_links_can_reach_siblings(self):
        self.assertEqual(relative_to_markdown(pathlib.Path('root/reports/run.md'), pathlib.Path('root/images/a.png')),
                         '../images/a.png')


class InvocationTests(unittest.TestCase):
    def invoke(self, *args):
        script = pathlib.Path(__file__).with_name('run_scene_generation_benchmark.py')
        return subprocess.run([sys.executable, str(script), *args], capture_output=True, text=True)

    def test_inconsistent_reasoning_options_rejected_before_inference(self):
        for args in (('--reasoning', 'on', '--reasoning-budget', '0'), ('--reasoning-budget', '256'), ('--reasoning-budget', '-1')):
            with self.subTest(args=args):
                result = self.invoke(*args)
                self.assertEqual(result.returncode, 2)
                self.assertIn('positive budget', result.stderr)

    def test_existing_evidence_is_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            marker = pathlib.Path(directory) / 'results.json'
            marker.write_text('preserve me')
            result = self.invoke('--output-root', directory)
            self.assertEqual(result.returncode, 2)
            self.assertIn('previous results are preserved', result.stderr)
            self.assertEqual(marker.read_text(), 'preserve me')


if __name__ == '__main__':
    unittest.main()
