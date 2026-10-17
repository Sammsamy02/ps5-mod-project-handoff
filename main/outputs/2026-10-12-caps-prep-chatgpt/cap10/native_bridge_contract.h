#ifndef MODBRIDGE_CAP10_CONTRACT_H
#define MODBRIDGE_CAP10_CONTRACT_H
/* SOURCE-ONLY CONTRACT. Staged definitions are uncompiled/unapplied;
   no managed host, public export or PInvoke resolution is established.
   No C++ objects, strings, engine pointers or ownership cross this ABI. */
#include <stdint.h>
#ifdef __cplusplus
#define MODBRIDGE_CAP10_NOEXCEPT noexcept
extern "C" {
#else
#define MODBRIDGE_CAP10_NOEXCEPT
#endif
/* Exactly 1 for this bridge contract; not a runtime/build version. */
int32_t modbridge_cap10_abi_version(void) MODBRIDGE_CAP10_NOEXCEPT;
/* Caller supplies valid caller-owned writable int32 storage for the complete
   synchronous call. Only null is checked; arbitrary addresses are NOT validated.
   Admission exists only on the genuine on_engine_initialized callback thread,
   within its RAII window, while actual program state is started and no shutdown
   is requested. No asynchronous/nested managed host invocation is admitted.
   Success: return 0 and echo cookie. Failures leave reply unchanged:
   -3 absent scope/shutdown/not started; -4 null reply; -5 unexpected C++ exception.
   Both C boundaries prevent C++ exceptions escaping. Provider module must stay
   loaded; future resolver must bind exact names/addresses with owned lifetime. */
int32_t modbridge_cap10_runtime_probe(int32_t cookie, int32_t* reply) MODBRIDGE_CAP10_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef MODBRIDGE_CAP10_NOEXCEPT
#endif
