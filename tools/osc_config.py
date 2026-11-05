#!/usr/bin/env python3
"""Payload schema 1 e comandi USB; APPLY persistente richiede --apply esplicito."""
import argparse
import json
from pathlib import Path
import sys
from hardware_test import Client, ProtocolError


def preset(name='Orientation', ip='127.0.0.1', port=16888, device_id='0'):
    definitions = {
        'Orientation': [('orientation', ['yaw','pitch','roll']), ('quaternion', ['q0','q1','q2','q3'])],
        'Motion': [('motion/linear', ['axReal','ayReal','azReal']),
                   ('motion/world', ['axWorld','ayWorld','azWorld']), ('motion/gyro', ['gx','gy','gz'])],
        'Custom': [('custom', ['yaw'])]}
    return dict(schema=1, preset=name, id=device_id, listen_port=16889, tare_address='/tare',
                ack=dict(enabled=False, address='/tared/{id}', destination=0),
                destinations=[dict(ip=ip,port=port)],
                messages=[dict(address='/'+address+'/{id}', hz=100, bundle=True, fields=fields, destinations=[0])
                          for address,fields in definitions[name]])


def main():
    p = argparse.ArgumentParser(description=__doc__)
    action = p.add_mutually_exclusive_group(required=True)
    action.add_argument('--preset', choices=['Orientation','Motion','Custom'])
    action.add_argument('--get', action='store_true')
    action.add_argument('--validate', type=Path)
    action.add_argument('--apply', type=Path)
    p.add_argument('--port')
    p.add_argument('--ip', default='127.0.0.1')
    p.add_argument('--udp-port', type=int, default=16888)
    p.add_argument('--id', default='0')
    p.add_argument('--timeout', type=float, default=10, help='include eventuale scrittura NVS')
    a = p.parse_args()
    if a.preset:
        print(json.dumps(preset(a.preset,a.ip,a.udp_port,a.id),separators=(',',':')))
        return 0
    if not a.port:
        p.error('--port obbligatoria per comandi USB')
    if a.timeout<=0:
        p.error('timeout deve essere positivo')
    try:
        import serial
        command = 'CONFIG GET'
        if a.validate or a.apply:
            path=a.validate or a.apply
            if path.stat().st_size>7168:
                raise ProtocolError('configurazione oltre 7168 byte')
            document=json.loads(path.read_text())
            command=('CONFIG APPLY ' if a.apply else 'CONFIG VALIDATE ')+json.dumps(document,ensure_ascii=True,separators=(',',':'),allow_nan=False)
        with serial.Serial(a.port,115200,timeout=.05,write_timeout=2,exclusive=True) as port:
            result=Client(port,a.timeout).request(command)
        print(json.dumps(dict(result='PASS',command=command.split('{',1)[0].strip(),data=result),indent=2))
        return 0
    except (ImportError,OSError,ValueError,ProtocolError) as exc:
        print(json.dumps(dict(result='FAIL',error=str(exc))))
        return 1


if __name__ == '__main__':
    sys.exit(main())
