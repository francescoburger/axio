#!/usr/bin/env python3
"""Read-only USB/HTTP Phase 4 check. No CALIBRATION, Apply, Restart, Sleep or metric reset."""
import argparse
import gzip
import http.client
import json
from pathlib import Path
import time
from urllib.parse import urlsplit
from hardware_test import Client,rates
from osc_config import normalize_calibration

def main():
 import serial
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--port',required=True)
 parser.add_argument('--before',type=Path,required=True)
 parser.add_argument('--report',type=Path,required=True)
 parser.add_argument('--seconds',type=float,default=20)
 args=parser.parse_args()
 before=json.loads(args.before.read_text());report={'scope':'physical device, read-only USB and HTTP','checks':[]}
 def check(name,passed,detail=None):
  report['checks'].append({'name':name,'result':'PASS' if passed else 'FAIL','detail':detail})
 with serial.Serial(args.port,115200,timeout=.05,write_timeout=2,exclusive=True) as port:
  client=Client(port,timeout=3)
  deadline=time.monotonic()+25
  while True:
   try:
    info=client.request('INFO');status=client.request('STATUS')
    if status.get('wifi_connected'):break
   except Exception:
    if time.monotonic()>=deadline:raise
   if time.monotonic()>=deadline:raise RuntimeError('STA unavailable')
   time.sleep(.2)
  report['info']=info;report['initial_status']=status
  check('firmware_4',info['version'].startswith('4.'))
  networks=client.request('WIFI GET');config=normalize_calibration(client.request('CONFIG GET'))
  before['osc']=normalize_calibration(before['osc'])
  check('saved_networks_preserved',networks==before['networks'])
  check('individual_hostname_preserved',status['connectivity']['mdns_hostname']==before['status']['connectivity']['mdns_hostname'])
  for key in ('id','listen_port','destinations','calibration_address','ack'):
   check('osc_'+key+'_preserved',config[key]==before['osc'][key])
  check('single_editor_schema',config['schema']==2 and 'editor' in config)
  host=status['connectivity']['sta_ip']
  def get(path):
   conn=http.client.HTTPConnection(host,80,timeout=3)
   conn.request('GET',path,headers={'Host':host})
   response=conn.getresponse();data=response.read();code=response.status
   if response.getheader('Content-Encoding')=='gzip':data=gzip.decompress(data)
   result=json.loads(data) if response.getheader('Content-Type')=='application/json' else data.decode()
   conn.close();return code,result
  for path in ('/','/style.css','/app.js','/api/session','/api/snapshot','/api/battery'):
   code,data=get(path);check('http_'+path,code==200)
   if path=='/api/snapshot':report['snapshot_before']=data
   if path=='/api/battery':report['battery']=data
  def window(stream,seconds):
   start=client.request('METRICS');end=time.monotonic()+seconds;next_read=time.monotonic();samples=0;errors=[];maximum=0
   while time.monotonic()<end:
    if stream:
     began=time.monotonic()
     try:
      code,row=get('/api/sample')
      if code!=200 or not row.get('valid'):errors.append({'code':code,'sensor':row.get('sensor')})
      else:samples+=1
     except Exception as exc:errors.append(str(exc))
     maximum=max(maximum,time.monotonic()-began);next_read+=.1
     time.sleep(max(0,next_read-time.monotonic()))
    else:time.sleep(min(.1,max(0,end-time.monotonic())))
   finish=client.request('METRICS')
   udp_seconds=(finish['engine']['window_us']-start['engine']['window_us'])/1e6
   udp={'seconds':udp_seconds,'queued_hz':(finish['engine']['udp_queued']-start['engine']['udp_queued'])/udp_seconds,'error_delta':finish['engine']['udp_errors']-start['engine']['udp_errors']}
   return {'udp':udp,'usb_rates':rates(start,finish),'before':start,'after':finish,'http_valid_samples':samples,'http_errors':errors,'http_max_seconds':maximum}
  report['idle']=window(False,5)
  report['web_telemetry']=window(True,args.seconds)
  check('telemetry_no_http_errors',not report['web_telemetry']['http_errors'])
  check('no_imu_read_errors_in_windows',all(report[k]['usb_rates']['read_errors_delta']==0 for k in ('idle','web_telemetry')))
  report['snapshot_after']=get('/api/snapshot')[1]
  report['final_status']=client.request('STATUS')
  check('networks_still_preserved',client.request('WIFI GET')==before['networks'])
 args.report.parent.mkdir(parents=True,exist_ok=True)
 with args.report.open('x') as output:json.dump(report,output,indent=2,allow_nan=False)
 print(json.dumps({'checks':report['checks'],'idle':report['idle']['usb_rates'],'web_telemetry':report['web_telemetry']['usb_rates'],'http_valid_samples':report['web_telemetry']['http_valid_samples'],'battery':report['battery']},indent=2))
 return int(any(c['result']=='FAIL' for c in report['checks']))
if __name__=='__main__':raise SystemExit(main())
