#!/usr/bin/env python3
"""Compile actual initializer adapter and Phase 3 against fake native OS/time.
Only steady_clock::now and already-gated metadata readers are replaced.
Heap read_direct, orchestration and engine are production source verbatim.
"""
from pathlib import Path
import subprocess, hashlib
root=Path(__file__).resolve().parents[1]
src=(root/'units/UnrealInitializer.cpp').read_text()
adapter=src[src.index('        namespace admission ='):src.index('        static auto consume_required_slot_seeds')]
adapter=adapter.replace('std::chrono::steady_clock::now()', 'FakeClock::now()')
phase=src[src.index('            // This one admission deadline'):src.index('            // ---- Phase 4: bind the ctor')]
pre=r'''
#include <chrono>
#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include "PoolAdmissionEngine.hpp"
#include "pool_admission.h"
#include "SeedValidation.hpp"
#include "SeedTable.h"
namespace SeedTableNS=RC::Unreal::UnrealInitializer::SeedTable;
#define STR(x) x
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(std::string("ASSERT ")+ #x);}while(0)
std::string log_text,events; uint64_t fake_ms=0; unsigned vq_calls=0,sink_calls=0,metadata_reads=0; bool mapped=true,readable=true,span_ok=true,metadata_short=false,bridge_fault=false,second_fault=false,ready=true;
bool capture_ready=false;uint64_t captured_addr=0;uint32_t captured_len=0;int oracle_result=-1;uint64_t sent_deadline=0;
alignas(16) uint8_t heap_bytes[64]{};
alignas(16) uint8_t copied_bytes[64]{};
struct Output { template<class...T> static void send(const char* text,T&&...args){std::ostringstream o;o<<text;((o<<" "<<args),...);log_text+=o.str();} };
const char* ensure_str(const char*s){return s;}
namespace fmt{template<class...T>std::string format(const char*t,T&&...args){std::ostringstream o;o<<t;((o<<" "<<args),...);return o.str();}}
struct FakeClock{static auto now(){return std::chrono::steady_clock::time_point(std::chrono::milliseconds(fake_ms));}};
void CheckStartupWait(){if(fake_ms>=300000)throw std::runtime_error("outer 300s timeout");}
void WaitForStartupRetry(std::chrono::milliseconds n){fake_ms+=n.count();CheckStartupWait();}
[[noreturn]] void seed_refused(const std::string&s){throw std::runtime_error(s);}
template<class T>void require_present(const T&){}
struct PoolWords{uint32_t current_block=0,current_byte_offset=64;uint64_t blocks0=0;uint32_t none_id=0,byte_property_id=4;};
PoolWords read_pool_words(uint64_t){++metadata_reads;PoolWords w;w.blocks0=reinterpret_cast<uint64_t>(heap_bytes);if(!ready)w.byte_property_id=0;return w;}
uint64_t read_u64(const volatile uint8_t*,uint64_t){return reinterpret_cast<uint64_t>(heap_bytes);}
uint32_t bridge_readable_span_count(){return span_ok?1:0;}
int bridge_readable_span_at(uint32_t,uint64_t*lo,uint64_t*hi){*lo=0x9000000;*hi=metadata_short?0x9765ed8:0xa000000;return 1;}
extern "C" int bridge_write_copyin_capture(uint64_t a,const uint8_t**out,uint32_t*n){events+='C';const bool found=capture_ready&&a>=captured_addr&&a-captured_addr<captured_len;*out=found?copied_bytes+(a-reinterpret_cast<uint64_t>(heap_bytes)):nullptr;*n=found?captured_len-static_cast<uint32_t>(a-captured_addr):0;return found?1:0;}
'''
post=r'''
extern "C" int sceKernelVirtualQuery(const void*ptr,int flags,L13VqInfo* out,size_t size){
 events+='V';++vq_calls;REQUIRE(flags==0);REQUIRE(size==0x48);
 if(!mapped)return static_cast<int>(0x80020001u);
 out->flags=0x10;out->start=heap_bytes;out->end=heap_bytes+64;out->protection=(readable&&!(second_fault&&ptr==heap_bytes+8))?1:0;return 0;
}
extern "C" int bridge_copyin_range_faulted(uint64_t,uint64_t){return bridge_fault?1:0;}
extern "C" int bridge_kernel_write_copyin_probe_until(uint64_t address,uint64_t length,uint64_t deadline){events+='O';++sink_calls;sent_deadline=deadline;if(oracle_result==1){capture_ready=true;captured_addr=address;captured_len=static_cast<uint32_t>(length);}return oracle_result;}
void phase3(){const uint64_t base=SeedTableNS::kBaseObserved;std::chrono::seconds validation_budget(10);
PHASE
}
int main(int argc,char**argv){
 const std::string scenario=argv[1];
 heap_bytes[0]=0;heap_bytes[1]=1;std::memcpy(heap_bytes+2,"None",4);
 heap_bytes[8]=0;heap_bytes[9]=3;std::memcpy(heap_bytes+10,"ByteProperty",12);
 std::memcpy(copied_bytes,heap_bytes,64);
 bool expect_refused=false;
 if(scenario=="positive_vq"){}
 else if(scenario=="oracle_capture"){mapped=false;oracle_result=1;}
 else if(scenario=="unavailable_budget"){mapped=false;expect_refused=true;}
 else if(scenario=="negative_blocks_shadow"){readable=false;expect_refused=true;}
 else if(scenario=="faulted_capture_excluded"){bridge_fault=true;capture_ready=true;expect_refused=true;}
 else if(scenario=="entry_range_negative"){second_fault=true;expect_refused=true;}
 else if(scenario=="metadata_range_missing"){metadata_short=true;expect_refused=true;}
 else if(scenario=="zero_spans"){span_ok=false;expect_refused=true;}
 else if(scenario=="not_ready_budget"){ready=false;expect_refused=true;}
 else return 2;
 std::string refusal;
 try{phase3();}catch(const std::exception&e){refusal=e.what();}
 try{
 REQUIRE(expect_refused==!refusal.empty());
 if(scenario=="positive_vq"){REQUIRE(events=="VVV");REQUIRE(sink_calls==0);REQUIRE(log_text.find("virtual-query")!=std::string::npos);REQUIRE(log_text.find("region=[")!=std::string::npos);}
 if(scenario=="oracle_capture"){REQUIRE(events[0]=='V');REQUIRE(sink_calls==2);REQUIRE(sent_deadline==60000);REQUIRE(log_text.find("copyin-owned-read")!=std::string::npos);}
 if(scenario=="unavailable_budget"){REQUIRE(fake_ms==60000);REQUIRE(sink_calls==1);REQUIRE(log_text.find("shadow fallback considered")!=std::string::npos);REQUIRE(log_text.find("identity cannot prove live heap")!=std::string::npos);}
 if(scenario=="negative_blocks_shadow"){REQUIRE(events=="V");REQUIRE(fake_ms==0);REQUIRE(log_text.find("shadow fallback considered")==std::string::npos);}
 if(scenario=="faulted_capture_excluded"){REQUIRE(events.empty());REQUIRE(fake_ms==0);}
 if(scenario=="entry_range_negative"){REQUIRE(events=="VVV");REQUIRE(sink_calls==0);REQUIRE(refusal.find("no CPU-read")!=std::string::npos);}
 if(scenario=="metadata_range_missing"||scenario=="zero_spans"){REQUIRE(metadata_reads==0);REQUIRE(events.empty());}
 if(scenario=="not_ready_budget"){REQUIRE(fake_ms==60000);REQUIRE(events.empty());}
 REQUIRE(fake_ms<300000);
 std::cout<<"PASS "<<scenario<<" elapsed_ms="<<fake_ms<<" vq="<<vq_calls<<" sink="<<sink_calls<<'\n';
 }catch(const std::exception&e){std::cerr<<"FAIL "<<scenario<<": "<<e.what()<<" refusal="<<refusal<<" events="<<events<<'\n';return 1;}
}
'''.replace('PHASE',phase)
harness=root/'tests/initializer_admission_extracted.cpp'
harness.write_text(pre+adapter+post)
binary=root/'tests/initializer_admission_extracted'
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I'+str(root/'units'),str(harness),'-o',str(binary)],check=True)
lines=['initializer_sha256='+hashlib.sha256(src.encode()).hexdigest()]
for name in ['positive_vq','oracle_capture','unavailable_budget','negative_blocks_shadow','faulted_capture_excluded','entry_range_negative','metadata_range_missing','zero_spans','not_ready_budget']:
 r=subprocess.run([str(binary),name],capture_output=True,text=True)
 lines += [r.stdout.strip(),r.stderr.strip(),f'exit={r.returncode}']
 if r.returncode: print('\n'.join(lines));raise SystemExit(r.returncode)
(root/'reports/initializer-admission-green.txt').write_text('\n'.join(line for line in lines if line)+'\n')
print('\n'.join(line for line in lines if line))
