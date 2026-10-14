
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
#include "CodeImageCache.hpp"
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
static uint64_t module_view_base(){return 0x400000;}
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
        namespace admission = rc::pooladmission;
        struct L13VqInfo
        {
            void* start;
            void* end;
            int64_t offset;
            int32_t protection;
            int32_t memory_type;
            uint8_t flags;
            char name[32];
            uint8_t gpu_mask_id;
        };
        static_assert(sizeof(L13VqInfo) == 0x48, "SceKernelVirtualQueryInfo layout drifted");
        extern "C" int sceKernelVirtualQuery(const void*, int, L13VqInfo*, size_t);
        extern "C" int bridge_kernel_write_copyin_probe_until(uint64_t, uint64_t, uint64_t);
        extern "C" int bridge_copyin_range_faulted(uint64_t, uint64_t);
        extern "C" void bridge_copyin_initializer_complete(void);
        extern "C" int bridge_code_cache_context(uint64_t,const rc::codecache::Metadata**);
        extern "C" int bridge_code_cache_signature(uint64_t,uint64_t,const uint8_t*,size_t);

        extern "C" int bridge_image_range(uint64_t,size_t,rc::imagecoverage::Hit*);
        static const char* l14_verdict(admission::Verdict v) {
            switch(v) {
            case admission::Verdict::Readable:return "readable";
            case admission::Verdict::HardNegative:return "hard-negative";
            case admission::Verdict::Unavailable:return "unavailable";
            case admission::Verdict::Expired:return "expired";
            case admission::Verdict::Invalid:return "invalid";
            }return "unknown";
        }
        static const char* l14_source(admission::Source s) {
            switch(s) {
            case admission::Source::ImageSegments:return "image-segments";
            case admission::Source::VirtualQuery:return "virtual-query";
            case admission::Source::CopyinCapture:return "copyin-owned-read";
            default:return "none";
            }
        }
        struct L13AdmissionOps
        {
            const rc::codecache::Metadata* code_image = nullptr;
            uint64_t code_module_base = 0;
            auto code_identity(uint64_t address,size_t length) -> bool
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
            auto log_code_segment(uint64_t address,size_t length,const char* source) -> void
            {
                if(!code_image)return;
                const auto* entry=rc::codecache::named_entry(*code_image,address,length);
                if(!entry)return;
                const auto& segment=code_image->segments[entry->segment];
                const uint64_t start=code_image->base+segment.vaddr;
                // Every value below is authenticated owned cache metadata or
                // a retained module-info observation; no target text read.
                Output::send(STR("[Seeds][L18] code-signature={} pin={} needed=[0x{:X},0x{:X}) profile-PT_LOAD={} ELF-flags=0x{:X} bounds=[0x{:X},0x{:X}) file-end=0x{:X} prefix-file-offset=0x{:X} image-SHA256={} startup-observed-base=0x{:X} module-view-base=0x{:X} base-source={} module-info={}\n"),
                    ensure_str(source),ensure_str(entry->name),address,address+length,segment.index,segment.flags,
                    start,start+segment.memsz,start+segment.filesz,entry->offset,ensure_str(code_image->sha256),
                    code_image->base,code_module_base,code_image->base_source,code_image->module_info);
                for(size_t i=0;i<code_image->reported_count;++i)
                {
                    const auto& reported=code_image->reported[i];
                    if(reported.start<=address&&reported.end>address&&length<=reported.end-address)
                        Output::send(STR("[Seeds][L18] code module-segment={} bounds=[0x{:X},0x{:X}) protection=0x{:X}\n"),
                            i,reported.start,reported.end,reported.protection);
                }
            }
            bool shadow_enabled = true;
            unsigned oracle_calls = 0;
            bool shadow_considered = false;
            bool is_image(uint64_t address) const {
                return address >= SeedTableNS::kBaseObserved &&
                       address < SeedTableNS::kBaseObserved + rc_seed::kImageFileExtentBytes;
            }
            auto image_range(uint64_t address,size_t length) -> rc::imagecoverage::Hit {
                rc::imagecoverage::Hit hit{};bridge_image_range(address,length,&hit);return hit;
            }
            void log_image(uint64_t address,size_t length,const rc::imagecoverage::Hit& hit) {
                Output::send(STR("[Seeds][L14] image needed=[0x{:X},0x{:X}) segment-hit={} segment={} bounds=[0x{:X},0x{:X}) file-end=0x{:X} protection=0x{:X} source={} note={}\n"),
                    address,address+length,hit.readable,hit.index,hit.start,hit.end,hit.files_end,hit.protection,
                    ensure_str(hit.source),ensure_str(hit.note));
            }
            void log_check(const char* label,uint64_t address,size_t length,const admission::Result& result,const admission::Budget& budget) {
                Output::send(STR("[Seeds][L14] check={} needed=[0x{:X},0x{:X}) result={} source={} note={} oracle-probes={} shadow-considered={} shadow-byte-proof=disabled elapsed={}ms budget={}ms\n"),
                    ensure_str(label),address,address+length,ensure_str(l14_verdict(result.verdict)),ensure_str(l14_source(result.source)),
                    ensure_str(result.note),oracle_calls,shadow_considered,now_ms()-budget.start_ms,admission::kAdmissionBudgetMs);
            }
            auto now_ms() const -> uint64_t
            {
                return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    FakeClock::now().time_since_epoch()).count());
            }
            auto wait_ms(uint64_t duration) -> void
            {
                Output::send(STR("[Seeds][L14] admission not ready; retry in {}ms within shared deadline\n"), duration);
                WaitForStartupRetry(std::chrono::milliseconds(duration));
            }
            auto known_fault(uint64_t address, size_t length) -> bool
            {
                return bridge_copyin_range_faulted(address, length) != 0;
            }
            auto query(uint64_t address) -> admission::Query
            {
                CheckStartupWait();
                L13VqInfo info{};
                const int result = sceKernelVirtualQuery(reinterpret_cast<const void*>(address), 0, &info, sizeof(info));
                if (result != 0)
                {
                    // No documented exact-unmapped return code is established
                    // for this firmware. An API/policy error proves no mapping
                    // fact, so it remains inconclusive and can fall to copyin.
                    Output::send(STR("[Seeds][L14] VirtualQuery address=0x{:X} rc=0x{:X}: unavailable/inconclusive\n"),
                                 address, static_cast<uint32_t>(result));
                    return {admission::QueryKind::Unavailable, 0, 0, 0, 0, result};
                }
                return {admission::QueryKind::Available, reinterpret_cast<uint64_t>(info.start),
                        reinterpret_cast<uint64_t>(info.end), static_cast<uint32_t>(info.protection), info.flags, result};
            }
            auto log_region(uint64_t address, size_t length, const admission::Query& query) -> void
            {
                Output::send(STR("[Seeds][L14] VirtualQuery needed=[0x{:X},0x{:X}) region=[0x{:X},0x{:X}) protection=0x{:X} flags=0x{:X}\n"),
                             address, address + length, query.start, query.end, query.protection, query.flags);
            }
            auto capture(uint64_t address, size_t, const uint8_t** bytes, size_t* length) -> bool
            {
                uint32_t available = 0;
                const int result = bridge_write_copyin_capture(address, bytes, &available);
                *length = available;
                return result == 1;
            }
            auto probe(uint64_t address, size_t length, admission::Budget& budget) -> int
            {
                CheckStartupWait();
                ++oracle_calls;
                return bridge_kernel_write_copyin_probe_until(address, length, budget.deadline_ms);
            }
            auto shadow(admission::Budget&) -> bool
            {
                shadow_considered = true;
                // shadow_arm hashes/scans the complete disk image with no
                // cancellation seam. It can corroborate seed identity, but
                // cannot prove any live heap bytes. Do not start that work
                // inside this admission deadline or claim it admits a range.
                Output::send(STR("[Seeds][L14] shadow fallback considered after VQ/copyin unavailable: "
                                 "declined; image identity cannot prove live heap readability; "
                                 "deadline-aware shadow byte source unavailable\n"));
                return false;
            }
            auto read_direct(uint64_t address, size_t length, uint8_t* bytes) -> void
            {
                // Sole direct heap-read site: the engine calls this only
                // after proving this exact address/length with VirtualQuery.
                host_direct(address,length,bytes);
            }
        };

        struct L14TotalAdmissionLog {
            L13AdmissionOps& ops;admission::Budget& budget;const char* outcome="exception-or-cancellation";bool finished=false;
            void finish() noexcept {
                if(finished)return;finished=true;
                try {Output::send(STR("[Seeds][L14] total admission elapsed={}ms budget={}ms outcome={}\n"),
                    ops.now_ms()-budget.start_ms,admission::kAdmissionBudgetMs,ensure_str(outcome));}catch(...){}
            }
            ~L14TotalAdmissionLog() noexcept {finish();}
        };

        struct L15BindingLog {
            L13AdmissionOps& ops;admission::Budget& budget;const char* outcome="refused-or-exception";
            ~L15BindingLog() noexcept {
                try {Output::send(STR("[Seeds][L15] FName binding total elapsed={}ms shared-budget={}ms outcome={}\n"),
                    ops.now_ms()-budget.start_ms,admission::kAdmissionBudgetMs,ensure_str(outcome));}catch(...){}
            }
        };


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
 L13AdmissionOps admission_ops;admission::Budget admission_budget(0);
            // This one admission deadline began at startup cache preparation;
            // context/worker delay, settings IO, entry proofs and every retry
            // sleep consume its remaining time. OS calls are not preemptible.
            // The legacy per-scan setting must never multiply this budget.
            (void)validation_budget;

            L14TotalAdmissionLog total_admission{admission_ops,admission_budget};
            require_present(SeedTableNS::kFNameCtorWchar);
            const uint64_t pool_runtime = base + SeedTableNS::kFNamePoolLabel;
            if(admission_budget.expired(admission_ops.now_ms()))
                seed_refused("shared code-image/admission deadline expired before settings IO");
            const auto setting = read_pool_admission("/data/plugins/UE4SS-settings.ini");
            admission_ops.shadow_enabled = setting.mode != PoolAdmissionMode::Oracle;
            Output::send(STR("[Seeds][L14] pool admission: one total {}ms budget; mode={}; image-segment proof with VirtualQuery cross-check; heap VirtualQuery first; collect-all independent checks\n"),
                admission::kAdmissionBudgetMs,ensure_str(pool_admission_mode_name(setting.mode)));
            admission::Session admission_session;
            rc::fnamechecks::Evaluation evaluation;
            const auto result=admission::retry(admission_ops,admission_budget,[&]() -> admission::Result {
                CheckStartupWait();
                evaluation=rc::fnamechecks::evaluate(admission_ops,admission_budget,admission_session,pool_runtime);
                return evaluation.result;
            });
            const auto& words=evaluation.words;
            const char* pool_admission_source=evaluation.used_vq && evaluation.used_copyin ? "image-segments + virtual-query + copyin-owned-read" :
                evaluation.used_vq ? "image-segments + virtual-query" :
                evaluation.used_copyin ? "image-segments + copyin-owned-read" : "image-segments";
            total_admission.outcome=result.verdict==admission::Verdict::Readable?"admitted":"refused";
            total_admission.finish();
            if(result.verdict!=admission::Verdict::Readable)
                seed_refused(fmt::format("FName pool at 0x{:X}: {} (CurrentBlock {}, Blocks[0] 0x{:X}; elapsed {}ms; budget {}ms; all independent checks logged; unsafe dependencies blocked)",
                    pool_runtime,result.note,words.current_block,words.blocks0,admission_ops.now_ms()-admission_budget.start_ms,admission::kAdmissionBudgetMs));
            Output::send(STR("[Seeds] slot 1 fname pool 0x{:X}: admitted via {} (all exact needed ranges readable; elapsed {}ms)\n"),
                pool_runtime,ensure_str(pool_admission_source),admission_ops.now_ms()-admission_budget.start_ms);
            Output::send(STR("[Seeds] FName pool validated at {} (CurrentBlock {}, Blocks[0] 0x{:X}; source: {}; 'None' anchor ok; 'None' id 0x{:X}, 'ByteProperty' id 0x{:X}; elapsed {}ms)\n"),
                reinterpret_cast<void*>(pool_runtime),words.current_block,words.blocks0,ensure_str(pool_admission_source),words.none_id,words.byte_property_id,
                admission_ops.now_ms()-admission_budget.start_ms);


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
