#!/usr/bin/env python3
"""verify_walkguard_composition.py — worker-launch10-walkguard-prep, 2026-10-05.

Read-only verification of the GUARD+ARMED PREPARED candidate against the
launch-10 armed run receipt / candidate and the launch-11 dry-compose
candidate. Writes a JSON receipt under this worker's dir. No build steps,
no console contact."""
import hashlib
import json
import sys
from pathlib import Path

TEAM = Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team')
MY = TEAM / 'worker-launch10-walkguard-prep'
L10_RUN = TEAM / 'worker-launch10-prebuild/ue4ss-native-supplier-composition-6-l10/run-a/receipt.json'
L10_CAND = TEAM / 'worker-launch10-prebuild/prx-candidate-composition6-l10-a'
L11_CAND = TEAM / 'worker-launch11-dry-compose/prx-candidate-composition6-drycompose-a'
A = MY / 'prx-candidate-composition6-l10wg-a'
B = MY / 'prx-candidate-composition6-l10wg-b'


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


out = {'worker': 'worker-launch10-walkguard-prep', 'date': '2026-10-05'}

# 1. composed-run unit census vs launch-10 run receipt
ra = json.loads((MY / 'ue4ss-native-supplier-composition-6-l10wg/run-a/receipt.json').read_text())
r10 = json.loads(L10_RUN.read_text())
ua, u10 = ra['selected_runtime_units'], r10['selected_runtime_units']
out['units_names_equal'] = sorted(ua) == sorted(u10)
out['units_count_both'] = (len(ua), len(u10))
src_delta = sorted(n for n in ua if ua[n]['source_sha256'] != u10[n]['source_sha256'])
obj_delta = sorted(n for n in ua if ua[n]['object_sha256'] != u10[n]['object_sha256'])
out['source_delta_units'] = src_delta
out['object_delta_units'] = obj_delta
out['initializer_source_sha_equal'] = (ua['unreal-unrealinitializer']['source_sha256']
                                       == u10['unreal-unrealinitializer']['source_sha256'])
out['initializer_object_equal_launch10'] = (ua['unreal-unrealinitializer']['object_sha256']
                                            == u10['unreal-unrealinitializer']['object_sha256'])
out['initializer_object_sha'] = ua['unreal-unrealinitializer']['object_sha256']
out['asmhelper_object_sha'] = ua['asmhelper-asmhelper']['object_sha256']
out['asmhelper_object_eq_wst_fold0'] = (out['asmhelper_object_sha']
                                        == '81dd832bd59fab43a57c0feb0c9c15b756b2e62f74e667e320e666a2c66b4bb6')
out['composed_runtime_sha'] = ra['runtime_sha256']
out['composed_runtime_bytes'] = ra['runtime_bytes']
out['composed_runtime_eq_launch11_drycompose'] = (ra['runtime_sha256']
                                                  == '49fd49e73dafb145df77140f83259fe0e390b8efcebe5fe604b7be4d47068e09')
out['retained_units_replaced'] = ra.get('retained_units_replaced')
out['und_count'] = ra['undefined_delta']['current_count']
out['und_added_removed'] = (ra['undefined_delta']['added'], ra['undefined_delta']['removed'])
out['und_identical_to_launch10'] = (ra['undefined_delta']['current']
                                    == r10['undefined_delta']['current'])

# 2. candidate build objects vs launch-10 candidate
bld_a = sorted(p.name for p in (A / 'build').iterdir() if p.is_file())
bld_10 = sorted(p.name for p in (L10_CAND / 'build').iterdir() if p.is_file())
out['build_object_names_equal'] = bld_a == bld_10
same, diff = [], []
for n in bld_a:
    if n in bld_10:
        (same if sha(A / 'build' / n) == sha(L10_CAND / 'build' / n) else diff).append(n)
out['build_objects_identical_vs_launch10'] = same
out['build_objects_differing_vs_launch10'] = diff

# 3. supplier contract: 196 import names list-equal
ca = json.loads((A / 'supplier-contract.json').read_text())
c10 = json.loads((L10_CAND / 'supplier-contract.json').read_text())
names_a = [i['ImportSymbol']['Name'] for i in ca['contract']['imports']]
names_10 = [i['ImportSymbol']['Name'] for i in c10['contract']['imports']]
out['contract_counts'] = (ca['counts'], c10['counts'])
out['contract_import_names_equal'] = names_a == names_10
out['contract_import_count'] = len(names_a)

# 4. final artifacts (launch-10 vs launch-11 drycompose vs mine)
arts = {}
for label, cand in (('l10', L10_CAND), ('l11_drycompose', L11_CAND), ('mine_a', A), ('mine_b', B)):
    if not cand.is_dir():
        continue
    e = {}
    for art, rel in (('bare_original', 'bare-original/modbridge_supplier_composition.prx'),
                     ('bare_sdk', 'bare-sdk/modbridge_supplier_composition.prx'),
                     ('wrapped_fself', 'modbridge_supplier_composition.prx')):
        p = cand / rel
        if p.is_file():
            e[art] = {'sha256': sha(p), 'bytes': p.stat().st_size}
    e['combined_supplier_o'] = sha(cand / 'build/combined-supplier.o')
    arts[label] = e
out['artifacts'] = arts
if B.is_dir():
    out['d1_candidate_a_eq_b'] = all(
        arts['mine_a'][k] == arts['mine_b'][k] for k in arts['mine_a'])

# 5. run-b composed object identity (fresh Phase A reproduction)
rb_path = MY / 'ue4ss-native-supplier-composition-6-l10wg/run-b/receipt.json'
if rb_path.is_file():
    rb = json.loads(rb_path.read_text())
    out['run_b_composed_sha'] = rb['runtime_sha256']
    out['fresh_compose_reproduced'] = rb['runtime_sha256'] == ra['runtime_sha256']
    out['run_b_object_delta_units_vs_l10'] = sorted(
        n for n in rb['selected_runtime_units']
        if rb['selected_runtime_units'][n]['object_sha256'] != u10[n]['object_sha256'])

dest = MY / 'verification-receipt.json'
dest.write_text(json.dumps(out, indent=1) + '\n')
print(json.dumps(out, indent=1))
