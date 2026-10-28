"""Offline tests for the mlx_worker JSON-lines protocol and file integrity checks.

No MLX, GPU, network or real model is touched: Runtime.handle is never called and
verify_model only sees miniature files in a temporary directory.
"""
import hashlib
import io
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import mlx_worker
from mlx_worker import WorkerError, serve, verify_model


class FakeRuntime:
    """Stand-in for Runtime whose handle is supplied by each test."""

    def __init__(self, process):
        self.process = process
        self.requests = []

    def handle(self, request, emit):
        self.requests.append(request)
        self.process(request, emit)


def decode(destination):
    return [json.loads(line) for line in destination.getvalue().splitlines()]


class ProtocolTest(unittest.TestCase):
    def call_serve(self, text, process):
        runtime = FakeRuntime(process)
        destination = io.StringIO()
        serve(io.StringIO(text), destination, runtime)
        return runtime, destination

    def test_processes_every_line_until_end_of_stream(self):
        payloads = [
            {'protocol': 1, 'id': 1, 'messages': [{'role': 'user', 'content': 'Salut'}]},
            {'protocol': 1, 'id': 2, 'messages': [], 'count_only': True},
        ]

        def process(request, emit):
            emit({'type': 'ready', 'id': request['id']})
            emit({'type': 'delta', 'id': request['id'], 'text': 'ok'})
            emit({'type': 'done', 'id': request['id'], 'success': True})

        runtime, destination = self.call_serve(
            ''.join(json.dumps(payload) + '\n' for payload in payloads), process)
        expected = []
        for payload in payloads:
            expected.extend([
                {'type': 'ready', 'id': payload['id']},
                {'type': 'delta', 'id': payload['id'], 'text': 'ok'},
                {'type': 'done', 'id': payload['id'], 'success': True},
            ])
        self.assertEqual(runtime.requests, payloads)
        self.assertEqual(decode(destination), expected)

    def test_runtime_stdout_noise_is_kept_out_of_the_protocol(self):
        noise = io.StringIO()

        def process(request, emit):
            print('stray runtime output')
            emit({'type': 'ready'})

        with patch('sys.stderr', noise):
            runtime, destination = self.call_serve(json.dumps({'protocol': 1}) + '\n', process)
        self.assertEqual(decode(destination), [{'type': 'ready'}])
        self.assertNotIn('stray runtime output', destination.getvalue())
        self.assertIn('stray runtime output', noise.getvalue())

    def test_worker_error_is_reported_and_stream_continues(self):
        def process(request, emit):
            if request['id'] == 1:
                raise WorkerError('context_limit', 'Prompt and output exceed fixed context')
            if request['id'] == 2:
                raise ValueError('unexpected failure')
            emit({'type': 'done', 'success': True})

        runtime, destination = self.call_serve(
            ''.join(json.dumps({'protocol': 1, 'id': index}) + '\n' for index in (1, 2, 3)),
            process)
        self.assertEqual(len(runtime.requests), 3)
        records = decode(destination)
        self.assertEqual(records[0], {
            'type': 'error',
            'stop_reason': 'context_limit',
            'error_message': 'Prompt and output exceed fixed context',
        })
        self.assertEqual(records[1]['type'], 'error')
        self.assertEqual(records[1]['stop_reason'], 'error')
        self.assertIn('unexpected failure', records[1]['error_message'])
        self.assertEqual(records[2], {'type': 'done', 'success': True})

    def test_unreadable_json_is_reported_and_stream_continues(self):
        def process(request, emit):
            emit({'type': 'done', 'success': True})

        valid = {'protocol': 1, 'id': 7}
        runtime, destination = self.call_serve('{"protocol": 1\n' + json.dumps(valid) + '\n', process)
        records = decode(destination)
        self.assertEqual(records[0]['type'], 'error')
        self.assertEqual(records[0]['stop_reason'], 'error')
        self.assertEqual(runtime.requests, [valid])
        self.assertEqual(records[1], {'type': 'done', 'success': True})

    def test_rejects_oversized_or_incomplete_request(self):
        def process(request, emit):
            emit({'type': 'done'})

        with patch.object(mlx_worker, 'MAX_REQUEST_BYTES', 32):
            cases = {
                'oversized': 'x' * 40 + '\n' + json.dumps({'protocol': 1}) + '\n',
                'incomplete': json.dumps({'protocol': 1, 'id': 1}),
            }
            for label, text in cases.items():
                with self.subTest(case=label):
                    runtime, destination = self.call_serve(text, process)
                    self.assertEqual(runtime.requests, [])
                    records = decode(destination)
                    self.assertEqual(len(records), 1)
                    self.assertEqual(records[0]['type'], 'error')
                    self.assertEqual(records[0]['stop_reason'], 'protocol_error')

    def test_end_of_stream_is_silent(self):
        runtime, destination = self.call_serve('', lambda request, emit: emit({'type': 'done'}))
        self.assertEqual(runtime.requests, [])
        self.assertEqual(destination.getvalue(), '')


class VerifyModelTest(unittest.TestCase):
    def make_model(self, folder, files):
        model = Path(folder) / 'model'
        model.mkdir()
        entries = {}
        for name, data in files.items():
            (model / name).write_bytes(data)
            entries[name] = {'size_bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
        manifest = Path(folder) / 'models.json'
        manifest.write_text(json.dumps({'files': entries}), encoding='utf-8')
        return model, manifest, entries

    def test_accepts_matching_manifest_and_files(self):
        with tempfile.TemporaryDirectory() as folder:
            model, manifest, entries = self.make_model(
                folder, {'config.json': b'{}', 'README.md': b'mini'})
            result = verify_model(model, manifest)
            self.assertEqual(result['files'], entries)

    def test_missing_file_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            model, manifest, _ = self.make_model(
                folder, {'config.json': b'{}', 'README.md': b'mini'})
            (model / 'README.md').unlink()
            with self.assertRaises(WorkerError) as caught:
                verify_model(model, manifest)
            self.assertEqual(caught.exception.reason, 'model_missing')
            self.assertIn('README.md', str(caught.exception))

    def test_wrong_size_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            model, manifest, _ = self.make_model(folder, {'config.json': b'{}'})
            (model / 'config.json').write_bytes(b'{}extra')
            with self.assertRaises(WorkerError) as caught:
                verify_model(model, manifest)
            self.assertEqual(caught.exception.reason, 'model_corrupt')
            self.assertIn('Wrong MLX file size', str(caught.exception))

    def test_wrong_hash_with_same_size_is_reported(self):
        with tempfile.TemporaryDirectory() as folder:
            model, manifest, _ = self.make_model(folder, {'config.json': b'{}'})
            (model / 'config.json').write_bytes(b'[]')
            with self.assertRaises(WorkerError) as caught:
                verify_model(model, manifest)
            self.assertEqual(caught.exception.reason, 'model_corrupt')
            self.assertIn('Wrong MLX SHA-256', str(caught.exception))


if __name__ == '__main__':
    unittest.main()
