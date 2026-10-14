#!/usr/bin/env python3
"""Create a new actual-dependency-derived build plan only after ROOT-INPUTS is sealed."""
from pathlib import Path
import json,hashlib,shutil
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
BASE=ROOT/'outputs/2026-10-08-r5-thread-source-integration'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(n,d):(HERE/n).write_text(json.dumps(d,indent=2)+'\n')
inputs=json.loads((HERE/'ROOT-INPUTS.json').read_text())
for r in inputs['receipt_pins']+inputs['replacement_pins']:assert sha(r['path'])==r['sha256'],r['path']
assert sha(BASE/'SOURCE-SELECTION.json')=='45cd784b6d361679eb4525af210ee7d963abe4c7ca8a0e2e5c4b057d8edbffc6'
assert sha(BASE/'FINAL-RECEIPT.json')=='7ead4d03bd8fb36f9e79b30ef3efbe23fd0cab28dc6dc3ecbc3905e6f179d6e0'
composition=json.loads((BASE/'COMPOSITION-RECEIPT.json').read_text());base=composition['selected_units'];assert len(base)==374
for n in ['thread','objects','logs']:(HERE/n).mkdir(exist_ok=True)
for p in (BASE/'thread').glob('*'):shutil.copy2(p,HERE/'thread'/p.name)
old_hashes={p.name:sha(p) for p in (BASE/'thread').glob('*')}
for r in inputs['replacement_pins']:shutil.copy2(r['path'],HERE/'thread'/Path(r['path']).name)
changed_headers={p.name for p in (HERE/'thread').glob('*') if p.suffix in {'.h','.hpp'} and ( p.name not in old_hashes or sha(p)!=old_hashes[p.name])}
changed_sources={p.name for p in (HERE/'thread').glob('*.cpp') if p.name not in old_hashes or sha(p)!=old_hashes[p.name]}
olddeps=json.loads((BASE/'BASE-DEPENDENCIES.json').read_text())['units'];cache={r['name']:r['paths'] for r in olddeps}
for r in json.loads((BASE/'COMPILE-RECEIPT.json').read_text())['results']:cache[r['name']]=r['dependency_paths']
assert set(cache)=={r['name'] for r in base}
closure=[];units=[]
for r in base:
 matches=[p for p in cache[r['name']] if Path(p).name in changed_headers]
 direct=Path(r['source']).name in changed_sources
 fresh=bool(matches or direct)
 closure.append({'name':r['name'],'source':r['source'],'dependency_file':r['dependency'],'changed_header_dependencies':matches,'changed_source':direct,'fresh_required':fresh})
 if not fresh:continue
 src=HERE/'thread'/Path(r['source']).name if Path(r['source']).name in old_hashes or direct else Path(r['source'])
 argv=list(r['argv']);old=Path(r['source']).resolve();pos=[i for i,t in enumerate(argv) if not t.startswith('-') and (ROOT/t).resolve()==old];assert len(pos)==1,(r['name'],pos)
 argv[pos[0]]=str(src);obj=HERE/'objects'/(r['name']+'.o');dep=HERE/'objects'/(r['name']+'.d');argv[argv.index('-o')+1]=str(obj);argv[argv.index('-MF')+1]=str(dep)
 argv[1:1]=['-I',str(ROOT/'outputs/2026-10-08-r5-source-integration/include'),'-I',str(HERE/'thread'),'-I',str(ROOT/'outputs/2026-10-08-r5-source-integration/source'),'-I',str(ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include'),'-I',str(ROOT/'outputs/2026-10-07-l19-local-profile-r4/units')]
 units.append({'name':r['name'],'role':'revised-consumer','source':str(src),'source_sha256':sha(src),'old_object':r['object'],'object':str(obj),'dependency':str(dep),'LLVM_CONFIG':r['LLVM_CONFIG'],'argv':argv,'changed_header_dependencies':matches,'changed_source':direct})
write('CLOSURE.json',{'status':'ACTUAL_374_UNIT_REVISION_CLOSURE_COMPLETE','base_selection_sha256':sha(BASE/'SOURCE-SELECTION.json'),'root_inputs_sha256':sha(HERE/'ROOT-INPUTS.json'),'changed_headers':sorted(changed_headers),'changed_sources':sorted(changed_sources),'units':closure,'units_scanned':374,'fresh_units':[r['name'] for r in units],'fresh_count':len(units)})
write('COMPILE-PLAN.json',{'status':'EXPERIMENTAL_REVISION_NOT_PACKAGE_ADMITTED','root_inputs':str(HERE/'ROOT-INPUTS.json'),'root_inputs_sha256':sha(HERE/'ROOT-INPUTS.json'),'base_selection':str(BASE/'SOURCE-SELECTION.json'),'base_selection_sha256':sha(BASE/'SOURCE-SELECTION.json'),'closure_sha256':sha(HERE/'CLOSURE.json'),'units':units,'compile_count':len(units),'final_package_import_verified':False,'outer_entry_bridge_selected':False})
write('BASE-UNIT-DEPENDENCIES.json',{'scope':'Cached actual emitted .d path lists tied to preceding frozen composition','base_selection_sha256':sha(BASE/'SOURCE-SELECTION.json'),'paths':cache})
print(json.dumps({'actual_units_scanned':374,'changed_headers':sorted(changed_headers),'changed_sources':sorted(changed_sources),'fresh_units':[r['name'] for r in units]}))
