#include <polyhook2/OwnedMemory.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <cassert>
#include <cstring>
#include <limits>
#include <thread>
#include <atomic>
#include <iostream>
#include <unistd.h>
using namespace PLH;
using namespace PLH::OwnedMemory;
static uint64_t addr(const void* p) { return reinterpret_cast<uintptr_t>(p); }
int main() {
    MemAccessor access;
    unsigned char src[8]={1,2,3,4,5,6,7,8}, dst[8]={}; size_t count=77;
    // A missing registration must refuse even a genuinely accessible pointer.
    assert(!access.safe_mem_read(addr(src),addr(dst),8,count) && count==0);
    assert(!query(addr(src)));
    {
        ScopedRegion source(src,8,R), destination(dst,8,W);
        assert(access.safe_mem_read(addr(src),addr(dst),8,count) && count==8);
        assert(std::memcmp(src,dst,8)==0);
        assert(access.safe_mem_write(addr(dst),addr(src),8,count) && count==8);
        assert(access.mem_copy(addr(dst),addr(src),8));
        assert(!access.safe_mem_read(addr(dst),addr(src),1,count) && count==0);
        assert(!access.safe_mem_read(addr(src)+7,addr(dst),2,count) && count==0);
        assert(!access.safe_mem_read(UINT64_MAX-1,addr(dst),4,count) && count==0);
        assert(!access.safe_mem_read(addr(src),addr(dst),0,count) && count==0);
        bool status=true;
        assert(access.mem_protect(addr(src),8,RWX,status)==UNSET && !status);
        bool overlap=false;try { ScopedRegion bad(src+1,2,R); } catch(const std::invalid_argument&) { overlap=true; }
        assert(overlap);
        assert(access.safe_mem_read(addr(src),addr(dst),8,count) && count==8);
    }
    assert(!query(addr(src)) && !query(addr(dst)));
    assert(!access.mem_copy(addr(dst),addr(src),8));
    bool invalid=false;try { ScopedRegion bad(nullptr,8,R); }catch(const std::invalid_argument&){invalid=true;} assert(invalid);
    invalid=false;try { ScopedRegion bad(src,8,UNSET); }catch(const std::invalid_argument&){invalid=true;} assert(invalid);
    auto bytes=std::make_shared<unsigned char[]>(8); auto weak=std::weak_ptr<unsigned char[]>(bytes);
    { ScopedRegion retained(bytes.get(),8,R,bytes);bytes.reset();assert(!weak.expired()); }
    assert(weak.expired());
    std::optional<RegionInfo> lease;
    uint64_t old=0;
    {
        OwnedMapping mapping(1); old=addr(mapping.data()); const size_t page=mapping.size();
        assert(page>=1 && (page&(page-1))==0);
        assert(query(old)->protection==(R|W) && query(old)->protectable);
        assert(!query(old+page));
        assert(!access.safe_mem_read(old,addr(dst),1,count) && count==0);
        { ScopedRegion destination(dst,8,W);static_cast<unsigned char*>(mapping.data())[0]=42;
          assert(access.safe_mem_read(old,addr(dst),1,count) && count==1 && dst[0]==42); }
        bool status=false;
        assert(access.mem_protect(old+1,1,R,status)==(R|W) && status);
        assert(query(old)->protection==R);
        { ScopedRegion source(src,8,R);assert(!access.safe_mem_write(old,addr(src),1,count) && count==0); }
        assert(access.mem_protect(old,1,NONE,status)==R && status);
        { ScopedRegion destination(dst,8,W);assert(!access.safe_mem_read(old,addr(dst),1,count) && count==0); }
        assert(access.mem_protect(old,1,R|W,status)==NONE && status);
        assert(access.mem_protect(old,page+1,R,status)==UNSET && !status);
        assert(access.mem_protect(old,1,UNSET,status)==UNSET && !status);
        lease=query(old);assert(lease && lease->lifetime);
    }
    assert(!query(old)); lease.reset();
    {
        OwnedMapping two(32768);const auto base=addr(two.data());
        const size_t page=static_cast<size_t>(getpagesize());assert(two.size()>=page*2);
        bool status=false;assert(access.mem_protect(base+page,1,R,status)==(R|W) && status);
        assert(query(base)->size==page);
        assert(query(base+page)->protection==R);
        assert(!query(base,page*2));
        assert(access.mem_protect(base,page*2,R,status)==UNSET && !status);
        assert(access.mem_protect(base+page,1,R|W,status)==R && status);
        assert(query(base)->size==two.size());
        // memmove semantics avoid undefined behavior for overlapping owned bytes.
        std::memcpy(two.data(),src,8);assert(access.mem_copy(base+1,base,7));
        assert(static_cast<unsigned char*>(two.data())[7]==7);
    }
    // Scope revocation and reads serialize; no stale destination write succeeds.
    { ScopedRegion source(src,8,R); std::atomic<bool> ready=false, stop=false;std::atomic<size_t> reads=0;
      auto destination=std::make_unique<ScopedRegion>(dst,8,W);
      std::thread worker([&]{ready=true;while(!stop){size_t n;if(access.safe_mem_read(addr(src),addr(dst),8,n)){assert(n==8);++reads;}}});
      while(!ready || reads==0)std::this_thread::yield();destination.reset();
      for(int i=0;i<1000;++i)assert(!access.safe_mem_read(addr(src),addr(dst),8,count) && count==0);
      stop=true;worker.join();assert(reads>0); }
    std::cout << "owned memory: copy/read/write, bounds/permissions/lifetime, real page protection, mixed-page refusal, concurrency passed\n";
}
