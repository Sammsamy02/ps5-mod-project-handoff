"""Offline original archive survives job cleanup and store re-open."""
import pathlib, subprocess, tempfile, unittest
ROOT=pathlib.Path(__file__).resolve().parents[1]
class RetentionTests(unittest.TestCase):
 def test_worker_retains_unsupported_original(self):
  headers=list((ROOT/'build').glob('**/include/mbedtls/sha256.h'))
  self.assertTrue(headers, 'cached checked MbedTLS headers required')
  with tempfile.TemporaryDirectory() as tmp:
   out=pathlib.Path(tmp)
   def run(*args): subprocess.run([str(a) for a in args],check=True)
   for name in ('fd_io','posix_compat'):
    run('cc','-std=c11','-c',ROOT/'archive/private'/f'{name}.c','-o',out/f'{name}.o')
   run('c++','-std=c++20','-I',ROOT.parent/'third_party','-I',headers[0].parents[1],ROOT/'tests/download_retention_jobs_tests.cpp',ROOT/'download_store/store.cpp',out/'fd_io.o',out/'posix_compat.o','-o',out/'test')
   run(out/'test',out)
