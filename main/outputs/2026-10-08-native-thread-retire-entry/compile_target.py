from pathlib import Path
import json,subprocess,os
w=Path(__file__).resolve().parent;root=w.parent.parent
old=root/'outputs/2026-10-08-native-thread-name-adapter'
r=next(r for r in json.loads((old/'target-wiring/commands.json').read_text()) if r['name']=='ThreadNameRuntime')
out=w/'target';out.mkdir(exist_ok=False);rows=[]
for name,source in [('ThreadNameRuntime',w/'candidate/ThreadNameRuntime.cpp'),('target_c_entry',w/'tests/target_c_entry.c')]:
 a=list(r['argv']);a[1:1]=['-I',str(w/'candidate')]
 a[a.index('-c')+1]=str(source);a[a.index('-o')+1]=str(out/(name+'.o'));a[a.index('-MF')+1]=str(out/(name+'.d'))
 if name=='target_c_entry':
  a=[v for v in a if not v.startswith('-std=')];a[1:1]=['-x','c','-std=c11']
 p=subprocess.run(a,capture_output=True,text=True,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']),timeout=180)
 rows.append(dict(name=name,argv=a,LLVM_CONFIG=r['LLVM_CONFIG'],exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stderr[-2000:],flush=True)
 if p.returncode:raise SystemExit(p.returncode)
tool=Path(r['LLVM_CONFIG']).parent
for name,a in [('DEFINED',[str(tool/'llvm-nm'),'--defined-only','--format=posix',str(out/'ThreadNameRuntime.o')]),('C-UNDEFINED',[str(tool/'llvm-nm'),'--undefined-only','--format=posix',str(out/'target_c_entry.o')]),('RETIRE-DISASSEMBLY',[str(tool/'llvm-objdump'),'-dr','--disassemble-symbols=bridge_native_thread_authority_retire',str(out/'ThreadNameRuntime.o')])]:
 p=subprocess.run(a,capture_output=True,text=True,timeout=30);assert p.returncode==0
 (out/(name+'.stdout')).write_text(p.stdout);rows.append(dict(name=name,argv=a,exit=p.returncode));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
