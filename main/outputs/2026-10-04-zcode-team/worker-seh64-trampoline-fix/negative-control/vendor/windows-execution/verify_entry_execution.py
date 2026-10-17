#!/usr/bin/env python3
"""Execute owned dynamic-entry DLL fixtures locally; never contact a console."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import uuid

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-cc', default='clang')
    parser.add_argument('--llvm-bin', type=Path, default=ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin')
    parser.add_argument('--lld-link', type=Path, default=ROOT / '.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/rustup/toolchains/1.90.0-aarch64-apple-darwin/lib/rustlib/aarch64-apple-darwin/bin/gcc-ld/lld-link')
    parser.add_argument('--output', type=Path, default=ROOT / '.superpowers/native-dependency-spike/pe-entry-lifecycle-1')
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE/'pe_module.c', HERE/'pe_module.h', HERE/'tests/entry_fixture.c',
        HERE/'tests/run_entry_fixture.c', HERE/'verify_entry_execution.py',
        HERE.parent/'windows-services/mb_win_entry.c', HERE.parent/'windows-services/mb_win_entry.h',
        HERE.parent/'windows-image/pe_image.c', HERE.parent/'windows-image/pe_image.h']
    receipt = {'status': 'IN_PROGRESS', 'commands': [],
        'source_files': {str(p.relative_to(ROOT)): sha(p) for p in sources},
        'downloaded_dll_executed': False, 'console_contact': False, 'ps5_execution': False}

    def save():
        (out/'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')

    def run(label, argv):
        argv = list(map(str, argv))
        result = subprocess.run(argv, cwd=ROOT, text=True, capture_output=True, timeout=60)
        (out/(label+'.stdout')).write_text(result.stdout)
        (out/(label+'.stderr')).write_text(result.stderr)
        receipt['commands'].append({'label': label, 'argv': argv, 'exit': result.returncode})
        save()
        if result.returncode:
            raise RuntimeError(f'{label} failed: {out/(label+".stderr")}')
        return result.stdout

    try:
        for reject in (0, 1):
            name = 'reject' if reject else 'accept'
            run('compile-'+name, [args.llvm_bin/'clang', '--target=x86_64-pc-windows-msvc',
                '-O1', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
                '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-DREJECT_ATTACH='+str(reject),
                '-c', HERE/'tests/entry_fixture.c', '-o', out/(name+'.obj')])
            run('link-'+name, [args.lld_link, '/dll', '/entry:Entry', '/nodefaultlib',
                '/align:16384', '/dynamicbase', '/nxcompat', '/timestamp:0',
                '/out:'+str(out/(name+'.dll')), out/(name+'.obj')])
            # 16 KiB alignment deliberately matches the caller's protection
            # granule. LLD's /align-without-/driver warning is retained in logs.
            run('independent-read-'+name, [args.llvm_bin/'llvm-readobj', '--file-headers',
                '--sections', '--coff-imports', '--coff-exports', '--coff-basereloc', out/(name+'.dll')])
        run('compile-host-runtime', [args.host_cc, '-arch', 'x86_64', '-std=c11',
            '-Wall', '-Wextra', '-Werror', '-O1', '-ffreestanding', '-fno-builtin',
            HERE.parent/'windows-image/pe_image.c', HERE.parent/'windows-services/mb_win_entry.c',
            HERE/'pe_module.c', HERE/'tests/run_entry_fixture.c', '-o', out/'run-entry'])
        result = json.loads(run('execute-owned-entry-dlls', ['/usr/bin/arch', '-x86_64',
            out/'run-entry', out/'accept.dll', out/'reject.dll']))
        assert result == {'owned_entry_dlls_executed': 2, 'lifecycle_cases': 4,
            'attach_rejection_detached': True, 'cleanup_retry_detached_once': True,
            'busy_close_preserved_entry': True, 'console_contact': False}, result
        for name, digest in receipt['source_files'].items():
            assert sha(ROOT/name) == digest, f'Source changed during verification: {name}'
        receipt.update(status='OWNED_PE_ENTRY_HOST_EXECUTION_PASSED', result=result,
            fixtures={name: {'bytes': (out/(name+'.dll')).stat().st_size,
                'sha256': sha(out/(name+'.dll'))} for name in ('accept', 'reject')},
            host_runner_sha256=sha(out/'run-entry'),
            scope='Owned synchronous no-CRT/no-TLS/no-exception DLL entry lifecycle; not arbitrary DLL or PS5 execution')
        save()
        print(json.dumps({'status': receipt['status'], 'receipt': str(out/'receipt.json')}))
    except Exception as error:
        receipt.update(status='FAILED', error=str(error))
        save()
        raise


if __name__ == '__main__':
    main()
