#!/usr/bin/env python3
"""Execute extracted, unmodified l12 production branches with fake OS/time.
Run before candidate edits; receipt retains SHA and exact generated harness.
"""
from pathlib import Path
import hashlib, json, subprocess
root = Path(__file__).resolve().parents[1]
baseline = json.loads((root/'baseline.json').read_text())
entry = next(x for x in baseline['baseline'] if x['path'].endswith('/UnrealInitializer.cpp'))
source = root.parent.parent / entry['path']
src = source.read_text()
assert hashlib.sha256(src.encode()).hexdigest() == entry['sha256'], 'frozen l12 source changed'
vq = src[src.index('enum { kPsVqQueryExact'):src.index('        // (shadow_fallback.inc)')]
loop = src[src.index('            PoolWords words{};'):src.index('            // ---- v4 entry-0 content anchor')]
# Clock seam only: loop statements, limits and branching are preserved.
loop = loop.replace('std::chrono::steady_clock::now()', 'FakeClock::now()')
preamble = r'''
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include "SeedValidation.hpp"
#define STR(x) x
struct Output { template<class...T> static void send(T&&...) {} };
const char* ensure_str(const char* s) { return s; }
namespace fmt { template<class...T> std::string format(T&&...) { return "refused"; } }
uint64_t elapsed=0; std::string events;
struct FakeClock { static auto now() { return std::chrono::steady_clock::time_point(std::chrono::milliseconds(elapsed)); } };
void CheckStartupWait() {}
void WaitForStartupRetry(std::chrono::milliseconds n) { elapsed += n.count(); }
void seed_refused(const std::string&) { throw std::runtime_error("refused"); }
struct PoolWords { uint32_t current_block=1, current_byte_offset=100; uint64_t blocks0=0x2200000000ull; uint32_t none_id=0, byte_property_id=1; };
PoolWords read_pool_words(uint64_t) { return {}; }
extern "C" int bridge_write_copyin_capture(uint64_t,const uint8_t**,uint32_t*) { events += 'C'; return 0; }
extern "C" int bridge_kernel_write_copyin_probe(uint64_t,uint64_t) { events += 'O'; return -1; }
bool l12_try_pool_admission_fallback(uint64_t,const char*) { events += 'S'; return false; }
int query_mode=0;
'''
post = r'''
extern "C" int sceKernelVirtualQuery(const void* p,int,L12VqInfo* out,size_t) {
  events += 'V'; uint64_t a=reinterpret_cast<uint64_t>(p);
  if(query_mode==1) return static_cast<int>(0x80020001u); // policy/unavailable
  out->start=reinterpret_cast<void*>(query_mode==2?a+0x1000:a);
  out->end=reinterpret_cast<void*>(a+0x2000); out->protection=1; out->flags=0x10; return 0;
}
void run_loop() {
 uint64_t pool_runtime=0x9765ec0;
 rc_seed::AddressSpan published[]={{0x400000,0x10000000}};
 uint32_t published_count=1; uint64_t capture_envelope=0x10000000;
 std::chrono::seconds validation_budget(10);
 LOOP
}
int main(int argc,char**argv) {
 std::string scenario=argv[1]; bool ok=false;
 if(scenario=="policy_error_is_inconclusive") { query_mode=1; auto r=l12_virtual_query_coverage(0x2000,6); ok=!r.hard_negative&&!r.proven; }
 if(scenario=="noncontaining_region_never_proves") { query_mode=2; auto r=l12_virtual_query_coverage(0x2000,6); ok=!r.proven; }
 if(scenario=="vq_before_oracle") { try{run_loop();}catch(...){} ok=!events.empty()&&events[0]=='V'; std::cout << "first event=" << events[0] << '\n'; }
 if(scenario=="one_60s_total_budget") { try{run_loop();}catch(...){} ok=elapsed<=60000; std::cout<<"elapsed_ms="<<elapsed<<'\n'; }
 std::cout<<(ok?"PASS ":"FAIL ")<<scenario<<'\n'; return ok?0:1;
}
'''.replace(' LOOP',loop)
harness=root/'tests/l12_extracted_red.cpp'
harness.write_text(preamble+vq+post)
binary=root/'tests/l12_extracted_red'
subprocess.run(['clang++','-std=c++17','-I'+str(root/'units'),str(harness),'-o',str(binary)],check=True)
lines=['source_sha256='+hashlib.sha256(src.encode()).hexdigest()]
for name in ['policy_error_is_inconclusive','noncontaining_region_never_proves','vq_before_oracle','one_60s_total_budget']:
 r=subprocess.run([str(binary),name],text=True,capture_output=True)
 lines += [r.stdout.strip(),f'exit={r.returncode}']
 assert r.returncode == 1, f'{name}: expected regression failure against l12'
(root/'reports/admission-red.txt').write_text('\n'.join(lines)+'\n')
print('\n'.join(lines))
