"""Catch broken ABI bounds, lost DDS evidence and runtime fixture regressions."""
import json
import os
import pathlib
import subprocess
import tempfile
import unittest
from build_fixture import ROOT, compile_fixture, core_sources

class CoreTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (ROOT / 'core_bridge.h').exists():
            raise AssertionError('public C bridge is missing')
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.caller = compile_fixture(cls.temporary.name + '/caller', [ROOT / 'tests/core_caller.c'], core_sources(), sanitize=os.getenv('MB_CORE_SANITIZE') == '1')
        altered = pathlib.Path(cls.temporary.name) / 'failed-check.cpp'
        altered.write_text((ROOT / 'core_bridge.cpp').read_text().replace('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855', '0' * 64))
        cls.failed_check = compile_fixture(cls.temporary.name + '/failed-check', [ROOT / 'tests/core_caller.c'], [altered, *core_sources()[1:]])
        doubled_cleanup = pathlib.Path(cls.temporary.name) / 'double-cleanup.cpp'
        doubled_cleanup.write_text((ROOT / 'core_bridge.cpp').read_text().replace('destroyed=destroyed+1;', 'destroyed=destroyed+2;'))
        cls.double_cleanup = compile_fixture(cls.temporary.name + '/double-cleanup', [ROOT / 'tests/core_caller.c'], [doubled_cleanup, *core_sources()[1:]])
        cls.allocator = compile_fixture(cls.temporary.name + '/allocator', cpp_sources=[ROOT / 'tests/failing_allocator.cpp', *core_sources()])

    def run_c(self, mode):
        return subprocess.run([str(self.caller), mode], check=True, capture_output=True, text=True).stdout

    def test_actual_c_boundary_never_writes_partial_or_past_capacity(self):
        self.run_c('bounds')

    def test_allocation_failure_is_contained_and_subsequent_call_recovers(self):
        subprocess.run([str(self.allocator)], check=True)

    def test_selftest_checks_are_passed_and_repeated_results_are_deterministic(self):
        first = self.run_c('selftest')
        self.assertEqual(first, self.run_c('selftest'))
        report = json.loads(first)
        self.assertEqual(report['status'], 'passed')
        self.assertEqual(report['schema_version'], 1)
        self.assertEqual(report['core_version'], '0.1.0')
        self.assertEqual(report['scope'], 'owned_builtin_fixtures')
        self.assertFalse(report['installation_supported'])
        self.assertEqual(report['checks'], dict.fromkeys(('sha256_known_vector', 'allocator_roundtrip', 'exception_raii', 'thread_local_single_thread', 'dds_legacy_roundtrip', 'dds_cube_mip_roundtrip', 'dds_truncation_detected'), True))

    def test_false_check_reports_failed_without_hiding_individual_results(self):
        result = subprocess.run([str(self.failed_check), 'selftest'], check=True, capture_output=True, text=True)
        report = json.loads(result.stdout)
        self.assertEqual(report['status'], 'failed')
        self.assertFalse(report['checks']['sha256_known_vector'])
        self.assertEqual(sum(report['checks'].values()), 6)
        self.assertFalse(report['installation_supported'])

    def test_guard_counter_mismatch_reports_failed_even_when_exception_message_matches(self):
        result = subprocess.run([str(self.double_cleanup), 'selftest'], check=True, capture_output=True, text=True)
        report = json.loads(result.stdout)
        self.assertEqual(report['status'], 'failed')
        self.assertFalse(report['checks']['exception_raii'])
        self.assertEqual(sum(report['checks'].values()), 6)
        self.assertFalse(report['installation_supported'])

    def test_parser_outcomes_preserve_json_facts_and_unresolved_requirements(self):
        reasons = {'empty': 'magic_truncated', 'invalid': 'invalid_magic', 'unsupported': 'unsupported_format', 'opaque': 'unsupported_format', 'contradiction': 'input_size_contradiction', 'truncated': 'payload_truncated', 'prefix': 'inspection_prefix_incomplete', 'valid': 'complete'}
        for mode, reason in reasons.items():
            with self.subTest(mode=mode):
                report = json.loads(self.run_c(mode))
                self.assertEqual(report['facts']['reason'], reason)
                required = {r['id']: r for r in report['requirements']}
                for name in ('texture.ps5_writer', 'texture.game_acceptance'):
                    self.assertEqual(required[name]['status'], 'unresolved')
                    self.assertEqual(required[name]['consumer'], 'native_input.dds')
                if mode == 'opaque':
                    self.assertIsNone(report['facts']['format']['fourcc'])
                    self.assertEqual(report['facts']['format']['fourcc_bytes'], [255, 0, 128, 1])
                if mode == 'valid':
                    self.assertEqual(report['facts']['payload_expected_end'], 160)
                    self.assertEqual(report['facts']['trailer']['offset'], 160)
                    self.assertEqual(report['facts']['trailer']['size'], 3)
                else:
                    self.assertFalse(report['facts']['normal_form_eligible'])
