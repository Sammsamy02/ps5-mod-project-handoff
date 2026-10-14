#include "ThreadNameRuntime.hpp"
#include "NativeThreadAuthorityRetire.h"
#include <atomic>
#include <type_traits>
#include <OwnedMappedScanSource.hpp>
#include <stdexcept>
namespace NativeNameThread {
namespace {
// Trivial, constant-initialized guards outlive the managed owner state.
enum : unsigned { BeforeConstruction, Ready, Closing, AfterDestruction };
constinit std::atomic<unsigned> constructor_phase{BeforeConstruction};
constinit std::atomic<unsigned> retire_entries{0};
static_assert(std::atomic<unsigned>::is_always_lock_free);
static_assert(std::is_trivially_destructible_v<std::atomic<unsigned>>);
std::mutex owner_mutex;
std::shared_ptr<Binding> current_binding;
struct RuntimeLifetime final {
 RuntimeLifetime() noexcept {constructor_phase.store(Ready,std::memory_order_release);}
 ~RuntimeLifetime() noexcept {
  constructor_phase.store(Closing,std::memory_order_release);
  // Root must retire successfully before fini. This only protects C entries
  // already admitted while ready from managed owner-state destruction.
  while(retire_entries.load(std::memory_order_acquire))
   std::atomic_signal_fence(std::memory_order_seq_cst);
  constructor_phase.store(AfterDestruction,std::memory_order_release);
 }
} runtime_lifetime;
}
void prepare_runtime(const RC::Unreal::UnrealInitializer::Config& config,
 const rc::codecache::Metadata* image,const Data& data,const char* digest,std::uint64_t now,bool admitted){
 if(!config.ModuleSource||!config.NativeScanSource||config.NativeScanSource->source_owner()!=config.ModuleSource)
  throw std::runtime_error("[NativeNameThread] exact current main-image source owner mismatch");
 auto p=Binding::prepare(config.ModuleSource,image,data,digest,now,admitted,config.StartupWait);
 if(!p.binding)throw std::runtime_error("[NativeNameThread] preseed preparation refused: authenticated third code pin and exact postpackage scePthreadGetthreadid/libkernel import admission required");
 std::lock_guard lock{owner_mutex};if(current_binding)throw std::runtime_error("[NativeNameThread] refusing replacement of live authority");current_binding=std::move(p.binding);
}
Acquired acquire_runtime() noexcept {try{std::shared_ptr<Binding> owner;{std::lock_guard lock{owner_mutex};owner=current_binding;}return owner?owner->acquire():Acquired{{},Refusal::Owner};}catch(...){return {{},Refusal::Internal};}}
bool retire_runtime() noexcept {try{std::shared_ptr<Binding> owner;{std::lock_guard lock{owner_mutex};owner=current_binding;}if(!owner)return true;if(!owner->cancel_and_wait())return false;std::lock_guard lock{owner_mutex};if(current_binding==owner)current_binding.reset();return true;}catch(...){return false;}}
}

namespace NativeNameThread {
namespace {
int retire_entry() noexcept {
 if(constructor_phase.load(std::memory_order_acquire)!=Ready)return -1;
 retire_entries.fetch_add(1,std::memory_order_acq_rel);
 if(constructor_phase.load(std::memory_order_acquire)!=Ready){
  retire_entries.fetch_sub(1,std::memory_order_release);return -1;
 }
 const int status=retire_runtime()?0:1;
 retire_entries.fetch_sub(1,std::memory_order_release);
 return status;
}
}
}
extern "C" int bridge_native_thread_authority_retire(void) noexcept {
 return NativeNameThread::retire_entry();
}
