"""Checkpoint 3A: real production policy/journal with a synthetic clock and faults."""
from pathlib import Path
import json
import os
import shutil
import sys
import subprocess
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))

class ConnectivityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.binary=ROOT/'.build/tests/connectivity'
        cls.binary.parent.mkdir(parents=True,exist_ok=True)
        p=subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-Wall','-Wextra','-Werror',
                          '-I',str(ROOT/'firmware/Axio'),str(ROOT/'tests/connectivity_harness.cpp'),'-o',str(cls.binary)],capture_output=True,text=True)
        if p.returncode:raise RuntimeError(p.stderr)
        cls.runtime=ROOT/'.build/tests/connectivity_runtime'
        library=Path(os.environ.get('BNO055_LIBRARY',str(Path.home()/'Documents/Arduino/libraries/Adafruit_BNO055')))
        p=subprocess.run([shutil.which(os.environ.get('CXX','c++')),'-std=c++11','-Wall','-Wextra','-Werror',
                          '-I',str(ROOT/'tests/stubs/runtime'),'-I',str(library),'-I',str(ROOT/'firmware/Axio'),
                          str(ROOT/'tests/connectivity_runtime_harness.cpp'),
                          str(ROOT/'firmware/Axio/src/connectivity/ConnectivityRuntime.cpp'),
                          str(ROOT/'firmware/Axio/src/transport/OscRuntime.cpp'),str(ROOT/'firmware/Axio/src/connectivity/ApRuntime.cpp'),str(ROOT/'firmware/Axio/src/connectivity/DiscoveryRuntime.cpp'),'-o',str(cls.runtime)],capture_output=True,text=True)
        if p.returncode:raise RuntimeError(p.stderr)

    def run_case(self,name):
        p=subprocess.run([str(self.binary),name],capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertEqual(p.stdout.strip(),'PASS')

    def test_priority_duplicates_and_stable_ties(self):self.run_case('priority')
    def test_timeout_backoff_rollover_no_catchup(self):self.run_case('timing')
    def test_connection_loss_and_recovery(self):self.run_case('reconnect')
    def test_invalid_credentials_and_capacity(self):self.run_case('invalid')
    def test_nvs_write_commit_readback_corruption_and_reboot(self):self.run_case('persistence')
    def test_migration_failure_sdk_preserved_empty_saved_list(self):self.run_case('migration')

    def commands(self,commands):
        p=subprocess.run([str(self.runtime)],input='\n'.join(commands)+'\n',capture_output=True,text=True)
        self.assertEqual(p.returncode,0,p.stderr)
        self.assertNotIn('secret-migration',p.stdout)
        return [json.loads(s) for s in p.stdout.splitlines()]

    def test_usb_network_configuration_and_secret_redaction(self):
        ssid='room "x"'.encode().hex();password='another-secret'.encode().hex()
        rows=self.commands(['AXIO 1 WIFI GET',f'AXIO 2 WIFI SET 255 {ssid} {password}',
                            f'AXIO 3 WIFI SET 2 {ssid} {password}','AXIO 4 WIFI GET',
                            '@REBOOT','AXIO 5 WIFI GET','AXIO 6 STATUS','AXIO 7 INFO','AXIO 8 METRICS'])
        self.assertTrue(all(r['ok'] for r in rows),rows)
        networks=rows[3]['data']['networks'];self.assertEqual(len(networks),2)
        self.assertEqual(networks[1],dict(ssid_hex=ssid,priority=2))
        self.assertEqual(rows[4]['data']['networks'],networks)
        self.assertTrue(all('connectivity' in r['data'] for r in rows[5:]))
        self.assertNotIn(password,json.dumps(rows));self.assertNotIn('another-secret',json.dumps(rows))

    def test_usb_failed_mutation_preserves_networks(self):
        ssid='new'.encode().hex();password='password'.encode().hex()
        rows=self.commands(['AXIO 1 WIFI GET','@FAIL_NVS',f'AXIO 2 WIFI SET 1 {ssid} {password}',
                            'AXIO 3 WIFI GET','@OK_NVS','@REBOOT','AXIO 4 WIFI GET',
                            'AXIO 5 WIFI SET 256 61 -','AXIO 6 WIFI SET 1 00 -',
                            'AXIO 7 WIFI SET 1 61 6162','AXIO 8 WIFI DELETE 6d697373696e67'])
        self.assertEqual(rows[1]['error'],'persistence_failed')
        self.assertEqual(rows[0]['data'],rows[2]['data']);self.assertEqual(rows[0]['data'],rows[3]['data'])
        self.assertTrue(all(not r['ok'] for r in rows[4:]))

    def test_osc_sta_suspension_rebind_ip_and_recovery(self):
        rows=self.commands(['@TICK','AXIO 1 METRICS','@UP','@TICK','AXIO 2 METRICS',
                            '@DOWN','@TICK','AXIO 3 METRICS','@UP','@IP','@TICK','AXIO 4 METRICS'])
        engines=[r['data']['engine'] for r in rows]
        self.assertEqual(engines[0]['udp_attempts'],0)
        self.assertEqual(engines[0]['suspended_no_sta'],1)
        self.assertGreater(engines[1]['udp_attempts'],0)
        self.assertEqual(engines[2]['udp_attempts'],engines[1]['udp_attempts'])
        self.assertFalse(engines[2]['listening'])
        self.assertTrue(engines[3]['listening'])
        self.assertGreater(engines[3]['udp_attempts'],engines[2]['udp_attempts'])

    def test_listener_failure_retries_without_config_apply(self):
        rows=self.commands(['@BINDFAIL','@UP','@TICK','AXIO 1 METRICS',
                            '@BINDOK','@TIMEOUT','@TICK','AXIO 2 METRICS'])
        a,b=[r['data']['engine'] for r in rows]
        self.assertFalse(a['listening']);self.assertEqual(a['listener_failures'],1)
        self.assertEqual(a['suspended_listener'],1);self.assertEqual(a['udp_errors'],0)
        self.assertTrue(b['listening']);self.assertGreater(b['udp_attempts'],0)

    def test_max_osc_and_network_diagnostic_response(self):
        # Import the existing capacity fixtures, with no changes to OSC schema 1.
        from osc_benchmark import scenarios
        config=list(scenarios('127.0.0.1'))[-1]['config']
        commands=[f'AXIO {i} WIFI SET {i} '+('s'*31+str(i)).encode().hex()+' '+b'password'.hex() for i in range(1,8)]
        commands+=['AXIO a CONFIG APPLY '+json.dumps(config,separators=(',',':')),
                   '@UP','AXIO b METRICS','AXIO c WIFI GET','AXIO d WIFI SET 0 6578747261 -']
        rows=self.commands(commands)
        self.assertTrue(all(r['ok'] for r in rows[:-1]),rows)
        self.assertEqual(rows[-1]['error'],'too_many_networks')
        self.assertEqual(len(rows[-3]['data']['engine']['messages']),16)
        self.assertEqual(rows[-3]['data']['connectivity']['networks'],8)
        self.assertEqual(len(rows[-2]['data']['networks']),8)

    def test_wifi_and_osc_journals_survive_adapter_reload_independently(self):
        from osc_config import preset
        config=preset('Motion','127.0.0.1',16900,'independent')
        rows=self.commands(['AXIO 1 CONFIG APPLY '+json.dumps(config,separators=(',',':')),
                            'AXIO 2 WIFI SET 42 726f6f6d 70617373776f7264',
                            '@REBOOT','AXIO 3 CONFIG GET','AXIO 4 WIFI GET'])
        self.assertTrue(all(r['ok'] for r in rows),rows)
        self.assertEqual(rows[2]['data'],config)
        self.assertEqual(len(rows[3]['data']['networks']),2)

    def test_ap_only_fallback_and_sta_shutdown_resume_osc(self):
        rows=self.commands(['@AP_ADVANCE','@TICK','AXIO 1 STATUS','AXIO 2 METRICS',
                            '@UP','@TICK','AXIO 3 STATUS','AXIO 4 METRICS'])
        ap=rows[0]['data']['connectivity'];self.assertTrue(ap['ap_active'])
        self.assertEqual(ap['ap']['ssid'],'Axio-123456789abc');self.assertEqual(ap['ap_ip'],'192.168.4.1')
        self.assertGreaterEqual(ap['ap']['last_fallback_ms'],15000)
        self.assertEqual(rows[1]['data']['engine']['udp_attempts'],0)
        self.assertFalse(rows[1]['data']['engine']['listening'])
        self.assertFalse(rows[2]['data']['connectivity']['ap_active'])
        self.assertFalse(rows[2]['data']['connectivity']['ap']['http_active'])
        self.assertGreater(rows[3]['data']['engine']['udp_attempts'],0)

    def test_osc_blocked_even_with_sta_until_ap_stop_succeeds(self):
        rows=self.commands(['@AP_ADVANCE','@APSTOPFAIL','@APSTA','@TICK','AXIO 1 METRICS',
                            '@APSTOPOK','@TIMEOUT','@TICK','AXIO 2 METRICS'])
        a,b=[r['data']['engine'] for r in rows]
        self.assertEqual(a['udp_attempts'],0);self.assertEqual(a['suspended_ap'],1)
        self.assertFalse(a['listening']);self.assertEqual(a['udp_errors'],0)
        self.assertTrue(b['listening']);self.assertGreater(b['udp_attempts'],0)

    def test_ap_secret_usb_only_persistent_recovery_and_explicit_rekey(self):
        rows=self.commands(['AXIO 1 AP CREDENTIALS','AXIO 2 INFO','AXIO 3 STATUS','AXIO 4 METRICS',
                            '@REBOOT','AXIO 5 AP CREDENTIALS','@CORRUPT_AP','@REBOOT',
                            'AXIO 6 STATUS','AXIO 7 AP CREDENTIALS','AXIO 8 AP REKEY',
                            'AXIO 9 AP CREDENTIALS'])
        first=rows[0]['data'];self.assertEqual(first['password'],'12345678')
        self.assertEqual(first,rows[4]['data'])
        # The public fake MAC happens to contain the same digits; inspect fields/values.
        self.assertNotIn('"password":',json.dumps(rows[1:4]))
        self.assertNotIn(json.dumps(first['password']),json.dumps(rows[1:4]))
        self.assertFalse(rows[5]['data']['connectivity']['ap']['ready'])
        self.assertEqual(rows[6]['error'],'ap_secret_invalid');self.assertTrue(rows[7]['ok'])
        self.assertEqual(first['password'],rows[8]['data']['password'])

    def test_web_update_uses_existing_validated_wifi_journal(self):
        rows=self.commands(['@WEBSET','@REBOOT','AXIO 1 WIFI GET','@WEBDELETE','AXIO 2 WIFI GET'])
        self.assertIn(dict(ssid_hex=b'web-network'.hex(),priority=222),rows[0]['data']['networks'])
        self.assertEqual(len(rows[1]['data']['networks']),1)

    def test_ap_profile_and_radio_failures_never_enable_ap(self):
        for fault in ('@AP_PROFILE_FAIL','@AP_RADIO_FAIL'):
            rows=self.commands([fault,'@REBOOT','@AP_ADVANCE','@TICK','AXIO 1 STATUS','AXIO 2 AP CREDENTIALS'])
            ap=rows[0]['data']['connectivity']
            self.assertFalse(ap['ap_active']);self.assertFalse(ap['ap']['ready'])
            self.assertEqual(ap['ap']['error'],'ap_profile_failed')
            self.assertTrue(rows[1]['ok']) # Saved USB recovery secret remains available.

    def test_mdns_identity_discovery_response_and_offline_clear(self):
        rows=self.commands(['@UP','AXIO 1 STATUS','@DISC_ADVANCE','@DISC_PEER',
                            'AXIO 2 DISCOVERY GET','AXIO 3 METRICS','@DOWN','AXIO 4 DISCOVERY GET'])
        c=rows[0]['data']['connectivity'];self.assertEqual(c['mdns_hostname'],'axio-123456789abc')
        self.assertEqual(c['discovery'],'ready');self.assertEqual(c['discovery_details']['service'],'_axio._udp')
        peer=rows[1]['data']['peers'][0]
        self.assertEqual(peer['hostname'],'axio-aabbccddeeff.local');self.assertEqual(peer['ip'],'192.168.1.42')
        self.assertEqual(peer['port'],16889)
        self.assertEqual(rows[2]['data']['connectivity']['discovery_details']['completed'],1)
        self.assertEqual(rows[3]['data']['peers'],[])
        self.assertNotIn('password',json.dumps(rows))

    def test_entry_runtime_elects_native_owner_and_withdraws_on_conflict(self):
        rows=self.commands(['@ENTRY_ENABLE','@UP','@ENTRY_RUN','AXIO 1 STATUS',
                            '@ENTRY_CONFLICT','@TICK','AXIO 2 STATUS',
                            '@DOWN','AXIO 3 STATUS'])
        first=rows[0]['data']['connectivity']
        self.assertEqual(first['mdns_hostname'],'axio-123456789abc')
        self.assertEqual(first['discovery_details']['entry_state'],'leader')
        self.assertTrue(first['discovery_details']['entry_owned'])
        second=rows[1]['data']['connectivity']['discovery_details']
        self.assertFalse(second['entry_owned']);self.assertEqual(second['entry_conflicts'],1)
        self.assertEqual(rows[2]['data']['connectivity']['discovery_details']['entry_state'],'offline')

    def test_entry_runtime_retains_live_owner_and_defers_to_smaller_candidate(self):
        for mode in ('@ENTRY_OWNER','@ENTRY_SMALLER'):
            rows=self.commands(['@ENTRY_ENABLE',mode,'@UP','@ENTRY_RUN','AXIO 1 STATUS'])
            entry=rows[0]['data']['connectivity']['discovery_details']
            self.assertFalse(entry['entry_owned']);self.assertGreater(entry['entry_queries'],1)
            self.assertNotEqual(entry['entry_state'],'leader')

    def test_entry_announcements_retry_failure_and_stop_offline(self):
        rows=self.commands(['@ENTRY_ENABLE','@UP','@ENTRY_RUN','AXIO 1 STATUS',
                            '@ENTRY_ANNOUNCE_FAIL','@ENTRY_RUN','AXIO 2 STATUS',
                            '@DOWN','@ENTRY_RUN','AXIO 3 STATUS'])
        entries=[r['data']['connectivity']['discovery_details'] for r in rows]
        self.assertGreater(entries[0]['announcements'],0)
        self.assertEqual(entries[1]['announcements'],entries[0]['announcements'])
        self.assertGreater(entries[1]['announce_failures'],0)
        self.assertLessEqual(entries[1]['announce_failures'],4)
        self.assertTrue(entries[1]['entry_owned'])
        self.assertEqual(entries[2]['announce_failures'],entries[1]['announce_failures'])

    def test_discovery_counts_empty_partial_and_accepted_results(self):
        for mode,accepted,incomplete in [('',1,0),('@DISC_NO_ADDRESS',0,1)]:
            commands=['@UP','@DISC_ADVANCE']
            if mode:commands.append(mode)
            commands+=['@DISC_PEER','AXIO 1 STATUS']
            entry=self.commands(commands)[0]['data']['connectivity']['discovery_details']
            self.assertEqual(entry['result_records'],1)
            self.assertEqual(entry['accepted_results'],accepted)
            self.assertEqual(entry['incomplete_results'],incomplete)
        entry=self.commands(['@UP','@DISC_ADVANCE','@DISC_ADVANCE','AXIO 1 STATUS'])[0]['data']['connectivity']['discovery_details']
        self.assertEqual(entry['empty_results'],1)
        self.assertEqual(entry['result_records'],0)

    def test_entry_runtime_failed_alias_never_reports_ownership(self):
        rows=self.commands(['@ENTRY_ENABLE','@ENTRY_ALIAS_FAIL','@UP','@ENTRY_RUN','AXIO 1 STATUS'])
        entry=rows[0]['data']['connectivity']['discovery_details']
        self.assertFalse(entry['entry_owned']);self.assertFalse(entry['active'])
        self.assertEqual(entry['error'],'mdns_alias_failed')

    def test_entry_runtime_ip_change_requires_new_election(self):
        rows=self.commands(['@ENTRY_ENABLE','@UP','@ENTRY_RUN','AXIO 1 STATUS',
                            '@IP','@TICK','AXIO 2 STATUS'])
        self.assertTrue(rows[0]['data']['connectivity']['discovery_details']['entry_owned'])
        self.assertFalse(rows[1]['data']['connectivity']['discovery_details']['entry_owned'])
        self.assertEqual(rows[1]['data']['connectivity']['mdns_hostname'],'axio-123456789abc')

    def test_mdns_pending_query_ip_change_drains_without_stale_peers(self):
        rows=self.commands(['@UP','@DISC_ADVANCE','@IP','AXIO 1 STATUS','@DISC_PEER',
                            '@TICK','AXIO 2 STATUS','AXIO 3 DISCOVERY GET'])
        self.assertEqual(rows[0]['data']['connectivity']['discovery'],'draining')
        self.assertEqual(rows[1]['data']['connectivity']['discovery'],'ready')
        self.assertEqual(rows[1]['data']['connectivity']['discovery_details']['starts'],2)
        self.assertEqual(rows[2]['data']['peers'],[])

    def test_mdns_filters_other_schema_and_ap_interface(self):
        for mode in ('@DISC_BAD','@DISC_AP'):
            rows=self.commands(['@UP','@DISC_ADVANCE',mode,'@DISC_PEER',
                                'AXIO 1 DISCOVERY GET','AXIO 2 STATUS'])
            self.assertEqual(rows[0]['data']['peers'],[])
            self.assertEqual(rows[1]['data']['connectivity']['discovery_details']['filtered'],1)

    def test_mdns_failure_retry_and_conflict_name_diagnostics(self):
        rows=self.commands(['@DISC_FAIL','@UP','AXIO 1 STATUS','@DISC_OK','@DISC_ADVANCE',
                            '@DISC_RENAME','AXIO 2 STATUS'])
        self.assertFalse(rows[0]['data']['connectivity']['discovery_details']['active'])
        self.assertEqual(rows[0]['data']['connectivity']['discovery_details']['failures'],1)
        self.assertEqual(rows[1]['data']['connectivity']['mdns_hostname'],'axio-123456789abc-2')
        self.assertEqual(rows[1]['data']['connectivity']['discovery_details']['name_changes'],1)

    def test_mdns_service_port_tracks_osc_apply_without_schema_change(self):
        from osc_config import preset
        config=preset('Motion','127.0.0.1',16900,'mdns');config['listen_port']=16989
        rows=self.commands(['@UP','AXIO 1 CONFIG APPLY '+json.dumps(config,separators=(',',':')),
                            '@TICK','AXIO 2 STATUS','AXIO 3 CONFIG GET'])
        self.assertTrue(rows[0]['ok']);self.assertEqual(rows[1]['data']['connectivity']['discovery_details']['service_port'],16989)
        self.assertEqual(rows[2]['data'],config)

    def test_mdns_partial_service_failure_cleanup_and_retry(self):
        rows=self.commands(['@DISC_SERVICE_FAIL','@UP','AXIO 1 STATUS','@DISC_SERVICE_OK',
                            '@DISC_ADVANCE','AXIO 2 STATUS'])
        self.assertEqual(rows[0]['data']['connectivity']['discovery_details']['error'],'mdns_service_failed')
        self.assertFalse(rows[0]['data']['connectivity']['discovery_details']['active'])
        self.assertTrue(rows[1]['data']['connectivity']['discovery_details']['active'])

    def test_mdns_port_update_failure_preserves_osc_and_recovers(self):
        from osc_config import preset
        config=preset('Motion','127.0.0.1',16900,'mdns');config['listen_port']=16989
        rows=self.commands(['@UP','@DISC_PORT_FAIL','AXIO 1 CONFIG APPLY '+json.dumps(config,separators=(',',':')),
                            '@TICK','AXIO 2 STATUS','@TICK','@TICK','AXIO 3 STATUS','AXIO 4 CONFIG GET'])
        self.assertTrue(rows[0]['ok'])
        self.assertEqual(rows[1]['data']['connectivity']['discovery'],'draining')
        self.assertTrue(rows[2]['data']['connectivity']['discovery_details']['active'])
        self.assertEqual(rows[3]['data'],config)

    def test_mdns_query_creation_failure_is_separate_from_udp_errors(self):
        rows=self.commands(['@UP','@DISC_QUERY_FAIL','@DISC_ADVANCE','@TICK','AXIO 1 METRICS'])
        c=rows[0]['data']['connectivity']['discovery_details']
        self.assertEqual(c['query_failures'],1);self.assertFalse(c['query_pending'])
        self.assertEqual(rows[0]['data']['engine']['udp_errors'],0)

    def test_no_application_waits_scans_or_legacy_portal(self):
        source='\n'.join(p.read_text() for p in (ROOT/'firmware/Axio/src/connectivity').glob('*'))
        for forbidden in ('delay(', 'waitForConnectResult(', 'scanNetworks(', 'autoConnect(', 'startConfigPortal(', 'WiFi.begin('):
            self.assertNotIn(forbidden,source)
        self.assertNotIn('processLegacyPortal', (ROOT/'firmware/Axio/Axio.ino').read_text())
        self.assertIn('WIFI_STORAGE_RAM', source)
        self.assertNotIn('WIFI_STORAGE_FLASH', source)

if __name__=='__main__':unittest.main()
