#!/usr/bin/env python3
"""Read-only USB/LAN observations of axio.local; no upload, reset or calibration."""
import argparse
import datetime
import http.client
import json
from pathlib import Path
import socket
import struct
import time

from hardware_test import Client, rates


def dns_name(packet, offset, depth=0):
    if depth > 20:
        raise ValueError('DNS compression loop')
    labels = []
    while True:
        length = packet[offset]
        offset += 1
        if length & 0xc0 == 0xc0:
            pointer = ((length & 0x3f) << 8) | packet[offset]
            labels.append(dns_name(packet, pointer, depth + 1)[0])
            offset += 1
            break
        if length & 0xc0 or length > 63:
            raise ValueError('Invalid DNS label')
        if not length:
            break
        if offset + length > len(packet):
            raise ValueError('Truncated DNS label')
        labels.append(packet[offset:offset + length].decode('ascii'))
        offset += length
    return '.'.join(labels), offset


def a_records(packet, host):
    _, flags, questions, answers, authority, additional = struct.unpack_from('!6H', packet)
    if not flags & 0x8000 or flags & 0xf:
        return []
    offset, found = 12, []
    for _ in range(questions):
        _, offset = dns_name(packet, offset)
        offset += 4
    for _ in range(answers + authority + additional):
        owner, offset = dns_name(packet, offset)
        kind, cls, ttl, size = struct.unpack_from('!HHIH', packet, offset)
        offset += 10
        if offset + size > len(packet):
            raise ValueError('Truncated DNS record')
        if owner.lower() == host.lower() and kind == 1 and cls & 0x7fff == 1 and size == 4 and ttl:
            found.append(socket.inet_ntoa(packet[offset:offset + size]))
        offset += size
    return found


def query(host='axio.local', seconds=.7):
    labels = b''.join(bytes([len(p)]) + p.encode('ascii') for p in host.split('.')) + b'\0'
    packet = struct.pack('!6H', 0, 0, 1, 0, 0, 0) + labels + struct.pack('!HH', 1, 0x8001)
    found = set()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('', 0))
        sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 255)
        sock.settimeout(.1)
        sock.sendto(packet, ('224.0.0.251', 5353))
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                found.update(a_records(sock.recvfrom(9000)[0], host))
            except socket.timeout:
                pass
            except (ValueError, IndexError, struct.error, UnicodeError):
                pass
    return sorted(found)


def http_snapshot(address, host=None):
    connection = http.client.HTTPConnection(address, 80, timeout=3)
    try:
        connection.request('GET', '/api/snapshot', headers={'Host': host or address})
        response = connection.getresponse()
        body = response.read()
        return {'status': response.status, 'snapshot': json.loads(body)}
    finally:
        connection.close()


def single_reported_owner(observation):
    return sum(bool(d['status']['connectivity'].get('discovery_details', {}).get('entry_owned'))
               for d in observation['devices'].values()) == 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', action='append', required=True)
    parser.add_argument('--duration', type=float, default=25)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    if args.duration < 1 or args.duration > 300 or args.report.exists():
        parser.error('Duration must be 1..300 seconds and report must be new')
    import serial
    handles, clients = [], {}
    report = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'read_only': True, 'devices': {}, 'observations': [], 'tests': []}
    try:
        for path in args.port:
            handle = serial.Serial(path, 115200, timeout=.05, write_timeout=3, exclusive=True)
            handles.append(handle)
            client = clients[path] = Client(handle, 4)
            report['devices'][path] = {'before': {cmd: client.request(cmd) for cmd in
                ('INFO', 'STATUS', 'METRICS', 'CONFIG GET', 'WIFI GET')}}
        deadline = time.monotonic() + args.duration
        while time.monotonic() < deadline:
            row = {'at_monotonic': time.monotonic(), 'devices': {}, 'dns': query()}
            for path, client in clients.items():
                status = client.request('STATUS')
                device = row['devices'][path] = {'status': status}
                network = status['connectivity']
                if network['sta_connected']:
                    address = network['sta_ip']
                    try:
                        device['http'] = http_snapshot(address)
                        if address in row['dns']:
                            device['common_http'] = http_snapshot(address, 'axio.local')
                    except (OSError, ValueError) as error:
                        device['http_error'] = repr(error)
            report['observations'].append(row)
            print(json.dumps({'dns': row['dns'], 'devices': {p: {
                'imu': d['status']['imu'], 'ip': d['status']['connectivity']['sta_ip'],
                'entry': d['status']['connectivity'].get('discovery_details', {}).get('entry_state'),
                'http': d.get('http', {}).get('status'), 'common_http': d.get('common_http', {}).get('status')}
                for p, d in row['devices'].items()}}), flush=True)
            time.sleep(min(1, max(0, deadline - time.monotonic())))
        for path, client in clients.items():
            device = report['devices'][path]
            device['after'] = {cmd: client.request(cmd) for cmd in ('STATUS', 'METRICS', 'CONFIG GET', 'WIFI GET')}
            device['configuration_preserved'] = all(device['before'][cmd] == device['after'][cmd]
                                                      for cmd in ('CONFIG GET', 'WIFI GET'))
            if device['after']['STATUS']['imu'] == 'ready':
                device['imu_window'] = rates(device['before']['METRICS'], device['after']['METRICS'])
        observations = report['observations']
        final = observations[-1]
        checks = {
            'configuration_preserved': all(d['configuration_preserved'] for d in report['devices'].values()),
            'no_observed_multiple_a_records': all(len(o['dns']) <= 1 for o in observations),
            'final_single_a_record': len(final['dns']) == 1,
            'final_single_reported_owner': single_reported_owner(final),
            'final_owner_http': any(d.get('common_http', {}).get('status') == 200 for d in final['devices'].values()),
        }
        report['tests'] = [{'name': name, 'result': 'PASS' if passed else 'FAIL'} for name, passed in checks.items()]
    except Exception as error:
        report['error'] = repr(error)
        print('ERROR ' + repr(error), flush=True)
    finally:
        for handle in handles:
            handle.close()
        with args.report.open('x') as output:
            json.dump(report, output, indent=2)
            output.write('\n')
    return int('error' in report or any(t['result'] == 'FAIL' for t in report['tests']))


if __name__ == '__main__':
    raise SystemExit(main())
