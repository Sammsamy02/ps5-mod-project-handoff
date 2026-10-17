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

// Detach in reverse attachment order. Failed detaches remain in result.
// Caller must quiesce all guest threads and keep shim, bank, and segments
// alive until no attached modules remain.
bool pe_detach_guest_batch(GuestPeBatchResult& result, GuestTebView& teb);

} // namespace cap13
#endif
