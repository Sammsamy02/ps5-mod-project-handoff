// Separate provider: original Windows/Linux/Apple MemAccessor bodies untouched.
#if defined(__PROSPERO__) || defined(PLH_OWNED_MEMORY_HOST_TEST)
#include <polyhook2/OwnedMemory.hpp>
#include <polyhook2/MemAccessor.hpp>
#include <polyhook2/OwnedMemoryAllocatorBridge.hpp>
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>
#include <mutex>
#include <vector>
#include <limits>
#include <stdexcept>
#include <cstring>
#include <exception>
#include <algorithm>
#include <thread>
#if defined(PLH_OWNED_MEMORY_OS_FAILURE_TEST)
// Test fixture substitutes only the allocator OS-call boundary to report a
// refused call once; ordinary calls still use actual host mmap/mprotect/munmap.
extern "C" int owned_allocator_test_mprotect(void*,size_t,int);
extern "C" int owned_allocator_test_munmap(void*,size_t);
#endif

namespace PLH::OwnedMemory {
namespace {
struct Entry {
    uint64_t token,base;
    size_t bytes,page;
    std::vector<ProtFlag> protection;
    std::shared_ptr<const void> owner;
    unsigned allocator=0;
    size_t requested=0;
};
struct State { std::mutex mutex; std::vector<Entry> entries; uint64_t next=1; };
auto state() -> State& { static State value; return value; }
auto extent(uint64_t base,uint64_t bytes,uint64_t& end) noexcept -> bool {
    if (!base || !bytes || base>std::numeric_limits<uintptr_t>::max() ||
        bytes>std::numeric_limits<size_t>::max() || bytes>std::numeric_limits<uintptr_t>::max()-base) return false;
    end=base+bytes;return true;
}
auto valid_protection(ProtFlag p) noexcept -> bool {
    const auto bits=static_cast<unsigned>(p);
    return p==NONE || (bits!=0 && (bits & ~static_cast<unsigned>(RWX))==0);
}
auto covering(State& s,uint64_t address,uint64_t bytes) noexcept -> Entry* {
    uint64_t end;if(!extent(address,bytes,end))return nullptr;
    for(auto& e:s.entries)if(address>=e.base && end<=e.base+e.bytes)return &e;
    return nullptr;
}
auto add(Entry entry) -> uint64_t {
    uint64_t end;if(!extent(entry.base,entry.bytes,end))throw std::invalid_argument("Owned region extent invalid");
    auto& s=state();std::lock_guard lock(s.mutex);
    for(const auto& other:s.entries)if(entry.base<other.base+other.bytes && other.base<end)
        throw std::invalid_argument("Owned regions must not overlap");
    if(!s.next)throw std::overflow_error("Owned region token exhausted");
    entry.token=s.next; s.entries.push_back(std::move(entry));return s.next++;
}
auto remove(uint64_t token) noexcept -> void {
    // Release arbitrary retained-owner destructors outside the service mutex.
    std::shared_ptr<const void> owner;
    try {auto& s=state();std::lock_guard lock(s.mutex);
        for(auto it=s.entries.begin();it!=s.entries.end();++it)if(it->token==token){
            owner=std::move(it->owner);s.entries.erase(it);return;
        }
    }catch(...){std::terminate();}
}
auto page_indices(const Entry& e,uint64_t address,uint64_t bytes,size_t& first,size_t& last) noexcept -> void {
    first=e.page ? static_cast<size_t>((address-e.base)/e.page) : 0;
    last=e.page ? static_cast<size_t>((address-e.base+bytes-1)/e.page) : 0;
}
auto permitted(const Entry& e,uint64_t address,uint64_t bytes,ProtFlag permission) noexcept -> bool {
    size_t first,last;page_indices(e,address,bytes,first,last);
    for(size_t i=first;i<=last;++i)if(!(e.protection[i]&permission))return false;
    return true;
}
auto copy(uint64_t dest,uint64_t src,uint64_t bytes) -> bool {
    auto& s=state();std::lock_guard lock(s.mutex);
    auto* source=covering(s,src,bytes);auto* destination=covering(s,dest,bytes);
    if(!source || !destination || !permitted(*source,src,bytes,R) || !permitted(*destination,dest,bytes,W))return false;
    // No pointer is dereferenced until both explicitly owned extents pass.
    std::memmove(reinterpret_cast<void*>(static_cast<uintptr_t>(dest)),
                 reinterpret_cast<const void*>(static_cast<uintptr_t>(src)),static_cast<size_t>(bytes));return true;
}
auto protect(uint64_t address,uint64_t bytes,ProtFlag protection,bool& status) -> ProtFlag {
    status=false;if(!valid_protection(protection))return UNSET;
    auto& s=state();std::lock_guard lock(s.mutex);auto* e=covering(s,address,bytes);
    if(!e || !e->page)return UNSET;
    size_t first,last;page_indices(*e,address,bytes,first,last);const auto old=e->protection[first];
    // A single return flag cannot faithfully represent mixed previous states.
    for(size_t i=first;i<=last;++i)if(e->protection[i]!=old)return UNSET;
    const int flags=(protection&R ? PROT_READ:0)|(protection&W ? PROT_WRITE:0)|(protection&X ? PROT_EXEC:0);
    const auto begin=e->base+first*e->page;const auto length=(last-first+1)*e->page;
    if(::mprotect(reinterpret_cast<void*>(static_cast<uintptr_t>(begin)),length,flags)!=0)return UNSET;
    for(size_t i=first;i<=last;++i)e->protection[i]=protection;
    status=true;return old;
}
}


struct ReadLease::Impl {
    // Destroy the lock before vectors/retained storage to avoid reentrant owner
    // cleanup under the service lock. Registrations themselves retain owners.
    std::vector<ReadSpan> spans;
    std::unique_lock<std::mutex> lock;
    std::thread::id thread;
    explicit Impl(std::span<const ReadRange> ranges)
        : lock(state().mutex), thread(std::this_thread::get_id()) {
        if (ranges.empty()) throw std::invalid_argument("Owned read lease requires ranges");
        std::vector<ReadRange> ordered(ranges.begin(), ranges.end());
        std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
            return reinterpret_cast<uintptr_t>(a.data) < reinterpret_cast<uintptr_t>(b.data);
        });
        uint64_t previous_end = 0;
        for (const auto& range : ordered) {
            auto current = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(range.data));
            uint64_t end;
            if (!extent(current, range.size, end) || current < previous_end)
                throw std::invalid_argument("Owned read ranges overlap or have invalid bounds");
            previous_end = end;
            while (current < end) {
                const auto* e = covering(state(), current, 1);
                if (!e) throw std::invalid_argument("Owned read range includes unregistered bytes");
                size_t first, last;
                page_indices(*e, current, 1, first, last);
                const auto protection = e->protection[first];
                if (!(protection & R)) throw std::invalid_argument("Owned read range is unreadable");
                uint64_t region_end = e->base + e->bytes;
                if (e->page) {
                    while (last + 1 < e->protection.size() && e->protection[last + 1] == protection) ++last;
                    region_end = e->base + (last + 1) * e->page;
                }
                const auto clipped_end = std::min(end, region_end);
                spans.push_back({reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(current)),
                    static_cast<size_t>(clipped_end - current), protection});
                current = clipped_end;
            }
        }
    }
    ~Impl() noexcept {
        // std::mutex ownership cannot move to another thread.
        if (thread != std::this_thread::get_id()) std::terminate();
    }
};
ReadLease::ReadLease(std::span<const ReadRange> ranges) : impl_(std::make_unique<Impl>(ranges)) {}
ReadLease::~ReadLease() noexcept = default;
auto ReadLease::spans() const noexcept -> std::span<const ReadSpan> { return impl_->spans; }

namespace AllocatorBridge {
namespace {
auto native_flags(int protection,ProtFlag& result) noexcept -> bool {
    if(protection & ~(PROT_READ|PROT_WRITE|PROT_EXEC))return false;
    unsigned bits=0;
    if(protection & PROT_READ)bits|=R;
    if(protection & PROT_WRITE)bits|=W;
    if(protection & PROT_EXEC)bits|=X;
    result=bits ? static_cast<ProtFlag>(bits):NONE;return true;
}
}
auto record_mapping(void* address,size_t requested,int protection,unsigned producer) noexcept -> int {
    try {
        ProtFlag flags;if(!native_flags(protection,flags))return EINVAL;
        const int actual=::getpagesize();if(actual<=0 || actual>1048576 || (actual&(actual-1)))return EINVAL;
        const size_t page=static_cast<size_t>(actual);
        const auto base=reinterpret_cast<uintptr_t>(address);
        if(!base || (base&(page-1)) || !requested || requested>std::numeric_limits<size_t>::max()-(page-1))return EINVAL;
        const size_t rounded=(requested+page-1)&~(page-1);
        add({0,base,rounded,page,std::vector<ProtFlag>(rounded/page,flags),{},producer,requested});return 0;
    }catch(const std::bad_alloc&){return ENOMEM;}catch(...){return EINVAL;}
}
auto release_mapping(void* address,size_t requested,unsigned producer) noexcept -> int {
    try {
        auto& s=state();std::lock_guard lock(s.mutex);const auto base=reinterpret_cast<uintptr_t>(address);
        auto* e=covering(s,base,requested);
        if(!e || e->allocator!=producer || base!=e->base || requested!=e->requested)return EINVAL;
#if defined(PLH_OWNED_MEMORY_OS_FAILURE_TEST)
        if(owned_allocator_test_munmap(address,requested)!=0)return errno;
#else
        if(::munmap(address,requested)!=0)return errno;
#endif
        // Scalar allocator metadata has no second mapping owner/deleter.
        for(auto it=s.entries.begin();it!=s.entries.end();++it)if(&*it==e){s.entries.erase(it);break;}
        return 0;
    }catch(...){return EINVAL;}
}
auto record_created(void* address,size_t size,int protection) noexcept -> int {return record_mapping(address,size,protection,1);}
auto release_created(void* address,size_t size) noexcept -> int {return release_mapping(address,size,1);}
auto record_bounded(void* address,size_t size,int protection) noexcept -> int {return record_mapping(address,size,protection,2);}
auto release_bounded(void* address,size_t size) noexcept -> int {return release_mapping(address,size,2);}
auto protect_created(void* address,size_t bytes,int protection) noexcept -> int {
    try {
        ProtFlag flags;if(!native_flags(protection,flags))return EINVAL;
        auto& s=state();std::lock_guard lock(s.mutex);const auto base=reinterpret_cast<uintptr_t>(address);
        auto* e=covering(s,base,bytes);
        if(!e || e->allocator!=1 || !e->page || (base&(e->page-1)))return EINVAL;
#if defined(PLH_OWNED_MEMORY_OS_FAILURE_TEST)
        if(owned_allocator_test_mprotect(address,bytes,protection)!=0)return errno;
#else
        if(::mprotect(address,bytes,protection)!=0)return errno;
#endif
        size_t first,last;page_indices(*e,base,bytes,first,last);
        for(size_t i=first;i<=last;++i)e->protection[i]=flags;
        return 0;
    }catch(...){return EINVAL;}
}
}

ScopedRegion::ScopedRegion(void* data,size_t size,ProtFlag protection,std::shared_ptr<const void> owner) {
    if(!valid_protection(protection))throw std::invalid_argument("Unsupported owned region protection");
    token_=add({0,reinterpret_cast<uintptr_t>(data),size,0,{protection},std::move(owner)});
}
ScopedRegion::~ScopedRegion() noexcept {remove(token_);}
OwnedMapping::OwnedMapping(size_t bytes) {
    const int actual=::getpagesize();
    if(actual<=0 || actual>1048576 || (actual&(actual-1)))throw std::runtime_error("Invalid native page size");
    const size_t page=static_cast<size_t>(actual);
    if(!bytes || bytes>std::numeric_limits<size_t>::max()-(page-1))throw std::invalid_argument("Invalid mapping size");
    const size_t rounded=(bytes+page-1)&~(page-1);
    void* memory=::mmap(nullptr,rounded,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(memory==MAP_FAILED)throw std::runtime_error("Owned mmap refused");
    auto owner=std::shared_ptr<const void>(memory,[rounded](const void* p) noexcept {
        if(::munmap(const_cast<void*>(p),rounded)!=0)std::terminate();
    });
    token_=add({0,reinterpret_cast<uintptr_t>(memory),rounded,page,
                std::vector<ProtFlag>(rounded/page,R|W),std::move(owner)});
    data_=memory;size_=rounded;
}
OwnedMapping::~OwnedMapping() noexcept {remove(token_);}
auto query(uint64_t address,uint64_t bytes) noexcept -> std::optional<RegionInfo> {
    try {auto& s=state();std::lock_guard lock(s.mutex);auto* e=covering(s,address,bytes);if(!e)return {};
        size_t first,last;page_indices(*e,address,bytes,first,last);const auto protection=e->protection[first];
        for(size_t i=first;i<=last;++i)if(e->protection[i]!=protection)return {};
        if(!e->page)return RegionInfo{e->base,e->bytes,protection,false,e->owner};
        while(first && e->protection[first-1]==protection)--first;
        while(last+1<e->protection.size() && e->protection[last+1]==protection)++last;
        return RegionInfo{e->base+first*e->page,(last-first+1)*e->page,protection,true,e->owner};
    }catch(...){return {};}
}
}

bool PLH::MemAccessor::mem_copy(uint64_t dest,uint64_t src,uint64_t size) const {
    return OwnedMemory::copy(dest,src,size);
}
bool PLH::MemAccessor::safe_mem_write(uint64_t dest,uint64_t src,uint64_t size,size_t& written) const noexcept {
    written=0;try{if(!OwnedMemory::copy(dest,src,size))return false;written=static_cast<size_t>(size);return true;}catch(...){return false;}
}
bool PLH::MemAccessor::safe_mem_read(uint64_t src,uint64_t dest,uint64_t size,size_t& read) const noexcept {
    read=0;try{if(!OwnedMemory::copy(dest,src,size))return false;read=static_cast<size_t>(size);return true;}catch(...){return false;}
}
PLH::ProtFlag PLH::MemAccessor::mem_protect(uint64_t dest,uint64_t size,PLH::ProtFlag newProtection,bool& status) const {
    return OwnedMemory::protect(dest,size,newProtection,status);
}
#endif
