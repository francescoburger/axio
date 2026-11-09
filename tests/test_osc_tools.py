import json
from pathlib import Path
import select
import socket
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from osc_receiver import decode_packet, OSCError
from analyze_capture import summarize


def string(s):
    b = s.encode() + b'\0'
    return b + b'\0' * (-len(b) % 4)


def message(address='/ypr0'):
    return string(address) + string(',fff') + struct.pack('>fff', 0, .5, -1)


def bundle(*elements):
    return b'#bundle\0' + struct.pack('>Q', 1) + b''.join(struct.pack('>i', len(e)) + e for e in elements)


class OSCReceiverTests(unittest.TestCase):
    def test_baseline_bundle(self):
        addresses = ['/ypr0','/real0','/world0','/gyro0']
        result = decode_packet(bundle(*(message(a) for a in addresses)))
        self.assertEqual([m['address'] for m in result], addresses)
        for m in result:
            self.assertEqual(m['types'], ',fff')
            self.assertEqual(m['values'], [0,.5,-1])
            self.assertEqual(m['bundle_timetags'], [1])

    def test_nested_and_other_types(self):
        self.assertEqual(decode_packet(bundle(bundle(message())))[0]['bundle_timetags'], [1,1])
        data = string('/test') + string(',isTFNIbhd') + struct.pack('>i', -12) + string('abc')
        data += struct.pack('>i', 2) + b'ab\0\0' + struct.pack('>qd', 123, .25)
        self.assertEqual(decode_packet(data)[0]['values'], [-12,'abc',True,False,None,'impulse',{'base64':'YWI='},123,.25])

    def test_malformed(self):
        for data in (b'',b'abcd',message()[:-4],message()+b'\0'*4,
                     b'#bundle\0'+struct.pack('>Qi',1,-4),
                     string('/x')+string(',u'),
                     string('/x')+string(',b')+struct.pack('>i', -1)):
            with self.subTest(data=data), self.assertRaises(OSCError):
                decode_packet(data)
        data = message()
        for _ in range(18):
            data = bundle(data)
        with self.assertRaises(OSCError):
            decode_packet(data)

    def test_packet_rate_and_jitter(self):
        rows = [dict(source_ip='127.0.0.1',source_port=16889,received_monotonic_ns=t,
                     messages=[{}, {}, {}, {}]) for t in (0,10_000_000,30_000_000)]
        result = summarize(rows)[0]
        self.assertAlmostEqual(result['received_udp_hz'], 2/.03)
        self.assertAlmostEqual(result['received_osc_hz'], 2/.03)
        self.assertEqual(result['osc_messages'],12)
        self.assertEqual(result['jitter_population_stddev_ms'],5)
        self.assertIsNone(result['imu_hz'])
        self.assertIsNone(summarize(rows[:1])[0]['received_udp_hz'])
        invalid = dict(rows[-1])
        del invalid['messages']
        invalid['error'] = 'synthetic invalid packet'
        mixed = summarize(rows[:2] + [invalid])[0]
        self.assertEqual(mixed['received_osc_hz'],100)
        self.assertEqual(mixed['invalid_packets'],1)

    def test_udp_loopback(self):
        # Porta effimera e directory temporanea soltanto nel repository.
        folder = ROOT/'.build/tests'
        folder.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=folder) as directory:
            output = Path(directory)/'capture.jsonl'
            proc = subprocess.Popen([sys.executable,'-B',str(ROOT/'tools/osc_receiver.py'),
                                     '--bind','127.0.0.1','--port','0','--duration','1',
                                     '--output',str(output)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            try:
                ready, _, _ = select.select([proc.stdout],[],[],5)
                self.assertTrue(ready, 'Receiver not ready within 5 seconds')
                line = proc.stdout.readline().strip()
                if not line.startswith('Listening on '):
                    _, err = proc.communicate(timeout=5)
                    self.fail('Receiver did not start: '+err)
                port = int(line.rsplit(':',1)[1])
                with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sock:
                    sock.sendto(bundle(message()),('127.0.0.1',port))
                    sock.sendto(b'bad!',('127.0.0.1',port))
                _, err = proc.communicate(timeout=5)
                self.assertEqual(proc.returncode,0,err)
                rows = [json.loads(line) for line in output.read_text().splitlines()]
                self.assertEqual(len(rows),2)
                self.assertEqual(rows[0]['messages'][0]['types'],',fff')
                self.assertIn('error',rows[1])
                self.assertGreater(rows[0]['received_unix_ns'],0)
                self.assertLessEqual(rows[0]['received_monotonic_ns'],rows[1]['received_monotonic_ns'])
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate()


if __name__ == '__main__':
    unittest.main()
