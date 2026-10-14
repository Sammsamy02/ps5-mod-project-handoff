#ifndef CAP13_GUEST_VIRTUAL_MEMORY_H
#define CAP13_GUEST_VIRTUAL_MEMORY_H

#include "guest_read_map.h"
#include "guest_write_map.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <new>
#include <unordered_map>
#include <sys/mman.h>
#include <unistd.h>

namespace cap13 {

// Owns anonymous page regions. Only whole-region reserve/commit/release is
// admitted. The caller must quiesce direct guest accesses before destruction
// or release; map removal only serializes the guarded copy helpers.
class GuestVirtualMemory
{
  public:
    ~GuestVirtualMemory();
    bool bind(GuestReadMap* reads, GuestWriteMap* writes);
    bool ready() const;
    void* allocate(void* base, size_t bytes, uint32_t type,
                   uint32_t protect, uint32_t* error);
    bool release(void* base, size_t bytes, uint32_t type, uint32_t* error);

  private:
    struct Region
    {
        size_t mapped;
        bool committed;
        uint32_t protect;
    };
    mutable std::mutex mu_;
    GuestReadMap* reads_ = nullptr;
    GuestWriteMap* writes_ = nullptr;
    std::unordered_map<void*, Region> regions_;

    bool register_region(void* base, size_t length, uint32_t protect);
    void unregister_region(void* base, uint32_t protect);
};

namespace virtual_memory_detail {
constexpr uint32_t kCommit = 0x1000;
constexpr uint32_t kReserve = 0x2000;
constexpr uint32_t kRelease = 0x8000;
constexpr uint32_t kReadOnly = 0x02;
constexpr uint32_t kReadWrite = 0x04;
constexpr uint32_t kInvalidParameter = 87;
constexpr uint32_t kNotEnoughMemory = 8;
constexpr uint32_t kInvalidAddress = 487;
#if defined(MAP_ANONYMOUS)
constexpr int kAnonymousMap = MAP_ANONYMOUS;
#elif defined(MAP_ANON)
constexpr int kAnonymousMap = MAP_ANON;
#else
constexpr int kAnonymousMap = 0;
#endif
}

inline GuestVirtualMemory::~GuestVirtualMemory()
{
    std::lock_guard guard(mu_);
    for (const auto& row : regions_)
    {
        if (row.second.committed)
            unregister_region(row.first, row.second.protect);
        munmap(row.first, row.second.mapped);
    }
}

inline bool GuestVirtualMemory::bind(GuestReadMap* reads, GuestWriteMap* writes)
{
    if (!reads || !writes) return false;
    std::lock_guard guard(mu_);
    if (reads_ || writes_) return reads_ == reads && writes_ == writes;
    reads_ = reads;
    writes_ = writes;
    return true;
}

inline bool GuestVirtualMemory::ready() const
{
    std::lock_guard guard(mu_);
    return reads_ && writes_;
}

inline bool GuestVirtualMemory::register_region(void* base, size_t length,
                                                 uint32_t protect)
{
    if (!reads_->add(base, base, length)) return false;
    if (protect == virtual_memory_detail::kReadWrite &&
        !writes_->add(base, base, length))
    {
        reads_->remove(base);
        return false;
    }
    return true;
}

inline void GuestVirtualMemory::unregister_region(void* base, uint32_t protect)
{
    if (protect == virtual_memory_detail::kReadWrite) writes_->remove(base);
    reads_->remove(base);
}

inline void* GuestVirtualMemory::allocate(void* base, size_t bytes,
                                           uint32_t type, uint32_t protect,
                                           uint32_t* error)
{
    using namespace virtual_memory_detail;
    if (error) *error = kInvalidParameter;
    if (!bytes || (protect != kReadOnly && protect != kReadWrite) ||
        (type != kReserve && type != kCommit &&
         type != (kReserve | kCommit)))
        return nullptr;
    const long page_value = sysconf(_SC_PAGESIZE);
    if (page_value <= 0) return nullptr;
    const size_t page = static_cast<size_t>(page_value);
    if (bytes > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()) ||
        bytes > SIZE_MAX - (page - 1))
        return nullptr;
    const size_t length = ((bytes + page - 1) / page) * page;
    std::lock_guard guard(mu_);
    if (!reads_ || !writes_) return nullptr;

    if (base)
    {
        if (type != kCommit)
        {
            if (error) *error = kInvalidAddress;
            return nullptr; // Fixed-address reservation is not admitted.
        }
        auto found = regions_.find(base);
        if (found == regions_.end() || found->second.committed ||
            found->second.mapped != length)
        {
            if (error) *error = kInvalidAddress;
            return nullptr;
        }
        const int os_protect = protect == kReadWrite ?
            PROT_READ | PROT_WRITE : PROT_READ;
        if (mprotect(base, length, os_protect) != 0)
        {
            if (error) *error = kNotEnoughMemory;
            return nullptr;
        }
        if (!register_region(base, length, protect))
        {
            mprotect(base, length, PROT_NONE);
            if (error) *error = kNotEnoughMemory;
            return nullptr;
        }
        found->second.committed = true;
        found->second.protect = protect;
        return base;
    }

    if (!kAnonymousMap)
    {
        if (error) *error = kNotEnoughMemory;
        return nullptr;
    }
    void* memory = mmap(nullptr, length, PROT_NONE,
                        MAP_PRIVATE | kAnonymousMap, -1, 0);
    if (memory == MAP_FAILED)
    {
        if (error) *error = kNotEnoughMemory;
        return nullptr;
    }
    const bool commit = (type & kCommit) != 0;
    if (commit)
    {
        const int os_protect = protect == kReadWrite ?
            PROT_READ | PROT_WRITE : PROT_READ;
        if (mprotect(memory, length, os_protect) != 0 ||
            !register_region(memory, length, protect))
        {
            munmap(memory, length);
            if (error) *error = kNotEnoughMemory;
            return nullptr;
        }
    }
    try { regions_.emplace(memory, Region{length, commit, protect}); }
    catch (const std::bad_alloc&)
    {
        if (commit) unregister_region(memory, protect);
        munmap(memory, length);
        if (error) *error = kNotEnoughMemory;
        return nullptr;
    }
    return memory;
}

inline bool GuestVirtualMemory::release(void* base, size_t bytes,
                                        uint32_t type, uint32_t* error)
{
    using namespace virtual_memory_detail;
    if (error) *error = kInvalidParameter;
    if (!base || bytes || type != kRelease) return false;
    std::lock_guard guard(mu_);
    auto found = regions_.find(base);
    if (found == regions_.end())
    {
        if (error) *error = kInvalidAddress;
        return false;
    }
    if (found->second.committed)
        unregister_region(base, found->second.protect);
    const size_t length = found->second.mapped;
    if (munmap(base, length) != 0)
    {
        if (found->second.committed)
            register_region(base, length, found->second.protect);
        return false;
    }
    regions_.erase(found);
    return true;
}

} // namespace cap13
#endif
