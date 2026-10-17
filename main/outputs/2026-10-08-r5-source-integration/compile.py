#!/usr/bin/env python3
from pathlib import Path
import concurrent.futures,subprocess,json,hashlib,os,time,shlex
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];PLAN=json.loads((HERE/'COMPILE-PLAN.json').read_text());(HERE/'objects').mkdir(exist_ok=True);(HERE/'logs').mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def compile_one(u):
 started=time.monotonic();p=subprocess.run(u['argv'],cwd=ROOT,env=dict(os.environ,LLVM_CONFIG=u['LLVM_CONFIG']),capture_output=True,text=True,timeout=240)
 (HERE/'logs'/(u['name']+'.stdout')).write_text(p.stdout);(HERE/'logs'/(u['name']+'.stderr')).write_text(p.stderr)
 r={**u,'returncode':p.returncode,'elapsed_seconds':round(time.monotonic()-started,3)}
 if not p.returncode:
  r['object_sha256']=sha(u['object']);r['dependency_sha256']=sha(u['dependency'])
  deps=[str(Path(t).resolve()) for t in shlex.split(Path(u['dependency']).read_text().replace('\\\n',' ').split(':',1)[1])]
  headers=[t for t in deps if t.endswith('/Unreal/UFunctionStructs.hpp')];r['selected_changed_header']=headers
  assert headers==[str(HERE/'include/Unreal/UFunctionStructs.hpp')],(u['name'],headers)
 return r
rows=[];failures=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
 jobs={pool.submit(compile_one,u):u['name'] for u in PLAN['units']}
 for f in concurrent.futures.as_completed(jobs):
  try:r=f.result()
  except Exception as e:r={'name':jobs[f],'returncode':-1,'exception':str(e)}
  rows.append(r)
  if r['returncode']:failures.append(r['name']);print(json.dumps({'failed':r['name']}),flush=True)
  if len(rows)%10==0 or len(rows)==len(jobs):
   state={'completed':len(rows),'total':len(jobs),'failures':failures};(HERE/'PROGRESS.json').write_text(json.dumps(state,indent=2)+'\n');print(json.dumps(state),flush=True)
rows.sort(key=lambda x:x['name']);receipt={'status':'FRESH_TARGET_COMPILE_PASS' if not failures else 'FAIL','scope':'Isolated candidate; actual SDK compile, no native execution/console/package promotion','plan_sha256':sha(HERE/'COMPILE-PLAN.json'),'compiled_units':len(rows),'results':rows,'failures':failures}
(HERE/'COMPILE-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
raise SystemExit(bool(failures))
