
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "ImageCoverage.hpp"
#include "ImageLoadProfile.hpp"
#include "SeedValidation.hpp"
constexpr size_t kBridgeVmmapEntries=32768;constexpr uint64_t kCandidateBase=0x400000,kImageExtentVaddr=0xbc602c8;
namespace lmf{
 struct OsVmMapping{uint64_t start,end;int32_t protection;};
 struct Seg{void* address;uint32_t size;int32_t prot;};
 struct LiveModuleFactsSnapshot{void*observed_base;struct{bool obtained;uint32_t segment_count;Seg segments[4];}module_info;};
}
void diagnostic_event(const char*,uint32_t){}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %s\\n",#x);std::exit(1);}}while(0)

#include <string>
static rc_seed::VmSpanRecord g_spanpub_records[kBridgeVmmapEntries];  // WHOLE frozen snapshot (all records — non-readable records still prove enumeration coverage for the envelope)
static rc_seed::AddressSpan g_published_readable_spans[rc_seed::kMaxPublishedSpans];
static uint64_t g_spanpub_envelope_end = 0;  // max captured record end (truncation honesty)
static uint32_t g_published_readable_span_count = 0;
static uint32_t g_published_readable_span_total = 0;
static size_t g_image_mapping_count = 0;

static void bridge_spanpub_publish(const lmf::OsVmMapping *mappings, size_t count,
                                   uint64_t extent_lo, uint64_t extent_bytes)
{
    g_published_readable_span_count = 0;
    g_published_readable_span_total = 0;
    g_spanpub_envelope_end = 0;
    // Copy the WHOLE frozen snapshot (same-capture rule: this is the array
    // this function's caller just filled with bridge_query_vmentry — no
    // second sysctl). The pure filter skips non-readable records itself but
    // still counts them toward the captured envelope.
    const size_t bounded = count < kBridgeVmmapEntries ? count : kBridgeVmmapEntries;
    for (size_t i = 0; i < bounded; ++i) {
        g_spanpub_records[i].start = mappings[i].start;
        g_spanpub_records[i].end = mappings[i].end;
        g_spanpub_records[i].protection = static_cast<uint32_t>(mappings[i].protection);
    }
    g_image_mapping_count = bounded;
    const rc_seed::PublicationResult pub = rc_seed::publish_readable_spans(
        g_spanpub_records, bounded, extent_lo, extent_bytes,
        g_published_readable_spans, rc_seed::kMaxPublishedSpans);
    g_published_readable_span_count = pub.published;
    g_published_readable_span_total = pub.total;
    g_spanpub_envelope_end = pub.envelope_end;
    ::diagnostic_event("bridge_spanpub_count", pub.published);
    ::diagnostic_event("bridge_spanpub_total", pub.total);
    ::diagnostic_event("bridge_spanpub_cap", pub.cap_saturated);
    ::diagnostic_event("bridge_spanpub_env_low", (uint32_t)(pub.envelope_end & 0xffffffffu));
    ::diagnostic_event("bridge_spanpub_env_high", (uint32_t)(pub.envelope_end >> 32));
}

static uint64_t g_loaded_main_base = 0;
static rc::imagecoverage::Mapping g_loaded_module_segments[4]{};
static size_t g_loaded_module_segment_count = 0;
static bool g_loaded_module_info = false;

static void bridge_capture_loaded_image(const lmf::LiveModuleFactsSnapshot& facts) noexcept
{
    g_loaded_main_base = reinterpret_cast<uint64_t>(facts.observed_base);
    g_loaded_module_segment_count = 0;
    g_loaded_module_info = facts.module_info.obtained;
    if(g_loaded_module_info) {
        for(size_t i=0;i<facts.module_info.segment_count && i<4;++i) {
            const auto& s=facts.module_info.segments[i];
            const uint64_t lo=reinterpret_cast<uint64_t>(s.address);
            if(!lo || !s.size || s.size>UINT64_MAX-lo) { g_loaded_module_info=false;break; }
            g_loaded_module_segments[g_loaded_module_segment_count++]={lo,lo+s.size,static_cast<uint32_t>(s.prot)};
        }
    }
    ::diagnostic_event("image_segment_source",g_loaded_module_info?1u:2u);
    ::diagnostic_event("image_segment_base_low",static_cast<uint32_t>(g_loaded_main_base));
    ::diagnostic_event("image_segment_base_high",static_cast<uint32_t>(g_loaded_main_base>>32));
    ::diagnostic_event("image_segment_mapping_count",static_cast<uint32_t>(g_image_mapping_count));
}
extern "C" int bridge_image_range(uint64_t address,size_t length,rc::imagecoverage::Hit* out)
{
    if(!out)return 0;
    *out={};
    if(!g_loaded_main_base || g_loaded_main_base!=kCandidateBase) {
        out->note="loaded main-image base unavailable or profile mismatch";return 0;
    }
    if(g_loaded_module_info) {
        *out=rc::imagecoverage::from_module(g_loaded_module_segments,g_loaded_module_segment_count,address,length);
    } else {
        // Label this fallback accurately. These are captured PT_LOAD profile
        // bounds corroborated by THIS capture's real VM records, not an
        // invented successful sceKernelGetModuleInfo result.
        *out=rc::imagecoverage::from_profile(rcprofile::kLoadSegments,
            sizeof(rcprofile::kLoadSegments)/sizeof(rcprofile::kLoadSegments[0]),
            kCandidateBase,g_loaded_main_base,address,length,g_spanpub_records,g_image_mapping_count);
    }
    return out->readable?1:0;
}


int main(){
 lmf::OsVmMapping mappings[]={{0x96dc000,0x9a34000,3},{0x22c0000000,0x22c0020000,3}};
 bridge_spanpub_publish(mappings,2,kCandidateBase,kImageExtentVaddr);CHECK(g_published_readable_span_count==1);CHECK(g_published_readable_spans[0].lo==0x22c0000000);
 lmf::LiveModuleFactsSnapshot facts{};facts.observed_base=reinterpret_cast<void*>(kCandidateBase);bridge_capture_loaded_image(facts);
 rc::imagecoverage::Hit hit;CHECK(bridge_image_range(0x9765ec8,4,&hit)==1);CHECK(hit.index==7);CHECK(hit.start==0x953c000);CHECK(hit.end==0x9a33418);
 CHECK(bridge_image_range(0x9a33416,4,&hit)==0);CHECK(bridge_image_range(0x22c0000000,4,&hit)==0);
 facts.observed_base=nullptr;bridge_capture_loaded_image(facts);CHECK(bridge_image_range(0x9765ec8,4,&hit)==0);
 facts.observed_base=reinterpret_cast<void*>(kCandidateBase);facts.module_info.obtained=true;facts.module_info.segment_count=1;facts.module_info.segments[0]={reinterpret_cast<void*>(0x953c000),0x4f7418,3};bridge_capture_loaded_image(facts);CHECK(bridge_image_range(0x9765ec8,4,&hit)==1);CHECK(hit.source==std::string("loaded-module-info"));
 facts.module_info.segments[0].prot=4;bridge_capture_loaded_image(facts);CHECK(bridge_image_range(0x9765ec8,4,&hit)==0);
 std::puts("PASS production bridge publisher/capture/getter: image exclusion preserved; segment proof succeeds");
}
