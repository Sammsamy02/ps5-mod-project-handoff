#!/usr/bin/env python3
"""scan_game_constants.py - inventory of game-specific constants in a compiled file set.

Mac-only, read-only. Never contacts the console. Writes only the report paths
given on the command line.

Inputs (any mix, each may repeat):
  --deps-json F     JSON list/dict; every {"path": ...} entry is one compiled file
                    (e.g. l26-A/ACTUAL-DEPENDENCIES.json, SOURCE-SELECTION.json)
  --dfile-glob G    make-style .d dependency files (e.g. package-A/pass-*/build/*.d)
  --list F          newline-separated path list (e.g. compiled-files-1e49188b.txt)
  --manifest F      tree-snapshot manifest (files[].path relative to outputs/)
  --file F          one extra file (e.g. a generator input that is not compiled)
  --selection-dfiles F  SOURCE-SELECTION.json: parse every receipt_pins .d file in
                    it (the dependency lists of prebuilt objects linked into the
                    runtime, e.g. modforge_activation.o)

Every path is resolved, de-duplicated and scanned once. The scan prints how many
files it checked and EXITS 1 WHEN THAT COUNT IS ZERO, or when ANY single input
yields zero paths (a scan of nothing is a failed scan, CLAUDE.md section 4). Missing files are counted and listed.

Patterns (ids used in the report):
  TITLE_ID     PS title-id shape [A-Z]{4}[0-9]{5} (any prefix)
  TOWNFALL     "townfall" (any case)
  STELLAR      "stellar" (any case)
  APP0         a /app0 path literal
  MODROOT      /data/modbridge/roots
  HOMEBREW     /data/homebrew (install-specific dump folder)
  IMAGE_SHA    the Townfall eboot image digest 3b6cfad4... (full or 8+ hex prefix)
  IMAGE_SIZE   the Townfall image byte size 194241424 / extent 0xbc602c8
  SELF_NAME    <Name>-PS5-Shipping / -Test / -Development executable names

Output: --json-out (full hit list) and --tsv-out (file, line, pattern, token).
Context text is printed ONLY for files under the project's own trees
(outputs/ and .superpowers/), never for third-party or SDK files, and is
clipped to 120 characters.
"""
import argparse
import glob
import json
import os
import re
import sys

WHAT = '/WORKSPACE/.codex/worktrees/translation-core/what'

PATTERNS = [
    ('TITLE_ID', re.compile(r'(?<![A-Za-z0-9])[A-Z]{4}[0-9]{5}(?![0-9])')),
    ('TOWNFALL', re.compile(r'(?i)townfall')),
    ('STELLAR', re.compile(r'(?i)stellar')),
    ('APP0', re.compile(r'/app0\b[^\s"\')]*')),
    ('MODROOT', re.compile(r'/data/modbridge/roots')),
    ('HOMEBREW', re.compile(r'/data/homebrew')),
    ('IMAGE_SHA', re.compile(r'3b6cfad4[0-9a-f]*')),
    ('IMAGE_SIZE', re.compile(r'(?<![0-9])194241424(?![0-9])|0xbc602c8')),
    ('SELF_NAME', re.compile(r'[A-Za-z0-9_]+-PS5-(?:Shipping|Test|Development)')),
]

THIRD_PARTY_MARKERS = ('/deps/third/', '/LuaRaw/', 'ps5-payload-sdk', '/zydis', '/Zydis',
                       '/glaze/', '/fmt/', '/polyhook2/zydis', '/asmjit', '/asmtk')


def own_tree(path):
    rel = path[len(WHAT) + 1:] if path.startswith(WHAT + '/') else path
    if not (rel.startswith('outputs/') or rel.startswith('.superpowers/')):
        return False
    return not any(m in path for m in THIRD_PARTY_MARKERS)


def paths_from_json(spec):
    out = []
    with open(spec, errors='replace') as f:
        data = json.load(f)

    def walk(x):
        if isinstance(x, dict):
            p = x.get('path')
            if isinstance(p, str):
                out.append(p)
            for v in x.values():
                walk(v)
        elif isinstance(x, list):
            for v in x:
                walk(v)
    walk(data)
    return out


def paths_from_dfile(dfile):
    with open(dfile, errors='replace') as f:
        text = f.read().replace('\\\n', ' ')
    if ':' not in text:
        return []
    return text.split(':', 1)[1].split()


def paths_from_manifest(spec):
    with open(spec) as f:
        m = json.load(f)
    return [os.path.join(WHAT, 'outputs', r['path']) for r in m.get('files', [])]


def norm(p):
    if not os.path.isabs(p):
        p = os.path.join(WHAT, p)
    return os.path.realpath(p)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--deps-json', action='append', default=[])
    ap.add_argument('--dfile-glob', action='append', default=[])
    ap.add_argument('--list', action='append', default=[])
    ap.add_argument('--manifest', action='append', default=[])
    ap.add_argument('--file', action='append', default=[])
    ap.add_argument('--selection-dfiles', action='append', default=[])
    ap.add_argument('--json-out')
    ap.add_argument('--tsv-out')
    a = ap.parse_args()

    raw = []
    sources = {}
    for s in a.deps_json:
        got = paths_from_json(s); raw += got; sources[s] = len(got)
    for g in a.dfile_glob:
        files = sorted(glob.glob(g)); n = 0
        for d in files:
            got = paths_from_dfile(d); raw += got; n += len(got)
        sources[g] = n
        if not files:
            print(f'GAME-CONSTANT SCAN: ERROR glob matched no .d files: {g}')
            return 2
    for s in a.list:
        with open(s) as f:
            got = [ln.strip() for ln in f if ln.strip()]
        raw += got; sources[s] = len(got)
    for s in a.manifest:
        got = paths_from_manifest(s); raw += got; sources[s] = len(got)
    for s in a.file:
        raw.append(s); sources[s] = 1
    for s in a.selection_dfiles:
        with open(s) as f:
            sel = json.load(f)
        dfiles = sorted({r['path'] for r in sel.get('receipt_pins', []) if r['path'].endswith('.d')})
        n = 0
        for d in dfiles:
            if os.path.isfile(d):
                got = paths_from_dfile(d); raw += got; n += len(got)
        sources[s + ' (receipt .d files: %d)' % len(dfiles)] = n
        if not dfiles:
            print(f'GAME-CONSTANT SCAN: ERROR no receipt .d files in {s}')
            return 2

    # Per-input zero check (review m10): an input that yields no paths is a
    # failed input, even when the other inputs still give a non-zero total.
    zero_inputs = [k for k, v in sources.items() if v == 0]
    for k in zero_inputs:
        print(f'GAME-CONSTANT SCAN: ERROR input yielded zero paths: {k}')
    if zero_inputs:
        print('GAME-CONSTANT SCAN: FAIL - %d input(s) yielded zero paths' % len(zero_inputs))
        return 1

    files = sorted({norm(p) for p in raw})
    missing = [p for p in files if not os.path.isfile(p)]
    present = [p for p in files if os.path.isfile(p)]
    hits = []
    lines_checked = 0
    for p in present:
        try:
            with open(p, 'rb') as f:
                data = f.read()
        except OSError:
            missing.append(p)
            continue
        text = data.decode('utf-8', errors='replace')
        mine = own_tree(p)
        for ln_no, line in enumerate(text.splitlines(), 1):
            lines_checked += 1
            for pid, rx in PATTERNS:
                for m in rx.finditer(line):
                    tok = m.group(0)
                    if pid == 'TITLE_ID' and not re.match(r'^(PPSA|CUSA|PCAS|PCJS|PCKS|PLAS|PLJM|PLJS|ELAS|ELJM|ECAS|NPXS|FAKE)', tok):
                        pid_eff = 'TITLE_ID_SHAPE_OTHER'
                    else:
                        pid_eff = pid
                    rel = p[len(WHAT) + 1:] if p.startswith(WHAT + '/') else p
                    row = {'file': rel, 'line': ln_no, 'pattern': pid_eff, 'token': tok[:120],
                           'own_tree': mine}
                    if mine:
                        row['context'] = line.strip()[:120]
                    hits.append(row)

    by_pat = {}
    by_file = {}
    for h in hits:
        by_pat[h['pattern']] = by_pat.get(h['pattern'], 0) + 1
        by_file[h['file']] = by_file.get(h['file'], 0) + 1

    print(f'GAME-CONSTANT SCAN: inputs {len(sources)}; listed paths {len(raw)}; '
          f'unique files {len(files)}; checked {len(present)} files ({lines_checked} lines); '
          f'missing {len(missing)}')
    for k in sorted(by_pat):
        print(f'GAME-CONSTANT SCAN: pattern {k}: {by_pat[k]} hit(s)')
    print(f'GAME-CONSTANT SCAN: {len(by_file)} file(s) with hits')
    for f_, n in sorted(by_file.items(), key=lambda kv: (-kv[1], kv[0])):
        print(f'   {n:5d}  {f_}')

    if a.json_out:
        with open(a.json_out, 'w') as f:
            json.dump({'schema': 'game-constant-scan/1', 'console_contact': False,
                       'inputs': sources, 'unique_files': len(files),
                       'checked_files': len(present), 'checked_lines': lines_checked,
                       'missing_files': missing, 'hits_by_pattern': by_pat,
                       'hits_by_file': by_file, 'hits': hits}, f, indent=1)
            f.write('\n')
    if a.tsv_out:
        with open(a.tsv_out, 'w') as f:
            f.write('file\tline\tpattern\ttoken\n')
            for h in hits:
                f.write(f"{h['file']}\t{h['line']}\t{h['pattern']}\t{h['token']}\n")

    if len(present) == 0:
        print('GAME-CONSTANT SCAN: FAIL - zero files checked')
        return 1
    print('GAME-CONSTANT SCAN: DONE (inventory; classification is manual, see DESIGN.md)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
