"""Catch entropy fallback, misclassification, stale evidence, and incomplete wipes."""
import ctypes
import json
import os
import subprocess
import tempfile
import unittest

from build_fixture import ROOT, compile_fixture


class EntropyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.temporary.cleanup)
        sources = [ROOT / 'entropy.c', ROOT / 'http.c', ROOT / 'tests/web_assets_stub.c',
                   ROOT / 'tests/entropy_caller.c']
        flags = ['-O2', '-D_POSIX_C_SOURCE=200809L',
                 '-I' + str(ROOT / 'tests/entropy_shim')]
        sanitize = os.getenv('MB_ENTROPY_SANITIZE') == '1'
        cls.native = compile_fixture(cls.temporary.name + '/native', sources,
                                     defines=[*flags, '-DHOST_NATIVE=1'],
                                     sanitize=sanitize)
        cls.desktop = compile_fixture(cls.temporary.name + '/desktop', sources,
                                      defines=flags, sanitize=sanitize)

    def observe(self, return_code=0, error_number=0, returned_length=32,
                written_length=32, before=(0, 7, 900000000),
                after=(0, 9, 100999999), desktop=False):
        arguments = [return_code, error_number, returned_length, written_length,
                     *before, *after]
        result = subprocess.run([str(self.desktop if desktop else self.native),
                                 *(str(value) for value in arguments)],
                                check=True, capture_output=True, text=True)
        self.assertEqual(result.stderr, '')
        report = json.loads(result.stdout)
        self.assertEqual(report['scratch_cleared'], 1)
        self.assertEqual(report['before_canary_ok'], 1)
        self.assertEqual(report['after_canary_ok'], 1)
        self.assertEqual(report['http_status'], 200)
        self.assertEqual(report['report_cached'], 1)
        if not desktop:
            self.assertEqual(report['call_count'], 1)
            self.assertEqual(report['syscall_calls'], 1)
            self.assertEqual(report['clock_calls'], 2)
            for key in ('mib_ok', 'buffer_ok', 'request_length_ok',
                        'new_value_ok', 'call_order_ok', 'clock_ids_ok'):
                self.assertEqual(report[key], 1, key)
            self.assertEqual(report['incoming_errno'], 0)
        return report

    def assert_serialized(self, report, status, reason, code, error, length,
                          elapsed, call_count=1):
        # Exact field equality rejects raw bytes, pointers, hashes, and extra facts.
        self.assertEqual(report['report']['observations']['entropy'], {
            'status': status, 'reason': reason, 'api': 'sysctl(KERN_ARND)',
            'scope': 'api_result_only', 'requested_bytes': 32,
            'call_count': call_count, 'return_code': code, 'errno': error,
            'returned_length_bytes': length, 'elapsed_ms': elapsed,
            'hard_deadline_enforced': False,
        })

    def test_native_classifies_raw_return_without_retry_or_partial_wipe(self):
        cases = (
            ('partial_error', -1, 7, 5, 5, 'failed', 'api_return'),
            ('error_without_errno', -1, 0, 32, 9, 'failed', 'api_return'),
            ('positive_return', 2, 13, 0, 3, 'failed', 'api_return'),
            ('success_with_errno', 0, 9, 0, 7, 'failed', 'errno_nonzero'),
            ('no_progress', 0, 0, 0, 0, 'failed', 'no_progress'),
            ('short_one', 0, 0, 1, 1, 'failed', 'short_result'),
            ('short_thirty_one', 0, 0, 31, 31, 'failed', 'short_result'),
            ('exact', 0, 0, 32, 32, 'passed', 'exact_result'),
            ('oversized', 0, 0, 33, 32, 'failed', 'oversized_result'),
            ('size_max', 0, 0, 'max', 32, 'failed', 'oversized_result'),
        )
        size_max = (1 << (ctypes.sizeof(ctypes.c_size_t) * 8)) - 1
        for name, code, error, length, written, status, reason in cases:
            with self.subTest(case=name):
                report = self.observe(code, error, length, written)
                self.assertEqual(report['status'], status)
                self.assertEqual(report['reason'], reason)
                self.assertEqual(report['return_code'], code)
                self.assertEqual(report['error_number'], error)
                wanted_length = size_max if length == 'max' else length
                self.assertEqual(report['returned_length'], wanted_length)
                self.assertEqual(report['elapsed_ms'], 1200)
                self.assert_serialized(report, status, reason, code, error,
                                       wanted_length, 1200)

    def test_errno_is_cleared_after_preclock_and_captured_before_postclock(self):
        report = self.observe(error_number=-1)
        self.assertEqual(report['status'], 'passed')
        self.assertEqual(report['reason'], 'exact_result')
        self.assertEqual(report['error_number'], 0)
        self.assertEqual(report['returned_length'], 32)
        self.assert_serialized(report, 'passed', 'exact_result', 0, 0, 32, 1200)

    def test_native_http_report_preserves_size_max_after_partial_api_failure(self):
        report = self.observe(return_code=-1, error_number=7,
                              returned_length='max', written_length=5)
        size_max = (1 << (ctypes.sizeof(ctypes.c_size_t) * 8)) - 1
        self.assert_serialized(report, 'failed', 'api_return', -1, 7,
                               size_max, 1200)

    def test_zero_progress_still_wipes_bytes_written_before_zero_length(self):
        report = self.observe(returned_length=0, written_length=32)
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(report['reason'], 'no_progress')
        self.assert_serialized(report, 'failed', 'no_progress', 0, 0, 0, 1200)

    def test_elapsed_duration_floors_after_subtraction_across_second_boundary(self):
        report = self.observe(before=(0, 7, 999999999), after=(0, 8, 0))
        self.assertEqual(report['elapsed_ms'], 0)
        self.assert_serialized(report, 'passed', 'exact_result', 0, 0, 32, 0)

    def test_backward_clock_within_one_millisecond_is_unavailable(self):
        report = self.observe(before=(0, 8, 500001), after=(0, 8, 500000))
        self.assertEqual(report['elapsed_ms'], -1)
        self.assert_serialized(report, 'passed', 'exact_result', 0, 0, 32, None)

    def test_invalid_clocks_leave_timing_unavailable_without_hiding_api_evidence(self):
        cases = (
            ('before_call_failed', (-1, 7, 0), (0, 9, 0)),
            ('after_call_failed', (0, 7, 0), (-1, 9, 0)),
            ('before_negative_seconds', (0, -1, 0), (0, 9, 0)),
            ('after_negative_seconds', (0, 7, 0), (0, -1, 0)),
            ('before_negative_nanoseconds', (0, 7, -1), (0, 9, 0)),
            ('after_negative_nanoseconds', (0, 7, 0), (0, 9, -1)),
            ('before_nanoseconds_overflow', (0, 7, 1000000000), (0, 9, 0)),
            ('after_nanoseconds_overflow', (0, 7, 0), (0, 9, 1000000000)),
            ('seconds_backwards', (0, 9, 0), (0, 7, 0)),
            ('nanoseconds_backwards', (0, 7, 900000000), (0, 7, 800000000)),
            ('before_conversion_overflow', (0, 9223372036854775807, 0), (0, 9, 0)),
            ('after_conversion_overflow', (0, 7, 0), (0, 9223372036854775807, 0)),
        )
        for name, before, after in cases:
            with self.subTest(case=name):
                report = self.observe(return_code=-1, error_number=7,
                                      returned_length=5, written_length=5,
                                      before=before, after=after)
                self.assertEqual(report['status'], 'failed')
                self.assertEqual(report['reason'], 'api_return')
                self.assertEqual(report['return_code'], -1)
                self.assertEqual(report['error_number'], 7)
                self.assertEqual(report['returned_length'], 5)
                self.assertEqual(report['elapsed_ms'], -1)
                self.assert_serialized(report, 'failed', 'api_return', -1, 7, 5, None)

    def test_desktop_default_clears_scratch_without_syscall_or_clock(self):
        report = self.observe(desktop=True)
        self.assertEqual(report['status'], 'unavailable')
        self.assertEqual(report['reason'], 'desktop_fixture')
        self.assertEqual(report['call_count'], 0)
        self.assertEqual(report['syscall_calls'], 0)
        self.assertEqual(report['clock_calls'], 0)
        self.assertEqual(report['elapsed_ms'], -1)
        self.assert_serialized(report, 'unavailable', 'desktop_fixture',
                               None, None, None, None, call_count=0)


if __name__ == '__main__':
    unittest.main()
