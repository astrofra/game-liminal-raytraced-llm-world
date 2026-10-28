"""Regression for alias loss in constrained Gemma decoding; no MLX required."""
import unittest
from mlx_worker import token_vocabulary


class Tokenizer:
    all_special_ids = [0]

    def get_vocab(self):
        return {'<eos>': 0, '3': 100, '<0x33>': 3, '_': 101, '<0x5F>': 4,
                '<0xC3>': 5, '<0xA9>': 6, 'é': 102, '▁mot': 103}

    def convert_tokens_to_string(self, tokens):
        return ''.join(tokens).replace('▁', ' ')


class VocabularyTest(unittest.TestCase):
    def test_keeps_normal_and_byte_aliases_independent_of_vocab_order(self):
        tokenizer = Tokenizer()
        original = token_vocabulary(tokenizer)
        vocab = tokenizer.get_vocab()
        tokenizer.get_vocab = lambda: dict(reversed(list(vocab.items())))
        reordered = token_vocabulary(tokenizer)
        self.assertEqual(set(original[b'3']), {3, 100})
        self.assertEqual(set(original[b'_']), {4, 101})
        self.assertEqual({k: set(v) for k, v in original.items()},
                         {k: set(v) for k, v in reordered.items()})

    def test_preserves_utf8_byte_fallback_and_spaces_but_excludes_control(self):
        vocabulary = token_vocabulary(Tokenizer())
        self.assertEqual(vocabulary[b'\xc3'], [5])
        self.assertEqual(vocabulary[b'\xa9'], [6])
        self.assertEqual(vocabulary['é'.encode()], [102])
        self.assertEqual(vocabulary[b' mot'], [103])
        self.assertNotIn(b'<eos>', vocabulary)


if __name__ == '__main__':
    unittest.main()
