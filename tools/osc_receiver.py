#!/usr/bin/env python3
"""OSC UDP to JSONL using only the standard library. One record per datagram."""
import argparse
import base64
import json
import math
from pathlib import Path
import socket
import struct
import time


class OSCError(ValueError):
    pass


def decode_packet(data, depth=0, timetags=()):
    if depth > 16 or not data or len(data) % 4:
        raise OSCError('Invalid OSC size/alignment or depth')
    pos = 0

    def take(n):
        nonlocal pos
        if n < 0 or pos + n > len(data):
            raise OSCError('Truncated packet')
        value = data[pos:pos + n]
        pos += n
        return value

    def string():
        nonlocal pos
        end = data.find(b'\0', pos)
        if end < 0:
            raise OSCError('Unterminated string')
        raw = take(end - pos)
        padding = take(4 - len(raw) % 4)
        if any(padding):
            raise OSCError('Nonzero string padding')
        try:
            return raw.decode('utf-8')
        except UnicodeDecodeError as exc:
            raise OSCError('Invalid UTF-8') from exc

    def number(fmt):
        value = struct.unpack('>' + fmt, take(struct.calcsize('>' + fmt)))[0]
        # Keep JSON valid for OSC NaN/Inf floats; the type tag remains available.
        return str(value) if isinstance(value, float) and not math.isfinite(value) else value

    if data.startswith(b'#bundle\0'):
        take(8)
        tag = number('Q')
        result = []
        while pos < len(data):
            size = number('i')
            if size <= 0:
                raise OSCError('Invalid bundle element size')
            result.extend(decode_packet(take(size), depth + 1, timetags + (tag,)))
        return result
    address = string()
    tags = string()
    if not address.startswith('/') or not tags.startswith(','):
        raise OSCError('Invalid OSC address or type tag')
    values = []
    for tag in tags[1:]:
        if tag in 'ifhd':
            values.append(number({'i': 'i', 'f': 'f', 'h': 'q', 'd': 'd'}[tag]))
        elif tag == 's':
            values.append(string())
        elif tag == 'b':
            size = number('i')
            blob = take(size)
            if any(take((-size) % 4)):
                raise OSCError('Nonzero blob padding')
            values.append({'base64': base64.b64encode(blob).decode('ascii')})
        elif tag in 'TFNI':
            values.append({'T': True, 'F': False, 'N': None, 'I': 'impulse'}[tag])
        else:
            raise OSCError('Unsupported type tag: ' + tag)
    if pos != len(data):
        raise OSCError('Trailing bytes in message')
    return [{'address': address, 'types': tags, 'values': values,
             'bundle_timetags': list(timetags)}]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--bind', default='0.0.0.0')
    parser.add_argument('--port', type=int, default=16888)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=float, default=0, help='Seconds; 0 runs until Ctrl-C')
    args = parser.parse_args()
    if not 0 <= args.port <= 65535 or args.duration < 0:
        parser.error('Invalid port or duration')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Do not overwrite previous captures.
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.bind, args.port))
        sock.settimeout(0.2)
        with args.output.open('x', encoding='utf-8') as output:
            print('Listening on {}:{}'.format(*sock.getsockname()), flush=True)
            start = time.monotonic()
            count = 0
            try:
                while not args.duration or time.monotonic() - start < args.duration:
                    try:
                        data, peer = sock.recvfrom(65535)
                    except socket.timeout:
                        continue
                    mono_ns = time.monotonic_ns()
                    wall_ns = time.time_ns()
                    count += 1
                    record = {'packet_id': count, 'received_unix_ns': wall_ns,
                              'received_monotonic_ns': mono_ns, 'source_ip': peer[0],
                              'source_port': peer[1], 'size_bytes': len(data)}
                    try:
                        record['messages'] = decode_packet(data)
                    except OSCError as exc:
                        record.update(error=str(exc), raw_base64=base64.b64encode(data).decode('ascii'))
                    output.write(json.dumps(record, allow_nan=False) + '\n')
                    output.flush()
            except KeyboardInterrupt:
                pass
            print('Datagrams recorded: {}'.format(count))


if __name__ == '__main__':
    main()
