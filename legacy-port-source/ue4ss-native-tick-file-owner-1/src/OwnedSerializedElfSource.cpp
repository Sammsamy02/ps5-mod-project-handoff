#include <OwnedSerializedElfSource.hpp>
#include <patternsleuth_tick_file_witness.h>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace RC::Unreal::Platform {
namespace {
[[noreturn]] void refuse(){throw std::invalid_argument("Serialized ELF does not match the retained mapped owner");}
uint64_t end(uint64_t p,uint64_t n){if(n>std::numeric_limits<uint64_t>::max()-p)refuse();return p+n;}
bool overlaps(uint64_t a,uint64_t n,uint64_t b,uint64_t m){return n&&m&&a<end(b,m)&&b<end(a,n);}
uint64_t read(const std::vector<uint8_t>& b,uint64_t at,size_t width){if(at>b.size()||width>b.size()-at)refuse();uint64_t v=0;for(size_t i=0;i<width;++i)v|=uint64_t(b[size_t(at)+i])<<(8*i);return v;}
}
auto OwnedSerializedElfSource::Create(const OwnedMappedImageCaller& image,const std::vector<uint8_t>& file) -> std::shared_ptr<const OwnedSerializedElfSource> {
 return std::shared_ptr<const OwnedSerializedElfSource>(new OwnedSerializedElfSource(image,file));
}
OwnedSerializedElfSource::OwnedSerializedElfSource(const OwnedMappedImageCaller& image,const std::vector<uint8_t>& file)
 :owner_(image.source_owner()),bytes_(file) {
 // SCE executable constants are retained in SDK samples/install_app/make_fself.py.
 const auto type=read(bytes_,16,2);const bool executable_type=type==2||type==3||type==0xfe00||type==0xfe10||type==0xfe18;
 if(image.view().format!=PS_MAPPED_IMAGE_ELF64 || bytes_.size()<64 || read(bytes_,0,4)!=0x464c457f || read(bytes_,4,3)!=0x010102 || !executable_type || read(bytes_,18,2)!=62 || read(bytes_,20,4)!=1 || read(bytes_,52,2)!=64 || read(bytes_,54,2)!=56)refuse();
 const auto ph=read(bytes_,32,8),count=read(bytes_,56,2);if(!count || count>4096 || ph>bytes_.size() || count>(bytes_.size()-ph)/56)refuse();
 uint64_t dyn_offset=0,dyn_size=0;bool has_dynamic=false;
 for(uint64_t i=0;i<count;++i){const auto p=ph+i*56,type=read(bytes_,p,4),flags=read(bytes_,p+4,4),offset=read(bytes_,p+8,8),address=read(bytes_,p+16,8),files=read(bytes_,p+32,8),memory=read(bytes_,p+40,8),align=read(bytes_,p+48,8);
  // PT_TLS/DT_NEEDED describe the existing loader, not work for this reader.
  if(type==2){if(has_dynamic||!files||files%16||files>65536||offset>bytes_.size()||files>bytes_.size()-offset)refuse();has_dynamic=true;dyn_offset=offset;dyn_size=files;}
  if(type!=1)continue;
  if(!memory || files>memory || offset>bytes_.size() || files>bytes_.size()-offset || (flags&~7u) || (flags&3)==3 || (align>1 && ((align&(align-1)) || address%align!=offset%align)))refuse();
  end(address,memory);for(const auto& l:loads_)if(overlaps(address,memory,l.address,l.memory_size)||overlaps(offset,files,l.offset,l.file_size))refuse();
  loads_.push_back({offset,address,files,memory,uint32_t(flags)});
 }
 if(loads_.empty())refuse();std::sort(loads_.begin(),loads_.end(),[](const Load&a,const Load&b){return a.address<b.address;});
 bool mapped=false;
 for(const auto& l:loads_)if(l.flags){if(!mapped){low_=l.address;mapped=true;}high_=end(l.address,l.memory_size);}
 if(!mapped)refuse();
 const auto& main=owner_->Views().array[static_cast<size_t>(ScanTarget::MainExe)];runtime_base_=reinterpret_cast<uintptr_t>(main.image_base);
 if(main.image_size<high_-low_)refuse();end(runtime_base_,high_-low_);
 uint64_t tags[7]{};bool seen[7]{};const uint64_t names[]{7,8,9,23,2,20,0};
 if(has_dynamic){bool terminated=false;
  for(uint64_t i=0;i<dyn_size;i+=16){const auto tag=read(bytes_,dyn_offset+i,8),value=read(bytes_,dyn_offset+i+8,8);if(!tag){terminated=true;break;}
   if((tag==17||tag==18||tag==19||tag==35||tag==36||tag==37)&&value)supported_relocations_=false;
   for(size_t k=0;k<6;++k)if(tag==names[k]){if(seen[k])refuse();seen[k]=true;tags[k]=value;}
  }if(!terminated)refuse();
 }
 // Same bounded RELA formats consumed by the genuine witness endpoint. Other
 // formats cannot silently authorize a witness whose relocation effects are unknown.
 std::vector<std::pair<uint64_t,uint64_t>> directories;
 if(seen[0]||seen[1]||seen[2]){if(!seen[0]||!seen[1]||!seen[2]||tags[2]!=24||tags[1]%24)refuse();directories.emplace_back(tags[0],tags[1]);}
 if(seen[3]||seen[4]||seen[5]){if(!seen[3]||!seen[4]||!seen[5]||tags[5]!=7||tags[4]%24)refuse();directories.emplace_back(tags[3],tags[4]);}
 uint64_t row_count=0;
 for(size_t d=0;d<directories.size();++d){const auto [address,size]=directories[d];row_count=end(row_count,size/24);if(row_count>2000000)refuse();for(size_t j=0;j<d;++j)if(overlaps(address,size,directories[j].first,directories[j].second))refuse();
  const auto offset=file_offset(address,size);
  for(uint64_t i=0;i<size;i+=24){const auto dst=read(bytes_,offset+i,8),info=read(bytes_,offset+i+8,8),addend=read(bytes_,offset+i+16,8);const auto type=uint32_t(info);if(!type)continue;
   if(type!=1&&type!=6&&type!=7&&type!=8){supported_relocations_=false;continue;}
   (void)file_offset(dst,8);relocations_.push_back({dst,info,addend});
  }
 }
 std::sort(relocations_.begin(),relocations_.end(),[](const Relocation&a,const Relocation&b){return a.address<b.address;});
 for(size_t i=1;i<relocations_.size();++i)if(relocations_[i].address<end(relocations_[i-1].address,8))refuse();
 if(!matches(image))refuse();
}
auto OwnedSerializedElfSource::translate(uint64_t address,size_t size,bool code) const -> uint64_t {
 for(const auto& l:loads_){const auto extent=code?l.file_size:l.memory_size;if(l.flags && address>=l.address && address-l.address<extent && size<=extent-(address-l.address) && (!code || ((l.flags&1) && !(l.flags&2))))return end(runtime_base_,address-low_);}refuse();
}
auto OwnedSerializedElfSource::file_offset(uint64_t address,size_t size) const -> uint64_t {
 for(const auto& l:loads_)if(address>=l.address && address-l.address<=l.file_size && size<=l.file_size-(address-l.address))return l.offset+address-l.address;refuse();
}
auto OwnedSerializedElfSource::source_matches(const OwnedMappedImageCaller& image,uint64_t linked,size_t size) const -> bool {
 if(!size)return false;const auto offset=file_offset(linked,size),runtime=translate(linked,size,false);if(!image.permits_data(runtime,size))return false;
 const auto* actual=reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(runtime));uint64_t pos=0;
 for(const auto& r:relocations_)if(overlaps(linked,size,r.address,8)){
  if(r.address<linked || end(r.address,8)>end(linked,size) || r.info!=8 || r.addend>uint64_t(INT64_MAX))return false;
  const auto at=r.address-linked;if(at>pos && std::memcmp(actual+pos,bytes_.data()+offset+pos,size_t(at-pos)))return false;
  uint64_t value=0;std::memcpy(&value,actual+at,8);if(value!=translate(r.addend,1,false))return false;pos=at+8;
 }
 return pos==size || !std::memcmp(actual+pos,bytes_.data()+offset+pos,size_t(size-pos));
}
auto OwnedSerializedElfSource::matches(const OwnedMappedImageCaller& image) const noexcept -> bool {
 try {
  if(image.source_owner()!=owner_ || image.view().format!=PS_MAPPED_IMAGE_ELF64 || !supported_relocations_)return false;
  bool executable=false;
  for(const auto& l:loads_)if((l.flags&5) && !(l.flags&2) && l.file_size){
   const auto runtime=translate(l.address,1,false),stop=end(runtime,l.file_size);auto cursor=runtime;
   for(size_t i=0;i<image.view().segment_count && cursor<stop;++i){const auto& s=image.view().segments[i];const auto finish=end(s.runtime_address,s.size);if(finish<=cursor)continue;
    const bool permissions=(s.permissions&PS_MAPPED_READ) && !(s.permissions&PS_MAPPED_WRITE) && (!(l.flags&1) || (s.permissions&PS_MAPPED_EXECUTE));
    if(s.runtime_address>cursor || !permissions)return false;cursor=std::min(stop,finish);
   }
   if(cursor!=stop || !source_matches(image,l.address,l.file_size))return false;executable|=(l.flags&1)!=0;
  }return executable;
 }catch(...){return false;}
}
auto OwnedSerializedElfSource::tick(const OwnedMappedImageCaller& image,uint64_t& address) const noexcept -> int32_t {
 address=0;if(!matches(image))return 2;
 try {
  PsTickFileWitnessHeader header{};const auto first=ps_ugameengine_tick_file_witness_v1(bytes_.data(),bytes_.size(),&header,nullptr,0);
  if(first!=6)return first?first:2;
  if(header.version!=1 || header.flags!=1 || !header.linked_tick || !header.witness_count || header.witness_count>8192 || header.reserved)return 2;
  std::vector<PsTickFileWitness> witnesses(header.witness_count);const auto expected=header;
  const auto status=ps_ugameengine_tick_file_witness_v1(bytes_.data(),bytes_.size(),&header,witnesses.data(),witnesses.size());if(status)return status;
  if(std::memcmp(&header,&expected,sizeof(header)))return 2;
  for(const auto& w:witnesses){if(w.reserved || !w.byte_size)return 2;
   if(w.kind==1){if(w.expected_linked || !source_matches(image,w.linked_address,w.byte_size))return 2;}
   else if(w.kind==2){if(w.byte_size!=8)return 2;(void)file_offset(w.linked_address,8);
    const auto row=std::lower_bound(relocations_.begin(),relocations_.end(),w.linked_address,[](const Relocation&r,uint64_t a){return r.address<a;});
    if(row==relocations_.end()||row->address!=w.linked_address||row->info!=8||row->addend!=w.expected_linked||row->addend>uint64_t(INT64_MAX))return 2;
    const auto runtime=translate(w.linked_address,8,false);if(!image.permits_data(runtime,8))return 2;uint64_t actual=0;std::memcpy(&actual,reinterpret_cast<const void*>(static_cast<uintptr_t>(runtime)),8);if(actual!=translate(w.expected_linked,1,false))return 2;
   }else return 2;
  }
  const auto runtime=translate(header.linked_tick,1,true);if(!image.permits_code(runtime))return 2;address=runtime;return 0;
 }catch(...){return 2;}
}
}
