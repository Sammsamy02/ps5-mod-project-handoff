// Bounded dependency-ordered attachment of a caller-supplied PE DLL batch.
// No filesystem search, console segment backend, or game target matching.
#ifndef CAP13_GUEST_PE_BATCH_H
#define CAP13_GUEST_PE_BATCH_H

#include "guest_pe_loader.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace cap13 {

struct GuestPeBatchInput
{
    const char* source_path = nullptr;
    const void* file_bytes = nullptr;
    size_t file_size = 0;
    GuestSegmentSwitch* segment = nullptr;
};

// The source is consulted by imported DLL basename. It may search a mod
// folder or another caller-owned store, but must return unchanged PE bytes.
// A fetched segment backend must outlive any attached module in the result.
struct GuestPeFetchedDependency
{
    std::string source_path;
    std::vector<uint8_t> bytes;
    GuestSegmentSwitch* segment = nullptr;
};
using FetchPeDependency = bool (*)(void* user, const char* import_dll,
                                    GuestPeFetchedDependency* out);

struct GuestPeBatchResult
{
    bool ok = false;
    bool rollback_failed = false;
    std::string reason;
    std::vector<std::string> missing;
    // Parallel arrays in process-attach order. On failure, only modules whose
    // rollback failed remain here; the caller must retain and detach them.
    std::vector<size_t> input_indices;
    std::vector<std::unique_ptr<GuestPeModule>> modules;
};

GuestPeBatchResult pe_attach_guest_batch(
    const std::vector<GuestPeBatchInput>& inputs, WinApiShim& shim,
    const LoadOptions& options, StaticTlsBank& bank, GuestTebView& teb);

// Recursively collect static DLL imports from one root image and fetch
// locally available dependencies without per-game names or fixed addresses.
// System/framework imports already known to the shim are left to the shim.
GuestPeBatchResult pe_attach_guest_closure(
    const GuestPeBatchInput& root, FetchPeDependency fetch, void* fetch_user,
    WinApiShim& shim, const LoadOptions& options, StaticTlsBank& bank,
    GuestTebView& teb);

// Detach in reverse attachment order. Failed detaches remain in result.
// Caller must quiesce all guest threads and keep shim, bank, and segments
// alive until no attached modules remain.
bool pe_detach_guest_batch(GuestPeBatchResult& result, GuestTebView& teb);

} // namespace cap13
#endif
