from pathlib import Path
import subprocess,json,os,sys
w=Path(__file__).resolve().parent
root=w.parent.parent
b=root/'.superpowers/native-dependency-spike'
incs=[w/'candidate',root/'outputs/2026-10-07-l19-local-profile-r4/units',b/'ue4ss-native-read-lease-1/include',b/'ue4ss-native-ps-scan-abi-1/include',b/'native-coherent-runtime-1/source/port/unreal/include',b/'native-coherent-runtime-1/source/port/first/include',b/'ue4ss-unreal-native-build-1/port/ue4ss/include',b/'ue4ss-unreal-native-build-1/port/first/include',b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include',b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include',b/'ue4ss-polyhook-platform-port-1/patched/include',b/'ue4ss-unreal-native-build-1/source-dependencies/polyhook']
host=['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DPLH_OWNED_MEMORY_HOST_TEST','-DRC_HELPERS_BUILD_STATIC','-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC',*[v for p in incs for v in ['-I',str(p)]]]
mode=sys.argv[1]
out=w/'tests'/mode
out.mkdir(exist_ok=False)
rows=[]
def run(name,argv):
 env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 p=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=120)
 row=dict(name=name,argv=argv,exit=p.returncode,stdout=p.stdout,stderr=p.stderr)
 rows.append(row);(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n')
 (out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stdout[-400:],p.stderr[-2500:] if p.returncode else '',flush=True)
 return p.returncode
source=w/('red' if mode.startswith('RED') else 'candidate')/'NativeThreadAuthority.cpp'
if run('build',host+[str(w/'tests/authority_controls.cpp'),str(source),str(b/'ue4ss-native-read-lease-1/src/OwnedMemory.cpp'),'-o',str(out/'controls')]):raise SystemExit(2)
image=root/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
rc=run('run',[str(out/'controls'),str(image)])
if mode.startswith('RED'):
 if rc==0 or 'checks=23 failures=' not in rows[-1]['stdout']:raise SystemExit('RED did not establish a behavioral failure')
 print('expected RED behavioral refusal captured')
else:
 if rc:raise SystemExit(rc)
