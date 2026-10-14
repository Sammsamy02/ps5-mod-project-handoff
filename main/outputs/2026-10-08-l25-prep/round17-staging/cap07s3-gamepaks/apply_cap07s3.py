#!/usr/bin/env python3
"""Integrator: apply the reviewed #7 step 2 game-paks source patch to the live runtime tree.

Refuses unless every live file equals its staged base (BASE-SHA256SUMS). Backs up each live file next to
typescan-r4 as <name>.pre-cap07s3-20261014, copies new/ over it, re-pins CHANGES.json (files[] candidate_sha256 for
the two .cpp files, added_header_hashes for the three headers, plus the r17b_cap07s3_row_sync note) and adds the
R17b marker set to harvest/add_build_markers.py. CHANGES.json round-trips byte-exactly with json.dumps(indent=2).
"""
import hashlib, json, shutil, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
OUT = HERE.parents[2]                      # .../outputs
T = OUT / '2026-10-08-l26-runtime/typescan-r4'
MARK = OUT / '2026-10-08-l25-prep/harvest/add_build_markers.py'
TAG = '.pre-cap07s3-20261014'


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    base = {}
    for line in (HERE / 'BASE-SHA256SUMS').read_text().splitlines():
        s, rel = line.split(None, 1)
        base[rel[2:] if rel.startswith('./') else rel] = s
    if len(base) != 5:
        sys.exit('FAIL: expected 5 base rows, got %d' % len(base))
    for rel, s in base.items():
        if sha(T / rel) != s:
            sys.exit('FAIL: live file drifted from the staged base: ' + rel)
        if sha(HERE / 'base' / rel) != s:
            sys.exit('FAIL: staged base copy drifted: ' + rel)
    ch = T / 'CHANGES.json'
    text = ch.read_text()
    c = json.loads(text)
    if json.dumps(c, indent=2) != text:
        sys.exit('FAIL: CHANGES.json does not round-trip; refusing to rewrite it')
    if 'r17b_cap07s3_row_sync' in c:
        sys.exit('FAIL: already applied')
    new = {rel: sha(HERE / 'new' / rel) for rel in base}
    for rel in base:
        bak = T / (Path(rel).name + TAG)
        if bak.exists():
            sys.exit('FAIL: backup exists: ' + str(bak))
    for rel in base:
        shutil.copy2(T / rel, T / (Path(rel).name + TAG))
        shutil.copyfile(HERE / 'new' / rel, T / rel)
        if sha(T / rel) != new[rel]:
            sys.exit('FAIL: copy check ' + rel)
    n_files = 0
    for r in c['files']:
        for rel in ('candidate/src/LuaMod.cpp', 'candidate/src/UnrealInitializer.cpp'):
            if r.get('candidate') == str(T / rel):
                if r['candidate_sha256'] != base[rel]:
                    sys.exit('FAIL: CHANGES files[] pin differs from base for ' + rel)
                r['candidate_sha256'] = new[rel]
                n_files += 1
    n_hdr = 0
    for rel in ('include/PakRequestProducer.hpp', 'include/NativePakMountAdapter.inl', 'include/NativePakMountEntry.hpp'):
        k = str(T / rel)
        if c['added_header_hashes'].get(k) != base[rel]:
            sys.exit('FAIL: CHANGES added_header_hashes differs from base for ' + rel)
        c['added_header_hashes'][k] = new[rel]
        n_hdr += 1
    if n_files < 2 or n_hdr != 3:
        sys.exit('FAIL: pin rows updated files=%d headers=%d' % (n_files, n_hdr))
    c['r17b_cap07s3_row_sync'] = {
        'date': '2026-10-14',
        'by': 'Claude (integrator, R17b)',
        'content': ('Console-proven launches K/L: the configured <root>/pakmods is not visible from the game process after '
                    'module_start (stat ENOENT) and this engine build does not auto-mount Paks/~mods. Fix: a second pak-mod '
                    'source, the game\'s own <Project>/Content/Paks/~mods, derived from the configured game directory with '
                    'the generic UE package shape (the one non-Engine child holding Content; ASCII case-insensitive; zero or '
                    'several = miss; symlinks never followed; error_code calls only; every miss logs path + errno). The '
                    'configured pakmods stays first when visible. RunNativePakMountOnce gains the game-directory argument. '
                    'Host 17/17. One independent review (round17-staging/cap07s3-gamepaks/REVIEW.md). Shas: ' +
                    ', '.join('%s %s -> %s' % (Path(r).name, base[r][:8], new[r][:8]) for r in sorted(base)) +
                    '. Backups typescan-r4/<name>' + TAG + '.'),
    }
    ch.write_text(json.dumps(c, indent=2))
    m = MARK.read_text()
    if "'R17b': [" not in m:
        block = ''.join(l + '\n' for l in (HERE / 'MARKERS.txt').read_text().splitlines() if not l.startswith('#'))
        anchor = "    'R11': ["
        if m.count(anchor) != 1:
            sys.exit('FAIL: marker anchor')
        MARK.write_text(m.replace(anchor, block + anchor))
    print('APPLY-CAP07S3 PASS files=%d headers=%d' % (n_files, n_hdr))
    for rel in sorted(base):
        print('  %s %s -> %s' % (rel, base[rel][:12], new[rel][:12]))


if __name__ == '__main__':
    main()
