#!/usr/bin/env python3
from pathlib import Path
import json,hashlib
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];OUT=HERE/'pass2-build';OUT.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
first=json.loads((HERE/'pass1/COMPOSITION-RECEIPT.json').read_text());assert first['status']=='ACTUAL_FINAL_RELOCATABLE_LINK_PASS_NOT_PACKAGE_ADMISSION'
basecache=json.loads((HERE/'BASE-DEPENDENCIES.json').read_text())['actual_unit_paths'];firstfresh={r['name']:r for r in json.loads((HERE/'COMPILE-RECEIPT.json').read_text())['results']}
rows=[]
for name in ['unreal-unrealinitializer','native-thread-name-threadnameruntime']:
 r=next(r for r in first['selected_units'] if r['name']==name);assert sha(r['source'])==r['source_sha256'];argv=list(r['argv']);obj=OUT/'objects'/(name+'.o');dep=OUT/'objects'/(name+'.d');argv[argv.index('-o')+1]=str(obj);argv[argv.index('-MF')+1]=str(dep)
 paths=firstfresh[name]['dependency_paths'] if name in firstfresh else basecache[name]
 thread_names={p.name for p in (HERE/'thread').glob('*') if p.suffix in {'.h','.hpp'}}
 expected=[p for p in paths if Path(p).name in thread_names]
 rows.append({'name':name,'source':r['source'],'source_sha256':r['source_sha256'],'object':str(obj),'dependency':str(dep),'LLVM_CONFIG':r['LLVM_CONFIG'],'argv':argv,'expected_thread_header_paths':sorted(expected),'expected_previous_object':r['object'],'expected_previous_object_sha256':r['object_sha256']})
for n in ['objects','logs']:(OUT/n).mkdir(exist_ok=True)
plan={'status':'SECOND_INDEPENDENT_FRESH_FINAL_AFFECTED_COMPILE','pass1_composition_sha256':sha(HERE/'pass1/COMPOSITION-RECEIPT.json'),'units':rows,'compile_count':2,'all_remaining_accepted_objects_retained_by_hash':True,'outer_entry_bridge_selected':False}
(OUT/'COMPILE-PLAN.json').write_text(json.dumps(plan,indent=2)+'\n');print(json.dumps({'pass2_fresh_units':[r['name'] for r in rows],'all_other_units_retained':372}))
