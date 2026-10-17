import importlib.util
import pathlib
import struct
import sys
import subprocess
import tempfile
import unittest
import zipfile
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
REQUIRED_SOURCES = tuple('outputs/ps5-mod-bridge/' + name for name in (
    'native-host/host.c', 'native-host/http.c', 'native-host/http.h',
    'native-host/entropy.c', 'native-host/entropy.h',
    'native-host/drbg.c', 'native-host/drbg.h', 'native-host/drbg_internal.h',
    'native-host/crypto_config.h', 'native-host/crypto_build.py',
    'native-host/core_bridge.cpp', 'native-host/core_bridge.h',
    'native-host/nexus_client.cpp', 'native-host/nexus_client.h',
    'native-host/nexus_download.cpp', 'native-host/nexus_download.h',
    'native-host/auth.cpp', 'native-host/auth.h', 'native-host/oauth_code.cpp',
    'native-host/oauth_code.h', 'native-host/oauth_config.py',
    'native-host/download/oauth_code_transfer.h',
    'native-host/nexus_bridge.cpp', 'native-host/nexus_bridge.h',
    'native-host/download/runtime.h', 'native-host/download/provider_transfer.h',
    'native-host/build.py', 'native-host/Makefile', 'native-host/web_assets.py',
    'web/index.html', 'web/app.js', 'web/style.css', 'src/dds.cpp', 'src/hash.cpp',
    'include/modbridge/dds.hpp', 'include/modbridge/core.hpp',
    'src/nexus.cpp', 'include/modbridge/nexus.hpp',
    'third_party/nlohmann/json.hpp', 'third_party/picosha2/picosha2.h'))

class BuildTests(unittest.TestCase):
    def test_download_store_cli_and_identity(self):
        source = (ROOT / 'build.py').read_text()
        self.assertIn('--download-store-directory', source)
        self.assertIn('MB_HOST_DOWNLOAD_STORE_DIRECTORY', source)
        self.assertIn('native-host/download_store/store.cpp', source)
        self.assertIn('native-host/download_store/store.h', source)

    def setUp(self):
        self.assertTrue((ROOT / 'build.py').exists(), 'verified native SDK build wrapper is missing')
        spec = importlib.util.spec_from_file_location('native_host_build', ROOT / 'build.py')
        self.build = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.build)

    def test_identity_covers_bridge_shared_and_vendor_inputs_with_repository_keys(self):
        import shutil
        with tempfile.TemporaryDirectory() as temporary:
            repository = pathlib.Path(temporary)
            project = repository / 'outputs/ps5-mod-bridge'
            for relative in set(REQUIRED_SOURCES) | set(self.build.SOURCE_INPUTS):
                destination = repository / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(self.build.REPOSITORY / relative, destination)
            original_root = self.build.ROOT
            self.build.ROOT = project / 'native-host'
            try:
                config = self.build.profile_config('loopback', None)
                crypto = {'config_id': 'first-config', 'archive_sha256': 'first-archive'}
                before, metadata = self.build.identity(config, crypto)
                self.assertEqual(set(metadata['source_sha256']), set(self.build.SOURCE_INPUTS))
                self.assertGreaterEqual(set(metadata['source_sha256']), set(REQUIRED_SOURCES))
                for relative in REQUIRED_SOURCES:
                    path = repository / relative
                    original = path.read_bytes()
                    path.write_bytes(original + b'\n')
                    self.assertNotEqual(self.build.identity(config, crypto)[0], before)
                    path.write_bytes(original)
                self.assertNotEqual(self.build.identity(self.build.profile_config('development_lan', 'ps5-host-not-configured.invalid'), crypto)[0], before)
                for key in ('config_id', 'archive_sha256'):
                    changed = {**crypto, key: 'changed'}
                    self.assertNotEqual(self.build.identity(config, changed)[0], before)
            finally:
                self.build.ROOT = original_root

    def test_cached_cpp_driver_must_match_pinned_clang_bytes(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = pathlib.Path(temporary)
            for name in self.build.TOOL_HASHES:
                (directory / name).write_bytes(b'untrusted')
            with self.assertRaisesRegex(ValueError, r'clang\+\+'):
                self.build.verify_cpp_driver(directory)

    def test_native_identity_requires_the_actual_crypto_dependency_identity(self):
        config = self.build.profile_config('loopback', None)
        with self.assertRaisesRegex(ValueError, 'crypto'):
            self.build.identity(config)
        desktop = self.build.profile_config('desktop_fixture', None)
        self.assertNotIn('crypto', self.build.identity(desktop)[1])

    def test_native_cli_rejects_missing_explicit_archives_before_building(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run([sys.executable, str(ROOT / 'build.py'),
                                     '--profile', 'loopback', '--output-root', temporary],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('--mbedtls-archive and --framework-archive', result.stderr)
            self.assertEqual(list(pathlib.Path(temporary).iterdir()), [])

    def test_download_cli_requires_both_profile_and_source_before_any_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            for option in ('--download-profile', '--curl-source'):
                result = subprocess.run([sys.executable, str(ROOT / 'build.py'),
                                         '--profile', 'desktop_fixture', option, temporary,
                                         '--output-root', temporary],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('download builds require both --download-profile and --curl-source', result.stderr)
                self.assertEqual(list(pathlib.Path(temporary).iterdir()), [])

    def test_oauth_cli_requires_download_runtime_before_any_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run([sys.executable, str(ROOT / 'build.py'),
                '--profile', 'desktop_fixture', '--oauth-config', temporary,
                '--output-root', temporary], capture_output=True, text=True)
            self.assertEqual(result.returncode, 2)
            self.assertIn('--oauth-config requires the download runtime', result.stderr)
            self.assertEqual(list(pathlib.Path(temporary).iterdir()), [])

    def test_public_oauth_config_is_bound_into_build_identity(self):
        config = self.build.profile_config('desktop_fixture', None)
        config['oauth'] = {'configured': False, 'credential_storage': 'process_lifetime'}
        with mock.patch.object(self.build, 'digest', return_value='owned-source'):
            before, _ = self.build.identity(config)
            config['oauth']['configured'] = True
            self.assertNotEqual(before, self.build.identity(config)[0])

    def test_job_directory_is_explicit_bounded_and_canonical(self):
        self.assertEqual(self.build.job_config(None), {'enabled': False})
        value = self.build.job_config('/data/modbridge/downloads')
        self.assertEqual(value['directory'], '/data/modbridge/downloads')
        self.assertTrue(value['enabled'])
        for path in ('relative', '/', '/data//mods', '/data/../mods',
                     '/data/./mods', '/data/mods/', '/data/a\0b', '/data/a"b'):
            with self.subTest(path=path), self.assertRaises(ValueError):
                self.build.job_config(path)

    def test_job_cli_requires_paired_profile_and_directory_before_any_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            for option in ('--archive-profile', '--job-directory'):
                result = subprocess.run([sys.executable, str(ROOT / 'build.py'),
                    '--profile', 'desktop_fixture', option, temporary,
                    '--output-root', temporary], capture_output=True, text=True)
                self.assertEqual(result.returncode, 2)
                self.assertIn('jobs require both --archive-profile and --job-directory', result.stderr)
                self.assertEqual(list(pathlib.Path(temporary).iterdir()), [])

    def test_download_composition_excludes_incompatible_legacy_drbg(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = pathlib.Path(temporary)
            calls = []
            with mock.patch.object(self.build, 'run_recorded', side_effect=lambda cmd, commands, env: calls.append(cmd) or ''):
                objects = self.build.compile_objects('native-cc', 'native-cxx', output,
                    output / 'config.h', {}, output / 'assets.c',
                    {'include_dirs': [], 'compile_definitions': [],
                     'native_compile_options': [], 'forced_includes': []}, [],
                    download=True, native=True)
            sources = {pathlib.Path(cmd[cmd.index('-c') + 1]).name for cmd in calls}
            self.assertNotIn('drbg.c', sources)
            self.assertNotIn('entropy.c', sources)
            self.assertTrue({'owner.c', 'curl_download.c', 'file_sink.c', 'ps5_entropy.c'} <= sources)
            self.assertTrue({'nexus_client.cpp', 'nexus_bridge.cpp', 'nexus.cpp'} <= sources,
                            'download builds must include the actual typed Nexus client and decoder')
            self.assertIn('nexus_download.cpp', sources,
                          'download builds must include live account-bound file resolution')
            self.assertTrue({'auth.cpp', 'oauth_code.cpp'} <= sources,
                            'download composition must include native account and code exchange')
            self.assertEqual(len(objects), len(sources))

    def test_desktop_download_identity_binds_the_actual_dependency(self):
        config = self.build.profile_config('desktop_fixture', None)
        config['download_runtime'] = 'curl_8_18_exclusive_owner'
        with mock.patch.object(self.build, 'digest', return_value='owned-source'):
            first, metadata = self.build.identity(config, {'archive_sha256': 'first'})
            second, _ = self.build.identity(config, {'archive_sha256': 'second'})
        self.assertNotEqual(first, second)
        self.assertEqual(metadata['crypto']['archive_sha256'], 'first')

    def test_safe_archive_rejects_escape_before_any_write(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            archive = root / 'bad.zip'
            with zipfile.ZipFile(archive, 'w') as z:
                z.writestr('ps5-payload-sdk/bin/good', 'x')
                z.writestr('../escape', 'x')
            with self.assertRaises(ValueError): self.build.extract_sdk(archive, root / 'output')
            self.assertFalse((root / 'output').exists())
            self.assertFalse((root / 'escape').exists())

    def test_safe_archive_rejects_symlinks_and_duplicate_members(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for kind in ('symlink', 'duplicate'):
                archive = root / f'{kind}.zip'
                with zipfile.ZipFile(archive, 'w') as z:
                    if kind == 'symlink':
                        info = zipfile.ZipInfo('ps5-payload-sdk/link'); info.create_system = 3
                        info.external_attr = (0o120777 << 16)
                        z.writestr(info, '../../escape')
                    else:
                        z.writestr('ps5-payload-sdk/a', 'x')
                        with self.assertWarns(UserWarning): z.writestr('ps5-payload-sdk/a', 'y')
                with self.assertRaises(ValueError): self.build.extract_sdk(archive, root / kind)
                self.assertFalse((root / kind).exists())

    def test_elf_rejects_program_table_outside_transmitted_prefix(self):
        # A complete on-disk table may still be absent from the loader's section-derived send.
        elf = bytearray(256)
        ident = b'\x7fELF\x02\x01\x01' + bytes(9)
        struct.pack_into('<16sHHIQQQIHHHHHH', elf, 0,
                         ident, 3, 62, 1, 0, 64, 192, 0, 64, 56, 1, 64, 1, 0)
        struct.pack_into('<IIQQQQQQ', elf, 64, 1, 5, 0, 0, 0, 128, 128, 1)
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / 'fixture.elf'
            path.write_bytes(elf)
            self.assertEqual(self.build.elf_layout(path)['loader_prefix'], 256)
            headers = bytes(elf[64:120])
            struct.pack_into('<Q', elf, 32, len(elf))
            elf.extend(headers)
            path.write_bytes(elf)
            with self.assertRaisesRegex(ValueError, 'table.*loader prefix'):
                self.build.elf_layout(path)

    def test_development_profile_rejects_non_console_bind_addresses(self):
        for address in ('0.0.0.0', '127.0.0.1', '224.1.2.3', '::1', 'example.com', None):
            with self.subTest(address=address):
                with self.assertRaises(ValueError): self.build.profile_config('development_lan', address)
        config = self.build.profile_config('development_lan', 'ps5-host-not-configured.invalid')
        self.assertEqual(config['address'], 'ps5-host-not-configured.invalid')
        self.assertEqual(config['lifetime'], 600)
        self.assertEqual(self.build.profile_config('loopback', None)['address'], '127.0.0.1')

if __name__ == '__main__': unittest.main()
