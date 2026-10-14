#!/usr/bin/env python3
"""Mac-only call6 frame check, then a bounded six-argument Win64 call."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
source = (root / 'src/win64_bridge.S').read_text()
body = source.split('GLOBALFN(cap13_win64_call6)', 1)[1].split('// ===', 1)[0]
allocation = re.search(r'sub\s+rsp,\s*(0x[0-9a-fA-F]+)', body)
slots = [int(x, 16) for x in re.findall(r'mov\s+\[rsp\s*\+\s*(0x[0-9a-fA-F]+)\],\s*r(?:9|10|11)', body)]
if not allocation or len(slots) != 2 or max(slots) + 8 > int(allocation.group(1), 16):
    print(f'call6 frame bounds FAIL: allocation={allocation.group(1) if allocation else None} slots={slots}')
    raise SystemExit(1)
print(f'call6 frame bounds PASS: allocation={allocation.group(1)} slots={slots}')

build = root / 'build'
build.mkdir(exist_ok=True)
test = build / 'bridge6_guest.cpp'
test.write_text(r'''
#include <cstdint>
#include <cstdio>
extern "C" uint64_t cap13_win64_call5(void*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
extern "C" uint64_t cap13_win64_call6(void*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
static uint64_t seen[6];
extern "C" __attribute__((ms_abi)) uint64_t guest5(uint64_t a,uint64_t b,uint64_t c,
                                                     uint64_t d,uint64_t e) {
    seen[0]=a;seen[1]=b;seen[2]=c;seen[3]=d;seen[4]=e;
    return UINT64_C(0x123456789abcdef0);
}
extern "C" __attribute__((ms_abi)) uint64_t guest(uint64_t a,uint64_t b,uint64_t c,
                                                    uint64_t d,uint64_t e,uint64_t f) {
    seen[0]=a;seen[1]=b;seen[2]=c;seen[3]=d;seen[4]=e;seen[5]=f;
    return UINT64_C(0x123456789abcdef0);
}
int main() {
    const uint64_t ret5=cap13_win64_call5((void*)&guest5,11,22,33,44,55);
    bool ok5=ret5==UINT64_C(0x123456789abcdef0);
    for(int i=0;i<5;++i)ok5=ok5 && seen[i]==uint64_t((i+1)*11);
    std::printf("call5 guest %s args=%llu,%llu,%llu,%llu,%llu\n",ok5?"PASS":"FAIL",
        (unsigned long long)seen[0],(unsigned long long)seen[1],
        (unsigned long long)seen[2],(unsigned long long)seen[3],
        (unsigned long long)seen[4]);
    const uint64_t ret=cap13_win64_call6((void*)&guest,11,22,33,44,55,66);
    bool ok=ok5 && ret==UINT64_C(0x123456789abcdef0);
    for(int i=0;i<6;++i)ok=ok && seen[i]==uint64_t((i+1)*11);
    std::printf("call6 guest %s ret=0x%llx args=%llu,%llu,%llu,%llu,%llu,%llu\n",
        ok?"PASS":"FAIL",(unsigned long long)ret,
        (unsigned long long)seen[0],(unsigned long long)seen[1],
        (unsigned long long)seen[2],(unsigned long long)seen[3],
        (unsigned long long)seen[4],(unsigned long long)seen[5]);
    return ok?0:1;
}
''')
exe = build / 'bridge6_guest'
compile_result = subprocess.run(['clang++','-arch','x86_64','-std=c++17','-O1','-Wall','-Wextra',
    str(test),str(root/'src/win64_bridge.S'),'-o',str(exe)],capture_output=True,text=True,timeout=20)
if compile_result.returncode:
    print(compile_result.stderr)
    raise SystemExit(compile_result.returncode)
run = subprocess.run(['arch','-x86_64',str(exe)],capture_output=True,text=True,timeout=5)
print(run.stdout,end='')
if run.stderr:print(run.stderr,file=sys.stderr)
raise SystemExit(run.returncode)
