"""Real MLX smoke probes through the C++ executable (requires installed weights)."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT / 'build_macos/liminal_cornell_renderer')
    parser.add_argument('--out', type=Path, required=True, help='New evidence directory outside Git')
    args = parser.parse_args()
    executable = args.exe.resolve()
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=False)
    prompt = output / 'prompt.json'
    schema = output / 'schema.json'
    prompt.write_text(json.dumps([{'role': 'user', 'content':
        'Return JSON with status ok and body_temperature_c 37.0.'}]))
    schema.write_text(json.dumps({'type': 'object', 'properties': {
        'status': {'type': 'string', 'enum': ['ok']},
        'body_temperature_c': {'type': 'number', 'minimum': 36.6, 'maximum': 37.4}},
        'required': ['status', 'body_temperature_c'], 'additionalProperties': False}))
    results = []
    cases = [
        ('reuse', ['--llm-probe-repeat', '2'], 'eos'),
        ('context', ['--llm-context', '128', '--llm-predict', '768'], 'context_limit'),
        ('timeout', ['--llm-timeout-ms', '1'], 'timeout'),
        ('cancel', ['--llm-cancel-after', '1'], 'cancelled'),
        ('budget', ['--llm-predict', '1'], 'token_limit'),
    ]
    for name, options, expected in cases:
        result = output / f'{name}.json'
        command = [str(executable), '--llm-probe', str(prompt), '--llm-probe-schema', str(schema),
                   '--llm-probe-output', str(result), *options]
        with (output / f'{name}.log').open('w') as log:
            process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=240, cwd=output)
        data = json.loads(result.read_text())
        records = data if isinstance(data, list) else [data]
        if process.returncode != (0 if expected == 'eos' else 2):
            raise RuntimeError(f'{name}: unexpected exit {process.returncode}; inspect {result}')
        for record in records:
            if record['stop_reason'] != expected or record['success'] != (expected == 'eos'):
                raise RuntimeError(f'{name}: unexpected result; inspect {result}')
            if record['stream_leaked_marker'] or record['thought']:
                raise RuntimeError(f'{name}: private channel leak')
            if expected == 'eos':
                value = json.loads(record['final'])
                if value['status'] != 'ok' or not 36.6 <= value['body_temperature_c'] <= 37.4:
                    raise RuntimeError(f'{name}: invalid structured output')
            elif record['final']:
                raise RuntimeError(f'{name}: failed response published')
        if name == 'reuse' and (records[0]['model_reused'] or not records[1]['model_reused']):
            raise RuntimeError('Resident model reuse was not observed')
        results.append({'case': name, 'stop_reason': expected, 'passed': True})
        print(f'{name}: passed', flush=True)
    (output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
