#!/usr/bin/env python3
"""Read completed local build receipts and independently hash the actual artifacts."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[3]
OWN=Path(__file__).resolve().parent.parent
def load(p): return json.loads(Path(p).read_text())
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p): return {'path':str(Path(p).resolve()),'sha256':sha(p)}
def contract(p):
    j=load(p)
    rows=j['contract']['imports']; result={}
    for r in rows:
        k=r['ImportSymbol']['Name']; assert k not in result
        result[k]={'ImportSymbol':r['ImportSymbol'],'symbol_type':r['symbol_type'],'binding':r['binding']}
    return result
current=OWN/'prx-candidate-l19-2/supplier-contract.json'
baseline=ROOT/'outputs/2026-10-07-l18-full-build/prx-candidate-l18-2/supplier-contract.json'
a,b=contract(current),contract(baseline)
delta={'scope':'Mac-only; exact typed contract comparison against saved final l18',
       'added':sorted(a.keys()-b.keys()),'removed':sorted(b.keys()-a.keys()),
       'changed_identities':sorted(k for k in a.keys()&b.keys() if a[k]!=b[k]),
       'current_count':len(a),'baseline_count':len(b),'current':pin(current),'baseline':pin(baseline)}
delta['pass']=not any(delta[k] for k in ('added','removed','changed_identities')) and len(a)==len(b)==206
assert delta['pass']
(OWN/'IMPORT-DELTA-VS-L18.json').write_text(json.dumps(delta,indent=2)+'\n')
builds=[]
for i in (1,2):
    d=OWN/f'prx-candidate-l19-{i}'; run=load(OWN/f'runs/run-l19-{i}/receipt.json')
    artifact=load(d/'artifacts-receipt.json')['artifacts']['final_prx']
    q=Path(artifact['path']); assert sha(q)==artifact['sha256'] and q.stat().st_size==artifact['bytes']
    assert q.read_bytes()[:4].hex()=='4f153d1d'
    obj=Path(run['runtime_object']); assert sha(obj)==run['runtime_sha256'] and obj.stat().st_size==run['runtime_bytes']
    gate=load(OWN/f'gate-receipt-l11-prx-candidate-l19-{i}.json')
    assert gate['failed_checks']==(['D1'] if i==1 else [])
    assert all(c['status']=='PASS' for c in gate['checks'] if c['id']!='D1')
    static=load(d/'verification-evidence/static-checks.json')
    assert static['status']=='STATIC_CHECKS_PASSED' and all(c['pass'] for c in static['checks'].values())
    profile=load(d/'package-profile.json'); assert profile['sdk_version']=='0x11000000'
    whole=load(OWN/f'receipts/whole-selected-prx-candidate-l19-{i}.json')
    assert whole['status']=='PASS-WHOLE-SELECTED-ARTIFACTS' and whole['selected_count']==369 and whole['input_hash_count']==386
    assert len(run['runtime_object_order'])==369 and len(set(run['runtime_object_order']))==369
    builds.append({'run':i,'artifact':str(q),'sha256':sha(q),'bytes':q.stat().st_size,
      'magic':q.read_bytes()[:4].hex(),'composition_sha256':sha(obj),'composition_bytes':obj.stat().st_size,
      'gate_status':gate['verdict'],'failed_checks':gate['failed_checks'],
      'static_checks':{'pass':True,'count':len(static['checks'])},'imports':len(contract(d/'supplier-contract.json')),
      'sdk':profile['sdk_version'],'selected_units':369,'inputs':386})
assert builds[0]['sha256']==builds[1]['sha256']
assert builds[0]['composition_sha256']==builds[1]['composition_sha256']
pc=load(ROOT/'outputs/2026-10-07-l19-local-profile-r2/profiles/townfall-build-facts.json')
unknown={kind:[p['name'] for p in pc[k] if p['offset'] is None] for kind,k in [('members','required_members'),('vtables','required_vtables')]}
unknown['other_static']=pc['unknown_required_static_facts']
cmp=load(OWN/'receipts/luamod-comparator.json')
assert cmp['object_sha256']==cmp['comparator']=='457ca039e51074c22bfe0519ebb1b555f6aa52666fde6301be5d2daa300a644b'
j={'scope':'Mac-only; no console contact or upload. Not menu/spawn-ready.',
   'builds':builds,'composition_deterministic':True,'prx_deterministic':True,
   'import_delta_vs_l18':delta,'unresolved_profile_facts':unknown,
   'staged_layout_files_expanded':False,'mod_and_settings_changed':False,
   'original_LuaMod_object_comparator':pin(OWN/'receipts/luamod-comparator.json'),
   'source_manifest':pin(OWN/'SOURCE-SELECTION.bound.json'),'pass':True}
(OWN/'FULL-PIPELINE-RECEIPT.json').write_text(json.dumps(j,indent=2)+'\n')
print(json.dumps({'pass':True,'builds':builds,'receipt':pin(OWN/'FULL-PIPELINE-RECEIPT.json'),'unresolved':unknown},indent=2))
