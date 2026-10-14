from pathlib import Path
import subprocess,json,os
w=Path(__file__).resolve().parent;root=w.parent.parent
r=json.loads((root/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json').read_text())['recipes']['unreal-unrealinitializer']
a=r['original_argv'];args=a[:a.index('-c')];args[1:1]=r['include_prefix']+['-I',str(w/'candidate'),'-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units')]
out=w/'target';out.mkdir(exist_ok=False);rows=[]
for name in ['NameThreadPolicy','NativeThreadNameAdapter','TargetEngineCalls','TargetAbiCensus']:
 argv=args+['-c',str(w/'candidate'/(name+'.cpp')),'-o',str(out/(name+'.o')),'-MD','-MF',str(out/(name+'.d'))]
 p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,LLVM_CONFIG=r['LLVM_CONFIG']),timeout=180)
 rows.append(dict(name=name,argv=argv,LLVM_CONFIG=r['LLVM_CONFIG'],exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stderr[-2500:] if p.returncode else '',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
