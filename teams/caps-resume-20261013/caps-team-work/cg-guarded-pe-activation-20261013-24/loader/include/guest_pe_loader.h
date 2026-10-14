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

    GuestNotifyStatus process_attach(GuestTebView& teb);
    GuestNotifyStatus thread_attach(GuestTebView& teb);
    GuestNotifyStatus thread_detach(GuestTebView& teb);
    GuestNotifyStatus process_detach(GuestTebView& teb);

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
    GuestPeModule(LoadResult load, StaticTlsBank& bank,
                  GuestSegmentSwitch& segment, std::vector<uint64_t> callbacks);

    LoadResult load_;
    StaticTlsBank& bank_;
    GuestSegmentSwitch& segment_;
    GuestModuleLifecycle lifecycle_;
    std::atomic<bool> attached_{false};
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

} // namespace cap13
#endif
