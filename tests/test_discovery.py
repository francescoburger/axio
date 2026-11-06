"""3C policy/cache with a fake driver; runtime integration lives in test_connectivity."""
import os
from pathlib import Path
import shutil
import subprocess
import unittest
ROOT=Path(__file__).resolve().parents[1]

class DiscoveryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary=ROOT/'.build/tests/discovery';cls.binary.parent.mkdir(parents=True,exist_ok=True)
        p=subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-Wall','-Wextra','-Werror',
                          '-I',str(ROOT/'firmware/Axio'),str(ROOT/'tests/discovery_harness.cpp'),'-o',str(cls.binary)],capture_output=True,text=True)
        if p.returncode:raise RuntimeError(p.stderr)
    def case(self,name):
        p=subprocess.run([str(self.binary),name],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr);self.assertEqual(p.stdout.strip(),'PASS')
    def test_mac_identity_and_conflict_suffix(self):self.case('names')
    def test_peer_refresh_ttl_goodbye_self_invalid_rollover(self):self.case('cache')
    def test_peer_capacity_and_clear(self):self.case('capacity')
    def test_connection_loss_pending_query_change_ip_and_port(self):self.case('lifecycle')
    def test_async_query_pacing_no_busy_wait_no_catchup(self):self.case('pacing')
    def test_start_query_and_service_failures(self):self.case('failures')
    def test_only_zero_timeout_async_queries_in_runtime(self):
        source=(ROOT/'firmware/Axio/src/connectivity/DiscoveryRuntime.cpp').read_text()
        self.assertIn('mdns_query_async_get_results(search,0,',source)
        self.assertNotIn('mdns_query_ptr(',source);self.assertNotIn('queryService(',source)
        self.assertIn('mdns_unregister_netif(ap)',source)
if __name__=='__main__':unittest.main()
