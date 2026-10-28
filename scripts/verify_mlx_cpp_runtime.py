"""Real native MLX contract checks; requires the pinned model and an Apple GPU."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import shutil
import queue
import threading
import time
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    exe = args.exe.resolve()
    if 'MLX C++ 0.32.4' not in subprocess.check_output([str(exe), '--llama-info'], text=True):
        parser.error('This suite requires the native MLX C++ binary')
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    prompt = out / 'prompt.json'
    prompt.write_text(json.dumps([{'role': 'user', 'content': 'Return exactly {"ok":false,"extra":123}.'}]))
    schema = out / 'constraint.json'
    schema.write_text(json.dumps({'type': 'object', 'properties': {'ok': {'const': True}},
                                 'required': ['ok'], 'additionalProperties': False}))
    manifest = json.loads((ROOT / 'scripts/models_mlx_cpp.json').read_text())
    corrupt = out / 'corrupt'
    corrupt.mkdir()
    for name in manifest['files']:
        (corrupt / name).write_bytes(b'broken')
    cases = [
        ('schema', [], 'eos'),
        ('resident-20', ['--llm-probe-repeat', '20'], 'eos'),
        ('missing-model', ['--model', str(out / 'missing')], 'model_missing'),
        ('corrupt-model', ['--model', str(corrupt)], 'model_corrupt'),
        ('token-limit', ['--llm-predict', '1'], 'token_limit'),
        ('cancel-before', ['--llm-cancel-after', '0'], 'cancelled'),
        ('cancel-load', ['--llm-cancel-during-load'], 'cancelled'),
        ('cancel-stream', ['--llm-cancel-after', '2'], 'cancelled'),
        ('timeout', ['--llm-timeout-ms', '1'], 'timeout'),
        ('context-limit', ['--llm-context', '128'], 'context_limit'),
        ('reject-llama-offload', ['--llm-gpu-layers', '0'], 'error'),
    ]
    results = []
    base = [str(exe), '--llm-probe', str(prompt), '--llm-probe-schema', str(schema), '--llm-predict', '128']
    for name, flags, expected in cases:
        target = out / (name + '.json')
        assert target not in (prompt, schema), 'Probe output must never overwrite an input'
        with (out / (name + '.log')).open('wb') as log:
            process = subprocess.run([*base, '--llm-probe-output', str(target), *flags],
                                     stdout=log, stderr=log, timeout=300, cwd=out)
        data = json.loads(target.read_text()) if target.exists() else {}
        rows = data if isinstance(data, list) else [data]
        passed = process.returncode == (0 if expected == 'eos' else 2)
        for i, row in enumerate(rows):
            passed &= row.get('stop_reason') == expected and not row.get('stream_leaked_marker', True)
            if expected == 'eos':
                passed &= row.get('success', False) and json.loads(row.get('final', '{}')) == {'ok': True}
                if name == 'resident-20':
                    passed &= row.get('model_reused') == bool(i) and (i == 0 or row.get('load_time_ms') == 0)
            else:
                passed &= not row.get('success', True) and row.get('final') == ''
        if name == 'resident-20':
            passed &= len(rows) == 20
        results.append({'test': name, 'passed': bool(passed), 'returncode': process.returncode,
                        'observed': [row.get('stop_reason') for row in rows]})
        print(name, bool(passed), flush=True)
        (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')

    # Isolated small-file copy; the large verified weights are read-only via a symlink.
    # Mutating a metadata file after the first response must invalidate resident reuse.
    fixture = out / 'mutation-model'
    fixture.mkdir()
    source = ROOT / manifest['relative_path']
    for name in manifest['files']:
        if name == 'model.safetensors':
            (fixture / name).symlink_to(source / name)
        else:
            shutil.copy2(source / name, fixture / name)
    target = out / 'mutation.json'
    with (out / 'mutation.log').open('w') as log:
        process = subprocess.Popen([*base, '--model', str(fixture), '--llm-probe-repeat', '20',
                                    '--llm-probe-output', str(target)], stdout=log, stderr=subprocess.PIPE,
                                   text=True, cwd=out)
        changed = False
        lines = queue.Queue()
        def read_stderr():
            for line in process.stderr:
                lines.put(line)
            lines.put(None)
        reader = threading.Thread(target=read_stderr, daemon=True)
        reader.start()
        deadline = time.monotonic() + 300
        try:
            while True:
                line = lines.get(timeout=max(0.001, deadline - time.monotonic()))
                if line is None:
                    break
                log.write(line)
                if not changed and line.startswith('LIMINAL_LLM_METRIC ') and '"success":true' in line:
                    with (fixture / 'config.json').open('a') as stream:
                        stream.write(' ')
                    changed = True
            process.wait(timeout=max(0.001, deadline - time.monotonic()))
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
    rows = json.loads(target.read_text()) if target.exists() else []
    passed = changed and process.returncode == 2 and isinstance(rows, list) and any(
        row.get('stop_reason') == 'model_corrupt' and not row.get('success') and row.get('final') == '' for row in rows)
    results.append({'test': 'resident-file-mutation', 'passed': bool(passed)})
    print('resident-file-mutation', bool(passed), flush=True)
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    return 0 if all(row['passed'] for row in results) else 1


if __name__ == '__main__':
    raise SystemExit(main())
