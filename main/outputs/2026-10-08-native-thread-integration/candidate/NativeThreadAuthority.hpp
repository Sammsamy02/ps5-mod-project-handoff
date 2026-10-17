#pragma once
#include <Unreal/PlatformModuleSnapshot.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include "CodeImageCache.hpp"
#include "ImageCoverage.hpp"
#include "StrictNativeThreadPolicy.hpp"
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace NativeThreadIntegration {
struct Profile {
    const char* image_sha{};const char* layout_digest{};const char* fact_digest{};
    std::uint64_t image_size{},image_extent{},id_offset{},ready_offset{};
    const char* import_name{};const char* import_nid{};const char* import_library{};
};
struct ImportProof {
    bool independently_verified=false;
    std::string name,nid,library,review_digest;
};
struct MappingLifetimeProof {
    // Producer must independently establish that this owner pins the actual
    // described mappings and prevents external unmap/protection changes.
    // Retaining only VmScan metadata does not satisfy this assertion.
    bool independently_verified=false;
    std::shared_ptr<const void> actual_mapping_owner;
    std::function<bool()> cancelled;
};
enum class Refusal {None,ImageOwner,Metadata,Profile,Base,ImportAuthority,MappingLifetime,
                    ReadableBss,RegistryRead,ThreadIdentity,Cancelled,InternalFailure};
class Authority;
class Lease final {
    friend class Authority;
    std::shared_ptr<Authority> authority_;
    std::thread::id thread_;
    explicit Lease(std::shared_ptr<Authority> a):authority_(std::move(a)),thread_(std::this_thread::get_id()){}
public:
    ~Lease() noexcept;
    Lease(const Lease&)=delete;Lease& operator=(const Lease&)=delete;
};
struct Prepared {std::shared_ptr<Authority> authority;Refusal refusal=Refusal::InternalFailure;};
struct Acquired {std::unique_ptr<Lease> lease;Refusal refusal=Refusal::InternalFailure;};
// The only callable ID path is the exact typed static import in the .cpp.
// The profile/proof seam cannot supply an arbitrary function pointer.
std::int32_t current_native_id();
class Authority final:public std::enable_shared_from_this<Authority> {
    friend class Lease;
    std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> owner_;
    rc::codecache::Metadata image_{};
    Profile profile_{};
    std::string sha_,layout_,facts_,import_name_,import_nid_,import_library_;
    MappingLifetimeProof lifetime_;
    NativeThreadAudit::StrictThreadPolicy policy_;
    std::atomic<bool> cancelled_{false};
    std::mutex mutex_;std::condition_variable changed_;
    std::size_t active_{};
    std::unordered_map<std::thread::id,std::size_t> threads_;
    std::thread::id affinity_{};
    struct Reader {Authority& authority;Refusal failure=Refusal::None;};
    static bool same(const char*,const char*);
    bool externally_cancelled() noexcept;
    bool in_supplied_range(std::uint64_t,std::size_t) const noexcept;
    bool bss(std::uint64_t,std::size_t) const noexcept;
    bool snapshot(NativeThreadAudit::EngineSample&,Refusal&) noexcept;
    static bool read_sample(void*,NativeThreadAudit::EngineSample&);
    static bool read_id(void*,std::int32_t&);
    void release() noexcept;
public:
    static Prepared prepare(std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> owner,
                            const rc::codecache::Metadata* image,const Profile& profile,
                            const char* selected_layout_digest,std::uint64_t now_ms,
                            ImportProof imports,MappingLifetimeProof lifetime) noexcept;
    Acquired acquire() noexcept;
    // False means self-wait would deadlock. Admission is cancelled regardless;
    // the caller must retain this authority/runtime until existing scopes end.
    bool cancel_and_wait() noexcept;
    std::thread::id known_affinity() noexcept;
};
}
