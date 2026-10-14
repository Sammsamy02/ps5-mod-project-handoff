#!/usr/bin/env python3
"""Build the LIVEFACTS_PROBE SDK ELF payload; never dispatch or execute it.

Fresh output directory per run (uuid); history is never overwritten. Only the
pinned toolchain is used: SDK prospero-clang/prospero-clang++ wrappers and the
pinned LLVM 21.1.8 bin dir. One build at a time; no network; no console
contact. The supplier TUs of worker-1 are compiled READ-ONLY as evidence;
the probe itself is written directly against the raw pinned SDK APIs.

Steps: host record-writer test -> supplier TU compile evidence -> probe TU
compile -> SDK ELF link (crt1.o _start, caller-local linker script via the
pinned link_layout.py) -> ET_DYN + dynamic-layout validation -> llvm-nm
import/provider table (every dynamic import must be declared by an SDK target
lib) -> receipt.json.
"""
import argparse
import hashlib
import importlib.util
import json
import os
import struct
import subprocess
import sys
import uuid
from pathlib import Path

def sha(p: Path) -> str:
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main() -> int:
    here = Path(__file__).resolve().parent
    root = here.parents[2]  # .../what (authoritative worktree root)
    sdk_default = root / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk'
    llvm_default = root / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
    supplier_default = root / 'outputs/2026-10-04-zcode-team/worker-1-live-supplier'
    layout_default = root / 'outputs/ps5-mod-bridge/native-host/archive/link_layout.py'

    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--sdk', type=Path, default=sdk_default)
    ap.add_argument('--llvm-bin', type=Path, default=llvm_default)
    ap.add_argument('--supplier', type=Path, default=supplier_default)
    ap.add_argument('--link-layout', type=Path, default=layout_default)
    ap.add_argument('--log-path', default='/data/modbridge-livefacts-probe.jsonl',
                    help='Target-side JSONL path the probe appends to (build never opens it).')
    ap.add_argument('--skip-supplier-evidence', action='store_true')
    a = ap.parse_args()

    out = here / 'build' / uuid.uuid4().hex
    out.mkdir(parents=True, exist_ok=False)
    build_id = 'lfp-' + uuid.uuid4().hex

    receipt = {
        'status': 'IN_PROGRESS',
        'build_id': build_id,
        'output_dir': str(out),
        'log_path': a.log_path,
        'log_path_opened_by_build': False,
        'console_contact': False,
        'target_execution': False,
        'prx_created': False,
        'network_calls': False,
        'commands': [],
        'source_files': {},
    }
    for f in sorted(here.glob('src/*')) + sorted(here.glob('tests/*')) + [here / 'build_probe.py']:
        receipt['source_files'][str(f)] = sha(f)

    def save() -> None:
        (out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')

    def run(label, cmd, env=None, cwd=None, timeout=180):
        cmd = [str(c) for c in cmd]
        r = subprocess.run(cmd, capture_output=True, text=True, env=env,
                           cwd=cwd or root, timeout=timeout)
        (out / (label + '.stdout')).write_text(r.stdout)
        (out / (label + '.stderr')).write_text(r.stderr)
        receipt['commands'].append({'label': label, 'argv': cmd,
                                    'exit': r.returncode})
        save()
        return r

    def must(label, cmd, env=None, cwd=None, timeout=180):
        r = run(label, cmd, env, cwd, timeout)
        if r.returncode != 0:
            receipt['status'] = 'FAILED'
            receipt['error'] = label + ' failed; see ' + str(out / (label + '.stderr'))
            save()
            print(receipt['error'], file=sys.stderr)
            raise SystemExit(1)
        return r

    try:
        sdk = a.sdk.resolve()
        llvm = a.llvm_bin.resolve()
        supplier = a.supplier.resolve()
        cc = sdk / 'bin/prospero-clang'
        cxx = sdk / 'bin/prospero-clang++'
        env = dict(os.environ, LLVM_CONFIG=str(llvm / 'llvm-config'))

        # --- 1. Host record-writer test with the pinned LLVM clang --------
        must('host-record-test-compile',
             [llvm / 'clang', '-std=c++17', '-Wall', '-Wextra', '-Werror',
              here / 'tests/test_probe_record.cpp', here / 'src/probe_record.cpp',
              '-o', out / 'test-probe-record'])
        r = must('host-record-test-run', [out / 'test-probe-record',
                                          out / 'host-record-demo.jsonl'])
        demo = (out / 'host-record-demo.jsonl').read_text()
        lines = [l for l in demo.splitlines() if l.strip()]
        for l in lines:
            rec = json.loads(l)  # raises on invalid JSON
            if (rec.get('marker') != 'modbridge.livefacts' or
                    rec.get('schema') != 1 or 'fields' not in rec):
                raise RuntimeError('host record schema mismatch: ' + l)
        receipt['host_record_test'] = {
            'output': r.stdout.strip(),
            'records': len(lines),
            'max_line_bytes': max(len(l.encode()) for l in lines),
            'all_valid_json': True,
        }

        # --- 2. Supplier TU compile evidence (read-only, not linked) ------
        receipt['supplier_compile_evidence'] = {}
        if not a.skip_supplier_evidence:
            for name in ['live_module_facts.cpp', 'live_module_os_ops_sce.cpp']:
                src = supplier / 'src' / name
                obj = out / ('supplier-' + name.replace('.cpp', '.o'))
                r = run('supplier-' + name.replace('.cpp', ''),
                        [cxx, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                         '-fPIC', '-I', supplier / 'src', '-c', src, '-o', obj],
                        env=env)
                entry = {'source': str(src), 'source_sha256': sha(src),
                         'exit': r.returncode, 'linked_into_probe': False}
                if r.returncode == 0 and obj.exists():
                    entry['object_sha256'] = sha(obj)
                else:
                    entry['stderr_tail'] = r.stderr[-1000:]
                receipt['supplier_compile_evidence'][name] = entry

        # --- 3. Probe TU compile (pinned SDK wrapper) ----------------------
        flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fPIC',
                 '-fno-exceptions', '-fno-rtti', '-O1',
                 '-DLIVEFACTS_BUILD_ID=' + json.dumps(build_id),
                 '-DLIVEFACTS_LOG_PATH=' + json.dumps(a.log_path),
                 '-I', here / 'src']
        objects = []
        for name in ['probe_record.cpp', 'livefacts_probe.cpp']:
            obj = out / (name.replace('.cpp', '.o'))
            must('probe-' + name.replace('.cpp', ''),
                 [cxx, *flags, '-c', here / 'src' / name, '-o', obj], env=env)
            objects.append(obj)
        receipt['probe_objects'] = {str(o.name): sha(o) for o in objects}

        # --- 4. SDK ELF link: crt1 _start, caller-local linker script -----
        spec = importlib.util.spec_from_file_location('livefacts_link_layout',
                                                      a.link_layout)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        script = module.write_linker_script(sdk, out)
        artifact = out / 'livefacts-probe.elf'
        must('sdk-elf-link',
             [cc, '-nostartfiles', '-Wl,-T,' + script['caller_script'],
              '-Wl,-e,_start', '-fno-lto', '-fPIC', '-pthread',
              '-Wl,--no-undefined', '-Wl,-Map,' + str(out / 'link.map'),
              *objects, sdk / 'target/lib/crt1.o', '-o', artifact], env=env)
        data = artifact.read_bytes()
        if data[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<HH', data, 16) != (3, 62):
            raise RuntimeError('Expected SDK AMD64 ELF ET_DYN')
        receipt['elf_header'] = {'magic': 'ELF64 LE', 'e_type': 3,
                                 'e_type_name': 'ET_DYN', 'e_machine': 62}
        receipt['linker_script'] = script
        receipt['dynamic_layout'] = module.validate_dynamic_layout(artifact)

        # --- 5. Symbol evidence + import provider table -------------------
        undefined = must('elf-undefined', [llvm / 'llvm-nm', '-u', artifact]).stdout
        run('elf-headers', [llvm / 'llvm-readelf', '-h', '-l', '-d', '-r', artifact])
        run('elf-symbols', [llvm / 'llvm-nm', '--defined-only', artifact])
        run('elf-startup', [llvm / 'llvm-objdump',
                            '--disassemble-symbols=_start,main', artifact])
        names = sorted(set(line.split()[-1] for line in undefined.splitlines()
                           if line.strip()))
        providers = {}
        for lib in sorted((sdk / 'target/lib').glob('*.so')):
            symbols = run('provider-' + lib.name,
                          [llvm / 'llvm-nm', '-D', '--defined-only', lib]).stdout
            providers[lib.name] = set(line.split()[-1]
                                      for line in symbols.splitlines()
                                      if line.strip())
        mappings = {name: sorted(lib for lib, syms in providers.items()
                                 if name in syms) for name in names}
        missing = {n: p for n, p in mappings.items() if not p}
        if missing:
            raise RuntimeError('SDK declares no provider for imports: ' +
                               json.dumps(missing))
        receipt['elf_undefined_symbols'] = names
        receipt['import_providers'] = mappings
        # Startup proof: _start must be crt1.o's entry, main must be ours,
        # kernel_get_fw_version comes statically from crt1.o (E7 needs no
        # dynamic import); the probed sce entry points must be dynamic imports.
        defined = must('elf-defined', [llvm / 'llvm-nm', '--defined-only',
                                       artifact]).stdout
        defined_names = set(line.split()[-1]
                            for line in defined.splitlines() if line.strip())
        for required in ['_start', 'main', 'kernel_get_fw_version',
                         '_DYNAMIC']:
            if required not in defined_names:
                raise RuntimeError('missing required defined symbol: ' + required)
        for imported in ['sceKernelGetModuleList',
                         'sceKernelGetExecutableModuleHandle',
                         'sceKernelGetModuleInfo', 'sysctl', 'getpid',
                         'open', 'write', '__error']:
            if imported not in names:
                raise RuntimeError('missing required dynamic import: ' + imported)
        # DISCOVERED at link time: dlopen/dlclose resolve STATICALLY from
        # libc.a(dlfcn.o) into crt1.o's bundled rtld (__dlopen/__dlclose),
        # so E6 retention goes through the SDK payload's own loader, not a
        # dynamic libkernel import (libkernel exports dlopen too, unused).
        receipt['dlopen_resolution'] = {
            'dynamic_import': False,
            'evidence': 'link.map: .text.dlopen from libc.a(dlfcn.o); '
                        '.text.__dlopen from crt1.o',
        }

        # --- 6. Final artifact + dependency pins --------------------------
        receipt['artifact'] = {
            'path': str(artifact),
            'sha256': sha(artifact),
            'bytes': len(data),
            'startup': 'Genuine SDK payload crt1 _start, not a PRX module entry',
            'entry_symbol': '_start (crt1.o); crt calls main()',
            'execution': 'NOT executed by this build; controller dispatches',
        }
        receipt['exit_code_contract'] = {
            0: 'all experiments + summary recorded',
            90: 'log open failed (nothing written)',
            92: 'log write/close failed (partial records possible)',
        }
        receipt['experiment_static_notes'] = {
            'e1_dtdebug_map': 'walks own _DYNAMIC via extern Elf64_Dyn _DYNAMIC[]; '
                              'DT_DEBUG from sys/elf_common.h; r_debug/link_map from '
                              'sys/link_elf.h; <=64 entries, names <=64 bytes escaped; '
                              'rc -1 absent, -2 unfilled, -3 null r_map',
            'e2_proc_pathname': 'sysctl mib {CTL_KERN,KERN_PROC,KERN_PROC_PATHNAME,-1}; '
                                'path echoed escaped (<=256 bytes)',
            'e3_exec_handle': 'UNPINNED decl int64_t(void) (probe_abi.hpp); raw + errno '
                              'recorded; success judged only by 352-byte size echo + '
                              'segment_count<=4 via pinned ModuleInfo',
            'e4_module_list_unit': 'no SDK header; prosperodev 3-arg and brief 4-arg '
                                   'spellings both called with size=4N and N through a '
                                   '6-arg fn pointer; every arg register holds 0 or owned '
                                   'zeroed scratch; validation via 352-byte echo',
            'e5_proc_vmmap': 'mib {CTL_KERN,KERN_PROC,KERN_PROC_VMMAP,getpid()}; growth '
                             '64KiB*2 attempts to 4MiB cap; entries per sizeof(kinfo_vmentry) '
                             'from sys/user.h',
            'e6_self_retention': 'RTLD_NOLOAD(0x02000)|RTLD_NOW from SDK dlfcn.h; dlclose '
                                 'exactly once; domain equality vs E3 raw handle printed',
            'e7_fw_version': 'kernel_get_fw_version() declared in pinned '
                             'target/include/ps5/kernel.h; DEFINED by crt1.o (static, no '
                             'dynamic import); raw uint32 recorded',
        }
        receipt['pinned_dependencies'] = {
            str(a.link_layout): sha(a.link_layout),
            str(cc): sha(cc),
            str(cxx): sha(cxx),
            str(llvm / 'clang'): sha(llvm / 'clang'),
            str(llvm / 'llvm-nm'): sha(llvm / 'llvm-nm'),
            str(llvm / 'llvm-readelf'): sha(llvm / 'llvm-readelf'),
            str(llvm / 'llvm-objdump'): sha(llvm / 'llvm-objdump'),
        }
        for name in ['crt1.o', 'libc.a', 'libpthread.a', 'libkernel_web.so',
                     'libSceLibcInternal.so', 'libSceNet.so', 'libkernel.so']:
            p = sdk / 'target/lib' / name
            if p.exists():
                receipt['pinned_dependencies'][str(p)] = sha(p)
        for f, digest in receipt['source_files'].items():
            if sha(f) != digest:
                raise RuntimeError('source changed during build: ' + f)
        receipt['status'] = 'LIVEFACTS_PROBE_BUILT_NOT_EXECUTED'
        save()
        print(json.dumps({'status': receipt['status'], 'build_id': build_id,
                          'receipt': str(out / 'receipt.json'),
                          'artifact': str(artifact),
                          'sha256': receipt['artifact']['sha256'],
                          'bytes': len(data)}))
        return 0
    except SystemExit:
        raise
    except Exception as e:
        receipt['status'] = 'FAILED'
        receipt['error'] = str(e)
        save()
        print(json.dumps({'status': 'FAILED', 'error': str(e)}), file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
