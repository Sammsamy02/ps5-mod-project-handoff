#!/usr/bin/env python3
"""apply_r15.py -- integrator: put cap09 (#9 C++ mod ABI) + cap11 (#11 C++ mod windows) into the candidate tree on
top of R14. Dry run by default; --execute writes. Every check prints its count; any failure or a zero count exits 1
BEFORE anything is written (the cap09 anchored apply is the first write and itself refuses unless all anchors are
exact-once).

Order (cap09 APPLY.md + cap11 APPLY.md):
  1. pins: CHANGES.json rows for UE4SSProgram.cpp / LuaMod.cpp == live R14 shas; every staged file == its row sha;
     no target file exists yet (or already has identical bytes); cap09 apply_anchored --check anchors exact-once.
  2. backup CHANGES.json.
  3. cap09 apply_anchored.py --apply-to <typescan> (patches the 2 live files, writes NativeCppModPath.hpp + original).
  4. copy cap09 + cap11 added files (sha re-checked after copy), CppMod.cpp = the cap11 version.
  5. CHANGES.json: cap09 files_rows (CppMod.cpp sha -> cap11), refresh the 2 live rows, added_headers(+hashes) for
     every added file, cap09/cap11/r15 row-sync records.
  6. re-check every pin the way build_variants G2 does.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys

W = '/WORKSPACE/.codex/worktrees/translation-core/what'
T = W + '/outputs/2026-10-08-l26-runtime/typescan-r4'
D = '/WORKSPACE/.codex/worktrees/translation-core/caps-team-D/caps-team-work/cap09-cap11'
P9, P11 = D + '/cap09/PATCH', D + '/cap11/PATCH'
R14 = {'src/UE4SSProgram.cpp': 'ad3ae621a7c86d771f4ed56e54eddf3330b8db6a1525c5a73a8466a6c57078cf',
       'src/LuaMod.cpp': 'fd53a4f295390677e47288cbf80df40b623aa32be47a1b6d4546299b04d1599c'}
BACKUP = T + '/CHANGES.json.pre-cap09-cap11-r15-20261013'


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def src_of(row, team_root):
    f = row['from']
    return f if f.startswith('/') else os.path.join(D, f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--execute', action='store_true')
    a = ap.parse_args()
    bad = []
    n = 0
    c = json.load(open(T + '/CHANGES.json'))
    r9 = json.load(open(P9 + '/CHANGES-ROWS.json'))
    r11 = json.load(open(P11 + '/CHANGES-ROWS.json'))

    # 1a. live rows == R14
    for rel, want in R14.items():
        live = sha(T + '/candidate/' + rel)
        rows = [f for f in c['files'] if f.get('candidate', '').endswith('/candidate/' + rel)]
        n += 1
        if live != want or len(rows) != 1 or rows[0]['candidate_sha256'] != want:
            bad.append('live/row pin %s live=%s rows=%d' % (rel, live[:8], len(rows)))
    # 1b. staged sources == their declared sha, targets free (or identical)
    adds = [(x, P9) for x in r9['added_files']] + [(x, P11) for x in r11['added_files']]
    for row, root in adds:
        s = src_of(row, root)
        n += 1
        if not os.path.exists(s) or sha(s) != row['sha256']:
            bad.append('staged sha %s' % s)
        if os.path.exists(row['path']) and sha(row['path']) != row['sha256']:
            bad.append('target exists with other bytes %s' % row['path'])
    cpp11 = P11 + '/candidate/src/Mod/CppMod.cpp'
    want11 = r11['files_rows_replace'][0]['candidate_sha256']
    n += 1
    if sha(cpp11) != want11:
        bad.append('cap11 CppMod.cpp sha')
    # cap09 rows: originals must match their declared original sha
    for row in r9['files_rows']:
        n += 1
        if sha(row['original']) != row['original_sha256']:
            bad.append('original sha %s' % row['original'])
        if any(f.get('candidate') == row['candidate'] for f in c['files']):
            bad.append('CHANGES already has a row for %s' % row['candidate'])
    # 1c. anchors
    chk = subprocess.run([sys.executable, '-I', P9 + '/apply_anchored.py', '--check'], capture_output=True, text=True)
    out = chk.stdout + chk.stderr
    anchor_fail = [l for l in out.splitlines() if l.strip().startswith('FAIL') and 'staged copy == regenerated' not in l]
    n += 1
    if 'items_checked=25' not in out or anchor_fail:
        bad.append('anchors: ' + '; '.join(anchor_fail[:3]))
    if os.path.exists(BACKUP):
        bad.append('backup already exists ' + BACKUP)
    print('pre-checks: %d items checked, %d added files, %d problems' % (n, len(adds), len(bad)))
    for b in bad:
        print('  PROBLEM', b)
    if n == 0 or bad:
        return 1
    if not a.execute:
        print('DRY RUN: nothing written. Re-run with --execute.')
        return 0

    # 2. backup
    shutil.copy2(T + '/CHANGES.json', BACKUP)
    # 3. anchored apply on the live tree
    r = subprocess.run([sys.executable, '-I', P9 + '/apply_anchored.py', '--apply-to', T], capture_output=True, text=True)
    print(r.stdout[-1500:], r.stderr[-800:])
    if r.returncode != 0:
        print('FAIL: apply_anchored --apply-to rc=%d (CHANGES.json untouched; backup kept)' % r.returncode)
        return 1
    # 4. copies
    for row, root in adds:
        os.makedirs(os.path.dirname(row['path']), exist_ok=True)
        shutil.copy2(src_of(row, root), row['path'])
        if sha(row['path']) != row['sha256']:
            print('FAIL: copy sha', row['path']); return 1
    dst = T + '/candidate/src/Mod/CppMod.cpp'
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(cpp11, dst)
    if sha(dst) != want11:
        print('FAIL: CppMod.cpp copy sha'); return 1
    # 5. CHANGES.json rows
    for row in r9['files_rows']:
        row = {k: v for k, v in row.items() if k not in ('from', 'apply')}
        if not os.path.exists(row['copied_original']):
            os.makedirs(os.path.dirname(row['copied_original']), exist_ok=True)
            shutil.copy2(row['original'], row['copied_original'])
        if sha(row['copied_original']) != row['copied_original_sha256']:
            print('FAIL: copied_original sha', row['copied_original']); return 1
        if row['candidate'] == dst:
            row['candidate_sha256'] = want11
        if sha(row['candidate']) != row['candidate_sha256']:
            print('FAIL: new row candidate sha', row['candidate']); return 1
        c['files'].append(row)
    new_live = {}
    for rel in R14:
        p = T + '/candidate/' + rel
        new_live[rel] = sha(p)
        for f in c['files']:
            if f.get('candidate', '').endswith('/candidate/' + rel):
                f['candidate_sha256'] = new_live[rel]
    for row, _ in adds:
        if row['path'] not in c['added_headers']:
            c['added_headers'].append(row['path'])
        c['added_header_hashes'][row['path']] = row['sha256']
    c['cap09_row_sync'] = {'date': '2026-10-13', 'by': 'Claude (integrator, R15)', 'content': r9['scope_cap09']}
    c['cap11_row_sync'] = {'date': '2026-10-13', 'by': 'Claude (integrator, R15)', 'content': r11['scope_cap11']}
    c['r15_row_sync'] = {'date': '2026-10-13', 'by': 'Claude (integrator, R15)',
                         'content': 'R14 + cap09 (#9 C++ mod ABI v1) + cap11 (#11 native GUI host), Team D, one independent shipping review (REVIEW-SHIP PASS-WITH-FIXES, fixes F1-F7 folded in).',
                         'files': {rel: new_live[rel] for rel in R14} | {'src/Mod/CppMod.cpp': want11}}
    json.dump(c, open(T + '/CHANGES.json', 'w'), indent=2)
    # 6. G2-style re-check
    c2 = json.load(open(T + '/CHANGES.json'))
    m = 0
    for f in c2['files']:
        m += 1
        if sha(f['original']) != f['original_sha256'] or sha(f['candidate']) != f['candidate_sha256']:
            print('FAIL: G2 pin', f['candidate']); return 1
    for p, h in c2['added_header_hashes'].items():
        m += 1
        if sha(p) != h:
            print('FAIL: added header pin', p); return 1
    print('APPLIED: UE4SSProgram.cpp -> %s, LuaMod.cpp -> %s, CppMod.cpp -> %s; %d added files; post-check %d pins PASS'
          % (new_live['src/UE4SSProgram.cpp'][:8], new_live['src/LuaMod.cpp'][:8], want11[:8], len(adds), m))
    return 0 if m else 1


if __name__ == '__main__':
    sys.exit(main())
