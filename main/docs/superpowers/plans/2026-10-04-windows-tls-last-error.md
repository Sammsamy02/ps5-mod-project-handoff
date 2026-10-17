# Windows dynamic TLS and last-error services

The previous goal turn completed actual local DLL heap/errno execution and
published its source at `8a5fa4bea334174f265ca6bfb9c780b771d1e3ab`.
This turn continues the retained DLL's observed `KERNEL32.dll` imports:
`GetLastError`, `SetLastError`, `TlsAlloc`, `TlsFree`, `TlsGetValue`, `TlsSetValue`.
The full UE4SS/DLL goal remains active and incomplete.

Extend the existing native thread domain, keeping Windows errno, Windows
last-error and host errno separate. Use real registered native threads and
serialized process-wide slot allocation. Every new TLS index begins NULL for
existing and future threads; reuse cannot reveal a previous allocation's value.
Do not free the application-owned object stored in a TLS slot. Preserve the
documented last-error behavior for each API, including TlsGetValue's successful
clear. The bounded slot table is an explicit runtime limit, not a claim about
all Windows versions. Unregistered native calls violate the entry contract.

Module sessions pin the thread domain through PE lifecycle and cleanup retries.
Private import entry points preserve the Microsoft x64 ABI; public control
functions remain native C. Compose the new exact bindings with the existing
heap/stateless binder so one DLL can use all services. Control changes require
quiescent imported calls; native thread workers retain their own registrations.

Work in parallel: native owner plus real pthread behavior, Win64 bridge and
owned DLL integration, independent review against primary Microsoft contracts.
Use focused failing tests before implementation, then compile target objects
and rerun the affected old errno/heap tests once the shared source is stable.
The owned DLL must exercise every new import with actual native threads,
last-error isolation, TLS allocation/free/reuse and retained module ownership.

Do not conflate these explicit Tls APIs with PE static TLS/TEB setup, TLS
callbacks, Windows thread creation, FLS/fibers, exceptions or engine bindings.
The retained Windows UE4SS image has a nonzero static TLS directory; that remains
a separate unfinished requirement. No console contact or game/system writes
are part of this slice, and no host result counts as PS5 execution.
