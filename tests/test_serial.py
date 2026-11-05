"""Production protocol and service on host; no device, Wi-Fi or Python packages."""
import json
import contextlib
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import types
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from hardware_test import Client, ProtocolError, rates
import hardware_test


class SerialTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary = ROOT/'.build/tests/serial'
        cls.binary.parent.mkdir(parents=True, exist_ok=True)
        library = Path(os.environ.get('BNO055_LIBRARY', str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        subprocess.run([shutil.which(os.environ.get('CXX', 'c++')), '-std=c++11',
                        '-Wall', '-Wextra', '-Werror', '-I', str(library),
                        '-I', str(ROOT/'firmware/Axio'), str(ROOT/'tests/serial_harness.cpp'),
                        '-o', str(cls.binary)], check=True, capture_output=True, text=True)

    def exchange(self, wire):
        p = subprocess.run([str(self.binary)], input=wire, capture_output=True, check=True)
        return [json.loads(line) for line in p.stdout.splitlines()]

    def test_commands_jsonl_and_reset(self):
        commands = ['PING', 'INFO', 'STATUS', 'METRICS', 'CONFIG GET', 'TARE', 'DIAG RESET', 'METRICS']
        rows = self.exchange(''.join('AXIO {} {}\r\n'.format(i,c) for i,c in enumerate(commands)).encode())
        self.assertEqual(len(rows),len(commands))
        self.assertTrue(all(r['ok'] and r['id'] == str(i) and r['axio'] == 1 for i,r in enumerate(rows)))
        self.assertTrue(rows[0]['data']['pong'])
        self.assertEqual(rows[2]['data']['imu'], 'ready')
        self.assertEqual(rows[3]['data']['acquired'], 1)
        self.assertIsNone(rows[3]['data']['udp_sent'])
        self.assertFalse(rows[4]['data']['editable'])
        self.assertNotIn('password', json.dumps(rows).lower())
        self.assertTrue(rows[5]['data']['tared'])
        self.assertEqual(rows[7]['data']['acquired'], 0)
        self.assertEqual(rows[7]['data']['tare_requests'], 0)

    def test_malformed_and_bounded_input(self):
        wire = (b'AXIO 1 UNKNOWN\nAXIO 2 CONFIG VALIDATE {bad}\n'
                b'AXIO 3 CONFIG APPLY {}\nAXIO 4 CONFIG VALIDATE {"x":[true,false,null,-2.5e+3,"a\\u0041"]}\n'
                b'AXIO 5 PING\x00\nAXIO 6 ' + b'x'*1100 + b'\nAXIO 7 PING\n'
                b'garbage\nAXIO bad! PING\nAXIO 01234567890123456 PING\n')
        rows = self.exchange(wire)
        self.assertEqual([r.get('error') for r in rows], ['unknown_command', 'invalid_json',
            'config_engine_unavailable', 'config_engine_unavailable', 'invalid_encoding',
            'line_too_long', None, 'invalid_request', 'invalid_request', 'invalid_request'])
        self.assertEqual(rows[5]['id'], '6')
        self.assertEqual(rows[6]['id'], '7')
        self.assertIsNone(rows[-1]['id'])

    def test_json_syntax_errors(self):
        invalid = ['[]','{"a":01}','{"a":NaN}','{"a":1,}', '{"a":}',
                   '{"a":"\\q"}','{} tail','{"a":1e}','{"a":.5}',
                   '{"a":' + '['*10+'0'+']'*10+'}', '{"a":true false}', '{"a":1']
        rows = self.exchange(''.join('AXIO j CONFIG VALIDATE '+x+'\n' for x in invalid).encode())
        self.assertEqual(len(rows), len(invalid))
        self.assertTrue(all(r['error']=='invalid_json' for r in rows))

    def test_timeout_rollover_backpressure_and_real_tare(self):
        subprocess.run([str(self.binary), '--self-test'], check=True, capture_output=True)

    def test_python_client_correlation_and_errors(self):
        class Port:
            def __init__(self, response):
                self.response = bytearray(response)
            def write(self, data):
                self.written = data
                return len(data)
            def read(self, n):
                result = self.response[:n]
                del self.response[:n]
                return bytes(result)
        port = Port(b'# log\nboot ROM\n'+b'x'*2100+b'\n'+
                    b'{"axio":1,"id":"old","ok":true,"data":{}}\n'+
                    b'{"axio":1,"id":"1","ok":true,"data":{"pong":true}}\n')
        self.assertEqual(Client(port,.1).request('PING'), {'pong':True})
        self.assertEqual(port.written,b'AXIO 1 PING\n')
        for response in (b'{bad}\n',b'{"axio":1,"id":"1","ok":false,"error":"imu_unavailable"}\n',
                         b'{"axio":1,"id":"1","ok":true,"data":[]}\n', b''):
            with self.subTest(response=response), self.assertRaises(ProtocolError):
                Client(Port(response),.001).request('PING')

    def test_rates_and_no_port_report(self):
        before = dict(window_us=1_000_000, acquired=10, snapshots=10, osc_attempts=10, read_errors=0)
        after = dict(window_us=3_000_000, acquired=210, snapshots=210, osc_attempts=210, read_errors=0)
        self.assertEqual(rates(before,after)['acquired_hz'],100)
        with self.assertRaises(ProtocolError):
            rates(after,before)
        run = subprocess.run([sys.executable,'-B',str(ROOT/'tools/hardware_test.py')],capture_output=True,text=True)
        self.assertEqual(run.returncode,2)
        report = json.loads(run.stdout)
        self.assertFalse(report['hardware_executed'])
        self.assertTrue(all(t['result']=='NOT RUN' for t in report['tests']))

    def test_hardware_runner_mutations_opt_in_and_failures(self):
        class Port:
            def __init__(self, *args, **kwargs):
                self.response = bytearray()
                self.commands = []
                self.metrics = 0
            def __enter__(self):
                return self
            def __exit__(self, *args):
                pass
            def write(self, wire):
                _, ident, command = wire.decode().strip().split(' ', 2)
                self.commands.append(command)
                if command == 'METRICS':
                    self.metrics += 1
                    data = dict(window_us=self.metrics*1_000_000, acquired=self.metrics*100,
                                snapshots=self.metrics*100, osc_attempts=self.metrics*100, read_errors=0)
                else:
                    data = dict(pong=True, imu='ready', sample_valid=True, wifi_connected=False)
                self.response.extend((json.dumps(dict(axio=1,id=ident,ok=True,data=data))+'\n').encode())
                return len(wire)
            def read(self, n):
                result=self.response[:n]
                del self.response[:n]
                return bytes(result)
        serial_module = types.ModuleType('serial')
        serial_module.SerialException = OSError
        tools_module = types.ModuleType('serial.tools')
        tools_module.list_ports = types.SimpleNamespace(comports=lambda: [])
        for options in ([], ['--tare', '--reset-metrics'], ['--require-wifi']):
            port = Port()
            serial_module.Serial = lambda *a, **kw: port
            with mock.patch.dict(sys.modules, {'serial':serial_module, 'serial.tools':tools_module}), \
                    mock.patch.object(hardware_test.time, 'sleep'), contextlib.redirect_stdout(io.StringIO()) as output:
                code = hardware_test.main(['--port','FAKE',*options])
            report = json.loads(output.getvalue())
            self.assertEqual(code, 1 if '--require-wifi' in options else 0)
            self.assertEqual('TARE' in port.commands, '--tare' in options)
            self.assertEqual('DIAG RESET' in port.commands, '--reset-metrics' in options)
            self.assertFalse(report['persistent_changes'])
            self.assertEqual(next(t for t in report['tests'] if t['name']=='counters')['result'], 'PASS')


if __name__ == '__main__':
    unittest.main()
