# Stage 26: guarded mapped-module registry

**Goal:** Let later DLLs resolve validated named exports from an already attached guarded PE module through the reviewed shim, using its source path rather than per-game configuration.

**Architecture:** Add a collision-refusing mapped-module entry to `WinApiShim` keyed by a lowercase basename derived from slash or backslash paths. It stores a lookup callback and owner context, and removes only an exact matching registration. The guarded module reserves its name before DllMain, but the callback refuses export lookup until process attach succeeds. Attachment failure and clean detach remove the reservation. The existing manual module registry continues to work.

**Scope:** Named, file-backed executable exports only. Ordinals, forwarders, dynamic `GetProcAddress`/`LoadLibrary`, cross-module lifecycle ordering, Windows Unicode case folding, and actual PS5 execution remain separate work. No shared candidate or original DLL is edited.

## Tasks and tests

1. Write a failing host test using the pinned synthetic fixture: `C:\\mods\\Fixture.DLL` reserves `fixture.dll`, but its `start_mod` export is absent before attach and present afterward. Assert exact pointer range behavior through a guarded host invocation, not raw execution.
2. Test duplicate basename refusal, including a manually registered module, without replacing the existing registry entry. Test cleanup after backend-refused attach and after clean detach; a later load with the same basename may then attach.
3. Implement shim registration/unregistration and guarded loader integration. Keep the existing unnamed preparation API and existing module tests working.
4. Run focused and full host regressions, verify original DLL hashes, scratch replay against exact stage 25, and package report, patch, manifest, and logs.

## Review focus

- No export pointer is published before successful process attachment.
- Duplicate normalized basename never replaces an existing entry.
- Unregistration checks both name and owner handle/context.
- Failed attachment removes policy and module reservation.
- All guest calls remain behind the segment gate; registry lookup alone never invokes guest code.
