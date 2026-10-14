from pathlib import Path
import json,subprocess,hashlib,os,sys
root=Path(__file__).resolve().parents[1];w=root.parents[1];out=root/'tests/target-focus';out.mkdir(exist_ok=True)
before={n:hashlib.sha256((root/'units'/n).read_bytes()).hexdigest() for n in ['UnrealInitializer.cpp','NativeLayoutBinding.cpp','NativeLayoutBinding.hpp','RuntimeBuildProfile.hpp','BuildFactsChecks.hpp']}
commands=json.loads((w/'outputs/2026-10-07-l18-full-build/runs/run-l18-2/commands.json').read_text());rows=[]
for name,source in [('unreal-unrealinitializer','UnrealInitializer.cpp'),('native-layoutbinding','NativeLayoutBinding.cpp')]:
 cmd=list(next(c['argv'] for c in commands if c['name']==('unreal-unrealinitializer' if name=='native-layoutbinding' else name)));cmd[1:1]=['-I',str(w/'.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include'),'-I',str(w/'.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/include')];cmd[1:1]=['-I',str(root/'units')]
 for i,v in enumerate(cmd):
  if v.endswith('.cpp'):cmd[i]=str(root/'units'/source)
 cmd[cmd.index('-o')+1]=str(out/(name+'.o'))
 if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(out/(name+'.d'))
 env=dict(os.environ);env['LLVM_CONFIG']=str(w/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
 r=subprocess.run(cmd,capture_output=True,text=True,env=env,timeout=120);rows.append({'argv':cmd,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});print(name,r.returncode,flush=True)
 if r.returncode:print(r.stderr,flush=True)
assert before=={n:hashlib.sha256((root/'units'/n).read_bytes()).hexdigest() for n in before}
(root/'reports/NATIVE-FOCUS.json').write_text(json.dumps({'commands':rows,'source_hashes':{n:hashlib.sha256((root/'units'/n).read_bytes()).hexdigest() for n in ['UnrealInitializer.cpp','NativeLayoutBinding.cpp','NativeLayoutBinding.hpp','RuntimeBuildProfile.hpp','BuildFactsChecks.hpp']}},indent=2)+'\n');sys.exit(0 if all(r['exit']==0 for r in rows) else 1)
