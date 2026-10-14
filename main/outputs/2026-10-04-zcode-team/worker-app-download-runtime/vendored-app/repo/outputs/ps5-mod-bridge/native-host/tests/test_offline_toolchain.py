"""Focused tests for explicit use of the pinned extracted LLVM members."""
import hashlib
import importlib.util
import json
import os
import pathlib
import shutil
import stat
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parents[2]
sys.path.insert(0, str(ROOT))
SDK = ROOT / 'build/sdk-v0.43/ps5-payload-sdk'
LLVM_BIN = ROOT.parent / 'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
SELECTED_ROOT = (ROOT / 'build/crypto/task1-fix1-evidence/artifacts/llvm-b95bdd32a33a81ee')
MANIFEST = ROOT / 'llvm_selected_manifest.json'


def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()


class OfflineToolchainTests(unittest.TestCase):
    def setUp(self):
        crypto_spec = importlib.util.spec_from_file_location(
            'offline_crypto_build', ROOT / 'crypto_build.py')
        self.crypto = importlib.util.module_from_spec(crypto_spec)
        crypto_spec.loader.exec_module(self.crypto)
        build_spec = importlib.util.spec_from_file_location(
            'offline_native_host_build', ROOT / 'build.py')
        self.build = importlib.util.module_from_spec(build_spec)
        build_spec.loader.exec_module(self.build)
        self.temporary = tempfile.TemporaryDirectory(dir=ROOT / 'build')
        self.addCleanup(self.temporary.cleanup)
        self.work = pathlib.Path(self.temporary.name)

    def test_real_selected_root_is_captured_and_materialized_without_archive(self):
        members, manifest_sha = self.crypto._read_llvm_selected_root(SELECTED_ROOT)
        manifest = json.loads(MANIFEST.read_text())
        self.assertEqual(manifest_sha, self.crypto.LLVM_SELECTED_MANIFEST_SHA256)
        self.assertEqual(len(members), 299)
        self.assertEqual({name: sha_bytes(data) for name, data in members.items()},
                         manifest['members'])

        output = self.work / 'dependencies'
        compiler, archiver, resource, sdk, identity, input_evidence = self.crypto._native_tools(
            output, SDK, LLVM_BIN, llvm_selected_root=SELECTED_ROOT)
        extracted = output / ('llvm-' + self.crypto.LLVM_SHA[:16])
        self.assertTrue(pathlib.Path(compiler).is_file())
        self.assertTrue(pathlib.Path(archiver).is_file())
        self.assertTrue(pathlib.Path(resource, 'include/stddef.h').is_file())
        self.assertEqual(pathlib.Path(sdk), SDK.resolve())
        self.assertEqual(self.crypto._tree_hashes(extracted), manifest['members'])
        self.assertEqual(identity['llvm_archive_sha256'], self.crypto.LLVM_SHA)
        self.assertEqual(identity['llvm_selected_manifest_sha256'], manifest_sha)
        self.assertEqual(identity['llvm_selected_members'], manifest['members'])
        self.assertEqual(input_evidence['mode'], 'selected_root')
        self.assertEqual(input_evidence['manifest_sha256'], manifest_sha)

    def test_default_path_still_requires_the_original_archive(self):
        output = self.work / 'default-dependencies'
        with self.assertRaisesRegex(ValueError, 'archive missing or not a regular file'):
            self.crypto._native_tools(output, SDK, LLVM_BIN)
        self.assertFalse(output.exists())

    def test_selected_root_rejects_file_set_content_link_special_and_mode_changes(self):
        copied = self.work / 'copy'
        shutil.copytree(SELECTED_ROOT, copied)
        header = copied / 'lib/clang/21/include/stddef.h'
        original_header = header.read_bytes()
        archiver = copied / 'bin/llvm-ar'
        original_mode = stat.S_IMODE(archiver.stat().st_mode)

        def reject_without_staging(label):
            output = self.work / ('output-' + label)
            with self.assertRaises(ValueError):
                self.crypto._native_tools(output, SDK, LLVM_BIN,
                                          llvm_selected_root=copied)
            self.assertFalse(output.exists())

        header.unlink()
        reject_without_staging('missing')
        header.write_bytes(original_header)

        extra = copied / 'unexpected.bin'
        extra.write_bytes(b'extra')
        reject_without_staging('extra')
        extra.unlink()

        header.write_bytes(original_header + b'changed')
        reject_without_staging('changed')
        header.write_bytes(original_header)

        header.unlink()
        header.symlink_to(copied / 'lib/clang/21/include/stdint.h')
        reject_without_staging('symlink')
        header.unlink()
        header.write_bytes(original_header)

        hardlinked = copied / 'lib/clang/21/include/stdint.h'
        hardlink_target = copied / 'lib/clang/21/include/stddef.h'
        hardlink_target.unlink()
        os.link(hardlinked, hardlink_target)
        reject_without_staging('hardlink')
        hardlink_target.unlink()
        hardlink_target.write_bytes(original_header)

        special = copied / 'rogue-fifo'
        os.mkfifo(special)
        reject_without_staging('special')
        special.unlink()

        empty_extra_dir = copied / 'unexpected-empty-dir'
        empty_extra_dir.mkdir()
        reject_without_staging('empty-directory')
        empty_extra_dir.rmdir()

        oversized = copied / 'lib/clang/21/include/stddef.h'
        with oversized.open('r+b') as stream:
            stream.truncate(self.crypto.LLVM_SELECTED_MAX_MEMBER_BYTES + 1)
        reject_without_staging('oversized')
        oversized.write_bytes(original_header)

        archiver.chmod(0o644)
        reject_without_staging('archiver-mode')
        archiver.chmod(original_mode)

    def test_selected_root_rejects_symlink_root_and_symlink_ancestor(self):
        alias = self.work / 'selected-alias'
        alias.symlink_to(SELECTED_ROOT, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.crypto._read_llvm_selected_root(alias)

        parent_alias = self.work / 'parent-alias'
        parent_alias.symlink_to(self.work, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.crypto._read_llvm_selected_root(parent_alias / 'copy')

    def test_manifest_pin_and_identity_are_checked_before_tree_acceptance(self):
        with mock.patch.object(self.crypto, 'LLVM_SELECTED_MANIFEST_SHA256', '0' * 64):
            with self.assertRaisesRegex(ValueError, 'manifest.*identity|manifest.*pin'):
                self.crypto._read_llvm_selected_root(SELECTED_ROOT)

        document = json.loads(MANIFEST.read_text())
        document['package'] = 'other-package'
        bad_manifest = self.work / 'bad-manifest.json'
        raw = json.dumps(document, sort_keys=True, separators=(',', ':')).encode()
        bad_manifest.write_bytes(raw)
        with mock.patch.object(self.crypto, 'LLVM_SELECTED_MANIFEST_PATH', bad_manifest), \
             mock.patch.object(self.crypto, 'LLVM_SELECTED_MANIFEST_SHA256', sha_bytes(raw)):
            with self.assertRaisesRegex(ValueError, 'manifest.*package|package.*identity'):
                self.crypto._read_llvm_selected_root(SELECTED_ROOT)

    def test_host_mode_rejects_selected_native_input(self):
        with self.assertRaisesRegex(ValueError, 'host.*selected|native.*input'):
            self.crypto.prepare_crypto('host', self.work / 'host',
                                       REPOSITORY / '.superpowers/native-dependency-spike/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz',
                                       REPOSITORY / '.superpowers/native-dependency-spike/mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz',
                                       llvm_selected_root=SELECTED_ROOT)
        self.assertFalse((self.work / 'host').exists())

    def test_builder_identity_covers_manifest_and_host_cli_rejects_option(self):
        relative_manifest = 'outputs/ps5-mod-bridge/native-host/llvm_selected_manifest.json'
        self.assertIn(relative_manifest, self.build.SOURCE_INPUTS)

        output = self.work / 'host-cli-output'
        result = subprocess.run(
            [sys.executable, str(ROOT / 'build.py'), '--profile', 'desktop_fixture',
             '--llvm-selected-root', str(SELECTED_ROOT), '--output-root', str(output)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn('--llvm-selected-root requires a native build profile', result.stderr)
        self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
