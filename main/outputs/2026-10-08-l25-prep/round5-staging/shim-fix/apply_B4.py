#!/usr/bin/env python3
"""Apply labelled build B4 (shim write-area fix + mod-folder setup) to the candidate
tree. Run ONLY after B3 finished. Exits 1 (writing nothing) on any base mismatch.

  - candidate/src/LuaPathShim.cpp  <- IQ's fixed TU (base must be the crashed-build copy 821d74a2...)
  - candidate/src/UE4SSProgram.cpp <- IP shim-root patch (absolute native mods dir)
  - CHANGES.json: UE4SSProgram.cpp candidate sha (both lists). LuaPathShim.cpp is a
    NEW_UNITS source (not a CHANGES row); the snapshot pins it.
"""
import hashlib, json, os, shutil, subprocess, sys

W = '/WORKSPACE/.codex/worktrees/translation-core/what'
R = W + '/outputs/2026-10-08-l26-runtime/typescan-r4'
S = W + '/outputs/2026-10-08-l25-prep/round5-staging'


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def main():
    shim = R + '/candidate/src/LuaPathShim.cpp'
    prog = R + '/candidate/src/UE4SSProgram.cpp'
    if not sha(shim).startswith('821d74a2'):
        print('FAIL: candidate LuaPathShim.cpp is not the staged base (821d74a2...): %s' % sha(shim)[:12]); return 1
    import re
    unpatched = re.search(r'\b[0-9a-f]{64}\b', open(S + '/IP-shim-root/unpatched-sha256.txt').read()).group(0)  # first sha = the unpatched candidate
    if sha(prog) != unpatched:
        print('FAIL: candidate UE4SSProgram.cpp differs from the IP patch base'); return 1
    dry = subprocess.run(['patch', '-p1', '--dry-run', '-s', '-f', '-d', R + '/candidate', '-i', S + '/IP-shim-root/01-ue4ssprogram-shim-root.patch'])
    if dry.returncode != 0:
        print('FAIL: IP patch does not apply cleanly'); return 1
    shutil.copyfile(shim, S + '/shim-fix/LuaPathShim.cpp.candidate-before-B4')
    shutil.copyfile(prog, S + '/IP-shim-root/UE4SSProgram.cpp.candidate-before-B4')
    shutil.copyfile(S + '/shim-fix/LuaPathShim.cpp', shim)
    r = subprocess.run(['patch', '-p1', '-s', '-f', '--no-backup-if-mismatch', '-d', R + '/candidate', '-i', S + '/IP-shim-root/01-ue4ssprogram-shim-root.patch'])
    if r.returncode != 0:
        shutil.copyfile(S + '/shim-fix/LuaPathShim.cpp.candidate-before-B4', shim)
        print('FAIL: IP patch apply failed; shim restored'); return 1
    changes = json.load(open(R + '/CHANGES.json'))
    shutil.copyfile(R + '/CHANGES.json', R + '/CHANGES.json.pre-B4-20261012')
    n = 0
    for lst in ('changes', 'files'):
        for row in changes[lst]:
            if row['candidate'].endswith('/typescan-r4/candidate/src/UE4SSProgram.cpp'):
                row['candidate_sha256'] = sha(prog); n += 1
    if n != 2:
        print('FAIL: expected 2 UE4SSProgram rows, found %d' % n); return 1
    changes['b4_shim_row_sync'] = {
        'date': '2026-10-12', 'by': 'Claude overnight run (B4 labelled build)',
        'content': 'IQ shim write-area fix (install() does no fs work; lazy one-shot checked mkdir on first write; spill fallback; '
                   'registry-sourced context; absolute os table index — the pre-fix relative -1 index made lua_setfield index '
                   'the wrapper closure, an unprotected Lua error = PANIC/abort, matching both crash logs) + IP shim root '
                   '(absolute native mods dir from the runtime-carried root).',
        'luapathshim_sha256': sha(shim), 'ue4ssprogram_candidate_sha256': sha(prog)}
    json.dump(changes, open(R + '/CHANGES.json', 'w'), indent=1)
    print('B4 applied: LuaPathShim.cpp %s, UE4SSProgram.cpp %s' % (sha(shim)[:12], sha(prog)[:12]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
