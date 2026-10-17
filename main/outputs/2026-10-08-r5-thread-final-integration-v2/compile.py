#!/usr/bin/env python3
from pathlib import Path
import concurrent.futures, subprocess, json, hashlib, os, time, shlex
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];PLAN=json.loads((HERE/'COMPILE-PLAN.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def compile_one(u):
 assert sha(u['source'])==u['source_sha256']
 started=time.monotonic();p=subprocess.run(u['argv'],cwd=ROOT,env=dict(os.environ,LLVM_CONFIG=u['LLVM_CONFIG']),capture_output=True,text=True,timeout=300)
 (HERE/'logs'/(u['name']+'.stdout')).write_text(p.stdout);(HERE/'logs'/(u['name']+'.stderr')).write_text(p.stderr)
 r={**u,'returncode':p.returncode,'elapsed_seconds':round(time.monotonic()-started,3)}
 if not p.returncode:
  r['object_sha256']=sha(u['object']);r['dependency_sha256']=sha(u['dependency'])
  paths=sorted(set(str((ROOT/t).resolve()) for t in shlex.split(Path(u['dependency']).read_text().replace('\\\n',' ').split(':',1)[1])))
  r['dependency_paths']=paths
  expected={'UFunctionStructs.hpp':ROOT/'outputs/2026-10-08-r5-source-integration/include/Unreal/UFunctionStructs.hpp','FField.hpp':ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include/Unreal/FField.hpp'}
  checks={}
  for name,wanted in expected.items():
   found=[p for p in paths if Path(p).name==name]
   assert not found or found==[str(wanted)],(u['name'],name,found)
   checks[name]=found
  for p in (HERE/'thread').glob('*.hpp'):
   found=[t for t in paths if Path(t).name==p.name]
   assert not found or found==[str(p)],(u['name'],p.name,found)
  if u['name'] in {'unreal-unrealinitializer','luamod'}:
   assert checks['UFunctionStructs.hpp'] and checks['FField.hpp']
   assert str(HERE/'thread/ThreadNameRuntime.hpp') in paths
  r['coherence_checks']=checks
 return r
rows=[];failures=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 jobs={pool.submit(compile_one,u):u['name'] for u in PLAN['units']}
 for f in concurrent.futures.as_completed(jobs):
  try:r=f.result()
  except Exception as e:r={'name':jobs[f],'returncode':-1,'exception':str(e)}
  rows.append(r)
  if r['returncode']:failures.append(r['name'])
  state={'completed':len(rows),'total':len(jobs),'failures':failures};(HERE/'PROGRESS.json').write_text(json.dumps(state,indent=2)+'\n');print(json.dumps(state),flush=True)
rows.sort(key=lambda x:x['name']);receipt={'status':'FRESH_COMBINED_TARGET_COMPILE_PASS' if not failures else 'FAIL','scope':'Isolated admission/retire revision compile under accepted R5 and FField overlays. No final package admission, deployment or native execution.','plan_sha256':sha(HERE/'COMPILE-PLAN.json'),'compiled_units':len(rows),'results':rows,'failures':failures}
(HERE/'COMPILE-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
raise SystemExit(bool(failures))
