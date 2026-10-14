# DLL startup, native clocks and Lua notification dispatch

The owned DLL candidate now executes actual DLL startup/shutdown locally, and a
fresh SDK ELF is compiled. The native clock service supplies two additional
observed Windows imports as an independent checked binder. A UE4SS notification
bug was reproduced against a real Lua VM and fixed in the coherent native build.
PS5 DLL execution and Townfall engine calls remain unverified.

## DLL startup

The opt-in entry profile uses the production PE owner and typed Win64 entry
bridge. Both owned DLLs attach once, require their DllMain-created heap/TLS state
before exports can run, return 86/93, and detach once with imports/thread services
alive. Failed final release retains the original native thread admission and
never reruns detach. Rejected attach executes detach without exports and
preserves ENTRY_REJECTED through rollback retries. Default no-entry controls
retain their original behavior. Final independent review is clear.

The new 202,152-byte SDK ELF has SHA-256
`5931b705bec85e30035e21532d6f436d716d7d925d58af014496a4e62137a049`.
See the sibling runtime-entry-candidate report for source-bound receipts.
This is not PRX acceptance or support for static TLS, CRT startup, unwind,
entry-created threads, reentrant loader calls or arbitrary downloaded mods.

The earlier 20cd DLL ELF already supplied to the user is unchanged. Its target
result is pending; no load notification or absence of a crash is counted as
execution. No console was contacted or modified during this slice.

## Native clock services

The separate clock binder implements exact QueryPerformanceCounter/Frequency
imports using the real native monotonic clock and existing admitted-thread
last-error storage. Counts have nanosecond representation and frequency 1e9;
that is a scale, not a claim of hardware timing resolution. Failure preserves
output and records an explicit adapter error. Sessions pin their domain until
module cleanup succeeds. Host Win64 calls, failure controls and sanitizers pass;
provider/bridge objects compile with the SDK and match layout/provider evidence.
These services are not yet composed into the entry candidate. See CLOCK.md in
the Windows services source for API/provider evidence and remaining limits.

## UE4SS notification dispatch

Two queued objects for one cancelling callback caused two calls and two unrefs
of the same coroutine. The real Lua registry then returned the same reference
for two simultaneously live allocations. The four-line source delta skips
callback IDs already cancelled in that batch. The real-VM regression now shows
one invocation/unref, two independent new references and preserved values.
False/non-boolean callbacks and stale-object admission keep their behavior.

All four scenarios pass under ASan/UBSan. Engine object lookup/wrapping are
explicit host seams; this is genuine registry/dispatch evidence, not a game
execution. The native composition freshly replaces only LuaMod in 359 runtime
units and retains the initializer retry/cancellation, Tick affinity, activation,
controller input and native C++ integration. Imports remain 197 unchanged.
The 116,577,288-byte relocatable object has SHA-256
`b4760cbd5f3931358042bc9a8d82cf6483eab6d056b303cb406696cba75d2b67`.
Original source and the frozen console-proven PRX remain untouched.

Actual Item Spawner still requires genuine native loader facts, matching target
layouts and delivered game-thread engine calls. These changes do not supply
those missing inputs or prove universal mod support.
