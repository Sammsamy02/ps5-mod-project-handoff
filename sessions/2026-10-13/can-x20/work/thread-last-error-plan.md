# Win32 per-thread last-error imports

Stage `cg-thread-last-error-20261013-18`, copied from isolated stage 17. Shared candidate and original DLLs remain untouched.

All five unchanged DLL samples statically import `GetLastError`; one also imports `SetLastError`. Add exact zero/one argument rows and one process-wide, per-host-thread DWORD error slot. A successful `SetLastError` must be visible through `GetLastError` from any shim on the same thread, must not leak to another thread, and must survive a call such as `Sleep` that is not documented to set the error. Initial value is zero. Leave dynamic TLS and synchronization calls unreviewed until their own error semantics are complete.

Add failing host checks first, then implement, run prior regressions, and package an exact-base increment. Document that direct guest TEB `LastErrorValue` coherence is still absent; this step only implements import-call semantics and cannot establish full Windows thread environment behavior.
