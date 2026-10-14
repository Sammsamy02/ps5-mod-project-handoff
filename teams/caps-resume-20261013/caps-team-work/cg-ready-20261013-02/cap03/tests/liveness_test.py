#!/usr/bin/env python3
# Run actual kekcall_check return handling with ONLY the inline transport replaced.
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[1]
source=(root/'src/CodePatch.cpp').read_text();failed=0
for raw,cf,want in [(0,1,1),(0,0,0),(14,1,1),(123,0,1)]:
 with tempfile.TemporaryDirectory() as td:
  d=Path(td);s=source
  start=s.index('    __asm__ __volatile__(',s.index('int kekcall_check('));end=s.index(';',start)+1
  s=s[:start]+f'    ret={raw};is_error={cf};'+s[end:]
  (d/'production.cpp').write_text(s)
  (d/'test.cpp').write_text('''#include "production.cpp"
extern "C" int cap3_host_qmp(const void*,void**,void**,int*) { return -1; }
extern "C" int cap3_host_vq(const void*,int,cap03::VqInfo*,size_t) { return -1; }
extern "C" int cap3_host_mprotect(void*,size_t,int) { return -1; }
int main(){return cap03::kekcall_check()=='''+str(want)+'''?0:1;}
''')
  argv=['clang++','-arch','x86_64','-std=c++23','-O0','-Wno-unused-function','-DPAGE_SIZE=4096','-DPAGE_MASK=4095','-DsceKernelQueryMemoryProtection=cap3_host_qmp','-DsceKernelVirtualQuery=cap3_host_vq','-Dmprotect=cap3_host_mprotect','-I'+str(root/'src'),str(d/'test.cpp'),'-o',str(d/'test')]
  build=subprocess.run(argv,capture_output=True,text=True)
  if build.returncode:print(build.stderr);raise SystemExit(build.returncode)
  p=subprocess.run([str(d/'test')],capture_output=True,text=True,timeout=5)
  observed=f'raw_rax=0x{raw:016x} cf={cf}' in p.stdout
  ok=p.returncode==0 and observed
  print(f'raw={raw} cf={cf} expected={want}: {"PASS" if ok else "FAIL"}');failed+=not ok
print(f'cases=4 failed={failed}');sys.exit(bool(failed))
