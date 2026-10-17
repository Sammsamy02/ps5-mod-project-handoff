// guest_read_map.h -- bounded reads from validated, owned guest regions.
#ifndef CAP13_GUEST_READ_MAP_H
#define CAP13_GUEST_READ_MAP_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace cap13 {

// The owner must keep each registered allocation readable and alive until
// removal, and quiesce concurrent guest calls before releasing it. A range
// registration is not an OS memory probe or a substitute for PE validation.
class GuestReadMap
{
  public:
    bool add(const void* owner, const void* base, size_t length)
    {
        if (!owner || !base || !length) return false;
        const auto begin = reinterpret_cast<uintptr_t>(base);
        if (length > UINTPTR_MAX - begin) return false;
        const auto end = begin + length;
        std::lock_guard<std::mutex> guard(mu_);
        for (const auto& range : ranges_)
            if (range.owner == owner || (begin < range.end && range.begin < end))
                return false;
        try { ranges_.push_back({owner, begin, end}); }
        catch (const std::bad_alloc&) { return false; }
        return true;
    }

    bool remove(const void* owner)
    {
        if (!owner) return false;
        std::lock_guard<std::mutex> guard(mu_);
        for (auto it = ranges_.begin(); it != ranges_.end(); ++it)
            if (it->owner == owner)
            {
                ranges_.erase(it);
                return true;
            }
        return false;
    }

    bool read(const void* address, void* output, size_t length) const
    {
        if (!address || !output || !length) return false;
        const auto begin = reinterpret_cast<uintptr_t>(address);
        std::lock_guard<std::mutex> guard(mu_);
        for (const auto& range : ranges_)
            if (begin >= range.begin && begin < range.end &&
                length <= range.end - begin)
            {
                std::memcpy(output, address, length);
                return true;
            }
        return false;
    }

    static bool read_callback(void* user, const void* address,
                              void* output, size_t length)
    {
        return user && static_cast<GuestReadMap*>(user)->read(address, output,
                                                               length);
    }

  private:
    struct Range { const void* owner; uintptr_t begin; uintptr_t end; };
    mutable std::mutex mu_;
    std::vector<Range> ranges_;
};

} // namespace cap13
#endif // CAP13_GUEST_READ_MAP_H
