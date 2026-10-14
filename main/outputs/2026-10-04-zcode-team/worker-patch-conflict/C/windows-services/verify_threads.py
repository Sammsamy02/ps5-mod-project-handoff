#!/usr/bin/env python3
"""Test real native-thread errno storage and compile the same adapter for PS5."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import struct
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host-cc', default='clang')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--llvm-bin', type=Path, default=ROOT/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin')
    p.add_argument('--sdk', type=Path, default=ROOT/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk')
    p.add_argument('--output', type=Path, default=ROOT/'.superpowers/native-dependency-spike/windows-thread-errno-1')
    a = p.parse_args()
    out = a.output.resolve()/uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE/'mb_win_threads.c', HERE/'mb_win_threads.h', HERE/'tests/test_win_threads.c', Path(__file__).resolve()]
    receipt = {'status': 'IN_PROGRESS', 'commands': [],
        'sources': {str(f.relative_to(ROOT)): sha(f) for f in sources},
        'host_threads_executed': False, 'ps5_execution': False, 'console_contact': False,
        'scope': 'Native pthread-backed Windows errno storage; not Windows Tls/Fls or PE TLS'}
    env = dict(os.environ, LLVM_CONFIG=str(a.llvm_bin/'llvm-config'))

    def save():
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')

    def run(label, cmd):
        cmd = list(map(str, cmd))
        r = subprocess.run(cmd, cwd=ROOT, env=env, text=True, capture_output=True, timeout=60)
        (out/(label+'.stdout')).write_text(r.stdout)
        (out/(label+'.stderr')).write_text(r.stderr)
        receipt['commands'].append({'label': label, 'argv': cmd, 'exit': r.returncode})
        save()
        if r.returncode:
            raise RuntimeError(f'{label} failed; see {out/(label+".stderr")}')
        return r.stdout

    try:
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-pthread']
        run('host-build', [a.host_cc, *flags, HERE/'mb_win_threads.c', HERE/'tests/test_win_threads.c', '-o', out/'test-threads'])
        expected = '3 actual native threads: isolated stable Windows errno, explicit detach, exit destructor, busy close passed'
        result = run('host-threads', [out/'test-threads']).strip()
        assert result == expected, result
        if a.sanitize:
            run('sanitized-build', [a.host_cc, *flags, '-fsanitize=address,undefined', '-fno-omit-frame-pointer', HERE/'mb_win_threads.c', HERE/'tests/test_win_threads.c', '-o', out/'test-threads-sanitized'])
            assert run('sanitized-threads', [out/'test-threads-sanitized']).strip() == expected
        cc = a.sdk/'bin/prospero-clang'
        run('ps5-compile', [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2', '-fPIC', '-MD', '-MF', out/'threads.d', '-c', HERE/'mb_win_threads.c', '-o', out/'threads.o'])
        undefined = run('ps5-undefined', [a.llvm_bin/'llvm-nm', '--undefined-only', '--format=posix', out/'threads.o'])
        imports = sorted(line.split()[0] for line in undefined.splitlines() if line.strip())
        expected_imports = sorted(['abort', 'calloc', 'free', 'malloc', 'pthread_getspecific', 'pthread_key_create', 'pthread_key_delete', 'pthread_mutex_destroy', 'pthread_mutex_init', 'pthread_mutex_lock', 'pthread_mutex_unlock', 'pthread_setspecific'])
        assert imports == expected_imports, imports
        run('ps5-object', [a.llvm_bin/'llvm-readobj', '--file-headers', out/'threads.o'])
        elf = (out/'threads.o').read_bytes()
        assert elf[:6] == b'\x7fELF\x02\x01' and struct.unpack_from('<HH', elf, 16) == (1, 62)
        dependencies = shlex.split((out/'threads.d').read_text().replace('\\\n', ' ').split(':', 1)[1])
        for name, digest in receipt['sources'].items():
            assert sha(ROOT/name) == digest, name
        receipt.update(status='NATIVE_THREAD_ERRNO_HOST_AND_PS5_OBJECT_PASSED',
            host_threads_executed=True, host_result=result, sanitizer_executed=a.sanitize,
            native={'method': 'direct PS5 C frontend', 'object_bytes': (out/'threads.o').stat().st_size,
                'object_sha256': sha(out/'threads.o'), 'imports': imports,
                'compiler_wrapper_sha256': sha(cc)},
            native_dependencies={str(Path(n).resolve()): sha(Path(n)) for n in dependencies})
        save()
        print(json.dumps({'status': receipt['status'], 'receipt': str(out/'receipt.json')}))
    except Exception as error:
        receipt.update(status='FAILED', error=str(error))
        save()
        raise


if __name__ == '__main__':
    main()
