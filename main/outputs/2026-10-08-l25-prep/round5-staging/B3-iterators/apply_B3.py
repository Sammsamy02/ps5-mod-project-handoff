#!/usr/bin/env python3
"""Apply labelled build B3 (ForEachUObject + IterateGameDirectories + UEnum.ForEachName
logging) to the candidate tree. Run ONLY after B2b finished (it checks the base).

  - candidate/src/LuaMod.cpp            <- staged B3 LuaMod.cpp (base must be B2's)
  - candidate/src/LuaType/LuaUEnum.cpp  <- staged override (+ original copy)
  - CHANGES.json rows (both lists): LuaMod candidate sha, + new LuaUEnum row
Prints what it did; exits 1 on any mismatch (nothing is written in that case)."""
import hashlib, json, os, shutil, sys

W = '/WORKSPACE/.codex/worktrees/translation-core/what'
R = W + '/outputs/2026-10-08-l26-runtime/typescan-r4'
S = W + '/outputs/2026-10-08-l25-prep/round5-staging'
PORT_UENUM = W + '/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/LuaType/LuaUEnum.cpp'


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def main():
    cand_luamod = R + '/candidate/src/LuaMod.cpp'
    b2_luamod = S + '/B2-error-wrapper/ue4ss/src/LuaMod.cpp'
    if sha(cand_luamod) != sha(b2_luamod):
        print('FAIL: candidate LuaMod.cpp is not the B2 version; refusing')
        return 1
    if not sha(PORT_UENUM).startswith('4b4949d9799c'):
        print('FAIL: port LuaUEnum.cpp changed since staging; refusing')
        return 1
    changes = json.load(open(R + '/CHANGES.json'))
    if any(r['original'] == PORT_UENUM for r in changes['files']):
        print('FAIL: LuaUEnum row already present')
        return 1
    shutil.copyfile(S + '/B3-iterators/ue4ss/src/LuaMod.cpp', cand_luamod)
    os.makedirs(R + '/candidate/src/LuaType', exist_ok=True)
    os.makedirs(R + '/original/src/LuaType', exist_ok=True)
    cand_uenum = R + '/candidate/src/LuaType/LuaUEnum.cpp'
    orig_uenum = R + '/original/src/LuaType/LuaUEnum.cpp'
    shutil.copyfile(S + '/B3-iterators/ue4ss/src/LuaType/LuaUEnum.cpp', cand_uenum)
    shutil.copyfile(PORT_UENUM, orig_uenum)
    shutil.copyfile(R + '/CHANGES.json', R + '/CHANGES.json.pre-B3-20261012')
    row = {'original': PORT_UENUM, 'copied_original': orig_uenum, 'candidate': cand_uenum,
           'original_sha256': sha(PORT_UENUM), 'candidate_sha256': sha(cand_uenum), 'copied_original_sha256': sha(orig_uenum)}
    for lst in ('changes', 'files'):
        changes[lst].append(dict(row))
        for r in changes[lst]:
            if r['candidate'].endswith('/typescan-r4/candidate/src/LuaMod.cpp'):
                r['candidate_sha256'] = sha(cand_luamod)
    changes['b3_iterators_row_sync'] = {
        'date': '2026-10-12', 'by': 'Claude overnight run (B3 labelled build)',
        'content': 'ForEachUObject: per-iteration stack restore (upstream leak fix), checkstack guard, bounded per-object skip; '
                   'callback return value IGNORED (upstream parity; staged break-on-true dropped). '
                   'IterateGameDirectories: PS5 flat package layout (app root + the one child dir with Content, case-insensitive, '
                   'error_code calls only), result shape identical to upstream. UEnum.ForEachName: bounded first-3 [UEN] diagnostics.',
        'luamod_candidate_sha256': sha(cand_luamod), 'luauenum_candidate_sha256': sha(cand_uenum)}
    json.dump(changes, open(R + '/CHANGES.json', 'w'), indent=1)
    print('B3 applied: LuaMod.cpp %s, LuaUEnum.cpp %s (new override row in both lists)' % (sha(cand_luamod)[:12], sha(cand_uenum)[:12]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
