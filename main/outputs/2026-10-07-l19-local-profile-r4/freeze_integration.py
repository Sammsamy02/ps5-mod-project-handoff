"""Seal the tested isolated R4 inputs; preserve every prior frozen slice."""
from pathlib import Path
import hashlib,json,re,sys
O=Path(__file__).resolve().parent;W=O.parents[1];R3=W/'outputs/2026-10-07-l19-local-profile-r3'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
write=lambda p,x:Path(p).write_text(json.dumps(x,indent=2)+'\n')
assert not (O/'reports/FINAL-RECEIPT.json').exists(),'Preserving the sealed receipt'
p=json.loads((O/'profiles/townfall-build-facts.json').read_text())
digest=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
assert digest==re.search(r'kDigest="([0-9a-f]+)"',(O/'units/RuntimeBuildProfile.hpp').read_text())[1]
assert len(p['required_members'])==69 and len(p['required_vtables'])==7
assert all(r['offset'] is not None for r in p['required_members']+p['required_vtables']) and not p['unknown_required_static_facts']
names=['AUDITED-PROFILE-GREEN.json','COMPILED-DECLARATIONS.json','R4-INTEGRATION.json','R3-DECLARATION-BASELINE.json','R4-DECLARATION-MUTATIONS.json','R4-TARGET-COMPILE.json']
receipts={n:json.loads((O/'reports'/n).read_text()) for n in names}
assert all(x['pass'] is True for x in receipts.values())
census=json.loads((O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text())
assert census['exit']==0 and not census['missing_provider'] and not census['narrower_than_measured']
assert census['accepted_owner_header_dependency_verified'] and len(census['observed_widths'])==54 and census['observed_widths']['FField::Owner']==16
original=json.loads((R3/'reports/SOURCE-SELECTION.json').read_text())
assert sha(R3/'reports/SOURCE-SELECTION.json')=='cc0043df2f68d8ca1a61b49a5cef6d34e2a598fd49ded91e47cf3640d5f22337'
assert all(sha(R3/path)==pin for path,pin in original['source_files'].items())
generic=json.loads((O/'reports/ACCEPTED-GENERIC-INPUTS.json').read_text())
assert all(sha(O/path)==row['sha256']==sha(row['origin']) for path,row in generic['files'].items())
sources={}
def source(f):sources[str(f.relative_to(O))]=sha(f)
for directory in ['units','profiles']:
 for f in (O/directory).rglob('*'):
  if f.is_file() and f.suffix in ('.cpp','.hpp','.h','.inc','.py','.json'):source(f)
for n in ['generate_declarations.py','prepare_declarations.py','freeze_integration.py','validate_integration.py','tests/required_width_census.py','tests/r4_integration.py','tests/r4_target_compile.py','tests/r4_declaration_mutations.py','tests/r3_declaration_baseline.py']:
 source(O/n)
for directory in ['tests/r4-host','tests/r3-original-pair','tests/required-width-target']:
 for f in (O/directory).rglob('*'):
  if f.is_file() and f.suffix in ('.cpp','.hpp','.inc'):source(f)
for n in ['generated/known-declarations/MemberVariableLayout.ini','generated/known-declarations/VTableLayout.ini','generated/stageable-known-subset/MemberVariableLayout.ini','generated/stageable-known-subset/VTableLayout.ini','generated/UNRESOLVED.json','reports/DECLARATION-INPUT-SELECTION.json','reports/DECLARATION-INPUT-RECEIPT.json']:source(O/n)
evidence=dict(original['evidence_files'])
for path,pin in original['source_files'].items():evidence[str((R3/path).relative_to(W))]=pin
for path,pin in p['evidence']['sources'].items():assert sha(W/path)==pin;evidence[path]=pin
for row in p['evidence']['r4_accepted_native_fact_audits']:
 # The promoted profile stores exact audit and accepted source identities.
 for key in ['audit','accepted_fact_source']:
  if key in row:
   data=row[key]; evidence[str(Path(data['path']).relative_to(W))]=data['sha256']
for n in names+['ALL-REQUIRED-WIDTHS-GREEN.json','BASELINE.json','ACCEPTED-GENERIC-INPUTS.json']:
 f=O/'reports'/n;evidence[str(f.relative_to(W))]=sha(f)
for receipt in receipts.values():
 for path,pin in receipt.get('source_pins',receipt.get('source_hashes',{})).items():
  f=Path(path)
  if f.is_file() and not f.is_relative_to(O):evidence[str(f.relative_to(W))]=pin
for row in generic['files'].values():evidence[str(Path(row['origin']).relative_to(W))]=row['sha256']
for row in receipts['AUDITED-PROFILE-GREEN.json']['promoted_facts']:
 for k,h in [('audit_path','audit_sha256'),('fact_receipt_path','fact_receipt_sha256')]:evidence[str(Path(row[k]).relative_to(W))]=row[h]
for path,pin in census['source_hashes'].items():
 f=Path(path)
 if not f.is_relative_to(O):evidence[str(f.relative_to(W))]=pin
target=receipts['R4-TARGET-COMPILE.json']
dep_manifest=O/'reports/TARGET-DEPENDENCY-MANIFEST.json'
write(dep_manifest,{'scope':'Actual four whole-target-TU depfiles, frozen as compiled','files':target['dependency_pins'],'count':len(target['dependency_pins'])})
evidence[str(dep_manifest.relative_to(W))]=sha(dep_manifest)
# Exact target dependencies are evidence, including the separate owner overlay.
for path,pin in target['dependency_pins'].items():
 f=Path(path)
 if not f.is_relative_to(O):evidence[str(f.relative_to(W))]=pin
artifacts={}
for directory in ['tests/r4-target','tests/required-width-target','tests/r4-host/build','tests/r3-original-pair']:
 for f in (O/directory).rglob('*'):
  if f.is_file() and (f.suffix in ('.o','.d') or f.name.endswith('-san') or f.parent.name=='build'):
   artifacts[str(f.relative_to(O))]=sha(f)
for relative,pin in artifacts.items():evidence[str((O/relative).relative_to(W))]=pin
saved=W/'outputs/2026-10-04-zcode-team/worker-emit-final-pair/emission-final'
saved_pins={str((saved/n).relative_to(W)):sha(saved/n) for n in ['MemberVariableLayout.ini','VTableLayout.ini']}
evidence.update(saved_pins)
image=W/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
assert sha(image)==p['image']['sha256'];evidence[str(image.relative_to(W))]=p['image']['sha256']
for path,pin in evidence.items():assert sha(W/path)==pin,('evidence drift',path)
handoff={
 'scope':'Execution-role mapping is owned by the parent full pipeline, distinct from this source freeze',
 'profile':str(O/'profiles/townfall-build-facts.json'),'profile_sha256':sha(O/'profiles/townfall-build-facts.json'),'profile_digest':digest,
 'changed_or_new_source_roles':{n:{'path':str(O/'units'/f),'sha256':sha(O/'units'/f)} for n,f in [('program','UE4SSProgram.cpp'),('unreal_initializer','UnrealInitializer.cpp'),('native_layoutbinding','NativeLayoutBinding.cpp'),('unreal_ffield','FField.cpp')]},
 'flat_helper_headers':{f:{'path':str(O/'units'/f),'sha256':sha(O/'units'/f)} for f in ['RuntimeBuildProfile.hpp','CompiledNativeLayoutDeclarations.hpp','NativeLayoutDeclarations.hpp','NativeLayoutActiveWidths.hpp','NativeLayoutBinding.hpp','BuildFactsChecks.hpp']},
 'owner_header_separate_admission':{'tested_path':str(O/'units/Unreal/FField.hpp'),'sha256':sha(O/'units/Unreal/FField.hpp'),'required_pipeline_dependency_path':str(W/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include/Unreal/FField.hpp'),'must_not_be_flat_selected_sources_helper':True},
 'retain_actual_R3_source_roles':['runtime_owner','shadow_copy'],'unchanged_R3_role_paths_must_not_be_replaced_by_test_alias_copies':True,
 'Program_macro_required':'-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(O/'units/CompiledNativeLayoutDeclarations.hpp')+'"',
 'provider_header_presence_alone_selects_it':False,'declared_maps':69,'declared_vtables':7,'public_owner_binding_bytes':16,'owner_internal_native_read_bytes':8,
 'original_external_17_1_declarations_modified':False,'PC_mod_or_settings_modified':False,'full_pipeline_pending':True}
write(O/'reports/PIPELINE-HANDOFF.json',handoff);source(O/'reports/PIPELINE-HANDOFF.json')
selection=O/'reports/SOURCE-SELECTION.json'
write(selection,{'scope':'R4 LOCAL ONLY tested inputs and retained origins. No console/network/game execution; R3 and prior receipts immutable. Target source-role mapping is a separate parent admission.', 'source_files':dict(sorted(sources.items())),'evidence_files':dict(sorted(evidence.items())),'baseline_selection_sha256':sha(R3/'reports/SOURCE-SELECTION.json'),'owner_header_separate_admission':handoff['owner_header_separate_admission'],'unchanged_R3_original_source_pins_verified':len(original['source_files'])})
receipt={'scope':'Focused actual local parser/binder/Program/preflight integration and four target compiles; no runtime/menu/spawn proof','pass':True,'source_selection_sha256':sha(selection),'profile_sha256':sha(O/'profiles/townfall-build-facts.json'),'profile_digest':digest,'source_files':len(sources),'external_evidence_pins':len(evidence),'required_member_pins':69,'known_member_pins':69,'required_vtable_pins':7,'known_vtable_pins':7,'unknown_required_static_facts':[],'remaining_unknown_members':[],'remaining_unknown_vtables':[],'required_target_widths':54,'required_target_missing_widths':[],'required_target_narrower_than_measured':[],'actual_integration_scenarios':receipts['R4-INTEGRATION.json']['matrix_cases'],'actual_R3_original_pair_baseline_scenarios':1,'declaration_source_and_geometry_controls':len(receipts['R4-DECLARATION-MUTATIONS.json']['cases']),'target_translation_units':4,'target_dependency_files':len(target['dependency_pins']),'Owner_native_read_bytes':8,'Owner_public_binding_footprint_bytes':16,'width_provider_changed':False,'FField_totals_confidence':p['evidence']['r4_total_confidence'],'totals_direct_native_allocation_measurement':False,'live_size_or_image_proof':False,'host_positive_publications':3,'native_engine_calls':0,'refusals_before_publication_and_consumption':True,'cleanup_count_means_callback_invocation_only':True,'original_R3_source_pins_preserved':True,'saved_external_declarations':{'members':17,'vtables':1,'modified':False,'selected_by_compiled_provider':False,'hashes':saved_pins},'new_complete_declarations':{'members':69,'vtables':7,'selected_only_by_macro':True,'external_fallback_when_selected':False},'PC_mod_or_settings_modified':False,'target_artifact_hashes':artifacts,'full_pipeline_owned_by_parent':True,'linked_import_verification':'Parent full pipeline pending','receipt_hashes':{n:sha(O/'reports'/n) for n in names+['ALL-REQUIRED-WIDTHS-GREEN.json','TARGET-DEPENDENCY-MANIFEST.json','PIPELINE-HANDOFF.json','DECLARATION-INPUT-SELECTION.json','DECLARATION-INPUT-RECEIPT.json']}}
write(O/'reports/FINAL-RECEIPT.json',receipt)
print(json.dumps({'pass':True,'selection':sha(selection),'profile':receipt['profile_sha256'],'digest':digest,'final':sha(O/'reports/FINAL-RECEIPT.json'),'source_files':len(sources),'evidence_files':len(evidence)}))
