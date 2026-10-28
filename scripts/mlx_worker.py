"""Private JSON-lines worker owned by the C++ game (stdin/stdout, no server).

Only weights/tokenizer/compiled schema indexes persist. Every generation gets a
fresh KV cache, sampler and grammar state. stdout is reserved for the protocol.
"""
from __future__ import annotations
import contextlib
import hashlib
import json
import os
from pathlib import Path
import sys
import time

os.environ['HF_HUB_OFFLINE'] = '1'
os.environ['TRANSFORMERS_OFFLINE'] = '1'
os.environ['TOKENIZERS_PARALLELISM'] = 'false'
PROTOCOL = 1
MAX_REQUEST_BYTES = 4 * 1024 * 1024


class WorkerError(Exception):
    def __init__(self, reason, message):
        super().__init__(message)
        self.reason = reason


def verify_model(directory, manifest_path):
    manifest = json.loads(Path(manifest_path).read_text())
    for name, spec in manifest['files'].items():
        path = directory / name
        if not path.is_file():
            raise WorkerError('model_missing', f'Missing MLX file: {path}')
        if path.stat().st_size != spec['size_bytes']:
            raise WorkerError('model_corrupt', f'Wrong MLX file size: {path}')
        with path.open('rb') as source:
            digest = hashlib.file_digest(source, 'sha256').hexdigest()
        if digest != spec['sha256']:
            raise WorkerError('model_corrupt', f'Wrong MLX SHA-256: {path}')
    return manifest


def token_vocabulary(tokenizer):
    """Preserve aliases and byte fallback tokens (Gemma has both '3' and <0x33>).

    Overwriting aliases discards ordinary digit/punctuation tokens and corrupts
    sampling probabilities, even though the decoded JSON remains syntactic.
    """
    vocabulary = {}
    special = set(tokenizer.all_special_ids)
    for token, token_id in tokenizer.get_vocab().items():
        if token_id in special:
            continue
        if len(token) == 6 and token.startswith('<0x') and token.endswith('>'):
            piece = bytes([int(token[3:5], 16)])
        else:
            piece = tokenizer.convert_tokens_to_string([token]).encode('utf-8')
        if piece:
            vocabulary.setdefault(piece, []).append(token_id)
    return vocabulary


class Runtime:
    def __init__(self):
        self.model = self.tokenizer = self.vocabulary = None
        self.path = None
        self.indexes = {}
        self.fingerprint = None

    def handle(self, request, emit):
        if request.get('protocol') != PROTOCOL:
            raise WorkerError('protocol_error', 'Unsupported worker protocol')
        import mlx.core as mx
        from mlx_lm import load, stream_generate
        from mlx_lm.sample_utils import make_sampler
        from jsonschema import Draft202012Validator
        started = time.monotonic()
        path = Path(request['model_path']).resolve()
        manifest_path = Path(__file__).with_name('models_mlx.json')
        manifest = json.loads(manifest_path.read_text())
        try:
            fingerprint = [(name, (path / name).stat().st_size, (path / name).stat().st_mtime_ns)
                           for name in manifest['files']]
        except FileNotFoundError as error:
            raise WorkerError('model_missing', str(error)) from error
        reused = self.model is not None and self.path == path and self.fingerprint == fingerprint
        if not reused:
            verify_model(path, manifest_path)
            if not mx.metal.is_available():
                raise WorkerError('unavailable', 'MLX Metal device unavailable')
            self.model = self.tokenizer = self.vocabulary = None
            self.indexes.clear()
            self.model, self.tokenizer = load(str(path), tokenizer_config={'trust_remote_code': False})
            mx.eval(self.model.parameters())
            self.path = path
            self.fingerprint = fingerprint
        load_ms = (time.monotonic() - started) * 1000
        messages = [dict(message) for message in request['messages']]
        if request['use_json_grammar']:
            # The token mask constrains syntax. Describe numeric/semantic bounds
            # too; the complete schema is independently validated before success.
            instruction = '\nReturn only JSON conforming to this schema, including all bounds:\n' + (request['json_schema'] or '{"type":"object"}')
            if messages and messages[0]['role'] == 'system':
                messages[0]['content'] += instruction
            else:
                messages.insert(0, {'role': 'system', 'content': instruction})
        prompt = self.tokenizer.apply_chat_template(
            messages, tokenize=False, add_generation_prompt=True, enable_thinking=False)
        tokens = self.tokenizer.encode(prompt, add_special_tokens=False)
        metadata = dict(prompt_text=prompt, prompt_tokens=len(tokens),
                        model_reused=reused, load_time_ms=load_ms)
        emit(dict(type='ready', **metadata))
        if request['count_only']:
            emit(dict(type='done', success=True, stop_reason='count', **metadata))
            return
        if len(tokens) + request['n_predict'] + 1 > request['n_ctx']:
            raise WorkerError('context_limit', 'Prompt and reserved output exceed fixed context budget')
        processors = None
        schema = None
        if request['use_json_grammar']:
            from outlines_core import Index, Vocabulary, outlines_core
            from outlines.backends.outlines_core import OutlinesCoreLogitsProcessor
            schema_text = request['json_schema'] or '{"type":"object"}'
            schema = json.loads(schema_text)
            Draft202012Validator.check_schema(schema)
            if self.vocabulary is None:
                self.vocabulary = Vocabulary(self.tokenizer.eos_token_id, token_vocabulary(self.tokenizer))
            if schema_text not in self.indexes:
                # Bound memory: retain at most eight compiled schemas, never mutable guide state.
                if len(self.indexes) >= 8:
                    self.indexes.clear()
                regex = outlines_core.json_schema.build_regex_from_schema(schema_text)
                self.indexes[schema_text] = Index(regex, self.vocabulary)
            processor = OutlinesCoreLogitsProcessor(self.indexes[schema_text], 'mlx')
            # Outlines' Metal bitmask kernel mixes -INFINITY with logits. Use
            # float32 to avoid its bfloat16 compilation failure on macOS 14.
            processors = [lambda ids, logits: processor(ids, logits.astype(mx.float32))]
        mx.random.seed(request['seed'])
        sampler = make_sampler(temp=request['temperature'], top_p=request['top_p'], top_k=request['top_k'])
        text = ''
        last = None
        inference_started = time.monotonic()
        for response in stream_generate(self.model, self.tokenizer, tokens,
                                        max_tokens=request['n_predict'], sampler=sampler,
                                        logits_processors=processors,
                                        prefill_step_size=request['n_batch']):
            last = response
            text += response.text
            emit(dict(type='delta', text=response.text, generated_tokens=response.generation_tokens))
        if last is None or last.finish_reason != 'stop':
            raise WorkerError('token_limit', 'Total generation budget exhausted before EOS')
        if schema is not None:
            try:
                value = json.loads(text)
                if not isinstance(value, dict):
                    raise ValueError('Final JSON is not an object')
                Draft202012Validator(schema).validate(value)
            except Exception as error:
                raise WorkerError('invalid_final_json', str(error)) from error
        emit(dict(type='done', success=True, response_text=text, reached_eog=True,
                  generated_tokens=last.generation_tokens, stop_reason='eos',
                  inference_time_ms=(time.monotonic()-inference_started)*1000,
                  prompt_time_ms=1000*last.prompt_tokens/max(last.prompt_tps, 1e-9),
                  generation_time_ms=1000*last.generation_tokens/max(last.generation_tps, 1e-9),
                  **metadata))


def serve(source, destination, runtime=None):
    runtime = runtime or Runtime()
    def emit(value):
        destination.write(json.dumps(value, ensure_ascii=False) + '\n')
        destination.flush()
    while True:
        line = source.readline(MAX_REQUEST_BYTES + 1)
        if not line:
            return
        if len(line.encode('utf-8')) > MAX_REQUEST_BYTES or not line.endswith('\n'):
            emit(dict(type='error', stop_reason='protocol_error', error_message='Request too large or incomplete'))
            return
        try:
            request = json.loads(line)
            with contextlib.redirect_stdout(sys.stderr):
                runtime.handle(request, emit)
        except Exception as error:
            emit(dict(type='error', stop_reason=getattr(error, 'reason', 'error'), error_message=str(error)))


if __name__ == '__main__':
    serve(sys.stdin, sys.stdout)
