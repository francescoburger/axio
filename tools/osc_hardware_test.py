#!/usr/bin/env python3
"""Verifica OSC reale dalla XIAO; non applica configurazioni. Tara solo con --tare."""
import argparse
import json
import math
from pathlib import Path
import select
import socket
import sys
import time
from osc_receiver import decode_packet, OSCError
from analyze_capture import summarize
from hardware_test import Client


def osc_string(value):
    raw=value.encode('ascii')+b'\0'
    return raw+b'\0'*(-len(raw)%4)


def check_message(message, definition, device_id):
    expected=definition['address'].replace('{id}',device_id)
    if message['address']!=expected:
        raise ValueError('indirizzo inatteso: '+message['address'])
    if message['types']!=','+'f'*len(definition['fields']):
        raise ValueError('tipi/numero argomenti inattesi')
    if bool(message['bundle_timetags'])!=definition['bundle']:
        raise ValueError('bundle inatteso')
    if not all(isinstance(v,(int,float)) and math.isfinite(v) for v in message['values']):
        raise ValueError('valore OSC non finito')
    # Nominal ranges are deliberately not clamped or rejected.


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',type=Path,required=True)
    p.add_argument('--tracker-ip',required=True)
    p.add_argument('--bind',default='0.0.0.0')
    p.add_argument('--duration',type=float,default=10)
    p.add_argument('--serial-port',help='opzionale: legge CONFIG GET e METRICS prima/dopo')
    p.add_argument('--tare',action='store_true',help='MODIFICA riferimenti RAM; richiede ACK abilitato')
    p.add_argument('--report',type=Path,required=True)
    a=p.parse_args()
    if not math.isfinite(a.duration) or a.duration<=0:
        p.error('durata deve essere positiva e finita')
    if a.report.exists():
        p.error('report già esistente: scegliere un nome nuovo')
    config=json.loads(a.config.read_text())
    if a.tare and not config['ack']['enabled']:
        p.error('--tare richiede ACK abilitato nella configurazione già applicata')
    report=dict(schema=1,tests=[],hardware_executed=False,tare_requested=a.tare,configuration_changed=False)
    def record(name,result,detail):
        report['tests'].append(dict(name=name,result=result,detail=detail))
    sockets=[];serial_port=None;rows=[];counts={};ack_count=0
    try:
        ports={d['port'] for d in config['destinations']}
        for port in ports:
            sock=socket.socket(socket.AF_INET,socket.SOCK_DGRAM);sockets.append(sock)
            sock.bind((a.bind,port))
        expected={}
        for i,m in enumerate(config['messages']):
            address=m['address'].replace('{id}',config['id'])
            for route in m['destinations']:
                port=config['destinations'][route]['port']
                expected[(port,address)]=m;counts[(port,address)]=0
        if a.serial_port:
            import serial
            serial_port=serial.Serial(a.serial_port,115200,timeout=.05,write_timeout=2,exclusive=True)
            client=Client(serial_port,2)
            active=client.request('CONFIG GET')
            record('configuration', 'PASS' if active==config else 'FAIL',active)
            before=client.request('METRICS')
            report['metrics_before']=before
        start=time.monotonic();sent_tare=False
        while time.monotonic()-start<a.duration:
            if a.tare and not sent_tare:
                payload=osc_string(config['tare_address'].replace('{id}',config['id']))+osc_string(',')
                sockets[0].sendto(payload,(a.tracker_ip,config['listen_port']));sent_tare=True
            ready,_,_=select.select(sockets,[],[],min(.1,a.duration))
            for sock in ready:
                data,peer=sock.recvfrom(65535)
                if peer[0]!=a.tracker_ip:
                    continue
                report['hardware_executed']=True
                port=sock.getsockname()[1]
                row=dict(source_ip=peer[0],source_port=peer[1],received_monotonic_ns=time.monotonic_ns(),listen_port=port)
                try:
                    messages=decode_packet(data);row['messages']=messages
                    for m in messages:
                        key=(port,m['address'])
                        ack=config['ack']
                        ack_port=config['destinations'][ack['destination']]['port']
                        if a.tare and port==ack_port and m['address']==ack['address'].replace('{id}',config['id']):
                            if m['types']!=',i' or m['values']!=[1]:
                                raise ValueError('ACK non valido')
                            ack_count+=1
                        else:
                            if key not in expected:
                                raise ValueError('routing/indirizzo inatteso: '+str(key))
                            check_message(m,expected[key],config['id']);counts[key]+=1
                except (OSCError,ValueError) as exc:
                    row['error']=str(exc)
                rows.append(row)
        record('osc_format_routing', 'PASS' if rows and not any('error' in r for r in rows) else 'FAIL',
               [r['error'] for r in rows if 'error' in r])
        record('messages_received','PASS' if counts and all(counts.values()) else 'FAIL',
               [dict(port=k[0],address=k[1],received=v) for k,v in counts.items()])
        record('tare_ack',('PASS' if ack_count==1 else 'FAIL') if a.tare else 'NOT RUN',dict(acknowledgements=ack_count))
        report['receive_statistics']=[dict(listen_port=port,streams=summarize([r for r in rows if r['listen_port']==port]))
                                      for port in sorted(ports)]
        if serial_port:
            report['metrics_after']=client.request('METRICS')
    except (OSError,ImportError,ValueError,RuntimeError,KeyError) as exc:
        record('hardware','FAIL',str(exc))
    finally:
        for sock in sockets:sock.close()
        if serial_port:serial_port.close()
    a.report.parent.mkdir(parents=True,exist_ok=True)
    with a.report.open('x') as stream:
        json.dump(report,stream,indent=2,allow_nan=False);stream.write('\n')
    print(json.dumps(report,indent=2,allow_nan=False))
    return int(any(t['result']=='FAIL' for t in report['tests']))


if __name__=='__main__':
    sys.exit(main())
