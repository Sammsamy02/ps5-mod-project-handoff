#!/usr/bin/env python3
"""Claude round-10 item 3: static gate — fail the build if the runtime
uses any THROWING std::filesystem overload outside the approved list.
Host tests pass on macOS but the PS5 filesystem behaves differently.

2026-10-12 rewrite (Claude reviewer, gate law): the original computed its
source root one directory too short (<worktree>/outputs/outputs/...), globbed
ZERO files and printed PASS. Every gate now prints how many files and call
sites it checked and FAILS (exit 2) when it checked nothing.

What is checked:
  - every C/C++ source/header under each --root (default: the candidate tree)
    and every C/C++ path listed in each --files-from file (a build
    SOURCE-SELECTION.json, or a plain newline-separated list);
  - code parts only (comments and string contents stripped) on PS5-live
    lines (regions definitively not compiled on PS5 — '#ifndef __PROSPERO__',
    '#else' of '#ifdef __PROSPERO__', '#ifdef _WIN32' — are skipped and
    counted; unevaluable conditions count as live), via no_detour_gate.lex_cxx;
  - qualified calls through 'std::filesystem::', the conventional 'fs::'
    alias, and any 'namespace X = std::filesystem;' alias declared in the file;
  - calls spanning several lines (the argument list is paren-matched).

A call is accepted when its LAST argument is an error_code variable
(heuristic: name like ec / *_ec / err / error / errc, or contains
'error_code'). Otherwise it is a violation unless its line carries
'// [FS-THROW-APPROVED] <reason>' (counted and printed, never silent).
A 'using namespace std::filesystem' / 'using std::filesystem::<fn>' in live
code is itself a violation: unqualified calls cannot be verified.
Known false-positive class (fail-closed, approve after review): exists()/
is_directory()/is_regular_file() called on a file_status value are noexcept.

Baseline ratchet (2026-10-12): the first real scan of the 1e49188b build found
186 pre-existing throwing call sites (35 in the candidate tree). They are
listed in a baseline file (--baseline); known sites are printed as a count on
every run and do NOT fail the gate, but any NEW or CHANGED throwing call
fails it. Sites that disappear are reported as resolved (shrink the baseline
with --write-baseline). The baseline is a tracked debt list, not an approval.

Usage:
    python3 fs_throw_gate.py                                # candidate tree
    python3 fs_throw_gate.py --files-from <SOURCE-SELECTION.json> [--root DIR]...
                             [--baseline FILE] [--write-baseline FILE]
    python3 fs_throw_gate.py --selftest
Exit codes: 0 = PASS (and >0 files checked), 1 = VIOLATIONS, 2 = usage/env
error or ZERO files checked.
"""
import json
import os
import re
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from no_detour_gate import lex_cxx, src_files  # noqa: E402  (same directory)

# <worktree>/outputs/2026-10-08-l25-prep/harvest/ -> four dirnames = worktree root.
WHAT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DEFAULT_SRC = os.path.join(WHAT, 'outputs', '2026-10-08-l26-runtime', 'typescan-r4', 'candidate')
APPROVED_MARK = '// [FS-THROW-APPROVED]'
CXX_EXT = ('.cpp', '.cc', '.cxx', '.c', '.hpp', '.h', '.hh', '.inl', '.inc', '.ipp')

# Throwing free functions (each has an error_code overload). The first line is
# the original list; the rest completes the <filesystem> set.
BANNED = (
    'weakly_canonical', 'canonical', 'absolute', 'current_path', 'create_directory',
    'create_directories', 'remove', 'rename', 'copy', 'file_size', 'last_write_time',
    'exists', 'is_directory', 'is_regular_file',
    'remove_all', 'copy_file', 'copy_symlink', 'create_symlink', 'create_directory_symlink',
    'create_hard_link', 'read_symlink', 'resize_file', 'permissions', 'relative', 'proximate',
    'temp_directory_path', 'hard_link_count', 'status', 'symlink_status', 'is_symlink',
    'is_empty', 'equivalent', 'space', 'is_other', 'is_block_file', 'is_character_file',
    'is_fifo', 'is_socket',
)
ITERATORS = ('recursive_directory_iterator', 'directory_iterator')
ALIAS_DECL = re.compile(r'\bnamespace\s+(\w+)\s*=\s*(?:::)?\s*std\s*::\s*filesystem\s*;')
USING_DIRECTIVE = re.compile(r'\busing\s+namespace\s+(?:::)?\s*std\s*::\s*filesystem\b'
                             r'|\busing\s+(?:::)?\s*std\s*::\s*filesystem\s*::\s*\w+\s*;')
EC_LAST_ARG = re.compile(r'(?i)(?:^|[^\w])(?:\w*_ec|ec\d*|ec_\w*|\w*_err|err\d*|error|errc|\w*error_code\w*)\s*$')


def build_patterns(aliases):
    names = ['std\\s*::\\s*filesystem'] + [re.escape(a) for a in sorted(aliases)]
    qual = r'(?<![\w:])(?:::\s*)?(?:' + '|'.join(names) + r')\s*::\s*'
    call = re.compile(qual + r'(' + '|'.join(BANNED) + r')\s*\(')
    # iterator construction: fs::directory_iterator(p) / {p} / it(p) / it{p}
    itr = re.compile(qual + r'(' + '|'.join(ITERATORS) + r')\b\s*(?:[A-Za-z_]\w*\s*)?([({])')
    return call, itr


def args_of(text, open_idx):
    """Paren/brace-match from text[open_idx]; return the list of top-level
    argument strings, or None if unbalanced within the file."""
    pairs = {'(': ')', '{': '}', '[': ']'}
    args, cur, stack = [], [], []
    for j in range(open_idx, len(text)):
        c = text[j]
        if c in pairs:
            stack.append(pairs[c])
            if len(stack) == 1:
                continue
        elif stack and c == stack[-1]:
            stack.pop()
            if not stack:
                args.append(''.join(cur))
                return [a for a in args if a.strip()]
        elif c == ',' and len(stack) == 1:
            args.append(''.join(cur))
            cur = []
            continue
        cur.append(c)
    return None


def is_ec_call(args):
    return bool(args) and (bool(EC_LAST_ARG.search(args[-1].strip())) or 'error_code' in args[-1])


def scan_file(path, rel, stats, issues, approved, keys=None):
    if keys is None:
        keys = []
    try:
        with open(path, errors='replace') as f:
            views = lex_cxx(f.readlines())
    except OSError as e:
        issues.append(f'{rel}: unreadable: {e}')
        return
    stats['files'] += 1
    aliases = {'fs'}
    for v in views:
        m = ALIAS_DECL.search(v.code)
        if m:
            aliases.add(m.group(1))
    call_re, itr_re = build_patterns(aliases)
    # joined live code with an offset -> line map
    parts, starts, pos = [], [], 0
    host_text = []
    for v in views:
        starts.append(pos)
        code = v.code if v.live else ''
        if not v.live and v.code:
            host_text.append(v.code)
        if v.live and v.code.strip():
            stats['live_lines'] += 1
        parts.append(code)
        pos += len(code) + 1
    text = '\n'.join(parts)
    host = '\n'.join(host_text)
    stats['host_only_matches'] += len(call_re.findall(host)) + len(itr_re.findall(host))

    def line_of(off):
        lo, hi = 0, len(starts) - 1
        while lo < hi:
            mid = (lo + hi + 1) // 2
            if starts[mid] <= off:
                lo = mid
            else:
                hi = mid - 1
        return views[lo]

    for v in views:
        if v.live and USING_DIRECTIVE.search(v.code):
            if APPROVED_MARK in v.raw:
                approved.append(f'{rel}:{v.no}: {v.code.strip()[:100]}')
            else:
                issues.append(f'{rel}:{v.no}: using-directive hides std::filesystem calls: {v.code.strip()[:100]}')
                keys.append(baseline_key(rel, 'using-directive', v.code))

    for kind, rx in (('call', call_re), ('iter', itr_re)):
        for m in rx.finditer(text):
            open_idx = m.end() - 1
            args = args_of(text, open_idx)
            v = line_of(m.start())
            if kind == 'iter' and args == []:
                continue  # default-constructed end iterator: noexcept
            stats['sites'] += 1
            if args is not None and is_ec_call(args):
                stats['ec'] += 1
                continue
            if APPROVED_MARK in v.raw:
                approved.append(f'{rel}:{v.no}: {m.group(1)}: {v.code.strip()[:100]}')
                continue
            why = 'unbalanced call' if args is None else 'no error_code argument'
            issues.append(f'{rel}:{v.no}: {m.group(1)} ({why}): {v.code.strip()[:100]}')
            keys.append(baseline_key(rel, m.group(1), v.code))


def baseline_key(rel, fn, code):
    return rel + '|' + fn + '|' + ' '.join(code.split())


def read_baseline(path):
    counts = {}
    with open(path, errors='replace') as f:
        for ln in f:
            ln = ln.rstrip('\n')
            if ln and not ln.startswith('#'):
                counts[ln] = counts.get(ln, 0) + 1
    return counts


def files_from(spec):
    """SOURCE-SELECTION.json (any {'path': ...} entries) or a newline list."""
    out = set()
    with open(spec, errors='replace') as f:
        raw = f.read()
    try:
        data = json.loads(raw)
    except ValueError:
        data = None
    if data is None:
        cands = [ln.strip() for ln in raw.splitlines() if ln.strip()]
    else:
        cands = []
        def walk(x):
            if isinstance(x, dict):
                p = x.get('path')
                if isinstance(p, str):
                    cands.append(p)
                for val in x.values():
                    walk(val)
            elif isinstance(x, list):
                for val in x:
                    walk(val)
        walk(data)
    for p in cands:
        if p.endswith(CXX_EXT):
            out.add(p if os.path.isabs(p) else os.path.join(WHAT, p))
    return out


def run(roots, specs, verbose=False, baseline=None, write_baseline=None):
    paths, missing = set(), []
    for r in roots:
        if not os.path.isdir(r):
            print(f'FS-THROW GATE: ERROR root does not exist: {r}')
            return 2
        paths.update(src_files(r))
    for s in specs:
        if not os.path.isfile(s):
            print(f'FS-THROW GATE: ERROR --files-from not found: {s}')
            return 2
        listed = files_from(s)
        if not listed:
            print(f'FS-THROW GATE: ERROR --files-from lists ZERO C/C++ files: {s}')
            return 2
        for p in listed:
            (paths.add(p) if os.path.isfile(p) else missing.append(p))
    if missing:
        print(f'FS-THROW GATE: ERROR {len(missing)} listed file(s) missing, first: {missing[0]}')
        return 2
    stats = dict(files=0, live_lines=0, sites=0, ec=0, host_only_matches=0)
    issues, approved, keys = [], [], []
    for p in sorted(paths):
        rel = os.path.relpath(p, WHAT)
        scan_file(p, rel, stats, issues, approved, keys)
    print(f"FS-THROW GATE: scope {len(roots)} root(s) + {len(specs)} file list(s)")
    print(f"FS-THROW GATE: checked {stats['files']} files ({stats['live_lines']} PS5-live code lines); "
          f"{stats['sites']} std::filesystem call sites: {stats['ec']} error_code overloads, "
          f"{len(approved)} approved, {len(issues)} violations; "
          f"{stats['host_only_matches']} matches in host-only regions skipped")
    if verbose:
        for a in approved:
            print('  APPROVED', a)
    if stats['files'] == 0:
        print('FS-THROW GATE: FAIL — checked ZERO files (a gate that scans nothing is a failed gate)')
        return 2
    if write_baseline:
        with open(write_baseline, 'w') as f:
            f.write('# fs_throw_gate baseline: known throwing std::filesystem call sites\n'
                    '# (tracked debt, NOT approvals). One key per site: relpath|function|code\n')
            for k in sorted(keys):
                f.write(k + '\n')
        print(f'FS-THROW GATE: wrote baseline of {len(keys)} known sites -> {write_baseline}')
        return 0
    if baseline:
        if not os.path.isfile(baseline):
            print(f'FS-THROW GATE: ERROR baseline not found: {baseline}')
            return 2
        known = read_baseline(baseline)
        if not known:
            print(f'FS-THROW GATE: ERROR baseline lists ZERO sites: {baseline}')
            return 2
        remaining = dict(known)
        new = []
        for k, msg in zip(keys, issues):
            if remaining.get(k, 0) > 0:
                remaining[k] -= 1
            else:
                new.append(msg)
        resolved = sum(remaining.values())
        print(f'FS-THROW GATE: baseline {os.path.basename(baseline)}: {sum(known.values())} known sites; '
              f'{len(issues) - len(new)} still present, {resolved} resolved, {len(new)} NEW')
        if resolved:
            print('FS-THROW GATE: note: resolved sites -> shrink the baseline with --write-baseline')
        if new:
            print('FS-THROW GATE: VIOLATIONS (new or changed throwing calls not in the baseline)')
            for x in new:
                print('  ', x)
            return 1
        print('FS-THROW GATE: PASS (no new throwing calls; known baseline debt printed above)')
        return 0
    if issues:
        print('FS-THROW GATE: VIOLATIONS')
        for x in issues:
            print('  ', x)
        return 1
    print('FS-THROW GATE: PASS')
    return 0


def selftest():
    cases = [
        ('throwing call', 'void f(P p){ std::filesystem::exists(p); }', 1),
        ('ec overload', 'void f(P p){ std::error_code ec; std::filesystem::exists(p, ec); }', 0),
        ('ec overload multi-line', 'void f(P p){ std::error_code ec;\n  std::filesystem::create_directories(\n    p,\n    ec); }', 0),
        ('fs alias declared', 'namespace stdfs = std::filesystem;\nvoid f(P p){ stdfs::remove(p); }', 1),
        ('conventional fs alias', 'void f(P p){ fs::weakly_canonical(p); }', 1),
        ('comment ignored', '// std::filesystem::exists(p);\n/* fs::remove(p); */', 0),
        ('string ignored', 'const char* s = "std::filesystem::exists(p)";', 0),
        ('host-only region skipped', '#ifndef __PROSPERO__\nvoid f(P p){ std::filesystem::exists(p); }\n#endif', 0),
        ('PS5 branch live', '#ifdef __PROSPERO__\nvoid f(P p){ fs::exists(p); }\n#endif', 1),
        ('approved marker', 'void f(P p){ fs::exists(p); } // [FS-THROW-APPROVED] status arg', 0),
        ('iterator throws', 'void f(P p){ for (auto& e : std::filesystem::directory_iterator(p)) {} }', 1),
        ('iterator ec', 'void f(P p){ std::error_code ec; fs::directory_iterator it(p, ec); }', 0),
        ('end iterator', 'void f(){ auto e = fs::directory_iterator(); }', 0),
        ('using directive', 'using namespace std::filesystem;\nvoid f(P p){ exists(p); }', 1),
        ('member call not flagged', 'void f(Path p){ p.exists(); obj->remove(x); }', 0),
        ('path type not flagged', 'std::filesystem::path p = std::filesystem::path("x");', 0),
        ('rename 3-arg ec', 'void f(P a,P b){ std::error_code err; fs::rename(a, b, err); }', 0),
        ('rename 2-arg throws', 'void f(P a,P b){ fs::rename(a, b); }', 1),
    ]
    failed = 0
    with tempfile.TemporaryDirectory() as d:
        for name, src, want in cases:
            p = os.path.join(d, 'case.cpp')
            with open(p, 'w') as f:
                f.write(src + '\n')
            stats = dict(files=0, live_lines=0, sites=0, ec=0, host_only_matches=0)
            issues, approved = [], []
            scan_file(p, 'case.cpp', stats, issues, approved)
            ok = len(issues) == want
            failed += not ok
            print(f"  {'PASS' if ok else 'FAIL'}  {name}: violations={len(issues)} want={want}")
        empty = os.path.join(d, 'empty-root')
        os.mkdir(empty)
        rc = run([empty], [])
        extra = [('empty root -> exit 2', rc == 2)]
        # baseline ratchet: known site passes, a new site fails, an empty baseline errors
        tree = os.path.join(d, 'tree')
        os.mkdir(tree)
        with open(os.path.join(tree, 'a.cpp'), 'w') as f:
            f.write('void f(P p){ fs::exists(p); }\n')
        bl = os.path.join(d, 'bl.txt')
        extra.append(('write baseline', run([tree], [], write_baseline=bl) == 0))
        extra.append(('known site passes', run([tree], [], baseline=bl) == 0))
        with open(os.path.join(tree, 'b.cpp'), 'w') as f:
            f.write('void g(P p){ fs::remove(p); }\n')
        extra.append(('new site fails', run([tree], [], baseline=bl) == 1))
        with open(bl, 'w') as f:
            f.write('# empty\n')
        extra.append(('empty baseline -> exit 2', run([tree], [], baseline=bl) == 2))
        for name, ok in extra:
            failed += not ok
            print(f"  {'PASS' if ok else 'FAIL'}  {name}")
    total = len(cases) + len(extra)
    print(f'FS-THROW GATE SELFTEST: {total - failed}/{total} PASS')
    return 1 if failed else 0


def main(argv):
    roots, specs, verbose = [], [], False
    opts = {}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--selftest':
            return selftest()
        if a in ('--root', '--files-from') and i + 1 < len(argv):
            (roots if a == '--root' else specs).append(os.path.abspath(argv[i + 1]))
            i += 2
            continue
        if a in ('--baseline', '--write-baseline') and i + 1 < len(argv):
            opts[a[2:].replace('-', '_')] = os.path.abspath(argv[i + 1])
            i += 2
            continue
        if a in ('-v', '--verbose'):
            verbose = True
            i += 1
            continue
        print(f'usage error: unknown argument {a!r}\n{__doc__}')
        return 2
    if not roots and not specs:
        roots = [DEFAULT_SRC]
    return run(roots, specs, verbose, **opts)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
