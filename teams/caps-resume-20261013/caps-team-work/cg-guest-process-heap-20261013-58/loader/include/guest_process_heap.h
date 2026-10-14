#ifndef CAP13_GUEST_PROCESS_HEAP_H
#define CAP13_GUEST_PROCESS_HEAP_H

#include "guest_read_map.h"
#include "guest_write_map.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <unordered_map>

namespace cap13 {

// One process-heap handle for all DLLs sharing this WinApiShim. Every live
// block is registered in both guarded guest maps before publication. The
// maps and heap must remain alive until all guest callers are quiescent.
class GuestProcessHeap
{
  public:
    bool bind(GuestReadMap* reads, GuestWriteMap* writes);
    bool ready() const;
    void* handle() const;
    void* allocate(void* heap, uint32_t flags, size_t bytes);
    bool release(void* heap, uint32_t flags, void* block);

  private:
    mutable std::mutex mu_;
    GuestReadMap* reads_ = nullptr;
    GuestWriteMap* writes_ = nullptr;
    std::unordered_map<void*, size_t> blocks_;
};


inline bool GuestProcessHeap::bind(GuestReadMap* reads, GuestWriteMap* writes)
{
    if (!reads || !writes) return false;
    std::lock_guard guard(mu_);
    if (reads_ || writes_)
        return reads_ == reads && writes_ == writes;
    reads_ = reads;
    writes_ = writes;
    return true;
}

inline bool GuestProcessHeap::ready() const
{
    std::lock_guard guard(mu_);
    return reads_ && writes_;
}

inline void* GuestProcessHeap::handle() const
{
    std::lock_guard guard(mu_);
    return reads_ && writes_ ? const_cast<GuestProcessHeap*>(this) : nullptr;
}

inline void* GuestProcessHeap::allocate(void* heap, uint32_t flags, size_t bytes)
{
    if (heap != this || (flags & ~uint32_t{8})) return nullptr;
    std::lock_guard guard(mu_);
    if (!reads_ || !writes_) return nullptr;
    const size_t length = bytes ? bytes : 1;
    void* block = std::malloc(length);
    if (!block) return nullptr;
    if (flags & 8) std::memset(block, 0, length);
    if (!reads_->add(block, block, length))
    {
        std::free(block);
        return nullptr;
    }
    if (!writes_->add(block, block, length))
    {
        reads_->remove(block);
        std::free(block);
        return nullptr;
    }
    try { blocks_.emplace(block, length); }
    catch (const std::bad_alloc&)
    {
        writes_->remove(block);
        reads_->remove(block);
        std::free(block);
        return nullptr;
    }
    return block;
}

inline bool GuestProcessHeap::release(void* heap, uint32_t flags, void* block)
{
    if (heap != this || flags) return false;
    std::lock_guard guard(mu_);
    if (!reads_ || !writes_) return false;
    if (!block) return true;
    auto found = blocks_.find(block);
    if (found == blocks_.end()) return false;
    // Each removal waits for in-progress guarded reads/writes. A guest must
    // not directly dereference a block while another thread frees it.
    const bool read_removed = reads_->remove(block);
    const bool write_removed = writes_->remove(block);
    if (!read_removed || !write_removed) return false;
    blocks_.erase(found);
    std::free(block);
    return true;
}

} // namespace cap13
#endif
