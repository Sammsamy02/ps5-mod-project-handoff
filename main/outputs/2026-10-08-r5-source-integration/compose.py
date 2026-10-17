#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,subprocess,os,shlex,struct,collections
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
plan=json.loads((HERE/'COMPILE-PLAN.json').read_text());comp=json.loads((HERE/'COMPILE-RECEIPT.json').read_text());assert comp['status']=='FRESH_TARGET_COMPILE_PASS'
base_file=Path(plan['base_r4_receipt']);base=json.loads(base_file.read_text());closure=json.loads((HERE/'INITIAL-CLOSURE.json').read_text());fresh={r['name']:r for r in comp['results']};by_old={r['old_object']:r['object'] for r in comp['results'] if 'old_object' in r}
selected=[];retained=[]
for c in closure['units']:
 old=c['selected'];assert sha(old['object'])==old['object_sha256'],c['name']
 if c['dependent_on_changed_header']:
  row=fresh[c['name']];assert sha(row['source'])==row['source_sha256'];assert sha(row['object'])==row['object_sha256'];assert row['selected_changed_header']==[str(HERE/'include/Unreal/UFunctionStructs.hpp')];selected.append({'name':c['name'],'source':row['source'],'source_sha256':row['source_sha256'],'object':row['object'],'object_sha256':row['object_sha256'],'selection':'fresh-header-consumer'})
 else:
  assert sha(old['source'])==old['source_sha256'],c['name'];assert not c['header_paths'];assert not any(t.endswith('/Unreal/UFunctionStructs.hpp') for t in shlex.split(Path(c['dependency_path']).read_text().replace('\\\n',' ').split(':',1)[1]))
  row={'name':c['name'],'source':old['source'],'source_sha256':old['source_sha256'],'object':old['object'],'object_sha256':old['object_sha256'],'selection':'retained-header-excluded','dependency':c['dependency_path'],'dependency_sha256':sha(c['dependency_path'])};selected.append(row);retained.append(row)
provider=fresh['native-script-hook-thunks'];selected.append({'name':provider['name'],'source':provider['source'],'source_sha256':provider['source_sha256'],'object':provider['object'],'object_sha256':provider['object_sha256'],'selection':'new-provider'})
cmd=list(plan['link_template']['argv']);output=HERE/'runtime-r5.o';cmd[cmd.index('-o')+1]=str(output)
cmd=[by_old.get(t,t) for t in cmd];location=next(i for i,t in enumerate(cmd) if t==fresh['unreal-flat-ufunctionstructs']['object']);cmd.insert(location+1,provider['object'])
inputs=[t for i,t in enumerate(cmd) if t.endswith(('.o','.a')) and i!=cmd.index('-o')+1]
assert len(inputs)==387 and len(set(inputs))==387
for row in selected:assert inputs.count(row['object'])==1,row['name']
external=[{'path':t,'sha256':sha(t),'size':Path(t).stat().st_size} for t in inputs if t not in {x['object'] for x in selected}]
(HERE/'LINK-COMMAND.json').write_text(json.dumps({'argv':cmd,'base_argv_sha256':sha(HERE/'COMPILE-PLAN.json'),'scope':'Actual full R4 relocatable composition,164 fresh consumers+1provider; unchanged root stop sources pending separate supply','inputs':inputs,'fixed_inputs':external},indent=2)+'\n')
r=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=240);(HERE/'link.stdout').write_text(r.stdout);(HERE/'link.stderr').write_text(r.stderr)
if r.returncode:raise RuntimeError(r.stderr)
header=output.read_bytes()[:64];assert header[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',header,16)[0]==1 and struct.unpack_from('<H',header,18)[0]==62
sdk=Path(fresh['unreal-ufunction']['argv'][0]);nm=sdk.with_name('prospero-nm');env=dict(os.environ,LLVM_CONFIG=fresh['unreal-ufunction']['LLVM_CONFIG'])
sets={}
for label,o in [('r4',Path(base['runtime_object'])),('r5',output)]:
 x=subprocess.run([str(nm),'-u',str(o)],capture_output=True,text=True,env=env,check=True);(HERE/(label+'.undefined.nm')).write_text(x.stdout);sets[label]={l.split()[-1] for l in x.stdout.splitlines() if l.split()}
added=sorted(sets['r5']-sets['r4']);removed=sorted(sets['r4']-sets['r5']);assert not any(t.startswith('__atomic') for t in sets['r5'])
x=subprocess.run([str(nm),'--defined-only',str(output)],capture_output=True,text=True,env=env,check=True);(HERE/'r5.defined.nm').write_text(x.stdout);symbols=[l.split()[-1] for l in x.stdout.splitlines() if l.split()]
assert symbols.count('bridge_native_code_residence_required')==1
thunks=[n for n in symbols if '5ThunkILm' in n];assert len(thunks)==256
receipt={'status':'FULL_RELOCATABLE_R5_COMPOSITION_PASS','scope':'Isolated target composition only; not package/import admission, native execution, thread owner certification or console readiness','base_receipt':str(base_file),'base_receipt_sha256':sha(base_file),'base_runtime':base['runtime_object'],'base_runtime_sha256':sha(base['runtime_object']),'compile_receipt_sha256':sha(HERE/'COMPILE-RECEIPT.json'),'link_command_sha256':sha(HERE/'LINK-COMMAND.json'),'object':str(output),'object_sha256':sha(output),'object_bytes':output.stat().st_size,'ELF_type':'REL','ELF_machine':'x86_64','selected_units':selected,'selected_count':370,'fresh_existing_units':164,'new_provider_units':1,'retained_header_excluded_units':205,'full_link_inputs':387,'baseline_undefined_count':len(sets['r4']),'candidate_undefined_count':len(sets['r5']),'actual_undefined_added':added,'actual_undefined_removed':removed,'new_C_provider_definitions':{'bridge_native_code_residence_required':1},'static_thunk_symbols':256,'atomic_runtime_imports':[],'thread_authority_overlay_selected':False,'root_stop_sources_replaced':False,'console_contact':False}
(HERE/'COMPOSITION-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps({k:receipt[k] for k in ['status','selected_count','fresh_existing_units','full_link_inputs','object_sha256','baseline_undefined_count','candidate_undefined_count','actual_undefined_added','actual_undefined_removed']}))
