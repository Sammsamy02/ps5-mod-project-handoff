"""Actual inherited historical lineage and SDK evidence; no compiler/package."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest

OWN = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(OWN))
sys.path.insert(0, str(OWN / 'execution'))
from admission import Refusal, read
from composition import Composer, historical_delta, sdk_nm_evidence


class LineageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data = read(OWN / 'parent-review/EXECUTION-ADMISSION.FINAL.v3.json')
        cls.nm = Path(data['nm']['path'])
        cls.base = read(OWN.parent / '2026-10-07-l19-r3-full-build/runs/run-l19-2/receipt.json')

    def test_actual197_to210_cumulative_delta_not_immediate_delta(self):
        delta = historical_delta(self.base, self.base['undefined_delta']['current'])
        self.assertEqual(delta['added'], self.base['undefined_delta']['added'])
        self.assertEqual(len(delta['added']), 13)
        self.assertEqual(delta['removed'], [])
        self.assertEqual(delta['changed_bindings'], {})
        self.assertEqual(delta['expected_bootstrap_resolutions'], self.base['undefined_delta']['expected_bootstrap_resolutions'])

    def test_wrong_historical_receipt_hash_refuses(self):
        base = copy.deepcopy(self.base); base['base_receipt_sha256'] = '0' * 64
        with self.assertRaisesRegex(Refusal, 'historical'):
            historical_delta(base, base['undefined_delta']['current'])

    def test_fabricated_cumulative_claim_refuses(self):
        base = copy.deepcopy(self.base); base['undefined_delta']['added'] = []
        with self.assertRaisesRegex(Refusal, 'cumulative'):
            historical_delta(base, base['undefined_delta']['current'])

    def test_sdk_provider_hash_refuses_before_tool_invocation(self):
        base = copy.deepcopy(self.base)
        for path in base['undefined_delta']['provider_library_hashes']:
            if Path(path).name == 'libkernel.so': base['undefined_delta']['provider_library_hashes'][path] = '0' * 64
        calls = []
        with tempfile.TemporaryDirectory(dir=OWN / 'tests') as temp:
            with self.assertRaisesRegex(Refusal, 'SDK provider'):
                sdk_nm_evidence(base, self.nm, lambda *args: calls.append(args), Path(temp), {})
        self.assertEqual(calls, [])

    def test_three_actual_sdk_captures_use_original_flags(self):
        with tempfile.TemporaryDirectory(dir=OWN / 'tests') as temp:
            run = Path(temp)
            evidence = sdk_nm_evidence(self.base, self.nm, Composer(types.SimpleNamespace()).command, run, {})
            self.assertEqual(len(evidence), 3)
            for name, record in evidence.items():
                command = read(run / (name + '.command.json'))
                self.assertEqual(command['argv'][1:4], ['-D', '--defined-only', '--format=posix'])
                self.assertEqual(command['exit'], 0)
                self.assertTrue(Path(record['path']).is_file())
                self.assertEqual(Path(record['path']).read_text(), command['stdout'])


if __name__ == '__main__':
    unittest.main()
