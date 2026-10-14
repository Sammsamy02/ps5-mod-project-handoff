"""Regression checks for private-address detection in handoff text."""
import unittest
import contextlib
import io
import json
from pathlib import Path
import tempfile
from unittest.mock import patch
import verify_export
from verify_export import PATTERNS


class PrivateAddressTests(unittest.TestCase):
    def test_address_joined_to_a_word_is_detected(self):
        address=b'.'.join([b'10',b'23',b'45',b'67'])
        self.assertIsNotNone(PATTERNS['private-network-literal'].search(b'host'+address+b'towardconsole'))

    def test_address_joined_to_another_word_is_detected(self):
        address=b'.'.join([b'192',b'168',b'17',b'29'])
        self.assertIsNotNone(PATTERNS['private-network-literal'].search(b'console'+address+b';'))

    def test_private_range_and_loopback_keep_their_different_meanings(self):
        self.assertIsNotNone(PATTERNS['private-network-literal'].search(b'.'.join([b'172',b'20',b'30',b'40'])))
        self.assertIsNone(PATTERNS['private-network-literal'].search(b'127.0.0.1'))

    def test_larger_numeric_identifier_is_not_a_private_address(self):
        self.assertIsNone(PATTERNS['private-network-literal'].search(b'1110.23.45.67'))

    def test_private_machine_identifier_file_detects_a_saved_machine_name(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            (root/'handoff').mkdir()
            (root/'handoff/SOURCE-MANIFEST.json').write_text('{"files": []}')
            (root/'.local').mkdir()
            (root/'.local/privacy-machine-identifiers.json').write_text('{"ComputerName": "OWNED-COMPUTER-FIXTURE"}')
            (root/'receipt.txt').write_text('Build on OWNED-COMPUTER-FIXTURE')
            output=io.StringIO()
            with patch.object(verify_export,'ROOT',root),patch('sys.argv',['verify_export']),contextlib.redirect_stdout(output):
                result=verify_export.main()
            self.assertEqual(result,1)
            self.assertIn('actual-computer-identifier',output.getvalue())
            self.assertNotIn('OWNED-COMPUTER-FIXTURE',output.getvalue())


if __name__ == '__main__':
    unittest.main()
