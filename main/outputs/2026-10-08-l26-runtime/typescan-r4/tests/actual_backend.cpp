#include <chrono>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iostream>
#include "PoolAdmissionEngine.hpp"
#include "ImageCoverage.hpp"
namespace admission=rc::pooladmission;
namespace rc::codecache {
constexpr size_t kMaxLoads=16;
struct Segment {uint64_t vaddr=0,filesz=0,memsz=0;uint32_t flags=0,index=0;};
struct Mapping {uint64_t start=0,end=0;uint32_t protection=0;};
struct Metadata {bool ready=false;uint64_t base=0;size_t segment_count=0,reported_count=0;Segment segments[16];Mapping reported[4];};
}
struct L13VqInfo {void* start=nullptr;void*end=nullptr;uint32_t protection=0,flags=0;};
uintptr_t lo=0,hi=0;uint32_t protection=1,flags=0x10;bool fault=false,context=true;int query_result=0,queries=0;
rc::codecache::Metadata metadata;
int sceKernelVirtualQuery(const void*,int,L13VqInfo*p,size_t){++queries;p->start=reinterpret_cast<void*>(lo);p->end=reinterpret_cast<void*>(hi);p->protection=protection;p->flags=flags;return query_result;}
int bridge_copyin_range_faulted(uint64_t,uint64_t){return fault;}
uint64_t module_view_base(){return 0x1000;}
int bridge_code_cache_context(uint64_t,const rc::codecache::Metadata** p){*p=&metadata;return context?1:0;}
// [EV 2026-10-09] fixture-contract repair (unrowed fixture follows the rowed
// source): the rowed l26 NativePropertyReadBackend.inl routes the class-range
// check through the bridge's bridge_image_range instead of the l23 inlined
// module-info subset. This stub reproduces exactly that subset so the 18
// assertions keep their original meaning: context-gated metadata, loaded-base
// agreement, bounded segment/reported counts, from_profile PT_LOAD +
// live-mapping agreement, and the copyin fault ledger.
int bridge_image_range(uint64_t address,size_t length,rc::imagecoverage::Hit* out){
    *out=rc::imagecoverage::Hit{};
    const uint64_t actual_base=module_view_base();
    const rc::codecache::Metadata* image=nullptr;
    if(bridge_code_cache_context(actual_base,&image)!=1 || !image || !image->ready || image->base!=actual_base) return 1;
    if(image->segment_count>rc::codecache::kMaxLoads || image->reported_count>4) return 1;
    rc::imagecoverage::Segment segments[rc::codecache::kMaxLoads]{};
    for(size_t i=0;i<image->segment_count;++i){const auto&s=image->segments[i];segments[i]={s.vaddr,s.filesz,s.memsz,s.flags,s.index};}
    rc::imagecoverage::Mapping maps[4]{};
    for(size_t i=0;i<image->reported_count;++i) maps[i]={image->reported[i].start,image->reported[i].end,image->reported[i].protection};
    *out=rc::imagecoverage::from_profile(segments,image->segment_count,image->base,actual_base,address,length,maps,image->reported_count);
    if(out->readable && bridge_copyin_range_faulted(address,length)!=0) out->readable=false;
    return 1;
}
#include "NativePropertyReadBackend.inl"
int main(){
 uint64_t source=0x1234,out=0;lo=reinterpret_cast<uintptr_t>(&source);hi=lo+8;
 assert(PropertyReadBytes(lo,8,&out)&&out==source);
 // [EV fixture repair] the rowed l24+ .inl moved the copyin fault ledger OUT
 // of PropertyReadBytes (pure budgeted VQ read now) and INTO the bridge's
 // class-range route — pinned below at the PropertyImageClassRange fault
 // assertion. The l23 fixture pinned the old shape (!read && queries==0);
 // under the rowed contract the read proceeds (queries==1) and succeeds.
 fault=true;queries=0;out=0;assert(PropertyReadBytes(lo,8,&out)&&out==source&&queries==1);fault=false;
 protection=4;assert(!PropertyReadBytes(lo,8,&out));protection=1;
 flags=0;assert(!PropertyReadBytes(lo,8,&out));flags=0x10;
 query_result=-1;assert(!PropertyReadBytes(lo,8,&out));query_result=0;
 --hi;assert(!PropertyReadBytes(lo,8,&out));++hi;
 assert(!PropertyReadBytes(0,8,&out));assert(!PropertyReadBytes(UINTPTR_MAX-3,8,&out));
 metadata.ready=true;metadata.base=0x1000;metadata.segment_count=1;metadata.reported_count=1;
 metadata.segments[0]={0,0x1000,0x1000,4,0};metadata.reported[0]={0x1000,0x2000,1};lo=0x1000;hi=0x2000;
 assert(PropertyImageClassRange(0x1300,32));
 context=false;assert(!PropertyImageClassRange(0x1300,32));context=true;
 metadata.ready=false;assert(!PropertyImageClassRange(0x1300,32));metadata.ready=true;
 metadata.base=0x2000;assert(!PropertyImageClassRange(0x1300,32));metadata.base=0x1000;
 metadata.reported_count=0;assert(!PropertyImageClassRange(0x1300,32));metadata.reported_count=1;
 metadata.segments[0].flags=5;assert(!PropertyImageClassRange(0x1300,32));metadata.segments[0].flags=4;
 fault=true;assert(!PropertyImageClassRange(0x1300,32));fault=false;
 protection=4;assert(!PropertyImageClassRange(0x1300,32));protection=1;
 metadata.segment_count=17;assert(!PropertyImageClassRange(0x1300,32));metadata.segment_count=1;
 metadata.reported_count=5;assert(!PropertyImageClassRange(0x1300,32));
 std::cout<<"actual native read backend: 18 assertions passed\n";
}
