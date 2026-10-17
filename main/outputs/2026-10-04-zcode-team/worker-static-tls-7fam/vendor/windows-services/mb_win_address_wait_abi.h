#ifndef MB_WIN_ADDRESS_WAIT_ABI_H
#define MB_WIN_ADDRESS_WAIT_ABI_H
#include "mb_win_address_wait.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_address_wait_session {
    mb_win_address_wait_pin pin;
    mb_pe_module *module;
    struct mb_win_address_wait_session *next;
    uint8_t active;
} mb_win_address_wait_session;
/* Native C control API; private thunks have exact Microsoft x64 signatures:
 * BOOL WaitOnAddress(volatile void*,void*,SIZE_T,DWORD), void Wake...(void*).
 * Only api-ms-win-core-synch-l1-2-0.dll and exact three names are bound, library
 * ASCII case-insensitive, function names case-sensitive. Other DLLs/ordinals
 * remain UNRESOLVED; invalid session/import/output is ERROR without fallback.
 *
 * Zero-init stationary disjoint session/module/owner. begin before EMPTY module
 * load; end only after module EMPTY and every call through it retired. The exact
 * original session pins the owner throughout ACTIVE/CLEANUP. Bind requires
 * LOADING or ACTIVE. All controls/bind are externally serialized. First install
 * and last retirement require all calls quiescent. Intermediate same-owner
 * module sessions may change under a loader control mutex while unrelated
 * imported calls run; installed owner remains immutable while any session lives.
 * Every executing module must keep its own session, and calls must retain their
 * PE code/buffer owners independently (e.g. worker code leases). This binder
 * supplies no automatic composition into an existing runtime binder.
 */
mb_win_address_wait_status mb_win_address_wait_begin(mb_win_address_wait_session *,mb_win_address_wait *,mb_pe_module *);
mb_win_address_wait_status mb_win_address_wait_end(mb_win_address_wait_session *);
mb_pe_bind_result mb_win_address_wait_bind(void *,const mb_pe_import *,uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
