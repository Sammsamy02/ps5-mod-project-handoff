from pathlib import Path
import json,subprocess,os
w=Path(__file__).resolve().parent;root=w.parent.parent
prior=root/'outputs/2026-10-08-native-thread-retire-entry'
r=next(r for r in json.loads((prior/'target/commands.json').read_text()) if r['name']=='ThreadNameRuntime')
out=w/'target';out.mkdir(exist_ok=False);rows=[]
a=list(r['argv']);a[1:1]=['-I',str(w/'candidate')];a[a.index('-c')+1]=str(w/'candidate/ThreadNameRuntime.cpp');a[a.index('-o')+1]=str(out/'ThreadNameRuntime.o');a[a.index('-MF')+1]=str(out/'ThreadNameRuntime.d')
p=subprocess.run(a,capture_output=True,text=True,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']),timeout=180)
rows.append(dict(name='ThreadNameRuntime',argv=a,LLVM_CONFIG=r['LLVM_CONFIG'],exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/'ThreadNameRuntime.log').write_text(p.stdout+p.stderr)
print('provider',p.returncode,p.stderr[-1500:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
tool=Path(r['LLVM_CONFIG']).parent
for name,a in [('DEFINED',[str(tool/'llvm-nm'),'--defined-only','--format=posix',str(out/'ThreadNameRuntime.o')]),('DISASSEMBLY',[str(tool/'llvm-objdump'),'-dr',str(out/'ThreadNameRuntime.o')])]:
 p=subprocess.run(a,capture_output=True,text=True,timeout=30);assert p.returncode==0
 (out/(name+'.stdout')).write_text(p.stdout);rows.append(dict(name=name,argv=a,exit=p.returncode));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
