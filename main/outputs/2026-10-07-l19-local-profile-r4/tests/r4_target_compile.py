"""Fresh whole selected Program/initializer/binder/owner TUs with genuine target headers."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,sys
O=Path(__file__).resolve().parents[1];W=O.parents[1];out=O/'tests/r4-target';out.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
env=dict(os.environ);env['LLVM_CONFIG']=str(W/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
baseline=json.loads((W/'outputs/2026-10-07-l18-full-build/runs/run-l18-2/commands.json').read_text())
program=list(next(x['argv'] for x in json.loads((W/'outputs/2026-10-07-l19-r4-compiled-loader/reports/TARGET-COMPILE.json').read_text())['commands'] if x['mode']=='selected'))
initial=list(next(x['argv'] for x in baseline if x['name']=='unreal-unrealinitializer'))
selected=['UE4SSProgram.cpp','UnrealInitializer.cpp','NativeLayoutBinding.cpp','FField.cpp','RuntimeBuildProfile.hpp','NativeLayoutDeclarations.hpp','CompiledNativeLayoutDeclarations.hpp','NativeLayoutBinding.hpp','NativeLayoutActiveWidths.hpp','Unreal/FField.hpp']
before={n:sha(O/'units'/n) for n in selected};rows=[];deps={}
for name,source in [('program','UE4SSProgram.cpp'),('initializer','UnrealInitializer.cpp'),('binder','NativeLayoutBinding.cpp'),('owner','FField.cpp')]:
 cmd=list(program if name=='program' else initial)
 cmd=[v for v in cmd if not v.startswith('-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER=')]
 cmd[1:1]=['-I',str(O/'units'),'-I',str(W/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include'),'-I',str(W/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/include')]
 if name=='program':cmd.insert(1,'-DRC_NATIVE_LAYOUT_DECLARATIONS_HEADER="'+str(O/'units/CompiledNativeLayoutDeclarations.hpp')+'"')
 for i,v in enumerate(cmd):
  if v.endswith('.cpp'):cmd[i]=str(O/'units'/source)
 cmd[cmd.index('-o')+1]=str(out/(name+'.o'))
 if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(out/(name+'.d'))
 else:cmd[1:1]=['-MMD','-MF',str(out/(name+'.d'))]
 r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120)
 dep=out/(name+'.d');dep_paths=[]
 if r.returncode==0:
  dep_paths=[Path(x).resolve() for x in shlex.split(dep.read_text().replace('\\\n',' '))[1:] if Path(x).is_file()]
  mandatory=[O/'units'/source]
  if name=='program':mandatory += [O/'units/RuntimeBuildProfile.hpp',O/'units/NativeLayoutDeclarations.hpp',O/'units/CompiledNativeLayoutDeclarations.hpp']
  if name=='initializer':mandatory += [O/'units/RuntimeBuildProfile.hpp',O/'units/BuildFactsChecks.hpp']
  if name in ('program','initializer','binder','owner'):mandatory += [O/'units/Unreal/FField.hpp']
  missing=[str(f) for f in mandatory if f not in dep_paths];assert not missing,(name,missing)
  deps.update({str(f):sha(f) for f in dep_paths})
 rows.append({'name':name,'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr,'object_sha256':sha(out/(name+'.o')) if r.returncode==0 else None,'dependency_sha256':sha(dep) if dep.exists() else None,'dependency_files':len(dep_paths),'mandatory_dependencies_verified':r.returncode==0})
 print(name,r.returncode,flush=True)
 if r.returncode:print(r.stderr[-6000:],flush=True);break
assert before=={n:sha(O/'units'/n) for n in selected}
result={'pass':len(rows)==4 and all(x['exit']==0 for x in rows),'commands':rows,'source_pins':{str(O/'units'/n):v for n,v in before.items()},'dependency_pins':dict(sorted(deps.items())),'scope':'Four fresh genuine PS5 target translation units. No link, console execution or engine calls.'}
(O/'reports/R4-TARGET-COMPILE.json').write_text(json.dumps(result,indent=2)+'\n');sys.exit(0 if result['pass'] else 1)
