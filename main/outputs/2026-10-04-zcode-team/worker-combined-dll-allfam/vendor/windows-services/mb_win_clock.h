#ifndef MB_WIN_CLOCK_H
#define MB_WIN_CLOCK_H
#include "mb_win_threads.h"
#include "../windows-execution/pe_module.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MB_WIN_CLOCK_FREQUENCY INT64_C(1000000000)
#define MB_WIN_CLOCK_ERROR_NATIVE UINT32_C(31)
#define MB_WIN_CLOCK_ERROR_RANGE UINT32_C(534)
typedef enum mb_win_clock_status {
    MB_WIN_CLOCK_OK = 0, MB_WIN_CLOCK_INVALID_ARGUMENT, MB_WIN_CLOCK_BAD_STATE,
    MB_WIN_CLOCK_BUSY, MB_WIN_CLOCK_OWNER_ERROR, MB_WIN_CLOCK_NATIVE_ERROR,
    MB_WIN_CLOCK_RANGE_ERROR
} mb_win_clock_status;
typedef struct mb_win_clock_session {
    mb_win_thread_pin pin;
    mb_pe_module *module;
    struct mb_win_clock_session *next;
    uint8_t active;
} mb_win_clock_session;

/* Checked native C helpers. CLOCK_MONOTONIC seconds/nanoseconds are converted
 * into nonnegative int64 nanosecond counts with overflow checks. Frequency
 * verifies that same native provider and returns the fixed count scale 1e9.
 * Scale is not a claim of nanosecond hardware resolution or accuracy. A native
 * failure or invalid/out-of-range clock sample leaves output unchanged. There
 * is no substitute wall clock, fabricated timestamp, or cached success value.
 * Native helpers do not use or modify a Windows error slot.
 */
mb_win_clock_status mb_win_clock_read_counter(int64_t *);
mb_win_clock_status mb_win_clock_read_frequency(int64_t *);

/* Native interface with two private Microsoft x64 BOOL(LARGE_INTEGER*) thunks:
 * KERNEL32.dll!QueryPerformanceCounter and QueryPerformanceFrequency only.
 * BOOL is int32, LARGE_INTEGER storage is int64, and pointers are 64 bits.
 * Library names use ASCII case folding, function names match exactly. Ordinals
 * and all other names remain UNRESOLVED; only BOUND writes the address output.
 *
 * Zero-initialize sessions and keep session/domain/module objects stationary
 * and disjoint until end succeeds. begin requires an EMPTY module and pins the
 * existing thread domain. One domain is installed while sessions live. end
 * requires the original session and module EMPTY; BUSY/failed end retains the
 * domain pin through module CLEANUP. Binding requires module LOADING or ACTIVE.
 * Code and the native clock provider must outlive all sessions and calls.
 *
 * Controls/binding are externally serialized. First installation and final
 * retirement require all imported calls quiescent; each retiring module must
 * have no outstanding calls and be EMPTY. Adding/retiring another module with
 * the same installed domain may coexist with unrelated calls. Each calling
 * thread must successfully enter that domain before executing DLL code and
 * leave after every call/borrowed-pointer use ends. Domain controls supply their
 * own locks; ordinary clock calls need no external domain lock. Session controls
 * may be invoked by serialized loader service hooks. No exception/longjmp or
 * callback may escape a call; null output pointers violate the Windows call's
 * valid-parameter contract (defensively return zero with last-error 87).
 *
 * On native failure the thunks return BOOL 0, preserve the int64 output and set
 * the current admitted thread's real Windows last-error slot to 31; range or
 * malformed-sample failure uses 534. These numeric errors are this adapter's
 * explicit policy, not promises about every Windows implementation. Success
 * returns BOOL 1 and preserves Windows last-error. CRT errno is independent.
 * This separate binder is not automatically composed into any existing binder
 * or candidate. File-time and thread/process identity imports stay unsupported.
 */
mb_win_clock_status mb_win_clock_begin(mb_win_clock_session *, mb_win_thread_domain *, mb_pe_module *);
mb_win_clock_status mb_win_clock_end(mb_win_clock_session *);
mb_pe_bind_result mb_win_clock_bind(void *, const mb_pe_import *, uint64_t *);
#ifdef __cplusplus
}
#endif
#endif
