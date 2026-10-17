#!/usr/bin/env python3
"""Apply B6 (staged in round9-staging/b6-direct) to the candidate tree. Refuses unless
every candidate file still has its B5 sha. Updates CHANGES.json candidate shas for the
4 files (rows exist since B2/B4/B5) and adds b6_row_sync. Exit 1 on any mismatch."""
import hashlib, json, shutil, sys
W = '/WORKSPACE/.codex/worktrees/translation-core/what'
R = W + '/outputs/2026-10-08-l26-runtime/typescan-r4'
S = W + '/outputs/2026-10-08-l25-prep/round9-staging/b6-direct'
FILES = {'src/LuaMod.cpp': 'afaf5caa3f09', 'src/UE4SSProgram.cpp': '4be1a757fa2d',
         'src/LuaMadeSimple/LuaMadeSimple.cpp': '418c19505c88', 'include/LuaMadeSimple/LuaMadeSimple.hpp': '03bf0031884f'}
sha = lambda p: hashlib.sha256(open(p, 'rb').read()).hexdigest()
for f, pre in FILES.items():
    if not sha(R + '/candidate/' + f).startswith(pre):
        print('FAIL: candidate %s is not the B5 version' % f); sys.exit(1)
c = json.load(open(R + '/CHANGES.json'))
shutil.copyfile(R + '/CHANGES.json', R + '/CHANGES.json.pre-B6-20261012')
new = {}
for f in FILES:
    shutil.copyfile(S + '/' + f, R + '/candidate/' + f)
    new[f] = sha(R + '/candidate/' + f)
    n = 0
    for lst in ('changes', 'files'):
        for r in c[lst]:
            if r['candidate'].endswith('/typescan-r4/candidate/' + f):
                r['candidate_sha256'] = new[f]; n += 1
    if n != 2:
        print('FAIL: expected 2 CHANGES rows for %s, found %d' % (f, n)); sys.exit(1)
c['b6_row_sync'] = {'date': '2026-10-12', 'by': 'Claude (B6 labelled build)',
    'content': '[R9-MEM] Lua states allocate from engine GMalloc (fail-closed to libc); [R9-EIGT] game-action flag RAII guard; '
               '[REFUSED] uniform refusal log + GetRefusedHookCount; stale get_function_ref C++-throw variant under the action lock; '
               '[B6-RATE] per-widget construct log rate limit. Diff: outputs/2026-10-08-l25-prep/round9-staging/b6-direct/final-b6.diff',
    'candidate_sha256': new}
json.dump(c, open(R + '/CHANGES.json', 'w'), indent=1)
for f, s in new.items(): print('applied %s %s' % (f, s[:12]))
