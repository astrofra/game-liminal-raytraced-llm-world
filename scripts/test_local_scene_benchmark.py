"""Failure-path and repair-budget checks without loading model weights."""
import json
from pathlib import Path
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

import run_local_scene_benchmark as bench

VALID = bench.HEADER + ''.join(f'box "mass_{i}" pos({i},2,8) size(1,4,1) gray(0.4)\n' for i in range(8))
CASE = {'id':'test_case','brief':'Frame a distant marker.','landmark':'mass_0'}
NATIVE = {'audit':{'accepted':True},'render_ms':1.0,'image_path':'observed.png'}


def response(text=VALID, reason='stop'):
    return {'message':{'content':text},'done_reason':reason,'eval_count':100,'eval_duration':1_000_000_000}


class TrialTests(unittest.TestCase):
    def trial(self, replies, audits, budget=5):
        client = Mock()
        client.request.side_effect = replies
        options = SimpleNamespace(deadline=budget,tokens=2048)
        with tempfile.TemporaryDirectory() as temp, patch.object(bench,'audit_render',side_effect=audits) as audit:
            result = bench.run_trial(client,'test-local',CASE,0,options,Path(temp)/'trial',True)
            saved=json.loads((Path(temp)/'trial/result.json').read_text())
        self.assertEqual(result,saved)
        return result,client,audit

    def test_complete_program_reaches_native_validator(self):
        r,c,a=self.trial([response()], [([],NATIVE)])
        self.assertTrue(r['first_pass'])
        self.assertTrue(r['accepted'])
        self.assertEqual(a.call_count,1)

    def test_truncated_valid_prefix_is_never_rendered(self):
        r,c,a=self.trial([response(reason='length'),response(reason='length')],[])
        self.assertFalse(r['accepted'])
        self.assertEqual(c.request.call_count,2)
        self.assertEqual(a.call_count,0)

    def test_one_correction_receives_measured_diagnostic_and_same_deadline(self):
        r,c,a=self.trial([response(),response()],[(['foreground_occlusion block_4: 98%'],{}),([],NATIVE)])
        self.assertTrue(r['accepted'])
        self.assertFalse(r['first_pass'])
        first,second=c.request.call_args_list
        self.assertEqual(first.args[2],second.args[2])
        self.assertIn('block_4: 98%',second.args[1]['messages'][-1]['content'])
        self.assertFalse(second.args[1]['think'])

    def test_budget_failure_is_retained_without_retry(self):
        r,c,a=self.trial([TimeoutError('deadline')],[])
        self.assertFalse(r['accepted'])
        self.assertEqual(c.request.call_count,1)
        self.assertEqual(len(r['calls']),1)
        self.assertNotIn('generation_ms',r['calls'][0])

    def test_second_failure_is_final(self):
        r,c,a=self.trial([response(),response()],[(['missing landmark'],{}),(['camera inside'],{})])
        self.assertFalse(r['accepted'])
        self.assertEqual(c.request.call_count,2)
        self.assertEqual(r['error'],'camera inside')

    def test_fixed_camera_cannot_be_changed(self):
        issues=bench.validate_contract(VALID.replace('eye(0,1.7,-10)','eye(0,2,-10)'),'stop')
        self.assertTrue(issues)
        self.assertTrue(bench.validate_contract(VALID+'prefab_gate "extra"\n','stop'))

    def test_single_transport_wrapper_does_not_change_geometry(self):
        self.assertEqual(bench.unwrap_program('```scene\n'+VALID+'```'),(VALID,True))
        prose='Here it is:\n```scene\n'+VALID+'```'
        self.assertEqual(bench.unwrap_program(prose),(prose,False))
        r,c,a=self.trial([response('```scene\n'+VALID+'```',reason='length')]*2,[])
        self.assertFalse(r['accepted'])
        self.assertEqual(a.call_count,0)

    def test_cancelled_trial_is_saved(self):
        client=Mock();client.request.side_effect=KeyboardInterrupt
        with tempfile.TemporaryDirectory() as temp:
            directory=Path(temp)/'trial'
            with self.assertRaises(KeyboardInterrupt):
                bench.run_trial(client,'test',CASE,0,SimpleNamespace(deadline=5,tokens=20),directory,False)
            record=json.loads((directory/'result.json').read_text())
            self.assertTrue(record['cancelled'])
            self.assertFalse(record['accepted'])

    def test_metrics_convert_nanoseconds_without_inventing_missing_values(self):
        m=bench.completion_metrics(response(),1500)
        self.assertEqual(m['tokens_per_second'],100)
        self.assertEqual(m['generation_ms'],1000)
        self.assertIsNone(m['load_ms'])

    def test_nonlocal_endpoint_rejected(self):
        with self.assertRaises(ValueError):
            bench.Ollama('https://example.com')

    def test_streaming_cannot_extend_shared_wall_deadline(self):
        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass
            def do_POST(self):
                self.rfile.read(int(self.headers['Content-Length']))
                self.send_response(200);self.end_headers()
                try:
                    for _ in range(30):
                        self.wfile.write(b'{"message":{"content":"partial"},"done":false}\n')
                        self.wfile.flush();time.sleep(.02)
                except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                    pass
        server=HTTPServer(('127.0.0.1',0),Handler)
        worker=threading.Thread(target=server.serve_forever,daemon=True);worker.start()
        start=time.perf_counter()
        try:
            client=bench.Ollama(f'http://127.0.0.1:{server.server_port}')
            with self.assertRaises(TimeoutError):
                client.request('/api/chat',{'stream':True},start+.12)
            self.assertLess(time.perf_counter()-start,.5)
        finally:
            server.shutdown();server.server_close();worker.join()


if __name__=='__main__':
    unittest.main()
