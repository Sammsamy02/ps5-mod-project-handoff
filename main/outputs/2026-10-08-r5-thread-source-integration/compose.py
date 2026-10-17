#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,subprocess,os,struct,collections
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p);return {'path':str(p),'sha256':sha(p),'size':p.stat().st_size}
def write(n,d):(HERE/n).write_text(json.dumps(d,indent=2)+'\n')
base=json.loads((HERE/'BASE-SELECTION.json').read_text());comp=json.loads((HERE/'COMPILE-RECEIPT.json').read_text());assert comp['status']=='FRESH_COMBINED_TARGET_COMPILE_PASS'
assert sha(base['primary_receipt'])==base['primary_receipt_sha256'];assert sha(base['combined_receipt'])==base['combined_receipt_sha256'];assert sha(base['actual_link_command'])==base['actual_link_command_sha256']
previous=json.loads(Path(base['combined_receipt']).read_text());assert sha(previous['object']['path'])==previous['object']['sha256']
fresh={r['name']:r for r in comp['results']};replace={r['old_object']:r['object'] for r in comp['results'] if r['old_object']};selected=[]
for r in base['base_units']:
 assert sha(r['source'])==r['source_sha256'],r['name'];assert sha(r['object'])==r['object_sha256'],r['name']
 if r['name'] in fresh:
  n=fresh[r['name']];assert n['role']=='replacement';selected.append({k:n[k] for k in ['name','source','source_sha256','object','object_sha256','dependency','dependency_sha256','argv','LLVM_CONFIG']}|{'selection':'fresh-thread-consumer'})
 else:selected.append(r|{'selection':'retained-current-R5-ACK-pad'})
for r in comp['results']:
 assert sha(r['source'])==r['source_sha256'];assert sha(r['object'])==r['object_sha256'];assert sha(r['dependency'])==r['dependency_sha256']
 if r['role']=='new-provider':selected.append({k:r[k] for k in ['name','source','source_sha256','object','object_sha256','dependency','dependency_sha256','argv','LLVM_CONFIG']}|{'selection':'new-thread-provider'})
assert len(selected)==374
cmd=list(json.loads(Path(base['actual_link_command']).read_text())['argv']);output=HERE/'runtime-r5-stop-pad-thread.o'
cmd[cmd.index('-o')+1]=str(output);cmd=[replace.get(t,t) for t in cmd];cmd=[('-Map='+str(HERE/'runtime-r5-stop-pad-thread.map')) if t.startswith('-Map=') else t for t in cmd]
providers=[r['object'] for r in comp['results'] if r['role']=='new-provider'];at=cmd.index('-o');cmd[at:at]=providers
inputs=[t for i,t in enumerate(cmd) if t.endswith(('.o','.a')) and i!=cmd.index('-o')+1];assert len(inputs)==len(set(inputs))==391
for r in selected:assert inputs.count(r['object'])==1,r['name']
fixed=[pin(t) for t in inputs if t not in {x['object'] for x in selected}];assert len(fixed)==17
write('LINK-COMMAND.json',{'argv':cmd,'ordered_inputs':inputs,'fixed_inputs':fixed,'base_link_command':pin(base['actual_link_command']),'compile_receipt':pin(HERE/'COMPILE-RECEIPT.json'),'outer_entry_bridge_inputs':False})
x=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=240);(HERE/'link.stdout').write_text(x.stdout);(HERE/'link.stderr').write_text(x.stderr);assert x.returncode==0,x.stderr
with output.open('rb') as f:h=f.read(64)
assert h[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',h,16)[0]==1 and struct.unpack_from('<H',h,18)[0]==62
nm=Path(comp['results'][0]['argv'][0]).with_name('prospero-nm');env=dict(os.environ,LLVM_CONFIG=comp['results'][0]['LLVM_CONFIG']);sets={}
for label,obj in [('base',previous['object']['path']),('combined',str(output))]:
 p=subprocess.run([str(nm),'-u',obj],env=env,capture_output=True,text=True,check=True);(HERE/(label+'.undefined.nm')).write_text(p.stdout);sets[label]={l.split()[-1] for l in p.stdout.splitlines() if l.split()}
p=subprocess.run([str(nm),'--defined-only',str(output)],env=env,capture_output=True,text=True,check=True);(HERE/'combined.defined.nm').write_text(p.stdout);symbols=collections.Counter(l.split()[-1] for l in p.stdout.splitlines() if l.split());assert symbols['bridge_native_code_residence_required']==symbols['bridge_native_pad_code_residence_required']==1
thunks=[s for s in symbols if '5ThunkILm' in s];assert len(thunks)==256;assert not any(s.startswith('__atomic') for s in sets['combined'])
added=sorted(sets['combined']-sets['base']);removed=sorted(sets['base']-sets['combined'])
write('IMPORT-DELTA.json',{'status':'ACTUAL_UNDEFINED_CENSUS_NOT_PACKAGE_ADMISSION','baseline_object':previous['object'],'candidate_object':pin(output),'SDK_nm':str(nm),'LLVM_CONFIG':comp['results'][0]['LLVM_CONFIG'],'baseline_count':len(sets['base']),'candidate_count':len(sets['combined']),'added':added,'removed':removed,'package207_admitted':False,'outer_bridge_code_pin_selected':False})
# Pin complete actual selected .d source/header union. These are dependency paths, not extra compilations.
cache={r['name']:r for r in json.loads((HERE/'BASE-DEPENDENCIES.json').read_text())['units']};dep_paths=set();dep_receipts=[]
for r in selected:
 if r['name'] in fresh:paths=fresh[r['name']]['dependency_paths'];dep=r['dependency'];dsha=r['dependency_sha256']
 else:old=cache[r['name']];paths=old['paths'];dep=old['dependency_file'];dsha=old['dependency_sha256'];assert sha(dep)==dsha
 dep_receipts.append({'name':r['name'],'path':dep,'sha256':dsha});dep_paths.update(paths)
files=[pin(p) for p in sorted(dep_paths)];write('ACTUAL-DEPENDENCIES.json',{'status':'FULL_SELECTED_DEPENDENCY_GRAPH_PINNED','emitted_dependency_receipts':len(dep_receipts),'fresh_emitted_dependency_receipts':6,'distinct_dependency_path_pins':len(files),'dependency_receipts':dep_receipts,'files':files})
receipt={'status':'EXPERIMENTAL_FULL_R5_STOP_PAD_THREAD_LINK_PASS','scope':'Actual coherent target relocatable composition only. Import admission false; outer entry/bridge and runtime retirement integration pending. No package, promotion, native execution, menu proof or console contact.','base_selection':pin(HERE/'BASE-SELECTION.json'),'producer_selection':pin(ROOT/'outputs/2026-10-08-native-thread-name-adapter/SOURCE-SELECTION.json'),'producer_final_receipt':pin(ROOT/'outputs/2026-10-08-native-thread-name-adapter/FINAL-RECEIPT.json'),'closure':pin(HERE/'CLOSURE.json'),'compile_receipt':pin(HERE/'COMPILE-RECEIPT.json'),'link_command':pin(HERE/'LINK-COMMAND.json'),'dependency_graph':pin(HERE/'ACTUAL-DEPENDENCIES.json'),'import_delta':pin(HERE/'IMPORT-DELTA.json'),'object':pin(output),'ELF_type':'REL','ELF_machine':'x86_64','selected_units':selected,'selected_count':len(selected),'fresh_existing_units':2,'new_provider_units':4,'retained_current_units':368,'full_link_inputs':len(inputs),'fixed_link_inputs':17,'baseline_undefined_count':len(sets['base']),'candidate_undefined_count':len(sets['combined']),'actual_undefined_added':added,'actual_undefined_removed':removed,'unique_code_residence_provider':True,'unique_pad_residence_provider':True,'static_thunk_symbols':256,'Config_changed':False,'shared_public_class_definition_changed':False,'native_thread_name_overlay_selected':True,'old_BSS_authority_overlay_selected':False,'thread_import_admission':False,'thread_retire_runtime_wired':False,'outer_entry_bridge_selected':False,'console_contact':False}
write('COMPOSITION-RECEIPT.json',receipt)
print(json.dumps({k:receipt[k] for k in ['status','selected_count','fresh_existing_units','new_provider_units','full_link_inputs','baseline_undefined_count','candidate_undefined_count','actual_undefined_added','actual_undefined_removed']}|{'object_sha256':receipt['object']['sha256'],'object_bytes':receipt['object']['size'],'dependency_path_pins':len(files)}))
