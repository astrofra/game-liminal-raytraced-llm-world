"""GGUF v3 extraction failures must be caught before publishing; no model needed."""
import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
import prepare_mlx_cpp as vocabulary


def text(value):
    raw = value.encode()
    return struct.pack('<Q', len(raw)) + raw


def header(count, tensor_count=0, magic=b'GGUF', version=3):
    return struct.pack('<4sIQQ', magic, version, tensor_count, count)


def entry(key, kind, payload):
    return text(key) + struct.pack('<I', kind) + payload


def strings(*values):
    return struct.pack('<IQ', 8, len(values)) + b''.join(text(value) for value in values)


METADATA = (entry('general.architecture', 8, text('gemma3'))
            + entry('gemma3.block_count', 4, struct.pack('<I', 26))
            + entry('tokenizer.ggml.tokens', 9, strings('<start_of_turn>', '<end_of_turn>', 'hello')))


class ExtractMetadataTests(unittest.TestCase):
    def extract(self, payload):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'source.gguf'
            destination = root / 'tokenizer.gguf'
            source.write_bytes(payload)
            vocabulary.extract_metadata(source, destination)
            return destination.read_bytes()

    def test_metadata_is_preserved_and_tensors_are_dropped(self):
        payload = header(3, tensor_count=2) + METADATA + b'TENSOR-DESCRIPTOR' + b'TENSOR-DATA' * 4
        output = self.extract(payload)
        padding = -(24 + len(METADATA)) % 32
        self.assertEqual(output[:8], b'GGUF' + struct.pack('<I', 3))
        self.assertEqual(struct.unpack('<Q', output[8:16])[0], 0)
        self.assertEqual(struct.unpack('<Q', output[16:24])[0], 3)
        self.assertEqual(output[24:24 + len(METADATA)], METADATA)
        self.assertEqual(output[24 + len(METADATA):], b'\0' * padding)
        self.assertEqual(len(output) % 32, 0)
        self.assertNotIn(b'TENSOR', output)
        self.assertLess(len(output), len(payload))

    def test_invalid_header_is_rejected(self):
        for label, payload in {'magic': header(0, magic=b'GGUX'),
                               'version': header(0, version=2),
                               'count': header(100_001)}.items():
            with self.subTest(label):
                with self.assertRaisesRegex(ValueError, 'GGUF v3 header'):
                    self.extract(payload)

    def test_truncated_metadata_is_rejected(self):
        for label, payload in {'key': header(1) + struct.pack('<Q', 10) + b'abc',
                               'value': header(1) + entry('key', 8, struct.pack('<Q', 5) + b'ab')}.items():
            with self.subTest(label):
                with self.assertRaisesRegex(ValueError, 'Truncated GGUF metadata'):
                    self.extract(payload)

    def test_unknown_value_type_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unsupported GGUF value type 99'):
            self.extract(header(1) + entry('key', 99, b''))

    def test_excessive_counts_are_rejected(self):
        with self.assertRaisesRegex(ValueError, 'bounded'):
            self.extract(header(100_001))
        with self.assertRaisesRegex(ValueError, 'Oversized GGUF array'):
            self.extract(header(1) + entry('key', 9, struct.pack('<IQ', 8, 1_000_001)))

    def test_nested_array_is_rejected(self):
        payload = header(1) + entry('key', 9, struct.pack('<IQ', 9, 1))
        with self.assertRaisesRegex(ValueError, 'Unsupported GGUF value type 9'):
            self.extract(payload)


class VerifiedTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.payload = b'synthetic vocabulary'
        self.path = self.root / 'tokenizer.gguf'
        self.path.write_bytes(self.payload)
        self.spec = {'size_bytes': len(self.payload),
                     'sha256': hashlib.sha256(self.payload).hexdigest()}

    def test_matching_size_and_digest(self):
        self.assertTrue(vocabulary.verified(self.path, self.spec))

    def test_digest_mismatch_is_rejected(self):
        self.assertFalse(vocabulary.verified(self.path, {**self.spec, 'sha256': '0' * 64}))

    def test_size_mismatch_is_rejected(self):
        self.assertFalse(vocabulary.verified(self.path, {**self.spec, 'size_bytes': len(self.payload) + 1}))

    def test_missing_file_is_rejected(self):
        self.assertFalse(vocabulary.verified(self.root / 'missing.gguf', self.spec))


if __name__ == '__main__':
    unittest.main()
