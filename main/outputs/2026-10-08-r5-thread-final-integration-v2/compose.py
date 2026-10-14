#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,subprocess,os,struct,collections,sys
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];BASE=ROOT/'outputs/2026-10-08-r5-thread-source-integration'
passno=int(sys.argv[1]) if len(sys.argv)>1 else 1;assert passno in (1,2)
OUT=HERE/('pass'+str(passno));OUT.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p);return {'path':str(p),'sha256':sha(p),'size':p.stat().st_size}
def write(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n')
base=json.loads((HERE/'BASE-SELECTION.json').read_text());units=base['units'];authoritative={r['name']:r for r in json.loads((BASE/'ACTUAL-DEPENDENCIES.json').read_text())['dependency_receipts']};
for u in units:
 if u['name']!='unreal-unrealinitializer':
  proof=authoritative[u['name']];assert proof['path']==u['dependency'] and sha(u['dependency'])==proof['sha256'];u['dependency_sha256']=proof['sha256']
compfile=HERE/'COMPILE-RECEIPT.json' if passno==1 else HERE/'pass2-build/COMPILE-RECEIPT.json';comp=json.loads(compfile.read_text());assert comp['status']=='FRESH_COMBINED_TARGET_COMPILE_PASS'
fresh={r['name']:r for r in comp['results']};selected=[]
for r in units:
 assert sha(r['source'])==r['source_sha256'],r['name'];assert sha(r['object'])==r['object_sha256'],r['name']
 if r['name'] in fresh:
  u=fresh[r['name']];assert sha(u['source'])==u['source_sha256'] and sha(u['object'])==u['object_sha256'] and sha(u['dependency'])==u['dependency_sha256'];selected.append({k:u[k] for k in ['name','source','source_sha256','object','object_sha256','dependency','dependency_sha256','argv','LLVM_CONFIG']}|{'selection':'fresh_final_affected_TU'})
 else:selected.append(r|{'selection':'retained_exact_accepted_or_admitted_TU'})
assert len(selected)==374
old=json.loads((BASE/'COMPOSITION-RECEIPT.json').read_text());bynew={r['name']:r for r in selected};replace={r['object']:bynew[r['name']]['object'] for r in old['selected_units']}
cmd=list(json.loads((BASE/'LINK-COMMAND.json').read_text())['argv']);obj=OUT/'runtime-r5-final.o';cmd[cmd.index('-o')+1]=str(obj);cmd=[replace.get(t,t) for t in cmd];cmd=[('-Map='+str(OUT/'runtime.map')) if t.startswith('-Map=') else t for t in cmd]
inputs=[t for i,t in enumerate(cmd) if t.endswith(('.o','.a')) and i!=cmd.index('-o')+1];assert len(inputs)==len(set(inputs))==391
for r in selected:assert inputs.count(r['object'])==1
fixed=[pin(p) for p in inputs if p not in {r['object'] for r in selected}];assert len(fixed)==17
write('LINK-COMMAND.json',{'argv':cmd,'ordered_inputs':inputs,'fixed_inputs':fixed,'compile_receipt':pin(compfile),'base_link_command':pin(BASE/'LINK-COMMAND.json'),'root_inputs':pin(HERE/'ROOT-INPUTS.json'),'pass':passno})
if '--resume-post-link' in sys.argv:
 proof=json.loads((HERE/'POST-LINK-METADATA-RECOVERY.json').read_text());assert passno==1 and sha(OUT/'LINK-COMMAND.json')==proof['original_link_command_sha256'] and sha(obj)==proof['linked_output']['sha256']
else:
 r=subprocess.run(cmd,cwd=ROOT,capture_output=True,text=True,timeout=240);(OUT/'link.stdout').write_text(r.stdout);(OUT/'link.stderr').write_text(r.stderr);assert r.returncode==0,r.stderr
with obj.open('rb') as f:h=f.read(64)
assert h[:6]==b'\x7fELF\x02\x01' and struct.unpack_from('<H',h,16)[0]==1 and struct.unpack_from('<H',h,18)[0]==62
nm=Path(comp['results'][0]['argv'][0]).with_name('prospero-nm');env=dict(os.environ,LLVM_CONFIG=comp['results'][0]['LLVM_CONFIG']);sets={}
for label,path in [('false_control',old['object']['path']),('final',str(obj))]:
 r=subprocess.run([str(nm),'-u',path],env=env,capture_output=True,text=True,check=True);(OUT/(label+'.undefined.nm')).write_text(r.stdout);sets[label]={l.split()[-1] for l in r.stdout.splitlines() if l.split()}
r=subprocess.run([str(nm),'--defined-only',str(obj)],env=env,capture_output=True,text=True,check=True);(OUT/'final.defined.nm').write_text(r.stdout);symbols=collections.Counter(l.split()[-1] for l in r.stdout.splitlines() if l.split())
exports=['bridge_native_code_residence_required','bridge_native_pad_code_residence_required','bridge_native_thread_authority_retire']
for n in exports:assert symbols[n]==1,n
assert len([n for n in symbols if '5ThunkILm' in n])==256;assert not any(n.startswith('__atomic') for n in sets['final'])
added=sorted(sets['final']-sets['false_control']);removed=sorted(sets['false_control']-sets['final']);assert added==['bridge_native_thread_import_ready'] and not removed
# No new system UND vs false-control; the sole additional symbol is the outer packaging-owned provider.
write('IMPORT-DELTA.json',{'status':'ACTUAL_UND_CENSUS_NOT_PRX_IMPORT_ADMISSION','baseline_count':len(sets['false_control']),'candidate_count':len(sets['final']),'added':added,'removed':removed,'new_system_symbols_vs_false_control':[],'packaging_owned_undefined':['bridge_native_thread_import_ready'],'getter_system_symbol':'scePthreadGetthreadid','final_PRX_NID_identity_verified':False,'SDK_nm':str(nm),'LLVM_CONFIG':comp['results'][0]['LLVM_CONFIG']})
cache=json.loads((HERE/'BASE-DEPENDENCIES.json').read_text())['actual_unit_paths'];dep_paths=set();dep_receipts=[]
for u in selected:
 if u['name'] in fresh:paths=fresh[u['name']]['dependency_paths']
 else:paths=cache[u['name']]
 assert sha(u['dependency'])==u.get('dependency_sha256',sha(u['dependency']))
 dep_receipts.append({'name':u['name'],'path':u['dependency'],'sha256':sha(u['dependency'])});dep_paths.update(paths)
write('ACTUAL-DEPENDENCIES.json',{'status':'COMPLETE_ACTUAL_SELECTED_DEPENDENCY_PINS','emitted_dependency_receipts':374,'fresh_emitted_dependency_receipts':len(fresh),'distinct_dependency_path_pins':len(dep_paths),'dependency_receipts':dep_receipts,'files':[pin(p) for p in sorted(dep_paths)]})
receipt={'status':'ACTUAL_FINAL_RELOCATABLE_LINK_PASS_NOT_PACKAGE_ADMISSION','pass':passno,'scope':'Actual coherent target relocatable link. Corrected retirement ordering source selected; source review/native teardown/import admission are separate root gates. Outer entry/bridge not in this composition. No native/menu/console claim.','base_selection':pin(HERE/'BASE-SELECTION.json'),'root_inputs':pin(HERE/'ROOT-INPUTS.json'),'closure':pin(HERE/'CLOSURE.json'),'compile_receipt':pin(compfile),'link_command':pin(OUT/'LINK-COMMAND.json'),'dependency_graph':pin(OUT/'ACTUAL-DEPENDENCIES.json'),'import_delta':pin(OUT/'IMPORT-DELTA.json'),'object':pin(obj),'ELF_type':'REL','ELF_machine':'x86_64','selected_units':selected,'selected_count':374,'full_link_inputs':391,'fresh_current_pass':len(fresh),'retained_current_units':374-len(fresh),'baseline_undefined_count':len(sets['false_control']),'candidate_undefined_count':len(sets['final']),'actual_undefined_added':added,'actual_undefined_removed':removed,'packaging_owned_undefined':['bridge_native_thread_import_ready'],'unique_C_exports':{n:1 for n in exports},'static_thunk_symbols':256,'outer_entry_bridge_selected':False,'final_package_import_identity_verified':False,'native_execution':False,'menu_success':False,'console_contact':False}
write('COMPOSITION-RECEIPT.json',receipt)
print(json.dumps({'pass':passno,'status':receipt['status'],'selected':374,'inputs':391,'fresh_compiles':len(fresh),'UND':len(sets['final']),'UND_added':added,'object_sha256':receipt['object']['sha256'],'object_bytes':receipt['object']['size']}))
