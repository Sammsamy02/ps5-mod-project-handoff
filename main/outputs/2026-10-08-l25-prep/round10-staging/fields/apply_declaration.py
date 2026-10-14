#!/usr/bin/env python3
"""apply_declaration.py — FIELDS track (round 10) staged edit of the compiled PS5 layout
declaration (members literal only; vtables, selection label and profile digest untouched).

Usage: python3 apply_declaration.py [PROFILE_ROOT] [--repoint]
  PROFILE_ROOT default: outputs/2026-10-08-l26-runtime/profile (the real profile)
  edits PROFILE_ROOT/units/CompiledNativeLayoutDeclarations.hpp and refreshes that file's
  candidate_sha256 row in PROFILE_ROOT/CHANGES.json (+ a fields_round10 provenance block).
  --repoint (staging tests only): also point that CHANGES.json row's 'candidate' path at the
  copy under PROFILE_ROOT, so profile/tests/run_tests.py can run on a copy.

Added (each from two independent sources, see FIELDS.md / evidence/FIELD-EVIDENCE.json):
  [FUObjectItem] SerialNumber=0x10
  [FMulticastDelegateProperty] SignatureFunction=0x70 UEP_TotalSize=0x78
  [FWorldContext] ThisCurrentWorld=0x2c0 UEP_TotalSize=0x2c8
  [UEnum] Names=0x40 UEP_TotalSize=0x68
Exact-once replacements; any anchor count != 1 -> exit 1, nothing written.
"""
import hashlib, json, os, sys

REAL = '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/profile'
REAL_DECL = REAL + '/units/CompiledNativeLayoutDeclarations.hpp'

EDITS = [
    ('FUObjectItem section',
     r'[FUObjectItem]\nFlags=0x8\nObject=0x0\nUEP_TotalSize=0x18\n',
     r'[FUObjectItem]\nFlags=0x8\nObject=0x0\nSerialNumber=0x10\nUEP_TotalSize=0x18\n'),
    ('members literal tail (FSetProperty section + closing quote)',
     r'[FSetProperty]\nElementProp=0x70\nUEP_TotalSize=0x90\n",',
     r'[FSetProperty]\nElementProp=0x70\nUEP_TotalSize=0x90\n'
     r'\n[FMulticastDelegateProperty]\nSignatureFunction=0x70\nUEP_TotalSize=0x78\n'
     r'\n[FWorldContext]\nThisCurrentWorld=0x2c0\nUEP_TotalSize=0x2c8\n'
     r'\n[UEnum]\nNames=0x40\nUEP_TotalSize=0x68\n",'),
]

PROVENANCE = {
    'date': '2026-10-12',
    'by': 'round10 FIELDS track (staged; applied only by the build owner)',
    'what': 'members literal gains FUObjectItem::SerialNumber, FMulticastDelegateProperty::{SignatureFunction,UEP_TotalSize}, FWorldContext::{ThisCurrentWorld,UEP_TotalSize}, UEnum::{Names,UEP_TotalSize}; nothing else changes (vtables literal, selection label, profile digest and townfall-build-facts.json are untouched; these are not kRequiredMembers pins)',
    'evidence': 'outputs/2026-10-08-l25-prep/round10-staging/fields/evidence/FIELD-EVIDENCE.json (+ FIELD-DISASSEMBLY.txt, tools/measure_fields.py); table FIELDS.md',
    'sources': 'per value: (1) saved decrypt-off image sha 3b6cfad4... engine code or reflection data, (2) l23 generic compiler measurement of official 5.6.1 headers or the image reflection record; UE4SS upstream 5.6 default layout agrees (corroboration only)',
    'console_ini': 'no change: the PS5 runtime stages this compiled declaration and does not read ue4ss/MemberVariableLayout.ini while it exists',
}


def sha(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    repoint = '--repoint' in sys.argv
    root = args[0] if args else REAL
    decl = os.path.join(root, 'units/CompiledNativeLayoutDeclarations.hpp')
    changes = os.path.join(root, 'CHANGES.json')
    for p in (decl, changes):
        if not os.path.isfile(p):
            print('apply_declaration: FAIL missing %s' % p); return 2
    text = open(decl, encoding='utf-8').read()
    lines = text.split('\n')
    members_idx = next(i for i, l in enumerate(lines) if l.startswith('"'))
    members = lines[members_idx]
    for label, old, new in EDITS:
        n = members.count(old)
        if n != 1:
            print('apply_declaration: FAIL anchor "%s" matched %d times (need 1)' % (label, n)); return 1
        members = members.replace(old, new, 1)
    # the new literal must still parse and keep the binding rules (positive totals, members inside)
    lit = json.loads(members.rstrip(','))
    sec, rows = None, {}
    for ln in lit.split('\n'):
        if ln.startswith('['):
            sec = ln.strip('[]'); continue
        if '=' in ln and sec != 'NativeLayout':
            k, v = ln.split('=', 1); rows.setdefault(sec, {})[k] = int(v, 0)
    width = {'SerialNumber': 4, 'SignatureFunction': 8, 'ThisCurrentWorld': 8, 'Names': 16}
    for s_, kv in rows.items():
        tot = kv.get('UEP_TotalSize', 0)
        if tot <= 0:
            print('apply_declaration: FAIL section %s lacks positive UEP_TotalSize' % s_); return 1
        for k, v in kv.items():
            if k != 'UEP_TotalSize' and v + width.get(k, 1) > tot:
                print('apply_declaration: FAIL %s::%s outside class size' % (s_, k)); return 1
    lines[members_idx] = members
    new_text = '\n'.join(lines)
    data = json.loads(open(changes, encoding='utf-8').read())
    hit = 0
    for row in data.get('files', []):
        if row.get('candidate', '').endswith('/units/CompiledNativeLayoutDeclarations.hpp'):
            hit += 1
            target_row = row
    if hit != 1:
        print('apply_declaration: FAIL CHANGES.json declaration row matched %d times (need 1)' % hit); return 1
    with open(decl, 'w', encoding='utf-8') as f:
        f.write(new_text)
    if repoint:
        target_row['candidate'] = os.path.abspath(decl)
    target_row['candidate_sha256'] = sha(decl)
    data['fields_round10'] = dict(PROVENANCE, declaration_sha256=target_row['candidate_sha256'])
    with open(changes, 'w', encoding='utf-8') as f:
        f.write(json.dumps(data, indent=1) + '\n')
    print('apply_declaration: OK %s (sha256 %s); %d sections now; CHANGES.json row refreshed%s' %
          (decl, target_row['candidate_sha256'], len(rows), ' (repointed to this copy)' if repoint else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
