from pathlib import Path
import json,subprocess,sys,os,hashlib
w=Path(__file__).resolve().parent;b=w.parent;r=b.parent.parent;label=sys.argv[1];out=w/label;out.mkdir(exist_ok=False);sdk=r/'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk';llvm=r/'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin';env=dict(os.environ,LLVM_CONFIG=str(llvm/'llvm-config'))
incs=[w/'include',b/'ue4ss-polyhook-platform-port-1/patched/include',b/'ue4ss-unreal-native-build-1/source-dependencies/polyhook'];flags=['-std=c++23','-O1','-g',*[x for p in incs for x in ['-I',str(p)]]];rows=[]
def run(name,argv):
 p=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=120);(out/(name+'.stdout')).write_text(p.stdout);(out/(name+'.stderr')).write_text(p.stderr);rows.append({'name':name,'argv':argv,'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr});(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');print(name,p.returncode,p.stdout.strip(),p.stderr[-1600:]if p.returncode else'',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
run('host-build',['/usr/bin/clang++',*flags,'-DPLH_OWNED_MEMORY_HOST_TEST','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(w/'tests/owned_controls.cpp'),str(w/'src/OwnedMemory.cpp'),'-o',str(out/'owned_controls')]);run('host-run',[str(out/'owned_controls')])
for name,source in [('OwnedMemory',w/'src/OwnedMemory.cpp'),('native-owned',w/'tests/native_owned.cpp')]:
 run('native-'+name,[str(sdk/'bin/prospero-clang++'),*flags,'-fPIC','-MD','-MF',str(out/(name+'.d')),'-c',str(source),'-o',str(out/(name+'.o'))])
print('host behavior and real SDK consumers compiled')
