#!/usr/bin/env python3
from pathlib import Path
import subprocess,json
root=Path(__file__).resolve().parents[1];ws=root.parents[2];old=ws/'outputs/2026-10-07-l19-local-profile-r4';test=root/'tests'
s=(root/'units/UnrealInitializer.cpp').read_text();start=s.index('            require_present(SeedTableNS::kStaticConstructObject);');end=s.index('\n        }',start);block=s[start:end]
a=s.index('            auto code_identity(');b=s.index('            auto log_code_segment(',a);ops=s[a:b]
bridge=(old/'units/runtime_start_bridge_r14_diag_spanpub.cpp').read_text();a=bridge.index('extern "C" int bridge_code_cache_context(');b=bridge.index('extern "C" int bridge_write_copyin_capture(',a);bridge=bridge[a:b]
code=r'''
#include "CodeSignatureProfile.hpp"
#include <cassert>
#include <fcntl.h>
#include <unistd.h>
#include <stdexcept>
#include <iostream>
#include <sstream>
#define STR(x) x
static std::string log_text;
struct Output {template<class...T>static void send(const char*f,const T&...args){std::ostringstream o;o<<f;((o<<" "<<args),...);log_text+=o.str();}};
namespace fmt {template<class...T>std::string format(const char*f,T&&...){return f;}}
static const uint64_t base=rc::codeprofile::seeds::kBaseObserved;
static uint64_t module_view_base(){return base;}
static rc::codecache::Cache g_code_cache;
struct Owner{bool enabled=true;bool allows(){return enabled;}}g_copyin_initializer_owner;
static bool faulted=false;static bool bridge_copyin_range_faulted(uint64_t,size_t){return faulted;}
BRIDGE
struct Ops {const rc::codecache::Metadata*code_image=nullptr;uint64_t code_module_base=0,now=1;uint64_t now_ms(){return now;}OPS}admission_ops;
struct Budget{uint64_t deadline=60000;bool expired(uint64_t now){return now>=deadline;}}admission_budget;
namespace SeedTableNS=rc::codeprofile::seeds;
template<class T>void require_present(const T&){}
[[noreturn]]static void seed_refused(const std::string&why){throw std::runtime_error(why);}
namespace rc_seed {enum class SeedAddressClass{CodeCallOnly};static SeedAddressClass classify_seed_address(uint64_t,uint64_t){return SeedAddressClass::CodeCallOnly;}static const char*seed_class_name(SeedAddressClass){return "code-call-only";}}
namespace UObjectGlobals{static void*bound=nullptr;static void SetupStaticConstructObjectInternalAddress(void*p){bound=p;}}
static void bind(){BLOCK}
struct FileOps {const char*path;int open(const char*){return ::open(path,O_RDONLY);}int64_t pread(int f,void*p,size_t n,uint64_t o){return ::pread(f,p,n,o);}int close(int f){return ::close(f);}uint64_t now_ms(){return 1;}};
int main(int argc,char**argv){assert(argc==3);std::string mode=argv[1];FileOps files{argv[2]};auto auth=g_code_cache.prepare(rc::codeprofile::kProfile,base,base,base+rc::codeprofile::kProfile.extent,1,files);assert(auth.ok);auto&m=g_code_cache.metadata;bool expected=false;
 if(mode=="authenticated_cached_image")expected=true;
 else if(mode=="wrong_prefix")m.entries[1].bytes[0]^=1;
 else if(mode=="nonexec")m.segments[m.entries[1].segment].flags=4;
 else if(mode=="wrong_base")m.base++;
 else if(mode=="expired")admission_ops.now=60000;
 else if(mode=="faulted")faulted=true;
 else if(mode=="owner_denied")g_copyin_initializer_owner.enabled=false;
 else if(mode=="cleared")g_code_cache.clear();
 else return 3;
 bool passed=true;try{bind();}catch(const std::runtime_error&e){passed=false;std::cout<<e.what()<<"\n";}
 assert(passed==expected);assert((UObjectGlobals::bound!=nullptr)==expected);
 if(expected){assert(uint64_t(UObjectGlobals::bound)==base+SeedTableNS::kStaticConstructObject.label);assert(log_text.find("statically_verified_empty_callback")!=std::string::npos);assert(log_text.find("native success not observed")!=std::string::npos);}
 std::cout<<"PASS "<<mode<<"; actual slot4 block/cache bridges; no native call\n";
}
'''.replace('BRIDGE',bridge).replace('OPS',ops).replace('BLOCK',block)
(test/'actual_sco.cpp').write_text(code)
commands=[]
def run(c):
 r=subprocess.run(c,capture_output=True,text=True);commands.append({'argv':c,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});assert r.returncode==0,commands[-1]
run(['clang++','-std=c++20','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(old/'units'),str(test/'actual_sco.cpp'),'-o',str(test/'actual_sco')])
image=ws/'.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin'
for mode in ['authenticated_cached_image','wrong_prefix','nonexec','wrong_base','expired','faulted','owner_denied','cleared']:run([str(test/'actual_sco'),mode,str(image)])
(test/'SCO-RESULTS.json').write_text(json.dumps({'status':'PASS','scope':'actual new slot4 block + existing cache methods/bridges; host read-only saved image; binding only, no native invocation','commands':commands},indent=2)+'\n')
print('PASS SCO8 host controls; authenticated saved image; cached signature/address/permission/owner/deadline gates')
