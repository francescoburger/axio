#!/usr/bin/env python3
"""Two-device restart or operator-controlled outage. No network Apply or calibration."""
import argparse
import datetime
import http.client
import json
from pathlib import Path
import signal
import socket
import threading
import time
from hardware_test import Client
from entry_point_probe import query, http_snapshot
from endurance_test import Statistics, udp_worker, atomic_json


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def boot_skew_ms(rows, before):
    # A reply received just after the HTTP ACK may still precede ESP.restart().
    row = next(r for r in rows if all('STATUS' in r['devices'][n] and
               r['devices'][n]['STATUS']['uptime_us'] < before[n]['STATUS']['uptime_us'] for n in before))
    origins = [v['status_host_epoch'] - v['STATUS']['uptime_us'] / 1e6 for v in row['devices'].values()]
    return (max(origins) - min(origins)) * 1000


def recovery_ready(devices, dns):
    # Browsing runs every 30..35 s: a DNS leader alone cannot end recovery
    # before the first reciprocal browse has had a chance to finish.
    if len(devices) != 2 or len(dns) != 1:
        return False
    owners = []
    for device in devices.values():
        network = device.get('STATUS', {}).get('connectivity', {})
        discovery = network.get('discovery_details', {})
        if not network.get('sta_connected') or network.get('ap_active') or not discovery.get('peers'):
            return False
        if discovery.get('entry_owned'):
            owners.append(network.get('sta_ip'))
    return owners == dns


def restart_pair(addresses):
    barrier = threading.Barrier(len(addresses))
    results = {}
    def restart(name, ip):
        connection = http.client.HTTPConnection(ip, 80, timeout=4)
        try:
            connection.request('GET', '/api/session')
            response = connection.getresponse()
            token = json.loads(response.read())['token']
            connection.close()
            connection = http.client.HTTPConnection(ip, 80, timeout=4)
            connection.connect()
            barrier.wait(timeout=6)
            sent = time.time()
            connection.request('POST', '/api/control', json.dumps({'action': 'restart', 'confirm': True}),
                               {'Content-Type': 'application/json', 'X-Axio-Token': token, 'Origin': 'http://' + ip})
            response = connection.getresponse()
            body = json.loads(response.read())
            results[name] = {'sent_epoch': sent, 'status': response.status, 'ack': body}
        except Exception as error:
            results[name] = {'error': repr(error)}
            barrier.abort()
        finally:
            connection.close()
    workers = [threading.Thread(target=restart, args=(n, ip)) for n, ip in addresses.items()]
    for w in workers: w.start()
    for w in workers: w.join()
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('restart', 'outage'), required=True)
    parser.add_argument('--primary', default='/dev/cu.usbmodem2101')
    parser.add_argument('--secondary', default='/dev/cu.usbmodem1101')
    parser.add_argument('--osc-bind', default='192.168.1.4')
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--allow-unmatched-osc', action='store_true', help='Preserve old destination; verify runtime counters, not host delivery')
    args = parser.parse_args()
    args.run_dir.resolve().relative_to((Path.cwd() / 'captures').resolve())
    args.run_dir.mkdir(parents=True, exist_ok=False)
    import serial
    from serial.tools import list_ports
    expected = {'primary': 'AC:27:6E:7E:32:38', 'secondary': 'AC:27:6E:7C:31:28'}
    paths = {'primary': args.primary, 'secondary': args.secondary}
    handles, clients = {}, {}
    stop = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM): signal.signal(sig, lambda *_: stop.set())
    report = {'mode': args.mode, 'started_utc': now(), 'before': {}, 'after': {}, 'tests': []}
    stats = Statistics()
    receiver = sock = None
    rows = []
    log = (args.run_dir / 'observations.jsonl').open('x', buffering=1)

    def close(name):
        if name in handles: handles.pop(name).close()
        clients.pop(name, None)

    def read(name, commands):
        if name not in clients:
            ports = [p for p in list_ports.comports() if (p.serial_number or '').upper() == expected[name]]
            if len(ports) != 1: raise RuntimeError('USB identity not found: ' + name)
            handle = serial.Serial(ports[0].device, 115200, timeout=.05, write_timeout=2, exclusive=True)
            handles[name] = handle
            clients[name] = Client(handle, 2)
        try:
            result = {}
            for command in commands:
                result[command] = clients[name].request(command)
                if command == 'STATUS': result['status_host_epoch'] = time.time()
            return result
        except Exception:
            close(name)
            raise

    try:
        for name in paths:
            value = report['before'][name] = read(name, ('INFO', 'STATUS', 'METRICS', 'CONFIG GET', 'WIFI GET'))
            expected_host = 'axio-' + expected[name].lower().replace(':', '')
            if value['STATUS']['connectivity']['mdns_hostname'] != expected_host: raise RuntimeError('Unexpected identity')
            if not value['STATUS']['wifi_connected']: raise RuntimeError('Initial STA unavailable: ' + name)
        primary = report['before']['primary']
        if primary['STATUS']['imu'] != 'ready': raise RuntimeError('Primary IMU not ready')
        matched = {'ip': args.osc_bind, 'port': 16888} in primary['CONFIG GET']['destinations']
        report['osc_receiver_matches_destination'] = matched
        if not matched and not args.allow_unmatched_osc:
            raise RuntimeError('Receiver does not match saved OSC destination')
        atomic_json(args.run_dir / 'before.json', report['before'])
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((args.osc_bind if matched else '0.0.0.0', 16888)); sock.settimeout(.2)
        receiver = threading.Thread(target=udp_worker, args=(sock, primary['STATUS']['connectivity']['sta_ip'], stop, stats))
        receiver.start()
        if args.mode == 'restart':
            report['restart'] = restart_pair({n: v['STATUS']['connectivity']['sta_ip'] for n, v in report['before'].items()})
            for name in paths: close(name)
            if not all(v.get('status') == 200 and v.get('ack', {}).get('acknowledged') for v in report['restart'].values()):
                raise RuntimeError('Both restart acknowledgements required')
        started = time.monotonic()
        interrupted = args.mode == 'restart'
        interrupted_at = started if interrupted else None
        stable_at = None
        resumed_packets = 0
        resumed_queued = 0
        last_print = None
        while not stop.is_set():
            elapsed = time.monotonic() - started
            row = {'utc': now(), 'elapsed': elapsed, 'devices': {}, 'udp_packets': stats.snapshot()['counts'].get('udp_packets', 0)}
            for name in paths:
                try: row['devices'][name] = read(name, ('STATUS', 'METRICS'))
                except Exception as error: row['devices'][name] = {'error': repr(error)}
            both = all('STATUS' in row['devices'][n] for n in paths)
            all_online = both and all(v['STATUS']['connectivity']['sta_connected'] and not v['STATUS']['connectivity']['ap_active'] for v in row['devices'].values())
            if args.mode == 'outage' and not interrupted and both and all(not v['STATUS']['connectivity']['sta_connected'] for v in row['devices'].values()):
                interrupted = True; interrupted_at = time.monotonic()
            if all_online and interrupted:
                try: row['dns'] = query(seconds=.4)
                except OSError as error: row['dns_error'] = repr(error)
                if recovery_ready(row['devices'], row.get('dns', [])):
                    if stable_at is None:
                        stable_at = time.monotonic(); resumed_packets = row['udp_packets']
                        resumed_queued = row['devices']['primary']['METRICS']['engine']['udp_queued']
                else: stable_at = None
            else: stable_at = None
            rows.append(row); log.write(json.dumps(row) + '\n')
            state = 'recovering' if interrupted else 'ready_for_operator'
            atomic_json(args.run_dir / 'status.json', {'state': state, 'mode': args.mode, 'utc': now(), 'elapsed': elapsed,
                'sta': {n: v.get('STATUS', {}).get('wifi_connected') for n, v in row['devices'].items()}, 'dns': row.get('dns')})
            compact = (state, tuple(v.get('STATUS', {}).get('wifi_connected') for v in row['devices'].values()), tuple(row.get('dns', [])))
            if compact != last_print: print(json.dumps({'utc': now(), 'state': compact}), flush=True); last_print = compact
            if stable_at is not None and time.monotonic() - stable_at >= 12:
                report['resumed_udp_packets'] = row['udp_packets'] - resumed_packets
                report['resumed_engine_datagrams'] = row['devices']['primary']['METRICS']['engine']['udp_queued'] - resumed_queued
                break
            if not interrupted and elapsed > 600: raise RuntimeError('No outage observed within 10 minutes')
            if interrupted_at is not None and time.monotonic() - interrupted_at > 240: raise RuntimeError('Recovery timeout')
            stop.wait(1)
        for name in paths: report['after'][name] = read(name, ('STATUS', 'METRICS', 'CONFIG GET', 'WIFI GET', 'DISCOVERY GET'))
        final = report['after']
        checks = {
            'configuration_preserved': all(final[n][c] == report['before'][n][c] for n in paths for c in ('CONFIG GET', 'WIFI GET')),
            'both_sta_recovered': all(v['STATUS']['wifi_connected'] and not v['STATUS']['connectivity']['ap_active'] for v in final.values()),
            'primary_imu_ready': final['primary']['STATUS']['imu'] == 'ready',
            'reciprocal_discovery': all(bool(v['DISCOVERY GET']['peers']) for v in final.values()),
            'one_owner': sum(v['STATUS']['connectivity']['discovery_details']['entry_owned'] for v in final.values()) == 1,
            'osc_runtime_resumed': report.get('resumed_engine_datagrams', 0) > 0,
            'osc_format': not stats.snapshot()['counts'].get('error_osc_format', 0),
        }
        ips = query()
        report['final_dns'] = ips
        report['http'] = {n: http_snapshot(v['STATUS']['connectivity']['sta_ip'])['status'] for n, v in final.items()}
        report['common_http'] = http_snapshot(ips[0], 'axio.local')['status'] if len(ips) == 1 else None
        checks['final_http'] = all(c == 200 for c in report['http'].values()) and report['common_http'] == 200
        report['multiple_dns_observed'] = any(len(r.get('dns', [])) > 1 for r in rows)
        checks['no_multiple_dns_observed'] = not report['multiple_dns_observed']
        if args.mode == 'restart':
            checks['both_restarted'] = all(final[n]['STATUS']['uptime_us'] < report['before'][n]['STATUS']['uptime_us'] for n in paths)
            times = [v['sent_epoch'] for v in report['restart'].values()]
            report['restart_send_skew_ms'] = (max(times) - min(times)) * 1000
            report['estimated_boot_skew_ms'] = boot_skew_ms(rows, report['before'])
        else:
            offline = [r for r in rows if 'STATUS' in r['devices']['primary'] and not r['devices']['primary']['STATUS']['connectivity']['sta_connected']]
            checks['offline_observed'] = len(offline) >= 2
            checks['no_reboot'] = all(final[n]['STATUS']['uptime_us'] > report['before'][n]['STATUS']['uptime_us'] for n in paths)
            checks['ap_observed'] = all(any(r['devices'].get(n, {}).get('STATUS', {}).get('connectivity', {}).get('ap_active') for r in rows) for n in paths)
            checks['imu_continued_offline'] = len(offline) >= 2 and offline[-1]['devices']['primary']['METRICS']['acquired'] > offline[0]['devices']['primary']['METRICS']['acquired']
            checks['osc_suspended_offline'] = len(offline) >= 2 and all(not r['devices']['primary']['METRICS']['engine']['listening'] for r in offline[1:])
        report['tests'] = [{'name': n, 'result': 'PASS' if ok else 'FAIL'} for n, ok in checks.items()]
        report['tests'].append({'name': 'osc_host_reception', 'result': ('PASS' if report.get('resumed_udp_packets', 0) > 0 else 'FAIL') if matched else 'NOT RUN'})
        if matched: checks['osc_host_reception'] = report.get('resumed_udp_packets', 0) > 0
        report['result'] = 'PASS' if all(checks.values()) else 'FAIL'
    except Exception as error:
        report['result'] = 'FAIL'; report['error'] = repr(error)
    finally:
        stop.set()
        if receiver is not None: receiver.join(timeout=2)
        if sock is not None: sock.close()
        for name in list(handles): close(name)
        log.close()
        report['statistics'] = stats.snapshot(); report['finished_utc'] = now()
        atomic_json(args.run_dir / 'report.json', report)
        atomic_json(args.run_dir / 'status.json', {'state': 'finished', 'result': report['result'], 'utc': now()})
        print(json.dumps({'result': report['result'], 'tests': report['tests'], 'error': report.get('error')}), flush=True)
    return 0 if report['result'] == 'PASS' else 1


if __name__ == '__main__': raise SystemExit(main())
