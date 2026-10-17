#include "guest_pe_batch.h"
#include "guest_thread_manager.h"
#include "win_api_shim.h"

#include <utility>

namespace cap13 {

bool pe_attach_guest_batch_managed(
    const std::vector<GuestPeBatchInput>& inputs, WinApiShim& shim,
    const LoadOptions& options, StaticTlsBank& bank, GuestTebView& teb,
    GuestThreadManager& manager, GuestPeBatchResult& out)
{
    if (out.ok || !out.modules.empty() || !manager.awaiting_batch() ||
        !shim.uses_guest_thread_manager(&manager))
    {
        out.reason = "managed batch requires an empty result and its bound pending thread manager";
        return false;
    }
    out = pe_attach_guest_batch(inputs, shim, options, bank, teb);
    if (!out.ok)
    {
        if (!manager.abort_pending())
            out.reason += "; pending thread abort failed";
        return false;
    }
    if (manager.activate(out)) return true;
    out.ok = false;
    out.reason = "queued guest thread activation failed";
    if (!manager.abort_pending())
        out.reason += "; pending thread abort failed";
    if (!pe_detach_guest_batch(out, teb))
    {
        out.rollback_failed = true;
        out.reason += "; batch rollback failed";
    }
    return false;
}

} // namespace cap13
