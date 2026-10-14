from pathlib import Path
import json,subprocess,os,hashlib
r=Path.cwd();o=r/'outputs/2026-10-08-l25-runtime/typescan-r4';comp=json.load(open(r/'outputs/2026-10-08-l24-build-r2/pass1/COMPOSITION-RECEIPT.json'));rows=json.load(open(o/'CHANGES.json'));changes=rows['changes']
# E3 routing fix (agent R, per Z13): header-row candidates must be reachable
# by EVERY unit this local smoke compiles, exactly like build_variants.py's
# include_dirs rule (parents of replaced .hpp/.h candidates). Without the
# candidate/include route, <Unreal/UObjectArray.hpp> resolves to the port
# root pre-fix header (4-slot vtable) instead of the l25 candidate (5 slots).
hdr_inc=sorted({str(Path(x['candidate']).parent) for x in (rows.get('files',[])+changes) if x['candidate'].endswith(('.hpp','.h'))})
results=[]
for c in changes:
 if not c['candidate'].endswith('.cpp'):continue
 u=next(x for x in comp['selected_units'] if x['source']==c['original'])
 a=u['argv'][:];a[1:1]=[x for inc in [str(o/'include'),str(o/'candidate/generated_include')]+hdr_inc+[str(Path(c['original']).parent)] for x in ('-I',inc)];a[a.index('-c')+1]=c['candidate'];a[a.index('-o')+1]=str(o/'results'/f'{u["name"]}.o');a[a.index('-MF')+1]=str(o/'results'/f'{u["name"]}.d')
 e=os.environ.copy();e['LLVM_CONFIG']=u['LLVM_CONFIG'];p=subprocess.run(a,env=e,capture_output=True,text=True);(o/'results'/f'{u["name"]}.log').write_text(p.stdout+p.stderr);results.append(dict(name=u['name'],exit=p.returncode,argv=a));print(u['name'],p.returncode)
 if p.returncode:print(p.stderr[-6000:])
(o/'results/TARGET-COMPILE.json').write_text(json.dumps(results,indent=2));assert all(x['exit']==0 for x in results)
