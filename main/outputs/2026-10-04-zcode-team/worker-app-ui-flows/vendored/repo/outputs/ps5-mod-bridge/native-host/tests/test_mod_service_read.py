"""Offline fake-service and auth-admission wiring checks; no network."""
import pathlib
import subprocess
import tempfile
import unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
PROJECT = ROOT.parent
class ReadBridgeTests(unittest.TestCase):
    def test_bridge_and_session_routes(self):
        with tempfile.TemporaryDirectory(prefix='mod-service-test-') as tmp:
            out = pathlib.Path(tmp)
            def run(*args):
                subprocess.run([str(x) for x in args], check=True)
            includes = ['-I', ROOT, '-I', PROJECT / 'third_party']
            run('c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', *includes,
                '-c', ROOT / 'mod_service/read_bridge.cpp', '-o', out / 'bridge.o')
            run('c++', '-std=c++20', *includes, ROOT / 'tests/mod_service_read_tests.cpp',
                out / 'bridge.o', '-o', out / 'bridge-test')
            run(out / 'bridge-test')
            for name, source, flags in [
                ('http', ROOT / 'http.c', ['-DMB_HOST_DOWNLOAD=1', '-DMB_HOST_DOWNLOAD_STORE=1']),
                ('test', ROOT / 'tests/mod_service_http_tests.c', []),
                ('assets', ROOT / 'tests/web_assets_stub.c', [])]:
                run('cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *includes,
                    *flags, '-c', source, '-o', out / (name + '.o'))
            run('c++', out / 'http.o', out / 'test.o', out / 'assets.o',
                out / 'bridge.o', '-o', out / 'http-test')
            run(out / 'http-test')
if __name__ == '__main__':
    unittest.main()
