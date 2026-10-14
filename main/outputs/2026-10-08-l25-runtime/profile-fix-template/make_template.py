#!/usr/bin/env python3
"""AM profile-fix template generator (Mac-local, read-only on the live tree).

Regenerates profile-fix.diff.tmpl FROM the CURRENT live l25 profile files, so the
template's context lines can never drift from reality. Reads only; writes only
into this profile-fix-template/ directory. Slots introduced (single source of
truth for names; instantiate_profile_fix.py substitutes them):

  <<CREATE_LISTENERS_DEC>> / <<CREATE_LISTENERS_HEX>>      UObjectCreateListeners
  <<DELETE_LISTENERS_DEC>> / <<DELETE_LISTENERS_HEX>>      UObjectDeleteListeners
  <<CRITICAL_SECTION_DEC>> / <<CRITICAL_SECTION_HEX>>      UObjectDeleteListenersCritical (NEW fact)
  <<PRIMARY_SERIAL_DEC>>   / <<PRIMARY_SERIAL_HEX>>        PrimarySerialNumber (adjacent, shifts)
  <<BSHOULD_RECYCLE_DEC>>  / <<BSHOULD_RECYCLE_HEX>>       bShouldRecycleObjectIndices (adjacent)
  <<FUOBJECTARRAY_TOTAL_DEC>> / <<FUOBJECTARRAY_TOTAL_HEX>> FUObjectArray::UEP_TotalSize (adjacent)
  <<SOURCE_AA>>            provenance source string (agent-AA measurement receipt)
  <<PROFILE_DIGEST_NEW>>   recomputed sha256 of the canonical final JSON (helper fills)

Adjacency rationale (derived; agent-AL consumer map was absent at generation
time): THREADSAFE_UOBJECTS=1 (ObjectMacros.h:33) inserts the critical sections
after the two listener arrays inside FUObjectArray, so PrimarySerialNumber,
bShouldRecycleObjectIndices and UEP_TotalSize all shift; they are edited as
slots so agent AA's measured truth lands everywhere at once.
"""
from pathlib import Path
import difflib, json, re, sys

HERE = Path(__file__).resolve().parent
PROFILE = HERE.parents[0] / 'profile'          # outputs/2026-10-08-l25-runtime/profile
OLD_DIGEST = '4667891671fda00328bc61e1029c86058b1f558df4c4f4ae0922aa18ddd13317'
OLD_SOURCE = 'retained authenticated image/static evidence'

# name -> (decimal sentinel, live decimal, live hex)
FIELDS = {
    'UObjectCreateListeners':      (-1000001, 104, '0x68'),
    'UObjectDeleteListeners':      (-1000002, 120, '0x78'),
    'UObjectDeleteListenersCritical': (-1000007, None, None),   # new fact, no live value
    'PrimarySerialNumber':         (-1000003, 176, '0xb0'),
    'bShouldRecycleObjectIndices': (-1000004, 180, '0xb4'),
    'UEP_TotalSize':               (-1000005, 184, '0xb8'),
}
DEC_SLOT = {
    'UObjectCreateListeners': '<<CREATE_LISTENERS_DEC>>',
    'UObjectDeleteListeners': '<<DELETE_LISTENERS_DEC>>',
    'UObjectDeleteListenersCritical': '<<CRITICAL_SECTION_DEC>>',
    'PrimarySerialNumber': '<<PRIMARY_SERIAL_DEC>>',
    'bShouldRecycleObjectIndices': '<<BSHOULD_RECYCLE_DEC>>',
    'UEP_TotalSize': '<<FUOBJECTARRAY_TOTAL_DEC>>',
}
HEX_SLOT = {
    'UObjectCreateListeners': '<<CREATE_LISTENERS_HEX>>',
    'UObjectDeleteListeners': '<<DELETE_LISTENERS_HEX>>',
    'UObjectDeleteListenersCritical': '<<CRITICAL_SECTION_HEX>>',
    'PrimarySerialNumber': '<<PRIMARY_SERIAL_HEX>>',
    'bShouldRecycleObjectIndices': '<<BSHOULD_RECYCLE_HEX>>',
    'UEP_TotalSize': '<<FUOBJECTARRAY_TOTAL_HEX>>',
}


def die(msg):
    raise SystemExit('make_template: ' + msg)


def diff_block(rel, old_text, new_text):
    a = old_text.splitlines(keepends=True)
    b = new_text.splitlines(keepends=True)
    d = list(difflib.unified_diff(a, b, fromfile='a/' + rel, tofile='b/' + rel, n=3))
    if not d:
        die('no diff produced for ' + rel)
    return ''.join(d)


def edit_facts_json(old_text):
    p = json.loads(old_text)
    members = p['required_members']
    for m in members:
        short = m['name'].split('::')[1]
        if m['name'].startswith('FUObjectArray::') and short in FIELDS and FIELDS[short][1] is not None:
            m['offset'] = FIELDS[short][0]        # sentinel; replaced by slot after dumps
            m['source'] = '<<SOURCE_AA>>'
    p['required_members'].append({'name': 'FUObjectArray::UObjectDeleteListenersCritical',
                                  'offset': FIELDS['UObjectDeleteListenersCritical'][0],
                                  'width': None, 'source': '<<SOURCE_AA>>'})
    text = json.dumps(p, indent=2) + '\n'
    for short, (sentinel, live, _hex) in FIELDS.items():
        needle = '"offset": ' + str(sentinel)
        if needle not in text:
            die('sentinel missing for ' + short)
        text = text.replace(needle, '"offset": ' + DEC_SLOT[short])
    return text


def edit_ini_hpp(old_text):
    lines = old_text.splitlines(keepends=True)
    ini = json.loads(lines[3].rstrip(',\n '))
    assert ini.startswith('[NativeLayout]')
    checks = [
        ('UObjectCreateListeners=0x68', 'UObjectCreateListeners=' + HEX_SLOT['UObjectCreateListeners']),
        ('UObjectDeleteListeners=0x78', 'UObjectDeleteListeners=' + HEX_SLOT['UObjectDeleteListeners']
         + '\nUObjectDeleteListenersCritical=' + HEX_SLOT['UObjectDeleteListenersCritical']),
        ('PrimarySerialNumber=0xb0', 'PrimarySerialNumber=' + HEX_SLOT['PrimarySerialNumber']),
        ('bShouldRecycleObjectIndices=0xb4', 'bShouldRecycleObjectIndices=' + HEX_SLOT['bShouldRecycleObjectIndices']),
        ('UEP_TotalSize=0xb8', 'UEP_TotalSize=' + HEX_SLOT['UEP_TotalSize']),
    ]
    for old, new in checks:
        if ini.count(old) != 1:
            die('INI key not unique: ' + old)
        ini = ini.replace(old, new)
    lines[3] = json.dumps(ini) + ',\n'
    if json.loads(lines[6].rstrip(',\n ')) != OLD_DIGEST:
        die('digest line moved')
    lines[6] = '"<<PROFILE_DIGEST_NEW>>"\n'
    return ''.join(lines)


def edit_runtime_profile_hpp(old_text):
    rows = {
        'UObjectCreateListeners': '{"FUObjectArray::UObjectCreateListeners",true,104,0,"' + OLD_SOURCE + '"}',
        'UObjectDeleteListeners': '{"FUObjectArray::UObjectDeleteListeners",true,120,0,"' + OLD_SOURCE + '"}',
        'PrimarySerialNumber': '{"FUObjectArray::PrimarySerialNumber",true,176,0,"' + OLD_SOURCE + '"}',
        'bShouldRecycleObjectIndices': '{"FUObjectArray::bShouldRecycleObjectIndices",true,180,0,"' + OLD_SOURCE + '"}',
        'UEP_TotalSize': '{"FUObjectArray::UEP_TotalSize",true,184,0,"' + OLD_SOURCE + '"}',
    }
    new = old_text
    for short, row in rows.items():
        if new.count(row) != 1:
            die('RuntimeBuildProfile row not unique: ' + short)
        parts = row.rsplit(',', 1)   # keep trailing '}'
        fields = row[:-1].rsplit(',', 3)
        rebuilt = fields[0] + ',' + DEC_SLOT[short] + ',0,"<<SOURCE_AA>>"}'
        new = new.replace(row, rebuilt)
    marker = '{"FSetProperty::UEP_TotalSize",true,144,0,'
    idx = new.find(marker)
    if idx < 0:
        die('FSetProperty::UEP_TotalSize row not found')
    end = new.find('};', idx)
    if end < 0:
        die('kRequiredMembers terminator not found')
    addition = ',{"FUObjectArray::UObjectDeleteListenersCritical",true,' + DEC_SLOT['UObjectDeleteListenersCritical'] + ',0,"<<SOURCE_AA>>"}'
    new = new[:end] + addition + new[end:]
    if new.count('kDigest="' + OLD_DIGEST + '"') != 1:
        die('kDigest line not unique')
    new = new.replace('kDigest="' + OLD_DIGEST + '"', 'kDigest="<<PROFILE_DIGEST_NEW>>"')
    return new


def edit_joined_preflight_hpp(old_text):
    last_row = ' {"FSetProperty::ElementProp",Kind::Member,Disposition::Required,true,112,"startup_core_map_set_members"}\n};'
    if old_text.count(last_row) != 1:
        die('preflight last row context not found')
    new_rows = (' {"FSetProperty::ElementProp",Kind::Member,Disposition::Required,true,112,"startup_core_map_set_members"},\n'
                ' {"FUObjectArray::UObjectCreateListeners",Kind::Member,Disposition::Required,true,' + DEC_SLOT['UObjectCreateListeners'] + ',"startup_listener_registration"},\n'
                ' {"FUObjectArray::UObjectDeleteListeners",Kind::Member,Disposition::Required,true,' + DEC_SLOT['UObjectDeleteListeners'] + ',"startup_listener_registration"},\n'
                ' {"FUObjectArray::UObjectDeleteListenersCritical",Kind::Member,Disposition::Required,true,' + DEC_SLOT['UObjectDeleteListenersCritical'] + ',"startup_listener_registration"}\n'
                '};')
    new = old_text.replace(last_row, new_rows)
    comment_old = '// Prior 63 startup/menu/spawn dependencies plus l24/l25 container promotion: map/set keys are startup-required.\n'
    if new.count(comment_old) != 1:
        die('preflight comment not unique')
    new = new.replace(comment_old, comment_old.rstrip('\n') + ' AM profile-fix: FUObjectArray listener-zone keys are startup-required.\n')
    if new.count('static_assert(sizeof(kKeys)/sizeof(kKeys[0])==66);') != 1:
        die('static_assert 66 not unique')
    new = new.replace('static_assert(sizeof(kKeys)/sizeof(kKeys[0])==66);',
                      'static_assert(sizeof(kKeys)/sizeof(kKeys[0])==69);')
    return new


def edit_run_tests_py(old_text):
    old_block = ("assert p['required_members'][:69]==base['required_members'] and p['required_vtables']==base['required_vtables']\n"
                 "assert len(p['required_members'])+len(p['required_vtables'])==93+changes['new_promoted_facts']==98\n"
                 "promoted=[{k:m[k] for k in ('name','offset','width')} for m in p['required_members'][69:]]\n")
    new_block = ("# AM profile-fix (l25): FUObjectArray listener-zone offsets corrected in place from agent-AA\n"
                 "# engine-binary measurement; corrected names are excluded from the retained-93 byte-identity\n"
                 "# check; new listener facts append after the promoted container five.\n"
                 "CORRECTED={'FUObjectArray::UObjectCreateListeners','FUObjectArray::UObjectDeleteListeners','FUObjectArray::UObjectDeleteListenersCritical','FUObjectArray::PrimarySerialNumber','FUObjectArray::bShouldRecycleObjectIndices','FUObjectArray::UEP_TotalSize'}\n"
                 "retained_p=[m for m in p['required_members'][:69] if m['name'] not in CORRECTED];retained_b=[m for m in base['required_members'][:69] if m['name'] not in CORRECTED]\n"
                 "assert retained_p==retained_b and p['required_vtables']==base['required_vtables']\n"
                 "assert len(p['required_members'])+len(p['required_vtables'])==98+changes.get('listener_new_facts',0)\n"
                 "promoted=[{k:m[k] for k in ('name','offset','width')} for m in p['required_members'][69:69+changes['new_promoted_facts']]]\n")
    new = old_text
    if new.count(old_block) != 1:
        die('run_tests prior-93 block not found verbatim')
    new = new.replace(old_block, new_block)
    pairs = [
        ('assert(r.checks.size()==66&&r.required==53&&r.conditional==11&&r.deferred==2);assert(r.ok==expected);',
         'assert(r.checks.size()==69&&r.required==56&&r.conditional==11&&r.deferred==2);assert(r.ok==expected);'),
        ('std::cout<<"PASS "<<mode<<" 66 keys, 53 required, 11 conditional, 2 deferred; no mutation\\n";',
         'std::cout<<"PASS "<<mode<<" 69 keys, 56 required, 11 conditional, 2 deferred; no mutation\\n";'),
        ("'prior93_retained':True,", "'prior93_retained':'88 untouched + 5 FUObjectArray listener-zone corrected in place (AM profile-fix)',"),
    ]
    for old, repl in pairs:
        if new.count(old) != 1:
            die('run_tests marker not unique: ' + old[:60])
        new = new.replace(old, repl)
    return new


def main():
    edits = [
        ('profiles/townfall-build-facts.json', edit_facts_json),
        ('units/CompiledNativeLayoutDeclarations.hpp', edit_ini_hpp),
        ('units/RuntimeBuildProfile.hpp', edit_runtime_profile_hpp),
        ('units/JoinedLayoutPreflight.hpp', edit_joined_preflight_hpp),
        ('tests/run_tests.py', edit_run_tests_py),
    ]
    out = []
    for rel, fn in edits:
        old_text = (PROFILE / rel).read_text()
        new_text = fn(old_text)
        # sanity: every <<SLOT>> we advertise must appear, and JSON-side text must round-trip
        if rel.endswith('.json'):
            probe = new_text
            for s in DEC_SLOT.values():
                probe = probe.replace('"offset": ' + s, '"offset": 1')
            probe = probe.replace('"<<SOURCE_AA>>"', '"x"')
            json.loads(probe)  # raises if slot placement broke JSON shape
        out.append(diff_block(rel, old_text, new_text))
    (HERE / 'profile-fix.diff.tmpl').write_text(''.join(out))
    slots = sorted(set(re.findall(r'<<[A-Z_0-9]+>>', ''.join(out))))
    print(json.dumps({'template': str(HERE / 'profile-fix.diff.tmpl'),
                      'files': [rel for rel, _ in edits],
                      'slots': slots}, indent=2))


if __name__ == '__main__':
    main()
