#pragma once
#include <OwnedMappedImageCaller.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

// Test-only checked ELF materialization of compiler-owned PIC fixtures. Never
// executes code. Registers only storage created by the real OwnedMapping.
struct MappedFixture {
    struct Load { size_t offset,address,file_size,memory_size;uint32_t flags; };
    std::shared_ptr<PLH::OwnedMemory::OwnedMapping> owner;
    std::vector<Load> loads;
    size_t text{},frame{},frame_size{},header{},header_size{};
    size_t image_size{};
    explicit MappedFixture(const char* path) {
        std::ifstream in(path,std::ios::binary);
        std::vector<uint8_t> file{std::istreambuf_iterator<char>(in),{}};
        assert(file.size()>=64 && std::memcmp(file.data(),"\x7f" "ELF",4)==0);
        assert(file[4]==2 && file[5]==1);
        auto value=[&]<class T>(size_t offset) {
            assert(offset<=file.size() && sizeof(T)<=file.size()-offset);
            T v;std::memcpy(&v,file.data()+offset,sizeof(T));return v;
        };
        assert(value.operator()<uint16_t>(18)==62);
        auto u16=[&](size_t n){return value.operator()<uint16_t>(n);};
        auto u32=[&](size_t n){return value.operator()<uint32_t>(n);};
        auto u64=[&](size_t n){return value.operator()<uint64_t>(n);};
        const size_t ph=u64(32);assert(u16(54)==56 && u16(56)<32);
        for(size_t i=0;i<u16(56);++i) {
            const auto p=ph+i*56;if(u32(p)!=1)continue;
            Load l{u64(p+8),u64(p+16),u64(p+32),u64(p+40),u32(p+4)};
            assert(l.file_size<=l.memory_size && l.offset<=file.size() && l.file_size<=file.size()-l.offset);
            assert(l.memory_size && l.address<1048576 && l.memory_size<1048576-l.address);
            assert(l.flags&4);loads.push_back(l);image_size=std::max(image_size,l.address+l.memory_size);
        }
        assert(!loads.empty() && loads.front().address==0);
        const auto sh=u64(40);const size_t count=u16(60);assert(u16(58)==64 && count<128);
        assert(u16(62)<count);const auto names=sh+u16(62)*64;
        const size_t no=u64(names+24),ns=u64(names+32);
        assert(no<=file.size() && ns<=file.size()-no);
        for(size_t i=0;i<count;++i) {
            const auto s=sh+i*64;const size_t n=u32(s);assert(n<ns);
            const auto* start=reinterpret_cast<const char*>(file.data()+no+n);
            const auto* end=static_cast<const char*>(std::memchr(start,0,ns-n));assert(end);
            const std::string name(start,end);
            if(name==".text")text=u64(s+16);
            else if(name==".eh_frame"){frame=u64(s+16);frame_size=u64(s+32);}
            else if(name==".eh_frame_hdr"){header=u64(s+16);header_size=u64(s+32);}
        }
        assert(text);
        owner=std::make_shared<PLH::OwnedMemory::OwnedMapping>(image_size);
        std::memset(data(),0,owner->size());
        for(const auto& l:loads)std::memcpy(data()+l.address,file.data()+l.offset,l.file_size);
    }
    auto data()const->uint8_t* {return static_cast<uint8_t*>(owner->data());}
    auto address(size_t rva)const->uint64_t {assert(rva<owner->size());return reinterpret_cast<uintptr_t>(data()+rva);}
    auto context()const->RC::Platform::NativeProgramContext {
        using namespace RC::Unreal::Platform;
        std::vector<RC::ReadableRange> ranges;
        for(const auto& l:loads)ranges.push_back({data()+l.address,l.memory_size});
        ModuleInput input{};input.lifetime=owner;input.monolithic=true;
        input.images.emplace(RC::ScanTarget::MainExe,ModuleImage{data(),image_size,std::move(ranges)});
        RC::Platform::NativeProgramContext result{};result.module_source=ModuleSnapshot::Create(std::move(input));return result;
    }
    auto frame_extent()const->PsMappedExtent{return frame_size?PsMappedExtent{address(frame),frame_size}:PsMappedExtent{};}
    auto header_extent()const->PsMappedExtent{return header_size?PsMappedExtent{address(header),header_size}:PsMappedExtent{};}
    void protect() {
        PLH::MemAccessor accessor;
        for(const auto& l:loads) {
            const auto flags=(l.flags&4?PLH::R:PLH::UNSET)|(l.flags&2?PLH::W:PLH::UNSET)|(l.flags&1?PLH::X:PLH::UNSET);
            bool ok=false;accessor.mem_protect(address(l.address),l.memory_size,flags,ok);assert(ok);
        }
    }
};
