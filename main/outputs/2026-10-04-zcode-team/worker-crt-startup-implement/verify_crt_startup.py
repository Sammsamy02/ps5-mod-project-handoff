#!/usr/bin/env python3
"""CRT startup family (_initterm/_initterm_e/_seh_filter_dll/_crt_atexit/onexit tables/_cexit) for the retained PE runtime.
VENDORED ADAPTED COPY (origin: pattern of outputs/2026-10-04-zcode-team/worker-4-win-services/verify_init_once.py; unchanged original).
No target execution, console contact, arbitrary DLL execution or runtime install.
Recorded native SDK provider declarations are component evidence only, never a target resolution claim."""
import argparse, importlib.util, json, os, platform, re, subprocess, uuid
from pathlib import Path

HERE = Path(__file__).resolve().parent
SERV = HERE.parents[2] / 'outputs/ps5-mod-bridge/windows-services'
EXEC = HERE.parents[2] / 'outputs/ps5-mod-bridge/windows-execution'
IMAGE = HERE.parents[2] / 'outputs/ps5-mod-bridge/windows-image'
SRC = HERE / 'sources'
_spec = importlib.util.spec_from_file_location('mb_windows_verify_helpers', SERV / 'verify.py')
_helpers = importlib.util.module_from_spec(_spec); _spec.loader.exec_module(_helpers)
ROOT, sha, data_layout = _helpers.ROOT, _helpers.sha, _helpers.data_layout


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host-only', action='store_true'); p.add_argument('--sanitize', action='store_true')
    p.add_argument('--red-stub', action='store_true')
    p.add_argument('--output', type=Path, default=HERE / 'receipts')
    a = p.parse_args(); out = a.output.resolve() / uuid.uuid4().hex; out.mkdir(parents=True)
    sdk = ROOT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
    llvm = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    sources = [SRC / x for x in ['mb_win_crt_startup.h', 'mb_win_crt_startup.c',
                                 'mb_win_crt_startup_abi.h', 'mb_win_crt_startup_abi.c',
                                 'tests/test_win_crt_startup.c']]
    sources += [HERE / 'verify_crt_startup.py', SERV / 'verify.py', EXEC / 'pe_module.h', IMAGE / 'pe_image.h']
    rec = {'status': 'IN_PROGRESS', 'commands': [], 'sources': {str(x): sha(x) for x in sources},
           'target_executed': False, 'console_contact': False, 'pe_fixture_executed': False}
    def save(): (out / 'receipt.json').write_text(json.dumps(rec, indent=2) + '\n')
    def run(name, argv, env=None, expected=0):
        argv = list(map(str, argv)); r = subprocess.run(argv, cwd=ROOT, env=env, capture_output=True, text=True, timeout=120)
        (out / (name + '.stdout')).write_text(r.stdout); (out / (name + '.stderr')).write_text(r.stderr)
        rec['commands'].append({'label': name, 'argv': argv, 'exit': r.returncode, 'expected_exit': expected}); save()
        if r.returncode != expected: raise RuntimeError(name + ' failed: ' + r.stderr)
        return r.stdout
    flags = ['-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin']
    arch = ['-arch', 'x86_64'] if platform.system() == 'Darwin' else []
    runner = ['/usr/bin/arch', '-x86_64'] if arch else []
    include = ['-I', SRC, '-I', EXEC]
    try:
        owner_source = SRC / 'mb_win_crt_startup.c'
        if a.red_stub:
            content = owner_source.read_text()
            start = content.index('{', content.index('int mb_win_crt_dll_process_attach(')); end = start + 1; depth = 1
            while depth:
                depth += (content[end] == '{') - (content[end] == '}'); end += 1
            owner_source = out / 'attach-stub.c'
            owner_source.write_text(content[:start] + '{(void)xi_begin;(void)xi_end;(void)xc_begin;(void)xc_end;return -1;}' + content[end:])
            rec['negative_control'] = {'path': str(owner_source), 'sha256': sha(owner_source),
                                       'mutation': 'Only the attach orchestrator returns -1; expected real typed test rejects it.'}
        modes = [('host', [])] + ([('sanitized', ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]
                                  if a.sanitize and not a.red_stub else [])
        for label, extra in modes:
            exe = out / (label + '-test')
            run(label + '-compile', ['clang', *arch, *flags, *extra, *include, owner_source, SRC / 'mb_win_crt_startup_abi.c',
                                     SRC / 'tests/test_win_crt_startup.c', '-o', exe])
            rec[label + '_result'] = run(label + '-run', [*runner, exe], expected=1 if a.red_stub else 0).strip()
        if a.red_stub:
            assert 'mb_win_crt_dll_process_attach(g_xi,g_xi+5,g_xc_atexit,g_xc_atexit+2)==0' in (out / 'host-run.stderr').read_text().replace(' ', '')
            assert rec['sources'] == {str(x): sha(x) for x in sources}
            rec['sources_stable'] = True; rec['status'] = 'EXPECTED_ATTACH_STUB_REJECTION'; save()
            print(json.dumps({'status': rec['status'], 'receipt': str(out / 'receipt.json')})); return
        if not a.host_only:
            cc = sdk / 'bin/prospero-clang'; env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config'))
            nf = [*flags, '-fPIC', '-march=x86-64', *include]
            bf = [*nf, '--target=x86_64-unknown-freebsd9', '-fshort-wchar']
            for label, fl in [('native', nf), ('bridge', bf)]:
                run(label + '-layout', [cc, *fl, '-DMB_WIN_CRT_STARTUP_LAYOUT', '-S', '-emit-llvm',
                                        SRC / 'tests/test_win_crt_startup.c', '-o', out / (label + '-layout.ll')], env)
            n = (out / 'native-layout.ll').read_text(); b = (out / 'bridge-layout.ll').read_text()
            def layout(s): return list(map(int, re.findall(r'i64 (\d+)', next(x for x in s.splitlines() if x.startswith('@mb_crt_startup_layout =')))))
            expected_layout = [8, 8, 4, 4, 24, 8, 0, 8, 16, 24, 0, 16]
            assert data_layout(n) == data_layout(b) and layout(n) == layout(b) == expected_layout, (layout(n), layout(b))
            run('native-owner', [cc, *nf, '-MD', '-MF', out / 'owner.d', '-c', SRC / 'mb_win_crt_startup.c', '-o', out / 'mb_win_crt_startup.o'], env)
            ir = out / 'crt_startup_abi.ll'
            run('bridge-ir', [cc, *bf, '-MD', '-MF', out / 'abi.d', '-S', '-emit-llvm', SRC / 'mb_win_crt_startup_abi.c', '-o', ir], env)
            source = ir.read_text()
            defs = [x for x in source.splitlines() if x.startswith('define ') and 'win64cc' in x]
            assert len(defs) == 8 and data_layout(source) == data_layout(n)
            assert sum('win64cc void @svc_initterm(' in x for x in defs) == 1
            assert sum(bool(re.search(r'win64cc i32 @svc_initterm_e\(ptr [^,]*, ptr [^)]*\)', x)) for x in defs) == 1
            assert sum(bool(re.search(r'win64cc i32 @svc_seh_filter_dll\(i32 [^,]*, ptr [^)]*\)', x)) for x in defs) == 1
            assert sum(bool(re.search(r'win64cc i32 @svc_crt_atexit\(ptr [^)]*\)', x)) for x in defs) == 1
            assert sum('win64cc void @svc_cexit()' in x for x in defs) == 1
            assert sum(bool(re.search(r'win64cc i32 @svc_initialize_onexit_table\(ptr [^)]*\)', x)) for x in defs) == 1
            assert sum(bool(re.search(r'win64cc ptr @svc_register_onexit_function\(ptr [^,]*, ptr [^)]*\)', x)) for x in defs) == 1
            assert sum(bool(re.search(r'win64cc i32 @svc_execute_onexit_table\(ptr [^)]*\)', x)) for x in defs) == 1
            binder = next(x for x in source.splitlines() if '@mb_win_crt_startup_bind(' in x and x.startswith('define '))
            assert 'win64cc' not in binder
            run('bridge-backend', [cc, '-x', 'ir', '-O2', '-fPIC', '-Werror', '-Wno-override-module', '-c', ir, '-o', out / 'mb_win_crt_startup_abi.o'], env)
            rec['native'] = {'method': 'Direct PS5 core; FreeBSD9 frontend -> unedited Win64 IR -> PS5 backend',
                             'layout': layout(n), 'data_layout': data_layout(n), 'ir_sha256': sha(ir), 'objects': []}
            for name in ['mb_win_crt_startup', 'mb_win_crt_startup_abi']:
                obj = out / (name + '.o')
                undef = run(name + '-undefined', [llvm / 'llvm-nm', '--undefined-only', '--format=posix', obj])
                run(name + '-disassembly', [llvm / 'llvm-objdump', '-dr', obj])
                rec['native']['objects'].append({'path': str(obj), 'sha256': sha(obj), 'bytes': obj.stat().st_size,
                                                 'imports': [x.split()[0] for x in undef.splitlines() if x.split()]})
            rec['native_dependencies'] = {}
            for dep in ['owner.d', 'abi.d']:
                for word in (out / dep).read_text().replace('\\\n', ' ').split()[1:]:
                    f = Path(word)
                    if f.is_file(): rec['native_dependencies'][str(f)] = sha(f)
            # The owner object is freestanding: it must place zero undefined-symbol
            # demands on the target SDK. The ABI object may reference only the
            # owner's own exported mb_win_crt_* symbols (cross-object, same
            # runtime); every such reference must be defined by the owner object.
            owner_undef = set(rec['native']['objects'][0]['imports'])
            assert not owner_undef, sorted(owner_undef)
            owner_defined = run('owner-defined', [llvm / 'llvm-nm', '--defined-only', '--format=posix', out / 'mb_win_crt_startup.o'])
            defined = {x.split()[0] for x in owner_defined.splitlines() if x.split()}
            abi_undef = set(rec['native']['objects'][1]['imports'])
            rec['sdk_declarations'] = {'note': 'Freestanding family: owner has no undefined imports; the only ABI-unit references are mb_win_crt_* cross-object symbols defined by the owner object. No target(PS5) support claim.',
                                       'owner_undefined': sorted(owner_undef),
                                       'abi_undefined': sorted(abi_undef), 'target_resolution_proven': False}
            assert abi_undef <= defined, sorted(abi_undef - defined)
        assert rec['sources'] == {str(x): sha(x) for x in sources}
        rec['sources_stable'] = True
        rec['status'] = 'CRT_STARTUP_WIN64_AND_NATIVE_OBJECTS_PASSED' if not a.host_only else 'CRT_STARTUP_WIN64_PASSED'
        save()
        print(json.dumps({'status': rec['status'], 'receipt': str(out / 'receipt.json')}, indent=2))
    except Exception as e:
        rec['status'] = 'FAILED'; rec['error'] = str(e); save(); raise
if __name__ == '__main__':
    main()
