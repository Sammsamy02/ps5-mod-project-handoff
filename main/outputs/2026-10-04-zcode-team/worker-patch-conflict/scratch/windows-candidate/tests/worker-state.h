#ifndef MB_OWNED_WORKER_FIXTURE_STATE_H
#define MB_OWNED_WORKER_FIXTURE_STATE_H
#define MB_WORKER_EXPECTED_EXIT 0x81234567U
#define MB_WORKER_PARAMETER_COOKIE 0x1122334455667788ULL
#define MB_WORKER_LAST_ERROR 0xe17395abU
#define MB_WORKER_CREATOR_ERROR 0xc2468aceU
/* Windows exports, invoked only after ACTIVE through the production typed
 * int32(pointer) native-to-Windows call route. DLL owns all asynchronous state.
 * Begin out: handle, thread ID, zero-wait result258, running exit status259.
 * Collect out: full32-bit exit, 64-bit cookie, worker last-error, repeat wait0.
 * The host-only MB_WORKER_CLOSE_EARLY variant closes its handle while gated;
 * Collect then reports no exit-code claim (out[0]=0,out[3]=0xffffffff), and
 * the native runner separately joins before retiring the code lease.
 * A successful Begin requires Collect even if a subsequent test/control fails.
 * An import failure after creation opens the gate and attempts CloseHandle;
 * out[0] on such failure is zero if closure succeeded, otherwise the retained
 * handle for native cleanup retry. Never call Collect after failed Begin. */
#endif
