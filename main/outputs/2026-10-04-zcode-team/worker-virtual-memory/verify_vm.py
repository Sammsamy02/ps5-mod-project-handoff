#!/usr/bin/env python3
"""Virtual-memory family verifier: sanitized host state-machine tests plus
cross-compile of the same owner under the pinned prospero-clang SDK.

No target execution, console contact, network, or runtime install. Recorded
SDK export declarations are component evidence only, never a target
resolution claim. Receipts land under receipts/<uuid>/."""
import argparse, hashlib, json, os, platform, re, subprocess, uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2] / 'outputs/ps5-mod-bridge'
SRC = HERE / 'sources'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def data_layout(ir):
    return re.search(r'^target datalayout = "([^"]+)"$', ir, re.M).group(1)


def layout(ir):
    line = next(x for x in ir.splitlines() if x.startswith('@mb_win_vm_layout ='))
    return list(map(int, re.findall(r'i64 (\d+)', line)))


EXPECTED_LAYOUT = [48, 0, 8, 16, 20, 24, 32, 36, 40, 80, 40, 8]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host-only', action='store_true')
    p.add_argument('--output', type=Path, default=HERE / 'receipts')
    a = p.parse_args()
    out = a.output.resolve() / uuid.uuid4().hex
    out.mkdir(parents=True)
    sdk = ROOT / 'native-host/build/sdk-v0.43/ps5-payload-sdk'
    llvm = ROOT / 'native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    sources = [SRC / 'mb_win_vm.h', SRC / 'mb_win_vm.c',
               SRC / 'tests/test_win_vm.c', HERE / 'verify_vm.py']
    rec = {'status': 'IN_PROGRESS', 'commands': [], 'sources': {str(x): sha(x) for x in sources},
           'target_executed': False, 'console_contact': False, 'pe_fixture_executed': False}
    def save(): (out / 'receipt.json').write_text(json.dumps(rec, indent=2) + '\n')
    def run(name, argv, env=None, expected=0):
        argv = list(map(str, argv))
        r = subprocess.run(argv, cwd=ROOT, env=env, capture_output=True, text=True, timeout=300)
        (out / (name + '.stdout')).write_text(r.stdout)
        (out / (name + '.stderr')).write_text(r.stderr)
        rec['commands'].append({'label': name, 'argv': argv, 'exit': r.returncode,
                                'expected_exit': expected})
        save()
        if r.returncode != expected:
            raise RuntimeError(name + ' failed: ' + r.stderr[-2000:])
        return r.stdout
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin']
    arch = ['-arch', 'x86_64'] if platform.system() == 'Darwin' else []
    runner = ['/usr/bin/arch', '-x86_64'] if arch else []
    include = ['-I', SRC]
    try:
        # Host: sanitized plus plain builds of the full test binary.
        for label, extra in [('sanitized', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']),
                             ('plain', [])]:
            exe = out / (label + '-test')
            run(label + '-compile', ['clang', *arch, *flags, *extra, *include,
                                     SRC / 'mb_win_vm.c', SRC / 'tests/test_win_vm.c', '-o', exe])
            rec[label + '_result'] = run(label + '-run', [*runner, exe]).strip()
        if not a.host_only:
            # Pinned prospero-clang: owner object for the PS5 core, plus the
            # same translation unit emitted as Win64-flavoured IR for layout
            # comparison (FreeBSD9 frontend, short wchar), per the bridge
            # route used by the windows-services families.
            cc = sdk / 'bin/prospero-clang'
            env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config'))
            nf = [*flags, '-fPIC', '-march=x86-64', *include]
            bf = [*nf, '--target=x86_64-unknown-freebsd9', '-fshort-wchar']
            for label, fl in [('native', nf), ('bridge', bf)]:
                run(label + '-layout', [cc, *fl, '-DMB_WIN_VM_LAYOUT', '-S', '-emit-llvm',
                                        SRC / 'tests/test_win_vm.c', '-o', out / (label + '-layout.ll')], env)
            n, b = (out / 'native-layout.ll').read_text(), (out / 'bridge-layout.ll').read_text()
            assert data_layout(n) == data_layout(b), 'target data layouts diverge'
            assert layout(n) == layout(b) == EXPECTED_LAYOUT, (layout(n), layout(b))
            rec['layout'] = {'memory_basic_information_win64': EXPECTED_LAYOUT[:9],
                             'owner_bytes': EXPECTED_LAYOUT[9], 'ops_bytes': EXPECTED_LAYOUT[10],
                             'pointer_bytes': EXPECTED_LAYOUT[11]}
            run('native-owner', [cc, *nf, '-MD', '-MF', out / 'owner.d', '-c', SRC / 'mb_win_vm.c',
                                 '-o', out / 'mb_win_vm.o'], env)
            # Retained SDK export declarations only, never a target resolution claim.
            obj = out / 'mb_win_vm.o'
            undef = run('owner-undefined', [llvm / 'llvm-nm', '--undefined-only', '--format=posix', obj])
            imports = [x.split()[0] for x in undef.splitlines() if x.split()]
            rec['native'] = {'method': 'Direct PS5 core object via pinned prospero-clang',
                             'object': str(obj), 'sha256': sha(obj),
                             'bytes': obj.stat().st_size, 'imports': imports}
            rec['sdk_declarations'] = {}
            declared = set()
            for lib in ['libkernel_web.so', 'libSceLibcInternal.so']:
                path = sdk / 'target/lib' / lib
                if path.exists():
                    text = run(lib + '-exports', [llvm / 'llvm-nm', '-D', '--defined-only',
                                                  '--format=posix', path])
                    names = {x.split()[0] for x in text.splitlines() if x.split()}
                    needed = set(imports)
                    rec['sdk_declarations'][lib] = {'path': str(path), 'sha256': sha(path),
                                                    'covers': sorted(needed & names)}
                    declared |= names
            required = set(imports)
            assert required <= declared, sorted(required - declared)
            rec['sdk_declarations_proven'] = True
        assert rec['sources'] == {str(x): sha(x) for x in sources}
        rec['sources_stable'] = True
        rec['status'] = 'VM_FAMILY_HOST_AND_NATIVE_OBJECTS_PASSED' if not a.host_only \
            else 'VM_FAMILY_HOST_PASSED'
        save()
        print(json.dumps({'status': rec['status'], 'checks': rec.get('sanitized_result'),
                          'receipt': str(out / 'receipt.json')}, indent=2))
    except Exception as e:
        rec['status'] = 'FAILED'; rec['error'] = str(e); save(); raise


if __name__ == '__main__':
    main()
