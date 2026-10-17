#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,subprocess,os
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1];CANDIDATE=HERE/'candidate';OUT=HERE/'target';OUT.mkdir(exist_ok=True)
recipe_file=ROOT/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json'
recipes=json.loads(recipe_file.read_text())['recipes']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
rows=[]
for source,key in [('UFunctionStructs.cpp','unreal-flat-ufunctionstructs'),('UFunction.cpp','unreal-ufunction'),('ScriptHookThunks.cpp','unreal-flat-ufunctionstructs')]:
 recipe=recipes[key];command=list(recipe['pass1_argv']);command[1:1]=['-I',str(CANDIDATE/'include'),'-I',str(CANDIDATE)]
 for flag,value in [('-c',str(CANDIDATE/source)),('-o',str(OUT/(Path(source).stem+'.o'))),('-MF',str(OUT/(Path(source).stem+'.d')))]:
  if flag not in command:raise RuntimeError('expected flag '+flag)
  command[command.index(flag)+1]=value
 env=dict(os.environ,LLVM_CONFIG=recipe['LLVM_CONFIG'])
 r=subprocess.run(command,capture_output=True,text=True,env=env,timeout=120)
 (OUT/(Path(source).stem+'.stdout')).write_text(r.stdout);(OUT/(Path(source).stem+'.stderr')).write_text(r.stderr)
 row={'source':source,'source_sha256':sha(CANDIDATE/source),'recipe_role':key,'command':command,'LLVM_CONFIG':recipe['LLVM_CONFIG'],'returncode':r.returncode}
 if r.returncode==0:row.update(object_sha256=sha(OUT/(Path(source).stem+'.o')),dependency_sha256=sha(OUT/(Path(source).stem+'.d')))
 rows.append(row);print(json.dumps({'source':source,'returncode':r.returncode}),flush=True)
receipt={'status':'PASS' if all(x['returncode']==0 for x in rows) else 'FAIL','scope':'Three fresh whole SDK target TUs with actual selected headers; compilation only, not linked/packaged or native engine execution','recipe_file':str(recipe_file),'recipe_sha256':sha(recipe_file),'candidate_pins':{str(p.relative_to(CANDIDATE)):sha(p) for p in CANDIDATE.rglob('*') if p.is_file()},'results':rows}
(OUT/'TARGET-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
