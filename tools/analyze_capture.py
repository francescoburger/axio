#!/usr/bin/env python3
"""UDP reception statistics by source; no IMU rate estimate."""
import argparse
import collections
import json
import math
import statistics


def summarize(records):
    groups = collections.defaultdict(list)
    for r in records:
        groups[(r['source_ip'], r['source_port'])].append(r)
    result = []
    for (ip, port), rows in sorted(groups.items()):
        times = [r['received_monotonic_ns'] for r in rows]
        dt = [(b - a) / 1e6 for a, b in zip(times, times[1:])]
        if any(d < 0 for d in dt):
            raise ValueError('Nonmonotonic timestamps in capture')
        span = (times[-1] - times[0]) / 1e9
        osc_times = [r['received_monotonic_ns'] for r in rows if 'messages' in r]
        osc_span = (osc_times[-1] - osc_times[0]) / 1e9 if osc_times else 0
        result.append({'source_ip': ip, 'source_port': port, 'udp_packets': len(rows),
                       'valid_osc_packets': sum('messages' in r for r in rows),
                       'osc_messages': sum(len(r.get('messages', [])) for r in rows),
                       'invalid_packets': sum('error' in r for r in rows),
                       'received_udp_hz': (len(rows) - 1) / span if span > 0 else None,
                       'received_osc_hz': (len(osc_times) - 1) / osc_span if osc_span > 0 else None,
                       'interval_mean_ms': statistics.mean(dt) if dt else None,
                       'interval_median_ms': statistics.median(dt) if dt else None,
                       'interval_p95_ms': sorted(dt)[max(0, math.ceil(.95 * len(dt)) - 1)] if dt else None,
                       'interval_max_ms': max(dt) if dt else None,
                       'jitter_population_stddev_ms': statistics.pstdev(dt) if dt else None,
                       'imu_hz': None})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    args = parser.parse_args()
    with open(args.capture, encoding='utf-8') as stream:
        print(json.dumps(summarize(json.loads(line) for line in stream if line.strip()), indent=2))


if __name__ == '__main__':
    main()
