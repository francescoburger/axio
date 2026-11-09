"""Unattended collector: counter resets, false PASS prevention and worker errors."""
import json
from pathlib import Path
import struct
import sys
import unittest
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from endurance_test import Statistics, outcome, window, sample_worker, udp_worker


class Stop:
    def __init__(self, remaining=3):
        self.remaining = remaining
    def is_set(self):
        return self.remaining <= 0
    def wait(self, seconds):
        self.remaining -= 1


class EnduranceTests(unittest.TestCase):
    def metrics(self, seconds, samples, queued, errors=0):
        return dict(window_us=seconds * 1000000, acquired=samples, snapshots=samples,
                    osc_attempts=samples, read_errors=0,
                    engine=dict(udp_queued=queued, udp_errors=errors))

    def test_counter_deltas_are_separate_and_resets_are_rejected(self):
        before, after = self.metrics(10, 1000, 500, 7), self.metrics(20, 1998, 1470, 7)
        result = window(before, after)
        self.assertAlmostEqual(result['acquired_hz'], 99.8)
        self.assertEqual(result['udp_queued_delta'], 970)
        self.assertEqual(result['udp_errors_delta'], 0)
        for changed in (self.metrics(5, 500, 200), self.metrics(20, 1998, 300, 7)):
            with self.assertRaises(RuntimeError if changed['window_us'] < before['window_us'] else ValueError):
                window(before, changed)

    def test_bounded_memory_and_no_false_pass(self):
        stats = Statistics()
        stats.add('http_valid', 100)
        stats.add('osc_valid_packets', 1000)
        self.assertEqual(outcome(stats.snapshot(), True, True)[0], 'PASS')
        self.assertEqual(outcome(stats.snapshot(), False, True)[0], 'NOT RUN')
        self.assertEqual(outcome(stats.snapshot(), True, False)[0], 'FAIL')
        for value in range(1000):
            stats.observe('heap', value)
            stats.error('usb', value)
        result = stats.snapshot()
        self.assertEqual(result['ranges']['heap'], (0, 999))
        self.assertEqual(len(result['errors']), 10)
        self.assertEqual(result['counts']['error_usb'], 1000)
        self.assertEqual(outcome(result, True, True)[0], 'FAIL')

    def test_cached_web_samples_allowed_but_regression_is_reported(self):
        samples = iter([(10, 100), (10, 100), (9, 90)])
        connections = []
        class Connection:
            status = 200
            def __init__(self, *args, **kwargs):
                self.closed = False
                connections.append(self)
            def request(self, method, path, headers):
                self.path = path
                self.method = method
            def getresponse(self):
                return self
            def read(self):
                sequence, timestamp = next(samples)
                return json.dumps(dict(valid=True, sequence=sequence, timestamp_us=timestamp)).encode()
            def close(self):
                self.closed = True
        stats = Statistics()
        with mock.patch('endurance_test.http.client.HTTPConnection', Connection):
            sample_worker('192.168.1.15', Stop(), stats)
        counts = stats.snapshot()['counts']
        self.assertEqual(counts['http_valid'], 2)
        self.assertEqual(counts['error_http'], 1)
        self.assertTrue(all(c.closed and c.path == '/api/sample' and c.method == 'GET' for c in connections))

    def test_osc_source_filter_and_malformed_packet(self):
        stop = Stop(1)
        valid = b'/x\0\0,f\0\0' + struct.pack('>f', 1)
        packets = [(valid, ('192.168.1.99', 1)), (b'bad!', ('192.168.1.15', 1)),
                   (valid, ('192.168.1.15', 1))]
        class Socket:
            def recvfrom(self, size):
                packet = packets.pop(0)
                if not packets:
                    stop.remaining = 0
                return packet
        stats = Statistics()
        udp_worker(Socket(), '192.168.1.15', stop, stats)
        counts = stats.snapshot()['counts']
        self.assertEqual(counts['udp_other_sources'], 1)
        self.assertEqual(counts['udp_packets'], 2)
        self.assertEqual(counts['osc_valid_packets'], 1)
        self.assertEqual(counts['error_osc_format'], 1)

    def test_all_errors_reach_sink_beyond_summary_limit(self):
        events = []
        stats = Statistics(events.append)
        for index in range(25):
            stats.error('http', {'request': index, 'stage': 'headers'})
        self.assertEqual(len(stats.snapshot()['errors']), 10)
        self.assertEqual(len(events), 25)
        self.assertEqual(events[-1]['detail']['request'], 24)

    def test_trace_identifies_header_timeout_after_successful_connect(self):
        traces, errors = [], []
        class Connection:
            closed = False
            def __init__(self, *args, **kwargs):
                pass
            def connect(self):
                pass
            def request(self, *args, **kwargs):
                pass
            def getresponse(self):
                raise TimeoutError('headers timed out')
            def close(self):
                Connection.closed = True
        stats = Statistics(errors.append)
        with mock.patch('endurance_test.http.client.HTTPConnection', Connection):
            sample_worker('192.168.1.15', Stop(1), stats, traces.append)
        self.assertEqual(len(traces), 1)
        self.assertEqual(traces[0]['error_stage'], 'response_headers')
        self.assertIn('connected_seconds', traces[0])
        self.assertNotIn('headers_seconds', traces[0])
        self.assertEqual(errors[0]['detail']['id'], 1)
        self.assertEqual(stats.snapshot()['counts']['error_http'], 1)
        self.assertTrue(Connection.closed)


if __name__ == '__main__':
    unittest.main()
