#!/usr/bin/env python3
"""Explicit temporary uploads: isolate native mDNS RX, rejoin, restore production."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import signal
import socket
import struct
import subprocess
import threading
import time

import serial
from serial.tools import list_ports
from hardware_test import Client, rates, ProtocolError
from entry_point_probe import a_records, http_snapshot

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'.build/mdns-partition'
OPTIONS='CDCOnBoot=default,CPUFreq=160,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=default,DebugLevel=none,EraseFlash=none,UploadSpeed=921600'
IDENTITIES={'primary':'AC:27:6E:7E:32:38','secondary':'AC:27:6E:7C:31:28'}
COMMANDS=('INFO','STATUS','METRICS','CONFIG GET','WIFI GET')

def port(name):
    found=[p.device for p in list_ports.comports() if (p.serial_number or '').upper()==IDENTITIES[name]]
    if len(found)!=1:raise RuntimeError('USB identity not found: '+name)
    return found[0]

def read(name,commands=COMMANDS):
    with serial.Serial(port(name),115200,timeout=.05,write_timeout=3,exclusive=True) as handle:
        client=Client(handle,4)
        return {cmd:client.request(cmd) for cmd in commands}

def direct_query(address,host):
    wire=struct.pack('!6H',0,0,1,0,0,0)+b''.join(bytes([len(p)])+p.encode('ascii') for p in host.split('.'))+b'\0'+struct.pack('!HH',1,0x8001)
    found=set()
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as handle:
        handle.bind(('',0));handle.settimeout(.1)
        for _ in range(2):
            handle.sendto(wire,(address,5353));deadline=time.monotonic()+1
            while time.monotonic()<deadline:
                try:
                    packet,source=handle.recvfrom(9000)
                    if source[0]==address:found.update(a_records(packet,host))
                except socket.timeout:pass
                except (ValueError,IndexError,struct.error,UnicodeError):pass
    return sorted(found)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--interface-ip',required=True)
    parser.add_argument('--run-dir',type=Path,required=True)
    parser.add_argument('--allow-upload',action='store_true',required=True)
    args=parser.parse_args()
    def interrupted(signum,frame):
        raise InterruptedError('Signal '+str(signum))
    signal.signal(signal.SIGTERM,interrupted)
    args.run_dir.resolve().relative_to((ROOT/'captures').resolve())
    manifest=json.loads((WORK/'manifest.json').read_text())
    for folder,key in [('restore','production_sha256'),('artifacts','probe_sha256')]:
        if hashlib.sha256((WORK/folder/'Axio.ino.bin').read_bytes()).hexdigest()!=manifest[key]:
            raise RuntimeError('Artifact changed: '+folder)
    args.run_dir.mkdir(exist_ok=False)
    report={'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'scope':'instrumented Axio; native mDNS RX partition only; production policy unchanged',
            'manifest':manifest,'before':{},'restored':{},'tests':{},'observations':[]}
    touched=[];clients={};handles={};packets=[];stage='preflight';stop=threading.Event();listener=None;thread=None
    lock=threading.Lock()
    packet_log=(args.run_dir/'mdns-packets.jsonl').open('x',buffering=1)
    def checkpoint():
        temporary=args.run_dir/'report.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n');temporary.replace(args.run_dir/'report.json')
    def upload(name,probe):
        directory=WORK/('artifacts' if probe else 'restore')
        sketch=WORK/'Axio' if probe else ROOT/'firmware/Axio'
        result=subprocess.run(['arduino-cli','upload','--fqbn','esp32:esp32:XIAO_ESP32C3','--board-options',OPTIONS,
            '--port',port(name),'--input-dir',str(directory),str(sketch)],capture_output=True,text=True,timeout=100)
        (args.run_dir/(name+('-probe' if probe else '-restore')+'-upload.log')).write_text(result.stdout+result.stderr)
        if result.returncode:raise RuntimeError('Upload failed: '+name)
        print(json.dumps({'uploaded':name,'probe':probe}),flush=True)
    def online(name,probe):
        deadline=time.monotonic()+120
        while time.monotonic()<deadline:
            try:
                value=read(name,COMMANDS+(('TEST MDNS STATUS',) if probe else ()))
                if value['STATUS']['wifi_connected']:
                    if probe and not value['TEST MDNS STATUS']['bound']:raise RuntimeError('RX wrapper not bound')
                    return value
            except (OSError,ProtocolError,RuntimeError):pass
            time.sleep(1)
        raise RuntimeError('STA/probe not ready: '+name)
    def observe():
        row={'stage':stage,'at':time.monotonic(),'devices':{}}
        for name,client in clients.items():
            row['devices'][name]={cmd:client.request(cmd) for cmd in ('STATUS','METRICS','TEST MDNS STATUS')}
        report['observations'].append(row)
        return row
    def capture():
        while not stop.is_set():
            try:packet,source=listener.recvfrom(9000)
            except socket.timeout:continue
            except OSError:break
            if source[0] not in ips.values():continue
            try:common=a_records(packet,'axio.local')
            except (ValueError,IndexError,struct.error,UnicodeError):continue
            row={'at':time.monotonic(),'stage':stage,'source':source[0],'common_a':common,'hex':packet.hex()}
            packet_log.write(json.dumps(row)+'\n')
            with lock:
                packets.append(row)
                if len(packets)>512:del packets[0]
    try:
        report['before']={name:read(name) for name in IDENTITIES};checkpoint()
        for name,value in report['before'].items():
            if not value['STATUS']['wifi_connected']:raise RuntimeError('Initial STA unavailable: '+name)
        if report['before']['primary']['STATUS']['imu']!='ready':raise RuntimeError('Primary IMU not ready')
        for name in IDENTITIES:
            touched.append(name);checkpoint();upload(name,True)
        report['probe_initial']={name:online(name,True) for name in IDENTITIES}
        ips={name:value['STATUS']['connectivity']['sta_ip'] for name,value in report['probe_initial'].items()}
        listener=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        listener.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEPORT,1)
        listener.bind(('',5353));listener.settimeout(.2)
        listener.setsockopt(socket.IPPROTO_IP,socket.IP_ADD_MEMBERSHIP,socket.inet_aton('224.0.0.251')+socket.inet_aton(args.interface_ip))
        thread=threading.Thread(target=capture);thread.start()
        for name in IDENTITIES:
            handles[name]=serial.Serial(port(name),115200,timeout=.05,write_timeout=3,exclusive=True)
            clients[name]=Client(handles[name],4)
        stage='isolated'
        report['hold']={name:client.request('TEST MDNS HOLD') for name,client in clients.items()}
        print(json.dumps({'stage':stage,'automatic_release_seconds':60}),flush=True)
        held=time.monotonic();deadline=held+40;dual_since=None
        while time.monotonic()<deadline:
            row=observe()
            both=all(v['STATUS']['connectivity']['discovery_details']['entry_owned'] and
                     v['TEST MDNS STATUS']['native']=='axio' and v['TEST MDNS STATUS']['remaining_ms']>5000
                     for v in row['devices'].values())
            if both:
                if dual_since is None:dual_since=row['at'];report['first_dual']=row;checkpoint()
                with lock:
                    seen={p['source'] for p in packets if p['at']>=dual_since and p['stage']=='isolated' and p['source'] in p['common_a']}
                if row['at']-dual_since>=8 and seen==set(ips.values()):break
            else:dual_since=None
            time.sleep(.5)
        else:raise RuntimeError('Could not establish two native leaders with A records from both')
        report['tests']['two_established_native_leaders']=True
        report['tests']['both_publish_common_a_while_isolated']=True
        report['dual_before_join']=row
        print(json.dumps({'stage':'two_native_leaders_verified','isolated_seconds':row['at']-held}),flush=True)
        stage='rejoined';joined=time.monotonic();report['join_at']=joined
        report['join']={}
        for name,client in clients.items():
            sent=time.monotonic();reply=client.request('TEST MDNS JOIN')
            report['join'][name]={'sent':sent,'reply':reply}
        deadline=time.monotonic()+120;stable=None
        while time.monotonic()<deadline:
            row=observe();owners=[name for name,v in row['devices'].items() if v['STATUS']['connectivity']['discovery_details']['entry_owned']]
            with lock:
                recent={ip for p in packets if p['at']>=max(joined,row['at']-15) for ip in p['common_a']}
            ready=len(owners)==1 and all(v['STATUS']['connectivity']['discovery_details']['peers']==1 for v in row['devices'].values()) and recent=={ips[owners[0]]}
            if ready:
                if stable is None:stable=row['at']
                if row['at']-stable>=12:break
            else:stable=None
            time.sleep(.5)
        else:raise RuntimeError('Two-leader reconciliation timeout')
        report['convergence_seconds']=stable-joined;report['final_probe']=row
        print(json.dumps({'stage':'converged','seconds':report['convergence_seconds'],'owner':owners[0]}),flush=True)
        report['tests']['single_owner_stable_with_reciprocal_discovery']=True
        report['tests']['native_conflict_detected']=any(v['STATUS']['connectivity']['discovery_details']['entry_conflicts']>report['dual_before_join']['devices'][name]['STATUS']['connectivity']['discovery_details']['entry_conflicts'] for name,v in row['devices'].items())
        report['tests']['rx_dropped_while_isolated']=all(v['TEST MDNS STATUS']['dropped']>report['hold'][name]['dropped'] for name,v in report['dual_before_join']['devices'].items())
        report['tests']['rx_resumed_after_join']=all(v['TEST MDNS STATUS']['forwarded']>report['dual_before_join']['devices'][name]['TEST MDNS STATUS']['forwarded'] for name,v in row['devices'].items())
        report['imu_window']=rates(report['probe_initial']['primary']['METRICS'],row['devices']['primary']['METRICS'])
        report['tests']['no_imu_read_errors']=report['imu_window']['read_errors_delta']==0
        report['tests']['osc_runtime_continues']=row['devices']['primary']['METRICS']['engine']['udp_queued']>report['probe_initial']['primary']['METRICS']['engine']['udp_queued']
        report['direct_dns']={name:direct_query(ip,'axio.local') for name,ip in ips.items()}
        report['tests']['direct_dns_single_owner']={ip for values in report['direct_dns'].values() for ip in values}=={ips[owners[0]]}
        report['individual_dns']={name:direct_query(ip,'axio-'+IDENTITIES[name].lower().replace(':','')+'.local') for name,ip in ips.items()}
        report['tests']['individual_names_preserved']=all(report['individual_dns'][name]==[ip] for name,ip in ips.items())
        report['http']={name:http_snapshot(ip,'axio.local' if name==owners[0] else ip)['status'] for name,ip in ips.items()}
        report['tests']['http_common_and_individual']=all(value==200 for value in report['http'].values())
    except (Exception,KeyboardInterrupt) as error:
        report['error']=repr(error);print('ERROR '+repr(error),flush=True)
    finally:
        signal.signal(signal.SIGTERM,signal.SIG_IGN)
        signal.signal(signal.SIGINT,signal.SIG_IGN)
        for handle in handles.values():handle.close()
        stop.set()
        if thread:thread.join(timeout=2)
        if listener:listener.close()
        packet_log.close();checkpoint()
        for name in touched:
            try:
                upload(name,False);report['restored'][name]=online(name,False)
                report['tests'][name+'_configuration_preserved']=all(report['restored'][name][cmd]==report['before'][name][cmd] for cmd in ('CONFIG GET','WIFI GET'))
                report['tests'][name+'_production_build_restored']=report['restored'][name]['INFO']['build']==report['before'][name]['INFO']['build']
                expected='ready' if name=='primary' else 'init_failed'
                report['tests'][name+'_imu_expected']=report['restored'][name]['STATUS']['imu']==expected
            except Exception as error:report['restored'][name]={'error':repr(error)}
            checkpoint()
        report['result']='PASS' if 'error' not in report and 'final_probe' in report and len(report['restored'])==2 and all('error' not in v for v in report['restored'].values()) and all(report['tests'].values()) else 'FAIL'
        report['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat();checkpoint()
        print(json.dumps({'result':report['result'],'tests':report['tests'],'error':report.get('error'),'convergence_seconds':report.get('convergence_seconds')}),flush=True)
    return int(report['result']!='PASS')

if __name__=='__main__':raise SystemExit(main())
