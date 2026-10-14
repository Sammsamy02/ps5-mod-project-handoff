#include "SelectedThreadAuthorityAdapter.hpp"
#include <stdexcept>
#include <OwnedMappedScanSource.hpp>
extern "C" int bridge_code_cache_context(std::uint64_t,const rc::codecache::Metadata**);
namespace NativeThreadIntegration {
SelectionReport prepare_selected_source(const RC::Unreal::UnrealInitializer::Config& config,
                                        const Profile& profile,const char* digest,std::uint64_t now) noexcept {
 SelectionReport r;
 r.cooperative_cancellation_available=config.StartupWait&&bool(config.StartupWait->IsCancelled);
 try {
  if(!config.ModuleSource||!config.NativeScanSource||config.NativeScanSource->source_owner()!=config.ModuleSource){r.prepared.refusal=Refusal::ImageOwner;return r;}
  r.same_current_source=true;
  const auto& view=config.ModuleSource->Views().array[static_cast<std::size_t>(RC::ScanTarget::MainExe)];
  const auto base=reinterpret_cast<std::uintptr_t>(view.image_base);
  const rc::codecache::Metadata* authenticated=nullptr;
  if(bridge_code_cache_context(base,&authenticated)!=1||!authenticated){r.prepared.refusal=Refusal::Metadata;return r;}
  r.initializer_metadata=true;
  // Current Config/NativeProgramContext expose neither supported proof.
  // VmScan supplier retention=None is not promoted by the shared_ptr alias.
  // A static import must separately receive the packager/provider review;
  // ModuleSnapshot::FindExport is unset and is deliberately not consulted.
  r.prepared=Authority::prepare(config.ModuleSource,authenticated,profile,digest,now,ImportProof{},MappingLifetimeProof{});
 }catch(...){r.prepared.refusal=Refusal::InternalFailure;}
 return r;
}
void require_selected_authority(const RC::Unreal::UnrealInitializer::Config& config,
                               const Profile& profile,const char* digest,std::uint64_t now) {
 const auto r=prepare_selected_source(config,profile,digest,now);
 if(!r.prepared.authority)throw std::runtime_error(
  "[NativeThreadAuthority] preparation refused before seed consumption/map publication: current source lacks supported actual-mapping lifetime contract and independently reviewed scePthreadGetthreadid/libkernel import authority");
}
}
