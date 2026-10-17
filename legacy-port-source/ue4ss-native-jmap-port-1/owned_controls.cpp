#include <JMapGenerator/NativeSupport.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <array>
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    using RC::JMapGenerator::Native::MemoryChecker;
    using namespace PLH;
    MemoryChecker checker;
    std::array<unsigned char,64> bytes{};
    assert(!checker.is_readable(bytes.data()));
    {
        OwnedMemory::ScopedRegion left(bytes.data(),32,R);
        OwnedMemory::ScopedRegion right(bytes.data()+32,32,R);
        assert(checker.is_readable(bytes.data(),64));
        assert(checker.is_readable(bytes.data()+63,0));
        assert(!checker.is_readable(bytes.data(),65));
        assert(!checker.is_readable(bytes.data()+64));
        assert(!checker.is_executable(bytes.data()));
    }
    assert(!checker.is_readable(bytes.data()));
    {
        OwnedMemory::ScopedRegion left(bytes.data(),16,R);
        OwnedMemory::ScopedRegion right(bytes.data()+32,32,R);
        assert(!checker.is_readable(bytes.data(),64));
    }
    {
        OwnedMemory::ScopedRegion left(bytes.data(),32,R);
        OwnedMemory::ScopedRegion right(bytes.data()+32,32,NONE);
        assert(!checker.is_readable(bytes.data(),64));
    }
    assert(!checker.is_readable(nullptr));
    assert(!checker.is_readable(reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max()-1),4));
    OwnedMemory::OwnedMapping pages(8192);
    MemAccessor memory;
    bool changed=false;
    assert(checker.is_readable(pages.data(),pages.size()));
    memory.mem_protect(reinterpret_cast<uintptr_t>(pages.data()),pages.size(),NONE,changed);
    assert(changed && !checker.is_readable(pages.data()));
    memory.mem_protect(reinterpret_cast<uintptr_t>(pages.data()),pages.size(),R|X,changed);
    assert(changed && checker.is_readable(pages.data()) && checker.is_executable(pages.data()));
    memory.mem_protect(reinterpret_cast<uintptr_t>(pages.data()),pages.size(),R|W,changed);
    assert(changed && checker.is_readable(pages.data()) && !checker.is_executable(pages.data()));
    assert(RC::JMapGenerator::Native::utc_timestamp(0)=="1970-01-01T00:00:00Z");
    bool rejected=false;
    try{RC::JMapGenerator::Native::utc_timestamp(-1);}catch(const std::runtime_error&){rejected=true;}
    assert(rejected);
    assert(RC::JMapGenerator::Native::source_filename("/data/日本語/Game.elf")=="Game.elf");
    assert(RC::JMapGenerator::Native::source_filename("/data/モッド.elf")=="モッド.elf");
    rejected=false;
    try{RC::JMapGenerator::Native::source_filename("/");}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
    std::cout << "JMap native helpers: owned-region, fresh-permission, timestamp and filename controls passed\n";
}
