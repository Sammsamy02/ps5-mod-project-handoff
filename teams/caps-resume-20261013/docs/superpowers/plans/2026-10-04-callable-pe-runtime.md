# Callable PE runtime slice

Continue the user's authorized reusable DLL/runtime work from the verified PE
preparation component. This slice tests real cross-ABI calls from a compiler-built
Windows DLL on the local x86_64 host. It does not run downloaded DLLs or contact
the console. Keep game/build facts in profiles and production code generic.

Implement a small actual CRT service set with Microsoft x64 calling convention,
using the same import identities observed in the original UE4SS DLLs. Unknown
imports stay unresolved. Implement a portable module owner using explicit caller
allocation/protection/cache/release operations, refusing all runtime features not
yet implemented. The initial subset is /noentry, no TLS, no exceptions, and only
ordinary exports/imports/relocations plus inert resource/debug/security metadata.
Page protections must never combine writable/executable requirements; active call
leases prevent release while code is in use. Preserve failed release ownership.

Root compiles an owned fixture with real imports and a relocated data pointer,
then calls its exports through the module owner using Microsoft ABI function
pointers. Test observable output and arguments in registers and stack, including
floating point arguments. Check unload while leased and actual release after the
call. Compile the production service/owner sources for PS5 separately; host
execution must not be reported as PS5 execution or arbitrary DLL compatibility.

Parallel engine work may only use justified retained source facts. Native runtime
construction is already connected to supplied context; the real game context,
matching Unreal layouts and Tick callback remain required. Do not repeat prior
refusal-only audits, fabricate layout/provider data or reroute previously stopped
native game-memory/mapbase/placement operations.

- [x] Implement callable CRT service subset and focused semantics tests.
- [x] Implement portable restricted module owner and focused lifetime/failure tests.
- [x] Execute owned compiler-built PE through both production components locally.
- [x] Compile production components with the PS5 SDK, review meaningful changes,
  save current evidence and publish coherent source privately.
