"""Offline gate tests use unchanged upstream DLLs and a no-TLS control."""

import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

from admission_dossier import load_registry, make_dossier, verify_source_pins
from pe_analyzer import analyze_file


ROOT = Path(__file__).resolve().parent.parent
UPSTREAM = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap13/upstream-inputs')
REGISTRY = ROOT / 'TOOL/loader_capabilities.json'
LOADER_ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-gs-guard-20261013-07')


class AdmissionDossierTests(unittest.TestCase):
    def _mutated_fixture_report(self, change):
        source = ROOT / 'FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'
        data = bytearray(source.read_bytes())
        pe_off = struct.unpack_from('<I', data, 0x3C)[0]
        opt_off = pe_off + 4 + 20
        change(data, opt_off)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'mutated-FIXTURE.dll'
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        return report

    def _mutated_worker_report(self, change):
        source = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap13/SAMPLE/real-worker-standard.dll')
        data = bytearray(source.read_bytes())
        pe_off = struct.unpack_from('<I', data, 0x3C)[0]
        opt_off = pe_off + 4 + 20
        change(data, opt_off)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'mutated-worker-FIXTURE.dll'
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        return report

    def test_five_unchanged_upstream_dlls_refuse_real_static_tls(self):
        samples = [ROOT / 'SAMPLE/TrueFOV-upstream-winmm.dll'] + [
            UPSTREAM / name for name in (
                'ZeroCompanyMandoWardrobe-upstream-main.dll',
                'TFWWorkbench-upstream-main.dll',
                'EventViewerMod-upstream-main.dll',
                'KismetDebuggerMod-upstream-main.dll',
            )]
        registry = load_registry(REGISTRY)
        for path in samples:
            with self.subTest(path=path.name):
                before = hashlib.sha256(path.read_bytes()).hexdigest()
                _, report = analyze_file(str(path))
                dossier = make_dossier(report, registry)
                self.assertEqual(dossier['sha256'], before)
                self.assertEqual(dossier['offline_gate'], 'blocked')
                self.assertIn('real_static_tls', [x['feature'] for x in dossier['blockers']])
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), before)

    def test_no_tls_fixture_with_shared_write_execute_page_is_blocked(self):
        _, report = analyze_file(str(ROOT / 'FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll'))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertEqual(dossier['offline_gate'], 'blocked')
        self.assertIn('rwx_partition', [x['feature'] for x in dossier['blockers']])

    def test_no_tls_separate_page_sample_still_needs_loader_preflight(self):
        path = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap13/SAMPLE/real-worker-standard.dll')
        _, report = analyze_file(str(path))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertEqual(dossier['offline_gate'], 'requires_loader_preflight')
        self.assertEqual(dossier['blockers'], [])
        self.assertIn('loader_preflight', dossier['unverified_gates'])
        self.assertIn('target_binding', dossier['unverified_gates'])
        self.assertIn('windows_import_abi', dossier['unverified_gates'])

    def test_empty_tls_template_with_callback_pointer_is_refused(self):
        source = ROOT / 'SAMPLE/TrueFOV-upstream-winmm.dll'
        data = bytearray(source.read_bytes())
        image, _ = analyze_file(str(source))
        tls_off = image.rva_to_offset(image._dir(9)['rva'])
        start = struct.unpack_from('<Q', data, tls_off)[0]
        struct.pack_into('<Q', data, tls_off + 8, start)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'empty-template-callback-pointer-FIXTURE.dll'
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        self.assertEqual(report['tls']['status'], 'empty')
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertEqual(dossier['offline_gate'], 'blocked')
        self.assertIn('tls_callback_table_pointer', [x['feature'] for x in dossier['blockers']])

    def test_unrecognized_registry_state_is_rejected(self):
        registry = json.loads(REGISTRY.read_text())
        registry['features']['real_static_tls'] = 'ready'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad-registry.json'
            path.write_text(json.dumps(registry))
            with self.assertRaisesRegex(ValueError, 'unknown feature state'):
                load_registry(path)

    def test_registry_matches_pinned_loader_sources(self):
        verify_source_pins(load_registry(REGISTRY), LOADER_ROOT)

    def test_mismatched_loader_source_is_rejected(self):
        registry = load_registry(REGISTRY)
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'missing or mismatched'):
                verify_source_pins(registry, Path(directory))

    def test_bad_pe_signature_is_refused_without_crashing_intake(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'bad-signature-FIXTURE.dll'
            data = bytearray(128)
            data[:2] = b'MZ'
            struct.pack_into('<I', data, 0x3C, 0x40)
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertEqual(report['classification'], 'unknown')
        self.assertEqual(dossier['offline_gate'], 'blocked')

    def test_rva_only_clr_directory_is_refused(self):
        def change(data, opt_off):
            struct.pack_into('<II', data, opt_off + 112 + 14 * 8, 0x2000, 0)
        report = self._mutated_fixture_report(change)
        self.assertFalse(report['managed'])
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('managed_clr', [x['feature'] for x in dossier['blockers']])

    def test_loader_minimum_data_directory_count_is_enforced(self):
        def change(data, opt_off):
            struct.pack_into('<I', data, opt_off + 108, 9)
        report = self._mutated_fixture_report(change)
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('metadata_integrity', [x['feature'] for x in dossier['blockers']])

    def test_declared_tls_directory_must_be_wholly_file_backed(self):
        source = ROOT / 'SAMPLE/cap13-mod-splitscreen.dll'
        data = bytearray(source.read_bytes())
        pe_off = struct.unpack_from('<I', data, 0x3C)[0]
        opt_off = pe_off + 4 + 20
        struct.pack_into('<I', data, opt_off + 112 + 9 * 8 + 4, 0x601)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'oversized-tls-FIXTURE.dll'
            path.write_bytes(data)
            _, report = analyze_file(str(path))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertEqual(report['tls']['status'], 'malformed')
        self.assertIn('real_static_tls', [x['feature'] for x in dossier['blockers']])

    def test_absolute_source_pin_cannot_bypass_loader_root(self):
        registry = load_registry(REGISTRY)
        registry['source_pins'] = {str(LOADER_ROOT / 'loader/src/pe_loader.cpp'):
                                   registry['source_pins']['loader/src/pe_loader.cpp']}
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'source pin'):
                verify_source_pins(registry, Path(directory))

    def test_registry_requires_pe_structure_header_pin(self):
        registry = json.loads(REGISTRY.read_text())
        registry['source_pins'].pop('loader/src/pe_types.h', None)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'missing-header-pin.json'
            path.write_text(json.dumps(registry))
            with self.assertRaisesRegex(ValueError, 'source pins'):
                load_registry(path)

    def test_image_over_loader_cap_is_refused(self):
        report = self._mutated_worker_report(
            lambda data, opt_off: struct.pack_into('<I', data, opt_off + 56, 0x20001000))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('metadata_integrity', [x['feature'] for x in dossier['blockers']])

    def test_entrypoint_in_read_only_data_is_refused(self):
        source = Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap13/SAMPLE/real-worker-standard.dll')
        image, _ = analyze_file(str(source))
        rdata_rva = next(s['virtual_address'] for s in image.sections if s['name'] == '.rdata')
        report = self._mutated_worker_report(
            lambda data, opt_off: struct.pack_into('<I', data, opt_off + 16, rdata_rva))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('metadata_integrity', [x['feature'] for x in dossier['blockers']])


if __name__ == '__main__':
    unittest.main()
