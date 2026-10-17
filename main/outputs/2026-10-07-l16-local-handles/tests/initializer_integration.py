from pathlib import Path
import subprocess,json,hashlib,sys
root=Path(__file__).resolve().parents[1]
src=(root/'units/UnrealInitializer.cpp').read_text()
adapter=src[src.index('        namespace admission ='):src.index('        // Retained public UE4SS Function API')]
adapter=adapter.replace('std::chrono::steady_clock::now()','FakeClock::now()')
old='''                const auto* source = reinterpret_cast<const volatile uint8_t*>(address);
                for (size_t i = 0; i < length; ++i) bytes[i] = source[i];'''
assert old in adapter
adapter=adapter.replace(old,'                host_direct(address,length,bytes);')
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
#include <map>
#include "PoolAdmissionEngine.hpp"
#include "FNamePoolChecks.hpp"
#include "ImageLoadProfile.hpp"
#include "pool_admission.h"
#include "SeedValidation.hpp"
#include "SeedTable.h"
namespace SeedTableNS=RC::Unreal::UnrealInitializer::SeedTable;
#define STR(x) x
#define REQUIRE(x) do{if(!(x))throw std::runtime_error(std::string("ASSERT ")+ #x);}while(0)
std::string log_text;uint64_t fake_ms=0;std::map<uint64_t,uint8_t> memory;
constexpr uint64_t pool=0x9765ec0,heap=0x22c0000000;unsigned direct_count=0,vq_count=0,sink_count=0;
uint64_t negative=0,prior_fault=0;bool segment_available=true,mapping_available=true,vq_available=true;uint64_t query_cost=0;
uint8_t captured[32]{};uint64_t cap_address=0;unsigned cap_size=0;int oracle=2;
struct Output{template<class...T>static void send(const char*t,T&&...args){std::ostringstream o;o<<t;((o<<" "<<args),...);log_text+=o.str();}};
const char* ensure_str(const char*s){return s;}
namespace fmt{template<class...T>std::string format(const char*t,T&&...args){std::ostringstream o;o<<t;((o<<" "<<args),...);return o.str();}}
struct FakeClock{static auto now(){return std::chrono::steady_clock::time_point(std::chrono::milliseconds(fake_ms));}};
void CheckStartupWait(){if(fake_ms>=300000)throw std::runtime_error("outer 300s timeout");}
void WaitForStartupRetry(std::chrono::milliseconds n){fake_ms+=n.count();CheckStartupWait();}
[[noreturn]]void seed_refused(const std::string&s){throw std::runtime_error(s);}
template<class T>void require_present(const T&){}
void host_direct(uint64_t a,size_t n,uint8_t*d){++direct_count;REQUIRE(a!=negative);REQUIRE(a!=prior_fault);for(size_t i=0;i<n;++i){REQUIRE(memory.count(a+i));d[i]=memory[a+i];}}
extern "C" int bridge_write_copyin_capture(uint64_t a,const uint8_t**out,uint32_t*n){bool yes=a>=cap_address&&a-cap_address<cap_size;*out=yes?captured+(a-cap_address):nullptr;*n=yes?cap_size-static_cast<unsigned>(a-cap_address):0;return yes?1:0;}
'''
post=r'''
extern "C" int bridge_image_range(uint64_t a,size_t n,rc::imagecoverage::Hit*out){
 *out={};if(!segment_available)return 0;rc::imagecoverage::Mapping maps[]={{0x96dc000,0x9a34000,3}};
 *out=rc::imagecoverage::from_profile(rcprofile::kLoadSegments,5,0x400000,0x400000,a,n,maps,mapping_available?1:0);return out->readable?1:0;
}
extern "C" int sceKernelVirtualQuery(const void*p,int flags,L13VqInfo*out,size_t n){
 REQUIRE(flags==0&&n==0x48);++vq_count;fake_ms+=query_cost;uint64_t a=reinterpret_cast<uint64_t>(p);
 if(!vq_available)return static_cast<int>(0x80020001u);
 out->flags=0x10;out->protection=a==negative?0:1;
 bool image=a>=0x400000&&a<0xc0602c8;
 out->start=reinterpret_cast<void*>(a==negative?a:image?0x96dc000:heap);
 out->end=reinterpret_cast<void*>(a==negative?a+4:image?0x9a34000:heap+0x20000);return 0;
}
extern "C" int bridge_copyin_range_faulted(uint64_t a,uint64_t n){return prior_fault&&a<=prior_fault&&prior_fault<a+n?1:0;}
extern "C" int bridge_kernel_write_copyin_probe_until(uint64_t a,uint64_t n,uint64_t deadline){
 REQUIRE(deadline==60000);++sink_count;if(oracle==1){cap_address=a;cap_size=static_cast<unsigned>(n);for(unsigned i=0;i<n;++i)captured[i]=memory.at(a+i);}return oracle;
}
void phase3(){uint64_t base=SeedTableNS::kBaseObserved;std::chrono::seconds validation_budget(10);
PHASE
}
void put(uint64_t a,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)memory[a+i]=static_cast<uint8_t>(v>>(i*8));}
int main(int,char**argv){
 put(pool+8,0,4);put(pool+12,64,4);put(pool+16,heap,8);put(pool+0x14040,0,4);put(pool+0x14044,4,4);
 put(heap,4u<<6,2);for(unsigned i=0;i<4;++i)memory[heap+2+i]="None"[i];put(heap+8,12u<<6,2);for(unsigned i=0;i<12;++i)memory[heap+10+i]="ByteProperty"[i];
 // The SAME publication function used by the native bridge excludes the image.
 rc_seed::VmSpanRecord records[]={{0x96dc000,0x9a34000,3},{heap,heap+0x20000,3}};rc_seed::AddressSpan table[64];
 auto pub=rc_seed::publish_readable_spans(records,2,0x400000,rc_seed::kImageFileExtentBytes,table,64);
 REQUIRE(pub.published==1&&table[0].lo==heap);
 std::string scenario=argv[1];bool refuse=false;
 if(scenario=="exact_console"){}
 else if(scenario=="metadata_negative"){negative=pool+8;refuse=true;}
 else if(scenario=="heap_negative"){negative=heap;refuse=true;}
 else if(scenario=="missing_segment"){segment_available=false;refuse=true;}
 else if(scenario=="profile_no_live_mapping"){mapping_available=false;vq_available=false;refuse=true;}
 else if(scenario=="image_crosscheck_unavailable"){vq_available=false;oracle=1;}
 else if(scenario=="known_fault"){prior_fault=pool+8;refuse=true;}
 else if(scenario=="deadline"){query_cost=61000;refuse=true;}
 else return 2;
 std::string error;try{phase3();}catch(const std::exception&e){error=e.what();}
 REQUIRE(refuse==!error.empty());REQUIRE(fake_ms<300000);
 REQUIRE(log_text.find("total admission elapsed")!=std::string::npos);
 REQUIRE(log_text.find("ByteProperty-id")!=std::string::npos);
 REQUIRE(log_text.find("ByteProperty-entry")!=std::string::npos);
 if(!refuse){REQUIRE(log_text.find("admitted via")!=std::string::npos);REQUIRE(direct_count>=7);}
 if(scenario=="metadata_negative"||scenario=="heap_negative"||scenario=="known_fault"){REQUIRE(sink_count==0);REQUIRE(fake_ms==0);}
 if(scenario=="missing_segment"||scenario=="profile_no_live_mapping"){REQUIRE(direct_count==0);REQUIRE(fake_ms==60000);}
 if(scenario=="deadline"){REQUIRE(direct_count==0);REQUIRE(fake_ms==61000);}
 std::cout<<"PASS "<<scenario<<" elapsed_ms="<<fake_ms<<" direct_reads="<<direct_count<<" VQ="<<vq_count<<" probes="<<sink_count<<'\n';
 if(scenario=="exact_console"||scenario=="metadata_negative")std::cout<<log_text<<'\n';
}
'''.replace('PHASE',phase)
p=root/'tests/initializer_actual_extracted.cpp';p.write_text(pre+adapter+post)
binary=root/'tests/initializer_actual_extracted'
c=['clang++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'units'),str(p),'-o',str(binary)]
r=subprocess.run(c,capture_output=True,text=True);receipts=[{'argv':c,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr}]
if r.returncode:print(r.stderr);sys.exit(r.returncode)
for name in ['exact_console','metadata_negative','heap_negative','missing_segment','profile_no_live_mapping','image_crosscheck_unavailable','known_fault','deadline']:
 r=subprocess.run([str(binary),name],capture_output=True,text=True);receipts.append({'scenario':name,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr})
 print(r.stdout.splitlines()[0] if r.stdout else r.stderr)
 if r.returncode:print(r.stderr);break
(root/'receipts/initializer-integration.json').write_text(json.dumps({'source_sha256':hashlib.sha256(src.encode()).hexdigest(),'native_boundary_replacements':['steady_clock::now','read_direct OS byte-copy body'],'commands':receipts},indent=2)+'\n')
sys.exit(r.returncode)
