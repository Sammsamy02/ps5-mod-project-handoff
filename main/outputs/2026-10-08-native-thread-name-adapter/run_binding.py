from pathlib import Path
import subprocess,json,os,sys
w=Path(__file__).resolve().parent;root=w.parent.parent;b=root/'.superpowers/native-dependency-spike'
incs=[w/'candidate',root/'outputs/2026-10-07-l19-local-profile-r4/units',b/'native-coherent-runtime-1/source/port/unreal/include',b/'native-coherent-runtime-1/source/port/first/include',b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include',b/'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include']
a=['/usr/bin/clang++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DRC_UNREAL_BUILD_STATIC','-DRC_HELPERS_BUILD_STATIC','-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC',*[v for i in incs for v in ['-I',str(i)]]]
out=w/(sys.argv[1] if len(sys.argv)>1 else 'tests/BINDING');out.mkdir(exist_ok=False);rows=[]
for name,argv in [('build',a+[str(w/'tests/binding_controls.cpp'),str(w/'candidate/NativeThreadNameAdapter.cpp'),str(w/'candidate/NameThreadPolicy.cpp'),'-o',str(out/'controls')]),('run',[str(out/'controls'),str(root/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin')])]:
 p=subprocess.run(argv,capture_output=True,text=True,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),timeout=120)
 rows.append(dict(name=name,argv=argv,exit=p.returncode,stdout=p.stdout,stderr=p.stderr));(out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stdout[-500:],p.stderr[-1500:] if p.returncode else '',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
