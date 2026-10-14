#include <chrono>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iostream>
#include "PoolAdmissionEngine.hpp"
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
#include "NativePropertyReadBackend.inl"
int main(){
 uint64_t source=0x1234,out=0;lo=reinterpret_cast<uintptr_t>(&source);hi=lo+8;
 assert(PropertyReadBytes(lo,8,&out)&&out==source);
 fault=true;queries=0;out=0;assert(!PropertyReadBytes(lo,8,&out)&&out==0&&queries==0);fault=false;
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
