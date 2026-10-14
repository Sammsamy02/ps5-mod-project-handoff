from pathlib import Path
import concurrent.futures, hashlib, json, os, subprocess, sys
w=Path(__file__).resolve().parent; b=w.parent
out=w/sys.argv[1]; out.mkdir(exist_ok=False)
base=json.loads((b/'ue4ss-unreal-native-build-1/consolidated-23/result.json').read_text())
units={u['name']:u for u in base['units']}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def build(task):
 name,source,recipe=task; source=w/'src'/source
 old=units[recipe]; c=json.loads(Path(old['command']).read_text()); argv=c['argv'][:]
 argv[argv.index(old['source'])]=str(source);obj=out/(name+'.o');argv[argv.index('-o')+1]=str(obj)
 for flag in ['-MF','-MT','-MQ']:
  while flag in argv:
   i=argv.index(flag);del argv[i:i+2]
 argv=[x for x in argv if x not in ['-MMD','-MD']]
 argv[1:1]=['-I',str(w/'include'),'-I',str(b/'ue4ss-native-layout-parser-1/include'),'-I',str(b/'ue4ss-native-layout-widths-1/include')]
 argv+=['-MD','-MF',str(out/(name+'.d'))]
 env=dict(os.environ);env['LLVM_CONFIG']=c.get('LLVM_CONFIG',old.get('LLVM_CONFIG',str(w.parents[2]/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')))
 r=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=120)
 (out/(name+'.stdout')).write_text(r.stdout);(out/(name+'.stderr')).write_text(r.stderr)
 command=out/(name+'-command.json');command.write_text(json.dumps(dict(argv=argv,exit=r.returncode,LLVM_CONFIG=env['LLVM_CONFIG']),indent=2)+'\n')
 v=dict(name=name,source=str(source),source_sha256=sha(source),exit=r.returncode,command=str(command),dependency_file=str(out/(name+'.d')))
 if r.returncode==0:v.update(object=str(obj),object_sha256=sha(obj),object_bytes=obj.stat().st_size)
 print(name,r.returncode,flush=True)
 if r.returncode:print(r.stderr[-5000:],flush=True)
 return v
names=[('native-layoutbinding','NativeLayoutBinding.cpp','program'),('program',str(b/'ue4ss-native-cpp-start-order-1/src/UE4SSProgram.cpp'),'program'),('unreal-unrealinitializer','UnrealInitializer.cpp','unreal-unrealinitializer'),('unreal-src-versionedcontainer-container','Container.cpp','unreal-src-versionedcontainer-container')]
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool: result=list(pool.map(build,names))
(out/'result.json').write_text(json.dumps(dict(units=result,all_compiled=all(u['exit']==0 for u in result),executed=False),indent=2)+'\n')
sys.exit(0 if all(u['exit']==0 for u in result)else 1)
