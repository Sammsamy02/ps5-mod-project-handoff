#!/usr/bin/env python3
import subprocess,pathlib,sys
root=pathlib.Path(__file__).resolve().parents[1]
build=root/'build';build.mkdir(exist_ok=True)
flags=['clang++','-arch','x86_64','-std=c++23','-O0','-g','-Wall','-Wno-unused-function','-DPAGE_SIZE=4096','-DPAGE_MASK=4095','-I'+str(root/'src')]
renames=['-DsceKernelQueryMemoryProtection=cap3_host_qmp','-DsceKernelVirtualQuery=cap3_host_vq','-Dmprotect=cap3_host_mprotect']
subprocess.run(flags+renames+['-c',str(root/'src/CodePatch.cpp'),'-o',str(build/'production.o')],check=True)
subprocess.run(flags+[str(root/'tests/regression.cpp'),str(build/'production.o'),'-o',str(build/'regression')],check=True)
subprocess.run(flags+[str(root/'tests/legacy_dispatch.cpp'),str(build/'production.o'),'-o',str(build/'legacy')],check=True)
failed=0
for case in ['unreadable','verify-open','dirty-restore','dirty-recovery','flags','backup','query-bounds','scope-refusal']:
 p=subprocess.run([str(build/'regression'),case],capture_output=True,text=True,timeout=10)
 print(f'{case}: exit={p.returncode}\n{p.stdout}{p.stderr}')
 failed+=p.returncode!=0
import re
legacy_cases=re.findall(r'^static void (test_\w+)\(void\)',(root/'tests/legacy_host_test.cpp').read_text(),re.M)
for case in legacy_cases:
 p=subprocess.run([str(build/'legacy'),case],capture_output=True,text=True,timeout=10)
 print(f'{case}: exit={p.returncode}\n{p.stdout}{p.stderr}')
 failed+=p.returncode!=0
print(f'cases={8+len(legacy_cases)} failed={failed}')
sys.exit(bool(failed))
