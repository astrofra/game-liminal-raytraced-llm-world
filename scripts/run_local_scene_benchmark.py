"""Fixed-frame Eryx composition through local Ollama and the native raytracer.

One complete repair, shared deadline, sequential models, retained failed trials.
The existing llama-cli benchmark remains available for historical comparisons.
"""
from __future__ import annotations

import argparse
import hashlib
import http.client
import json
import pathlib
import re
import subprocess
import time
from urllib.parse import urlsplit

ROOT = pathlib.Path(__file__).resolve().parents[1]
CONTRACT = json.loads((ROOT/'assets/scene_generation/local_v1.json').read_text(encoding='utf-8'))
HEADER = CONTRACT['header']
SYSTEM = CONTRACT['system']


def prompt_for(case):
    return CONTRACT['prompt_template'].format(brief=case['brief'], landmark=case['landmark'])


def write_json(path, data):
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False, allow_nan=False), encoding='utf-8')


class Ollama:
    def __init__(self, endpoint):
        url = urlsplit(endpoint)
        if url.scheme != 'http' or url.hostname not in ('localhost', '127.0.0.1', '::1') or url.path not in ('', '/'):
            raise ValueError('Only a local HTTP Ollama endpoint is supported')
        self.host, self.port = url.hostname, url.port or 11434

    def request(self, route, body=None, deadline=None):
        deadline = deadline or time.perf_counter() + 30
        remaining = deadline - time.perf_counter()
        if remaining <= 0:
            raise TimeoutError('Shared scene deadline exhausted')
        connection = http.client.HTTPConnection(self.host, self.port, timeout=remaining)
        response = None
        try:
            connection.request('GET' if body is None else 'POST', route,
                               None if body is None else json.dumps(body).encode(),
                               {'Content-Type': 'application/json'})
            transport = connection.sock
            response = connection.getresponse()
            chunks, size = [], 0
            while True:
                remaining = deadline - time.perf_counter()
                if remaining <= 0:
                    raise TimeoutError('Shared scene deadline exhausted')
                # read1 returns after one underlying read, so a streaming response
                # cannot reset an unlimited per-line timeout.
                if transport:
                    transport.settimeout(remaining)
                chunk = response.read1(65536)
                if not chunk:
                    break
                size += len(chunk)
                if size > 2_000_000:
                    raise ValueError('Ollama response exceeds 2 MB limit')
                chunks.append(chunk)
            text = b''.join(chunks).decode('utf-8')
            if response.status != 200:
                raise RuntimeError(f'Ollama HTTP {response.status}: {text[:400]}')
            if body and body.get('stream'):
                events = [json.loads(line) for line in text.splitlines() if line.strip()]
                if not events or not events[-1].get('done'):
                    raise ValueError('Incomplete Ollama stream')
                if any('error' in event for event in events):
                    raise RuntimeError('Ollama stream error')
                result = events[-1].copy()
                result['message'] = {'content': ''.join(e.get('message', {}).get('content', '') for e in events)}
                return result
            return json.loads(text)
        finally:
            if response is not None:
                response.close()
            connection.close()  # Also cancels the local request on Ctrl+C / timeout.

    def unload(self, model):
        return self.request('/api/generate', {'model': model, 'keep_alive': 0, 'stream': False}, time.perf_counter()+20)


def completion_metrics(response, wall_ms):
    result = {'wall_ms': wall_ms, 'done_reason': response.get('done_reason')}
    for source, target in [('load_duration', 'load_ms'), ('prompt_eval_duration', 'prompt_ms'),
                           ('eval_duration', 'generation_ms'), ('total_duration', 'server_ms')]:
        result[target] = response[source]/1e6 if source in response else None
    result['eval_count'] = response.get('eval_count')
    result['prompt_eval_count'] = response.get('prompt_eval_count')
    result['tokens_per_second'] = (response['eval_count']*1e9/response['eval_duration']
        if response.get('eval_duration', 0)>0 and 'eval_count' in response else None)
    return result


def validate_contract(text, done_reason):
    issues = []
    if done_reason != 'stop':
        issues.append(f'Incomplete generation: done_reason={done_reason}; provide a shorter COMPLETE program')
    lines = [line.strip() for line in text.splitlines() if line.strip()]
    fixed = HEADER.strip().splitlines()
    if lines[:len(fixed)] != fixed:
        issues.append('Fixed header changed or missing; copy it exactly, without Markdown')
    body = lines[len(fixed):]
    if not 8 <= len(body) <= 20:
        issues.append(f'Expected 8..20 boxes, observed {len(body)} body lines')
    for line in body:
        if not re.match(r'^box "[a-z][a-z0-9_]{0,63}" ', line) or 'emit' in line:
            issues.append('Only named box directives without emit are allowed after the header')
            break
    # Numeric/syntax/rotation/complexity checks use the actual native parser.
    return issues


def unwrap_program(text):
    """Same transport allowance as dynamic_scene.cpp; never extract from prose."""
    stripped = text.strip()
    match = re.fullmatch(r'```(?:scene|\.scene)?\r?\n([\s\S]*\n)```', stripped)
    if match and '```' not in match[1]:
        return match[1], True
    return text, False


def audit_render(text, case, directory, attempt, options, deadline):
    prefix = directory / f'attempt-{attempt}'
    scene = prefix.with_suffix('.scene')
    scene.write_text(text, encoding='utf-8', newline='\n')
    image = prefix.with_suffix('.png')
    remaining = deadline-time.perf_counter()
    if remaining <= 0:
        raise TimeoutError('Shared scene deadline exhausted before native audit')
    command = [str(options.renderer), '--audit-scene-view', str(scene), '--view-landmark', case['landmark'],
               '--scene-artifacts', str(prefix), '--output', str(image), '--width', str(options.width),
               '--height', str(options.height), '--samples', str(options.samples), '--bounces', '3',
               '--direct-samples', '1', '--seed', '77']
    start = time.perf_counter()
    process = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, encoding='utf-8',
                             errors='replace', timeout=remaining)
    wall_ms = (time.perf_counter()-start)*1000
    prefix.with_suffix('.native.txt').write_text(process.stdout+'\n'+process.stderr, encoding='utf-8')
    audit_path = prefix.with_suffix('.audit.json')
    audit = json.loads(audit_path.read_text()) if audit_path.exists() else {}
    issues = [f"{d['code']} {d['object_id']}: {d['detail']}" for d in audit.get('diagnostics', [])]
    if process.returncode or not image.exists() or not audit.get('accepted'):
        issues.append(f'Native audit/render rejected (exit {process.returncode})')
    render = re.search(r'Render time:\s*([0-9.]+)\s*ms', process.stdout)
    return issues, {'audit': audit, 'audit_render_ms': wall_ms,
                    'render_ms': float(render[1]) if render else None, 'image_path': str(image) if image.exists() else None}


def run_trial(client, model, case, trial, options, directory, thinking, initial_scene=None):
    directory.mkdir(parents=True)
    started = time.perf_counter()
    deadline = started + options.deadline
    record = {'model': model, 'case_id': case['id'], 'trial': trial, 'accepted': False,
              'first_pass': False, 'calls': [], 'attempts': [], 'error': None,
              'render_ms': None, 'image_path': None, 'source': 'injected-defect' if initial_scene else 'generated'}
    messages = [{'role': 'system', 'content': SYSTEM}, {'role': 'user', 'content': prompt_for(case)}]
    try:
        for attempt in range(2):
            if attempt == 0 and initial_scene is not None:
                text, reason = initial_scene, 'stop'
            else:
                payload = {'model': model, 'messages': messages, 'stream': True, 'keep_alive': '10m',
                           'options': {'num_ctx': 8192, 'num_predict': options.tokens, 'temperature': .2,
                                       'seed': 42+trial, 'top_k': 40, 'top_p': .95}}
                if thinking:
                    payload['think'] = False
                write_json(directory/f'attempt-{attempt}.request.json', payload)
                call_start = time.perf_counter()
                try:
                    response = client.request('/api/chat', payload, deadline)
                except (Exception, KeyboardInterrupt) as error:
                    record['calls'].append({'wall_ms': (time.perf_counter()-call_start)*1000,
                                            'error': type(error).__name__})
                    raise
                record['calls'].append(completion_metrics(response, (time.perf_counter()-call_start)*1000))
                write_json(directory/f'attempt-{attempt}.response.json', response)
                text, reason = response.get('message', {}).get('content', ''), response.get('done_reason')
            text, unwrapped = unwrap_program(text)
            (directory/f'attempt-{attempt}.scene').write_text(text, encoding='utf-8', newline='\n')
            issues = validate_contract(text, reason)
            native = {}
            if not issues:
                issues, native = audit_render(text, case, directory, attempt, options, deadline)
            record['attempts'].append({'issues': issues, 'transport_wrapper_removed': unwrapped, **native})
            if not issues and time.perf_counter() <= deadline:
                record.update(accepted=True, first_pass=attempt==0, error=None,
                              render_ms=native['render_ms'], image_path=native['image_path'],
                              scene_sha256=hashlib.sha256(text.encode()).hexdigest())
                break
            if not issues:
                issues = ['Shared scene deadline exhausted after render']
            record['error'] = '; '.join(issues)
            messages += [{'role': 'assistant', 'content': text}, {'role': 'user', 'content':
                CONTRACT['repair_prefix']+'\n'.join(issues)}]
    except KeyboardInterrupt:
        record['error'] = 'Cancelled by user';record['cancelled'] = True
        raise
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired, http.client.HTTPException) as error:
        record['error'] = f'{type(error).__name__}: {error}'
    finally:
        record['total_ms'] = (time.perf_counter()-started)*1000
        write_json(directory/'result.json', record)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--models', nargs='+', default=['gemma4:e4b', 'eryx-ministral8b:q4'])
    parser.add_argument('--endpoint', default='http://127.0.0.1:11434')
    parser.add_argument('--cases', type=pathlib.Path, default=ROOT/'scripts/eryx_local_scene_cases.json')
    parser.add_argument('--renderer', type=pathlib.Path, default=ROOT/'build/Release/liminal_cornell_renderer.exe')
    parser.add_argument('--out', type=pathlib.Path, required=True, help='New directory, never overwrites a campaign')
    parser.add_argument('--trials', type=int, default=2)
    parser.add_argument('--tokens', type=int, default=2048)
    parser.add_argument('--deadline', type=float, default=120, help='Shared generation/repair/audit/render seconds')
    parser.add_argument('--width', type=int, default=800)
    parser.add_argument('--height', type=int, default=400)
    parser.add_argument('--samples', type=int, default=8)
    parser.add_argument('--case', help='Run one case ID (smoke test only)')
    parser.add_argument('--repair-scene', type=pathlib.Path, help='Inject a defective scene; separate from normal trials')
    args = parser.parse_args()
    if any(v<=0 for v in (args.trials,args.tokens,args.deadline,args.width,args.height,args.samples)):
        parser.error('Budgets and dimensions must be positive')
    if len(set(args.models)) != len(args.models) or any(':cloud' in m or m.endswith('-cloud') for m in args.models):
        parser.error('Use distinct local models only')
    if not args.renderer.is_file():
        parser.error('Native renderer is missing; build it first')
    cases = json.loads(args.cases.read_text(encoding='utf-8'))
    if args.case:
        cases = [c for c in cases if c['id'] == args.case]
    if not cases or any(not re.fullmatch('[a-z][a-z0-9_]*', c['id']) for c in cases):
        parser.error('Missing or invalid case IDs')
    if len({c['id'] for c in cases}) != len(cases):
        parser.error('Duplicate case IDs')
    initial = args.repair_scene.read_text(encoding='utf-8') if args.repair_scene else None
    client = Ollama(args.endpoint)
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    records, metadata = [], {'models': {}, 'options': {k:str(v) if isinstance(v,pathlib.Path) else v for k,v in vars(args).items()},
        'version': client.request('/api/version'), 'tags': client.request('/api/tags'),
        'renderer_sha256': hashlib.sha256(args.renderer.read_bytes()).hexdigest(),
        'cases_sha256': hashlib.sha256(args.cases.read_bytes()).hexdigest()}
    active = client.request('/api/ps')['models']
    unrelated = [m['name'] for m in active if m['name'] not in args.models and m.get('size_vram',0)>0]
    if unrelated:
        raise RuntimeError(f'Other resident models would invalidate the comparison: {unrelated}')
    from scene_local_report import write_report
    active_model = None
    try:
        for index, model in enumerate(args.models):
            for resident in client.request('/api/ps')['models']:
                if resident['name'] in args.models:
                    client.unload(resident['name'])
            info = client.request('/api/show', {'model': model})
            if info.get('remote_host') or info.get('remote_model'):
                raise ValueError('Remote model is outside this local comparison')
            thinking = 'thinking' in info.get('capabilities', [])
            active_model = model
            start = time.perf_counter()
            cold = client.request('/api/generate', {'model':model,'stream':False,'keep_alive':'10m',
                'options':{'num_ctx':8192}}, time.perf_counter()+180)
            cold_ms = (time.perf_counter()-start)*1000
            warm_payload = {'model': model, 'messages':[{'role':'user','content':'Reply READY.'}],
                'stream':True,'keep_alive':'10m','options':{'num_ctx':8192,'num_predict':8,'temperature':0}}
            if thinking:
                warm_payload['think']=False
            warm = client.request('/api/chat', warm_payload, time.perf_counter()+180)
            metadata['models'][model] = {'show':info, 'cold':cold,'cold_wall_ms':cold_ms, 'warm':warm,
                'residency':client.request('/api/ps'), 'thinking_requested':False if thinking else 'unsupported'}
            write_json(args.out/'environment.json',metadata)
            print(f'{model}: loaded in {cold_ms/1000:.2f}s; starting {len(cases)*args.trials} trials',flush=True)
            try:
                for case in cases:
                    for trial in range(args.trials):
                        directory = args.out/f'model-{index}'/f"{case['id']}-{trial}"
                        try:
                            record = run_trial(client,model,case,trial,args,directory,thinking,initial)
                        except KeyboardInterrupt:
                            if (directory/'result.json').exists():
                                records.append(json.loads((directory/'result.json').read_text(encoding='utf-8')))
                            raise
                        records.append(record)
                        write_json(args.out/'results.json',records)
                        write_report(records,args.out/'REPORT.md')
                        print(f"{model} {case['id']} #{trial}: {'accepted' if record['accepted'] else 'REJECTED'} "
                              f"{record['total_ms']/1000:.2f}s, calls={len(record['calls'])} {record['error'] or ''}",flush=True)
            finally:
                client.unload(model)
                active_model = None
    except KeyboardInterrupt:
        print('Cancelled; completed and interrupted trial records retained.',flush=True)
        return 130
    finally:
        if active_model is not None:
            try:
                client.unload(active_model)
            except (OSError, RuntimeError, ValueError, http.client.HTTPException) as error:
                metadata['unload_error'] = str(error)
        write_json(args.out/'environment.json',metadata)
        write_json(args.out/'results.json',records)
        write_report(records,args.out/'REPORT.md')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
