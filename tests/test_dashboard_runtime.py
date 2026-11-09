"""Real firmware HTTP adapter on host; fake radio/NVS/BNO, real TCP and JSON."""
import copy
import gzip
import http.client
import json
import subprocess
import time
import unittest
from support_dashboard_build import build

class DashboardHttpCase:
    @classmethod
    def setUpClass(cls):
        cls.binary=build()
    def setUp(self):
        self.start()
    def start(self,*args):
        self.process=subprocess.Popen([str(self.binary),*args],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertEqual(self.process.stdout.readline().strip(),'READY')
        self.token=self.get('/api/session')[1]['token']
    def tearDown(self):
        self.process.terminate()
        try:self.process.communicate(timeout=3)
        except subprocess.TimeoutExpired:self.process.kill();self.process.communicate()
    def request(self,method,path,body=None,headers=None):
        c=http.client.HTTPConnection('127.0.0.1',17884,timeout=3)
        h={'Host':'127.0.0.1:17884'}
        if body is not None:h.update({'Content-Type':'application/json','X-Axio-Token':self.token,'Origin':'http://127.0.0.1:17884'})
        if headers:h.update(headers)
        c.request(method,path,None if body is None else json.dumps(body,separators=(',',':')),h)
        r=c.getresponse();data=r.read();code=r.status
        if r.getheader('Content-Encoding')=='gzip':data=gzip.decompress(data)
        value=json.loads(data) if r.getheader('Content-Type')=='application/json' else data.decode()
        c.close();return code,value
    def get(self,path):return self.request('GET',path)
    def post(self,path,body,headers=None):return self.request('POST',path,body,headers)
    def snapshot(self):
        code,data=self.get('/api/snapshot');self.assertEqual(code,200,data);return data
    def radio(self,command):
        self.process.stdin.write(command+'\n');self.process.stdin.flush()
    def wait_snapshot(self,predicate,seconds=4):
        deadline=time.monotonic()+seconds;last=None
        while time.monotonic()<deadline:
            try:
                last=self.snapshot()
                if predicate(last):return last
            except (ConnectionError,OSError):pass
            time.sleep(.05)
        self.fail('Runtime transition timed out: '+repr(last))

class DashboardRuntimeTests(DashboardHttpCase,unittest.TestCase):
    def test_common_host_origin_and_token_only_accepted_by_current_owner(self):
        common={'Host':'axio.local:17884','Origin':'http://axio.local:17884'}
        self.assertEqual(self.request('GET','/api/session',headers=common)[0],403)
        self.radio('ENTRY_RUN')
        owner=self.wait_snapshot(lambda s:s['network']['entry_state']=='leader')
        self.assertEqual(owner['network']['entry_point'],'axio.local')
        self.assertEqual(owner['device']['hostname'],'axio-123456789abc.local')
        self.assertEqual(self.request('GET','/api/session',headers=common)[0],200)
        self.assertEqual(self.post('/api/osc/validate',owner['osc'],common)[0],200)
        self.assertEqual(self.post('/api/osc/validate',owner['osc'],dict(common,Origin='http://wrong.local'))[0],403)
        self.assertEqual(self.post('/api/osc/validate',owner['osc'],dict(common,**{'X-Axio-Token':'bad'}))[0],403)
        self.radio('ENTRY_CONFLICT')
        self.wait_snapshot(lambda s:s['network']['entry_state']!='leader')
        self.assertEqual(self.request('GET','/api/session',headers=common)[0],403)
        self.assertEqual(self.get('/api/session')[0],200)

    def test_old_osc_key_is_read_and_canonicalized_without_changing_endpoints(self):
        original=self.snapshot()['osc'];old=copy.deepcopy(original)
        del old['calibration_address'];old['tare_address']='/tare'
        old['ack']['address']='/tared/{id}'
        self.assertEqual(self.post('/api/osc/validate',old)[0],200)
        self.assertEqual(self.snapshot()['osc'],original)
        self.assertEqual(self.post('/api/osc/apply',old)[0],200)
        saved=self.snapshot()['osc']
        self.assertEqual(saved['calibration_address'],'/tare');self.assertNotIn('tare_address',saved)
        self.assertEqual(saved['ack']['address'],'/tared/{id}')
        ambiguous=dict(old,calibration_address='/other')
        self.assertEqual(self.post('/api/osc/apply',ambiguous)[0],400)
        self.assertEqual(self.snapshot()['osc'],saved)

    def test_assets_snapshot_sensor_battery_and_secret_redaction(self):
        code,html=self.get('/?page=motion');self.assertEqual(code,200);self.assertIn('Motion Data',html)
        self.assertIn("/api/",self.get('/app.js')[1]);self.assertIn('--purple',self.get('/style.css')[1])
        state=self.snapshot();self.assertEqual(state['device']['id'],'123456789abc');self.assertEqual(state['osc']['schema'],2)
        self.assertEqual(state['network']['networks'][0]['ssid_hex'],b'old-ssid'.hex())
        self.assertIsNone(state['network']['entry_point'])
        self.assertNotIn('secret-migration',json.dumps(state))
        code,sample=self.get('/api/sample');self.assertEqual(code,200);self.assertTrue(sample['valid']);self.assertLess(sample['axWorld'],-1)
        code,cached=self.get('/api/sample');self.assertEqual(code,200);self.assertEqual(cached['sequence'],sample['sequence'])
        time.sleep(1.1);battery=self.get('/api/battery')[1];self.assertAlmostEqual(battery['voltage'],3.9);self.assertIsNone(battery['percent'])
    def test_validate_apply_identity_and_rejected_global_id(self):
        state=self.snapshot();draft=copy.deepcopy(state['osc']);draft['editor']['interval_ms']=13
        draft['calibration_address']='/cal';draft['ack'].update(enabled=True,address='/calok')
        self.assertEqual(self.post('/api/osc/validate',draft)[0],200)
        self.assertEqual(self.snapshot()['osc']['editor']['interval_ms'],20)
        self.assertEqual(self.snapshot()['osc']['calibration_address'],state['osc']['calibration_address'])
        self.assertEqual(self.post('/api/osc/apply',draft)[0],200)
        self.assertEqual(self.snapshot()['osc']['editor']['interval_ms'],13)
        self.assertEqual(self.snapshot()['osc'],draft)
        draft['id']='other';self.assertEqual(self.post('/api/osc/apply',draft)[0],400)
        self.assertEqual(self.post('/api/identity',{'id':'custom'})[0],200)
        deadline=time.monotonic()+4
        while True:
            try:
                updated=self.snapshot()
                if updated['device']['ap_ssid']=='Axio-custom':break
                if time.monotonic()>=deadline:self.fail('Deferred identity activation did not complete')
                time.sleep(.05)
            except (ConnectionError,OSError):
                if time.monotonic()>=deadline:raise
                time.sleep(.05)
        self.assertEqual(updated['device']['id'],'custom')
        self.assertEqual(updated['device']['hostname'],state['device']['hostname'])
        self.assertEqual(updated['device']['ap_ssid'],'Axio-custom');self.assertEqual(updated['osc']['id'],'custom')
    def test_network_preserved_password_revision_validate_and_activation(self):
        before=self.snapshot();draft={'revision':before['network']['revision'],'networks':[{'original':0,'ssid_hex':b'renamed'.hex()}],'ap_password_hex':b'new-ap-password'.hex()}
        self.assertEqual(self.post('/api/network/validate',draft)[0],200)
        self.assertEqual(self.snapshot()['network']['networks'],before['network']['networks'])
        code,ack=self.post('/api/network/apply',draft);self.assertEqual(code,200,ack);self.assertTrue(ack['persistent'])
        time.sleep(.5);after=self.snapshot();self.assertEqual(after['network']['networks'][0]['ssid_hex'],b'renamed'.hex())
        self.assertEqual(self.post('/api/network/apply',draft)[1]['error'],'network_changed_reload')
        # C++ harness asserts that the original secret survives this rename.
        self.assertIsNone(self.process.poll())
    def test_security_confirmations_and_manual_metrics(self):
        self.assertEqual(self.post('/api/command',{'command':'AP CREDENTIALS'})[0],403)
        self.assertEqual(self.post('/api/command',{'command':'CALIBRATION'},{'X-Axio-Token':'bad'})[0],403)
        self.assertEqual(self.post('/api/command',{'command':'CALIBRATION'},{'Origin':'http://evil.test'})[0],403)
        self.assertEqual(self.get('/api/session')[0],200)
        self.assertIn('wake_cause',self.get('/api/power')[1])
        self.assertEqual(self.post('/api/control',{'action':'sleep','confirm':False})[0],400)
        before=self.snapshot();self.assertEqual(self.post('/api/command',{'command':'DIAG RESET'})[0],200)
        after=self.snapshot();self.assertEqual(after['osc'],before['osc']);self.assertEqual(after['network']['networks'],before['network']['networks'])
    def test_physical_button_suspends_sensor_without_network_or_browser(self):
        result=subprocess.run([str(self.binary),'--sleep-button-offline'],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('PASS: physical long press',result.stdout)

class DashboardApTests(DashboardHttpCase,unittest.TestCase):
    def setUp(self):self.start('--ap')

    def test_same_assets_all_tabs_apis_host_token_and_sta_ap_transitions(self):
        ap=self.snapshot();self.assertTrue(ap['network']['ap_active']);self.assertFalse(ap['network']['connected'])
        self.assertEqual(ap['device']['osc_status'],'Suspended');self.assertIsNone(ap['network']['gateway'])
        self.assertNotIn('secret-migration',json.dumps(ap))
        assets={path:self.get(path) for path in ('/','/style.css','/app.js')}
        for page in ('overview','motion','osc','network','diagnostics'):
            self.assertEqual(self.get('/?page='+page),assets['/'])
        self.assertTrue(self.get('/api/sample')[1]['valid'])
        self.assertEqual(self.get('/api/battery')[0],200)
        self.assertEqual(self.get('/api/power')[0],200)
        self.assertEqual(self.request('GET','/',headers={'Host':'wrong.local'})[0],403)
        self.assertEqual(self.request('GET','/',headers={'Host':ap['device']['hostname']})[0],403)
        config=copy.deepcopy(ap['osc'])
        self.assertEqual(self.post('/api/osc/validate',config,{'X-Axio-Token':'wrong'})[0],403)
        self.assertEqual(self.post('/api/osc/validate',config,{'Origin':'http://other.local'})[0],403)
        self.assertEqual(self.post('/api/osc/validate',config)[0],200)
        config['editor']['interval_ms']=13
        self.assertEqual(self.post('/api/osc/apply',config)[0],200)
        for command in ('CALIBRATION','TARE'):
            self.assertEqual(self.post('/api/command',{'command':command})[0],200)
        self.radio('FAIL_AP_STOP');self.radio('STA')
        still_ap=self.wait_snapshot(lambda s:s['network']['connected'])
        self.assertTrue(still_ap['network']['ap_active']);self.assertEqual(still_ap['device']['osc_status'],'Suspended')
        self.radio('OK_AP_STOP')
        sta=self.wait_snapshot(lambda s:not s['network']['ap_active'] and s['network']['connected'])
        self.assertEqual(sta['device']['osc_status'],'Active')
        self.assertEqual(sta['osc'],config)
        self.assertEqual(self.request('GET','/',headers={'Host':sta['device']['hostname']})[0],200)
        self.assertEqual({path:self.get(path) for path in assets},assets)
        self.radio('AP')
        back=self.wait_snapshot(lambda s:s['network']['ap_active'] and not s['network']['connected'])
        self.assertEqual(back['osc'],config);self.assertEqual(back['network']['networks'],ap['network']['networks'])
        self.assertEqual(self.get('/')[0],200)

    def test_provisioning_add_reorder_delete_validation_and_storage_failure(self):
        before=self.snapshot()
        rows=[{'ssid_hex':b'new-network'.hex(),'password_hex':b'new-password'.hex()},
              {'original':0,'ssid_hex':b'old-ssid'.hex()}]
        draft={'revision':before['network']['revision'],'networks':rows}
        self.assertEqual(self.post('/api/network/validate',draft)[0],200)
        self.assertEqual(self.snapshot()['network'],before['network'])
        invalid=copy.deepcopy(draft);invalid['networks'][0]['password_hex']=b'short'.hex()
        self.assertEqual(self.post('/api/network/apply',invalid)[0],400)
        self.radio('FAIL_NVS');time.sleep(.05)
        self.assertEqual(self.post('/api/network/apply',draft)[1]['error'],'persistence_failed')
        self.assertEqual(self.snapshot()['network']['networks'],before['network']['networks'])
        self.radio('OK_NVS');time.sleep(.05)
        code,ack=self.post('/api/network/apply',draft)
        self.assertEqual(code,200,ack);self.assertTrue(ack['persistent']);self.assertTrue(ack['activation_pending'])
        time.sleep(.25);self.radio('AP')
        changed=self.wait_snapshot(lambda s:len(s['network']['networks'])==2)
        self.assertEqual([n['ssid_hex'] for n in changed['network']['networks']],[b'new-network'.hex(),b'old-ssid'.hex()])
        self.assertEqual(changed['osc'],before['osc'])
        self.assertEqual(self.post('/api/network/apply',draft)[1]['error'],'network_changed_reload')
        saved=changed['network']['networks']
        reorder={'revision':changed['network']['revision'],'networks':[saved[1],saved[0]]}
        self.assertEqual(self.post('/api/network/apply',reorder)[0],200)
        time.sleep(.25);self.radio('AP')
        reordered=self.wait_snapshot(lambda s:s['network']['networks'][0]['ssid_hex']==b'old-ssid'.hex())
        remove={'revision':reordered['network']['revision'],'networks':[reordered['network']['networks'][0]]}
        self.assertEqual(self.post('/api/network/apply',remove)[0],200)
        time.sleep(.25);self.radio('STA')
        final=self.wait_snapshot(lambda s:s['network']['connected'] and not s['network']['ap_active'])
        self.assertEqual([n['ssid_hex'] for n in final['network']['networks']],[b'old-ssid'.hex()])
        self.assertEqual(final['osc'],before['osc'])
