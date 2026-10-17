import os
import pathlib
import subprocess
import tempfile
import unittest

from build_fixture import compile_fixture

ROOT = pathlib.Path(__file__).resolve().parents[1]
PROJECT = ROOT.parent


class NexusAuthCatalogTests(unittest.TestCase):
    def test_local_admission_and_authenticated_catalog_projection(self):
        sanitize = os.getenv('MB_NEXUS_AUTH_CATALOG_SANITIZE') == '1'
        with tempfile.TemporaryDirectory(prefix='nexus-auth-catalog-') as temporary:
            binary = compile_fixture(
                pathlib.Path(temporary) / 'nexus-auth-catalog',
                cpp_sources=[
                    ROOT / 'tests/nexus_auth_catalog_driver.cpp',
                    ROOT / 'auth.cpp',
                    ROOT / 'oauth_code.cpp',
                    ROOT / 'nexus_client.cpp',
                    ROOT / 'nexus_bridge.cpp',
                    PROJECT / 'src/nexus.cpp',
                ],
                sanitize=sanitize,
            )
            result = subprocess.run(
                [str(binary)], capture_output=True, text=True,
                env={'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'ASAN_OPTIONS': 'detect_leaks=0'},
                timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stderr or result.stdout)
            self.assertIn('PASS auth-admission-and-catalog', result.stdout)
            self.assertNotIn('fixture_access_token', result.stdout + result.stderr)
            self.assertNotIn('fixture_refresh_token', result.stdout + result.stderr)
