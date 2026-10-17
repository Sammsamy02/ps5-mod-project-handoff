from pathlib import Path
import subprocess,json,os,sys
w=Path(__file__).resolve().parent;root=w.parent.parent;b=root/'.superpowers/native-dependency-spike'
old=root/'outputs/2026-10-08-native-thread-name-adapter'
provider=w/'tests/red_provider.cpp' if sys.argv[1]=='RED' else w/'candidate/ThreadNameRuntime.cpp'
incs=[w/'tests/include',w/'candidate',old/'candidate',root/'outputs/2026-10-07-l19-local-profile-r4/units',b/'native-coherent-runtime-1/source/port/unreal/include',b/'native-coherent-runtime-1/source/port/first/include',b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include',b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include']
incs.append(b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Constructs/include')
target=next(r for r in json.loads((old/'target-wiring/commands.json').read_text()) if r['name']=='ThreadNameRuntime')['argv']
for i,v in enumerate(target):
 if v=='-I':incs.append(Path(target[i+1]))
 elif v.startswith('-I') and len(v)>2:incs.append(Path(v[2:]))
incs=list(dict.fromkeys(incs))
a=['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DRC_UNREAL_BUILD_STATIC','-DRC_HELPERS_BUILD_STATIC','-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC',*[v for i in incs for v in ['-I',str(i)]]]
out=w/'tests'/(sys.argv[2] if len(sys.argv)>2 else sys.argv[1]);out.mkdir(exist_ok=False);rows=[]
argv=a+[str(w/'tests/lifetime_probe.cpp'),str(provider),str(w/'tests/retire_controls.cpp'),str(old/'candidate/NativeThreadNameAdapter.cpp'),str(old/'candidate/NameThreadPolicy.cpp'),'-o',str(out/'controls'),'-MD','-MF',str(out/'controls.d')]
for name,argv in [('build',argv),('run',[str(out/'controls')])]:
 p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=120)
 rows.append(dict(name=name,argv=argv,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stdout[-1800:],p.stderr[-1500:],flush=True)
 if p.returncode:raise SystemExit(p.returncode)
