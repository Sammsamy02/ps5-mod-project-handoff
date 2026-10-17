from pathlib import Path
import subprocess,json,os,sys
w=Path(__file__).resolve().parent;mode=sys.argv[1];out=w/'tests'/(mode+'-controls');out.mkdir(exist_ok=False)
source=w/('tests/red-stub' if mode=='RED' else 'candidate')/'NameThreadPolicy.cpp';rows=[]
for name,a in [('build',['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(w/'candidate'),str(w/'tests/name_controls.cpp'),str(source),'-o',str(out/'controls')]),('run',[str(out/'controls')])]:
 p=subprocess.run(a,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=120)
 rows.append(dict(name=name,argv=a,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stdout[-300:],p.stderr[-1500:] if p.returncode else '',flush=True)
 if name=='build' and p.returncode:raise SystemExit(p.returncode)
if mode=='RED':
 assert rows[-1]['exit']==1 and 'checks=20 failures=' in rows[-1]['stdout']
else:assert rows[-1]['exit']==0 and 'checks=20 failures=0' in rows[-1]['stdout']
