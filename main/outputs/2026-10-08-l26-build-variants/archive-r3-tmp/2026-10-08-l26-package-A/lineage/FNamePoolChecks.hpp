#pragma once
#include "PoolAdmissionEngine.hpp"
#include "SeedValidation.hpp"
// One safe diagnostic round. Independent checks continue after a refusal;
// unknown dependencies produce explicit blocked records, never guessed reads.
namespace rc::fnamechecks {
namespace admission=rc::pooladmission;
struct Words {uint32_t current_block=0,current_byte_offset=0;uint64_t blocks0=0;uint32_t none_id=0,byte_property_id=0;};
struct Evaluation { admission::Result result{admission::Verdict::Readable,admission::Source::None,"all checks passed"};Words words{};bool used_vq=false,used_copyin=false,used_image=false; };
inline uint64_t decode(const uint8_t*b,size_t n){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(b[i])<<(i*8);return v;}
inline int rank(admission::Verdict v){using V=admission::Verdict;return v==V::HardNegative?4:v==V::Invalid?3:v==V::Expired?2:v==V::Unavailable?1:0;}
template<class Ops>
Evaluation evaluate(Ops& ops,admission::Budget& budget,admission::Session& session,uint64_t pool){
 using namespace admission;Evaluation e;
 const auto record=[&](const char*label,uint64_t a,size_t n,const Result&r){
  ops.log_check(label,a,n,r,budget);
  if(rank(r.verdict)>rank(e.result.verdict))e.result=r;
  e.used_vq|=r.source==Source::VirtualQuery;e.used_copyin|=r.source==Source::CopyinCapture;e.used_image|=r.source==Source::ImageSegments;
 };
 const auto exact=[&](const char*label,uint64_t a,size_t n,uint8_t*bytes){
  const auto r=ops.is_image(a)?read_image(ops,budget,session,a,n,bytes):read(ops,budget,session,a,n,bytes);
  record(label,a,n,r);return r.verdict==Verdict::Readable;
 };
 const auto blocked=[&](const char*label,const char*why){record(label,0,0,{Verdict::Unavailable,Source::None,why});};
 uint8_t bytes[32]{};bool ok[5]{};
 const uint64_t offsets[]={rc_seed::kPoolLayout.current_block_offset,rc_seed::kPoolLayout.current_byte_offset_offset,
  rc_seed::kPoolLayout.blocks_offset,rc_seed::kPoolLayout.none_id_offset,rc_seed::kPoolLayout.byte_property_id_offset};
 const char*labels[]={"current-block","current-byte-offset","blocks[0]-pointer","None-id","ByteProperty-id"};
 for(unsigned i=0;i<5;++i){
  const size_t n=i==2?8:4;
  if(offsets[i]>UINT64_MAX-pool){record(labels[i],pool,n,{Verdict::Invalid,Source::None,"metadata address overflow"});continue;}
  ok[i]=exact(labels[i],pool+offsets[i],n,bytes);
  if(!ok[i])continue;const auto v=decode(bytes,n);
  if(i==0)e.words.current_block=static_cast<uint32_t>(v);
  else if(i==1)e.words.current_byte_offset=static_cast<uint32_t>(v);
  else if(i==2)e.words.blocks0=v;
  else if(i==3)e.words.none_id=static_cast<uint32_t>(v);
  else e.words.byte_property_id=static_cast<uint32_t>(v);
 }
 const bool block_valid=ok[2]&&e.words.blocks0&&(e.words.blocks0&7u)==0;
 if(block_valid){
  if(exact("None-anchor",e.words.blocks0,2+rc_seed::kNoneString.size(),bytes)){
   const auto c=rc_seed::check_none_anchor(bytes,2+rc_seed::kNoneString.size());
   record("None-anchor-content",e.words.blocks0,6,{c.ok?Verdict::Readable:Verdict::Invalid,Source::None,c.ok?"None anchor matches":c.failure});
  }
 }else blocked("None-anchor","blocked: Blocks[0] pointer unreadable, null or misaligned");
 const auto entry=[&](const char*label,const char*pointer_label,uint32_t id,bool id_ok,std::string_view expected){
  if(!ok[0]||!ok[1]||!id_ok){blocked(pointer_label,"blocked: entry metadata unavailable");blocked(label,"blocked: entry metadata unavailable");return;}
  const auto c=rc_seed::check_entry_bounds(id,e.words.current_block,e.words.current_byte_offset,2+expected.size());
  if(!c.ok){record(pointer_label,0,0,{Verdict::Unavailable,Source::None,"blocked: entry bounds not ready"});record(label,0,0,{Verdict::Unavailable,Source::None,c.failure});return;}
  const auto location=rc_seed::entry_location(id);
  const uint64_t off=rc_seed::kPoolLayout.blocks_offset+location.block_index*8;
  if(off>UINT64_MAX-pool){record(pointer_label,pool,8,{Verdict::Invalid,Source::None,"indexed pointer address overflow"});blocked(label,"blocked: pointer address overflow");return;}
  if(!exact(pointer_label,pool+off,8,bytes)){blocked(label,"blocked: indexed block pointer unreadable");return;}
  const uint64_t block=decode(bytes,8);
  if(!block||(block&7u)||location.byte_offset>UINT64_MAX-block){record(label,block,expected.size()+2,{Verdict::Unavailable,Source::None,"entry block pointer not ready"});return;}
  if(location.block_index==0&&ok[2]&&block!=e.words.blocks0){record(label,block,expected.size()+2,{Verdict::Unavailable,Source::None,"Blocks[0] changed during admission; retry fresh"});return;}
  if(exact(label,block+location.byte_offset,2+expected.size(),bytes)){
   const auto check=rc_seed::entry_matches_ascii(bytes,2+expected.size(),expected);
   record(label,block+location.byte_offset,2+expected.size(),{check.ok?Verdict::Readable:Verdict::Invalid,Source::None,check.ok?"entry string matches":check.failure});
  }
 };
 entry("None-entry","None-block-pointer",e.words.none_id,ok[3],rc_seed::kNoneString);
 entry("ByteProperty-entry","ByteProperty-block-pointer",e.words.byte_property_id,ok[4]&&e.words.byte_property_id!=0,rc_seed::kBytePropertyString);
 if(ok[0]&&e.words.current_block>=rc_seed::kPoolLayout.max_blocks)
  record("pool-shape",pool,0,{Verdict::Unavailable,Source::None,"CurrentBlock not initialized"});
 return e;
}
}
