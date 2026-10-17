from pathlib import Path
import subprocess,json,os
w=Path(__file__).resolve().parent;root=w.parent.parent
r=json.loads((root/'outputs/2026-10-07-l19-r4-full-build/parent-review/AFFECTED-RECIPES.json').read_text())['recipes']['unreal-unrealinitializer']
a=r['original_argv'];args=['/usr/bin/clang++']+a[1:a.index('-c')]
args[1:1]=['-I',str(w/'candidate'),'-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units'),'-I',str(root/'.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/include')]
args+=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-DPLH_OWNED_MEMORY_HOST_TEST']
out=w/'tests/ADAPTER';out.mkdir(exist_ok=False);rows=[]
for name,argv in [('build',args+[str(w/'tests/adapter_refusal_controls.cpp'),str(w/'candidate/SelectedThreadAuthorityAdapter.cpp'),str(w/'candidate/NativeThreadAuthority.cpp'),str(root/'.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/src/OwnedMemory.cpp'),'-o',str(out/'controls')]),('run',[str(out/'controls')])]:
 p=subprocess.run(argv,env=dict(os.environ,ASAN_OPTIONS='halt_on_error=1:detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'),capture_output=True,text=True,timeout=120)
 rows.append(dict(name=name,argv=argv,exit=p.returncode,stdout=p.stdout,stderr=p.stderr))
 (out/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(out/(name+'.log')).write_text(p.stdout+p.stderr)
 print(name,p.returncode,p.stdout,p.stderr[-3000:] if p.returncode else '',flush=True)
 if p.returncode:raise SystemExit(p.returncode)
