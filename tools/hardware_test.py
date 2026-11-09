#!/usr/bin/env python3
"""Explicit Axio USB tests; no automatic upload, calibration or reset."""
import argparse
import glob
import json
import math
from pathlib import Path
import socket
import secrets
import sys
import time


class ProtocolError(RuntimeError):
    pass


class Client:
    def __init__(self, port, timeout=2, session_id=None):
        self.port, self.timeout = port, timeout
        # A delayed response can survive close/reopen of USB. Never reuse IDs
        # across independent client sessions. Empty ID prefix is a test seam.
        self.session_id = secrets.token_hex(4) if session_id is None else session_id
        self.sequence = 0
        self.buffer = bytearray()
        self.discard = False

    def request(self, command):
        self.sequence += 1
        if self.sequence > 0xffffffff:
            raise ProtocolError('Session IDs exhausted')
        request_id = self.session_id + format(self.sequence, '08x') if self.session_id else str(self.sequence)
        wire = ('AXIO ' + request_id + ' ' + command + '\n').encode('ascii')
        if len(wire) > 8193:
            raise ProtocolError('Request too long')
        if self.port.write(wire) != len(wire):
            raise ProtocolError('Incomplete serial write')
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            byte = self.port.read(1)
            if not byte:
                continue
            if byte == b'\n':
                raw = bytes(self.buffer)
                self.buffer.clear()
                if self.discard:
                    self.discard = False
                    continue
                if not raw.startswith(b'{'):
                    continue  # boot ROM / ordinary logs
                try:
                    row = json.loads(raw)
                except (ValueError, UnicodeError) as exc:
                    raise ProtocolError('Malformed JSONL') from exc
                if not isinstance(row, dict) or row.get('axio') != 1:
                    continue
                if row.get('id') != request_id:
                    continue
                if type(row.get('ok')) is not bool:
                    raise ProtocolError('Response missing a boolean outcome')
                if not row['ok']:
                    raise ProtocolError(str(row.get('error', 'Error without a code')))
                if not isinstance(row.get('data'), dict):
                    raise ProtocolError('data is not an object')
                return row['data']
            elif not self.discard:
                self.buffer.extend(byte)
                if len(self.buffer) > 12288:
                    self.buffer.clear()
                    self.discard = True
        raise ProtocolError('Response timeout: ' + command)


def rates(before, after):
    dt = (after['window_us'] - before['window_us']) / 1e6
    if dt <= 0:
        raise ProtocolError('Metric window reset or not increasing')
    result = {'seconds': dt}
    for key in ('acquired', 'snapshots', 'osc_attempts', 'read_errors'):
        delta = after[key] - before[key]
        if delta < 0:
            raise ProtocolError('Counters reset during measurement')
        result[key + '_delta'] = delta
        result[key + '_hz'] = delta / dt
    return result


def wait_for_wifi(client, status, seconds):
    """Optional read-only grace period for asynchronous firmware startup."""
    deadline = time.monotonic() + seconds
    while not status.get('wifi_connected') and time.monotonic() < deadline:
        time.sleep(min(.1, max(0, deadline-time.monotonic())))
        status = client.request('STATUS')
    return status


def osc_capture(duration, bind, port):
    from osc_receiver import decode_packet, OSCError
    from analyze_capture import summarize
    rows = []
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((bind, port))
        sock.settimeout(.1)
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            try:
                packet, peer = sock.recvfrom(65535)
            except socket.timeout:
                continue
            row = dict(source_ip=peer[0], source_port=peer[1],
                       received_monotonic_ns=time.monotonic_ns())
            try:
                row['messages'] = decode_packet(packet)
            except OSCError as exc:
                row['error'] = str(exc)
            rows.append(row)
    return summarize(rows)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='Explicit USB port; no automatic selection')
    parser.add_argument('--list-ports', action='store_true')
    parser.add_argument('--timeout', type=float, default=2)
    parser.add_argument('--boot-timeout', type=float, default=20)
    parser.add_argument('--metrics-seconds', type=float, default=2)
    parser.add_argument('--calibrate', '--tare', dest='calibrate', action='store_true', help='CHANGES RAM references (--tare: legacy alias)')
    parser.add_argument('--reset-metrics', action='store_true', help='CHANGES diagnostic counters')
    parser.add_argument('--require-wifi', action='store_true')
    parser.add_argument('--wifi-wait-seconds', type=float, default=0,
                        help='Read-only STATUS wait before checking --require-wifi (default 0)')
    parser.add_argument('--osc-seconds', type=float, default=0, help='Test OSC reception from the network')
    parser.add_argument('--osc-bind', default='0.0.0.0')
    parser.add_argument('--osc-port', type=int, default=16888)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args(argv)
    if (not all(math.isfinite(v) for v in (args.timeout, args.boot_timeout, args.metrics_seconds, args.osc_seconds, args.wifi_wait_seconds)) or
            min(args.timeout, args.boot_timeout, args.metrics_seconds) <= 0 or
            args.osc_seconds < 0 or args.wifi_wait_seconds < 0 or not 1 <= args.osc_port <= 65535):
        parser.error('Invalid timeout, durations or port')
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        serial = None
    if args.list_ports:
        ports = ([dict(device=p.device, description=p.description, hwid=p.hwid)
                  for p in list_ports.comports()] if serial else
                 [dict(device=p, description='Candidate; pyserial missing')
                  for p in sorted(glob.glob('/dev/cu.*'))])
        print(json.dumps(ports, indent=2))
        if not args.port:
            return 0
    report = {'schema': 1, 'port': args.port, 'tests': [],
              'hardware_executed': False, 'persistent_changes': False}

    def record(name, state, detail, category='usb_read_only'):
        report['tests'].append(dict(name=name, result=state, category=category, detail=detail))

    if not args.port or serial is None:
        record('hardware', 'NOT RUN', 'Specify --port and install pyserial in the documented venv')
    else:
        try:
            with serial.Serial(args.port, 115200, timeout=.05, write_timeout=args.timeout,
                               exclusive=True) as port:
                client = Client(port, args.timeout)
                deadline = time.monotonic() + args.boot_timeout
                last_error = 'Firmware not ready'
                while time.monotonic() < deadline:
                    try:
                        pong = client.request('PING')
                        if pong.get('pong') is not True:
                            raise ProtocolError('PING without pong')
                        break
                    except ProtocolError as exc:
                        last_error = str(exc)
                else:
                    raise ProtocolError('Startup: ' + last_error)
                report['hardware_executed'] = True
                record('communication', 'PASS', pong)
                for command in ('INFO', 'STATUS', 'CONFIG GET'):
                    data = client.request(command)
                    if command == 'STATUS' and args.require_wifi and args.wifi_wait_seconds:
                        data = wait_for_wifi(client, data, args.wifi_wait_seconds)
                    record(command.lower(), 'PASS', data)
                    if command == 'STATUS':
                        record('imu', 'PASS' if data.get('imu') == 'ready' and data.get('sample_valid') else 'FAIL', data)
                        record('wifi', ('PASS' if data.get('wifi_connected') else 'FAIL') if args.require_wifi else 'NOT RUN',
                               data.get('wifi_connected'), 'wifi')
                for enabled, command in ((args.calibrate, 'CALIBRATION'), (args.reset_metrics, 'DIAG RESET')):
                    if enabled:
                        record(command.lower(), 'PASS', client.request(command), 'state_change')
                    else:
                        record(command.lower(), 'NOT RUN', 'Requires an explicit option', 'state_change')
                before = client.request('METRICS')
                time.sleep(args.metrics_seconds)
                after = client.request('METRICS')
                measured = rates(before, after)
                record('counters', 'PASS' if measured['acquired_delta'] > 0 and measured['read_errors_delta'] == 0 else 'FAIL', measured)
                report['metrics'] = dict(before=before, after=after)
                if args.osc_seconds:
                    capture = osc_capture(args.osc_seconds, args.osc_bind, args.osc_port)
                    record('osc_receive', 'PASS' if capture and all(r['invalid_packets'] == 0 and r['valid_osc_packets'] > 0 for r in capture) else 'FAIL', capture, 'wifi_receiver')
                else:
                    record('osc_receive', 'NOT RUN', 'Requires --osc-seconds and an already configured Mac destination', 'wifi_receiver')
        except (OSError, ProtocolError, serial.SerialException, KeyError, TypeError, ValueError) as exc:
            record('hardware', 'FAIL', str(exc))
    existing = {t['name'] for t in report['tests']}
    for name, category in [('communication', 'usb_read_only'), ('info', 'usb_read_only'),
                           ('status', 'usb_read_only'), ('imu', 'usb_read_only'),
                           ('config get', 'usb_read_only'), ('counters', 'usb_read_only'),
                           ('calibration', 'state_change'), ('diag reset', 'state_change'),
                           ('wifi', 'wifi'), ('osc_receive', 'wifi_receiver')]:
        if name not in existing:
            record(name, 'NOT RUN', 'Not reached; see prerequisites or hardware error', category)
    record('configuration_persistence', 'NOT RUN', 'Requires an explicit persistence test with osc_config.py and restart', 'persistent_change')
    print(json.dumps(report, indent=2, allow_nan=False))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open('x', encoding='utf-8') as output:
            json.dump(report, output, indent=2, allow_nan=False)
            output.write('\n')
    if any(t['result'] == 'FAIL' for t in report['tests']):
        return 1
    return 0 if report['hardware_executed'] else 2


if __name__ == '__main__':
    sys.exit(main())
