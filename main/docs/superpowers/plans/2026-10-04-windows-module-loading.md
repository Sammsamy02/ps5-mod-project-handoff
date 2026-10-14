# Windows DLL module loading

Continue the user's authorized runtime work before app polish. The bounded
milestone is an actual Windows DLL loading another DLL from a file through
LoadLibrary, looking up a function through GetProcAddress and calling it with
the existing heap/TLS services. Host execution and target object compilation
remain separate from actual PS5 execution.

1. Extend the existing validated PE walker with a public import visitor. Validate
   the entire supported image before callbacks; retain exact stopping diagnostics.
2. Add the native loader owner over PE link/dependency leases: lazy catalog
   acquisition, recursive imports, explicit load references, image-base handles,
   retained cleanup and procedure leases. Reject cycles and unsupported paths.
3. Supply regular files from installer-owned profile paths as bounded immutable
   snapshots, keeping source ownership through final module retirement.
4. Compose typed Windows LoadLibrary/GetModuleHandle/GetProcAddress/FreeLibrary
   thunks with existing heap/TLS binders. New module sessions may start while
   unrelated DLL calls run; installed bridge config is immutable while any
   session is active. Keep loader and thread/heap mutex ownership separate.
5. Execute a compiled caller and disk-acquired helper locally; verify returned
   values, service calls, reference/dependency lifetimes and cleanup. Compile the
   actual thunks through the existing unedited Win64 IR target route. Use focused
   tests and an independent review of ownership and API contracts.
6. Save source-bound receipts and publish only this coherent source delta to the
   private handoff branch. Preserve all unrelated dirty files and frozen captures.

Root owns import visitation, file intake, affected existing fixture regressions
and integration. win_services owns the native loader and its checks.
pe_call_lifecycle owns the Windows bridge, concurrency fix and compiled DLL
fixture. ue_binding_next reviews public contracts and final implementations.

Remaining limits: no main-executable lookup, static PE TLS/TEB/FLS, exceptions,
forwarders, arbitrary DLL search paths or reentrant DllMain. Target executable
memory and game/provider integration must be genuine before a PS5 playtest.
UE4SS Item Spawner still requires matching engine layouts and real game-thread
calls; this plan does not count startup or pure Lua as game success. No console
contact or game/system writes in this slice, and no rerouting prior stopped
native probing/placement actions.
