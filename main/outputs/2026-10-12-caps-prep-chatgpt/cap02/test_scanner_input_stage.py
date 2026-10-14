"""Actual derived parser tests; synthetic ELF bytes prove input handling only."""
import contextlib
import hashlib
import io
import json
import struct
import subprocess
import sys
import tempfile
import types
import unittest
from pathlib import Path

import stage_scanner_inputs as stage
import stage_version_era as era

HERE = Path(__file__).resolve().parent


def elf(payload=b'fixture', entry_size=56):
    ident = b'\x7fELF' + bytes([2, 1, 1]) + bytes(9)
    off = 64 + entry_size
    header = struct.pack('<16sHHIQQQIHHHHHH', ident, 2, 0x3e, 1,
                         0x1000, 64, 0, 0, 64, entry_size, 1, 0, 0, 0)
    ph = struct.pack('<IIQQQQQQ', 1, 5, off, 0x1000, 0,
                     len(payload), len(payload), 1)
    return header + ph + bytes(entry_size - 56) + payload


def changed(data, offset, fmt, value):
    result = bytearray(data)
    struct.pack_into(fmt, result, offset, value)
    return bytes(result)


def derived_module():
    result = types.ModuleType('derived_scanner')
    exec(compile(stage.transform(era.SOURCE.read_bytes()), '<derived-scanner>', 'exec'), result.__dict__)
    return result


class ScannerInputStage(unittest.TestCase):
    def setUp(self):
        self.module = derived_module()
        self.temp = tempfile.TemporaryDirectory(dir=HERE/'tmp')
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)/'synthetic.elf'

    def parse(self, data):
        self.path.write_bytes(data)
        return self.module.parse_image(self.path)

    def test_wrong_elf_encoding_refuses_before_marker_admission(self):
        base = elf()
        for offset, fmt, value in [(4,'B',0),(4,'B',1),(5,'B',0),(5,'B',2),
                                   (6,'B',0),(6,'B',2),(20,'<I',0),
                                   (20,'<I',2),(52,'<H',63)]:
            with self.subTest(offset=offset,value=value):
                with self.assertRaises(ValueError):
                    self.parse(changed(base,offset,fmt,value))

    def test_invalid_program_table_is_a_controlled_input_error(self):
        base = elf()
        for offset, fmt, value in [(54,'<H',0),(54,'<H',55),(32,'<Q',0),
                                   (32,'<Q',63),(32,'<Q',len(base)-8),
                                   (56,'<H',2),(56,'<H',0xffff)]:
            with self.subTest(offset=offset,value=value):
                with self.assertRaises(ValueError):
                    self.parse(changed(base,offset,fmt,value))

    def test_invalid_load_range_and_shape_refuse(self):
        base = elf()
        for offset, fmt, value in [(72,'<Q',len(base)+1),(96,'<Q',100),
                                   (104,'<Q',1),(80,'<Q',2**64-2),
                                   (112,'<Q',3),(112,'<Q',4096)]:
            with self.subTest(offset=offset,value=value):
                with self.assertRaises(ValueError):
                    self.parse(changed(base,offset,fmt,value))

    def test_valid_padded_entries_keep_exact_file_identity_and_marker_span(self):
        marker = 'UE fixture'
        payload = marker.encode('utf-16-le')
        for entry_size in [56,64]:
            data = elf(payload,entry_size)
            image = self.parse(data)
            self.assertEqual(image['sha256'],hashlib.sha256(data).hexdigest())
            self.assertEqual(image['size'],len(data))
            self.assertEqual(image['loads'],[{'off':64+entry_size,'vaddr':0x1000,
                                            'filesz':len(payload),'memsz':len(payload)}])
            self.assertEqual(self.module.scan_markers(image,[{'string':marker}]),[True])

    def test_cli_malformed_input_returns_two_without_traceback(self):
        self.path.write_bytes(changed(elf(),54,'<H',55))
        before = self.path.read_bytes()
        stdout,stderr = io.StringIO(),io.StringIO()
        with contextlib.redirect_stdout(stdout),contextlib.redirect_stderr(stderr):
            result = self.module.main(['scanner',str(self.path),'--json'])
        self.assertEqual(result,2)
        self.assertNotIn('Traceback',stderr.getvalue())
        self.assertTrue(stderr.getvalue())
        self.assertEqual(stdout.getvalue(),'')
        self.assertEqual(self.path.read_bytes(),before)

    def test_combined_stage_keeps_numeric_fix_and_refuses_second_application(self):
        original = era.SOURCE.read_bytes()
        result = stage.transform(original)
        self.assertIn('ue4-legacy',self.module.anchor_pack_recommendation('UE4','4.7'))
        with self.assertRaises(ValueError):stage.transform(result)
        with self.assertRaises(ValueError):stage.transform(original+b'\n')
        self.assertEqual(era.SOURCE.read_bytes(),original)

    def test_staging_cli_receipt_and_derived_copy_bind_both_replacements(self):
        dest = Path(self.temp.name)/'derived'
        result = subprocess.run([sys.executable,str(HERE/'stage_scanner_inputs.py'),
                                 '--write-dir',str(dest)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        receipt = json.loads(result.stdout)
        self.assertEqual(receipt['replacements'],2)
        derived = (dest/'engine_match.py').read_bytes()
        self.assertEqual(receipt['derived_sha256'],hashlib.sha256(derived).hexdigest())
        self.assertEqual(derived,stage.transform(era.SOURCE.read_bytes()))
        second = subprocess.run([sys.executable,str(HERE/'stage_scanner_inputs.py'),
                                 '--write-dir',str(dest)],capture_output=True,text=True)
        self.assertNotEqual(second.returncode,0)
        self.assertEqual((dest/'engine_match.py').read_bytes(),derived)


if __name__ == '__main__':
    unittest.main()
