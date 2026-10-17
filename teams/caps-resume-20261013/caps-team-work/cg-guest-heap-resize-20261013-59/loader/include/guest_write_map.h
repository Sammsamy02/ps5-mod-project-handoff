// Bounded output writes only into explicitly registered owned guest memory.
#ifndef CAP13_GUEST_WRITE_MAP_H
#define CAP13_GUEST_WRITE_MAP_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace cap13 {

// The registering owner must prove that the allocation is writable and keep
// it alive until removal. This map cannot infer OS permissions from a pointer.
// Callers must quiesce guest users before freeing a registered allocation.
class GuestWriteMap
{
  public:
    struct Span { void* base; size_t length; };
    bool add(const void* owner, void* base, size_t length)
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
        bool removed = false;
        for (auto it = ranges_.begin(); it != ranges_.end();)
            if (it->owner == owner)
            {
                it = ranges_.erase(it);
                removed = true;
            }
            else ++it;
        return removed;
    }

    // Atomically add every writable section of one owned PE. A plain add()
    // still permits only one range per owner for stack/heap registrations.
    bool add_many(const void* owner, const std::vector<Span>& spans)
    {
        if (!owner || spans.empty()) return false;
        std::lock_guard<std::mutex> guard(mu_);
        for (const auto& range : ranges_)
            if (range.owner == owner) return false;
        for (size_t i = 0; i < spans.size(); ++i)
        {
            if (!spans[i].base || !spans[i].length) return false;
            const uintptr_t begin = reinterpret_cast<uintptr_t>(spans[i].base);
            if (spans[i].length > UINTPTR_MAX - begin) return false;
            const uintptr_t end = begin + spans[i].length;
            for (const auto& range : ranges_)
                if (begin < range.end && range.begin < end) return false;
            for (size_t j = 0; j < i; ++j)
            {
                const uintptr_t prev = reinterpret_cast<uintptr_t>(spans[j].base);
                const uintptr_t prev_end = prev + spans[j].length;
                if (begin < prev_end && prev < end) return false;
            }
        }
        if (spans.size() > ranges_.max_size() - ranges_.size()) return false;
        try { ranges_.reserve(ranges_.size() + spans.size()); }
        catch (const std::bad_alloc&) { return false; }
        for (const auto& span : spans)
        {
            const uintptr_t begin = reinterpret_cast<uintptr_t>(span.base);
            ranges_.push_back({owner, begin, begin + span.length});
        }
        return true;
    }

    bool write(void* address, const void* source, size_t length) const
    {
        if (!address || !source || !length) return false;
        const auto begin = reinterpret_cast<uintptr_t>(address);
        std::lock_guard<std::mutex> guard(mu_);
        for (const auto& range : ranges_)
            if (begin >= range.begin && begin < range.end &&
                length <= range.end - begin)
            {
                std::memmove(address, source, length);
                return true;
            }
        return false;
    }

    static bool write_callback(void* user, void* address,
                               const void* source, size_t length)
    {
        return user && static_cast<GuestWriteMap*>(user)->write(address,
                                                                 source, length);
    }

  private:
    struct Range { const void* owner; uintptr_t begin; uintptr_t end; };
    mutable std::mutex mu_;
    std::vector<Range> ranges_;
};

} // namespace cap13
#endif
