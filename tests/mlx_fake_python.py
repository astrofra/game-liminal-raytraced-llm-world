#!/usr/bin/env python3
"""Protocol peer for the C++ transport test; never imports MLX or loads weights."""
import json
import os
import sys
import time

for line in sys.stdin:
    request = json.loads(line)
    mode = request['messages'][0]['content']
    if mode == 'crash':
        os._exit(7)
    if mode == 'hang':
        time.sleep(60)
    metadata = dict(prompt_text='test prompt', prompt_tokens=3, model_reused=False, load_time_ms=1)
    print(json.dumps(dict(type='ready', **metadata)), flush=True)
    if request['count_only']:
        print(json.dumps(dict(type='done', **metadata)), flush=True)
        continue
    text = '<think>private' if mode == 'private' else '{}'
    print(json.dumps(dict(type='delta', text=text, generated_tokens=1)), flush=True)
    if mode == 'hang-after-token':
        time.sleep(60)
    print(json.dumps(dict(type='done', response_text=text, reached_eog=True,
                         inference_time_ms=2, prompt_time_ms=1, generation_time_ms=1,
                         **metadata)), flush=True)
