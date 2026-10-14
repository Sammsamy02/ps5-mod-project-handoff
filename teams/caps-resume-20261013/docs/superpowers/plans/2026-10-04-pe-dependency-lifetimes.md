# PE dependency calls and observed search imports

Continue the full DLL/runtime goal from the recorded checkpoint. Previous turn
made verified source changes and executed owned DLL fixtures; classify it as
progress. Target game/UE4SS execution remains unfinished and is not assumed.

1. Implement the four observed VCRUNTIME140 byte-search primitives, with exact
   binding and actual owned-DLL calls. Preserve all existing services.
2. Add an import owner that binds a consumer to caller-declared, already-loaded
   provider DLLs. Acquire real export leases during binding. Keep those leases
   until consumer cleanup succeeds, including failed-load/unload retries.
3. Execute a compiler-built provider/consumer DLL pair to prove real dependency
   calls and refusal to unload the provider while its consumer remains alive.
4. Record focused checks, review, the remaining engine gap and private source
   publication. Do not contact the console or repeat stopped native actions.

The dependency owner is generic native C code over `mb_pe_module`; it does not
discover files, resolve forwarded/data exports, emulate LoadLibrary/GetProcAddress,
handle cycles or invent dependency implementations. Registry entries explicitly
name provider DLLs; registered names take priority and cannot silently fall back
to a different service provider. System functions use the existing binder only
when no registered provider owns that library. Imports by ordinal are supported
through the existing executable-export lease API. No new per-game paths/settings.

All registry/module/lease storage is caller-owned and externally serialized.
Consumer entry functions obey the existing synchronous no-escaping-work contract.
This is a prerequisite for a complete loader, not proof of general DLL or game
compatibility. Windows allocation/TLS/exception services remain separate work.
