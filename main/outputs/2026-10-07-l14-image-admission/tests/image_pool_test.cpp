#include "FNamePoolChecks.hpp"
#include "ImageCoverage.hpp"
#include "ImageLoadProfile.hpp"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace rc::pooladmission;
#define CHECK(x) do { if(!(x)) { std::fprintf(stderr,"FAIL %s:%d %s\n",__func__,__LINE__,#x);std::exit(1); } } while(0)
constexpr uint64_t base=0x400000, pool=0x9765ec0, heap=0x22c0000000;
struct Ops {
 uint64_t time=0,query_cost=0;bool shadow_enabled=true,segments=true,live_maps=true,vq_unavailable=false;
 uint64_t negative=0,missing=0,fault=0;size_t reads=0,probes=0,shadows=0;
 std::map<uint64_t,uint8_t> memory;std::vector<uint64_t> queries,direct;std::vector<std::string> labels;std::vector<Result> results;
 std::string segment_source;
 Ops(){
 rc_seed::VmSpanRecord records[]={{0x96dc000,0x9a34000,3},{heap,heap+0x20000,3}};rc_seed::AddressSpan table[64];
 const auto publication=rc_seed::publish_readable_spans(records,2,base,rc_seed::kImageFileExtentBytes,table,64);
 CHECK(publication.published==1);CHECK(table[0].lo==heap);CHECK(!(pool>=table[0].lo&&pool<table[0].hi));
 put(pool+8,0,4);put(pool+12,64,4);put(pool+16,heap,8);put(pool+0x14040,0,4);put(pool+0x14044,4,4);put(heap,4u<<6,2);for(unsigned i=0;i<4;++i)memory[heap+2+i]="None"[i];put(heap+8,12u<<6,2);for(unsigned i=0;i<12;++i)memory[heap+10+i]="ByteProperty"[i];}
 void put(uint64_t a,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)memory[a+i]=static_cast<uint8_t>(v>>(i*8));}
 uint64_t now_ms()const{return time;}void wait_ms(uint64_t n){time+=n;}
 bool known_fault(uint64_t a,size_t n){return fault&&a<=fault&&fault<a+n;}
 bool is_image(uint64_t a)const{return a>=base&&a<base+rc_seed::kImageFileExtentBytes;}
 rc::imagecoverage::Hit image_range(uint64_t a,size_t n){
  if(!segments)return {};
  rc::imagecoverage::Mapping maps[]={{0x96dc000,0x9a34000,3}};
  return rc::imagecoverage::from_profile(rcprofile::kLoadSegments,5,base,base,a,n,maps,live_maps?1:0);
 }
 void log_image(uint64_t,size_t,const rc::imagecoverage::Hit& h){segment_source=h.source;}
 Query query(uint64_t a){queries.push_back(a);time+=query_cost;if(vq_unavailable)return {};if(a==negative)return {QueryKind::Available,a,a+4,0,0x10,0};return {QueryKind::Available,is_image(a)?0x96dc000:heap,is_image(a)?0x9a34000:heap+0x20000,(a==negative||a==missing)?0u:1u,0x10,0};}
 void log_region(uint64_t,size_t,const Query&){}
 bool capture(uint64_t,size_t,const uint8_t**p,size_t*n){*p=nullptr;*n=0;return false;}
 int probe(uint64_t,size_t,Budget&){++probes;return 2;}bool shadow(Budget&){++shadows;return false;}
 void read_direct(uint64_t a,size_t n,uint8_t*d){++reads;direct.push_back(a);CHECK(a!=negative);CHECK(a!=fault);for(size_t i=0;i<n;++i){CHECK(memory.count(a+i));d[i]=memory[a+i];}}
 void log_check(const char*l,uint64_t,size_t,const Result&r,const Budget&){labels.push_back(l);results.push_back(r);}
};
void exact_case(){Ops o;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::Readable);CHECK(e.words.blocks0==heap);CHECK(o.probes==0);CHECK(o.segment_source=="profile-PT_LOAD+live-mappings");CHECK(o.labels.size()>=10);CHECK(o.time<60000);}
void collect_all(){Ops o;o.negative=pool+8;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::HardNegative);CHECK(o.labels.size()>=10);CHECK(std::find(o.queries.begin(),o.queries.end(),pool+0x14044)!=o.queries.end());CHECK(std::find(o.direct.begin(),o.direct.end(),o.negative)==o.direct.end());CHECK(o.shadows==0);}
void image_needs_segments(){Ops o;o.segments=false;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict!=Verdict::Readable);CHECK(o.reads==0);CHECK(o.labels.size()>=10);}
void profile_alone_insufficient(){Ops o;o.live_maps=false;o.vq_unavailable=true;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict!=Verdict::Readable);CHECK(o.reads==0);}
void image_vq_unavailable(){Ops o;o.vq_unavailable=true;Budget b(0);Session s;uint8_t bytes[4];CHECK(read_image(o,b,s,pool+8,4,bytes).verdict==Verdict::Readable);CHECK(o.reads==1);CHECK(o.probes==0);}
void heap_negative(){Ops o;o.negative=heap;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::HardNegative);CHECK(o.probes==0);CHECK(o.shadows==0);CHECK(o.labels.size()>=10);}
void known_fault(){Ops o;o.fault=pool+8;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::HardNegative);CHECK(std::find(o.direct.begin(),o.direct.end(),pool+8)==o.direct.end());}
void deadline(){Ops o;o.query_cost=61000;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::Expired);CHECK(o.reads==0);CHECK(o.labels.size()>=10);CHECK(o.time==61000);CHECK(o.time<300000);}
void segment_edges(){using namespace rc::imagecoverage;Mapping m[]={{0x96dc000,0x9a34000,3}};CHECK(from_profile(rcprofile::kLoadSegments,5,base,base,0x9a33414,4,m,1).readable);CHECK(!from_profile(rcprofile::kLoadSegments,5,base,base,0x9a33414,8,m,1).readable);CHECK(!from_profile(rcprofile::kLoadSegments,5,base,0x800000,pool,4,m,1).readable);CHECK(!from_profile(rcprofile::kLoadSegments,5,base,base,UINT64_MAX-1,4,m,1).readable);}
void module_segments(){using namespace rc::imagecoverage;Mapping segs[]={{0x953c000,0x9a33418,3}};auto h=from_module(segs,1,pool+8,4);CHECK(h.readable);CHECK(h.source==std::string("loaded-module-info"));CHECK(!from_module(segs,1,0x9a33416,4).readable);segs[0].protection=4;CHECK(!from_module(segs,1,pool,4).readable);}
void disconnected_mappings(){using namespace rc::imagecoverage;Mapping m[]={{pool,pool+2,3},{pool+3,pool+8,3}};CHECK(!from_profile(rcprofile::kLoadSegments,5,base,base,pool,4,m,2).readable);m[1].start=pool+2;CHECK(from_profile(rcprofile::kLoadSegments,5,base,base,pool,4,m,2).readable);}
void content_failure_collects(){Ops o;o.memory[heap+2]='X';Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::Invalid);CHECK(std::find(o.labels.begin(),o.labels.end(),"ByteProperty-entry")!=o.labels.end());}
void unreadable_pointer_collects(){Ops o;o.negative=pool+16;Budget b(0);Session s;auto e=rc::fnamechecks::evaluate(o,b,s,pool);CHECK(e.result.verdict==Verdict::HardNegative);CHECK(std::find(o.labels.begin(),o.labels.end(),"None-anchor")!=o.labels.end());for(auto a:o.direct)CHECK(a!=heap&&a!=heap+8);}
void image_exec_and_slack_refuse(){using namespace rc::imagecoverage;Mapping m[]={{base,0xc064000,7}};CHECK(!from_profile(rcprofile::kLoadSegments,5,base,base,base+8,4,m,1).readable);CHECK(!from_profile(rcprofile::kLoadSegments,5,base,base,0x9a33418,4,m,1).readable);}
int main(){module_segments();disconnected_mappings();content_failure_collects();unreadable_pointer_collects();image_exec_and_slack_refuse();exact_case();collect_all();image_needs_segments();profile_alone_insufficient();image_vq_unavailable();heap_negative();known_fault();deadline();segment_edges();std::puts("PASS 14 image/pool integration scenarios; exact image base/pool/heap fixture");}
