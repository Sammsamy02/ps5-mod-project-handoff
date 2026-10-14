#!/usr/bin/env python3
"""L10LM compile gate v2 (worker-luamod-rebase, 2026-10-04).

Recovers the EXACT pinned luamod unit argv from the notify-cancel build-1 base
receipt (172 tokens; the recovery pattern worker-launch10-gmalloc-integration
used), then runs three compiles and four comparisons:

  control    : the UNTOUCHED overlay copy 0f0697a3... under the pinned argv
               (source/-o/-MF redirected). MUST compile exit 0. It is a DRIFT
               WITNESS, not a bit-reproduction: the rr-attested Resolution-B
               NativeProgramContext.hpp edit (2026-10-04 21:41) post-dates the
               pinned build-1 luamod.o (2026-10-03 22:04), and
               UE4SSProgram.hpp:127 embeds Platform::NativeProgramContext by
               value, so UE4SSProgram member offsets moved. The gate ASSERTS
               the control-vs-pinned delta is EXACTLY the attributed set of
               member-offset immediates (see ATTRIBUTED below); any additional
               differing byte fails the gate.
  variant B  : staging/luamod-l10rebase/LuaMod.cpp (rebased DIFF-1, file-local
               kFuncPtrMarshalExecutor, NO SettingsManager.hpp change) under
               the SAME redirected argv - byte-for-byte the compile the
               retained-unit replacement path performs (SEC32-3 argv-drift
               {si,oi,mi}). Must add R3 symbols and add NOTHING ELSE vs control
               except the R3 additions.
  variant A  : staging/luamod-l10rebase-variantA-full/LuaMod.cpp (WITH the enum
               append), one prepended -I shadowing the port SettingsManager.hpp
               with the DIFF-1 patched copy. Fallback evidence only; variant A
               is NOT admitted by the delivered driver edition.

  checks: control==drift-witness (6 attributed bytes vs pinned);
          variantB object == variantA object (formulation equivalence at
          object level); variantB vs control delta confined to R3 additions
          (size + symbol census).

Writes receipts/compile-gate.json. No console contact, no composition writes,
originals untouched everywhere.
"""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
MY = ROOT / 'outputs/2026-10-04-zcode-team/worker-luamod-rebase'
BASE_RECEIPT = ROOT / '.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/receipt.json'
PINNED_OBJ = ROOT / '.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/luamod.o'
ODR_HEADER = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp'
LLVM_NM = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm'
OUT = MY / 'receipts'

# The only byte deltas control-vs-pinned that this gate attributes: member-offset
# immediates in RC::UE4SSProgram header-member functions + LuaMod::fire_on_lua_start_for_cpp_mods,
# each +0x30, from the rr-attested NativeProgramContext.hpp edit (measured twice,
# offsets stable across runs).
ATTRIBUTED_OFFSETS = [9331, 9421, 9479, 9747, 61309, 62141]
ATTRIBUTED_DELTA = 0x30

# Symbols/strings that MEASURABLY survive in the variant B object (reachable from
# ExecuteInGameThread paths). The drain cluster (funcptr_marshal_hook,
# arm_funcptr_marshal_drain, r3_marshal_carrier_precheck) and the R3Marshal* bridge
# references are file-static with NO TU-internal caller and are dead-code-eliminated
# by construction - recorded as a measured finding (r3_link_inertness), not a gate
# failure: it is inherent to DIFF-1's authored linkage, independent of the rebase.
R3_SURVIVING_SYMBOLS = ['m_funcptr_marshal_armed', 'is_funcptr_marshal_armed']
R3_SURVIVING_STRINGS = ['FuncPtrMarshal',
                        'EngineTick method requested but neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed',
                        'Neither EngineTick nor ProcessEvent hooks are available and the R3 FuncPtr-marshal carrier is not armed']
R3_ELIMINATED = ['funcptr_marshal_hook', 'arm_funcptr_marshal_drain', 'r3_marshal_carrier_precheck',
                 'R3MarshalPublishGameThreadIdentity', 'R3MarshalQueryHarnessDispatch', 'R3MarshalPumpPending']


def sha(p: Path) -> str:
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def nm(path: Path) -> dict:
    p = subprocess.run([str(LLVM_NM), '--defined-only', str(path)], capture_output=True, text=True)
    defined = set()
    for line in p.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3:
            defined.add(parts[2])
    pu = subprocess.run([str(LLVM_NM), '--undefined-only', str(path)], capture_output=True, text=True)
    und = set()
    for line in pu.stdout.splitlines():
        parts = line.split()
        if len(parts) == 2:
            und.add(parts[1])
    return {'defined': sorted(defined), 'undefined': sorted(und)}


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    rec = json.loads(BASE_RECEIPT.read_text())
    unit = rec['selected_runtime_units']['luamod']
    argv = list(unit['argv'])
    llvm_config = unit['LLVM_CONFIG']
    pinned_obj_sha = unit['object_sha256']
    pinned_src = unit['source']

    si = argv.index(pinned_src)
    oi = argv.index('-o') + 1
    mi = argv.index('-MF') + 1

    def redirected(src: Path, obj: Path, dep: Path, prepend_include=None):
        a = list(argv)
        a[si] = str(src)
        a[oi] = str(obj)
        a[mi] = str(dep)
        if prepend_include is not None:
            i = a.index('-I')
            a[i:i] = ['-I', str(prepend_include)]
        env = dict(os.environ, LLVM_CONFIG=llvm_config)
        p = subprocess.run(a, capture_output=True, text=True, env=env, cwd=str(ROOT))
        return p, a

    results = {'gate': 'L10LM-COMPILE-GATE-2', 'worker': 'worker-luamod-rebase',
               'argv_tokens': len(argv), 'llvm_config': llvm_config,
               'pinned_object_sha256': pinned_obj_sha, 'pinned_source': pinned_src,
               'odr_header_mtime_attribution': {
                   'header': str(ODR_HEADER),
                   'pinned_build_mtime': '2026-10-03T22:04:49 (luamod.o)',
                   'header_edit_mtime': '2026-10-04T21:41 (post-pinned; rr-attested Resolution-B edit)',
                   'chain': 'UE4SSProgram.hpp:127 embeds Platform::NativeProgramContext by value'},
               'compiles': {}}
    ok = True

    def do_compile(name, src, obj, dep, prepend=None):
        nonlocal ok
        p, a = redirected(src, obj, dep, prepend_include=prepend)
        entry = {'name': name, 'exit': p.returncode,
                 'object_sha256': sha(obj) if p.returncode == 0 else None,
                 'object_bytes': obj.stat().st_size if p.returncode == 0 else None,
                 'dependency_files': len(dep.read_text().replace('\\\n', ' ').split()) - 1 if p.returncode == 0 else None}
        if p.returncode:
            entry['stderr_tail'] = p.stderr[-4000:]
            ok = False
        return entry, a

    ctrl, _ = do_compile('control-overlay-0f0697a3-drift-witness',
                         MY / 'work/tree-overlay/port/ue4ss/src/Mod/LuaMod.cpp',
                         OUT / 'gate-control.o', OUT / 'gate-control.d')
    vub, vub_argv = do_compile('variantB-rebased-noheader',
                               MY / 'staging/luamod-l10rebase/LuaMod.cpp',
                               OUT / 'gate-variantB.o', OUT / 'gate-variantB.d')
    vub['argv_drift_indices'] = [i for i in range(len(vub_argv)) if vub_argv[i] != argv[i]]
    vub['argv_drift_expected'] = sorted({si, oi, mi})
    vub['argv_drift_exact'] = sorted(vub['argv_drift_indices']) == sorted({si, oi, mi})
    ok &= vub['argv_drift_exact']
    vua, _ = do_compile('variantA-with-enum-append-FALLBACK',
                        MY / 'staging/luamod-l10rebase-variantA-full/LuaMod.cpp',
                        OUT / 'gate-variantA.o', OUT / 'gate-variantA.d',
                        prepend=MY / 'staging/luamod-l10rebase-variantA-full/port-ue4ss-include')
    vua['note'] = ('prepended -I shadows the port SettingsManager.hpp with the DIFF-1 patched '
                   'copy; NOT admitted by the delivered driver edition (BLOCKER 3 recompile set)')

    results['compiles'] = {'control': ctrl, 'variantB': vub, 'variantA': vua}

    # ---- drift-witness assertion: control vs pinned -------------------------
    if ctrl['object_sha256']:
        a = PINNED_OBJ.read_bytes()
        b = (OUT / 'gate-control.o').read_bytes()
        diffs = [{'offset': i, 'pinned': a[i], 'fresh': b[i], 'delta': b[i] - a[i]}
                 for i in range(min(len(a), len(b))) if a[i] != b[i]]
        sizes_equal = len(a) == len(b)
        attributed = (len(diffs) == len(ATTRIBUTED_OFFSETS)
                      and all(d['offset'] == o and d['delta'] == ATTRIBUTED_DELTA
                              for d, o in zip(diffs, ATTRIBUTED_OFFSETS)))
        ctrl['vs_pinned'] = {'sizes_equal': sizes_equal, 'differing_bytes': len(diffs),
                             'diffs': diffs, 'matches_attribution': attributed,
                             'verdict': ('all differing bytes are the attributed +0x30 '
                                         'UE4SSProgram member-offset shifts from the rr-attested '
                                         'NativeProgramContext.hpp edit' if attributed else
                                         'UNEXPECTED DRIFT - gate fails')}
        if not (sizes_equal and attributed):
            ok = False

    # ---- formulation equivalence: variantB == variantA ----------------------
    if vub['object_sha256'] and vua['object_sha256']:
        results['formulation_equivalence'] = {
            'variantB_object': vub['object_sha256'],
            'variantA_object': vua['object_sha256'],
            'identical': vub['object_sha256'] == vua['object_sha256'],
            'note': ('named appended enumerator (value 2) vs file-local constexpr alias of 2 '
                     'produce the byte-identical object over the same include tree')}
        ok &= vub['object_sha256'] == vua['object_sha256']

    # ---- R3 additions: variantB vs control ---------------------------------
    if vub['object_sha256'] and ctrl['object_sha256']:
        def nm_demangle(path: Path) -> set:
            p = subprocess.run([str(LLVM_NM), '--demangle', str(path)], capture_output=True, text=True)
            return {line.split(' ', 2)[-1] for line in p.stdout.splitlines() if line.strip()}
        sym_b = nm_demangle(OUT / 'gate-variantB.o')
        sym_c = nm_demangle(OUT / 'gate-control.o')
        str_b = (OUT / 'gate-variantB.o').read_bytes()
        str_c = (OUT / 'gate-control.o').read_bytes()

        def has_str(blob: bytes, s: str) -> bool:
            encodings = [s.encode('utf-8'), s.encode('utf-16-le')]
            return any(e in blob for e in encodings)

        surviving = {s: {'variantB_symbol': any(s in x for x in sym_b), 'control_symbol': any(s in x for x in sym_c)} for s in R3_SURVIVING_SYMBOLS}
        surviving_strings = {s: {'variantB': has_str(str_b, s), 'control': has_str(str_c, s)} for s in R3_SURVIVING_STRINGS}
        eliminated = {s: {'variantB_symbol': any(s in x for x in sym_b), 'variantB_in_strings': s.encode() in str_b}
                      for s in R3_ELIMINATED}
        pu = subprocess.run([str(LLVM_NM), str(OUT / 'gate-variantB.o')], capture_output=True, text=True)
        pu2 = subprocess.run([str(LLVM_NM), str(OUT / 'gate-control.o')], capture_output=True, text=True)
        und_b = {l.split()[-1] for l in pu.stdout.splitlines() if ' U ' in l}
        und_c = {l.split()[-1] for l in pu2.stdout.splitlines() if ' U ' in l}
        results['r3_additions'] = {
            'variantB_bytes_minus_control': vub['object_bytes'] - ctrl['object_bytes'],
            'surviving_symbols': surviving,
            'surviving_strings': surviving_strings,
            'r3_link_inertness': {
                'finding': ('the drain cluster and the R3Marshal* bridge references are file-static '
                            'with NO TU-internal caller; the compiler dead-code-eliminates them. '
                            'DIFF-1 as authored delivers readiness + ladder + table entry; arming and '
                            'the pump drain are INERT until a caller for arm_funcptr_marshal_drain '
                            'exists (non-static linkage or an in-TU caller). Inherent to DIFF-1\'s '
                            'authored linkage; a controller-level design decision, not a rebase defect.'),
                'eliminated_symbols': eliminated,
                'variantB_undefined_count': len(und_b),
                'control_undefined_count': len(und_c),
                'undefined_added': sorted(und_b - und_c),
                'undefined_removed': sorted(und_c - und_b),
                'port_base_copy_dependency_evidence': {
                    'compile': 'worker-r3-diffs-authoring/work/patched/LuaMod.cpp (ce61f02a...) under the pinned luamod argv over the CURRENT unpatched header',
                    'exit': 1,
                    'stderr': 'receipts/gate-portA.stderr',
                    'errors': '4x no-member-named-FuncPtrMarshal - the as-authored DIFF-1 hard-depends on the SettingsManager.hpp enum append; variant B removes the dependency',
                },
            },
        }
        # gate assertions: surviving markers present in variantB, absent in control;
        # ladder strings added; NO new UND symbols
        for s, st in surviving.items():
            if not st['variantB_symbol'] or st['control_symbol']:
                ok = False
        for s, st in surviving_strings.items():
            if not st['variantB'] or st['control']:
                ok = False
        if und_b - und_c:
            ok = False

    results['gate_pass'] = bool(ok)
    (OUT / 'compile-gate.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps({'gate_pass': results['gate_pass'],
                      'control_vs_pinned': ctrl.get('vs_pinned', {}).get('verdict'),
                      'formulation_equivalence': results.get('formulation_equivalence', {}).get('identical'),
                      'variantB_minus_control_bytes': results.get('r3_additions', {}).get('variantB_bytes_minus_control')}, indent=1))
    return 0 if results['gate_pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
