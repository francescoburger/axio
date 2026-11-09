"""Raw DNS evidence must not count questions, goodbyes, or malformed packets."""
from pathlib import Path
import socket
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from entry_point_probe import a_records, dns_name


class EntryPointProbeTests(unittest.TestCase):
    def packet(self, ttl=120, cls=0x8001):
        name = b'\x04axio\x05local\0'
        question = name + struct.pack('!HH', 1, 1)
        answer = b'\xc0\x0c' + struct.pack('!HHIH', 1, cls, ttl, 4) + socket.inet_aton('192.168.1.15')
        return struct.pack('!6H', 0, 0x8400, 1, 1, 0, 0) + question + answer

    def test_compressed_cache_flush_answer_and_goodbye(self):
        self.assertEqual(a_records(self.packet(), 'AXIO.local'), ['192.168.1.15'])
        self.assertEqual(a_records(self.packet(ttl=0), 'axio.local'), [])
        self.assertEqual(a_records(self.packet(cls=2), 'axio.local'), [])
        self.assertEqual(a_records(self.packet(), 'other.local'), [])

    def test_truncated_records_and_compression_loop(self):
        with self.assertRaises(ValueError):
            a_records(self.packet()[:-1], 'axio.local')
        with self.assertRaises(ValueError):
            dns_name(b'\xc0\x00', 0)
        packet = bytearray(self.packet())
        packet[2:4] = b'\0\0'
        self.assertEqual(a_records(packet, 'axio.local'), [])


if __name__ == '__main__':
    unittest.main()
