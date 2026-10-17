#!/usr/bin/env python3
"""Instantiate + apply the AM profile-fix template (fill-in-the-blank mechanics).

Takes agent-AA's measured FUObjectArray offsets as a small JSON file, substitutes
them into profile-fix.diff.tmpl, applies the patch to a profile root via
/usr/bin/patch (clean-apply only, no fuzz/rejects), then recomputes the profile
digest chain and rewrites CHANGES.json pin rows so build_variants.py's G2 gates
see a consistent profile. Decimal values are the single authority; the INI hex
slots are derived (hex(dec)) so the two spellings can never diverge.

Values file schema (see values.example.json):
  {
    "UObjectCreateListeners": 104,            // decimal, required
    "UObjectDeleteListeners": 120,            // decimal, required
    "UObjectDeleteListenersCritical": 136,    // decimal, required (drop path in PROCEDURE.md if absent)
    "PrimarySerialNumber": 176,               // decimal, required (adjacent, shifted)
    "bShouldRecycleObjectIndices": 180,       // decimal, required (adjacent, shifted)
    "UEP_TotalSize": 184,                     // decimal, required (adjacent, shifted)
    "source": "AA engine-binary measurement: <receipt path> sha256 <hex>",
    "provenance": { ...free-form receipt facts copied into CHANGES.json... }
  }

Usage:
  python3 instantiate_profile_fix.py --values aa-values.json --root <profile-root> [--remap-from <live-profile-root>] [--dry-run] [--emit-patch PATH]

--root defaults to the LIVE l25 profile; touching the live root requires
--allow-live. Sandbox/validation runs pass a copy as --root (plus --remap-from so
CHANGES.json candidate paths are rewritten to the sandbox). Never edits anything
outside --root (except writing the emitted patch file).
"""
from pathlib import Path
import argparse, configparser, hashlib, json, subprocess, sys, tempfile

HERE = Path(__file__).resolve().parent
LIVE_ROOT = HERE.parents[0] / 'profile'      # outputs/2026-10-08-l25-runtime/profile
TEMPLATE = HERE / 'profile-fix.diff.tmpl'
DEC_FIELDS = ('UObjectCreateListeners', 'UObjectDeleteListeners', 'UObjectDeleteListenersCritical',
              'PrimarySerialNumber', 'bShouldRecycleObjectIndices', 'UEP_TotalSize')
SLOT_DEC = {
    'UObjectCreateListeners': '<<CREATE_LISTENERS_DEC>>',
    'UObjectDeleteListeners': '<<DELETE_LISTENERS_DEC>>',
    'UObjectDeleteListenersCritical': '<<CRITICAL_SECTION_DEC>>',
    'PrimarySerialNumber': '<<PRIMARY_SERIAL_DEC>>',
    'bShouldRecycleObjectIndices': '<<BSHOULD_RECYCLE_DEC>>',
    'UEP_TotalSize': '<<FUOBJECTARRAY_TOTAL_DEC>>',
}
SLOT_HEX = {
    'UObjectCreateListeners': '<<CREATE_LISTENERS_HEX>>',
    'UObjectDeleteListeners': '<<DELETE_LISTENERS_HEX>>',
    'UObjectDeleteListenersCritical': '<<CRITICAL_SECTION_HEX>>',
    'PrimarySerialNumber': '<<PRIMARY_SERIAL_HEX>>',
    'bShouldRecycleObjectIndices': '<<BSHOULD_RECYCLE_HEX>>',
    'UEP_TotalSize': '<<FUOBJECTARRAY_TOTAL_HEX>>',
}
CORRECTED_NAMES = ['FUObjectArray::UObjectCreateListeners', 'FUObjectArray::UObjectDeleteListeners',
                   'FUObjectArray::PrimarySerialNumber', 'FUObjectArray::bShouldRecycleObjectIndices',
                   'FUObjectArray::UEP_TotalSize']
NEW_NAMES = ['FUObjectArray::UObjectDeleteListenersCritical']
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()


def die(msg):
    raise SystemExit('instantiate_profile_fix: ' + msg)


def load_values(path):
    v = json.loads(Path(path).read_text())
    missing = [f for f in DEC_FIELDS + ('source',) if f not in v]
    if missing:
        die('values file missing fields: ' + repr(missing))
    for f in DEC_FIELDS:
        if not isinstance(v[f], int) or not (0 <= v[f] <= 0x7fffffff):
            die('field ' + f + ' must be an int in [0,0x7fffffff], got ' + repr(v[f]))
    total = v['UEP_TotalSize']
    over = [f for f in DEC_FIELDS if f != 'UEP_TotalSize' and v[f] >= total]
    if over:
        die('members at/after UEP_TotalSize (INI guard would refuse): ' + repr(over))
    if '<<' in json.dumps(v) or '>>' in json.dumps(v):
        die('values contain slot markers')
    return v


def build_patch(values):
    text = TEMPLATE.read_text()
    for f in DEC_FIELDS:
        text = text.replace(SLOT_DEC[f], str(values[f]))
        text = text.replace(SLOT_HEX[f], hex(values[f]))
    text = text.replace('<<SOURCE_AA>>', values['source'])
    leftover = [s for s in ('<<CREATE', '<<DELETE', '<<CRITICAL', '<<PRIMARY', '<<BSHOULD',
                            '<<FUOBJECTARRAY', '<<SOURCE_AA') if s in text]
    if leftover:
        die('unresolved slots after substitution: ' + repr(leftover))
    return text  # <<PROFILE_DIGEST_NEW>> intentionally remains


def apply_patch(root, patch_text):
    with tempfile.NamedTemporaryFile('w', suffix='.patch', delete=False) as tf:
        tf.write(patch_text)
        patch_path = tf.name
    r = subprocess.run(['/usr/bin/patch', '--forward', '--no-backup-if-mismatch',
                        '-i', patch_path, '-d', str(root), '-p1'],
                       capture_output=True, text=True)
    log = (r.stdout or '') + (r.stderr or '')
    rej = list(root.rglob('*.rej'))
    if r.returncode != 0 or rej or 'fuzz' in log.lower():
        die('patch did not apply cleanly (rebase template via make_template.py needed):\n' + log[-2000:])
    Path(patch_path).unlink()
    return log


def fill_digest(root):
    p = json.loads((root / 'profiles/townfall-build-facts.json').read_text())
    digest = hashlib.sha256(json.dumps(p, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    for rel in ('units/CompiledNativeLayoutDeclarations.hpp', 'units/RuntimeBuildProfile.hpp'):
        f = root / rel
        t = f.read_text()
        if t.count('<<PROFILE_DIGEST_NEW>>') != 1:
            die('digest slot not unique in ' + rel)
        f.write_text(t.replace('<<PROFILE_DIGEST_NEW>>', digest))
    return p, digest


def rewrite_changes(root, values, digest, remap_from=None):
    f = root / 'CHANGES.json'
    d = json.loads(f.read_text())
    d['profile_digest'] = digest
    d['required_members'] = 74 + len(NEW_NAMES)
    d['prior_facts_retained'] = 88  # 93 retained facts minus the 5 corrected in place
    d['listener_corrected_facts'] = CORRECTED_NAMES
    d['listener_new_facts'] = len(NEW_NAMES)
    d['listener_fix_provenance'] = values.get('provenance', {'source': values['source']})
    d['joined_counts'] = {'total': 69, 'startup_required': 56, 'conditional_maps': 11, 'runtime_deferred': 2}
    d['scope'] += (' AM profile-fix: FUObjectArray listener-zone offsets (UObjectCreateListeners/'
                   'UObjectDeleteListeners/new UObjectDeleteListenersCritical plus shifted PrimarySerialNumber/'
                   'bShouldRecycleObjectIndices/UEP_TotalSize) corrected from agent-AA engine-binary measurement.')
    d['regenerate'] = ('SUPERSEDED-BY-AM-PROFILE-FIX: do NOT re-run generate_overlay.py after the listener '
                       'correction (it regenerates from l23 and would erase it). Instantiation: python3 '
                       + str(TEMPLATE.parents[0] / 'instantiate_profile_fix.py')
                       + ' --values <aa-values.json> --root <this profile dir>')
    rf = str(Path(remap_from).resolve()) if remap_from is not None else None
    for row in d['files']:
        cand = Path(row['candidate'])
        if rf is not None:
            if str(cand).startswith(rf + '/'):
                cand = Path(str(cand).replace(rf, str(root), 1))
            row['candidate'] = str(cand)
            # originals always stay the frozen l23 paths; only candidates remap
        if not cand.exists():
            die('candidate missing: ' + str(cand))
        row['candidate_sha256'] = sha(cand)
    if rf is not None:
        d['include_directories'] = [x.replace(rf, str(root)) for x in d['include_directories']]
    f.write_text(json.dumps(d, indent=2) + '\n')
    return d


def validate(root):
    p = json.loads((root / 'profiles/townfall-build-facts.json').read_text())  # JSON validity
    digest = hashlib.sha256(json.dumps(p, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    names = [m['name'] for m in p['required_members']]
    if len(names) != len(set(names)):
        die('duplicate required_members after patch')
    ini_hpp = (root / 'units/CompiledNativeLayoutDeclarations.hpp').read_text()
    rbp = (root / 'units/RuntimeBuildProfile.hpp').read_text()
    changes = json.loads((root / 'CHANGES.json').read_text())
    for label, text in (('INI hpp', ini_hpp), ('RuntimeBuildProfile', rbp), ('CHANGES', json.dumps(changes))):
        if digest not in text:
            die('digest missing from ' + label)
    # INI structural guard (mirrors run_tests.py NativeLayoutBinding::Prepare rule)
    strings = [json.loads(x.rstrip(',\n ')) for x in ini_hpp.splitlines() if x.lstrip().startswith('"')]
    c = configparser.ConfigParser()
    c.optionxform = str
    c.read_string(strings[0])
    for section in c.sections():
        if section == 'NativeLayout':
            continue
        total = int(c.get(section, 'UEP_TotalSize'), 0)
        if total <= 0:
            die('section without positive UEP_TotalSize: ' + section)
        for key, v in c[section].items():
            if key != 'UEP_TotalSize' and int(v, 0) >= total:
                die('member outside class size: ' + section + '::' + key)
    for key in ('UObjectCreateListeners', 'UObjectDeleteListeners', 'UObjectDeleteListenersCritical'):
        if not c.has_option('FUObjectArray', key):
            die('FUObjectArray INI missing ' + key)
    pre = (root / 'units/JoinedLayoutPreflight.hpp').read_text()
    rows = [l for l in pre.splitlines() if l.startswith(' {"')]
    if len(rows) != 69:
        die('joined preflight rows == %d, expected 69' % len(rows))
    if 'static_assert(sizeof(kKeys)/sizeof(kKeys[0])==69);' not in pre:
        die('joined preflight static_assert not 69')
    rt = (root / 'tests/run_tests.py').read_text()
    if 'CORRECTED={' not in rt or 'r.checks.size()==69' not in rt:
        die('run_tests.py listener-fix hunks missing')
    return {'required_members': len(names), 'joined_rows': len(rows), 'digest': digest}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--values', required=True)
    ap.add_argument('--root', default=str(LIVE_ROOT))
    ap.add_argument('--remap-from', default=None, help='rewrite CHANGES candidate paths from this root to --root (sandbox mode)')
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--allow-live', action='store_true')
    ap.add_argument('--emit-patch', default=None)
    args = ap.parse_args()
    root = Path(args.root).resolve()
    values = load_values(args.values)
    patch_text = build_patch(values)
    if args.emit_patch:
        Path(args.emit_patch).write_text(patch_text)
    if args.dry_run:
        print(json.dumps({'dry_run': True, 'root': str(root), 'patch': args.emit_patch or '(not saved)'}, indent=2))
        return
    if root == LIVE_ROOT and not args.allow_live:
        die('refusing to touch the live profile without --allow-live')
    log = apply_patch(root, patch_text)
    _p, digest = fill_digest(root)
    rewrite_changes(root, values, digest, remap_from=args.remap_from)
    summary = validate(root)
    summary.update({'root': str(root), 'patch_log_tail': log[-300:],
                    'new_hashes': {Path(r['candidate']).name: r['candidate_sha256']
                                   for r in json.loads((root / 'CHANGES.json').read_text())['files']}})
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()
