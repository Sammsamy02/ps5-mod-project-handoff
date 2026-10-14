"""Real pinned crypto; fake only OS/clock and explicit fault/observation seams.

Breaks caught: reset budget, raw-byte reuse, wrong personalization, extra entropy
or generation, silent fallback/retry, lost source metadata, incomplete erasure,
byte-bearing report, and inappropriate desktop cryptographic execution.
"""
import ctypes
import hashlib
import json
import os
import pathlib
import subprocess
import tempfile
import unittest

from build_fixture import ROOT, compile_fixture, instrument_crypto_fixture

DESCRIPTOR = ROOT / 'build/crypto/task1-fix1-evidence/host-green-descriptor.json'
PRODUCTION = [ROOT / name for name in ('drbg.c', 'drbg.h', 'drbg_internal.h')]


class DrbgTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.crypto = json.loads(pathlib.Path(os.getenv('MB_DRBG_CRYPTO_DESCRIPTOR', DESCRIPTOR)).read_text())
        cls.flags = ['-D' + flag for flag in cls.crypto['compile_definitions']]
        cls.sanitize = os.getenv('MB_DRBG_SANITIZE') == '1'
        cls.library = instrument_crypto_fixture(cls.crypto) if cls.sanitize else cls.crypto['archive']
        cls.reference = compile_fixture(pathlib.Path(cls.temporary.name) / 'reference',
            [ROOT / 'tests/drbg_reference.c'], defines=cls.flags,
            include_dirs=cls.crypto['include_dirs'], libraries=[cls.library], sanitize=cls.sanitize)
        cls.native = None

    def fixture(self, mode, *arguments):
        self.assertTrue(all(p.is_file() for p in PRODUCTION),
                        'bounded startup checkpoint implementation is missing')
        if self.__class__.native is None:
            wrappers = ['-Dmbedtls_entropy_add_source=fixture_add_source',
                        '-Dmbedtls_entropy_func=fixture_entropy_func',
                        '-Dmbedtls_ctr_drbg_seed=fixture_seed',
                        '-Dmbedtls_ctr_drbg_random=fixture_random']
            self.__class__.native = compile_fixture(pathlib.Path(self.temporary.name) / 'native',
                [ROOT / 'entropy.c', ROOT / 'drbg.c', ROOT / 'http.c', ROOT / 'tests/web_assets_stub.c',
                 ROOT / 'tests/drbg_caller.c'],
                defines=[*self.flags, '-DHOST_NATIVE=1', '-DMB_DRBG_TESTING=1', '-O2',
                         '-DMB_CRYPTO_CONFIG_ID="' + self.crypto['config_id'] + '"',
                         '-I' + str(ROOT / 'tests/entropy_shim')],
                include_dirs=self.crypto['include_dirs'], libraries=[self.library],
                source_defines={'drbg.c': wrappers}, sanitize=self.sanitize)
        run = subprocess.run([str(self.native), mode, *(str(arg) for arg in arguments)],
                             check=True, capture_output=True, text=True)
        self.assertEqual(run.stderr, '')
        actual = json.loads(run.stdout)
        self.assertEqual(actual['contracts_ok'], 1)
        self.assertEqual(actual['prohibited_calls'], 0)
        self.assertEqual(actual['workspace_cleared'], 1)
        self.assertEqual(actual['canaries_ok'], 1)
        return actual

    def checkpoint(self, phase=0, code=0, error=0, length=32, written=32, fault=0):
        actual = self.fixture('checkpoint', phase, code, error, length, written, fault)
        self.assertEqual(actual['cached'], 1)
        self.assertEqual(actual['http_status'], 200)
        self.assertLess(actual['body_size'], 8192)
        actual['drbg'] = actual['report']['observations']['drbg']
        self.assertEqual(set(actual['drbg']), {
            'status', 'reason', 'scope', 'mbedtls_version', 'config_id',
            'requested_generation_bytes', 'add_source_rc', 'seed_rc', 'generate_rc',
            'completed_length_bytes', 'source_callback_count', 'source_os_call_count',
            'total_api_call_count', 'api_call_limit', 'elapsed_ms',
            'hard_deadline_enforced', 'source_failure'})
        self.assertEqual(actual['drbg']['config_id'], self.crypto['config_id'])
        self.assertEqual(actual['drbg']['mbedtls_version'], '3.6.7')
        self.assertEqual(actual['drbg']['scope'], 'owned_startup_checkpoint')
        self.assertEqual(actual['drbg']['api_call_limit'], 4)
        self.assertEqual(actual['drbg']['requested_generation_bytes'], 32)
        self.assertFalse(actual['drbg']['hard_deadline_enforced'])
        return actual

    def test_pinned_upstream_second_generation_kat_independently_passes(self):
        run = subprocess.run([str(self.reference)], check=True, capture_output=True, text=True)
        self.assertEqual(run.stderr, '')
        self.assertEqual(json.loads(run.stdout), {'kat': True})

    def test_full_default_shape_uses_three_calls_and_frozen_first_generation(self):
        actual = self.checkpoint()
        for field, wanted in {'os_calls': 3, 'add_calls': 1, 'seed_calls': 1,
                              'random_calls': 1, 'entropy_calls': 1, 'source_entries': 2,
                              'observer_calls': 1, 'observer_match': 1}.items():
            self.assertEqual(actual[field], wanted, field)
        self.assertEqual({k: actual['drbg'][k] for k in (
            'status', 'reason', 'add_source_rc', 'seed_rc', 'generate_rc',
            'completed_length_bytes', 'source_callback_count', 'source_os_call_count',
            'total_api_call_count', 'source_failure')}, {
            'status': 'passed', 'reason': 'checkpoint_complete', 'add_source_rc': 0,
            'seed_rc': 0, 'generate_rc': 0, 'completed_length_bytes': 32,
            'source_callback_count': 2, 'source_os_call_count': 2,
            'total_api_call_count': 3, 'source_failure': None})

    def test_raw_rejection_skips_all_mbed_operations(self):
        actual = self.checkpoint(phase=1, code=-1, error=4, length=5, written=5)
        self.assertEqual(actual['os_calls'], 1)
        self.assertEqual(actual['report']['observations']['entropy']['reason'], 'api_return')
        for field in ('add_calls', 'seed_calls', 'random_calls', 'entropy_calls', 'source_entries', 'observer_calls'):
            self.assertEqual(actual[field], 0)
        self.assertEqual(actual['drbg']['status'], 'unavailable')
        self.assertEqual(actual['drbg']['reason'], 'raw_prerequisite_failed')
        for field in ('add_source_rc', 'seed_rc', 'generate_rc', 'completed_length_bytes', 'source_failure'):
            self.assertIsNone(actual['drbg'][field])
        self.assertEqual(actual['drbg']['total_api_call_count'], 1)

    def test_each_first_and_second_poll_rejection_is_strict_and_never_retried(self):
        cases = [(-1, 7, 5, 5, 'api_return'), (-1, 0, 32, 7, 'api_return'),
                 (-8, 4, 32, 7, 'api_return'), (2, 0, 32, 7, 'api_return'),
                 (0, 9, 32, 7, 'errno_nonzero'), (0, 0, 0, 32, 'no_progress'),
                 (0, 0, 33, 32, 'oversized_result'), (0, 0, 'max', 32, 'oversized_result')]
        cases += [(0, 0, length, length, 'short_result') for length in range(1, 32)]
        size_max = (1 << (ctypes.sizeof(ctypes.c_size_t) * 8)) - 1
        for phase in (2, 3):
            for code, error, length, written, reason in cases:
                with self.subTest(phase=phase, code=code, error=error, length=length):
                    actual = self.checkpoint(phase, code, error, length, written)
                    self.assertEqual(actual['os_calls'], phase)
                    self.assertEqual(actual['source_entries'], phase - 1)
                    self.assertEqual(actual['entropy_calls'], 1)
                    self.assertEqual(actual['random_calls'], 0)
                    self.assertEqual(actual['observer_calls'], 0)
                    self.assertEqual(actual['drbg']['status'], 'failed')
                    self.assertEqual(actual['drbg']['reason'], 'seed_failed')
                    self.assertEqual(actual['drbg']['seed_rc'], -52)
                    self.assertIsNone(actual['drbg']['generate_rc'])
                    self.assertEqual(actual['drbg']['source_failure'], {
                        'reason': reason, 'policy_failure': None, 'return_code': code,
                        'errno': error, 'returned_length_bytes': size_max if length == 'max' else length})
                    self.assertEqual(actual['drbg']['source_callback_count'], phase - 1)
                    self.assertEqual(actual['drbg']['source_os_call_count'], phase - 1)
                    self.assertEqual(actual['drbg']['total_api_call_count'], phase)

    def test_no_sources_and_independent_extraction_use_no_defaults_and_two_polls(self):
        empty = self.fixture('no_sources', 128, 0, 0)
        self.assertEqual(empty['rc'], -64)
        self.assertEqual(empty['os_calls'], 0)
        self.assertEqual(empty['source_entries'], 0)
        extracted = self.fixture('extract', 128, 0, 0)
        self.assertEqual(extracted['rc'], 0)
        self.assertEqual(extracted['os_calls'], 2)
        self.assertEqual(extracted['source_entries'], 2)
        self.assertEqual(extracted['callbacks'], 2)
        self.assertEqual(extracted['total'], 2)

    def test_invalid_callback_storage_and_exhausted_budget_never_call_os(self):
        for capacity in (0, 1, 31, 129, 'max'):
            with self.subTest(capacity=capacity):
                actual = self.fixture('callback', capacity, 0, 0)
                self.assertEqual(actual['rc'], -60)
                self.assertEqual(actual['os_calls'], 0)
                self.assertEqual(actual['length'], 0)
        for invalid in range(1, 6):
            with self.subTest(invalid=invalid):
                actual = self.fixture('callback', 128, invalid, 0)
                self.assertEqual(actual['rc'], -60)
                self.assertEqual(actual['os_calls'], 0)
                if invalid != 3: self.assertEqual(actual['length'], 0)
                if invalid != 2: self.assertEqual(actual['output_cleared'], 1)
        for capacity in (32, 64, 128):
            actual = self.fixture('callback', capacity, 0, 4)
            self.assertEqual(actual['rc'], -60)
            self.assertEqual(actual['os_calls'], 0)
            self.assertEqual(actual['length'], 0)
            self.assertEqual(actual['output_cleared'], 1)
            self.assertEqual(actual['staging_cleared'], 1)
            self.assertEqual(actual['policy'], 'budget_exhausted')

    def test_reseed_allows_fourth_api_then_latches_denial_with_last_actual_metadata(self):
        actual = self.fixture('reseed', 128, 0, 0)
        self.assertEqual(actual['rc'], 0)
        self.assertEqual(actual['second_rc'], -52)
        self.assertEqual(actual['os_calls'], 4)
        self.assertEqual(actual['source_os_calls'], 3)
        self.assertEqual(actual['callbacks'], 5)
        self.assertEqual(actual['total'], 4)
        self.assertEqual(actual['policy'], 'budget_exhausted')
        self.assertEqual(actual['failed'], 1)
        self.assertEqual((actual['has_last'], actual['last_rc'], actual['last_errno'], actual['last_length']), (1, 0, 0, 32))
        self.assertEqual(actual['output_cleared'], 1)
        self.assertEqual(actual['staging_cleared'], 1)
        self.assertEqual(actual['report']['observations']['drbg']['source_failure'], {
            'reason':'exact_result','policy_failure':'budget_exhausted',
            'return_code':0,'errno':0,'returned_length_bytes':32})

    def test_invalid_and_backward_checkpoint_clocks_leave_api_metadata_truthful(self):
        for fault in (4,5):
            actual=self.checkpoint(fault=fault)
            self.assertEqual(actual['drbg']['status'],'passed')
            self.assertEqual(actual['os_calls'],3)
            self.assertIsNone(actual['drbg']['elapsed_ms'])
            self.assertEqual(actual['observer_match'],1)

    def test_callback_rejections_wipe_the_full_bounded_capacity_before_latched_reentry(self):
        cases = [(-1, 7, 5, 5), (-1, 0, 32, 7), (-8, 4, 32, 7),
                 (2, 0, 32, 7), (0, 9, 32, 7), (0, 0, 0, 32),
                 (0, 0, 33, 32), (0, 0, 'max', 32)]
        cases += [(0, 0, length, length) for length in range(1, 32)]
        for capacity in (32, 64, 128):
            for code, error, length, written in cases:
                with self.subTest(capacity=capacity, code=code, error=error, length=length):
                    actual=self.fixture('callback',capacity,0,0,code,error,length,written)
                    self.assertEqual(actual['rc'], -60)
                    self.assertEqual(actual['second_rc'], -60)
                    self.assertEqual(actual['os_calls'], 1)
                    self.assertEqual(actual['source_os_calls'], 1)
                    self.assertEqual(actual['callbacks'], 1)
                    self.assertEqual(actual['length'], 0)
                    self.assertEqual(actual['output_cleared'], 1)
                    self.assertEqual(actual['staging_cleared'], 1)
                    self.assertEqual(actual['tail_untouched'], 1)

    def test_add_source_real_allocation_and_partial_generation_failures_clear_workspace(self):
        for fault, reason, add, seed, generate, calls in (
            (1, 'add_source_failed', -62, None, None, 1),
            (2, 'seed_failed', 0, -52, None, 2),
            (3, 'generate_failed', 0, 0, -54, 3)):
            with self.subTest(fault=fault):
                actual = self.checkpoint(fault=fault)
                self.assertEqual(actual['drbg']['reason'], reason)
                self.assertEqual(actual['drbg']['add_source_rc'], add)
                self.assertEqual(actual['drbg']['seed_rc'], seed)
                self.assertEqual(actual['drbg']['generate_rc'], generate)
                self.assertEqual(actual['os_calls'], calls)
                self.assertEqual(actual['observer_calls'], 0)
                self.assertIsNone(actual['drbg']['completed_length_bytes'])


if __name__ == '__main__':
    unittest.main()
