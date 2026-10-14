"""Bounded R3/attempt5 ODR provenance and body diagnosis, not a gate waiver."""
from pathlib import Path
import hashlib,json
O=Path(__file__).resolve().parent;W=O.parents[2];P=W/'outputs/2026-10-07-l19-r4-full-build';R=W/'outputs/2026-10-07-l19-r3-full-build'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
read=lambda p:json.loads(Path(p).read_text())
prior=R/'runs/run-l19-2/receipt.json';current=P/'runs/run-l19-1/receipt.json'
old=read(prior);new=read(current)
old_odr_path=R/'receipts/odr-l11.json';new_odr_path=P/'receipts/odr-l11.json'
old_odr=read(old_odr_path);new_odr=read(new_odr_path)
assert old_odr['result']=='PASS' and new_odr['result']=='FAIL'
assert old_odr['body_hashes']==new_odr['body_hashes']
matched=[];changed=[];checks=[]
for name in old['freshly_compiled_units']:
 b=old['selected_runtime_units'][name];c=new['selected_runtime_units'][name]
 assert sha(c['object'])==c['object_sha256']
 assert sha(c['source'])==c['source_sha256']
 if c['object_sha256']==b['object_sha256']:
  assert sha(b['object'])==b['object_sha256'] and sha(b['source'])==b['source_sha256']
  assert c['source_sha256']==b['source_sha256'],name
  matched.append(name)
 else:changed.append(name)
 checks.append({'unit':name,'actual_current_object':c['object'],'current_object_sha256':c['object_sha256'],'R3_object':b['object'],'R3_object_sha256':b['object_sha256'],'actual_current_source':c['source'],'source_sha256':c['source_sha256'],'R3_source_sha256':b['source_sha256'],'R3_object_byte_identical':c['object_sha256']==b['object_sha256'],'actual_fresh_in_attempt5':name in new['freshly_compiled_units']})
assert len(matched)==24 and set(changed)=={'program','unreal-unrealinitializer'}
assert len(new['freshly_compiled_units'])==3 and set(changed)<=set(new['freshly_compiled_units'])
assert set(matched)&set(new['freshly_compiled_units'])=={'native-layoutbinding'}
assert len(set(matched)|set(new['freshly_compiled_units']))==26
failed={k:v for k,v in new_odr['checks'].items() if not v['ok']}
assert set(failed)=={'A3_validate_no_retained_emitter_besides_program','B4a_offset_sensitive_retained_bodies_identical','C1_make_absent_from_retained','C2_nlf_token_absent_from_retained','D1_d2_changed_ctors_confined','E1_retained_npc_definers_subset_program'}
for name in ['A2_validate_all_emitter_bodies_identical','A4_validate_composed_single_and_body_equal','C3_native_mod_source_all_bodies_identical','C5_nms_composed_single_and_body_equal','C6_make_composed_equals_fresh_loadercontext_bytes']:
 assert old_odr['checks'][name]['ok'] and new_odr['checks'][name]['ok']
fields=['status','runtime_compilation_units','runtime_object_order','retained_units_replaced','host_receipt','host_receipt_sha256','rust_receipt','rust_receipt_sha256','base_receipt','base_receipt_sha256']
for k in fields:assert new[k]==old[k],k
package=read(P/'prx-candidate-l19-1/build-receipt.json')
assert not package.get('gaps')
artifact=read(P/'prx-candidate-l19-1/artifacts-receipt.json')
assert {'final_prx','bare_original','bare_sdk','bridge_object'}<=set(artifact['artifacts'])
diagnosis={'diagnosis_supported':True,'acceptance_pending':True,'hypothesis':'Path-prefix classification treats exact unchanged R3 current-vintage providers as retained; freshness receipt list is not read by the checker','inherited_classifier_source':{'path':str(P/'check_odr_emitters_l19.py'),'sha256':sha(P/'check_odr_emitters_l19.py'),'fresh_set_expression':'p.startswith(str(rundir) + "/")'},'fresh_receipt_list_used_by_classifier':False,'R3_fresh26':old['freshly_compiled_units'],'actual_R4_fresh3':new['freshly_compiled_units'],'matched_R3_object_and_source_providers':24,'additional_retained_path_providers':23,'matched_already_actual_fresh':['native-layoutbinding'],'expected_provenance_classification_union':26,'object_and_source_checks':checks,'changed_from_R3_actual_fresh':['program','unreal-unrealinitializer'],'unchanged_critical_emitter_body_hashes':new_odr['body_hashes'],'raw_attempt5_failures':failed,'inherited_receipt_fields_preserved':fields,'first_actual_package_gaps_empty':True,'artifact_schema_required_fields_present':True,'scope_limit':'Provenance/body diagnosis only; no classification adapter, full gate, source policy relaxation or complete runtime proof','source_pins':{str(p):sha(p) for p in [Path(__file__),prior,current,old_odr_path,new_odr_path,P/'check_odr_emitters_l19.py',P/'prx-candidate-l19-1/build-receipt.json',P/'prx-candidate-l19-1/artifacts-receipt.json']},'compile_link_package_restart_console':False}
(O/'DIAGNOSIS.json').write_text(json.dumps(diagnosis,indent=2)+'\n')
print(json.dumps({'diagnosis_supported':True,'matched_objects_and_sources':24,'additional_retained_paths':23,'actual_fresh':3,'union':26,'critical_body_hashes_unchanged':True,'sha256':sha(O/'DIAGNOSIS.json')}))
