#!/usr/bin/env python3
"""Claude round-12 item 3: static gate — fail the build BEFORE COMPILE if any
C++ source in the candidate tree (or any staged patch about to be composed
into it) could attempt a CODE DETOUR on PS5.

Why: PS5 game code pages are execute-only (no read, no write). Every hooking
mechanism that works by patching executable memory — PLH/PolyHook x64Detour,
MinHook/Detours-style APIs, mprotect(PROT_EXEC)+byte-patch — can only fail or
crash. The port already soft-refuses these at runtime (the R3 FuncPtr-marshal
carriers and the owned-memory registry exist precisely because detours cannot
work), but the LoadMap hook still TRIED a detour path
(outputs/2026-10-08-l25-prep/round5-staging/world-fix/ — the eager/deferred
RegisterLoadMapPreCallback/PostCallback arms). Claude round-12 item 3:
"a code-detour attempt must never be reachable on PS5" — enforced here at
BUILD time, not runtime.

WHAT IS FLAGGED (rules):
  A  Direct PLH/PolyHook/MinHook/Detours API usage (PLH::, x64Detour, IatHook
     installs via ->hook(), DetourAttach, MH_CreateHook, InstallDetour,
     hook_func, InstallHook/ActivateHook/EnsureHooked install primitives).
  B  Writes of function pointers INTO code pages (write-through destinations
     derived from text/code/exec segment words). Plain UFunction '->Func ='
     swaps are NOT flagged — those are data writes to UFunction objects.
  C  mprotect/mmap with PROT_EXEC (or VirtualProtect PAGE_EXECUTE*) in PS5
     code. Ownership of the target region is not statically knowable; the
     review marker is the escape hatch.
  D  Direct byte patching of code (memcpy/memmove/memset to a destination
     derived from text/code/exec segment words).
  E1 The LoadMap detour path that took the detour arm
     (Unreal::Hook::RegisterLoadMapPreCallback / RegisterLoadMapPostCallback
     calls) unless the site sits in a RECOGNIZED SOFT-REFUSED shape (see
     below).
  E2 (patch mode only) PURE-VMT: while the composed tree still contains the
     LoadMap detour-arm family, any patch that ADDS LoadMap hook machinery
     (vtable/slot/poke/entry/vmt) must ALSO remove at least one LoadMap
     detour-arm line (RegisterLoadMap*Callback call or try_deferred_loadmap_
     arm). This is Claude's "the LoadMap hook still needs the pure VMT
     redesign": a LoadMap patch that keeps the detour arm around is not the
     redesign and is rejected.

WHAT IS NOT FLAGGED (allowed by design):
  - UFunction Func swaps (data writes to UFunction objects).
  - Vtable slot swaps on PRIVATE COPIES (the VmtTickPump law: the original
    table is never written; e.g. '*copy_slot = ...').
  - The R3 carrier system (FuncPtr registration/marshal — no code writes).
  - Comments and log strings mentioning detours (informational; e.g. the
    "[R3] refused:" and "detour installation likely failed" lines).
  - Code inside '#ifndef __PROSPERO__' / '#else' of '#ifdef __PROSPERO__' /
    '#ifdef _WIN32' regions — a small preprocessor tracker marks those
    regions not-PS5-live and only PS5-live code is enforced (the gate is
    conservative: unevaluable conditions count as live).

RECOGNIZED SOFT-REFUSED SHAPES (E1 grandfathering, current tree state):
  The existing LoadMap detour-arm sites are runtime-soft-refused by the R3
  layer (owned-memory registry refuses before any exec access; loud refusal
  lines). The gate recognizes a site as soft-refused when any of:
      UE4SS_R3_MARSHAL_MODE / R3MarshalGate::refuse_registration_site /
      R3MarshalPumpPending / m_loadmap_deferred_arm_pending /
      "[R3] LoadMap deferred arm refused"
  appears within +/- SOFT_REFUSE_WINDOW lines. Such sites are reported as
  NOTE lines (visible, auditable), never as violations. Any NEW unguarded
  arm — or a patch that strips these guards away — is a violation.

EXEMPTIONS: append '  // [DETOUR-APPROVED] <reason>' to a source line (or,
in patch mode, add a diff comment line '# [DETOUR-APPROVED] <reason>' to
exempt the whole patch from E2). Exemptions are counted and printed, never
silent.

USAGE (build pipeline, BEFORE compile — same slot as fs_throw_gate.py):
    python3 no_detour_gate.py                     # scan default candidate tree
    python3 no_detour_gate.py --root <dir> ...    # scan extra/other roots
    python3 no_detour_gate.py --patch <f.patch>   # also gate staged patches
    python3 no_detour_gate.py --selftest          # run the built-in test suite

Exit codes: 0 = PASS, 1 = VIOLATIONS (do not build), 2 = usage/env error.
"""
import glob
import os
import re
import sys
import tempfile

# NOTE: this file lives at <worktree>/outputs/2026-10-08-l25-prep/harvest/ —
# four dirnames up is the worktree root. (fs_throw_gate.py in this directory
# uses only three and therefore silently scans ZERO files; do not copy that.)
WHAT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_SRC = os.path.join(WHAT, 'outputs', '2026-10-08-l26-runtime', 'typescan-r4', 'candidate')
REAL_PATCH = os.path.join(WHAT, 'outputs', '2026-10-08-l25-prep', 'round5-staging',
                          'world-fix', '04-event-map-logger.patch')

APPROVED_MARK = '// [DETOUR-APPROVED]'
PATCH_APPROVED_MARK = '# [DETOUR-APPROVED]'
SRC_GLOBS = ('*.cpp', '*.cc', '*.cxx', '*.hpp', '*.h', '*.inc', '*.inl')
SOFT_REFUSE_WINDOW = 60
SOFT_REFUSE_MARKERS = (
    'UE4SS_R3_MARSHAL_MODE',
    'R3MarshalGate::refuse_registration_site',
    'R3MarshalPumpPending',
    'm_loadmap_deferred_arm_pending',
    '[R3] LoadMap deferred arm refused',
)

# --- rule regexes (matched against CODE PARTS only: comments and string
# contents are stripped, so informational log lines never match) ------------
RULE_A_PLH = re.compile(
    r'\bPLH::|\bPolyHook\b|\bx64Detour\b|\bx86Detour\b|\bDetourAttach\b|'
    r'\bDetourDetach\b|\bDetourTransactionBegin\b|\bDetourUpdateThread\b|'
    r'\bMH_CreateHook\b|\bMH_EnableHook\b|\bMH_DisableHook\b|'
    r'\bCreateAndEnableHook\b|\bInstallDetour\b|\bhook_func\b')
RULE_A_INSTALL = re.compile(
    r'->\s*(?:hook|unhook)\s*\(|\bInstallHook\s*\(|\bActivateHook\s*\(|'
    r'\bEnsureHooked\s*\(')
RULE_C_EXECPROT = re.compile(
    r'\b(?:mprotect|mmap)\s*\([^\n]*PROT_EXEC|\bVirtualProtect\s*\([^\n]*PAGE_EXECUTE|'
    r'\bmmap\s*\([^\n]*MAP_JIT\b|\bpthread_jit_write_protect\b|\b__clear_cache\s*\(')
# destinations derived from code-segment pointers. Deliberately COMPOUND
# markers only (text_base, code_ptr, .text, ...) — bare 'text'/'code'/'exec'
# words are ordinary identifiers in this tree (FText* text, whence_text,
# codecache) and never indicate a code-page destination.
_SEG = (r'(?:\.text\b|text_base|text_addr|text_segment|text_seg|text_start|'
        r'text_begin|text_end|text_page|text_ptr|text_word|code_base|code_addr|'
        r'code_segment|code_seg|code_page|code_ptr|code_word|exec_base|exec_page|'
        r'exec_seg|TextSegment)')
RULE_D_MEMCPY = re.compile(
    r'\b(?:std::)?(?:memcpy|memmove|memset)\s*\(\s*[&]?\s*(' + _SEG + r')\b')
# write-through: a statement that STARTS by dereferencing a segment pointer
# ('*code_base = ...') or casts one ('*(void**)(code_base+8) = ...').
# Declarations ('void* code_base = resolve();') do not match.
RULE_D_WRITE = re.compile(
    r'(?:^\s*\*+\s*(' + _SEG + r')\b[^=]*=[^=]'
    r'|\*\s*\([^)]*\)\s*\(\s*[&]?\s*(' + _SEG + r')\b[^=]*=[^=])')
RULE_E1_LOADMAP_ARM = re.compile(
    r'\bRegisterLoadMap(?:Pre|Post)Callback\s*\(')
LOADMAP_FAMILY = re.compile(
    r'\bRegisterLoadMap(?:Pre|Post)Callback\s*\(|\btry_deferred_loadmap_arm\s*\(')
# patch-mode pure-VMT: patch adds LoadMap hook machinery...
LOADMAP_MACHINERY = re.compile(r'LoadMap', re.IGNORECASE)


class LineView:
    """One source line: raw text + code part + PS5-liveness."""
    __slots__ = ('no', 'raw', 'code', 'live')

    def __init__(self, no, raw, code, live):
        self.no, self.raw, self.code, self.live = no, raw, code, live


def lex_cxx(lines):
    """Yield LineViews. Strips //- and /* */-comments, blanks string/char
    literal contents, and tracks a conservative __PROSPERO__/_WIN32
    preprocessor stack for PS5 liveness (a line is 'live' when no enclosing
    region is definitively NOT taken on PS5)."""
    out = []
    pp = []          # stack of True/False/None (None = unevaluable -> live)
    in_block = False
    def frame_live():
        return all(f is not False for f in pp)
    for i, raw in enumerate(lines, 1):
        s = raw.rstrip('\n')
        t = s.lstrip()
        if in_block:
            end = s.find('*/')
            code = '' if end < 0 else s[end + 2:]
            if end < 0:
                out.append(LineView(i, s, '', frame_live()))
                continue
            s = code
            in_block = False
        if t.startswith('#'):
            d = t
            token_pros = '__PROSPERO__' in d
            token_win = re.search(r'\b_WIN32\b|\bWIN32\b', d)
            if re.match(r'#\s*ifdef\s', d) or re.match(r'#\s*ifndef\s', d):
                if token_pros or token_win:
                    # ifdef -> macro present; ifndef -> macro absent.
                    # __PROSPERO__ is defined on PS5; _WIN32/WIN32 is not.
                    present = bool(re.match(r'#\s*ifdef\s', d))
                    macro_on_ps5 = token_pros
                    frame = present == macro_on_ps5
                else:
                    frame = None
                pp.append(frame)
            elif re.match(r'#\s*if\s', d):
                if token_pros or token_win:
                    neg = bool(re.search(r'!\s*defined\s*\(\s*(__PROSPERO__|_WIN32)\s*\)', d))
                    if token_pros:
                        frame = not neg
                    else:
                        frame = neg
                else:
                    frame = None
                pp.append(frame)
            elif re.match(r'#\s*elif\s', d):
                if pp:
                    # flip the branch, then conservatively mark unevaluable
                    # unless the condition is clearly prosperity/win-only
                    pp[-1] = None
            elif re.match(r'#\s*else\b', d):
                if pp and pp[-1] is not None:
                    pp[-1] = not pp[-1]
            elif re.match(r'#\s*endif\b', d):
                if pp:
                    pp.pop()
            out.append(LineView(i, s, '', frame_live()))
            continue
        # strip comments / blank literals with a small scanner
        buf = []
        j, n = 0, len(s)
        state = 'code'
        while j < n:
            c = s[j]
            if state == 'code':
                if c == '/' and j + 1 < n and s[j + 1] == '/':
                    break
                if c == '/' and j + 1 < n and s[j + 1] == '*':
                    in_block = True
                    j += 2
                    k = s.find('*/', j)
                    if k < 0:
                        break
                    j = k + 2
                    continue
                if c == '"':
                    state = 'str'
                    buf.append('"')
                    j += 1
                    continue
                if c == "'":
                    state = 'chr'
                    buf.append("'")
                    j += 1
                    continue
                buf.append(c)
                j += 1
            elif state == 'str':
                if c == '\\' and j + 1 < n:
                    j += 2
                    continue
                if c == '"':
                    buf.append('"')
                    state = 'code'
                j += 1
            else:  # chr
                if c == '\\' and j + 1 < n:
                    j += 2
                    continue
                if c == "'":
                    buf.append("'")
                    state = 'code'
                j += 1
        out.append(LineView(i, s, ''.join(buf), frame_live()))
    return out


def src_files(root):
    files = []
    for pat in SRC_GLOBS:
        files.extend(glob.glob(os.path.join(root, '**', pat), recursive=True))
    return sorted(set(f for f in files
                      if not f.endswith(('.orig', '.rej', '.new', '.bak'))))


def scan_source(path, rel, issues, notes):
    """Rules A-D and E1 on one C++ file. Returns True if the LoadMap
    detour-arm family is present in this file (used for E2)."""
    try:
        with open(path, errors='replace') as f:
            views = lex_cxx(f.readlines())
    except OSError as e:
        issues.append(f'{rel}: unreadable: {e}')
        return False
    family_present = any(LOADMAP_FAMILY.search(v.code) for v in views)
    for idx, v in enumerate(views):
        if not v.code or not v.live:
            continue
        if APPROVED_MARK in v.raw:
            notes.append(f'{rel}:{v.no}: EXEMPT (review-approved): {v.code.strip()[:90]}')
            continue
        hit = None
        m = RULE_A_PLH.search(v.code) or RULE_A_INSTALL.search(v.code)
        if m:
            hit = 'A code-detour API (PLH/PolyHook/MinHook/install primitive)'
        if not hit:
            m = RULE_C_EXECPROT.search(v.code)
            if m:
                hit = 'C exec-page protection (mprotect/mmap PROT_EXEC class)'
        if not hit:
            m = RULE_D_MEMCPY.search(v.code) or RULE_D_WRITE.search(v.code)
            if m:
                hit = 'D code byte-patch (write/memcpy to text/code/exec destination)'
        if not hit and RULE_E1_LOADMAP_ARM.search(v.code):
            lo = max(0, idx - SOFT_REFUSE_WINDOW)
            hi = min(len(views), idx + SOFT_REFUSE_WINDOW + 1)
            window = ''.join(x.raw for x in views[lo:hi])
            if any(marker in window for marker in SOFT_REFUSE_MARKERS):
                notes.append(f'{rel}:{v.no}: NOTE LoadMap detour-arm site in a recognized '
                             f'soft-refused (R3) shape: {v.code.strip()[:80]}')
                continue
            hit = 'E1 LoadMap detour-arm outside any recognized soft-refused shape'
        if hit:
            issues.append(f'{rel}:{v.no}: {hit}: {v.code.strip()[:100]}')
    return family_present


# ---------------------------------------------------------------------------
# patch mode
# ---------------------------------------------------------------------------

class PatchView:
    def __init__(self, raw, kind):
        self.raw, self.kind = raw, kind   # kind: 'add' | 'del' | 'ctx' | 'hdr'


def parse_patch(path):
    """Minimal unified-diff parser -> (per-target-file list of PatchView)."""
    per_file = []
    cur = None
    with open(path, errors='replace') as f:
        for raw in f:
            raw = raw.rstrip('\n')
            if raw.startswith('+++ b/'):
                cur = {'file': raw[6:].strip(), 'lines': []}
                per_file.append(cur)
                continue
            if raw.startswith(('--- ', 'diff ', 'index ', 'new file mode',
                               'old mode', 'new mode', 'similarity ',
                               'rename ', 'copy ', 'Binary files', '@@ -', '@@ -0')):
                if raw.startswith('@@') and cur is not None:
                    cur['lines'].append(PatchView(raw, 'hdr'))
                continue
            if cur is None:
                continue
            if raw.startswith('+'):
                cur['lines'].append(PatchView(raw, 'add'))
            elif raw.startswith('-'):
                cur['lines'].append(PatchView(raw, 'del'))
            elif raw.startswith(('\\ ', 'GIT ')):
                continue
            else:
                cur['lines'].append(PatchView(raw, 'ctx'))
    return per_file


def scan_patch(path, rel, family_in_tree, issues, notes):
    """Rules A-D and E1 on ADDED lines (best-effort pp tracking through
    context+added lines) + E2 pure-VMT for LoadMap patches."""
    whole = open(path, errors='replace').read()
    patch_exempt = PATCH_APPROVED_MARK in whole
    # E2 aggregate across ALL target files: machinery added vs family removed
    adds_machinery = []
    removes_family = []
    for entry in parse_patch(path):
        for p in entry['lines']:
            if p.kind == 'add' and LOADMAP_MACHINERY.search(p.raw) \
                    and re.search(r'vtable|vmt|slot|poke|entry|hook', p.raw, re.IGNORECASE):
                adds_machinery.append((entry['file'], p.raw))
            elif p.kind == 'del' and LOADMAP_FAMILY.search(p.raw):
                removes_family.append((entry['file'], p.raw))
    if adds_machinery and family_in_tree and not removes_family and not patch_exempt:
        issues.append(
            f'{rel}: E2 not the pure VMT redesign: patch ADDS LoadMap vtable/slot '
            f'machinery ({len(adds_machinery)} added lines across its files, e.g. '
            f'"{adds_machinery[0][1].strip()[:60]}") while the composed tree still '
            f'contains the LoadMap detour-arm family (RegisterLoadMap*Callback / '
            f'try_deferred_loadmap_arm) and the patch removes none of it. Claude '
            f'round-12 item 3: the code-detour attempt must be unreachable on PS5; '
            f'finish the redesign or add "{PATCH_APPROVED_MARK} <reason>".')
    elif adds_machinery and family_in_tree and removes_family:
        notes.append(f'{rel}: NOTE LoadMap machinery added AND detour-arm lines '
                     f'removed ({len(removes_family)}) — pure-VMT shape OK')
    for entry in parse_patch(path):
        pv = entry['lines']
        views = []
        for p in pv:
            if p.kind == 'hdr':
                continue
            text = p.raw[1:] if p.kind in ('add', 'ctx') else p.raw[1:]
            views.append((p.kind, text))
        # preprocessor state via lex over ctx+add stream (removed lines do
        # not exist in the composed file)
        stream = [t for k, t in views if k in ('add', 'ctx')]
        lexed = lex_cxx(stream)
        # rule application per added line, aligned with lexed stream:
        li = 0
        for kind, text in views:
            if kind != 'add':
                continue
            if APPROVED_MARK in text:
                notes.append(f'{rel} (+{entry["file"]}): EXEMPT (review-approved): '
                             f'{text.strip()[:80]}')
                continue
            while li < len(lexed) and lexed[li].raw != text:
                li += 1
            if li >= len(lexed):
                lv = LineView(0, text, text, True)
            else:
                lv = lexed[li]
                li += 1
            if not lv.code or not lv.live:
                continue
            hit = None
            m = RULE_A_PLH.search(lv.code) or RULE_A_INSTALL.search(lv.code)
            if m:
                hit = 'A code-detour API (PLH/PolyHook/MinHook/install primitive)'
            if not hit:
                m = RULE_C_EXECPROT.search(lv.code)
                if m:
                    hit = 'C exec-page protection (mprotect/mmap PROT_EXEC class)'
            if not hit:
                m = RULE_D_MEMCPY.search(lv.code) or RULE_D_WRITE.search(lv.code)
                if m:
                    hit = 'D code byte-patch (write/memcpy to text/code/exec destination)'
            if not hit and RULE_E1_LOADMAP_ARM.search(lv.code):
                blob = '\n'.join(t for k, t in views)
                if any(marker in blob for marker in SOFT_REFUSE_MARKERS):
                    notes.append(f'{rel} (+{entry["file"]}): NOTE LoadMap detour-arm '
                                 f'added in a recognized soft-refused (R3) shape: '
                                 f'{lv.code.strip()[:80]}')
                    continue
                hit = 'E1 LoadMap detour-arm outside any recognized soft-refused shape'
            if hit:
                issues.append(f'{rel} -> {entry["file"]} (+): {hit}: {lv.code.strip()[:100]}')


def run(roots, patches):
    issues, notes = [], []
    family_in_tree = False
    nfiles = 0
    for root in roots:
        try:
            base = os.path.relpath(root, WHAT)
            if base.startswith('..'):
                raise ValueError
        except ValueError:
            base = os.path.abspath(root)
        for f in src_files(root):
            nfiles += 1
            if scan_source(f, os.path.join(base, os.path.relpath(f, root)), issues, notes):
                family_in_tree = True
        # family may also live in files without violations; do a raw sweep too
        for f in src_files(root):
            try:
                txt = open(f, errors='replace').read()
            except OSError:
                continue
            if LOADMAP_FAMILY.search(txt):
                family_in_tree = True
    for p in patches:
        try:
            prel = os.path.relpath(p, WHAT)
            if prel.startswith('..'):
                raise ValueError
        except ValueError:
            prel = os.path.abspath(p)
        scan_patch(p, prel, family_in_tree, issues, notes)
    return issues, notes, nfiles, family_in_tree


# ---------------------------------------------------------------------------
# selftest
# ---------------------------------------------------------------------------

def selftest():
    import shutil
    tmp = tempfile.mkdtemp(prefix='nodetour-gate-selftest-')
    fails = []
    def expect(name, src, want_violations, roots=None):
        root = os.path.join(tmp, name)
        os.makedirs(root, exist_ok=True)
        if src:
            open(os.path.join(root, 't.cpp'), 'w').write(src)
        issues, notes, n, fam = run(roots or [root], [])
        got = bool(issues)
        ok = got == want_violations
        print(f'  [{"OK" if ok else "FAIL"}] {name}: '
              f'violations={got} (want {want_violations}); {len(notes)} notes')
        if not ok:
            fails.append(name)
            for x in issues:
                print('        ->', x)
        return issues

    print('selftest: fixture cases')
    expect('T1_plh_apis', 'auto d = std::make_unique<PLH::x64Detour>(a, b);\nd->hook();\n', True)
    expect('T1b_install_primitives', 'EnsureHooked(x);\nActivateHook(y);\nInstallDetour(a,b);\nhook_func(t);\n', True)
    expect('T2_execprot', 'mprotect(pg, len, PROT_READ|PROT_WRITE|PROT_EXEC);\nvoid* m = mmap(0, sz, PROT_EXEC|PROT_READ, 0);\n', True)
    expect('T2b_mmap_var_ok', 'void* map = ::mmap(nullptr, size, protection, sharing, m_fd, 0);\n', False)
    expect('T3_bytepatch', 'std::memcpy(text_base+off, bytes, 14);\nmemset(code_ptr, 0x90, 5);\n', True)
    expect('T3b_memcpy_buffer_ok', 'std::memcpy(out, buf, n);\nstd::memcpy(output_buffer, error_message.data(), error_message.size());\n', False)
    expect('T4_writethru_code',
           '*(void**)(code_base+8) = &hookfn;\n*text_addr = hook_bytes;\n', True)
    expect('T4b_ufunc_and_copy_ok',
           'SomeUFunction->Func = &MyHook;\n*copy_slot = reinterpret_cast<void*>(&entry);\n'
           'allocation[k + i] = hook;\nhook_entry = hook;\n'
           'void* code_base = resolve_text();\n'                       # declaration
           'Unreal::FText* text = static_cast<Unreal::FText*>(p);\n'   # FText object ptr
           '*text = bridged.to_ftext();\n'                              # data write
           'const char* whence_text = lua_tolstring(L, 2, &n);\n',     # bare -text id
           False)
    expect('T5_loadmap_bare', 'Unreal::Hook::RegisterLoadMapPreCallback(&cb, opts);\n', True)
    expect('T5b_loadmap_softrefused',
           'if (Unreal::R3MarshalPumpPending())\n{\n m_loadmap_deferred_arm_pending.store(true);\n}\nelse\n{\n'
           'Unreal::Hook::RegisterLoadMapPreCallback(&cb, opts);\n}\n', False)
    expect('T6_win_only_ok', '#ifndef __PROSPERO__\nm->hook();\nPLH::IatHook h("k","L",0,&t,L"");\nh.hook();\n#endif\n', False)
    expect('T6b_prospero_live', '#ifdef __PROSPERO__\nUnreal::Hook::RegisterLoadMapPreCallback(&cb, o);\n#endif\n', True)
    expect('T7_exempt', 'PLH::x64Detour d(a,b);  // [DETOUR-APPROVED] host-fixture only\n', False)
    expect('T8_comments_logs',
           '// PLH::x64Detour never arms here (detour refused)\n'
           'Output::send(STR("[R3] refused: detour installation likely failed\\n"));\n'
           '/* InstallHook was the old path */\n',
           False)

    print('selftest: real targets')
    issues, notes, n, fam = run([DEFAULT_SRC], [])
    ok = (not issues) and fam
    print(f'  [{"OK" if ok else "FAIL"}] R1_current_tree: files={n} family_present={fam} '
          f'violations={len(issues)} notes={len(notes)} (want PASS + family present)')
    if not ok:
        fails.append('R1_current_tree')
        for x in issues[:20]:
            print('        ->', x)
    for x in notes:
        print('        note:', x)

    issues, notes, n, fam = run([DEFAULT_SRC], [REAL_PATCH])
    pure = [x for x in issues if 'E2' in x]
    ok = bool(issues) and bool(pure)
    print(f'  [{"OK" if ok else "FAIL"}] R2_loadmap_patch: violations={len(issues)} '
          f'(want >=1, incl. E2 pure-VMT); notes={len(notes)}')
    for x in issues:
        print('        ->', x[:220])
    if not ok:
        fails.append('R2_loadmap_patch')

    # synthetic: post-redesign root (no family) + machinery patch -> pass
    root2 = os.path.join(tmp, 'postredesign')
    os.makedirs(root2, exist_ok=True)
    open(os.path.join(root2, 'a.cpp'), 'w').write('int x = 1;\n')
    p_ok = os.path.join(tmp, 'vmt-add.patch')
    open(p_ok, 'w').write(
        '--- a/src/UnrealInitializer.cpp\n+++ b/src/UnrealInitializer.cpp\n'
        '@@ -10,3 +10,5 @@\n ctx\n+        static auto vmt_engine_loadmap_entry() noexcept -> void {}\n'
        '+        vmt_pump_poke_loadmap_slot();\n ctx\n')
    issues, notes, n, fam = run([root2], [p_ok])
    ok = not issues
    print(f'  [{"OK" if ok else "FAIL"}] R3_post_redesign_patch: violations={len(issues)} (want 0)')
    if not ok:
        fails.append('R3_post_redesign_patch')
        for x in issues:
            print('        ->', x[:200])

    # synthetic: redesign patch on family-bearing root (adds machinery AND
    # removes a detour-arm line) -> pass patch rules
    p_fix = os.path.join(tmp, 'vmt-redesign.patch')
    open(p_fix, 'w').write(
        '--- a/src/LuaMod.cpp\n+++ b/src/LuaMod.cpp\n'
        '@@ -100,4 +100,6 @@\n ctx\n'
        '-        Unreal::Hook::RegisterLoadMapPreCallback(&lua_load_map_pre_callback, common_opts);\n'
        '+        static auto vmt_engine_loadmap_entry() noexcept -> void {}\n'
        '+        vmt_pump_poke_loadmap_slot();\n ctx\n')
    issues, notes, n, fam = run([DEFAULT_SRC], [p_fix])
    e2 = [x for x in issues if 'E2' in x]
    ok = not e2
    print(f'  [{"OK" if ok else "FAIL"}] R4_redesign_patch: E2 violations={len(e2)} (want 0); '
          f'other violations={len(issues)}')
    if e2:
        fails.append('R4_redesign_patch')
        for x in e2:
            print('        ->', x[:200])

    # synthetic: patch ADDING a raw detour API is caught in patch mode too
    p_bad = os.path.join(tmp, 'raw-detour.patch')
    open(p_bad, 'w').write(
        '--- a/src/LuaMod.cpp\n+++ b/src/LuaMod.cpp\n@@ -1,3 +1,4 @@\n ctx\n'
        '+        auto d = std::make_unique<PLH::x64Detour>(fn, mine);\n ctx\n')
    issues, notes, n, fam = run([root2], [p_bad])
    ok = any('code-detour API' in x for x in issues)
    print(f'  [{"OK" if ok else "FAIL"}] R5_patch_raw_detour: caught={ok}')
    if not ok:
        fails.append('R5_patch_raw_detour')

    shutil.rmtree(tmp, ignore_errors=True)
    print(f'selftest: {"ALL PASS" if not fails else "FAILURES: " + ", ".join(fails)}')
    return 0 if not fails else 1


def main(argv):
    roots, patches = [], []
    if '--selftest' in argv:
        if len(argv) != 1:
            print('usage: no_detour_gate.py --selftest', file=sys.stderr)
            return 2
        return selftest()
    args = [a for a in argv if a != '--root' and a != '--patch']
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--root':
            i += 1
            if i >= len(argv):
                print('usage: --root needs a directory', file=sys.stderr)
                return 2
            roots.append(argv[i])
        elif a == '--patch':
            i += 1
            if i >= len(argv):
                print('usage: --patch needs a file', file=sys.stderr)
                return 2
            patches.append(argv[i])
        elif a.startswith('-'):
            print(f'usage: unknown option {a}\n'
                  f'  no_detour_gate.py [--root DIR]... [--patch FILE]... [--selftest]',
                  file=sys.stderr)
            return 2
        else:
            if a.endswith(('.patch', '.diff')):
                patches.append(a)
            elif os.path.isdir(a):
                roots.append(a)
            else:
                print(f'usage: unrecognized argument {a}', file=sys.stderr)
                return 2
        i += 1
    if not roots:
        roots = [DEFAULT_SRC]
    for r in roots:
        if not os.path.isdir(r):
            print(f'FAIL: root not found: {r}', file=sys.stderr)
            return 2
    for p in patches:
        if not os.path.isfile(p):
            print(f'FAIL: patch not found: {p}', file=sys.stderr)
            return 2

    issues, notes, nfiles, fam = run(roots, patches)
    if nfiles == 0:
        # refuse the silent-pass-on-empty-scan failure mode
        print('NO-DETOUR GATE: ENV ERROR — 0 C++ sources scanned under '
              f'{roots} (wrong root?); refusing to pass an empty scan',
              file=sys.stderr)
        return 2
    print('NO-DETOUR GATE (Claude round-12 item 3): '
          f'{"VIOLATIONS" if issues else "PASS"}')
    print(f'  scanned {nfiles} C++ sources under {len(roots)} root(s)'
          + (f' + {len(patches)} patch(es)' if patches else ''))
    print(f'  LoadMap detour-arm family in tree: {"present (pure-VMT rule ACTIVE for patches)" if fam else "absent"}')
    for x in issues:
        print('  VIOLATION:', x)
    for x in notes:
        print('  ', x)
    if issues:
        print('NO-DETOUR GATE: VIOLATIONS — a code-detour attempt must never be '
              'reachable on PS5. Fix the code, not the gate '
              f'(or review-approve with "{APPROVED_MARK}").')
        return 1
    print(f'NO-DETOUR GATE: PASS (0 detour-attempt sites; {len(notes)} '
          f'notes/exemptions)')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
