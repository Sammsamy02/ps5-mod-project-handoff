#include <cstdint>
#include <stdint.h>
#include "StrictNativeThreadPolicy.hpp"
// Exact declaration copied from saved public OpenOrbis header line573.
// That PS4 community header is corroboration; PS5 ABI is correlated separately
// to the authenticated native NID and EAX32 consumers, not official SDK prose.
extern "C" int32_t scePthreadGetthreadid(void);
namespace NativeThreadAudit {
bool read_current_native_id(void*,std::int32_t& id) {id=scePthreadGetthreadid();return id>0;}
Admission typed_policy_adapter(StrictThreadPolicy& policy,void* snapshot_context,
                             bool(*snapshot)(void*,EngineSample&)) {
    return policy.admit({snapshot_context,snapshot,read_current_native_id});
}
}
