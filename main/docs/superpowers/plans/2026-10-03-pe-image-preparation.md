# Windows DLL image preparation implementation plan

The user has resumed UE4SS/game-binding and DLL implementation and authorized
autonomous local experiments with focused checks. Existing source-built PRX
loading remains intact. This work implements a necessary Windows DLL loader
stage; it is not an executable mapper, Windows emulator or DLL compatibility
claim.

## Design and boundary

Add a portable C11 PE32+ AMD64 image-preparation component under
`outputs/ps5-mod-bridge/windows-image/`. It accepts immutable complete PE bytes
and caller-owned ordinary data storage. It validates headers/section spans,
copies headers and sections with zero-filled virtual tails, applies explicit
base relocations, enumerates/binds imports using a caller callback and exposes
entry/export RVAs as data. It never allocates executable storage, calls PE code,
changes page permissions, discovers a process, loads a system library or reads
game memory. No console contact or game/system writes occur.

The format reference is Microsoft's current PE/COFF specification:
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format . The real retained
UE4SS archive at `.superpowers/native-observations/nexus-real-mods-2026-10-01/ue4ss-v1.zip`
contains `dwmapi.dll` and `ue4ss/UE4SS.dll`; their original bytes are used unchanged
as integration inputs, not distributed in source handoffs. This avoids designing
only for an invented minimal DLL.

Required semantics: all RVA arithmetic is bounded; section overlap and invalid
spans fail; PE base relocation ABSOLUTE is skipped, DIR64 is applied, other types
are explicit unsupported errors. Import binding names/ordinals come from the
original immutable input, not a previously overwritten IAT. Missing imports are
reported exactly and never silently filled with no-op functions. Entry/TLS/
exceptions/delay-import/load-config presence remains visible to the future loader.
Prepared bytes are never labeled execution-ready. Input/output aliasing is refused.

## Tasks

- [x] Implement the preparation API and focused byte-level tests for actual
  section placement, virtual zero fill, positive/negative rebasing and named/
  ordinal import binding, plus malformed bounded cases. Run the test before the
  implementation to establish the missing behavior; use compiler-built PE DLL
  inputs where practical.
- [x] Run unchanged retained DLLs through preparation and compare image/relocation/
  import facts with an independent PE reader. Preserve exact unresolved imports
  and execution dependencies; do not substitute callable stand-ins as proof.
- [x] Compile the production C source with the retained PS5 SDK. Keep the existing
  native CppMod loader's Windows-PE rejection until a real execution backend exists.
- [x] Review the component and record compact reproducible results, source hashes
  and remaining execution requirements in the current checkpoint.

Root owns real-input/independent comparisons and checkpoint integration. The
implementation agent owns only this new component and its focused tests. Other
agents independently inspect native module enumeration semantics and concrete
Unreal call-ABI errors. No broad suite or speculative new framework is required.
