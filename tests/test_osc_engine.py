"""Production OSC engine, configuration and NVS journal exercised without XIAO."""
import copy
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from osc_receiver import decode_packet
from osc_config import preset, normalize_calibration
from osc_benchmark import scenarios
from osc_hardware_test import check_message, osc_string


class EngineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary=ROOT/'.build/tests/osc_engine'
        cls.binary.parent.mkdir(parents=True,exist_ok=True)
        library=Path(os.environ.get('BNO055_LIBRARY',str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-Wall','-Wextra','-Werror',
                        '-I',str(library),'-I',str(ROOT/'firmware/Axio'),str(ROOT/'tests/osc_engine_harness.cpp'),
                        '-o',str(cls.binary)],check=True,capture_output=True,text=True)
        cls.runtime=ROOT/'.build/tests/osc_runtime'
        subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-Wall','-Wextra','-Werror',
                        '-I',str(ROOT/'tests/stubs/runtime'),'-I',str(library),'-I',str(ROOT/'firmware/Axio'),
                        str(ROOT/'tests/osc_runtime_harness.cpp'),str(ROOT/'firmware/Axio/src/transport/OscRuntime.cpp'),
                        '-o',str(cls.runtime)],check=True,capture_output=True,text=True)

    def validate(self,config):
        wire=config if isinstance(config,str) else json.dumps(config,separators=(',',':'))
        return subprocess.run([str(self.binary),'--validate'],input=wire,text=True,capture_output=True)

    def test_presets_and_roundtrip(self):
        for name in ('Orientation','Motion','Custom'):
            with self.subTest(name=name):
                row=json.loads(subprocess.check_output([str(self.binary),'--preset',name]))
                p=self.validate(row)
                self.assertEqual(p.returncode,0,p.stdout)
                self.assertEqual(json.loads(p.stdout),row)
                self.assertEqual(row,preset(name))
                p=self.validate(preset(name))
                self.assertEqual(p.returncode,0,p.stdout)

    def test_old_calibration_key_endpoints_and_ambiguous_keys(self):
        for current in (preset(),self.modern()):
            old=copy.deepcopy(current)
            del old['calibration_address'];old['tare_address']='/custom-zero/{id}'
            old['ack']['address']='/custom-ack/{id}'
            result=self.validate(old)
            self.assertEqual(result.returncode,0,result.stdout)
            expected=normalize_calibration(old)
            self.assertEqual(json.loads(result.stdout),expected)
            self.assertEqual(old['tare_address'],'/custom-zero/{id}')
            for value in ('/custom-zero/{id}','/other'):
                ambiguous=dict(old,calibration_address=value)
                self.assertNotEqual(self.validate(ambiguous).returncode,0)
                with self.assertRaises(ValueError):normalize_calibration(ambiguous)
            wire=json.dumps(old,separators=(',',':'))
            commands=['AXIO 1 CONFIG APPLY '+wire,'@REBOOT','AXIO 2 CONFIG GET']
            rows=[json.loads(line) for line in subprocess.check_output(
                [str(self.runtime)],input='\n'.join(commands)+'\n',text=True).splitlines()]
            self.assertTrue(all(r['ok'] for r in rows),rows)
            self.assertEqual(rows[-1]['data'],expected)
    def modern(self):
        row=preset('Motion')
        row['schema']=2
        del row['preset']
        for m in row['messages']:
            m['interval_ms']=13
            for key in ('hz','bundle','destinations'): del m[key]
        return row

    def test_interval_ms_auto_bundle_migration_and_atomic_upgrade(self):
        output=subprocess.check_output([str(self.binary),'--phase4'],text=True)
        self.assertEqual(output.strip(),'PASS phase4')

    def test_calibration_alias_ack_conflicts_and_old_journal_bytes(self):
        output=subprocess.check_output([str(self.binary),'--calibration'],text=True)
        self.assertEqual(output.strip(),'PASS calibration compatibility')

    def test_pure_data_numeric_command_through_runtime_udp_and_acknowledgement(self):
        output=subprocess.check_output([str(self.runtime),'--pd-calibration'],text=True)
        self.assertIn('PASS: Pure Data numeric calibration',output)

    def test_schema2_roundtrip_strict_validation(self):
        row=self.modern()
        result=self.validate(row)
        self.assertEqual(result.returncode,0,result.stdout)
        self.assertEqual(json.loads(result.stdout),row)
        for value in (0,9,65536,-1,10.5,'13',True,None):
            bad=copy.deepcopy(row);bad['messages'][0]['interval_ms']=value
            with self.subTest(value=value):self.assertNotEqual(self.validate(bad).returncode,0)
        for key,value in [('hz',100),('bundle',True),('destinations',[0])]:
            bad=copy.deepcopy(row);bad['messages'][0][key]=value
            with self.subTest(key=key):self.assertNotEqual(self.validate(bad).returncode,0)
        bad=copy.deepcopy(row);bad['preset']='Motion'
        self.assertNotEqual(self.validate(bad).returncode,0)
        for interval in (10,11,13,30,65535):
            row['messages'][0]['interval_ms']=interval
            self.assertEqual(self.validate(row).returncode,0)
        # Schema 2 must budget all messages together, regardless of legacy bundle flags.
        row['messages']=[dict(address='/large/'+str(i),interval_ms=10,
                             fields=['q0']*16) for i in range(16)]
        self.assertEqual(self.validate(row).stdout.strip(),'packet_too_large')

    def test_single_editor_schema_roundtrip_and_group_validation(self):
        row=self.modern();del row['messages']
        row['editor']=dict(main_address='/motion/{id}',interval_ms=13,
                           suffixes=['','real','world','gyro',''],selected=[7,7,7,7,15])
        result=self.validate(row)
        self.assertEqual(result.returncode,0,result.stdout)
        self.assertEqual(json.loads(result.stdout),row)
        for key,value in [('selected',[0,0,0,0,0]),('selected',[8,7,7,7,15]),
                          ('suffixes',['/bad','','','','']),('interval_ms',9)]:
            invalid=copy.deepcopy(row);invalid['editor'][key]=value
            with self.subTest(key=key,value=value):self.assertNotEqual(self.validate(invalid).returncode,0)
        for fields in ('main_address','suffixes','selected','interval_ms'):
            invalid=copy.deepcopy(row);del invalid['editor'][fields]
            self.assertNotEqual(self.validate(invalid).returncode,0)

    def test_schema2_runtime_validate_apply_and_reboot(self):
        row=self.modern();wire=json.dumps(row,separators=(',',':'))
        commands=['AXIO 1 CONFIG GET','AXIO 2 CONFIG VALIDATE '+wire,
                  'AXIO 3 CONFIG GET','AXIO 4 CONFIG APPLY '+wire,
                  '@REBOOT','AXIO 5 CONFIG GET']
        rows=[json.loads(line) for line in subprocess.check_output(
            [str(self.runtime)],input='\n'.join(commands)+'\n',text=True).splitlines()]
        self.assertTrue(all(r['ok'] for r in rows),rows)
        self.assertEqual(rows[0]['data'],rows[2]['data'])
        self.assertEqual(rows[-1]['data'],row)

    def test_invalid_configuration(self):
        original=preset()
        variants=[]
        def changed(key,value):
            row=copy.deepcopy(original);row[key]=value;variants.append(row)
        for schema in (0,2,'1',None):changed('schema',schema)
        for ident in ('','a'*17,'bad/id','bad"id'):changed('id',ident)
        for port in (0,-1,65536,1.5):changed('listen_port',port)
        for address in ('invalid','/bad/{other}','/bad/*','/bad//x','/','/bad/'):changed('calibration_address',address)
        changed('unknown',True)
        changed('destinations',[])
        changed('destinations',[original['destinations'][0]]*2)
        changed('messages',[original['messages'][0]]*2)
        for hz in (0,-1,101,1.5,'100'):
            row=copy.deepcopy(original);row['messages'][0]['hz']=hz;variants.append(row)
        for ip in ('0.0.0.0','255.255.255.255','224.0.0.1','256.1.2.3','hostname','127.1.1.1x'):
            row=copy.deepcopy(original);row['destinations'][0]['ip']=ip;variants.append(row)
        for fields in ([],['unknown'],['yaw']*17,[1]):
            row=copy.deepcopy(original);row['messages'][0]['fields']=fields;variants.append(row)
        for routes in ([],[0,0],[1],[-1],[8]):
            row=copy.deepcopy(original);row['messages'][0]['destinations']=routes;variants.append(row)
        for row in variants:
            with self.subTest(row=row):self.assertNotEqual(self.validate(row).returncode,0)
        for text in ('{}','{"schema":1,"schema":1}','{bad}',json.dumps(original)[:-1]+',"schema":1}'):
            self.assertNotEqual(self.validate(text).returncode,0)

    def test_resource_and_packet_limits(self):
        for scenario in scenarios('127.0.0.1'):
            result=self.validate(scenario['config'])
            self.assertEqual(result.returncode,0,(scenario['name'],result.stdout))
        row=preset('Custom');row['messages']=[dict(address='/large/'+str(i),hz=100,bundle=True,
             fields=['q0']*16,destinations=[0]) for i in range(16)]
        self.assertEqual(self.validate(row).stdout.strip(),'packet_too_large')
        row=preset();row['messages']*=9
        self.assertNotEqual(self.validate(row).returncode,0)
        self.assertEqual(self.validate(' '*7169+'{}').stdout.strip(),'config_too_large')

    def test_engine_scheduling_routing_calibration_and_persistence(self):
        p=subprocess.run([str(self.binary)],check=True,capture_output=True,text=True)
        self.assertEqual(p.stdout.strip(),'PASS')

    def packets(self):
        return [bytes.fromhex(line) for line in subprocess.check_output([str(self.binary),'--packets'],text=True).splitlines()]

    def test_production_bytes_bundle_fields_and_routes(self):
        packets=self.packets()
        self.assertEqual(len(packets),6)
        self.assertEqual(packets[0],packets[1])
        rows=decode_packet(packets[0])
        self.assertEqual([r['address'] for r in rows],['/motion/linear/0','/motion/world/0','/motion/gyro/0'])
        self.assertEqual([r['types'] for r in rows],[',fff']*3)
        self.assertEqual(rows[0]['values'],[.25,0,0])
        self.assertEqual(rows[1]['values'],[0,-2,0]) # historical out-of-nominal values preserved
        self.assertEqual(rows[2]['values'],[0,0,3])
        self.assertTrue(all(r['bundle_timetags']==[1] for r in rows))

    def test_production_osc_localhost(self):
        with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as receiver, socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as sender:
            receiver.bind(('127.0.0.1',0));receiver.settimeout(2)
            payload=self.packets()[0]
            sender.sendto(payload,receiver.getsockname())
            actual,_=receiver.recvfrom(2048)
            self.assertEqual(actual,payload)
            self.assertEqual(len(decode_packet(actual)),3)

    def test_runtime_serial_apply_reboot_and_metrics(self):
        config=preset('Motion','127.0.0.1',16900,'runtime')
        wire=json.dumps(config,separators=(',',':'))
        commands=['AXIO 1 CONFIG GET','AXIO 2 CONFIG VALIDATE '+wire,'AXIO 3 CONFIG APPLY '+wire,
                  '@REBOOT','AXIO 4 CONFIG GET','AXIO 5 METRICS','AXIO 6 INFO','AXIO 7 DIAG RESET']
        output=subprocess.check_output([str(self.runtime)],input='\n'.join(commands)+'\n',text=True)
        rows=[json.loads(line) for line in output.splitlines()]
        self.assertTrue(all(r['ok'] for r in rows),rows)
        self.assertEqual(rows[0]['data']['schema'],1)
        self.assertTrue(rows[1]['data']['valid'])
        self.assertTrue(rows[2]['data']['persistent'])
        self.assertEqual(rows[3]['data'],config)
        self.assertEqual(rows[4]['data']['engine']['config_source'],'saved')
        self.assertEqual(len(rows[4]['data']['engine']['messages']),3)
        self.assertTrue(rows[5]['data']['config_engine'])

    def test_runtime_maximum_configuration_jsonl(self):
        config=list(scenarios('127.0.0.1'))[-1]['config']
        wire=json.dumps(config,separators=(',',':'))
        self.assertGreater(len(wire),1024)
        commands=['AXIO 1 CONFIG APPLY '+wire,'AXIO 2 CONFIG GET','AXIO 3 METRICS']
        output=subprocess.check_output([str(self.runtime)],input='\n'.join(commands)+'\n',text=True)
        rows=[json.loads(line) for line in output.splitlines()]
        self.assertTrue(all(r['ok'] for r in rows),rows)
        self.assertEqual(rows[1]['data'],config)
        self.assertEqual(len(rows[2]['data']['engine']['messages']),16)
        self.assertIsNone(rows[2]['data']['osc_requested_hz'])

    def test_runtime_failed_apply_preserves_active_configuration(self):
        config=preset();config['listen_port']=12345;config['id']='failed'
        for switch, error in (('--fail-nvs','persistence_failed'),('--fail-listener','listen_failed')):
            commands=['AXIO 1 CONFIG GET','AXIO 2 CONFIG APPLY '+json.dumps(config,separators=(',',':')),
                      'AXIO 3 CONFIG GET','@REBOOT','AXIO 4 CONFIG GET']
            output=subprocess.check_output([str(self.runtime),switch],input='\n'.join(commands)+'\n',text=True)
            rows=[json.loads(line) for line in output.splitlines()]
            self.assertEqual(rows[1]['error'],error)
            self.assertEqual(rows[0]['data'],rows[2]['data'])
            self.assertEqual(rows[0]['data'],rows[3]['data'])

    def test_hardware_receiver_contract(self):
        definition=dict(address='/motion/{id}',fields=['yaw','pitch','roll'],bundle=True)
        row=dict(address='/motion/3',types=',fff',values=[0,-2,3],bundle_timetags=[1])
        check_message(row,definition,'3')
        for key,value in [('address','/other'),('types',',ii'),('bundle_timetags',[]),('values',[float('nan')])]:
            invalid=dict(row);invalid[key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):check_message(invalid,definition,'3')
        self.assertEqual(osc_string('/calibration'),b'/calibration\0\0\0\0')
        matrix=list(scenarios('127.0.0.1'))
        self.assertEqual(len(matrix),36)
        self.assertTrue(all(s['result']=='NOT RUN' for s in matrix))


if __name__=='__main__':
    unittest.main()
