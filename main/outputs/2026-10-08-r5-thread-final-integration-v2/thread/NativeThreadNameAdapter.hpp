#pragma once
#include <Unreal/PlatformModuleSnapshot.hpp>
#include <Unreal/StartupWaitControl.hpp>
#include "ThreadNameProfile.hpp"
#include "NameThreadPolicy.hpp"
#include <condition_variable>
#include <memory>
#include <unordered_map>
namespace NativeNameThread {
struct Data {
 const char* image_sha{};const char* layout_digest{};
 std::uint64_t image_size{},image_extent{},accessor{},game_name{};
 const rc::codecache::Pin* pin{};
};
enum class Refusal {None,Owner,Image,Profile,CodePin,Storage,Import,Cancelled,NativeIdentity,Internal};
class Binding;
class Ticket final {
 friend class Binding;std::shared_ptr<Binding> owner_;std::thread::id thread_;
 explicit Ticket(std::shared_ptr<Binding> b):owner_(std::move(b)),thread_(std::this_thread::get_id()){}
public:~Ticket() noexcept;Ticket(const Ticket&)=delete;Ticket&operator=(const Ticket&)=delete;
};
struct Prepared {std::shared_ptr<Binding> binding;Refusal refusal=Refusal::Internal;};
struct Acquired {std::unique_ptr<Ticket> ticket;Refusal refusal=Refusal::Internal;};
class Binding final:public std::enable_shared_from_this<Binding> {
 friend class Ticket;
 std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot> owner_;
 std::shared_ptr<const RC::Unreal::UnrealInitializer::StartupWaitControl> cancellation_;
 std::uintptr_t function_{},expected_{};Policy policy_;std::atomic<bool> stopped_{false};
 std::mutex mutex_;std::condition_variable changed_;std::size_t active_{};
 std::unordered_map<std::thread::id,std::size_t> threads_;
 bool cancelled() noexcept;void release() noexcept;
 static bool current(void*,std::int32_t&);static bool name(void*,std::uint32_t,const void*&);
public:
 // Same ordinary native function-call ownership contract as existing FName/SCO:
 // caller must retain engine/runtime code residence across the ticket. This is
 // not a new direct-memory readable-mapping claim or a VmScan retention upgrade.
 static Prepared prepare(std::shared_ptr<const RC::Unreal::Platform::ModuleSnapshot>,
                         const rc::codecache::Metadata*,const Data&,const char* selected_digest,
                         std::uint64_t now_ms,bool exact_static_libkernel_import_admitted,
                         std::shared_ptr<const RC::Unreal::UnrealInitializer::StartupWaitControl>) noexcept;
 Acquired acquire() noexcept;
 bool cancel_and_wait() noexcept;
};
}
