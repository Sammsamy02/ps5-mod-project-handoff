#!/usr/bin/env python3
from pathlib import Path
import json, hashlib, shutil
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
PRODUCER=ROOT/'outputs/2026-10-08-native-thread-name-adapter'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def write(n,d):(HERE/n).write_text(json.dumps(d,indent=2)+'\n')
expected={'SOURCE-SELECTION.json':'f6cf5ae0b15abd73070f464de69935fa2412711f6a61f277ae6492d27bd7921e','FINAL-RECEIPT.json':'7fc33711974f740c5cd0dfb47d780448d13d48a92f59f858d89bc65ce65dca01','HANDOFF.json':'ab3c0e94d8e8c42483c2638e035e2b65652017af48ada007109eb88b1aaa0b26'}
for n,h in expected.items():assert sha(PRODUCER/n)==h,n
manifest=json.loads((PRODUCER/'SOURCE-SELECTION.json').read_text());handoff=json.loads((PRODUCER/'HANDOFF.json').read_text())
for r in manifest['owned_files']:assert sha(r['path'])==r['sha256'],r['path']
assert handoff['public_config_header_changed'] is False
assert sha(manifest['public_config']['path'])==manifest['public_config']['sha256']
base=json.loads((HERE/'BASE-SELECTION.json').read_text());deps=json.loads((HERE/'BASE-DEPENDENCIES.json').read_text());assert len(base['base_units'])==len(deps['units'])==370
by={r['name']:r for r in base['base_units']};depby={r['name']:r for r in deps['units']}
for r in deps['units']:assert sha(r['dependency_file'])==r['dependency_sha256'],r['name']
for n in ['thread','objects','logs']:(HERE/n).mkdir(exist_ok=True)
headers=sorted((PRODUCER/'candidate').glob('*.hpp'))
providers=[Path(x) for x in handoff['new_providers']]
replacements=handoff['source_replacements'];copies=headers+providers+[Path(x['candidate']['path']) for x in replacements]
for p in copies:shutil.copy2(p,HERE/'thread'/p.name);assert sha(p)==sha(HERE/'thread'/p.name)
# No pre-existing unit uses the new public headers. Shared Config/class declarations are unchanged.
header_names={p.name for p in headers};closure=[]
for r in base['base_units']:
 matches=[x for x in depby[r['name']]['paths'] if Path(x).name in header_names]
 closure.append({'name':r['name'],'dependency_file':depby[r['name']]['dependency_file'],'dependency_sha256':depby[r['name']]['dependency_sha256'],'matching_existing_headers':matches,'shared_config_present':manifest['public_config']['path'] in depby[r['name']]['paths']})
assert not [x for x in closure if x['matching_existing_headers']], 'Unexpected existing provider/header requires wider closure'
# Replace by actual source hash, not historical pathname alone.
rows=[]
for replacement in replacements:
 original=replacement['original'];candidate=replacement['candidate'];matches=[r for r in base['base_units'] if r['source_sha256']==original['sha256']]
 assert len(matches)==1,(original,len(matches))
 r=matches[0];assert sha(r['source'])==original['sha256'];rows.append((r['name'],r['argv'],r['LLVM_CONFIG'],r['source'],HERE/'thread'/Path(candidate['path']).name,r['object'],'replacement'))
recipes={}
for t in handoff['target_tus']:
 for r in json.loads(Path(t['command_receipt']).read_text()):recipes[(Path(r['argv'][r['argv'].index('-o')+1]).stem)]=r
for p in providers:
 r=recipes[p.stem];rows.append(('native-thread-name-'+p.stem.lower(),r['argv'],r['LLVM_CONFIG'],str(p),HERE/'thread'/p.name,None,'new-provider'))
units=[]
for name,original_argv,llvm,old_source,source,old_object,role in rows:
 argv=list(original_argv);source_matches=[i for i,t in enumerate(argv) if not t.startswith('-') and str((ROOT/t).resolve())==str(Path(old_source).resolve())]
 assert len(source_matches)==1,(name,source_matches)
 argv[source_matches[0]]=str(source)
 obj=HERE/'objects'/(name+'.o');dep=HERE/'objects'/(name+'.d')
 argv[argv.index('-o')+1]=str(obj);argv[argv.index('-MF')+1]=str(dep)
 prefix=['-I',str(ROOT/'outputs/2026-10-08-r5-source-integration/include'),'-I',str(HERE/'thread'),'-I',str(ROOT/'outputs/2026-10-08-r5-source-integration/source'),'-I',str(ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include'),'-I',str(ROOT/'outputs/2026-10-07-l19-local-profile-r4/units')]
 argv[1:1]=prefix
 units.append({'name':name,'role':role,'source':str(source),'source_sha256':sha(source),'original_source':old_source,'old_object':old_object,'object':str(obj),'dependency':str(dep),'LLVM_CONFIG':llvm,'argv':argv})
write('CLOSURE.json',{'status':'ACTUAL_370_UNIT_CLOSURE_COMPLETE','scope':'New thread headers have no existing consumers; Config and original shared class headers unchanged. Only two edited consumers plus four new providers. Existing R5 header consumers remain fresh from the primary freeze.','producer_selection':str(PRODUCER/'SOURCE-SELECTION.json'),'producer_selection_sha256':expected['SOURCE-SELECTION.json'],'base_selection_sha256':sha(HERE/'BASE-SELECTION.json'),'base_dependencies_sha256':sha(HERE/'BASE-DEPENDENCIES.json'),'units':closure,'units_scanned':370,'unchanged_config_consumers':sum(x['shared_config_present'] for x in closure),'fresh_replacements':[x['name'] for x in units if x['role']=='replacement'],'new_providers':[x['name'] for x in units if x['role']=='new-provider'],'Config_changed':False,'outer_bridge_selected':False})
write('COMPILE-PLAN.json',{'status':'EXPERIMENTAL_NOT_ADMITTED','producer_pins':{str(PRODUCER/n):h for n,h in expected.items()},'base_selection_sha256':sha(HERE/'BASE-SELECTION.json'),'closure_sha256':sha(HERE/'CLOSURE.json'),'units':units,'compile_count':len(units),'import_admission':False,'lifecycle_retire_wired':False,'outer_bridge_selected':False})
print(json.dumps({'closure_units':370,'Config_changed':False,'Config_consumers_unchanged':sum(x['shared_config_present'] for x in closure),'fresh_compiles':len(units),'new_providers':4,'imports':'HOLD'}))
