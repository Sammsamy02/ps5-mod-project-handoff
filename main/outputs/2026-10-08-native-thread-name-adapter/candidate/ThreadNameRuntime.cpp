#include "ThreadNameRuntime.hpp"
#include <OwnedMappedScanSource.hpp>
#include <stdexcept>
namespace NativeNameThread {
namespace {std::mutex owner_mutex;std::shared_ptr<Binding> current_binding;}
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
