#!/usr/bin/env python3
"""layout_binder_gate.py — pre-compile gate (added after launch 11, 2026-10-12): every member row in the compiled
PS5 layout declaration must pass the runtime layout binder's checks, on the Mac, BEFORE a build.

Launch 11 failed at startup with "Native accessor width unavailable: UEnum::Names": the declaration gained a row
whose accessor width (frozen generated table NativeLayoutWidths.cpp) is empty, and the binder (Prepare in
NativeLayoutBinding.cpp) refuses the whole layout. This gate reproduces the binder's checks:
  - schema row present (NativeLayoutSchema.inc MemberSchemaEntries; section/key), member target present,
  - accessor width present and non-zero (compile-time: static_assert(Find(cls,key).has_value()) on the exact
    width-table translation unit with its archived PS5 argv, -fsyntax-only),
  - UEP_TotalSize declared and positive per class, offset >= 0 and offset + width <= UEP_TotalSize.
Bitfield rows are skipped for the width check (the binder uses their storage bytes).
Prints the number of rows checked; exit 1 on any failure, 2 on zero rows or missing inputs."""
import json, os, re, subprocess, sys, tempfile
from pathlib import Path

W = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
DECL = W / 'outputs/2026-10-08-l26-runtime/profile/units/CompiledNativeLayoutDeclarations.hpp'
SCHEMA = W / '.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc'
WIDTHS = W / '.superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/src/NativeLayoutWidths.cpp'
# the runtime binder asks GetBindingAccessorWidth (packaging lineage header): genuine active-version getters for
# a few members first, then the broad table. The probe uses a constexpr copy of that exact header.
# the build routes the profile candidate (profile/CHANGES.json row) over the l19 original when one exists
ACTIVE_CAND = W / 'outputs/2026-10-08-l26-runtime/profile/units/NativeLayoutActiveWidths.hpp'
ACTIVE = ACTIVE_CAND if ACTIVE_CAND.is_file() else W / 'outputs/2026-10-07-l19-local-profile-r4/units/NativeLayoutActiveWidths.hpp'
RECEIPT_GLOB = ['outputs/2026-10-08-l26-build-variants/l26-A/pass1/COMPOSITION-RECEIPT.json',
                'outputs/2026-10-08-l26-build-variants/archive-r6']


def find_receipt():
    p = W / RECEIPT_GLOB[0]
    if p.is_file():
        return p
    arch = W / RECEIPT_GLOB[1]
    cands = sorted(arch.glob('*/l26-A/pass1/COMPOSITION-RECEIPT.json'), key=lambda x: x.stat().st_mtime, reverse=True)
    return cands[0] if cands else None


def members(decl_text):
    lit = decl_text.split('\n')[3] if len(decl_text.split('\n')) > 3 else ''
    body = lit.replace('\\n', '\n')
    out, sec = [], None
    for ln in body.split('\n'):
        ln = ln.strip().strip('"').strip(',').strip('"')
        m = re.match(r'^\[([A-Za-z0-9_:]+)\]$', ln)
        if m:
            sec = m.group(1); continue
        m = re.match(r'^([A-Za-z0-9_]+)=(0x[0-9a-fA-F]+|\d+)(?:,(\d+),(\d+),(\d+))?$', ln)
        if m and sec and sec != 'NativeLayout':
            out.append((sec, m.group(1), int(m.group(2), 0), m.group(3) is not None))
    return out


def main():
    global DECL
    if len(sys.argv) > 1:
        DECL = Path(sys.argv[1])
    for p in (DECL, SCHEMA, WIDTHS, ACTIVE):
        if not p.is_file():
            print('LAYOUT BINDER GATE: FAIL (input missing: %s)' % p); return 2
    rows = members(DECL.read_text())
    print('LAYOUT BINDER GATE: %d member rows in %s (active widths: %s)' % (len(rows), DECL.name, ACTIVE))
    if not rows:
        print('LAYOUT BINDER GATE: FAIL (zero rows parsed)'); return 2
    schema = {}
    for m in re.finditer(r'\{"([A-Za-z0-9_]+)", "([A-Za-z0-9_]+)", "([A-Za-z0-9_]+)", "([A-Za-z0-9_]+)", (true|false)\}', SCHEMA.read_text()):
        schema[(m.group(1), m.group(2))] = (m.group(3), m.group(4), m.group(5) == 'true')
    targets = set(re.findall(r'\{"([A-Za-z0-9_]+)", &Unreal::[A-Za-z0-9_]+::MemberOffsets', SCHEMA.read_text()))
    bad = []
    totals = {}
    for sec, key, off, is_bits in rows:
        if key == 'UEP_TotalSize':
            totals[sec] = off
    asserts, width_rows = [], []
    for sec, key, off, is_bits in rows:
        s = schema.get((sec, key))
        if not s:
            bad.append('unknown member (no schema row): %s::%s' % (sec, key)); continue
        if s[2] != is_bits:
            bad.append('bitfield mismatch: %s::%s' % (sec, key))
        if s[0] not in targets:
            bad.append('no member target: %s (for %s::%s)' % (s[0], sec, key))
        if key == 'UEP_TotalSize' or is_bits:
            continue
        width_rows.append((sec, key, off, s[0], s[1]))
        asserts.append('static_assert(RC::Platform::NativeLayouts::LbgWidth("%s","%s").has_value() && *RC::Platform::NativeLayouts::LbgWidth("%s","%s") > 0, "LBG-NOWIDTH %s::%s");'
                       % (s[0], s[1], s[0], s[1], sec, key))
    for sec in {r[0] for r in rows}:
        if totals.get(sec, 0) <= 0:
            bad.append('class without positive UEP_TotalSize: %s' % sec)
    # widths: compile-time probe against the exact width-table unit + archived argv
    rec = find_receipt()
    if rec is None:
        print('LAYOUT BINDER GATE: FAIL (no composition receipt found for the width-table argv)'); return 2
    comp = json.load(open(rec))
    unit = next((u for u in comp['selected_units'] if any(a.endswith('NativeLayoutWidths.cpp') for a in u['argv'])), None)
    if unit is None:
        print('LAYOUT BINDER GATE: FAIL (width-table unit not in %s)' % rec); return 2
    # also emit each width so offsets can be bounds-checked: one constexpr array, read back via static_assert messages
    with tempfile.TemporaryDirectory() as td:
        probe = Path(td) / 'lbg_probe.cpp'
        active = ACTIVE.read_text()
        active = '\n'.join(l for l in active.split('\n') if not l.startswith('#'))
        if active.count('inline auto GetBindingAccessorWidth(') != 1 or active.count('return GetAccessorWidth(class_name, key);') != 1:
            print('LAYOUT BINDER GATE: FAIL (active-width header shape changed: %s)' % ACTIVE); return 2
        active = active.replace('inline auto GetBindingAccessorWidth(', 'constexpr auto LbgWidth(').replace('return GetAccessorWidth(class_name, key);', 'return Find(class_name, key);')
        src = WIDTHS.read_text() + '\n' + active + '\n' + '\n'.join(asserts) + '\n'
        for i, (sec, key, off, cls, can) in enumerate(width_rows):
            tot = totals.get(sec, 0)
            src += ('static_assert(!RC::Platform::NativeLayouts::LbgWidth("%s","%s").has_value() || (%d >= 0 && %d < %d && '
                    '*RC::Platform::NativeLayouts::LbgWidth("%s","%s") <= static_cast<unsigned>(%d - %d)), "LBG-OUTSIDE %s::%s");\n'
                    % (cls, can, off, off, tot, cls, can, tot, off, sec, key))
        probe.write_text(src)
        argv = unit['argv'][:]
        orig = next(a for a in argv if a.endswith('NativeLayoutWidths.cpp'))
        argv[argv.index(orig)] = str(probe)
        for f in ('-o', '-MF'):
            if f in argv:
                k = argv.index(f); del argv[k:k + 2]
        if '-MD' in argv: argv.remove('-MD')
        argv[argv.index('-c')] = '-fsyntax-only'
        env = os.environ.copy(); env['LLVM_CONFIG'] = unit.get('LLVM_CONFIG', '')
        p = subprocess.run(argv, env=env, capture_output=True, text=True)
        for m in re.finditer(r'"(LBG-(?:NOWIDTH|OUTSIDE) [A-Za-z0-9_:]+)"', p.stderr):
            bad.append(m.group(1))
        if p.returncode != 0 and not any(b.startswith('LBG-') for b in bad):
            print(p.stderr[-2000:]); bad.append('width-table probe failed to compile (see above)')
    print('LAYOUT BINDER GATE: checked %d rows (%d width/bounds checks via %s, argv from %s)' % (len(rows), len(width_rows), WIDTHS.name, rec))
    for b in bad:
        print('  FAIL ' + b)
    print('LAYOUT BINDER GATE: %s' % ('FAIL' if bad else 'PASS'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
