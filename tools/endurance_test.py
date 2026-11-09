#!/usr/bin/env python3
"""Autonomous USB/HTTP/OSC endurance test. Read-only; never invokes Codex."""
import argparse
from collections import Counter
import datetime
import fcntl
import http.client
import json
import math
import os
from pathlib import Path
import signal
import socket
import threading
import time

from entry_point_probe import query, http_snapshot
from hardware_test import Client, rates
from osc_receiver import decode_packet


def utc():
    return datetime.datetime.now(datetime.timezone.utc)


def atomic_json(path, value):
    temporary = path.with_suffix('.tmp')
    temporary.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')
    temporary.replace(path)


class Statistics:
    """Bounded memory: aggregate counts/ranges, at most ten error examples."""
    def __init__(self, error_sink=None):
        self.lock = threading.Lock()
        self.counts = Counter()
        self.ranges = {}
        self.examples = []
        self.error_sink = error_sink

    def add(self, key, value=1):
        with self.lock:
            self.counts[key] += value

    def observe(self, key, value):
        if not math.isfinite(value):
            raise ValueError('Non-finite observation: ' + key)
        with self.lock:
            low, high = self.ranges.get(key, (value, value))
            self.ranges[key] = (min(low, value), max(high, value))

    def error(self, key, detail):
        event = {'utc': utc().isoformat(), 'monotonic': time.monotonic(), 'kind': key, 'detail': detail}
        with self.lock:
            self.counts['error_' + key] += 1
            if len(self.examples) < 10:
                self.examples.append(dict(event, detail=str(detail)))
        if self.error_sink is not None:
            self.error_sink(event)

    def snapshot(self):
        with self.lock:
            return {'counts': dict(self.counts), 'ranges': dict(self.ranges), 'errors': list(self.examples)}


def window(before, after):
    result = rates(before, after)
    for key in ('udp_queued', 'udp_errors'):
        delta = after['engine'][key] - before['engine'][key]
        if delta < 0:
            raise ValueError('OSC counters reset: ' + key)
        result[key + '_delta'] = delta
    return result


class JsonlSink:
    def __init__(self, path):
        self.file = path.open('x', buffering=1)
        self.lock = threading.Lock()

    def __call__(self, value):
        with self.lock:
            self.file.write(json.dumps(value, allow_nan=False) + '\n')

    def close(self):
        self.file.close()


def sample_worker(address, stop, stats, trace=None):
    previous = None
    changed_at = time.monotonic()
    request_id = 0
    while not stop.is_set():
        began = time.monotonic()
        request_id += 1
        record = {'id': request_id, 'utc': utc().isoformat(), 'started_monotonic': began,
                  'address': address, 'method': 'GET', 'path': '/api/sample'}
        stage = 'request'
        connection = http.client.HTTPConnection(address, 80, timeout=2)
        stats.add('http_requests')
        try:
            if trace is not None:
                stage = 'connect'
                connection.connect()
                record['connected_seconds'] = time.monotonic() - began
            stage = 'request'
            connection.request('GET', '/api/sample', headers={'Host': address})
            record['request_sent_seconds'] = time.monotonic() - began
            stage = 'response_headers'
            response = connection.getresponse()
            record['headers_seconds'] = time.monotonic() - began
            record['status'] = response.status
            stage = 'response_body'
            payload = response.read()
            record['body_seconds'] = time.monotonic() - began
            record['bytes'] = len(payload)
            stage = 'sample_validation'
            sample = json.loads(payload)
            if response.status != 200 or sample.get('valid') is not True:
                raise ValueError('Invalid sample/status: ' + str(response.status))
            current = (sample['sequence'], sample['timestamp_us'])
            if previous is not None and any(a < b for a, b in zip(current, previous)):
                raise ValueError('Sample sequence/timestamp moved backwards')
            if current != previous:
                changed_at = time.monotonic()
            elif time.monotonic() - changed_at > 1:
                stats.error('stale_sample', current)
            previous = current
            record['valid'] = True
            stats.add('http_valid')
        except Exception as error:
            record.update(valid=False, error=repr(error), error_stage=stage)
            stats.error('http', dict(record))
        finally:
            connection.close()
        elapsed = time.monotonic() - began
        record['elapsed_seconds'] = elapsed
        if trace is not None:
            trace(record)
        stats.observe('http_seconds', elapsed)
        if elapsed > .1:
            stats.add('http_over_100ms')
        # Target 10 Hz; never send catch-up bursts after a slow request.
        stop.wait(max(0, .1 - elapsed))


def udp_worker(sock, source, stop, stats):
    previous = None
    while not stop.is_set():
        try:
            packet, peer = sock.recvfrom(65535)
        except socket.timeout:
            continue
        except OSError as error:
            stats.error('udp_socket', repr(error))
            return
        if peer[0] != source:
            stats.add('udp_other_sources')
            continue
        now = time.monotonic()
        stats.add('udp_packets')
        stats.add('udp_bytes', len(packet))
        if previous is not None:
            stats.observe('udp_interarrival_seconds', now - previous)
        previous = now
        try:
            messages = decode_packet(packet)
            stats.add('osc_messages', len(messages))
            stats.add('osc_valid_packets')
        except ValueError as error:
            stats.error('osc_format', repr(error))


def outcome(stats, completed, preserved):
    counts = stats['counts']
    checks = [
        {'name': 'requested_duration', 'result': 'PASS' if completed else 'NOT RUN'},
        {'name': 'observed_errors', 'result': 'FAIL' if any(v for k, v in counts.items() if k.startswith('error_')) else 'PASS'},
        {'name': 'configuration_preserved', 'result': 'PASS' if preserved else 'FAIL'},
        {'name': 'web_samples_received', 'result': 'PASS' if counts.get('http_valid', 0) else 'FAIL'},
        {'name': 'osc_received', 'result': 'PASS' if counts.get('osc_valid_packets', 0) else 'FAIL'},
    ]
    result = 'FAIL' if any(c['result'] == 'FAIL' for c in checks) else 'PASS' if completed else 'NOT RUN'
    return result, checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--primary', required=True)
    parser.add_argument('--secondary', required=True)
    parser.add_argument('--seconds', type=float, default=3600)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--osc-bind', required=True, help='Existing OSC destination IP on this Mac')
    parser.add_argument('--osc-port', type=int, default=16888)
    parser.add_argument('--trace-http', action='store_true', help='Record every request and its timing stages')
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or not 5 <= args.seconds <= 86400 or not 1 <= args.osc_port <= 65535:
        parser.error('Invalid duration or UDP port')
    try:
        args.run_dir.resolve().relative_to((Path.cwd() / 'captures').resolve())
    except ValueError:
        parser.error('Run directory must be inside captures/')
    args.run_dir.mkdir(parents=True, exist_ok=False)
    Path('.build').mkdir(exist_ok=True)
    lock = Path('.build/phase4-endurance.lock').open('a')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        raise SystemExit('Another endurance test is running')
    stop = threading.Event()
    for sig in (signal.SIGTERM, signal.SIGINT):
        signal.signal(sig, lambda *_: stop.set())
    errors = JsonlSink(args.run_dir / 'errors.jsonl')
    trace = JsonlSink(args.run_dir / 'http-requests.jsonl') if args.trace_http else None
    stats = Statistics(errors)
    handles, workers, clients = [], [], {}
    sock = None
    started = None
    report = {'pid': os.getpid(), 'requested_seconds': args.seconds, 'read_only': True,
              'before': {}, 'after': {}, 'windows': {}, 'tests': [], 'result': 'NOT RUN'}
    events = (args.run_dir / 'observations.jsonl').open('x', buffering=1)
    began_utc = utc()

    def progress(state):
        value = {'pid': os.getpid(), 'state': state, 'result': report['result'],
                 'started_utc': began_utc.isoformat(),
                 'expected_end_utc': (began_utc + datetime.timedelta(seconds=args.seconds)).isoformat(),
                 'updated_utc': utc().isoformat(),
                 'elapsed_seconds': 0 if started is None else time.monotonic() - started,
                 'requested_seconds': args.seconds, 'statistics': stats.snapshot()}
        atomic_json(args.run_dir / 'status.json', value)
        summary = args.run_dir / 'summary.md'
        if not summary.exists():
            summary.write_text('# Axio — endurance in progress\n\nNo final result available.\n'
                               'Current state is in status.json; the final report will replace this file.\n')

    try:
        import serial
        progress('preflight')
        for name, path in (('primary', args.primary), ('secondary', args.secondary)):
            handle = serial.Serial(path, 115200, timeout=.05, write_timeout=3, exclusive=True)
            handles.append(handle)
            client = clients[name] = Client(handle, 3)
            report['before'][name] = {cmd: client.request(cmd) for cmd in ('INFO', 'STATUS', 'METRICS', 'CONFIG GET', 'WIFI GET')}
        initial = report['before']['primary']
        if initial['STATUS']['imu'] != 'ready' or not initial['STATUS']['sample_valid']:
            raise RuntimeError('Primary IMU is not ready')
        for name, device in report['before'].items():
            net = device['STATUS']['connectivity']
            if not net['sta_connected'] or net['ap_active']:
                raise RuntimeError(name + ': STA required')
        destination = {'ip': args.osc_bind, 'port': args.osc_port}
        if destination not in initial['CONFIG GET']['destinations']:
            raise RuntimeError('OSC receiver must match an existing destination; no Apply is allowed')
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((args.osc_bind, args.osc_port))  # Exclusive; never steals another receiver's port.
        sock.settimeout(.2)
        address = initial['STATUS']['connectivity']['sta_ip']
        for target, arguments in ((sample_worker, (address, stop, stats, trace)),
                                  (udp_worker, (sock, address, stop, stats))):
            worker = threading.Thread(target=target, args=arguments)
            workers.append(worker)
        atomic_json(args.run_dir / 'initial.json', report['before'])
        began_utc, started = utc(), time.monotonic()
        for worker in workers:
            worker.start()
        previous = report['before'].copy()
        next_usb = next_network = next_log = started
        while not stop.is_set() and time.monotonic() - started < args.seconds:
            now = time.monotonic()
            if any(not w.is_alive() for w in workers):
                raise RuntimeError('A worker stopped unexpectedly')
            if now >= next_usb:
                row = {'utc': utc().isoformat(), 'elapsed_seconds': now - started, 'devices': {}}
                for name, client in clients.items():
                    try:
                        current = {cmd: client.request(cmd) for cmd in ('STATUS', 'METRICS')}
                        row['devices'][name] = current
                        status = current['STATUS']
                        if status['uptime_us'] < previous[name]['STATUS']['uptime_us']:
                            stats.error('reboot_' + name, status['uptime_us'])
                        if not status['wifi_connected']:
                            stats.error('wifi_' + name, 'STA unavailable')
                        if name == 'primary':
                            measured = window(previous[name]['METRICS'], current['METRICS'])
                            row['imu_window'] = measured
                            if status['imu'] != 'ready' or not status['sample_valid'] or (measured['seconds'] >= 1 and measured['acquired_delta'] <= 0):
                                stats.error('imu', status['imu'])
                            if measured['read_errors_delta'] or measured['udp_errors_delta']:
                                stats.error('firmware_counters', measured)
                            if measured['seconds'] >= 1:
                                stats.observe('imu_hz', measured['acquired_hz'])
                        stats.observe(name + '_heap_free', status['heap_free'])
                        stats.observe(name + '_heap_min_cumulative', status['heap_min'])
                        previous[name] = current
                    except Exception as error:
                        stats.error('usb_' + name, repr(error))
                        row['devices'][name] = {'error': repr(error)}
                events.write(json.dumps(row, allow_nan=False) + '\n')
                progress('running')
                next_usb = time.monotonic() + 10
            if now >= next_network:
                row = {'utc': utc().isoformat(), 'elapsed_seconds': time.monotonic() - started}
                try:
                    row['dns'] = query()
                    ips = {v['STATUS']['connectivity']['sta_ip'] for v in report['before'].values()}
                    if len(row['dns']) != 1 or row['dns'][0] not in ips:
                        stats.error('dns', row['dns'])
                    for name, client in clients.items():
                        ip = previous[name]['STATUS']['connectivity']['sta_ip']
                        response = row[name] = http_snapshot(ip)
                        if response['status'] != 200:
                            raise RuntimeError(name + ': snapshot HTTP ' + str(response['status']))
                        largest = int(response['snapshot']['diagnostics']['largest_block'].split()[0])
                        stats.observe(name + '_largest_block', largest)
                        if ip in row['dns']:
                            row['common_http'] = http_snapshot(ip, 'axio.local')['status']
                            if row['common_http'] != 200:
                                stats.error('common_http', row['common_http'])
                except Exception as error:
                    stats.error('network_observation', repr(error))
                    row['error'] = repr(error)
                events.write(json.dumps(row, allow_nan=False) + '\n')
                next_network = time.monotonic() + 30
            if now >= next_log:
                print(json.dumps({'elapsed_seconds': round(time.monotonic() - started),
                                  'statistics': stats.snapshot()}), flush=True)
                next_log = time.monotonic() + 60
            stop.wait(min(.5, max(0, args.seconds - (time.monotonic() - started))))
        report['completed'] = time.monotonic() - started >= args.seconds
        report['elapsed_seconds'] = time.monotonic() - started
        stop.set()
        for worker in workers:
            worker.join(timeout=5)
        for name, client in clients.items():
            report['after'][name] = {cmd: client.request(cmd) for cmd in ('STATUS', 'METRICS', 'CONFIG GET', 'WIFI GET')}
        report['windows']['primary'] = window(initial['METRICS'], report['after']['primary']['METRICS'])
        preserved = all(report['before'][name][cmd] == report['after'][name][cmd]
                        for name in clients for cmd in ('CONFIG GET', 'WIFI GET'))
        report['result'], report['tests'] = outcome(stats.snapshot(), report['completed'], preserved)
    except Exception as error:
        stats.error('fatal', repr(error))
        report['result'] = 'FAIL'
        report['error'] = repr(error)
    finally:
        stop.set()
        for worker in workers:
            if worker.ident is not None:
                worker.join(timeout=5)
        if sock is not None:
            sock.close()
        for handle in handles:
            handle.close()
        events.close()
        if trace is not None:
            trace.close()
        errors.close()
        report['statistics'] = stats.snapshot()
        elapsed = report.get('elapsed_seconds', 0)
        if elapsed:
            counts = report['statistics']['counts']
            report['observed_http_hz'] = counts.get('http_valid', 0) / elapsed
            report['observed_udp_hz'] = counts.get('udp_packets', 0) / elapsed
        report['finished_utc'] = utc().isoformat()
        atomic_json(args.run_dir / 'report.json', report)
        progress('finished')
        lines = ['# Axio — endurance test', '', 'Result: **' + report['result'] + '**', '',
                 'Elapsed: ' + str(round(report.get('elapsed_seconds', 0), 1)) + ' seconds.', '',
                 'Observed valid HTTP samples/s: ' + str(round(report.get('observed_http_hz', 0), 3)),
                 'Observed UDP datagrams/s: ' + str(round(report.get('observed_udp_hz', 0), 3)), '',
                 'Checks:', ''] + ['- ' + c['name'] + ': ' + c['result'] for c in report['tests']]
        lines += ['', 'Statistics:', '', '```json', json.dumps(report['statistics'], indent=2), '```', '',
                  'IMU rate comes from USB counters. UDP arrival is a separate host measurement.',
                  'No packet-loss/duplication guarantee; no exact per-window maximum IMU interval.',
                  'Heap ranges describe observations and do not certify absence of memory leaks.',
                  'No Apply, calibration, metric reset, upload, sleep or restart was sent.']
        (args.run_dir / 'summary.md').write_text('\n'.join(lines) + '\n')
        print(json.dumps({'result': report['result'], 'run_dir': str(args.run_dir)}), flush=True)
        lock.close()
    return 0 if report['result'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
