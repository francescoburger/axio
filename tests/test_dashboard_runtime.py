"""Real firmware HTTP adapter on host; fake radio/NVS/BNO, real TCP and JSON."""
import copy
import gzip
import http.client
import json
import subprocess
import time
import unittest
from support_dashboard_build import build

class DashboardRuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary=build()
    def setUp(self):
        self.process=subprocess.Popen([str(self.binary)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
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
        self.assertEqual(self.post('/api/osc/validate',draft)[0],200)
        self.assertEqual(self.snapshot()['osc']['editor']['interval_ms'],20)
        self.assertEqual(self.post('/api/osc/apply',draft)[0],200)
        self.assertEqual(self.snapshot()['osc']['editor']['interval_ms'],13)
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
        self.assertEqual(self.post('/api/command',{'command':'TARE'},{'X-Axio-Token':'bad'})[0],403)
        self.assertEqual(self.post('/api/command',{'command':'TARE'},{'Origin':'http://evil.test'})[0],403)
        self.assertEqual(self.get('/api/session')[0],200)
        self.assertIn('wake_cause',self.get('/api/power')[1])
        self.assertEqual(self.post('/api/control',{'action':'sleep','confirm':False})[0],400)
        before=self.snapshot();self.assertEqual(self.post('/api/command',{'command':'DIAG RESET'})[0],200)
        after=self.snapshot();self.assertEqual(after['osc'],before['osc']);self.assertEqual(after['network']['networks'],before['network']['networks'])
    def test_physical_button_suspends_sensor_without_network_or_browser(self):
        result=subprocess.run([str(self.binary),'--sleep-button-offline'],capture_output=True,text=True,timeout=5)
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('PASS: physical long press',result.stdout)
