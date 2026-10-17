#!/usr/bin/env python3
from pathlib import Path
import concurrent.futures,subprocess,json,hashlib,os,time,shlex
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];OUT=HERE/'pass2-build';PLAN=json.loads((OUT/'COMPILE-PLAN.json').read_text())
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def one(u):
 assert sha(u['source'])==u['source_sha256'];start=time.monotonic();r=subprocess.run(u['argv'],cwd=ROOT,env=dict(os.environ,LLVM_CONFIG=u['LLVM_CONFIG']),capture_output=True,text=True,timeout=300)
 (OUT/'logs'/(u['name']+'.stdout')).write_text(r.stdout);(OUT/'logs'/(u['name']+'.stderr')).write_text(r.stderr);row={**u,'returncode':r.returncode,'elapsed_seconds':round(time.monotonic()-start,3)}
 if not r.returncode:
  paths=sorted(set(str((ROOT/t).resolve()) for t in shlex.split(Path(u['dependency']).read_text().replace('\\\n',' ').split(':',1)[1])));row['dependency_paths']=paths;row['object_sha256']=sha(u['object']);row['dependency_sha256']=sha(u['dependency'])
  names={p.name for p in (HERE/'thread').glob('*') if p.suffix in {'.h','.hpp'}};assert sorted(p for p in paths if Path(p).name in names)==u['expected_thread_header_paths']
  for name,expected in [('UFunctionStructs.hpp',ROOT/'outputs/2026-10-08-r5-source-integration/include/Unreal/UFunctionStructs.hpp'),('FField.hpp',ROOT/'outputs/2026-10-07-l19-r4-work/dependent-build-pass1/include/Unreal/FField.hpp')]:
   found=[p for p in paths if Path(p).name==name];assert not found or found==[str(expected)]
  assert row['object_sha256']==u['expected_previous_object_sha256'],(u['name'],'object differs across fresh compiles')
 return row
rows=[];fail=[]
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
 jobs={pool.submit(one,u):u['name'] for u in PLAN['units']}
 for f in concurrent.futures.as_completed(jobs):
  try:r=f.result()
  except Exception as e:r={'name':jobs[f],'returncode':-1,'exception':str(e)}
  rows.append(r)
  if r['returncode']:fail.append(r['name'])
  print(json.dumps({'pass':2,'completed':len(rows),'total':2,'failures':fail}),flush=True)
receipt={'status':'FRESH_COMBINED_TARGET_COMPILE_PASS' if not fail else 'FAIL','pass':2,'scope':'Independent second actual SDK compile of final initializer/provider; all stable accepted objects retained. No package/native/console claim.','plan_sha256':sha(OUT/'COMPILE-PLAN.json'),'compiled_units':2,'results':sorted(rows,key=lambda r:r['name']),'failures':fail}
(OUT/'COMPILE-RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n');raise SystemExit(bool(fail))
