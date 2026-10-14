#pragma once
#include <OwnedMappedImageCaller.hpp>
#include <memory>
#include <vector>
namespace RC::Unreal::Platform {
// Private immutable file bytes, tied to one exact retained snapshot by checked
// immutable-span correspondence. This validates an existing loaded image; it
// does not own its loader, imports, static TLS or unrelated mutable state.
// The genuine resolver emits the additional data witnesses needed for Tick.
class OwnedSerializedElfSource final {
    struct Load {uint64_t offset,address,file_size,memory_size;uint32_t flags;};
    struct Relocation {uint64_t address,info,addend;};
    std::shared_ptr<const ModuleSnapshot> owner_;
    const std::vector<uint8_t> bytes_;
    std::vector<Load> loads_;
    std::vector<Relocation> relocations_;
    uint64_t low_{},high_{},runtime_base_{};
    bool supported_relocations_{true};
    OwnedSerializedElfSource(const OwnedMappedImageCaller&,const std::vector<uint8_t>&);
    auto translate(uint64_t,size_t,bool) const -> uint64_t;
    auto file_offset(uint64_t,size_t) const -> uint64_t;
    auto source_matches(const OwnedMappedImageCaller&,uint64_t,size_t) const -> bool;
public:
    static auto Create(const OwnedMappedImageCaller&,const std::vector<uint8_t>&)
        -> std::shared_ptr<const OwnedSerializedElfSource>;
    auto matches(const OwnedMappedImageCaller&) const noexcept -> bool;
    // Revalidates every genuine witness under the same read lease. A result is
    // a file-derived mapped candidate, not live-wide uniqueness or callable ABI.
    auto tick(const OwnedMappedImageCaller&,uint64_t& runtime_address) const noexcept -> int32_t;
};
}
