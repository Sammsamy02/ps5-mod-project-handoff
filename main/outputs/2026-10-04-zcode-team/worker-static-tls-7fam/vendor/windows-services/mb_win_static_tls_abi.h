#ifndef MB_WIN_STATIC_TLS_ABI_H
#define MB_WIN_STATIC_TLS_ABI_H
/* VENDORED ADAPTED COPY (origin: pattern of outputs/ps5-mod-bridge/
 * windows-services/mb_win_init_once_abi.h; that original file is unchanged). */
#include "mb_win_static_tls.h"
#include "../windows-execution/pe_module.h" /* allfam include-path adaptation (worker-combined-dll-allfam precedent) */
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mb_win_static_tls_session {
    mb_win_static_tls_pin pin;
    mb_pe_module *module;
    struct mb_win_static_tls_session *next;
    uint8_t active;
} mb_win_static_tls_session;
/* Original stationary zeroed sessions; begin before module EMPTY->LOADING,
 * end after EMPTY and all its static-TLS records unregister. First/final
 * controls require global call quiescence; other controls/binding are
 * externally serialized. One immutable installed owner while sessions exist.
 *
 * Bound names (ASCII exact, ANY library spelling, ordinals unresolved):
 * these are MSVC/PE static-TLS plumbing names, module-local data or adapter
 * surface - none is a KERNEL32 export, so no library filter is applied.
 *   __tls_index / _tls_index  -> address of the registered module's
 *                                AddressOfIndex DWORD (IAT receives the
 *                                address; the loader wrote the live index).
 *   __tls_start / _tls_start  -> resolved host template start.
 *   __tls_end   / _tls_end    -> resolved host template end.
 *   __tls_import / _imp___tls_import -> WIN64 adapter thunk (below).
 * "__tls_import" is the controller-backlog name surfaced by this task: no
 * census import row uses it and it is NOT a Microsoft declaration. The
 * adapter thunk has this family's documented signature:
 *   void *mb_win_static_tls_import(uint32_t index, uint64_t offset)
 * returning the calling thread's block for a pool index plus the template
 * offset - the exact computation of the MSVC-generated access sequence
 * (tls_array[index] + offset) - for composition shims that cannot inline a
 * TEB read. NULL for an unallocated/never-published index; never
 * allocates. "__tls_array" stays UNRESOLVED: it is a gs-relative TEB
 * offset, not bindable data. Unknown names are UNRESOLVED so the dynamic
 * TLS bridge and other owners still answer. Invalid session/view -> ERROR;
 * only BOUND writes the output address.
 */
mb_win_static_tls_status mb_win_static_tls_begin_session(mb_win_static_tls_session *,
    mb_win_static_tls *, mb_pe_module *);
mb_win_static_tls_status mb_win_static_tls_end_session(mb_win_static_tls_session *);
mb_pe_bind_result mb_win_static_tls_bind(void *, const mb_pe_import *, uint64_t *);
/* Adapter thunk bound for __tls_import (see contract above). */
void *mb_win_static_tls_import(uint32_t index, uint64_t offset);
#ifdef __cplusplus
}
#endif
#endif
