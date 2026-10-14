#!/usr/bin/env python3
"""L10R3 fold-equivalence pre-check (worker-launch10-r3-armed).

Compiles the staged R3-armed initializer under the EXACT composition argv
construction used by build_runtime_composition6_fixed.py for affected-unit
replacement (pinned argv; only source/-o/-MF redirected; -I W/include and
-I witness/include prepended at argv[1:1]; LLVM_CONFIG env) and proves:

  flip1: staged copy with in-file fallback=1, NO build define  -> exit 0
  flag1: same bytes with fallback=0 + -DUE4SS_R3_MARSHAL_MODE=1 -> exit 0
  fold0: same bytes with fallback=0, no define                  -> exit 0

Checks:
  E1  sha256(flip1.o) == sha256(flag1.o)          (in-file flip == build define)
  E2  sha256(fold0.o) == the prebuild's armed fold-0 initializer object
      983b18e2... (rev-B fold-0 is content-neutral ON THE ARMED STAGING: the
      terminal -I resolves the ARMED SeedTable.h, so the no-define compile of
      the patched file must reproduce worker-launch10-prebuild's armed object
      byte-exactly). (The rebase gate's fe639a85 control identity corresponds
      to its DISARMED-table staging and is not comparable here.)
  E3  recorded: flip1 object vs the rebase gate's fold-1 object 7f058ed8...
      (that one compiled against the DISARMED 3/4 table, so a byte difference
      from the armed seed constants is expected; recorded, not asserted).

No console contact; writes only under this worker's dir.
"""
import hashlib
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
SPIKE = ROOT / '.superpowers/native-dependency-spike'
MY = ROOT / 'outputs/2026-10-04-zcode-team/worker-launch10-r3-armed'
BASE = SPIKE / 'ue4ss-native-notify-cancel-1/build-1/receipt.json'
W = SPIKE / 'ue4ss-native-tick-file-owner-1'
GATE = MY / 'work/gate'
GATE.mkdir(parents=True, exist_ok=True)

STAGED = MY / 'staging/initializer-l10-r3/UnrealInitializer.cpp'          # fallback=1
PATCHED0 = MY / 'work/patchroot/staging/UnrealInitializer.cpp'            # fallback=0
ATTESTED_OBJ_SHA = 'fe639a859b2f'
REBASE_FOLD1_SHA = '7f058ed8'


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    base = json.loads(BASE.read_text())
    unit = base['selected_runtime_units']['unreal-unrealinitializer']
    argv0 = list(unit['argv'])
    prepends = ['-I', str(W / 'include'), '-I', str(SPIKE / 'ue4ss-native-tick-witness-1/include')]
    print(f'base argv tokens: {len(argv0)} (expect 186)')

    variants = [
        ('flip1', STAGED, [], False),
        ('flag1', PATCHED0, ['-DUE4SS_R3_MARSHAL_MODE=1'], True),
        ('fold0', PATCHED0, [], True),
    ]
    results = {}
    for name, src, extra, terminal_inc in variants:
        argv = list(argv0)
        argv[1:1] = prepends            # the fixed script's argv[1:1] insert
        for j, x in enumerate(extra):
            argv.insert(1 + j, x)
        if terminal_inc:
            # rebase-gate pattern: one TERMINAL -I to the staging dir so the
            # quoted SeedTable.h/SeedValidation.hpp resolve (names unique in
            # the include chain); the same armed files flip1 resolves adjacent.
            argv += ['-I', str(MY / 'staging/initializer-l10-r3')]
        # token indices are computed on the FINAL argv (post-prepend), exactly
        # like the fixed script's argv.index(...) calls
        src_idx = next(i for i, x in enumerate(argv) if x.endswith('UnrealInitializer.cpp'))
        oi = argv.index('-o') + 1
        mi = argv.index('-MF') + 1
        obj = GATE / f'{name}.o'
        dep = GATE / f'{name}.d'
        argv[src_idx] = str(src)
        argv[oi] = str(obj)
        argv[mi] = str(dep)
        env = dict(os.environ, LLVM_CONFIG=unit.get('LLVM_CONFIG', ''))
        p = subprocess.run([str(a) for a in argv], capture_output=True, text=True, env=env)
        (GATE / f'{name}.stderr').write_text(p.stderr[-20000:])
        h = sha(obj) if p.returncode == 0 and obj.is_file() else None
        results[name] = {'exit': p.returncode, 'sha256': h, 'argv_tokens': len(argv)}
        print(f'{name}: exit={p.returncode} obj_sha={h}')
        if p.returncode:
            print(p.stderr[-3000:])
            sys.exit(1)

    e1 = results['flip1']['sha256'] == results['flag1']['sha256']
    e2 = results['fold0']['sha256'].startswith('983b18e24d32')
    e3 = results['flip1']['sha256'].startswith(REBASE_FOLD1_SHA)
    print(f'E1 flip==flag: {e1}')
    print(f'E2 fold0==prebuild armed object 983b18e2...: {e2}')
    print(f'E3 flip1==rebase-fold1 {REBASE_FOLD1_SHA}...: {e3} (recorded only)')
    out = {'variants': results, 'E1_flip_equals_flag': e1,
           'E2_fold0_equals_prebuild_armed_object': e2,
           'E3_equals_rebase_fold1_object': e3,
           'E3_note': 'rebase fold-1 compiled against the DISARMED 3/4 SeedTable.h; '
                      'difference from flip1 is the armed seed constants, not fold mechanics',
           'attested_base_object': unit['object_sha256'],
           'flip1_sha256': results['flip1']['sha256']}
    (MY / 'receipts/fold-equivalence.json').write_text(json.dumps(out, indent=1) + '\n')
    if not (e1 and e2):
        sys.exit('FOLD EQUIVALENCE FAILED')


if __name__ == '__main__':
    main()
