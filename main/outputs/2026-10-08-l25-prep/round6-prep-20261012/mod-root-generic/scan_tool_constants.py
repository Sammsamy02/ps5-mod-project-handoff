#!/usr/bin/env python3
"""scan_tool_constants.py - Townfall-only values in the Mac tools, the in-game
diagnostic Lua and the launch data of outputs/2026-10-08-l25-prep (NOT the PRX;
scan_game_constants.py covers the compiled runtime).

Mac-only, read-only, no network. Writes only the --tsv-out / --json-out paths.
Scans every *.py *.sh *.lua *.ini *.txt *.json under each --root, skipping the
--skip directory names (evidence trees, review packets, other round-6 tracks).
Patterns: TITLE (PPSA33286), TOWNFALL (any case), ROOTPATH
(/data/modbridge/roots/<id>), HOMEBREW (/data/homebrew/...).
Each hit file gets ONE class by path/type rules (first match wins):
  console-lua     Lua that is deployed into the game (not host harness/tests)
  tool            Mac deploy/launch/harvest tool (.py/.sh, not a test)
  test-fixture    host tests, fixtures, harness files
  launch-data     loadlists / ini / manifests written for one Townfall launch
  record          receipts, logs, verdicts, tables (historical; fine)
Prints how many files it checked; exits 1 when zero files were checked or
when any --root yields zero files (a scan of nothing is a failed scan).
"""
import argparse, json, os, re, sys

PATTERNS = [
    ('TITLE', re.compile(r'PPSA33286')),
    ('TOWNFALL', re.compile(r'(?i)townfall')),
    ('ROOTPATH', re.compile(r'/data/modbridge/roots/[A-Z]{4}[0-9]{5}')),
    ('HOMEBREW', re.compile(r'/data/homebrew/[^"\'\s]*')),
]
EXTS = ('.py', '.sh', '.lua', '.ini', '.txt', '.json')


def classify(rel):
    low = rel.lower()
    base = os.path.basename(low)
    testy = ('/fixtures/' in '/' + low or low.startswith('host-tests/') or low.startswith('host-lua-harness/')
             or '-tests/' in low or base.startswith('test_') or base.startswith('run_') and 'test' in low
             or '/tests/' in '/' + low)
    if low.endswith('.lua'):
        return 'test-fixture' if testy else 'console-lua'
    if low.endswith(('.py', '.sh')):
        return 'test-fixture' if testy else 'tool'
    if testy:
        return 'test-fixture'
    if low.endswith('.ini') or 'loadlist' in base or low.startswith('launch-manifests/') or '/upload/' in low:
        return 'launch-data'
    return 'record'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', action='append', required=True)
    ap.add_argument('--skip', action='append', default=[])
    ap.add_argument('--tsv-out'); ap.add_argument('--json-out')
    a = ap.parse_args()
    files_checked, per_root, hits = 0, {}, []
    for root in a.root:
        n = 0
        for dp, dns, fns in os.walk(root):
            rel_dir = os.path.relpath(dp, root)
            dns[:] = sorted(d for d in dns if d not in a.skip and os.path.join(rel_dir, d).lstrip('./') not in a.skip)
            for fn in sorted(fns):
                if not fn.endswith(EXTS):
                    continue
                p = os.path.join(dp, fn)
                rel = os.path.relpath(p, root)
                try:
                    text = open(p, 'rb').read().decode('utf-8', 'replace')
                except OSError:
                    continue
                n += 1
                for ln, line in enumerate(text.splitlines(), 1):
                    for pid, rx in PATTERNS:
                        for m in rx.finditer(line):
                            hits.append({'root': root, 'file': rel, 'line': ln, 'pattern': pid,
                                         'token': m.group(0)[:100], 'class': classify(rel)})
        per_root[root] = n
        files_checked += n
    by_class, files_by_class = {}, {}
    for h in hits:
        by_class[h['class']] = by_class.get(h['class'], 0) + 1
        files_by_class.setdefault(h['class'], {}).setdefault(h['file'], 0)
        files_by_class[h['class']][h['file']] += 1
    print(f'TOOL-CONSTANT SCAN: roots {len(a.root)}; checked {files_checked} files; {len(hits)} hit(s); '
          f'{len({h["file"] for h in hits})} file(s) with hits')
    for r, n in per_root.items():
        print(f'TOOL-CONSTANT SCAN: root {r}: {n} file(s)')
    for c in ('console-lua', 'tool', 'launch-data', 'test-fixture', 'record'):
        fs = files_by_class.get(c, {})
        print(f'TOOL-CONSTANT SCAN: class {c}: {len(fs)} file(s), {by_class.get(c, 0)} hit(s)')
        if c in ('console-lua', 'tool'):
            for f, k in sorted(fs.items()):
                lines = sorted({h['line'] for h in hits if h['file'] == f})
                print(f'   {k:4d}  {f}  lines {",".join(map(str, lines[:12]))}{" ..." if len(lines) > 12 else ""}')
    if a.tsv_out:
        with open(a.tsv_out, 'w') as f:
            f.write('class\tfile\tline\tpattern\ttoken\n')
            for h in hits:
                f.write(f"{h['class']}\t{h['file']}\t{h['line']}\t{h['pattern']}\t{h['token']}\n")
    if a.json_out:
        with open(a.json_out, 'w') as f:
            json.dump({'schema': 'tool-constant-scan/1', 'console_contact': False, 'roots': per_root,
                       'skip': a.skip, 'checked_files': files_checked, 'hits_by_class': by_class,
                       'files_by_class': files_by_class}, f, indent=1)
            f.write('\n')
    zero = [r for r, n in per_root.items() if n == 0]
    if files_checked == 0 or zero:
        print(f'TOOL-CONSTANT SCAN: FAIL - zero files checked (roots with zero: {zero})')
        return 1
    print('TOOL-CONSTANT SCAN: DONE (class is a path rule; see DESIGN.md list E for the verdicts)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
