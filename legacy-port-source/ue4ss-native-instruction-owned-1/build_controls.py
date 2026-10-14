from pathlib import Path
import json,subprocess,sys,hashlib
w=Path(__file__).resolve().parent;b=w.parent;label=sys.argv[1];out=w/label;out.mkdir(exist_ok=False)
provider=b/'ue4ss-native-owned-memory-1';poly=b/'ue4ss-unreal-native-build-1/source-dependencies/polyhook';decoder=b/'ue4ss-virtual-dispatch-dependencies-1'
cmd=['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DPLH_OWNED_MEMORY_HOST_TEST','-DZYDIS_STATIC_BUILD','-DZYCORE_STATIC_BUILD']
roots=[w/'include',provider/'include',b/'ue4ss-polyhook-platform-port-1/patched/include',poly,decoder/'zydis/include',decoder/'zycore/include']
if '--upstream' in sys.argv: roots.remove(w/'include')
for p in roots:cmd+=['-I',str(p)]
cmd +=[str(w/'owned_controls.cpp'),str(provider/'src/OwnedMemory.cpp'),str(poly/'sources/UID.cpp'),'-o',str(out/'owned_controls')]
records=[]
for argv in [cmd,[str(out/'owned_controls')]]:
 p=subprocess.run(argv,capture_output=True,text=True);records.append(dict(argv=argv,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
 if p.returncode:break
print(json.dumps({'exit':p.returncode,'stdout':p.stdout,'stderr':p.stderr[:1500]}));sys.exit(p.returncode)
