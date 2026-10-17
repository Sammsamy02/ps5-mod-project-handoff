#if defined(__PROSPERO__) || defined(PLH_OWNED_MEMORY_HOST_TEST)
#include <polyhook2/OwnedTrampoline.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include <memory>
#include <map>
#include <mutex>
namespace PLH::OwnedMemory {
namespace {
struct Owners {std::mutex mutex;std::map<uint64_t,std::unique_ptr<OwnedMapping>> values;};
auto owners()->Owners& {static Owners value;return value;}
}
auto allocate_trampoline(size_t bytes) noexcept->uint64_t {
 try {auto memory=std::make_unique<OwnedMapping>(bytes);
  const auto address=reinterpret_cast<uintptr_t>(memory->data());
  auto& state=owners();std::lock_guard lock(state.mutex);
  const auto inserted=state.values.emplace(address,std::move(memory));
  return inserted.second ? address : 0;
 }catch(...){return 0;}
}
auto release_trampoline(uint64_t address) noexcept->bool {
 try {std::unique_ptr<OwnedMapping> retired;
  {auto& state=owners();std::lock_guard lock(state.mutex);
   auto found=state.values.find(address);if(found==state.values.end())return false;
   retired=std::move(found->second);state.values.erase(found);
  }
  // Destroy outside the owner-map lock; authoritative service revokes and
  // waits outstanding operations before its retained page owner can unmap.
  retired.reset();return true;
 }catch(...){return false;}
}
}
#endif
