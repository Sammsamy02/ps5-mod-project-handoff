"""Offline crypto build contract, archive boundaries, and real-library checks."""
import hashlib
import importlib.util
import io
import json
import pathlib
import shutil
import struct
import subprocess
import tarfile
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
REPOSITORY = ROOT.parents[2]
SPIKE = REPOSITORY / '.superpowers/native-dependency-spike'
SOURCE = SPIKE / 'mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5.tar.gz'
FRAMEWORK = SPIKE / 'mbedtls-framework-dde0c4a0e448a0552f18817dcea633bb851fd288.tar.gz'
SDK = ROOT / 'build/sdk-v0.43/ps5-payload-sdk'
LLVM = ROOT.parent / 'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'


def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


class CryptoBuildTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue((ROOT / 'crypto_build.py').is_file(),
                        'prepare_crypto dependency builder is missing')
        spec = importlib.util.spec_from_file_location('crypto_build', ROOT / 'crypto_build.py')
        self.build = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(self.build)
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.work = pathlib.Path(self.temporary.name)

    def prepare(self, mode='host', output=None, **kwargs):
        return self.build.prepare_crypto(mode, output or self.work / 'build', SOURCE,
                                         FRAMEWORK, **kwargs)

    def archive(self, name, members):
        path = self.work / (name + '.tar.gz')
        with tarfile.open(path, 'w:gz') as archive:
            for filename, kind, data in members:
                info = tarfile.TarInfo(filename)
                if kind in ('file', 'pax'):
                    if kind == 'pax':
                        info.type = tarfile.XGLTYPE
                    info.size = len(data)
                    archive.addfile(info, io.BytesIO(data))
                else:
                    info.type = {'symlink': tarfile.SYMTYPE, 'hardlink': tarfile.LNKTYPE,
                                 'dir': tarfile.DIRTYPE, 'fifo': tarfile.FIFOTYPE}[kind]
                    if kind in ('symlink', 'hardlink'):
                        info.linkname = data.decode()
                    archive.addfile(info)
        return path

    def test_missing_and_wrong_archive_identity_reject_before_output_writes(self):
        wrong = self.work / 'wrong.tar.gz'
        wrong.write_bytes(b'not the pinned archive')
        for source, framework in ((self.work / 'missing', FRAMEWORK), (wrong, FRAMEWORK),
                                  (SOURCE, wrong), (SOURCE, self.work / 'missing')):
            with self.subTest(source=source.name, framework=framework.name):
                with self.assertRaisesRegex(ValueError, 'archive.*identity|archive.*missing'):
                    self.build.prepare_crypto('host', self.work / 'output', source, framework)
                self.assertFalse((self.work / 'output').exists())

    def test_unsafe_tar_rejects_all_members_before_any_write(self):
        cases = {
            'escape': ('../outside', 'file', b'x'),
            'absolute': ('/root/outside', 'file', b'x'),
            'backslash': ('pkg\\outside', 'file', b'x'),
            'symlink': ('pkg/link', 'symlink', b'good'),
            'hardlink': ('pkg/link', 'hardlink', b'pkg/good'),
            'fifo': ('pkg/fifo', 'fifo', b''),
            'duplicate': ('pkg/good', 'file', b'y'),
            'normalized_duplicate': ('pkg/./good', 'file', b'y'),
            'file_parent': ('pkg/good/child', 'file', b'y'),
        }
        for name, bad in cases.items():
            with self.subTest(name=name):
                archive = self.archive(name, [('pkg/good', 'file', b'x'), bad])
                output = self.work / name
                with self.assertRaises(ValueError):
                    self.build.extract_source_archive(archive, output, sha(archive), 'pkg')
                self.assertFalse(output.exists())
        self.assertFalse((self.work / 'outside').exists())

    def test_tar_count_member_and_total_bounds_reject_before_writes(self):
        archive = self.archive('bounded', [('pkg/a', 'file', b'1234'),
                                           ('pkg/b', 'file', b'1234')])
        for limits in ({'max_members': 1}, {'max_member_bytes': 3}, {'max_total_bytes': 7}):
            with self.subTest(limits=limits):
                with self.assertRaisesRegex(ValueError, 'bound'):
                    self.build.extract_source_archive(archive, self.work / 'output',
                                                      sha(archive), 'pkg', **limits)
                self.assertFalse((self.work / 'output').exists())

    def test_raw_pax_metadata_cannot_bypass_extraction_bounds(self):
        # tarfile hides this 2000-byte metadata member from ordinary iteration.
        archive = self.archive('pax', [('pax_global_header', 'pax', b'20 comment=abcdefgh\n' * 100),
                                     ('pkg/a', 'file', b'x')])
        for limits in ({'max_members': 1}, {'max_member_bytes': 64}, {'max_total_bytes': 32}):
            with self.subTest(limits=limits):
                with self.assertRaisesRegex(ValueError, 'bound'):
                    self.build.extract_source_archive(archive, self.work / 'output',
                                                      sha(archive), 'pkg', **limits)
                self.assertFalse((self.work / 'output').exists())

    def test_source_cache_checks_bytes_extras_and_symlinks(self):
        archive = self.archive('safe', [('pkg/a', 'file', b'1234')])
        output = self.work / 'output'
        extracted = self.build.extract_source_archive(archive, output, sha(archive), 'pkg')
        self.assertEqual((extracted / 'a').read_bytes(), b'1234')
        self.assertEqual(self.build.extract_source_archive(archive, output, sha(archive), 'pkg'), extracted)
        for mutation in ('changed', 'extra', 'symlink'):
            with self.subTest(mutation=mutation):
                if mutation == 'changed':
                    (extracted / 'a').write_bytes(b'changed')
                elif mutation == 'extra':
                    (extracted / 'unexpected.c').write_bytes(b'x')
                else:
                    (extracted / 'a').unlink()
                    (extracted / 'a').symlink_to(archive)
                with self.assertRaisesRegex(ValueError, 'cache|symlink'):
                    self.build.extract_source_archive(archive, output, sha(archive), 'pkg')
                if mutation == 'extra':
                    (extracted / 'unexpected.c').unlink()
                else:
                    (extracted / 'a').unlink()
                    (extracted / 'a').write_bytes(b'1234')

    def test_native_requires_explicit_verified_sdk_and_llvm_before_writes(self):
        for kwargs in ({}, {'sdk': SDK}, {'llvm_bin': LLVM},
                       {'sdk': self.work, 'llvm_bin': pathlib.Path('/usr/bin')}):
            with self.subTest(kwargs=kwargs):
                with self.assertRaisesRegex(ValueError, 'SDK|LLVM|native'):
                    self.prepare('native', **kwargs)
                self.assertFalse((self.work / 'build').exists())

    def test_native_rejects_host_compiler_bytes_and_altered_sdk_headers(self):
        deps = self.work / 'deps'
        llvm = deps / 'llvm/LLVM-21.1.8-macOS-ARM64/bin'
        llvm.mkdir(parents=True)
        shutil.copyfile(LLVM.parents[2] / 'sdk.zip', deps / 'sdk.zip')
        (llvm / 'clang').symlink_to('/usr/bin/cc')
        with self.assertRaisesRegex(ValueError, 'LLVM tool identity'):
            self.prepare('native', sdk=SDK, llvm_bin=llvm)
        self.assertFalse((self.work / 'build').exists())
        sdk_copy = self.work / 'sdk'
        shutil.copytree(SDK, sdk_copy)
        (sdk_copy / 'target/include/stdint.h').write_bytes(b'/* altered target header */')
        with self.assertRaisesRegex(ValueError, 'cache bytes'):
            self.prepare('native', sdk=sdk_copy, llvm_bin=LLVM)
        self.assertFalse((self.work / 'build').exists())

    def test_semantic_configuration_changes_with_overlay_or_recipe(self):
        overlay = (ROOT / 'crypto_config.h').read_bytes()
        before = self.build.configuration_identity(overlay)
        self.assertNotEqual(before, self.build.configuration_identity(overlay + b'\n/* owned delta */\n'))
        original = self.build.CRYPTO_SOURCES
        try:
            self.build.CRYPTO_SOURCES = original[:-1]
            self.assertNotEqual(before, self.build.configuration_identity(overlay))
        finally:
            self.build.CRYPTO_SOURCES = original

    def test_host_descriptor_cache_rejects_source_overlay_object_archive_and_extra_bytes(self):
        descriptor = self.prepare()
        self.assertEqual(descriptor.get('native_compile_options'), [])
        manifest_path = pathlib.Path(descriptor['evidence_manifest'])
        cache = manifest_path.parent
        unchanged = self.prepare()
        self.assertEqual(descriptor['identity'], unchanged['identity'])
        self.assertEqual(descriptor['config_id'], unchanged['config_id'])
        self.assertTrue(pathlib.Path(descriptor['archive']).is_absolute())
        serialized = json.dumps(descriptor['identity'], sort_keys=True)
        self.assertNotIn(str(REPOSITORY), serialized)
        self.assertNotIn(str(self.work), serialized)
        manifest = json.loads(manifest_path.read_text())
        source = cache / manifest['source_root'] / 'library/entropy.c'
        for path in (source, cache / 'config/crypto_config.h', cache / 'objects/entropy.o',
                     pathlib.Path(descriptor['archive'])):
            with self.subTest(path=path.name):
                original = path.read_bytes()
                path.write_bytes(original + b'corruption')
                try:
                    with self.assertRaisesRegex(ValueError, 'cache|identity|member'):
                        self.prepare()
                finally:
                    path.write_bytes(original)
        for relative in ('sources/unexpected.c', 'config/unexpected.h', 'objects/unexpected.o'):
            path = cache / relative
            path.write_bytes(b'unidentified')
            try:
                with self.assertRaisesRegex(ValueError, 'cache|closure'):
                    self.prepare()
            finally:
                path.unlink()

    def test_real_host_library_has_no_defaults_and_never_calls_prohibited_poll(self):
        descriptor = self.prepare()
        source = self.work / 'no_defaults.c'
        source.write_text('''#include "crypto_effective.h"
#include <assert.h>
#include <stdlib.h>
static unsigned reader_calls, source_calls, prohibited_calls;
int mbedtls_platform_entropy_poll(void *p, unsigned char *b, size_t n, size_t *o) {
    (void)p; (void)b; (void)n; (void)o; ++prohibited_calls; abort();
}
static int source(void *p, unsigned char *b, size_t n, size_t *o) {
    (void)p; assert(n == 128); ++source_calls; ++reader_calls;
    for (size_t i=0; i<32; ++i) b[i]=(unsigned char)(i+source_calls);
    *o=32; return 0;
}
int main(void) {
    mbedtls_entropy_context entropy;
    unsigned char output[64] = {0};
    reader_calls=source_calls=prohibited_calls=0;
    mbedtls_entropy_init(&entropy);
    assert(mbedtls_entropy_func(&entropy,output,48)==MBEDTLS_ERR_ENTROPY_NO_SOURCES_DEFINED);
    assert(reader_calls==0 && source_calls==0 && prohibited_calls==0);
    assert(mbedtls_entropy_add_source(&entropy,source,0,32,MBEDTLS_ENTROPY_SOURCE_STRONG)==0);
    assert(mbedtls_entropy_func(&entropy,output,48)==0);
    assert(reader_calls==2 && source_calls==2 && prohibited_calls==0);
    mbedtls_entropy_free(&entropy);
    return 0;
}
''')
        command = ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror']
        command += ['-D' + item for item in descriptor['compile_definitions']]
        command += ['-I' + item for item in descriptor['include_dirs']]
        command += [str(source), descriptor['archive'], '-o', str(self.work / 'no_defaults')]
        subprocess.run(command, check=True, capture_output=True, text=True)
        result = subprocess.run([str(self.work / 'no_defaults')], check=True, capture_output=True)
        self.assertEqual(result.stdout, b'')
        self.assertEqual(result.stderr, b'')

    def test_cache_cannot_authorize_forged_source_claims_or_extra_config_objects(self):
        descriptor = self.prepare()
        manifest_path = pathlib.Path(descriptor['evidence_manifest'])
        cache = manifest_path.parent
        original = manifest_path.read_bytes()
        for claim in ('source', 'include'):
            with self.subTest(claim=claim):
                manifest = json.loads(original)
                if claim == 'source':
                    manifest['source_members']['mbedtls'][self.build.SOURCE_ROOT + '/library/entropy.c'] = '0' * 64
                    manifest['identity']['source_members'] = manifest['source_members']
                else:
                    manifest['include_closure']['entropy.c'][0]['sha256'] = '0' * 64
                    manifest['identity']['include_closure'] = manifest['include_closure']
                manifest_path.write_text(json.dumps(manifest))
                try:
                    with self.assertRaisesRegex(ValueError, 'cache|identity|source|include'):
                        self.prepare()
                finally:
                    manifest_path.write_bytes(original)
        for relative in ('config/forged.h', 'objects/forged.o'):
            path = cache / relative
            path.write_bytes(b'forged closure addition')
            manifest = json.loads(original)
            manifest['file_closure'][relative] = sha(path)
            manifest_path.write_text(json.dumps(manifest))
            try:
                with self.subTest(extra=relative):
                    with self.assertRaisesRegex(ValueError, 'cache|closure'):
                        self.prepare()
            finally:
                path.unlink()
                manifest_path.write_bytes(original)
        manifest = json.loads(original)
        manifest['config_id'] = '0' * 64
        manifest_path.write_text(json.dumps(manifest))
        try:
            with self.assertRaisesRegex(ValueError, 'cache|identity|config'):
                self.prepare()
        finally:
            manifest_path.write_bytes(original)

    def test_whole_sources_closure_rejects_manifest_authorized_sibling_bytes(self):
        descriptor = self.prepare()
        manifest_path = pathlib.Path(descriptor['evidence_manifest'])
        cache = manifest_path.parent
        original_manifest = manifest_path.read_bytes()
        extra = cache / 'sources/unexpected.c'
        extra.write_bytes(b'/* not present in either pinned source archive */')
        manifest = json.loads(original_manifest)
        manifest['file_closure']['sources/unexpected.c'] = sha(extra)
        manifest_path.write_text(json.dumps(manifest))
        try:
            with self.assertRaisesRegex(ValueError, 'cache|closure'):
                self.prepare()
        finally:
            extra.unlink()
            manifest_path.write_bytes(original_manifest)

    def test_coordinated_object_archive_and_manifest_changes_fail_reuse(self):
        descriptor = self.prepare()
        manifest_path = pathlib.Path(descriptor['evidence_manifest'])
        cache = manifest_path.parent
        obj = cache / 'objects/entropy.o'
        archive = pathlib.Path(descriptor['archive'])
        original_object, original_archive = obj.read_bytes(), archive.read_bytes()
        original_manifest = manifest_path.read_bytes()
        changed = original_object[:-1] + bytes([original_object[-1] ^ 1])
        self.assertEqual(original_archive.count(original_object), 1)
        obj.write_bytes(changed)
        archive.write_bytes(original_archive.replace(original_object, changed))
        manifest = json.loads(original_manifest)
        manifest['file_closure']['objects/entropy.o'] = sha(obj)
        manifest['file_closure']['libmbedcrypto.a'] = sha(archive)
        for member in manifest['members']:
            if member['name'] == 'entropy.o':
                member['sha256'] = sha(obj)
        manifest['identity']['members'] = manifest['members']
        manifest['identity']['archive_sha256'] = sha(archive)
        manifest_path.write_text(json.dumps(manifest))
        try:
            with self.assertRaisesRegex(ValueError, 'rebuild|generated'):
                self.prepare()
        finally:
            obj.write_bytes(original_object)
            archive.write_bytes(original_archive)
            manifest_path.write_bytes(original_manifest)

    def test_effective_assertions_reject_alternate_drbg_and_sha256_paths(self):
        descriptor = self.prepare()
        source = self.work / 'effective.c'
        source.write_text('#include "crypto_effective.h"\nint main(void) { return 0; }\n')
        command = ['cc', '-std=c11', '-fsyntax-only']
        command += ['-D' + item for item in descriptor['compile_definitions']]
        command += ['-I' + item for item in descriptor['include_dirs']]
        for macro in ('MBEDTLS_ENTROPY_FORCE_SHA256', 'MBEDTLS_CTR_DRBG_USE_128_BIT_KEY',
                      'MBEDTLS_ENTROPY_HARDWARE_ALT', 'MBEDTLS_ENTROPY_NV_SEED',
                      'MBEDTLS_THREADING_C', 'MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO',
                      'MBEDTLS_ENTROPY_ALT', 'MBEDTLS_CTR_DRBG_ALT'):
            with self.subTest(macro=macro):
                result = subprocess.run([*command, '-D' + macro, str(source)], capture_output=True)
                self.assertNotEqual(result.returncode, 0)

    def test_native_archive_members_are_target_elf_and_includes_are_verified(self):
        descriptor = self.prepare('native', output=ROOT / 'build/crypto/task1-fix1-evidence/artifacts',
                                  sdk=SDK, llvm_bin=LLVM)
        self.assertFalse(str(REPOSITORY) in json.dumps(descriptor['identity'], sort_keys=True),
                         'native stable identity contains an absolute workspace path')
        options = descriptor['native_compile_options']
        self.assertIn('-nostdinc', options)
        self.assertIn('-resource-dir', options)
        resource = pathlib.Path(options[options.index('-resource-dir') + 1])
        self.assertTrue((resource / 'include/stddef.h').is_file())
        manifest = json.loads(pathlib.Path(descriptor['evidence_manifest']).read_text())
        self.assertGreater(len(manifest['members']), 80)
        for member in manifest['members']:
            self.assertEqual(member['format'], 'ELF64')
            self.assertEqual(member['machine'], 62)
            self.assertEqual(member['sha256'], sha(pathlib.Path(descriptor['evidence_manifest']).parent /
                                                   'objects' / member['name']))
        closures = manifest['include_closure']
        self.assertEqual(len(closures), len(manifest['members']))
        origins = {item['origin'] for headers in closures.values() for item in headers}
        self.assertTrue({'source', 'overlay', 'sdk', 'llvm_resource'}.issubset(origins))
        self.assertNotIn('host', origins)
        macros = pathlib.Path(descriptor['evidence_manifest']).parent / 'evidence/effective-macros.txt'
        text = macros.read_text()
        self.assertIn('#define MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES', text)
        self.assertIn('#define MBEDTLS_NO_PLATFORM_ENTROPY', text)
        self.assertIn('#define MBEDTLS_ENTROPY_BLOCK_SIZE 64', text)
        self.assertIn('#define MBEDTLS_CTR_DRBG_ENTROPY_LEN 48', text)
        self.assertNotIn('#define MBEDTLS_CTR_DRBG_USE_PSA_CRYPTO', text)
        self.assertNotIn('#define MBEDTLS_MD_SHA512_VIA_PSA', text)
        (ROOT / 'build/crypto/task1-fix1-evidence/native-green-descriptor.json').write_text(
            json.dumps(descriptor, indent=2, sort_keys=True) + '\n')


if __name__ == '__main__':
    unittest.main()
