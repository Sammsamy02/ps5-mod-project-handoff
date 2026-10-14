from pathlib import Path
import json,subprocess,os,hashlib
r=Path.cwd();o=r/'outputs/2026-10-08-l23-runtime/typescan';comp=json.load(open(r/'outputs/2026-10-08-l22-build-r3/pass1/COMPOSITION-RECEIPT.json'));changes=json.load(open(o/'CHANGES.json'))['changes'];results=[]
for c in changes:
 if not c['candidate'].endswith('.cpp'):continue
 u=next(x for x in comp['selected_units'] if x['source']==c['original'])
 a=u['argv'][:];a[1:1]=['-I',str(o/'include'),'-I',str(o/'candidate/generated_include'),'-I',str(Path(c['original']).parent)];a[a.index('-c')+1]=c['candidate'];a[a.index('-o')+1]=str(o/'results'/f'{u["name"]}.o');a[a.index('-MF')+1]=str(o/'results'/f'{u["name"]}.d')
 e=os.environ.copy();e['LLVM_CONFIG']=u['LLVM_CONFIG'];p=subprocess.run(a,env=e,capture_output=True,text=True);(o/'results'/f'{u["name"]}.log').write_text(p.stdout+p.stderr);results.append(dict(name=u['name'],exit=p.returncode,argv=a));print(u['name'],p.returncode)
 if p.returncode:print(p.stderr[-6000:])
(o/'results/TARGET-COMPILE.json').write_text(json.dumps(results,indent=2));assert all(x['exit']==0 for x in results)
