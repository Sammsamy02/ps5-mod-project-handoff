#include <polyhook2/OwnedMemory.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <cstdint>
int main() {
    using namespace PLH;using namespace PLH::OwnedMemory;
    int32_t source=-17,destination=0;MemAccessor access;size_t count=0;
    const auto src=reinterpret_cast<uintptr_t>(&source),dst=reinterpret_cast<uintptr_t>(&destination);
    if(access.safe_mem_read(src,dst,4,count) || count)return 10;
    {ScopedRegion s(&source,4,R),d(&destination,4,W);
     if(!access.safe_mem_read(src,dst,4,count) || count!=4 || destination!=-17)return 11;
     if(!access.safe_mem_write(dst,src,4,count) || count!=4 || !access.mem_copy(dst,src,4))return 12;}
    if(query(src) || access.mem_copy(dst,src,4))return 13;
    OwnedMapping mapping(1);const auto base=reinterpret_cast<uintptr_t>(mapping.data());
    auto info=query(base);if(!info || !info->protectable || info->protection!=(R|W))return 14;
    bool status=false;if(access.mem_protect(base,1,R,status)!=(R|W) || !status)return 15;
    if(access.mem_protect(base,1,NONE,status)!=R || !status)return 16;
    {ScopedRegion d(&destination,4,W);if(access.safe_mem_read(base,dst,4,count) || count)return 17;}
    if(access.mem_protect(base,1,R|W,status)!=NONE || !status)return 18;
    return 0;
}
