import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from network_recovery_probe import boot_skew_ms, recovery_ready


class NetworkRecoveryTests(unittest.TestCase):
    def test_recovery_waits_for_both_peers_and_dns_matches_single_owner(self):
        devices = {n: {'STATUS': {'connectivity': {'sta_connected': True,
                   'ap_active': False, 'sta_ip': ip, 'discovery_details':
                   {'peers': 1, 'entry_owned': n == 'primary'}}}}
                   for n, ip in [('primary', '192.168.8.159'), ('secondary', '192.168.8.184')]}
        self.assertTrue(recovery_ready(devices, ['192.168.8.159']))
        self.assertFalse(recovery_ready(devices, []))
        self.assertFalse(recovery_ready(devices, ['192.168.8.184']))
        primary = devices['primary']['STATUS']['connectivity']
        primary['discovery_details']['peers'] = 0
        self.assertFalse(recovery_ready(devices, ['192.168.8.159']))
        primary['discovery_details']['peers'] = 1
        devices['secondary']['STATUS']['connectivity']['discovery_details']['entry_owned'] = True
        self.assertFalse(recovery_ready(devices, ['192.168.8.159']))

    def test_boot_estimate_ignores_old_uptimes_after_restart_ack(self):
        before = {n: {'STATUS': {'uptime_us': 100000000}} for n in ('primary', 'secondary')}
        old = {n: {'STATUS': {'uptime_us': 101000000}, 'status_host_epoch': 200} for n in before}
        new = {'primary': {'STATUS': {'uptime_us': 1000000}, 'status_host_epoch': 200},
               'secondary': {'STATUS': {'uptime_us': 990000}, 'status_host_epoch': 200}}
        self.assertAlmostEqual(boot_skew_ms([{'devices': old}, {'devices': new}], before), 10)

    def test_no_boot_is_not_misreported_as_simultaneous(self):
        before = {'primary': {'STATUS': {'uptime_us': 100}}}
        with self.assertRaises(StopIteration):
            boot_skew_ms([{'devices': {'primary': {'error': 'not ready'}}}], before)
