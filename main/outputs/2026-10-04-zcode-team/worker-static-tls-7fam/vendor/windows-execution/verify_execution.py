#!/usr/bin/env python3
"""Build and execute only our owned Windows DLL fixture on a local x86_64 Mac.

This is a developer test, not a console dispatcher or downloaded-DLL runner.
Production PS5 object builds are separate from this host execution result.
"""
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
    parser.add_argument('--output', type=Path, default=ROOT / '.superpowers/native-dependency-spike/callable-pe-runtime-1')
    args = parser.parse_args()
    out = args.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sources = [HERE/'pe_module.c', HERE/'pe_module.h', HERE/'tests/call_fixture.c',
        HERE/'tests/run_owned_fixture.c', HERE/'tests/crt_memory.def', HERE/'tests/crt_string.def',
        HERE/'verify_execution.py', HERE.parent/'windows-services/mb_win_crt.c',
        HERE.parent/'windows-services/mb_win_crt.h', HERE.parent/'windows-image/pe_image.c',
        HERE.parent/'windows-image/pe_image.h']
    binder_header = HERE.parent/'windows-services/mb_win_crt_bind.h'
    if binder_header.exists():
        sources.append(binder_header)
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
        for name in ('memory', 'string'):
            run(name+'-import-library', [args.lld_link, '/lib',
                '/def:'+str(HERE/f'tests/crt_{name}.def'), '/machine:x64',
                '/out:'+str(out/f'{name}.lib')])
        run('compile-owned-pe', [args.llvm_bin/'clang', '--target=x86_64-pc-windows-msvc',
            '-O1', '-ffreestanding', '-fno-builtin', '-fno-stack-protector',
            '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-c',
            HERE/'tests/call_fixture.c', '-o', out/'fixture.obj'])
        run('link-owned-pe', [args.lld_link, '/dll', '/noentry', '/nodefaultlib',
            '/align:16384', '/dynamicbase', '/nxcompat', '/timestamp:0',
            '/out:'+str(out/'call_fixture.dll'), out/'fixture.obj', out/'memory.lib', out/'string.lib'])
        # LLD warns about non-default section alignment; 16 KiB is deliberate
        # for this owned fixture and the explicit protection granule in the test.
        run('independent-pe-read', [args.llvm_bin/'llvm-readobj', '--file-headers',
            '--sections', '--coff-imports', '--coff-exports', '--coff-basereloc', out/'call_fixture.dll'])
        run('compile-host-runtime', [args.host_cc, '-arch', 'x86_64', '-std=c11',
            '-Wall', '-Wextra', '-Werror', '-O1', '-ffreestanding', '-fno-builtin',
            HERE.parent/'windows-image/pe_image.c', HERE.parent/'windows-services/mb_win_crt.c',
            HERE/'pe_module.c', HERE/'tests/run_owned_fixture.c', '-o', out/'run-owned'])
        result = json.loads(run('execute-owned-pe', ['/usr/bin/arch', '-x86_64',
                                                    out/'run-owned', out/'call_fixture.dll']))
        assert result == {'owned_pe_executed': True, 'imports_called': 6,
            'output': 'DLL->native:PortBridge', 'mixed_abi_result': 103.5,
            'unloaded': True, 'console_contact': False}, result
        for name, digest in receipt['source_files'].items():
            assert sha(ROOT/name) == digest, f'Source changed during verification: {name}'
        receipt.update(status='OWNED_PE_HOST_EXECUTION_PASSED', result=result,
            fixture={'bytes': (out/'call_fixture.dll').stat().st_size,
                     'sha256': sha(out/'call_fixture.dll')},
            host_runner_sha256=sha(out/'run-owned'),
            scope='Known-source no-entry/no-TLS/no-exception PE with six actual CRT imports; not arbitrary DLL or PS5 execution')
        save()
        print(json.dumps({'status': receipt['status'], 'receipt': str(out/'receipt.json')}))
    except Exception as error:
        receipt.update(status='FAILED', error=str(error))
        save()
        raise


if __name__ == '__main__':
    main()
