"""Catch decimal minor-version ordering and unknown-era guessing.

The staged scanner is actual existing source with exact replacements; no
real UE4 image, anchor, runtime profile or console support is tested here.
"""
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import types
import unittest

import stage_version_era as stage

SOURCE = Path(__file__).resolve().parents[3] / 'outputs/2026-10-08-l25-prep/phasea-tools/engine_match.py'


def module(data):
    obj = types.ModuleType('derived_scanner')
    exec(compile(data, '<derived-scanner>', 'exec'), obj.__dict__)
    return obj


class VersionEraTests(unittest.TestCase):
    def setUp(self):
        self.original = SOURCE.read_bytes()
        self.derived = stage.transform(self.original)
        self.scanner = module(self.derived)

    def test_one_digit_minor_versions_use_legacy_research_era(self):
        for version in ('4.7', '4.8', '4.9', '4.10', '4.22'):
            with self.subTest(version=version):
                self.assertTrue(self.scanner.anchor_pack_recommendation('UE4', version).startswith('ue4-legacy'))

    def test_existing_boundary_uses_numeric_components(self):
        for version in ('4.23', '4.24', '4.27'):
            with self.subTest(version=version):
                self.assertTrue(self.scanner.anchor_pack_recommendation('UE4', version).startswith('ue4-chunked'))

    def test_unknown_or_malformed_minor_never_selects_era(self):
        for version in (None, '', '4.x', '5.6', '4.23.1', 4.7):
            with self.subTest(version=version):
                self.assertTrue(self.scanner.anchor_pack_recommendation('UE4', version).startswith('ue4-era-unresolved'))

    def test_other_families_are_unchanged(self):
        original = module(self.original)
        for family, version in ((None, None), ('UE5', '5.6'), ('other', None)):
            self.assertEqual(self.scanner.anchor_pack_recommendation(family, version),
                             original.anchor_pack_recommendation(family, version))

    def test_transformation_does_not_modify_original_file(self):
        self.assertNotEqual(self.derived, self.original)
        self.assertEqual(SOURCE.read_bytes(), self.original)

    def test_drift_and_second_application_refuse(self):
        for source in (self.original + b'\n', self.derived):
            with self.assertRaises(ValueError):
                stage.transform(source)

    def test_cli_dryrun_and_new_derived_copy_preserve_source(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent/'tmp') as tmp:
            target = Path(tmp)/'derived'
            base = [sys.executable, '-B', str(Path(stage.__file__)), '--source', str(SOURCE)]
            dry = subprocess.run(base, capture_output=True, text=True)
            self.assertEqual(dry.returncode, 0, dry.stderr)
            self.assertFalse(target.exists())
            done = subprocess.run(base+['--write-dir', str(target)], capture_output=True, text=True)
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertEqual((target/'engine_match.py').read_bytes(), self.derived)
            again = subprocess.run(base+['--write-dir', str(target)], capture_output=True, text=True)
            self.assertNotEqual(again.returncode, 0)
            self.assertEqual(SOURCE.read_bytes(), self.original)

    def test_cli_outside_destination_refuses_without_writing(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent/'tmp') as tmp:
            # A different capability is outside this staging script's ownership.
            target = Path(__file__).resolve().parent.parent/'cap03'/'not-created-by-test'
            self.assertFalse(target.exists())
            result = subprocess.run([sys.executable, '-B', str(Path(stage.__file__)),
                                     '--source', str(SOURCE), '--write-dir', str(target)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(target.exists())


if __name__ == '__main__':
    unittest.main()
