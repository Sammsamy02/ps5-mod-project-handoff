#!/usr/bin/env python3
"""Build genuine static full/interp corlib with the frozen matching Mono compiler."""
from pathlib import Path
import hashlib, json, os, resource, shutil, subprocess, sys, time

BASE = Path(__file__).resolve().parents[1]
FROZEN = BASE.parent / 'cap10-mono-build'
WORK = FROZEN / 'runtime-work'
MDK = BASE.parent / 'mono-host-research/runtime-extracted/Library/Frameworks/Mono.framework/Versions/6.12.0'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def main():
    receipt = BASE / 'evidence/STATIC-CORLIB-BUILD.json'
    finalize = sys.argv[1:] == ['--finalize-existing']
    assert not receipt.exists() or finalize, 'Preserve existing receipt; reproduction needs a new owned directory.'
    compiler = WORK / 'mono/mini/mono-sgen'
    runtime = WORK / 'mono/mini/.libs/libmonosgen-2.0.1.dylib'
    original = MDK / 'lib/mono/4.5/mscorlib.dll'
    expected = {
        compiler: 'c301fbc2012d02f0f00c1625f6ffb366f46e8fa5cabfa24fd073138365d786fe',
        runtime: 'c564b99bf331c552323c57c4d70ce9a38d5f7e7c304fa0f343bfa1dbc3428f7c',
        original: '86364b7803c92d8c88b590031f015f80b10b00641e9d067e07a529a131d815e4',
        WORK / 'config.h': 'dfedf2637bae0a144813c5701fb0f6d993bd7f9abf5f2288d079c4fdeb7b3eff',
    }
    assert all(sha(p) == h for p, h in expected.items())
    corlib = BASE / 'owner-runtime/lib/mono/4.5/mscorlib.dll'
    shutil.copyfile(original, corlib)
    assembly = BASE / 'aot-build/mscorlib.dll.s'
    obj = BASE / 'aot-build/mscorlib.dll.o'
    env = os.environ.copy()
    for key in list(env):
        if key.startswith('MONO_') or key.startswith('DYLD_'):
            env.pop(key)
    # The compiler has no installed corlib. This build-only override points solely
    # at our genuine DLL; the actual owner run must not carry MONO_PATH.
    env.update(MONO_PATH=str(corlib.parent), MONO_CFG_DIR=str(MDK / 'etc'),
               MONO_CRASH_NOFILE='1', TMPDIR=str(BASE / 'tmp'))
    commands = [
        [str(compiler), '--aot=full,interp,static,asmonly,outfile=' + str(assembly), str(corlib)],
        ['/usr/bin/clang', '-arch', 'x86_64', '-c', str(assembly), '-o', str(obj)],
        ['/usr/bin/file', str(obj)],
        ['/usr/bin/nm', '-g', str(obj)],
    ]
    records = json.loads(receipt.read_text())['steps'] if finalize else []
    if finalize:
        assert len(records) == 4 and all(r['exit_code'] == 0 for r in records)
        assert [r['command'] for r in records] == commands
    for index, command in enumerate([] if finalize else commands):
        started = time.monotonic()
        result = subprocess.run(command, cwd=BASE / 'aot-build', env=env, text=True,
                                capture_output=True, preexec_fn=no_core, timeout=600)
        logfile = BASE / 'logs' / ('STATIC-BUILD-%02d.log' % index)
        logfile.write_text(result.stdout + result.stderr)
        records.append({'command': command, 'exit_code': result.returncode,
                        'elapsed_seconds': round(time.monotonic() - started, 3), 'log': str(logfile),
                        'cwd': str(BASE / 'aot-build')})
        receipt.write_text(json.dumps({'steps': records, 'complete': False}, indent=2) + '\n')
        assert result.returncode == 0, result.stdout + result.stderr
    assert '_mono_aot_module_mscorlib_info' in (BASE / 'logs/STATIC-BUILD-03.log').read_text()
    assert 'Mach-O 64-bit object x86_64' in (BASE / 'logs/STATIC-BUILD-02.log').read_text()
    emitted = assembly.read_text()
    marker = '_mono_aot_module_mscorlib_info:'
    symbol_definition = emitted[emitted.index(marker):].splitlines()[:3]
    assert [line.strip() for line in symbol_definition[1:]] == ['.align 3', '.quad _mono_aot_file_info'], symbol_definition
    assert all(sha(p) == h for p, h in expected.items())
    assert list(corlib.parent.iterdir()) == [corlib]
    proof = {'complete': True, 'steps': records, 'source_commit': '0cbf0e290c31adb476f9de0fa44b1d8829affa40',
             'frozen_inputs_sha256': {str(p): h for p, h in expected.items()},
             'frozen_inputs_unchanged': True, 'corlib_sha256': sha(corlib),
             'assembly_sha256': sha(assembly), 'object_sha256': sha(obj), 'object_bytes': obj.stat().st_size,
             'emitted_symbol_definition': symbol_definition, 'assembly_prefix_only_corlib_dll': True,
             'build_environment': {k: v for k, v in env.items() if k.startswith('MONO_') or k == 'TMPDIR'},
             'default_single_thread_aot': True, 'max_compile_jobs': 1, 'host_only': True, 'target_proof': False}
    receipt.write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps(proof, indent=2))

if __name__ == '__main__':
    main()
