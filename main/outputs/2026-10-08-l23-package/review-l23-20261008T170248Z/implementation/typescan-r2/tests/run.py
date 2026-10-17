from pathlib import Path
import subprocess,json
p=Path(__file__).resolve().parent.parent;root=Path.cwd();results=[]
x=subprocess.run(['python3',str(p/'tests/test_scan.py')],capture_output=True,text=True);assert x.returncode==0
x=subprocess.run(['python3',str(p/'tests/test_custom.py')],capture_output=True,text=True);assert x.returncode==0,x.stderr
for name,src in [('custom','actual_custom.cpp'),('policy','test_policy.cpp'),('getters','actual_getters.cpp'),('backend','actual_backend.cpp'),('scan','actual_scan.cpp')]:
 for mode in ['plain','asan-ubsan']:
  binary=p/'tests'/f'{name}-{mode}';a=['clang++','-std=c++20','-include','utility','-I',str(p/'include'),'-I',str(root/'outputs/2026-10-07-l19-local-profile-r4/units'),str(p/'tests'/src),'-o',str(binary)]
  if mode!='plain':a+=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
  x=subprocess.run(a,capture_output=True,text=True);assert x.returncode==0,x.stderr
  x=subprocess.run([str(binary)],capture_output=True,text=True);r=dict(name=name,mode=mode,exit=x.returncode,stdout=x.stdout,stderr=x.stderr,command=a);results.append(r);print(name,mode,x.returncode,x.stdout.strip());assert x.returncode==0,x.stderr
(p/'results/HOST-TESTS.json').write_text(json.dumps(results,indent=2))
