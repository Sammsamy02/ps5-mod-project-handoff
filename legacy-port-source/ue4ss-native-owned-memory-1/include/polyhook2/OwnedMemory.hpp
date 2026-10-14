#pragma once
#include <polyhook2/Enums.hpp>
#include <memory>
#include <optional>

namespace PLH::OwnedMemory {
struct RegionInfo {
    uint64_t base;
    size_t size;
    ProtFlag protection;
    bool protectable;
    // Retains a supplied owner/mapping only; does not freeze permissions.
    std::shared_ptr<const void> lifetime;
};

// Current metadata only. Never authorizes a later unchecked dereference.
// Returns the maximal uniform region containing the entire nonempty extent.
auto query(uint64_t address, uint64_t size = 1) noexcept -> std::optional<RegionInfo>;

class ScopedRegion final {
public:
    // Caller asserts these are genuinely owned, accessible bytes with the given
    // permissions. If owner is empty, lifetime is synchronous: storage must
    // outlive this scope and every access; no external unmap/protect is allowed.
    // An owner must actually retain these bytes, not merely an unrelated object.
    // This is not an API for registering discovered process/game mappings.
    ScopedRegion(void* data, size_t size, ProtFlag protection,
                 std::shared_ptr<const void> owner = {});
    ~ScopedRegion() noexcept;
    ScopedRegion(const ScopedRegion&) = delete;
    auto operator=(const ScopedRegion&) -> ScopedRegion& = delete;
    ScopedRegion(ScopedRegion&&) = delete;
    auto operator=(ScopedRegion&&) -> ScopedRegion& = delete;
private:
    uint64_t token_{};
};

class OwnedMapping final {
public:
    // Real anonymous private RW pages; no fixed mapping or retry. Throws on
    // invalid size or OS refusal. Only these mappings allow mem_protect.
    explicit OwnedMapping(size_t bytes);
    ~OwnedMapping() noexcept;
    OwnedMapping(const OwnedMapping&) = delete;
    auto operator=(const OwnedMapping&) -> OwnedMapping& = delete;
    OwnedMapping(OwnedMapping&&) = delete;
    auto operator=(OwnedMapping&&) -> OwnedMapping& = delete;
    auto data() const noexcept -> void* { return data_; }
    auto size() const noexcept -> size_t { return size_; }
private:
    void* data_{};
    size_t size_{};
    uint64_t token_{};
};
}
