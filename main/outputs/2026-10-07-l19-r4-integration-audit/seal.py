from pathlib import Path
import hashlib,json,re,struct,shlex
W=Path.cwd();A=Path(__file__).resolve().parent;O=W/'outputs/2026-10-07-l19-local-profile-r4';R3=W/'outputs/2026-10-07-l19-local-profile-r3';sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
S=O/'reports/SOURCE-SELECTION.json';F=O/'reports/FINAL-RECEIPT.json';C=W/'outputs/2026-10-07-l19-r4-full-build/parent-review/R4-SOURCE-SELECTION.compatible.json';s=json.loads(S.read_text());f=json.loads(F.read_text());c=json.loads(C.read_text())
assert sha(S)=='981c6ccf51a8310e1a9d4b93b7c0118dbe1dbddab7cd5d0339f142d49e18bd42';assert sha(C)=='5ac819c439244166e1c5f281aee19862ca2981da29f451bf76e1e8a0e644a79c'
for p,pin in s['source_files'].items():assert sha(O/p)==pin,p
for name,row in c['selected_sources'].items():assert sha(row['path'])==row['sha256'],name
assert len(s['source_files'])==71 and len(s['evidence_files'])==1572
P=O/'profiles/townfall-build-facts.json';p=json.loads(P.read_text());old=json.loads((R3/'profiles/townfall-build-facts.json').read_text());h=O/'units/RuntimeBuildProfile.hpp';provider=O/'units/CompiledNativeLayoutDeclarations.hpp';digest=hashlib.sha256(json.dumps(p,sort_keys=True,separators=(',',':')).encode()).hexdigest()
assert sha(P)==f['profile_sha256']=='0c3ca6c36d3397896e61808e8632adde08e44e5dd2eb75c71c0ca3e5fa5595c9' and digest==f['profile_digest']=='0a09270e315de7f955b2f90514881fc9c0ac2d992d57d76dc7463a04891204bc'
assert digest in h.read_text() and digest in provider.read_text()
for field in ['image','layout','axes','fname']:assert p[field]==old[field],field
names=lambda rows:{r['name']:r for r in rows};members=names(p['required_members']);virtuals=names(p['required_vtables']);before=names(old['required_members']+old['required_vtables']);after=dict(members,**virtuals)
assert len(members)==69 and len(virtuals)==7 and not p['unknown_required_static_facts'] and all(r['offset'] is not None for r in after.values())
expected={'FField::UEP_TotalSize':(0x30,None),'FFieldClass::UEP_TotalSize':(0x40,None),'UEngine::LoadMap':(0x4f8,8),'AGameModeBase::InitGameState':(0x7b8,8)}
assert {k for k in before if before[k]!=after[k]}==set(expected)
for name,(value,width) in expected.items():assert after[name]['offset']==value and after[name]['width']==width
assert sha(O/'units/UnrealInitializer.cpp')==sha(R3/'units/UnrealInitializer.cpp') and sha(O/'units/NativeLayoutBinding.cpp')==sha(R3/'units/NativeLayoutBinding.cpp')
assert sha(O/'units/NativeLayoutActiveWidths.hpp')==sha(R3/'units/NativeLayoutActiveWidths.hpp')
assert sha(O/'units/Unreal/FField.hpp')=='6891a6f15fa5124f5f0f57aafe081d501a5baec52e9001d2e3403d5a3f53ad3c' and sha(O/'units/FField.cpp')=='f56f9d407975e4335bf0157919f62cfdf5c892d1d3ca4a028f40d9a5d51b2b47'
for name in ['runtime_owner','shadow_copy.cpp']:assert '2026-10-07-l19-r3-full-build' in c['selected_sources'][name]['path']
# Independent decode of the selected-target object's actual54 public width words.
q=json.loads((O/'reports/ALL-REQUIRED-WIDTHS-GREEN.json').read_text());argv=q['argv'];obj=Path(argv[argv.index('-o')+1]);raw=obj.read_bytes();assert hashlib.sha256(raw).hexdigest()==q['object_sha256']
assert raw[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',raw,18)[0]==62
shoff=struct.unpack_from('<Q',raw,40)[0];size,num=struct.unpack_from('<HH',raw,58);secs=[struct.unpack_from('<IIQQQQIIQQ',raw,shoff+i*size) for i in range(num)];symbols={}
for sec in secs:
 if sec[1]!=2:continue
 ns=secs[sec[6]];strings=raw[ns[4]:ns[4]+ns[5]]
 for at in range(sec[4],sec[4]+sec[5],sec[9]):
  name,info,other,index,value,n=struct.unpack_from('<IBBHQQ',raw,at)
  if not index or index>=len(secs):continue
  name=strings[name:strings.index(0,name)].decode();ts=secs[index];pos=ts[4]+value-ts[3];symbols[name]=raw[pos:pos+n]
b=symbols[q['symbol']];assert b.hex()==q['symbol_bytes'];width_values=struct.unpack('<54I',b);observed=dict(zip([r['name'] for r in q['required_keys']],width_values));assert observed==q['observed_widths'] and len(observed)==54 and observed['FField::Owner']==16 and all(observed[x['name']]>0 and (x['width'] is None or observed[x['name']]>=x['width']) for x in p['required_members'] if not x['name'].endswith('::UEP_TotalSize'))
# All four actual whole target TUs/dependencies are verified without redundant recompilation.
t=json.loads((O/'reports/R4-TARGET-COMPILE.json').read_text());deps=json.loads((O/'reports/TARGET-DEPENDENCY-MANIFEST.json').read_text())['files'];assert len(deps)==1452
for path,pin in deps.items():assert sha(path)==pin,path
target_rows=[]
for row in t['commands']:
 assert row['exit']==0 and row['mandatory_dependencies_verified'];argv=row['argv'];obj=Path(argv[argv.index('-o')+1]);dep=Path(argv[argv.index('-MF')+1]);assert sha(obj)==row['object_sha256'] and sha(dep)==row['dependency_sha256']
 ds=shlex.split(dep.read_text().replace('\\\n',' ').split(':',1)[1]);assert str(O/'units/Unreal/FField.hpp') in ds
 target_rows.append({'name':row['name'],'object_sha256':sha(obj),'dependency_sha256':sha(dep),'owner_header_dependency':True})
assert {r['name'] for r in target_rows}=={'program','initializer','binder','owner'}
assert any(v=='-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(provider)+'"' for v in t['commands'][0]['argv'])
for role in ['RuntimeBuildProfile.hpp','CompiledNativeLayoutDeclarations.hpp','NativeLayoutDeclarations.hpp']:assert c['selected_sources'][role]['sha256']==sha(O/'units'/role)
# Accepted native fact and Owner audit receipts are immutable pins in the producer graph.
pins={}
for relative in ['outputs/2026-10-07-l19-r4-slot-audit/AUDIT.json','outputs/2026-10-07-l19-r4-total-audit/AUDIT.json','outputs/2026-10-07-l19-r4-owner-variant-review/FINAL-AUDIT.json']:
 path=W/relative
 if 'owner-variant-review' in relative:assert sha(path)=='a0df909b04777fadafd6f9a8e6427907e7fa2f13c342d786e60a85075b085a28'
 else:assert s['evidence_files'][relative]==sha(path)
 pins[relative]=sha(path)
for relative,pin in f['saved_external_declarations']['hashes'].items():assert sha(W/relative)==pin
assert f['saved_external_declarations']['members']==17 and f['saved_external_declarations']['vtables']==1 and not f['saved_external_declarations']['modified']
assert f['cleanup_count_means_callback_invocation_only'] and not f['native_engine_calls'] and f['Owner_native_read_bytes']==8 and f['Owner_public_binding_footprint_bytes']==16
replay=json.loads((A/'REPLAY-RECEIPT.json').read_text());assert replay['pass'] and replay['host_actual_integration_scenarios']==33 and replay['declaration_controls']==20
result={'pass':True,'findings':[],'scope':'Independent LOCAL R4 integration/source admission; no full link/console/menu proof','source_manifest_sha256':sha(C),'producer_source_selection_sha256':sha(S),'producer_final_receipt_sha256':sha(F),'profile_json_sha256':sha(P),'generated_header_sha256':sha(h),'compiled_declarations_header_sha256':sha(provider),'profile_digest':digest,'known_member_pins':69,'known_vtable_pins':7,'unknown_required_static_facts':[],'actual_target_widths_decoded':54,'Owner_public_binding_footprint_bytes':16,'Owner_internal_native_read_bytes':8,'fresh_actual_target_TUs':target_rows,'target_dependencies_verified':1452,'actual_host_integration_scenarios':33,'declaration_refusal_controls':20,'native_engine_calls':0,'cleanup_count_is_callback_invocation_only':True,'saved_external_17_1_unchanged':True,'R3_initializer_binder_width_helper_identical':True,'accepted_native_and_owner_audit_pins':pins,'totals_confidence':f['FField_totals_confidence'],'full_pipeline_link_import_checks':'parent-owned pending','audit_script_sha256':sha(__file__),'replay_receipt_sha256':sha(A/'REPLAY-RECEIPT.json')}
(A/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps({'pass':True,'findings':0,'members':69,'vtables':7,'target_widths':54,'source_manifest_sha256':sha(C)}))
