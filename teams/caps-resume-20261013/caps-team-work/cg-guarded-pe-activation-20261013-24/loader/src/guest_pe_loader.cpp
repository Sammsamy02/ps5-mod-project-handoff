#include "guest_pe_loader.h"

#include "pe_loader_internal.h"
#include "pe_tls_registration.h"
#include "win32_last_error_state.h"
#include "win64_bridge.h"

#include <cstring>
#include <new>
#include <utility>

namespace cap13 {

namespace {

struct PrepareContext
{
    StaticTlsBank& bank;
    std::vector<uint64_t> callbacks;
};

bool file_backed_executable(uint64_t callback, void* user)
{
    const auto& view = *static_cast<const PeMappedPreparation*>(user);
    const uint64_t base = reinterpret_cast<uint64_t>(view.image);
    if (callback < base || callback - base >= view.image_size) return false;
    const uint64_t rva = callback - base;
    for (uint16_t i = 0; i < view.section_count; ++i)
    {
        const PeSectionHeader& section = view.sections[i];
        if (!(section.Characteristics & PE_SCN_MEM_EXECUTE)) continue;
        const uint32_t backed = section.VirtualSize && section.VirtualSize < section.SizeOfRawData
            ? section.VirtualSize : section.SizeOfRawData;
        if (rva >= section.VirtualAddress &&
            rva - section.VirtualAddress < backed) return true;
    }
    return false;
}

bool prepare_image(const PeMappedPreparation& view, std::string& reason, void* user)
{
    auto& context = *static_cast<PrepareContext*>(user);
    if (!view.tls) return true;
    PeTlsDirectory64 tls{};
    std::memcpy(&tls, view.tls, sizeof(tls));
    // A directory with no template, callbacks, index, or zero-fill is only
    // metadata. Real static TLS must have a valid AddressOfIndex target.
    if (tls.StartAddressOfRawData == 0 && tls.EndAddressOfRawData == 0 &&
        tls.AddressOfIndex == 0 && tls.AddressOfCallBacks == 0 &&
        tls.SizeOfZeroFill == 0) return true;
    const uint64_t base = reinterpret_cast<uint64_t>(view.image);
    bool writable_index = false;
    if (tls.AddressOfIndex >= base && tls.AddressOfIndex - base < view.image_size)
    {
        const uint64_t index_rva = tls.AddressOfIndex - base;
        for (uint16_t i = 0; i < view.section_count; ++i)
        {
            const PeSectionHeader& section = view.sections[i];
            const uint64_t span = section.VirtualSize > section.SizeOfRawData
                ? section.VirtualSize : section.SizeOfRawData;
            if ((section.Characteristics & PE_SCN_MEM_WRITE) &&
                !(section.Characteristics & PE_SCN_MEM_EXECUTE) &&
                index_rva >= section.VirtualAddress &&
                index_rva - section.VirtualAddress <= span &&
                sizeof(uint32_t) <= span - (index_rva - section.VirtualAddress))
            {
                writable_index = true;
                break;
            }
        }
    }
    if (!writable_index)
    {
        reason = "TLS index target is not in a writable non-executable section";
        return false;
    }
    PeMappedPreparation validation_view = view;
    PeTlsRegistration prepared = register_mapped_static_tls(
        context.bank, view.image, view.image_size, tls,
        file_backed_executable, &validation_view);
    if (!prepared.ok)
    {
        reason = prepared.reason;
        return false;
    }
    context.callbacks = std::move(prepared.callbacks);
    return true;
}

class LeaveSegment final
{
  public:
    explicit LeaveSegment(GuestSegmentSwitch& segment) : segment_(segment) {}
    ~LeaveSegment() { segment_.leave(); }
    LeaveSegment(const LeaveSegment&) = delete;
    LeaveSegment& operator=(const LeaveSegment&) = delete;
  private:
    GuestSegmentSwitch& segment_;
};

uint64_t call_integer(void* fn, const uint64_t* a, size_t count)
{
    switch (count)
    {
        case 0: return cap13_win64_call0(fn);
        case 1: return cap13_win64_call1(fn, a[0]);
        case 2: return cap13_win64_call2(fn, a[0], a[1]);
        case 3: return cap13_win64_call3(fn, a[0], a[1], a[2]);
        case 4: return cap13_win64_call4(fn, a[0], a[1], a[2], a[3]);
        case 5: return cap13_win64_call5(fn, a[0], a[1], a[2], a[3], a[4]);
        case 6: return cap13_win64_call6(fn, a[0], a[1], a[2], a[3], a[4], a[5]);
        default: return 0; // Caller has already refused unsupported arity.
    }
}

} // namespace

GuestPeModule::GuestPeModule(LoadResult load, StaticTlsBank& bank,
                             GuestSegmentSwitch& segment,
                             std::vector<uint64_t> callbacks)
    : load_(std::move(load)),
      bank_(bank), segment_(segment),
      lifecycle_(bank, segment, reinterpret_cast<uint64_t>(load_.image),
                 load_.entry_rva ? reinterpret_cast<uint64_t>(load_.image + load_.entry_rva) : 0,
                 std::move(callbacks)) {}

GuestNotifyStatus GuestPeModule::process_attach(GuestTebView& teb)
{
    const auto status = lifecycle_.process_attach(teb);
    if (status == GuestNotifyStatus::Ok) attached_ = true;
    return status;
}
GuestNotifyStatus GuestPeModule::thread_attach(GuestTebView& teb)
{
    return lifecycle_.thread_attach(teb);
}
GuestNotifyStatus GuestPeModule::thread_detach(GuestTebView& teb)
{
    return lifecycle_.thread_detach(teb);
}
GuestNotifyStatus GuestPeModule::process_detach(GuestTebView& teb)
{
    const auto status = lifecycle_.process_detach(teb);
    if (status == GuestNotifyStatus::Ok) attached_ = false;
    return status;
}

GuestCallStatus GuestPeModule::invoke_integer_export(
    const char* name, GuestTebView& teb, const uint64_t* args,
    size_t arg_count, uint64_t* result)
{
    if (!attached_) return GuestCallStatus::WrongState;
    if (!teb.belongs_to(bank_) || !teb.on_owner_thread())
        return GuestCallStatus::WrongThreadOrBank;
    if (!name || !result || (arg_count && !args))
        return GuestCallStatus::InvalidArguments;
    if (arg_count > 6) return GuestCallStatus::UnsupportedArity;
    void* fn = load_.find_export(name);
    if (!fn) return GuestCallStatus::MissingExport;
    try { (void)teb.refresh(); }
    catch (const std::bad_alloc&) { return GuestCallStatus::TlsPreparationFailed; }
    if (!segment_.enter(teb.address())) return GuestCallStatus::BackendRefused;
    LeaveSegment leave(segment_);
    ScopedGuestLastErrorBinding error_binding(teb.address());
    *result = call_integer(fn, args, arg_count);
    return GuestCallStatus::Ok;
}

GuestPeLoad pe_prepare_guest(const void* file_bytes, size_t file_size,
                             ImportResolver& resolver, const LoadOptions& options,
                             StaticTlsBank& bank, GuestSegmentSwitch& segment)
{
    GuestPeLoad result;
    PrepareContext context{bank, {}};
    LoadResult mapped = pe_load_internal(file_bytes, file_size, resolver, options,
                                         true, prepare_image, &context);
    if (!mapped.ok)
    {
        result.reason = std::move(mapped.reason);
        result.missing = std::move(mapped.missing);
        return result;
    }
    result.module.reset(new GuestPeModule(std::move(mapped), bank, segment,
                                          std::move(context.callbacks)));
    result.ok = true;
    return result;
}

} // namespace cap13
