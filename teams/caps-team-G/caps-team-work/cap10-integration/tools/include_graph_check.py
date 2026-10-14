"""Offline replica of build_variants.compile_unit's include-graph rule for the
#10 units (Team G). A dependency row of a unit passes when it is (1) in the
template unit's R15e dependency set, or (2) under the l26-runtime tree (HERE;
scratch paths are mapped back to their live location), or (3) an approved
(basename, sha256) identity pinned in R15e's SOURCE-SELECTION source_pins, or
(4) an explicit exemption row (unit, resolved path, sha256) from --exempt.
Prints counts; exit 1 on any unapproved row, 2 on zero rows checked.
usage: include_graph_check.py --deps-dir DIR --scratch-outputs DIR [--exempt FILE] UNIT[=TEMPLATE] ...
"""
import argparse, hashlib, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from target_compile import find_r15e, W, sha
ap = argparse.ArgumentParser(); ap.add_argument('--deps-dir', required=True); ap.add_argument('--scratch-outputs', required=True)
ap.add_argument('--exempt'); ap.add_argument('units', nargs='+'); a = ap.parse_args()
r15e = find_r15e(); HERE = W / 'outputs/2026-10-08-l26-runtime'; scratch = Path(a.scratch_outputs).resolve()
sel = json.loads((r15e / 'SOURCE-SELECTION.json').read_text())
approved = {(Path(p['path']).name, p['sha256']) for p in sel['source_pins'] if Path(p['path']).suffix in ('.h', '.hpp', '.inc', '.inl')}
exempt = json.loads(Path(a.exempt).read_text())['rows'] if a.exempt else []
def deps(p):
    t = Path(p).read_text().replace('\\\n', ' ')
    return [Path(x) for x in t.split(':', 1)[1].split()]
total = bad = 0
for spec in a.units:
    unit, _, tmpl = spec.partition('='); tmpl = tmpl or 'program'
    old = {str(p.resolve()) for p in deps(r15e / 'pass1/objects' / f'{tmpl}.d')}
    rows = deps(Path(a.deps_dir) / f'{unit}.d'); counts = {'base': 0, 'here': 0, 'approved': 0, 'exempt': 0}; extra = []
    for p in rows:
        rp = p.resolve(); total += 1
        live = W / 'outputs' / rp.relative_to(scratch) if rp.is_relative_to(scratch) else rp
        if str(live) in old: counts['base'] += 1
        elif live.is_relative_to(HERE): counts['here'] += 1
        elif (rp.name, sha(rp)) in approved: counts['approved'] += 1
        elif any(e['unit'] == unit and e['path'] == str(rp) and e['sha256'] == sha(rp) for e in exempt): counts['exempt'] += 1
        else: extra.append(f'{rp} sha={sha(rp)}')
    bad += len(extra)
    print(f'  {unit} (template {tmpl}): {len(rows)} rows; ' + ', '.join(f'{k} {v}' for k, v in counts.items()) + f'; UNAPPROVED {len(extra)}')
    for x in extra: print('     NEW ' + x)
print(f'include_graph_check items_checked={total} unapproved={bad}')
sys.exit(2 if total == 0 else (1 if bad else 0))
