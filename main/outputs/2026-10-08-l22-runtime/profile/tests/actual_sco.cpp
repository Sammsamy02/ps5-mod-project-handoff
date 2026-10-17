
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
extern "C" int bridge_code_cache_context(uint64_t module_base,const rc::codecache::Metadata** out)
{
    if(out)*out=nullptr;
    if(!out||!g_copyin_initializer_owner.allows())return 0;
    if(!g_code_cache.identity(module_base))return 0;
    *out=&g_code_cache.metadata;return 1;
}
extern "C" int bridge_code_cache_signature(uint64_t module_base,uint64_t address,
    const uint8_t* expected,size_t length)
{
    if(!g_copyin_initializer_owner.allows())return 0;
    if(!g_code_cache.identity(module_base)||bridge_copyin_range_faulted(address,length))return 0;
    return g_code_cache.prefix(address,expected,length)?1:0;
}

struct Ops {const rc::codecache::Metadata*code_image=nullptr;uint64_t code_module_base=0,now=1;uint64_t now_ms(){return now;}            auto code_identity(uint64_t address,size_t length) -> bool
            {
                const rc::codecache::Metadata* image = nullptr;
                const uint64_t actual_base = module_view_base();
                if(bridge_code_cache_context(actual_base,&image)!=1)
                {
                    code_image=nullptr;code_module_base=0;return false;
                }
                code_image=image;code_module_base=actual_base;
                // Common to live and disk proof: a named fully file-backed
                // executable PT_LOAD and any obtained real module segment
                // must agree before the signature gate queries or reads code.
                return rc::codecache::named_entry(*image,address,length)!=nullptr;
            }
            auto cached_code_signature(uint64_t address,const uint8_t* expected,size_t length) -> bool
            {
                return bridge_code_cache_signature(module_view_base(),address,expected,length)==1;
            }
}admission_ops;
struct Budget{uint64_t deadline=60000;bool expired(uint64_t now){return now>=deadline;}}admission_budget;
namespace SeedTableNS=rc::codeprofile::seeds;
template<class T>void require_present(const T&){}
[[noreturn]]static void seed_refused(const std::string&why){throw std::runtime_error(why);}
namespace rc_seed {enum class SeedAddressClass{CodeCallOnly};static SeedAddressClass classify_seed_address(uint64_t,uint64_t){return SeedAddressClass::CodeCallOnly;}static const char*seed_class_name(SeedAddressClass){return "code-call-only";}}
namespace UObjectGlobals{static void*bound=nullptr;static void SetupStaticConstructObjectInternalAddress(void*p){bound=p;}}
static void bind(){            require_present(SeedTableNS::kStaticConstructObject);
            const uint64_t sco_runtime = base + SeedTableNS::kStaticConstructObject.label;
            // Code remains call-only. Identity uses the already authenticated
            // startup cache; this does not read the live executable mapping.
            if (const auto cls = rc_seed::classify_seed_address(sco_runtime, base); cls != rc_seed::SeedAddressClass::CodeCallOnly)
            {
                seed_refused(fmt::format("slot 4 static_construct_object 0x{:X}: seed class {} (expected exec-only call-only); refusing bind", sco_runtime, rc_seed::seed_class_name(cls)));
            }
            if(admission_budget.expired(admission_ops.now_ms()))
                seed_refused("SCO shared admission deadline expired before binding");
            if(!admission_ops.code_identity(sco_runtime,sizeof(rc::codeprofile::kScoPrefix))||
               !admission_ops.cached_code_signature(sco_runtime,rc::codeprofile::kScoPrefix,sizeof(rc::codeprofile::kScoPrefix)))
                seed_refused("slot 4 static_construct_object: authenticated cached image/signature admission failed");
            UObjectGlobals::SetupStaticConstructObjectInternalAddress(reinterpret_cast<void*>(sco_runtime));
            Output::send(STR("[Seeds] slot 4 static_construct_object_internal bound at {} (code; statically_verified_empty_callback; native success not observed)\n"),
                         reinterpret_cast<void*>(sco_runtime));}
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
