#pragma once
#include <cstdint>
#include <cstddef>
namespace PLH::OwnedMemory {
auto allocate_trampoline(size_t bytes) noexcept -> uint64_t;
auto release_trampoline(uint64_t address) noexcept -> bool;
}
