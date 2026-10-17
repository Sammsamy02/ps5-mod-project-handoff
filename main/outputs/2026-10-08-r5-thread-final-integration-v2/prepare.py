#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,shutil
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];BASE=ROOT/'outputs/2026-10-08-r5-thread-source-integration';HELD=ROOT/'outputs/2026-10-08-r5-thread-final-integration'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p);return {'path':str(p),'sha256':sha(p),'size':p.stat().st_size}
def write(n,d):(HERE/n).write_text(json.dumps(d,indent=2)+'\n')
inputs=json.loads((HERE/'ROOT-INPUTS.json').read_text())
for r in inputs['receipt_pins']+inputs['replacement_pins']:assert sha(r['path'])==r['sha256'],r['path']
assert sha(BASE/'SOURCE-SELECTION.json')=='45cd784b6d361679eb4525af210ee7d963abe4c7ca8a0e2e5c4b057d8edbffc6';assert sha(HELD/'SOURCE-SELECTION.json')=='28c3d4080a796524c03033185349213f1506ae3cc513c048ac86ac24f50097ce'
base=json.loads((BASE/'COMPOSITION-RECEIPT.json').read_text())['selected_units'];cache={r['name']:r['paths'] for r in json.loads((BASE/'BASE-DEPENDENCIES.json').read_text())['units']}
for r in json.loads((BASE/'COMPILE-RECEIPT.json').read_text())['results']:cache[r['name']]=r['dependency_paths']
init=next(r for r in json.loads((HELD/'COMPILE-RECEIPT.json').read_text())['results'] if r['name']=='unreal-unrealinitializer')
for k,pkey in [('source','source_sha256'),('object','object_sha256'),('dependency','dependency_sha256')]:assert sha(init[k])==init[pkey]
cache[init['name']]=init['dependency_paths']
for i,r in enumerate(base):
 if r['name']==init['name']:base[i]={k:init[k] for k in ['name','source','source_sha256','object','object_sha256','dependency','dependency_sha256','argv','LLVM_CONFIG']}|{'selection':'retained_exact_admitted_initializer'}
assert len(base)==len(cache)==374
for n in ['thread','objects','logs']:(HERE/n).mkdir(exist_ok=True)
for p in (HELD/'thread').glob('*'):shutil.copy2(p,HERE/'thread'/p.name)
for r in inputs['replacement_pins']:shutil.copy2(r['path'],HERE/'thread'/Path(r['path']).name)
# Corrected lifetime ordering is private to this provider. All public header bytes must match the held version.
header_equivalence=[]
for p in (HELD/'thread').glob('*'):
 if p.suffix in {'.h','.hpp'}:
  assert sha(p)==sha(HERE/'thread'/p.name),p.name;header_equivalence.append({'name':p.name,'previous':pin(p),'current':pin(HERE/'thread'/p.name),'byte_identical':True})
changed_cpp={p.name for p in (HERE/'thread').glob('*.cpp') if sha(p)!=sha(HELD/'thread'/p.name)};assert changed_cpp=={'ThreadNameRuntime.cpp'},changed_cpp
closure=[];fresh=[]
for r in base:
 affected=Path(r['source']).name in changed_cpp
 closure.append({'name':r['name'],'dependency_file':r['dependency'],'dependency_sha256':sha(r['dependency']),'provider_source_changed':affected,'shared_headers_changed':False,'fresh_required':affected})
 if not affected:continue
 source=HERE/'thread/ThreadNameRuntime.cpp';argv=list(r['argv']);positions=[i for i,t in enumerate(argv) if not t.startswith('-') and (ROOT/t).resolve()==Path(r['source']).resolve()];assert len(positions)==1
 argv[positions[0]]=str(source);obj=HERE/'objects'/(r['name']+'.o');dep=HERE/'objects'/(r['name']+'.d');argv[argv.index('-o')+1]=str(obj);argv[argv.index('-MF')+1]=str(dep)
 argv[1:1]=['-I',str(ROOT/'outputs/2026-10-08-r5-source-integration/include'),'-I',str(HERE/'thread'),'-I',str(ROOT/'outputs/2026-10-08-r5-source-integration/source'),'-I',str(ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include'),'-I',str(ROOT/'outputs/2026-10-07-l19-local-profile-r4/units')]
 fresh.append({'name':r['name'],'role':'corrected-private-provider','source':str(source),'source_sha256':sha(source),'object':str(obj),'dependency':str(dep),'old_object':r['object'],'LLVM_CONFIG':r['LLVM_CONFIG'],'argv':argv})
assert len(fresh)==1
write('BASE-SELECTION.json',{'status':'CURRENT374_BASE_WITH_EXACT_ADMITTED_INITIALIZER','units':base,'false_control_base_receipt':pin(BASE/'FINAL-RECEIPT.json'),'admission_compile_receipt':pin(HELD/'COMPILE-RECEIPT.json'),'held_source_selection':pin(HELD/'SOURCE-SELECTION.json'),'retained_initializer':{k:init[k] for k in ['source','source_sha256','object','object_sha256','dependency','dependency_sha256']}})
write('BASE-DEPENDENCIES.json',{'source':str(BASE),'actual_unit_paths':cache})
write('CLOSURE.json',{'status':'ACTUAL374_COMPLETE_PROVIDER_ONLY_REVISION','units':closure,'units_scanned':374,'fresh_units':[r['name'] for r in fresh],'shared_header_equivalence':header_equivalence,'Config_changed':False,'header_changes':[],'source_changes':['ThreadNameRuntime.cpp'],'retained_initializer_source_and_deps_exact':True})
write('COMPILE-PLAN.json',{'status':'CORRECTED_ORDERING_EXPERIMENT_NO_FINAL_PACKAGE_ADMISSION','root_inputs':pin(HERE/'ROOT-INPUTS.json'),'base_selection':pin(HERE/'BASE-SELECTION.json'),'closure':pin(HERE/'CLOSURE.json'),'units':fresh,'compile_count':1,'outer_entry_bridge_selected':False})
print(json.dumps({'closure_units':374,'public_header_changes':0,'fresh_compile_count':1,'retained_admitted_initializer':True}))
