"""Private acquisition capture remains source-bound across jobs and retention."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class AcquisitionMetadataTests(unittest.TestCase):
    def test_capture_admission_replay_and_retention(self):
        headers = list((ROOT / 'build').glob('**/include/mbedtls/sha256.h'))
        self.assertTrue(headers, 'cached checked MbedTLS headers required')
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp)
            def run(*args):
                subprocess.run([str(a) for a in args], check=True)
            for name in ('fd_io', 'posix_compat'):
                run('cc', '-std=c11', '-c', ROOT / 'archive/private' / f'{name}.c', '-o', out / f'{name}.o')
            run('c++', '-std=c++20', '-I', ROOT.parent / 'third_party', '-I', headers[0].parents[1],
                ROOT / 'tests/acquisition_metadata_jobs_tests.cpp', ROOT / 'download_store/store.cpp',
                out / 'fd_io.o', out / 'posix_compat.o', '-o', out / 'test')
            for case in ('saved', 'grant_retry', 'unknown', 'sidecar_failure', 'direct', 'wrong_order', 'wrong_file', 'missing',
                         'oversize', 'wrong_requirements', 'incomplete', 'unexpected', 'duplicate', 'bad_length'):
                with self.subTest(case=case):
                    directory = out / case
                    directory.mkdir()
                    run(out / 'test', directory, case)
