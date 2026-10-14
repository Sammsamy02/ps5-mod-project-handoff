#ifndef CAP13_GUEST_PROCESS_HEAP_H
#define CAP13_GUEST_PROCESS_HEAP_H

#include "guest_read_map.h"
#include "guest_write_map.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
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
    void* resize(void* heap, uint32_t flags, void* block, size_t bytes);
    size_t size_of(void* heap, uint32_t flags, const void* block) const;
    bool release(void* heap, uint32_t flags, void* block);

  private:
    struct Block { size_t requested; size_t mapped; };
    mutable std::mutex mu_;
    GuestReadMap* reads_ = nullptr;
    GuestWriteMap* writes_ = nullptr;
    std::unordered_map<void*, Block> blocks_;
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
    if (bytes > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()))
        return nullptr;
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
    try { blocks_.emplace(block, Block{bytes, length}); }
    catch (const std::bad_alloc&)
    {
        writes_->remove(block);
        reads_->remove(block);
        std::free(block);
        return nullptr;
    }
    return block;
}

inline void* GuestProcessHeap::resize(void* heap, uint32_t flags,
                                       void* block, size_t bytes)
{
    if (heap != this || !block || (flags & ~uint32_t{8})) return nullptr;
    if (bytes > static_cast<size_t>(std::numeric_limits<ptrdiff_t>::max()))
        return nullptr;
    std::lock_guard guard(mu_);
    if (!reads_ || !writes_) return nullptr;
    auto old = blocks_.find(block);
    if (old == blocks_.end()) return nullptr;
    const size_t old_requested = old->second.requested;
    const size_t length = bytes ? bytes : 1;
    void* replacement = std::malloc(length);
    if (!replacement) return nullptr; // The old block and both maps remain.
    const size_t copied = old_requested < bytes ? old_requested : bytes;
    if (copied) std::memcpy(replacement, block, copied);
    if ((flags & 8) && bytes > copied)
        std::memset(static_cast<uint8_t*>(replacement) + copied, 0,
                    bytes - copied);
    if (!reads_->add(replacement, replacement, length))
    {
        std::free(replacement);
        return nullptr;
    }
    if (!writes_->add(replacement, replacement, length))
    {
        reads_->remove(replacement);
        std::free(replacement);
        return nullptr;
    }
    try { blocks_.emplace(replacement, Block{bytes, length}); }
    catch (const std::bad_alloc&)
    {
        writes_->remove(replacement);
        reads_->remove(replacement);
        std::free(replacement);
        return nullptr;
    }
    // Remove both old registrations while the old allocation is still live.
    const bool read_removed = reads_->remove(block);
    const bool write_removed = writes_->remove(block);
    if (!read_removed || !write_removed)
    {
        blocks_.erase(replacement);
        writes_->remove(replacement);
        reads_->remove(replacement);
        std::free(replacement);
        return nullptr;
    }
    blocks_.erase(block);
    std::free(block);
    return replacement;
}

inline size_t GuestProcessHeap::size_of(void* heap, uint32_t flags,
                                        const void* block) const
{
    if (heap != this || flags || !block) return static_cast<size_t>(-1);
    std::lock_guard guard(mu_);
    auto found = blocks_.find(const_cast<void*>(block));
    return found == blocks_.end() ? static_cast<size_t>(-1) :
                                    found->second.requested;
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
