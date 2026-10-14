"""Direct GS access must be noticed regardless of PE TLS-directory status."""

import hashlib
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

from admission_dossier import load_registry, make_dossier
from gs_audit import audit_gs_access
from pe_analyzer import analyze_file


ROOT = Path(__file__).resolve().parent.parent
TREE = ROOT.parent
REGISTRY = ROOT / 'TOOL/loader_capabilities.json'
PROBE = TREE / 'cap13/SAMPLE/cap13-feature-probe.dll'
PROXY = Path('/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/windows-image-original-inputs-1/dwmapi.dll')
CONTROL = TREE / 'cap13/SAMPLE/real-worker-standard.dll'


class TebAuditTests(unittest.TestCase):
    def test_empty_tls_directory_does_not_hide_gs58(self):
        before = hashlib.sha256(PROBE.read_bytes()).hexdigest()
        _, report = analyze_file(str(PROBE))
        self.assertEqual(report['tls']['status'], 'empty')
        audit = report.get('gs_access')
        self.assertIsNotNone(audit)
        self.assertEqual(audit['status'], 'scanned')
        self.assertGreaterEqual(audit['candidate_count'], 3)
        self.assertTrue(any(site['rva'] == 0x101c and site['displacement'] == 0x58
                            for site in audit['sites']))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('direct_gs_access', [b['feature'] for b in dossier['blockers']])
        self.assertEqual(hashlib.sha256(PROBE.read_bytes()).hexdigest(), before)

    def test_no_tls_directory_does_not_hide_gs30(self):
        before = hashlib.sha256(PROXY.read_bytes()).hexdigest()
        _, report = analyze_file(str(PROXY))
        self.assertEqual(report['tls']['status'], 'none')
        audit = report.get('gs_access')
        self.assertIsNotNone(audit)
        self.assertEqual(audit['status'], 'scanned')
        self.assertTrue(any(site['rva'] == 0x161dd and site['displacement'] == 0x30
                            for site in audit['sites']))
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('direct_gs_access', [b['feature'] for b in dossier['blockers']])
        self.assertEqual(hashlib.sha256(PROXY.read_bytes()).hexdigest(), before)

    def test_candidate_free_result_does_not_claim_execution_safe(self):
        _, report = analyze_file(str(CONTROL))
        audit = report.get('gs_access')
        self.assertIsNotNone(audit)
        self.assertEqual(audit['status'], 'scanned')
        self.assertEqual(audit['candidate_count'], 0)
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('gs_access_scan_completeness', dossier['unverified_gates'])

    def test_executable_raw_tail_is_scanned_even_if_virtual_size_is_shorter(self):
        class Image:
            coff = {'machine': 0x8664}
            optional = {'format': 'PE32+'}
            sections = [{'executable': True, 'raw_size': 9, 'virtual_size': 1,
                         'raw_ptr': 0, 'virtual_address': 0x1000}]
            data = bytes.fromhex('65488b042560000000')
        audit = audit_gs_access(Image())
        self.assertEqual(audit['candidate_count'], 1)
        self.assertEqual(audit['sites'][0]['displacement'], 0x60)

    def test_missing_decoder_blocks_offline_dossier(self):
        with patch.dict(sys.modules, {'capstone': None}):
            _, report = analyze_file(str(CONTROL))
        self.assertEqual(report['gs_access']['status'], 'unavailable')
        dossier = make_dossier(report, load_registry(REGISTRY))
        self.assertIn('direct_gs_access', [b['feature'] for b in dossier['blockers']])


if __name__ == '__main__':
    unittest.main()
