#include "NativeThreadAuthority.hpp"
namespace NativeThreadIntegration {
std::int32_t current_native_id(){return 0;}
Lease::~Lease() noexcept {}
Prepared Authority::prepare(std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot>,const rc::codecache::Metadata*,const Profile&,const char*,std::uint64_t,ImportProof,MappingLifetimeProof) noexcept{return {{},Refusal::Metadata};}
Acquired Authority::acquire() noexcept{return {{},Refusal::ThreadIdentity};}
bool Authority::cancel_and_wait() noexcept {return false;}
std::thread::id Authority::known_affinity() noexcept {return {};}
}
