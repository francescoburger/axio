"""3B policy, secure storage and bounded HTTP protocol; no device is touched."""
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import unittest
from urllib.parse import urlencode

ROOT=Path(__file__).resolve().parents[1]
TOKEN='0123456789abcdef0123456789abcdef'

class ApTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build=ROOT/'.build/tests';cls.build.mkdir(parents=True,exist_ok=True)
        for name in ('ap','provisioning_http','provisioning_socket'):
            p=subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-Wall','-Wextra','-Werror',
                              '-I',str(ROOT/'firmware/Axio'),str(ROOT/f'tests/{name}_harness.cpp'),
                              '-o',str(cls.build/name)],capture_output=True,text=True)
            if p.returncode:raise RuntimeError(p.stderr)

    def case(self,name):
        p=subprocess.run([str(self.build/'ap'),name],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr);self.assertEqual(p.stdout.strip(),'PASS')

    def test_fixed_password_migration_reboot_corruption_explicit_restore(self):self.case('credentials')
    def test_secret_storage_unavailable_write_and_readback_fail_closed(self):self.case('failures')
    def test_fallback_15s_rollover_sta_shutdown_and_stop_retry(self):self.case('lifecycle')
    def test_ap_failed_start_timeout_bounded_retry_no_open_fallback(self):self.case('retry')

    def request(self,raw,option=None):
        p=subprocess.run([str(self.build/'provisioning_http')]+([option] if option else []),input=raw,capture_output=True)
        self.assertEqual(p.returncode,0,p.stderr.decode());return p.stdout,p.stderr.decode()

    def form(self,**overrides):
        fields=dict(ssid='Lab & room',password='private-password',priority='9',action='save',token=TOKEN)
        fields.update(overrides);return urlencode(fields)

    def post(self,body,extra='',host='192.168.4.1'):
        return (f'POST /network HTTP/1.1\r\nHost: {host}\r\nContent-Type: application/x-www-form-urlencoded\r\n'
                f'Content-Length: {len(body.encode())}\r\n{extra}\r\n{body}').encode()

    def assert_response(self,response,code):
        header,body=response.split(b'\r\n\r\n',1)
        self.assertTrue(header.startswith(f'HTTP/1.1 {code} '.encode()),response)
        length=int(next(s.split(b': ',1)[1] for s in header.split(b'\r\n') if s.startswith(b'Content-Length:')))
        self.assertEqual(len(body),length);self.assertIn(b'Cache-Control: no-store',header)
        return body

    def test_http_minimal_form_status_and_networks(self):
        for path in ('/','/status','/networks'):
            response,counts=self.request(f'GET {path} HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n'.encode())
            body=self.assert_response(response,200);self.assertEqual(counts,'updates=0')
            self.assertNotIn(b'private-password',response)
            if path=='/':self.assertIn(TOKEN.encode(),body);self.assertIn(b'type=password',body)
            else:json.loads(body)

    def test_http_valid_urlencoded_post_and_store_failure(self):
        raw=self.post(self.form(),extra='Origin: http://192.168.4.1\r\n')
        response,counts=self.request(raw);self.assert_response(response,200);self.assertEqual(counts,'updates=1')
        self.assertNotIn(b'private-password',response)
        response,counts=self.request(raw,'--fail-store');self.assert_response(response,400)
        self.assertIn(b'persistence_failed',response);self.assertEqual(counts,'updates=0')
        response,counts=self.request(self.post(self.form(action='delete')));self.assert_response(response,200)

    def test_http_csrf_and_host_checks(self):
        cases=[self.post(self.form(token='bad')),self.post(self.form(),host='evil.example'),
               self.post(self.form(),extra='Origin: http://evil.example\r\n'),
               self.post(self.form(),extra='Host: 192.168.4.1\r\n')]
        for raw in cases:
            with self.subTest(raw=raw[:80]):
                response,counts=self.request(raw);self.assert_response(response,403);self.assertEqual(counts,'updates=0')

    def test_http_bounded_lengths_duplicates_encoding_and_invalid_network(self):
        cases=[self.post(self.form(priority='256')),self.post(self.form(ssid='x'*33)),
               self.post(self.form(password='short')),self.post(self.form()+'&token='+TOKEN),
               self.post(self.form().replace('Lab+%26+room','%00')),
               self.post(self.form().replace('Lab+%26+room','%ZZ')),
               self.post(self.form(),extra='Content-Length: 1\r\n'),
               self.post(self.form(),extra='Transfer-Encoding: chunked\r\n')]
        for raw in cases:
            response,counts=self.request(raw);self.assert_response(response,400);self.assertEqual(counts,'updates=0')
        response,_=self.request(self.post('x'*513));self.assert_response(response,413)
        response,_=self.request(b'GET / HTTP/1.1\r\nHost: 192.168.4.1\r\nX: '+b'x'*1100);self.assert_response(response,431)

    def test_http_stalled_rx_tx_absolute_timeout_rollover(self):
        raw=b'GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n'
        for option in ('--stall-rx','--stall-tx'):
            response,counts=self.request(raw,option);self.assertEqual(response,b'');self.assertEqual(counts,'updates=0')
        response,counts=self.request(b'POST /network HTTP/1.1\r\nHost: 192.168.4.1\r\n')
        self.assertEqual(response,b'');self.assertEqual(counts,'updates=0')

    def test_real_nonblocking_http_socket_on_host_loopback(self):
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1',0));port=reservation.getsockname()[1]
        process=subprocess.Popen([str(self.build/'provisioning_socket'),str(port)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        try:
            self.assertEqual(process.stdout.readline(),b'READY\n')
            for path in ('/','/status','/networks'):
                with socket.create_connection(('127.0.0.1',port),timeout=2) as client:
                    client.sendall(f'GET {path} HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n'.encode())
                    response=b''
                    while True:
                        data=client.recv(4096)
                        if not data:break
                        response+=data
                    body=self.assert_response(response,200)
                    if path!='/':json.loads(body)
        finally:
            process.terminate();process.communicate(timeout=2)

if __name__=='__main__':unittest.main()
