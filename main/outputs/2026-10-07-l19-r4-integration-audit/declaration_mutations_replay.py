"""Actual unchanged declaration producer: complete GREEN and focused source/width REDs."""
from pathlib import Path
import copy,hashlib,importlib.util,json,sys
O=Path('/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-07-l19-local-profile-r4');W=O.parents[1];OWN=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('producer',O/'generate_declarations.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
profile=O/'profiles/townfall-build-facts.json';p=json.loads(profile.read_text());census=O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json';c=json.loads(census.read_text())
schema=W/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include/NativeLayoutSchema.inc';s=schema.read_text();widths=c['observed_widths']
rows=[]
def test(name,call,refuse=False):
 try:r=call(); error=None
 except (ValueError,KeyError,OSError) as e:r=None;error=str(e)
 passed=(error is not None)==refuse;rows.append({'case':name,'pass':passed,'refused':error is not None,'detail':error});assert passed,(name,error)
 return r
green=test('complete_69_7',lambda:m.build(p,s,widths,True));assert green['manifest']['ready']
old=json.loads((W/'outputs/2026-10-07-l19-local-profile-r3/profiles/townfall-build-facts.json').read_text())
test('r3_four_unknowns_require_complete',lambda:m.build(old,s,widths,True),True)
names=['FField::UEP_TotalSize','FFieldClass::UEP_TotalSize','UEngine::LoadMap','AGameModeBase::InitGameState']
for name in names:
 q=copy.deepcopy(p);r=next(r for r in q['required_members']+q['required_vtables'] if r['name']==name);r.update(offset=None,width=None,source='')
 test('missing_'+name,lambda q=q:m.build(q,s,widths,True),True)
 q=copy.deepcopy(p);next(r for r in q['required_members']+q['required_vtables'] if r['name']==name)['source']=''
 test('empty_source_'+name,lambda q=q:m.build(q,s,widths,True),True)
for case,qw in [('missing_owner_width',{k:v for k,v in widths.items() if k!='FField::Owner'}),('narrow_owner_width',dict(widths,**{'FField::Owner':7}))]:
 test(case,lambda qw=qw:m.build(p,s,qw,True),True)
for name,value in [('FField::UEP_TotalSize',0x27),('FField::Owner',0x21),('UFunction::Func',0xd9)]:
 q=copy.deepcopy(p);next(r for r in q['required_members'] if r['name']==name)['offset']=value
 test('geometry_'+name,lambda q=q:m.build(q,s,widths,True),True)
base_selection=O/'reports/DECLARATION-INPUT-SELECTION.json';base_receipt=O/'reports/DECLARATION-INPUT-RECEIPT.json'
selection=json.loads(base_selection.read_text());receipt=json.loads(base_receipt.read_text())
# Each mutation is a new local file with an intentionally stale authenticated pin.
T=OWN/'declaration-drift';T.mkdir(exist_ok=True)
for label,target in [('total_audit',W/'outputs/2026-10-07-l19-r4-total-audit/AUDIT.json'),('slot_audit',W/'outputs/2026-10-07-l19-r4-slot-audit/AUDIT.json'),('total_fact_source',W/'outputs/2026-10-07-l19-r4-total-evidence/official-engine-evidence/OFFICIAL-TOTAL-FACTS.json'),('slot_fact_source',W/'outputs/2026-10-07-l19-r4-engine-slots/ENGINE-SLOT-FACTS-v2.json'),('profile_header',O/'units/RuntimeBuildProfile.hpp')]:
 bad=T/(label+'.drift');bad.write_bytes(target.read_bytes()+b'\nDRIFT\n')
 q=copy.deepcopy(selection);q['source_files']={str(O/k):v for k,v in q['source_files'].items()}
 q['evidence_files'][str(bad.relative_to(W))]=sha(target)
 sp=T/(label+'-selection.json');sp.write_text(json.dumps(q,indent=2)+'\n')
 rp=T/(label+'-receipt.json');r=copy.deepcopy(receipt);r['source_selection_sha256']=sha(sp);rp.write_text(json.dumps(r,indent=2)+'\n')
 test('authenticated_pin_drift_'+label,lambda sp=sp,rp=rp,label=label:m.generate(W,profile,sp,rp,census,schema,T/(label+'-output'),True),True)
assert all(x['pass'] for x in rows)
(OWN/'DECLARATION-MUTATIONS.json').write_text(json.dumps({'pass':True,'cases':rows,'commands':[sys.executable,str(Path(__file__))],'source_pins':{str(f):sha(f) for f in [Path(__file__),O/'generate_declarations.py',profile,census,schema,base_selection,base_receipt]},'scope':'Actual generic producer validates complete profile, all four fact sources, compiled footprints, class bounds and frozen evidence drift; no publication'},indent=2)+'\n')
print('declaration controls',len(rows),'PASS')
