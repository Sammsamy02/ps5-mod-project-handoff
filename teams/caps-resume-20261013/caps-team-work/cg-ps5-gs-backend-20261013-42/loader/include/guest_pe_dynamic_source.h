// Adapter from the guarded LoadLibraryA hook to a caller-owned PE source.
#ifndef CAP13_GUEST_PE_DYNAMIC_SOURCE_H
#define CAP13_GUEST_PE_DYNAMIC_SOURCE_H

#include "guest_pe_batch.h"
#include "win_api_shim.h"

#include <string>
#include <vector>

namespace cap13 {

// Configure before guest entry. All pointers and the retained vector must
// outlive dynamic loads. Reserve retained capacity before invoking guest code;
// this callback refuses when no slot is available, before any PE attach.
// After quiescing guest threads, detach retained batches in reverse order.
struct GuestPeDynamicLoadContext
{
    FetchPeDependency fetch = nullptr;
    void* fetch_user = nullptr;
    WinApiShim* shim = nullptr;
    LoadOptions options{};
    StaticTlsBank* bank = nullptr;
    GuestTebView* teb = nullptr;
    std::vector<GuestPeBatchResult>* retained = nullptr;
    std::string last_error;
};

// WinApiShim::Hooks.load_pe callback. Returns the root PE's HMODULE only
// after its entire static dependency closure has attached successfully.
WinModule pe_load_guest_from_source(void* user, const char* dll_basename);

} // namespace cap13
#endif // CAP13_GUEST_PE_DYNAMIC_SOURCE_H
