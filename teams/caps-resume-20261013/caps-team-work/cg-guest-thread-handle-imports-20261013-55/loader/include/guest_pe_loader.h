// Opaque guarded PE route. No raw guest entry/export address is published.
#ifndef CAP13_GUEST_PE_LOADER_H
#define CAP13_GUEST_PE_LOADER_H

#include "guest_teb_lifecycle.h"
#include "pe_loader.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace cap13 {

struct GuestPeLoad;
class WinApiShim;

enum class GuestCallStatus
{
    Ok,
    WrongState,
    WrongThreadOrBank,
    InvalidArguments,
    UnsupportedArity,
    MissingExport,
    TlsPreparationFailed,
    BackendRefused
};

class GuestPeModule
{
  public:
    GuestPeModule(const GuestPeModule&) = delete;
    GuestPeModule& operator=(const GuestPeModule&) = delete;
    ~GuestPeModule();

    GuestNotifyStatus process_attach(GuestTebView& teb);
    GuestNotifyStatus thread_attach(GuestTebView& teb);
    GuestNotifyStatus thread_detach(GuestTebView& teb);
    GuestNotifyStatus process_detach(GuestTebView& teb);

    // Opaque Windows HMODULE for an attached mapped PE. Null before attach
    // and after detach. The image remains owned by this module's load result.
    void* mapped_handle() const
    {
        return attached_.load() ? load_.image : nullptr;
    }

    // The start address for a guest worker must lie in a validated
    // executable section of this attached image, not merely in its mapping.
    bool owns_executable_address(const void* address) const noexcept;

    // Invoke one validated Win64 worker-start function on the current host
    // thread after its DLL_THREAD_ATTACH notifications have completed.
    GuestCallStatus invoke_thread_start(void* start, void* argument,
                                        GuestTebView& teb,
                                        uint32_t* exit_code);

    // All-integer/pointer signatures only, zero through six arguments.
    // The caller serializes module notifications and export calls.
    GuestCallStatus invoke_integer_export(const char* name, GuestTebView& teb,
                                           const uint64_t* args, size_t arg_count,
                                           uint64_t* result);

  private:
    friend struct GuestPeLoad;
    friend GuestPeLoad pe_prepare_guest(const void*, size_t, ImportResolver&,
                                       const LoadOptions&, StaticTlsBank&,
                                       GuestSegmentSwitch&);
    friend GuestPeLoad pe_prepare_guest_with_shim(const void*, size_t,
                                                  WinApiShim&, const LoadOptions&,
                                                  StaticTlsBank&, GuestSegmentSwitch&,
                                                  const char*);
    GuestPeModule(LoadResult load, StaticTlsBank& bank,
                  GuestSegmentSwitch& segment, std::vector<uint64_t> callbacks,
                  bool has_static_tls);
    static bool disable_thread_notifications(void* user);
    static bool lookup_export(void* owner, const char* symbol,
                              uint64_t ordinal, void** out,
                              std::string* forwarder);

    LoadResult load_;
    StaticTlsBank& bank_;
    GuestSegmentSwitch& segment_;
    GuestModuleLifecycle lifecycle_;
    std::atomic<bool> attached_{false};
    WinApiShim* shim_ = nullptr; // shim must outlive this retained module
    bool has_static_tls_ = false;
    bool policy_registered_ = false;
    bool module_registered_ = false;
    std::string source_path_;
};

struct GuestPeLoad
{
    bool ok = false;
    std::string reason;
    std::vector<std::string> missing;
    std::unique_ptr<GuestPeModule> module;
};

// This route permits real static TLS and possible GS bytes only because all
// guest entry remains behind GuestSegmentSwitch. The production unavailable
// backend refuses attachment; no PS5 segment backend is provided here.
GuestPeLoad pe_prepare_guest(const void* file_bytes, size_t file_size,
                             ImportResolver& resolver, const LoadOptions& options,
                             StaticTlsBank& bank, GuestSegmentSwitch& segment);

// Runtime route: uses only reviewed WinApiShim import rows and registers the
// mapped HMODULE's thread-notification policy before each process attach.
// The shim and bank must outlive the module. Caller serializes notifications
// and ensures guest threads are quiescent before destroying the module.
GuestPeLoad pe_prepare_guest_with_shim(const void* file_bytes, size_t file_size,
                                       WinApiShim& shim, const LoadOptions& options,
                                       StaticTlsBank& bank, GuestSegmentSwitch& segment,
                                       const char* source_path = nullptr);

} // namespace cap13
#endif
