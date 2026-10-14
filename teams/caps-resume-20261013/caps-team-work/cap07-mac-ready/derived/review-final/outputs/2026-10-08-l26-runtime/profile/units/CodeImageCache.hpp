#pragma once
#include "CodeImageSha256.hpp"
#include "CopyinSink.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cerrno>

// Startup-only streaming authentication; sparse immutable proofs, never a
// readable-memory service. Ops owns public Open/Pread/Close and steady time.
namespace rc::codecache {
// [Seeds][L26/L27 pin admission, agent-EQ 2026-10-09] kMaxPins 4 -> 8: the
// code profile now registers six pins (fname ctor, SCO, engine thread name,
// engine FName::ToStringInternal, engine Add/RemoveUObjectDeleteListener).
// The old cap of 4 silently capped admission at slots 2/4 + thread: any
// UNREGISTERED label failed named_entry (exact runtime+size pin match) and
// was refused as 'identity/base unavailable or mismatched' — the 2026-10-09
// 11:36 L27/Phase-6 refusals. Keep BOTH compiled copies of this header
// byte-identical (lineage overlay + l19-local-profile-r4/units): Metadata
// layout must agree in every TU that calls named_entry on the bridge's cache.
constexpr size_t kMaxPins=16,kMaxLoads=16,kMaxPhdrs=32,kChunk=16384;
constexpr uint64_t kBudgetMs=60000;
struct Pin { uint32_t id;const char* name;uint64_t linked;const uint8_t* bytes;size_t size; };
struct Profile { const char* path;const char* sha256;uint64_t size,extent,base;const Pin* pins;size_t count; };
struct Segment { uint64_t vaddr,offset,filesz,memsz;uint32_t flags,index; };
struct ReportedSegment { uint64_t start=0,end=0;uint32_t protection=0; };
struct Entry { uint32_t id=0;const char* name=nullptr;uint64_t linked=0,runtime=0,offset=0;size_t size=0,collected=0;uint32_t segment=0;uint8_t bytes[64]{}; };
struct Metadata { bool ready=false;uint64_t base=0,extent=0,size=0,generation=0,start_ms=0,deadline_ms=0;char sha256[65]{};size_t count=0,segment_count=0;Entry entries[kMaxPins]{};Segment segments[kMaxLoads]{};bool module_info=false;uint32_t base_source=0;size_t reported_count=0;ReportedSegment reported[4]{}; };
struct Result { bool ok=false;const char* reason="invalid code-image profile";int64_t open_raw=-1,read_raw=0;int open_error=0,read_error=0,close_raw=0,close_error=0;uint32_t failed_io_op=0;uint64_t bytes_read=0,start_ms=0,deadline_ms=0;size_t verified_pins=0; };
inline uint16_t ld16(const uint8_t*p){return uint16_t(p[0])|uint16_t(uint16_t(p[1])<<8);}
inline uint32_t ld32(const uint8_t*p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(p[i])<<(i*8);return v;}
inline uint64_t ld64(const uint8_t*p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(p[i])<<(i*8);return v;}
inline bool hex_pin(const char*p){if(!p)return false;for(unsigned i=0;i<64;++i)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')||(p[i]>='A'&&p[i]<='F')))return false;return p[64]=='\0';}
inline const Entry* named_entry(const Metadata&m,uint64_t address,size_t n) noexcept {
 if(!m.ready)return nullptr;
 for(size_t i=0;i<m.count;++i){const auto&e=m.entries[i];if(e.runtime!=address||e.size!=n)continue;
  if(e.segment>=m.segment_count)return nullptr;const auto&s=m.segments[e.segment];
  if(!(s.flags&1u)||(s.flags&2u)||e.linked<s.vaddr||e.linked-s.vaddr>s.filesz||n>s.filesz-(e.linked-s.vaddr))return nullptr;
  if(m.module_info){bool covered=false;for(size_t j=0;j<m.reported_count;++j){const auto&r=m.reported[j];if(r.start<=address&&r.end>address&&n<=r.end-address&&(r.protection&4u)&&!(r.protection&2u))covered=true;}if(!covered)return nullptr;}
  return &e;
 }return nullptr;
}

class Cache {
 static bool headers(const Profile&p,const uint8_t*bytes,size_t n,Metadata&m,Result&r){
  r.reason="invalid or unsupported ELF header";
  if(n<64||std::memcmp(bytes,"\177ELF",4)!=0||bytes[4]!=2||bytes[5]!=1||bytes[6]!=1||ld16(bytes+18)!=62||ld32(bytes+20)!=1||ld16(bytes+52)!=64)return false;
  const uint64_t off=ld64(bytes+32);const size_t width=ld16(bytes+54),count=ld16(bytes+56);
  r.reason="ELF program-header table exceeds fixed checked bounds";
  if(width!=56||!count||count>kMaxPhdrs||off>n||count>(n-size_t(off))/width)return false;
  uint64_t extent=0;
  for(size_t i=0;i<count;++i){const auto*h=bytes+off+i*width;if(ld32(h)!=1)continue;
   Segment s{ld64(h+16),ld64(h+8),ld64(h+32),ld64(h+40),ld32(h+4),uint32_t(i)};
   r.reason="PT_LOAD file or virtual bounds overflow/truncation";
   if(s.filesz>s.memsz||s.offset>p.size||s.filesz>p.size-s.offset||s.memsz>UINT64_MAX-s.vaddr||s.vaddr>UINT64_MAX-p.base||s.memsz>UINT64_MAX-p.base-s.vaddr)return false;
   if(!s.memsz)continue;
   if(m.segment_count==kMaxLoads){r.reason="PT_LOAD count exceeds fixed cache limit";return false;}
   for(size_t j=0;j<m.segment_count;++j){const auto&old=m.segments[j];if(s.vaddr<old.vaddr+old.memsz&&old.vaddr<s.vaddr+s.memsz){r.reason="overlapping virtual PT_LOAD bounds";return false;}}
   m.segments[m.segment_count++]=s;if(s.vaddr+s.memsz>extent)extent=s.vaddr+s.memsz;
  }
  if(!m.segment_count||extent!=p.extent){r.reason="authenticated ELF extent differs from profile";return false;}
  for(size_t i=0;i<p.count;++i){const auto&pin=p.pins[i];Entry e{};e.id=pin.id;e.name=pin.name;e.linked=pin.linked;e.runtime=p.base+pin.linked;e.size=pin.size;
   size_t hits=0;for(size_t j=0;j<m.segment_count;++j){const auto&s=m.segments[j];if(pin.linked<s.vaddr)continue;const uint64_t delta=pin.linked-s.vaddr;
    if(delta>s.filesz||pin.size>s.filesz-delta)continue;
    if(!(s.flags&1u)||(s.flags&2u)){r.reason="function pin is not in immutable executable PT_LOAD";return false;}
    e.offset=s.offset+delta;e.segment=uint32_t(j);++hits;
   }
   if(hits!=1){r.reason="function prefix is not fully file-backed in exactly one executable PT_LOAD";return false;}
   m.entries[i]=e;
  }return true;
 }
 template<class Ops>struct FileOwner { Ops&ops;Result&r;int fd;
  void close(){if(fd<0)return;const int consumed=fd;fd=-1;errno=0;r.close_raw=ops.close(consumed);r.close_error=l13_sink::error(r.close_raw,errno);if(r.close_raw!=0&&!r.close_error)r.close_error=EIO;if(r.close_raw!=0||r.close_error)r.failed_io_op=4;}
  ~FileOwner(){close();}
 };
public:
 Metadata metadata{};
 void clear() noexcept {metadata={};}
 bool identity(uint64_t module_base)const noexcept{return metadata.ready&&module_base!=0&&module_base==metadata.base;}
 const Entry* entry(uint64_t address,size_t n)const noexcept{return named_entry(metadata,address,n);}
 const Entry* prefix(uint64_t address,const uint8_t*expected,size_t n)const noexcept{
  const auto*e=entry(address,n);if(!e||!expected||std::memcmp(e->bytes,expected,n)!=0)return nullptr;
  return e;
 }
 template<class Ops>
 Result prepare(const Profile&p,uint64_t observed,uint64_t resolved,uint64_t resolved_end,uint64_t generation,Ops&ops){
  clear();Result r{};r.start_ms=ops.now_ms();r.deadline_ms=r.start_ms>UINT64_MAX-kBudgetMs?UINT64_MAX:r.start_ms+kBudgetMs;
  if(!p.path||p.path[0]!='/'||!hex_pin(p.sha256)||p.size<64||p.size>uint64_t(INT64_MAX)||!p.extent||!p.base||!p.pins||!p.count||p.count>kMaxPins)return r;
  if(!observed||observed!=p.base||resolved!=observed||p.extent>UINT64_MAX-observed||resolved_end!=observed+p.extent){r.reason="actual startup base/extent differs from pinned image";return r;}
  for(size_t i=0;i<p.count;++i){const auto&pin=p.pins[i];if(!pin.id||!pin.name||!pin.bytes||!pin.size||pin.size>64||pin.linked>UINT64_MAX-observed||pin.size>UINT64_MAX-observed-pin.linked)return r;for(size_t j=0;j<i;++j)if(pin.id==p.pins[j].id||pin.linked==p.pins[j].linked){r.reason="duplicate function pin";return r;}}
  const auto expired=[&](){return ops.now_ms()>=r.deadline_ms;};
  if(expired()){r.reason="code-image shared deadline expired before open";return r;}
  errno=0;r.open_raw=ops.open(p.path);r.open_error=l13_sink::error(r.open_raw,errno);
  if(r.open_error||r.open_raw<0||r.open_raw>INT32_MAX){if(!r.open_error)r.open_error=EIO;r.failed_io_op=1;r.reason="code-image startup open failed";return r;}
  FileOwner<Ops>owner{ops,r,int(r.open_raw)};Metadata candidate{};candidate.base=observed;candidate.extent=p.extent;candidate.size=p.size;candidate.generation=generation;candidate.start_ms=r.start_ms;candidate.deadline_ms=r.deadline_ms;candidate.count=p.count;
  const bool proved=[&](){
   uint8_t chunk[kChunk];codehash::Sha256State sha;codehash::sha256_init(&sha);uint64_t offset=0;bool parsed=false;
   while(offset<p.size){const size_t wanted=p.size-offset<kChunk?size_t(p.size-offset):kChunk;size_t have=0;
    while(have<wanted){if(expired()){r.reason="code-image shared deadline expired before read";return false;}
     errno=0;r.read_raw=ops.pread(owner.fd,chunk+have,wanted-have,offset+have);r.read_error=l13_sink::error(r.read_raw,errno);
     if(r.read_error||r.read_raw<=0||uint64_t(r.read_raw)>wanted-have){r.reason=r.read_error?"code-image startup read failed":"code-image truncated or invalid read count";if(!r.read_error)r.read_error=EIO;r.failed_io_op=3;return false;}
     const size_t got=size_t(r.read_raw);r.bytes_read+=got;
     if(expired()){r.reason="code-image shared deadline expired after read";return false;}
     codehash::sha256_update(&sha,chunk+have,got);have+=got;
     if(expired()){r.reason="code-image shared deadline expired after hash chunk";return false;}
    }
    if(!parsed){if(!headers(p,chunk,have,candidate,r))return false;parsed=true;}
    for(size_t i=0;i<candidate.count;++i){auto&e=candidate.entries[i];const uint64_t hi=offset+have,entry_hi=e.offset+e.size;
     const uint64_t lo=offset>e.offset?offset:e.offset,end=hi<entry_hi?hi:entry_hi;
     if(lo<end){const size_t n=size_t(end-lo);std::memcpy(e.bytes+size_t(lo-e.offset),chunk+size_t(lo-offset),n);e.collected+=n;}
    }offset+=have;if(expired()){r.reason="code-image shared deadline expired after prefix extraction";return false;}
   }
   if(expired()){r.reason="code-image shared deadline expired before EOF check";return false;}
   errno=0;r.read_raw=ops.pread(owner.fd,chunk,1,p.size);r.read_error=l13_sink::error(r.read_raw,errno);
   if(r.read_error||r.read_raw!=0){r.reason="code-image trailing byte or EOF-read failure";if(r.read_raw<0&&!r.read_error)r.read_error=EIO;if(r.read_error)r.failed_io_op=3;return false;}
   if(expired()){r.reason="code-image shared deadline expired after EOF check";return false;}
   uint8_t digest[32];codehash::sha256_final(&sha,digest);const char hex[]="0123456789abcdef";
   for(size_t i=0;i<32;++i){candidate.sha256[i*2]=hex[digest[i]>>4];candidate.sha256[i*2+1]=hex[digest[i]&15u];}candidate.sha256[64]=0;
   if(!codehash::sha256_hex_equal(candidate.sha256,p.sha256)){r.reason="whole code-image SHA256 mismatch";return false;}
   for(size_t i=0;i<candidate.count;++i){const auto&e=candidate.entries[i];if(e.collected!=e.size||std::memcmp(e.bytes,p.pins[i].bytes,e.size)!=0){r.reason="authenticated function prefix mismatches profile";return false;}++r.verified_pins;}
   if(expired()){r.reason="code-image shared deadline expired before publication";return false;}return true;
  }();
  owner.close();
  if(r.close_raw!=0||r.close_error){r.reason="code-image close failed; consumed once without retry";return r;}
  if(!proved)return r;
  if(expired()){r.reason="code-image shared deadline expired after close";return r;}
  candidate.ready=true;metadata=candidate;r.ok=true;r.reason="whole image authenticated; all function prefixes cached; fd closed";return r;
 }
};
}
