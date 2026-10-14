# Stage 24: guarded PE preparation and activation

**Goal:** Let a validated PE with real static TLS or possible GS code be mapped only through a path that requires guest-segment entry for lifecycle and export calls. Keep the public `pe_load` refusal unchanged.

**Architecture:** Add an internal mapped-image preparation hook after relocation and import resolution but before page permissions. The guarded loader uses it to register mapped static TLS and validate each callback against file-backed executable section ranges. Return an opaque module without public raw entry or export pointers. Its lifecycle and integer export methods refresh the guest TEB, enter the segment backend, bind last-error state, invoke the Win64 bridge, and leave the backend. Any refusal returns a named status before guest code executes.

**Scope limits:** This stage does not implement a PS5 segment switch, PS5 executable-memory backend, Windows CRT/UE4SS imports, mixed floating-point ABI, or target correspondence. It cannot prove an unchanged gameplay DLL works. No shared candidate or original DLL is changed.

## Tasks

1. Add a focused host test that fails to compile before the guarded loader API exists. Verify strict loader refusal of a real TLS sample remains, guarded mapping prepares its TLS bank slot, and `UnavailableSegmentSwitch` refuses attach without invoking guest code.
2. Add an internal preparation seam to the mapper and a guarded module API. Verify the focused test and existing PE preflight tests.
3. Extend the test with safe synthetic Win64 entry/export code and a fake segment backend. Verify attach, export entry gating, wrong-bank refusal, and segment leave on every completed call.
4. Run related regressions, read-only unchanged-DLL source hashing, exact-base patch replay, and package the incremental report, manifest, and evidence.

## Review focus

- Callback target is inside image but outside file-backed executable code: refuse before TLS index write.
- TLS directory malformed after relocation: refuse before publication.
- Backend refuses segment entry: no callback, DllMain, or export runs.
- Export called before process attachment or after detach: refuse.
- Wrong thread or TLS bank: refuse before segment entry.
