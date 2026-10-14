"""Verify final READY input identities without creating GO or invoking any builder."""
from pathlib import Path
import hashlib,json,sys
sys.dont_write_bytecode=True
O=Path(__file__).resolve().parent;W=O.parents[1];P=W/'outputs/2026-10-07-l19-r4-full-build'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
final=P/'parent-review/EXECUTION-ADMISSION.FINAL.json';inputs=P/'parent-review/FINAL-EXECUTION-INPUTS.json'
assert sha(final)=='9face0624aa37cb143a069c7337644a43e3a41eafa179fdaddaf039428b9dfe8'
assert sha(inputs)=='cfb88b986ec97b6d67cd80b32fc2bb32fdff0a1f21926272c3b058fe7448246b'
a=read(final);v5=read(P/'parent-review/EXECUTION-ADMISSION.FINAL-INPUTS-PENDING.v5.json');base=read(O/'AUDIT.json')
assert base['pass'] and not base['findings'] and a['schema']==1 and a['readiness']=='READY'
for key in ['execution_source_pins','tool_pins','tool_aliases','profile_digest','source_manifest','profile_json','profile_header','generated_header','affected_recipes','input_substitutions','linker','nm','additional_source_pins']:
 assert a[key]==v5[key],key
verified={}
for key in ['ready_prep','edition','source_manifest','xom_audit','profile_json','profile_header','generated_header','final_review','integration_manifest','affected_recipes','input_substitutions','linker','nm']:
 r=a[key];p=Path(r['path']);assert p.is_absolute() and p.is_file() and not p.is_symlink() and sha(p)==r['sha256'],key;verified[key]=r
for p,h in a['execution_source_pins'].items():assert sha(p)==h,p
prep=read(a['ready_prep']['path']);edition=read(a['edition']['path']);records=read(a['source_manifest']['path'])['selected_sources']
assert prep['readiness']=='READY' and prep['source_manifest']==a['source_manifest'] and prep['xom_audit_receipt']==a['xom_audit']
assert edition['l19_scope']['selected_sources']==records
oldedition=read(P/'EDITIONS-l19.json')
assert edition['comparators']==oldedition['comparators']
assert set(edition['admissions'])==set(oldedition['admissions'])
edition_updates=[]
for name,current in edition['admissions'].items():
 old=oldedition['admissions'][name]
 if current==old:continue
 assert name=='initializer_fold'
 assert {k:v for k,v in current.items() if k not in ['files','evidence']}=={k:v for k,v in old.items() if k not in ['files','evidence']}
 assert current['evidence']==a['source_manifest']['path']
 for path,h in current['files'].items():assert sha(path)==h,path
 edition_updates.append({'admission':name,'changed_fields':['files','evidence'],'status_and_gate_fields_preserved':True,'source_manifest':a['source_manifest']})
review=read(a['final_review']['path']);assert review['pass'] is True and review['findings']==[]
for field,key in [('source_manifest_sha256','source_manifest'),('profile_json_sha256','profile_json'),('generated_header_sha256','generated_header')]:assert review[field]==a[key]['sha256'],field
profile=read(a['profile_json']['path'])
assert hashlib.sha256(json.dumps(profile,sort_keys=True,separators=(',',':')).encode()).hexdigest()==a['profile_digest']
assert a['profile_digest'] in Path(a['profile_header']['path']).read_text() and a['profile_digest'] in Path(a['generated_header']['path']).read_text()
sys.path.insert(0,str(P/'execution'))
from admission import Admission,Refusal,validate_complete_profile
validate_complete_profile(profile)
alias=Admission();alias.data=a;alias.pins={'linker':Path(a['linker']['path'])};alias.tool_pins=a['tool_pins'];alias.validate_tool_aliases()
for path,h in a['tool_pins'].items():assert not Path(path).is_symlink() and sha(path)==h,path
try:Admission.load(final,sha(final),None,None)
except Refusal as e:assert 'GO' in str(e)
else:raise AssertionError('Missing GO accepted')
ancillary=P/'parent-review/INHERITED-ANCILLARY-RECEIPTS.json'
assert sha(ancillary)=='bf75487ecca47fcc43efdbcf32c3f001d15e4e7f73a6b53984c6b269d594be01'
extra=read(ancillary);assert extra['pass'] and extra['gate_or_expected_pin_changes'] is False
for name,r in extra['files'].items():assert sha(r['source'])==r['sha256']==sha(r['destination']),name
result={'pass':True,'findings':[],'final_admission':{'path':str(final),'sha256':sha(final)},'final_input_receipt':{'path':str(inputs),'sha256':sha(inputs)},'verified_named_pins':verified,'all_current_code_pins':a['execution_source_pins'],'tool_pins':a['tool_pins'],'tool_aliases':a['tool_aliases'],'profile_canonical_digest':a['profile_digest'],'inherited_edition_admission_gate_fields_and_comparators_preserved':True,'explicit_initializer_fold_source_pin_updates':edition_updates,'inherited_ancillary_receipts':{'receipt':{'path':str(ancillary),'sha256':sha(ancillary)},'files':extra['files']},'real_loader_missing_GO_refused':True,'GO_created':False,'compiler_linker_packager_invoked':False,'scope':'Final READY metadata/code/input compatibility verified; parent must create and bind actual GO'}
(O/'FINAL-ADMISSION-VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n')
base['final_ready_admission']=result['final_admission'];base['final_input_receipt']=result['final_input_receipt'];base['final_ready_metadata_verified_without_GO']=True;base['final_admission_verification_sha256']=sha(O/'FINAL-ADMISSION-VERIFICATION.json')
base['scope']='Read-only final frozen execution/control and READY metadata audit; accepted for parent GO decision, no GO or full build created by auditor'
base['audit_source_files']={str(Path(__file__)):sha(__file__),str(O/'audit_execution.py'):sha(O/'audit_execution.py')}
(O/'FINAL-AUDIT.json').write_text(json.dumps(base,indent=2)+'\n')
print(json.dumps({'pass':True,'final_admission_sha256':sha(final),'audit_sha256':sha(O/'FINAL-AUDIT.json')}))
