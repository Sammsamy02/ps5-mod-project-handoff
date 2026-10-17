#!/usr/bin/env python3
"""K2 CHANGES.json row sync (Team I2). For the INTEGRATOR, after `patch -p1 < k2.patch` in typescan-r4.

usage: python3 -I apply_changes_rows.py <typescan-r4/CHANGES.json> [--dry-run]

Asserts every old value first (base pins, the two new files present with the NEW-SHA256SUMS hashes), then:
  files[] UE4SSProgram.cpp / LuaMod.cpp candidate_sha256 -> new; added_headers / added_header_hashes += the two new
  files; new key k2_kbm_row_sync. Prints what it changed and how many rows it checked; exits 1 on any mismatch,
  2 if it checked nothing. Writes nothing on --dry-run or on any failure.
"""
import hashlib, json, sys
from pathlib import Path

K2 = Path(__file__).resolve().parent
BASE = {'candidate/src/UE4SSProgram.cpp': 'b6a68e3f945b76f8ffef7caa5652b299022dd775b82f6a17823c8b288e4a451a',
        'candidate/src/LuaMod.cpp': '5f09ec1ee024cf7d8cc60ceac860bdf11711af415bd6a9517e0da49739eeb33a'}
NEW_FILES = ('include/Platform/KbmEngineKeys.hpp', 'candidate/src/KbmEngineKeysRuntime.inc')


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main(argv):
    if not argv or argv[0].startswith('-'):
        print(__doc__); return 2
    changes = Path(argv[0]).resolve()
    dry = '--dry-run' in argv[1:]
    tree = changes.parent  # typescan-r4
    new = {}
    for line in (K2 / 'NEW-SHA256SUMS').read_text().splitlines():
        h, p = line.split(None, 1)
        new[p.strip().lstrip('./')] = h
    errors, checked = [], 0
    for rel, h in new.items():
        checked += 1
        if not (tree / rel).is_file() or sha(tree / rel) != h: errors.append('tree file %s does not match NEW-SHA256SUMS (apply k2.patch first)' % rel)
    d = json.loads(changes.read_text())
    rows = {}
    for r in d['files']:
        for rel in BASE:
            if Path(r['candidate']).resolve() == (tree / rel).resolve(): rows.setdefault(rel, []).append(r)
    for rel, old in BASE.items():
        checked += 1
        if len(rows.get(rel, [])) != 1: errors.append('expected exactly one files[] row for %s, found %d' % (rel, len(rows.get(rel, []))))
        elif rows[rel][0]['candidate_sha256'] != old: errors.append('%s row candidate_sha256 is %s, expected base %s' % (rel, rows[rel][0]['candidate_sha256'], old))
    added = [str(tree / rel) for rel in NEW_FILES]
    for a in added:
        checked += 1
        if a in d['added_headers'] or a in d['added_header_hashes']: errors.append('already listed as an added header: ' + a)
    if 'k2_kbm_row_sync' in d: errors.append('k2_kbm_row_sync already present')
    if errors:
        for e in errors: print('ERROR ' + e)
        print('apply_changes_rows: FAIL (checked %d, nothing written)' % checked); return 1
    if checked == 0:
        print('apply_changes_rows: FAIL (nothing checked)'); return 2
    for rel in BASE:
        rows[rel][0]['candidate_sha256'] = new[rel]
        print('files[] %s: %s -> %s' % (rel, BASE[rel][:8], new[rel][:8]))
    for rel, a in zip(NEW_FILES, added):
        d['added_headers'].append(a)
        d['added_header_hashes'][a] = new[rel]
        print('added header %s %s' % (rel, new[rel][:8]))
    d['k2_kbm_row_sync'] = {
        'date': '2026-10-14',
        'by': 'Team I2 (caps-team-I/keyboard-mouse/k2), applied by the integrator',
        'content': ('[K2] keyboard and mouse via the engine\'s own key state: Merged{PS5Pad.ini source when the composition made one, '
                    'engine keys} composed in UE4SSProgram::validate_native_services; the game-thread sampler runs from the R3 marshal '
                    'drain and the VMT Tick drain (LuaMod.cpp, after NativeModsGameThreadFrame(0)/(1)) at most every 8 ms, only while keys are bound, and asks the local PlayerController\'s '
                    'reflected IsInputKeyDown(FKey) only about bound keys (modifiers only on an edge frame); layout checked by '
                    'reflection (one by-value Key struct with FName KeyName, bool return, reflected offsets / ParmsSize only), '
                    'find-only FName lookup with miss cache, controller cache rebuilt on miss with a generation counter; any failed '
                    'check = engine-keys off for good, pad chords unchanged; teardown handler shutdown -> KbmRetire -> unload. '
                    'Engine-free core include/Platform/KbmEngineKeys.hpp, Unreal binding candidate/src/KbmEngineKeysRuntime.inc '
                    '(included once by UE4SSProgram.cpp, no new unit). No new import (33 new undefined symbols of program.o all '
                    'defined in runtime-l26-A.o), no detour, no fs call, no new engine field.'),
        'files': {rel: new[rel] for rel in list(BASE) + list(NEW_FILES)},
        'backups': '.pre-k2-20261014',
    }
    if dry:
        print('apply_changes_rows: DRY RUN OK (checked %d, nothing written)' % checked); return 0
    changes.write_text(json.dumps(d, indent=2))  # the file's own style: indent 2, ASCII escapes, no final newline
    print('apply_changes_rows: PASS (checked %d, wrote %s)' % (checked, changes))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
