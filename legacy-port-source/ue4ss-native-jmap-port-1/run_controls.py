from pathlib import Path
import subprocess,json,sys
r=Path(__file__).resolve().parent;s=r.parent;out=r/(sys.argv[1]if len(sys.argv)>1 else'host-controls');out.mkdir(exist_ok=False)
cmd=['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DPLH_OWNED_MEMORY_HOST_TEST','-I',str(r/'include'),'-I',str(s/'ue4ss-native-owned-memory-1/include'),'-I',str(s/'ue4ss-polyhook-platform-port-1/patched/include'),'-I',str(s/'ue4ss-unreal-native-build-1/source-dependencies/polyhook'),str(r/'owned_controls.cpp'),str(s/'ue4ss-native-owned-memory-1/src/OwnedMemory.cpp'),'-o',str(out/'owned_controls')]
p=subprocess.run(cmd,capture_output=True,text=True);records=[dict(argv=cmd,exit=p.returncode,stdout=p.stdout,stderr=p.stderr)]
if p.returncode==0:
 p=subprocess.run([str(out/'owned_controls')],capture_output=True,text=True);records.append(dict(argv=[str(out/'owned_controls')],exit=p.returncode,stdout=p.stdout,stderr=p.stderr))
(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n');print(p.returncode,p.stdout,p.stderr[-3000:]);sys.exit(p.returncode)
