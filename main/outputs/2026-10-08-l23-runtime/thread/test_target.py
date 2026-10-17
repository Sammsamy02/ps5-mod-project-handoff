from pathlib import Path
import json,subprocess,hashlib,os
H=Path(__file__).resolve().parent;R=H.parents[2];B=H/'tests/target';B.mkdir(exist_ok=True)
base=json.load(open(R/'outputs/2026-10-08-l22-build-r3/pass1/COMPOSITION-RECEIPT.json'))
rows=[]
for name in ('LuaMod.cpp','UE4SSProgram.cpp'):
 u=next(u for u in base['selected_units'] if Path(u['source']).name==name)
 a=list(u['argv']);a[a.index(u['source'])]=str(H/'src'/name);a[a.index('-o')+1]=str(B/(name+'.o'));a[a.index('-MF')+1]=str(B/(name+'.d'))
 a[1:1]=['-I',str(H/'include'),'-I',str(H.parent/'typescan/include')]
 p=subprocess.run(a,cwd=R,env=dict(os.environ,LLVM_CONFIG=u["LLVM_CONFIG"],CCACHE_DISABLE="1"),capture_output=True,text=True);(B/(name+'.stderr')).write_text(p.stderr);(B/(name+'.stdout')).write_text(p.stdout)
 row={'unit':name,'argv':a,'returncode':p.returncode,'source_sha256':hashlib.sha256((H/'src'/name).read_bytes()).hexdigest()};rows.append(row)
 print(name,p.returncode,flush=True)
 if p.returncode:print(p.stderr[-2200:])
(B/'RESULTS.json').write_text(json.dumps(rows,indent=2)+'\n')
raise SystemExit(any(r['returncode'] for r in rows))
