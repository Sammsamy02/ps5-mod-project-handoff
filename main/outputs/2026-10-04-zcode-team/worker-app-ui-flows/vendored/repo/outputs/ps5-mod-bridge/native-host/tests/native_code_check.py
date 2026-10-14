"""Check emitted optimized native operations, never execute a PS5 artifact.

Run after native builds: python3 tests/native_code_check.py. Compiler-version
specific assertions deliberately check the pinned LLVM 21.1.8 x86_64 output.
Returned self-test booleans alone cannot detect optimized-away observations.
"""
import pathlib
import json
import hashlib
import os
import re
import subprocess
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
OBJDUMP = ROOT.parent / 'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-objdump'

class OptimizedNativeChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.artifacts = []
        cls.entropy_artifacts = []
        cls.drbg_artifacts = []
        cls.manifests = []
        for profile in ('loopback', 'development_lan'):
            directory = pathlib.Path(os.getenv('MB_NATIVE_BUILD_ROOT', ROOT / 'build/drbg')).resolve() / profile
            elf = directory / ('modbridge-native-host-loopback.elf' if profile == 'loopback'
                               else 'modbridge-native-host-development-lan.elf')
            cls.manifests.append((directory, json.loads((directory / 'manifest.json').read_text())))
            for artifact in (directory / 'core_bridge.o', directory / 'entropy.o', directory / 'drbg.o', elf):
                emitted = subprocess.run([str(OBJDUMP), '-dr', str(artifact)], check=True,
                                         capture_output=True, text=True).stdout
                functions = {}
                headers = list(re.finditer(r'^([0-9a-f]+) <([^\n]+)>:\n', emitted, re.M))
                for index, header in enumerate(headers):
                    end = headers[index + 1].start() if index + 1 < len(headers) else len(emitted)
                    functions[header.group(2)] = emitted[header.end():end]
                relocations = subprocess.run([str(OBJDUMP), '-R', str(artifact)], check=True,
                                             capture_output=True, text=True).stdout if artifact.suffix == '.elf' else ''
                observation = (str(artifact), functions, relocations)
                if artifact.stem == 'core_bridge' or artifact.suffix == '.elf': cls.artifacts.append(observation)
                if artifact.stem == 'entropy' or artifact.suffix == '.elf': cls.entropy_artifacts.append(observation)
                if artifact.stem == 'drbg' or artifact.suffix == '.elf': cls.drbg_artifacts.append(observation)
                name = ('optimized-entropy-object.txt' if artifact.stem == 'entropy' else
                        'optimized-drbg-object.txt' if artifact.stem == 'drbg' else
                        'optimized-object.txt' if artifact.suffix == '.o' else 'optimized-elf.txt')
                (directory / name).write_text(emitted)

    def body(self, functions, identifier):
        matches = [text for symbol, text in functions.items() if identifier in symbol]
        self.assertEqual(len(matches), 1, 'missing or ambiguous emitted operation: ' + identifier)
        return matches[0]

    def reference_address(self, body, identifier):
        last_call = None
        for line in body.splitlines():
            call = re.match(r'\s*([0-9a-f]+):.*callq', line)
            if call: last_call = int(call.group(1), 16)
            if identifier in line:
                self.assertIsNotNone(last_call)
                return last_call
        self.fail('missing emitted call: ' + identifier)

    def test_tls_41_to_42_observation_and_reset_survive_native_optimization(self):
        for artifact, functions, relocations in self.artifacts:
            with self.subTest(artifact=artifact):
                check = self.body(functions, 'tls_checkEv')
                mutation = self.body(functions, 'tls_changeEv')
                observation = self.body(functions, 'tls_observeEv')
                reset = self.body(functions, 'tls_resetEv')
                self.assertIn('tls_checkEv', self.body(functions, 'mb_core_selftest'))
                for helper in (mutation, observation, reset):
                    self.assertIn('__emutls_get_address', helper)
                self.assertRegex(mutation, r'movl\s+\$0x2a, \(%rax\)')
                self.assertRegex(observation, r'movl\s+\(%rax\), %eax')
                self.assertRegex(reset, r'movl\s+\$0x29, \(%rax\)')
                calls = re.findall(r'(tls_(?:observe|change|reset))Ev', check)
                self.assertEqual(calls, ['tls_observe', 'tls_change', 'tls_observe', 'tls_reset'])
                self.assertRegex(check, r'xorl\s+\$0x29,')
                self.assertRegex(check, r'xorl\s+\$0x2a,')
                self.assertRegex(check, r'orl[\s\S]*sete\s+%al')
                for helper in (mutation, observation, reset):
                    self.assertIn('tls_valueE', helper)

    def test_guard_cleanup_and_exactly_once_observation_survive_native_unwinding(self):
        for artifact, functions, relocations in self.artifacts:
            with self.subTest(artifact=artifact):
                check = self.body(functions, 'exception_checkEv')
                cleanup = self.body(functions, '_ZN12_GLOBAL__N_15GuardD2Ev')
                observation = self.body(functions, 'guard_observe')
                self.assertIn('exception_checkEv', self.body(functions, 'mb_core_selftest'))
                self.assertRegex(cleanup, r'(?:incl\s+\(%[a-z]+\)|movl\s+\(%[a-z]+\), %[a-z]+[\s\S]*incl[\s\S]*movl\s+%[a-z]+, \(%[a-z]+\))')
                self.assertRegex(observation, r'movl\s+\(%rdi\), %eax')
                self.assertIn('__cxa_throw', check)
                self.assertIn('__cxa_rethrow', check)
                self.assertIn('__cxa_begin_catch', check)
                if relocations:
                    strcmp = re.search(r'^([0-9a-f]+)\s+\S+\s+strcmp$', relocations, re.M)
                    self.assertIsNotNone(strcmp)
                    self.assertIn('0x' + format(int(strcmp.group(1), 16), 'x'), check)
                else:
                    self.assertIn('strcmp', check)
                self.assertEqual(check.count('GuardD2Ev'), 1, 'owned cleanup must have one emitted call site')
                self.assertLess(check.index('__cxa_throw'), check.index('GuardD2Ev'))
                self.assertLess(check.index('GuardD2Ev'), check.index('__cxa_begin_catch'))
                self.assertGreater(check.index('guard_observe'), check.index('__cxa_begin_catch'))
                # The rethrow landing pad ends its inner catch then jumps back
                # into the outer catch, after cleanup and before observation.
                cleanup_address = self.reference_address(check, 'GuardD2Ev')
                observe_address = self.reference_address(check, 'guard_observe')
                landing = check[check.index('__cxa_rethrow'):]
                self.assertIn('__cxa_end_catch', landing)
                targets = [int(target, 16) for target in re.findall(r'jmp\s+0x([0-9a-f]+)', landing)]
                self.assertTrue(any(cleanup_address < target < observe_address for target in targets),
                                'rethrow must resume the outer observation after owned cleanup')
                self.assertRegex(check, r'cmpl\s+\$0x1, %eax')

    def test_one_entropy_call_and_capacity_wipe_survive_native_optimization(self):
        for artifact, functions, relocations in self.entropy_artifacts:
            with self.subTest(artifact=artifact):
                observer = functions['mb_entropy_observe']
                adapter = functions['mb_entropy_read32']
                if relocations:
                    imported = re.search(r'^([0-9a-f]+)\s+\S+\s+sysctl$', relocations, re.M)
                    self.assertIsNotNone(imported)
                    reference = '0x' + format(int(imported.group(1), 16), 'x')
                else:
                    reference = 'sysctl-0x4'
                references = [line for line in adapter.splitlines() if reference in line]
                self.assertEqual(len(references), 1, 'one retained native API call site')
                if relocations: self.assertIn('callq', references[0])
                self.assertNotIn(reference, observer)
                self.assertEqual(observer.count('mb_entropy_read32'), 1)
                self.assertRegex(adapter, r'cmpl\s+\$0x4,')
                self.assertLess(adapter.index('incl'), adapter.index(reference), 'budget reserves before API')
                wipe = re.search(
                    r'([0-9a-f]+):[^\n]*movb\s+\$0x0, \(%[a-z0-9]+,(%[a-z0-9]+)\)\n'
                    r'[^\n]*incq\s+\2\n[^\n]*cmpq\s+\$0x20, \2\n'
                    r'[^\n]*jne\s+0x\1\b', observer)
                self.assertIsNotNone(wipe, 'retained volatile loop clears exactly the 32-byte capacity')
                self.assertLess(observer.index('mb_entropy_read32'), wipe.start())
                self.assertNotIn('retq', observer[:wipe.end()], 'every returned native path traverses cleanup')

    def test_checkpoint_operation_order_error_gates_and_full_owned_cleanup_are_retained(self):
        for artifact, functions, relocations in self.drbg_artifacts:
            with self.subTest(artifact=artifact):
                checkpoint=functions['mb_random_checkpoint_in']
                source=functions['mb_drbg_source']
                cleanup=functions['mb_drbg_cleanup']
                wrapper=functions['mb_random_checkpoint']
                sequence=['mb_entropy_read32','mbedtls_entropy_init','mbedtls_ctr_drbg_init',
                          'mbedtls_entropy_add_source','mbedtls_ctr_drbg_seed','mbedtls_ctr_drbg_random','mb_drbg_cleanup']
                positions=[checkpoint.index(name) for name in sequence]
                self.assertEqual(positions,sorted(positions))
                for name in ('mb_entropy_read32','mbedtls_entropy_add_source','mbedtls_ctr_drbg_seed','mbedtls_ctr_drbg_random','mb_drbg_cleanup'):
                    self.assertEqual(checkpoint.count(name),1)
                self.assertNotIn('mb_entropy_observe',checkpoint)
                self.assertNotIn('mbedtls_ctr_drbg_reseed',checkpoint)
                self.assertNotIn('mbedtls_ctr_drbg_set_',checkpoint)
                self.assertNotIn('mb_drbg_test_observe', '\n'.join(functions))
                for begin,end in ((0,3),(3,4),(4,5)):
                    self.assertRegex(checkpoint[positions[begin]:positions[end]],r'testl\s+%eax, %eax[\s\S]*jne')
                self.assertRegex(checkpoint,r'movl\s+\$0x22, %r8d')
                self.assertRegex(checkpoint[positions[4]:positions[5]],r'movl\s+\$0x20, %edx')
                self.assertLess(cleanup.index('mbedtls_ctr_drbg_free'),cleanup.index('mbedtls_entropy_free'))
                wipe=re.search(r'movb\s+\$0x0,[\s\S]*cmpq\s+\$0x([0-9a-f]+),[^\n]*\n[^\n]*jne',cleanup)
                self.assertIsNotNone(wipe)
                stack=re.search(r'subq\s+\$0x([0-9a-f]+), %rsp',wrapper)
                self.assertIsNotNone(stack)
                self.assertEqual(wipe.group(1),stack.group(1),'whole private workspace capacity')
                self.assertLess(cleanup.index('mbedtls_entropy_free'),wipe.start())
                self.assertNotIn('retq',cleanup[:wipe.end()])
                self.assertLess(checkpoint.index('mb_drbg_cleanup'),checkpoint.index('retq'))
                self.assertEqual(source.count('mb_entropy_read32'),1)
                self.assertRegex(source,r'cmpl\s+\$0x4,')
                self.assertRegex(source,r'cmpq\s+\$0x61,') # capacity-32 <= 96
                self.assertGreaterEqual(len(re.findall(r'movb\s+\$0x0,',source)),3)
                self.assertRegex(source,r'cmpq\s+%[a-z0-9]+, %[a-z0-9]+\n[^\n]*jne') # full callback capacity

    def test_actual_native_library_zeroizer_no_defaults_and_aes_dispatch_are_present(self):
        for artifact, functions, relocations in self.drbg_artifacts:
            if not relocations: continue
            with self.subTest(artifact=artifact):
                zeroizer=functions['mbedtls_platform_zeroize']
                self.assertIn('memset_func',zeroizer)
                self.assertRegex(zeroizer,r'callq\s+\*%rax')
                self.assertNotIn('mbedtls_entropy_add_source',functions['mbedtls_entropy_init'])
                for prohibited in ('mbedtls_platform_entropy_poll','mbedtls_hardware_poll','mbedtls_nv_seed_poll'):
                    self.assertNotIn(prohibited,functions)
                self.assertIn('mbedtls_aesni_has_support',functions['mbedtls_aes_crypt_ecb'])
                self.assertIn('cpuid',functions['mbedtls_aesni_has_support'])
                self.assertIn('aesenc',functions['mbedtls_aesni_crypt_ecb'])

    def test_current_native_artifact_input_flags_imports_and_loader_closure_match(self):
        for directory, manifest in self.manifests:
            with self.subTest(directory=str(directory)):
                artifact=directory/manifest['artifact']
                self.assertEqual(hashlib.sha256(artifact.read_bytes()).hexdigest(),manifest['artifact_sha256'])
                for name,digest in manifest['object_sha256'].items():
                    self.assertEqual(hashlib.sha256((directory/name).read_bytes()).hexdigest(),digest)
                for name,digest in manifest['source_sha256'].items():
                    self.assertEqual(hashlib.sha256((ROOT.parents[2]/name).read_bytes()).hexdigest(),digest)
                descriptor=manifest['crypto_descriptor']
                self.assertEqual(descriptor['config_id'],'0983001ade634835e0755be7c0a0d7d82b7fb966ffe53e1bcbc19a1b3e77cae2')
                self.assertEqual(descriptor['identity']['archive_sha256'],'ae9e0cf32fdfd2fb8281b3b3518c076ba709186c98387a95912c16beb8b2bb53')
                self.assertEqual(manifest['crypto'],descriptor['identity'])
                for record in manifest['commands']:
                    argv=record['argv']
                    self.assertEqual(record['returncode'],0)
                    self.assertEqual(record['stderr'],'')
                    if '-c' not in argv: continue
                    source=argv[argv.index('-c')+1]
                    if source.endswith('.c'):
                        for flag in descriptor['native_compile_options']:
                            self.assertIn(flag,argv)
                        for flag in descriptor['compile_definitions']:
                            self.assertIn('-D'+flag,argv)
                        for path in descriptor['include_dirs']:
                            self.assertIn('-I'+path,argv)
                    else:
                        self.assertIn('-std=c++20',argv)
                        self.assertNotIn('-nostdinc',argv)
                for entries in manifest['include_closure'].values():
                    for entry in entries:
                        self.assertNotEqual(entry['origin'],'host')
                        self.assertEqual(hashlib.sha256(pathlib.Path(entry['path']).read_bytes()).hexdigest(),entry['sha256'])
                self.assertTrue(all(load['alignment']==16384 for load in manifest['elf']['loads']))
                self.assertLessEqual(manifest['elf']['max_load_file_end'],manifest['elf']['loader_prefix'])
                self.assertLessEqual(manifest['elf']['program_table_end'],manifest['elf']['loader_prefix'])
                self.assertLessEqual(manifest['elf']['section_table_end'],manifest['elf']['loader_prefix'])
                self.assertIn('BIND_NOW',(directory/'readelf.txt').read_text())
                self.assertFalse(manifest['console_executed'])
                symbols=(directory/'symbols.txt').read_text()
                self.assertNotIn('mb_drbg_test_observe',symbols)
                self.assertNotIn('fixture_',symbols)
                self.assertNotIn('mbedtls_platform_entropy_poll',symbols)
                link=next(r['argv'] for r in manifest['commands'] if str(artifact) in r['argv'] and '-c' not in r['argv'])
                self.assertEqual([arg for arg in link if arg.endswith('.a')],[descriptor['archive']])
                needed=re.findall(r'Shared library: \[([^]]+)\]',(directory/'readelf.txt').read_text())
                self.assertEqual(set(needed),{'libkernel_web.sprx','libSceLibcInternal.sprx','libSceNet.sprx'})

if __name__ == '__main__': unittest.main(verbosity=2)
