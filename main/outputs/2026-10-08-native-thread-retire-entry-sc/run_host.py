from pathlib import Path
import json,subprocess,os
w=Path(__file__).resolve().parent;root=w.parent.parent
prior=root/'outputs/2026-10-08-native-thread-retire-entry'
record=json.loads((prior/'tests/GREEN-controls/commands.json').read_text())
out=w/'tests/GREEN-controls';out.mkdir(parents=True,exist_ok=False);rows=[]
a=list(record[0]['argv']);a[1:1]=['-I',str(w/'candidate')]
a[a.index(str(prior/'candidate/ThreadNameRuntime.cpp'))]=str(w/'candidate/ThreadNameRuntime.cpp')
a[a.index('-o')+1]=str(out/'controls');a[a.index('-MF')+1]=str(out/'controls.d')
for name,a in [('build',a),('run',[str(out/'controls')])]:
 p=subprocess.run(a,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=120)
 rows.append(dict(name=name,argv=a,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stdout[-1800:],p.stderr[-1500:],flush=True)
 if p.returncode:raise SystemExit(p.returncode)
