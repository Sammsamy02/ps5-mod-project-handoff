#pragma once
#include <cstddef>
// Private allocator call-site contract. This is not process-map registration.
// record_created is only called immediately after genuine allocator mmap
// success; allocation remains solely owned by that allocator. release_created
// performs the single real munmap and invalidates registration on success.
// Query metadata has no fabricated shared lifetime for these raw allocations.
namespace PLH::OwnedMemory::AllocatorBridge {
auto record_created(void* address,size_t requested_size,int native_protection) noexcept -> int;
auto release_created(void* address,size_t requested_size) noexcept -> int;
auto protect_created(void* address,size_t size,int native_protection) noexcept -> int;
// Separate producer identity: these accept only genuine bounded allocator maps.
auto record_bounded(void* address,size_t rounded_size,int native_protection) noexcept -> int;
auto release_bounded(void* address,size_t rounded_size) noexcept -> int;
// Return 0 on success; otherwise an errno value. Unknown/external ranges refuse.
}
