#pragma once
#include "MappedFixture.hpp"
#include <map>
static auto fixture_symbols(const char* path)->std::map<std::string,size_t> {
 std::ifstream in(path,std::ios::binary);std::vector<uint8_t> file{std::istreambuf_iterator<char>(in),{}};
 auto value=[&]<class T>(size_t at){if(at>file.size()||sizeof(T)>file.size()-at)throw std::runtime_error("owned fixture ELF bound");T v;std::memcpy(&v,file.data()+at,sizeof(T));return v;};
 auto u32=[&](size_t a){return value.operator()<uint32_t>(a);};auto u64=[&](size_t a){return value.operator()<uint64_t>(a);};
 const auto sh=u64(40);const auto count=value.operator()<uint16_t>(60);std::map<std::string,size_t> out;
 for(size_t n=0;n<count;++n){const auto s=sh+n*64;if(u32(s+4)!=2)continue;const auto off=u64(s+24),size=u64(s+32),ent=u64(s+56);if(ent!=24||size%ent)throw std::runtime_error("owned symbol entries");const auto str=sh+u32(s+40)*64;const auto strings=u64(str+24),string_size=u64(str+32);if(strings>file.size()||string_size>file.size()-strings)throw std::runtime_error("owned string extent");for(size_t i=0;i<size;i+=ent){const auto index=u32(off+i);if(index>=string_size)throw std::runtime_error("owned symbol name");const auto* p=reinterpret_cast<const char*>(file.data()+strings+index);const auto* end=static_cast<const char*>(std::memchr(p,0,string_size-index));if(!end)throw std::runtime_error("owned symbol NUL");out.emplace(std::string(p,end),u64(off+i+8));}}
 return out;
}
