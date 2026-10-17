# Windows worker services

The native worker owner and Win64 binder implement CreateThread, WaitForSingleObject, GetExitCodeThread, CloseHandle and Sleep for owned thread handles. Callers must already enter the shared Windows thread domain. Enable creation around ACTIVE PE exported work; suspend/restore admission around every DllMain, including nested loader callbacks. Default stack, NULL security attributes and zero creation flags are supported; other options fail explicitly.

Acquire the start routine's stationary executable code lease before scheduling. The routine can be internal rather than exported. The callback owner serializes PE access independently; code acquire/release callbacks run outside the worker mutex. Closing a Windows handle does not cancel the thread or release its code. Call collect after completion to join and retire the lease; failures retain the same obligation for retry. Retire every handle, worker, admission and session before closing the owner/domain.

The ABI translation unit keeps exact Microsoft x64 signatures, including six CreateThread parameters and the DWORD(void*) callback. It is lowered through unedited win64cc LLVM IR using the PS5 SDK backend. Native pthread functions, monotonic time and errno constants come from the SDK's native headers. No Windows TEB, static PE TLS, loader thread notifications, APCs, cancellation, exceptions or asynchronous DllMain behavior are supplied.

Focused verification: verify_workers.py (real pthread/Win64, failure and sanitizer controls); windows-execution/verify_code_lease.py (internal code lifetime); windows-candidate/tests/worker-verify.py (actual PE instructions); windows-candidate/build.py --entry --clock --workers (full owned host and SDK composition). All are local and never dispatch a payload.

See outputs/2026-10-04-dll-worker-runtime/README.md for current evidence and target limits.
