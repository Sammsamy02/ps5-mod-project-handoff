from pathlib import Path
import subprocess,json,hashlib
H=Path(__file__).resolve().parent;B=H/'tests';rows=[]
for mode,flags in [('normal',[]),('asan',['-fsanitize=address,undefined']),('tsan',['-fsanitize=thread'])]:
 exe=B/('state-'+mode);cmd=['clang++','-std=c++20','-pthread',*flags,'-I',str(H/'include'),str(B/'state.cpp'),'-o',str(exe)]
 p=subprocess.run(cmd,capture_output=True,text=True);q=subprocess.run([str(exe)],capture_output=True,text=True) if not p.returncode else None
 row={'mode':mode,'command':cmd,'compile_returncode':p.returncode,'run_returncode':q.returncode if q else None,'output':p.stdout+p.stderr+(q.stdout+q.stderr if q else '')};rows.append(row);print(mode,row['compile_returncode'],row['run_returncode'])
(B/'STATE-RESULTS.json').write_text(json.dumps({'rows':rows,'source_sha256':hashlib.sha256((B/'state.cpp').read_bytes()).hexdigest(),'header_sha256':hashlib.sha256((H/'include/L23GameThreadProbe.hpp').read_bytes()).hexdigest(),'cases':'submission, duplicate, failed enqueue, 30000ms boundary, one timeout, late drain, wrong native thread, wrong owner, cancelled owner, backward clock, exactly-once drain,100 concurrent timeout/drain/cancel races'},indent=2)+'\n')
assert all(r['compile_returncode']==0 and r['run_returncode']==0 for r in rows)
