// dllmain_invoker.h -- entry-point invocation for the mapped PE image.
// See dllmain_invoker.cpp for the policy (design section 3f).
#ifndef CAP13_DLLMAIN_INVOKER_H
#define CAP13_DLLMAIN_INVOKER_H

#include <cstdint>

#include "pe_loader.h"

namespace cap13 {

enum class EntryStatus
{
    Ok,
    NoEntryPoint,  // entry_rva == 0: legal for some DLLs; log, don't fail
    ReturnedFalse, // DllMain returned FALSE at attach -> treat as refusal
    BridgeRefused  // signature outside the bridge envelope (mixed float)
};

EntryStatus invoke_dllmain_attach(const LoadResult& mod, uint64_t& out_entry_host);
void invoke_dllmain_detach_quiet(const LoadResult& mod);

} // namespace cap13

#endif // CAP13_DLLMAIN_INVOKER_H
