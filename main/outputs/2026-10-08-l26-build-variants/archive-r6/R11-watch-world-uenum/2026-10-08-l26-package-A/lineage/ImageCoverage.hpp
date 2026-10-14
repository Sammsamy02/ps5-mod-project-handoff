#pragma once
#include <cstddef>
#include <cstdint>
// Pure segment policy. Profile values alone never grant live readability.
namespace rc::imagecoverage {
struct Segment { uint64_t vaddr, filesz, memsz; uint32_t flags,index; };
struct Mapping { uint64_t start,end; uint32_t protection; };
struct Hit {
 bool readable=false;uint64_t start=0,end=0,files_end=0;uint32_t protection=0,index=0;
 const char* source="none";const char* note="no loaded segment evidence";
};
inline bool range(uint64_t a,size_t n){return a&&n&&n<=UINT64_MAX-a;}
template<class Map>
inline bool live_coverage(const Map* m,size_t count,uint64_t a,size_t n){
 if(!range(a,n))return false;uint64_t cursor=a,end=a+n;
 for(size_t round=0;cursor<end&&round<count;++round){
  uint64_t next=cursor;
  for(size_t i=0;i<count;++i)if((m[i].protection&1u)&&m[i].start<=cursor&&m[i].end>cursor&&m[i].end>next)next=m[i].end;
  if(next==cursor)return false;cursor=next<end?next:end;
 }return cursor==end;
}
template<class Map>
inline Hit from_profile(const Segment* segments,size_t count,uint64_t profile_base,uint64_t observed_base,
                        uint64_t a,size_t n,const Map* maps,size_t map_count){
 Hit hit;
 if(!range(a,n)||!observed_base||observed_base!=profile_base){hit.note="invalid range or loaded base disagrees with profile";return hit;}
 for(size_t i=0;i<count;++i){const auto&s=segments[i];
  if(s.filesz>s.memsz||s.vaddr>UINT64_MAX-observed_base||s.memsz>UINT64_MAX-observed_base-s.vaddr)continue;
  const uint64_t lo=observed_base+s.vaddr,hi=lo+s.memsz;
  if(a<lo||a>=hi||n>hi-a)continue;
  hit={false,lo,hi,lo+s.filesz,s.flags,s.index,"profile-PT_LOAD+live-mappings","segment lacks readable data/BSS protection"};
  // ELF PF_R=4, PF_X=1. These bits are NOT the VM protection bits.
  if(!(s.flags&4u)||(s.flags&1u))return hit;
  if(!live_coverage(maps,map_count,a,n)){hit.note="segment hit; current readable mapping coverage missing";return hit;}
  hit.readable=true;hit.note="whole read inside one data/BSS PT_LOAD and current readable mappings";return hit;
 }
 return hit;
}
inline Hit from_module(const Mapping* segments,size_t count,uint64_t a,size_t n){
 Hit hit;if(!range(a,n))return hit;
 for(size_t i=0;i<count;++i){const auto&s=segments[i];if(a<s.start||a>=s.end||n>s.end-a)continue;
  hit={false,s.start,s.end,s.end,s.protection,static_cast<uint32_t>(i),"loaded-module-info","module segment lacks readable data protection"};
  // Module-info protection uses CPU VM bits: READ=1, EXEC=4.
  hit.readable=(s.protection&1u)&&!(s.protection&4u);
  if(hit.readable)hit.note="whole read inside reported readable data/BSS segment";
  return hit;
 }return hit;
}
}
