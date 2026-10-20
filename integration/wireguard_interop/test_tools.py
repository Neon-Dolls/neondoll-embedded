import base64
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import protocol
import peer
import serial_client


class PacketTests(unittest.TestCase):
    def test_shared_cpp_vector(self):
        self.assertEqual(protocol.packet('Q', 'D', 0x01020304, 16).hex(), '4e445747333243215144010203041213')

    def test_sizes_and_corruption(self):
        for size in (14, 64, 1232, 1372, 1373):
            data = protocol.packet('E', 'L', 0xFFFFFFFF, size)
            self.assertEqual(protocol.parse(data), ('E', 'L', 0xFFFFFFFF))
        for data in (b'', data[:13], data+b'x', data[:15]+b'X'+data[16:]):
            with self.assertRaises(ValueError):
                protocol.parse(data)
        for size in (13, 1374):
            with self.assertRaises(ValueError):
                protocol.packet('Q', 'L', 1, size)

    def test_header_and_range(self):
        for args in (('X', 'L', 1, 16), ('Q', 'XX', 1, 16), ('Q', 'D', -1, 16)):
            with self.assertRaises(ValueError):
                protocol.packet(*args)


class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.path = Path(self.directory.name) / 'private.json'
        # Generated only at test runtime; no credential/private-key fixture is committed.
        self.config = dict(wifi_ssid='test', wifi_password='temporary test string',
            private_key=base64.b64encode(os.urandom(32)).decode(),
            peer_public_key=base64.b64encode(os.urandom(32)).decode(),
            endpoint_ipv4='192.0.2.1', endpoint_port=51820, local_ipv6='fd42:4e44:2::2',
            peer_ipv6='fd42:4e44:1::1', allowed_prefix='fd42:4e44:1::', allowed_length=65)
        self.write()

    def tearDown(self):
        self.directory.cleanup()

    def write(self):
        self.path.write_text(json.dumps(self.config))
        self.path.chmod(0o600)

    def test_ram_line_has_clock_and_opt_in(self):
        data = protocol.config_line(self.path, 1800000000)
        self.assertLessEqual(len(data), 1025)
        self.assertTrue(data.endswith(b'\n'))
        obj = json.loads(data)
        self.assertEqual(obj['command'], 'configure')
        self.assertEqual(obj['unix_time'], 1800000000)

    def test_public_permissions_and_symlinks_rejected(self):
        self.path.chmod(0o644)
        with self.assertRaises(ValueError):
            protocol.config_line(self.path, 1800000000)
        self.path.chmod(0o600)
        link = self.path.with_suffix('.link')
        link.symlink_to(self.path)
        with self.assertRaises(ValueError):
            protocol.config_line(link, 1800000000)

    def test_invalid_configuration_does_not_echo_secrets(self):
        private_secret, password_secret = self.config['private_key'], self.config['wifi_password']
        for key, value in [('private_key', 'invalid'), ('endpoint_port', True), ('allowed_length', 0),
                           ('allowed_prefix', 'fd42:4e44:1::1'), ('peer_ipv6', 'fd42:4e44:1:0:8000::1'),
                           ('wifi_ssid', 'x'*33), ('wifi_password', '\x00'), ('endpoint_ipv4', '127.0.0.1')]:
            original = self.config[key]
            self.config[key] = value
            self.write()
            with self.assertRaises(ValueError) as caught:
                protocol.config_line(self.path, 1800000000)
            self.assertNotIn(password_secret, str(caught.exception))
            self.assertNotIn(private_secret, str(caught.exception))
            self.config[key] = original

    def test_oversized_file_rejected(self):
        self.path.write_text(' '*1025)
        with self.assertRaises(ValueError):
            protocol.config_line(self.path, 1800000000)


class EvidenceToolsTests(unittest.TestCase):
    def test_safe_wg_fields_only(self):
        with patch('peer.subprocess.run') as run:
            run.return_value.stdout = 'PUBLICKEY 42\n'
            result = peer.wg_metrics('ndwgtest')
            self.assertEqual(result['latest-handshakes'], [[42]])
            self.assertEqual([call.args[0][-1] for call in run.call_args_list], ['latest-handshakes', 'transfer'])

    def test_bounded_command_schedule(self):
        self.assertEqual(serial_client.schedule('2.5:tunnel_stop'), (2.5, 'tunnel_stop'))
        for item in ('-1:metrics', '1:configure', '90000:shutdown', 'nan:metrics'):
            with self.assertRaises(Exception):
                serial_client.schedule(item)


if __name__ == '__main__':
    unittest.main()
