# Reviewer corrections — apply to all #1/#3/#13 work (2026-10-13)

## Labeling corrections (effective immediately)

1. KA's import analysis = "dependency coverage" NOT "measured compatibility"
   until those DLLs actually execute correctly on target.

2. KF's Mac E2E = "host behavior" NOT "PS5 execution" — record:
   - Mac architecture (ARM64? x86_64? Rosetta?)
   - Translation environment used
   - ABI/runtime mechanisms
   - What passed vs what is PS5-untested

3. #1's new kernel sub-op = "requires stated authorization" — the
   feasibility is established, the authorization is NOT.

4. #3's self-patch test = "probe-process behavior" — game-process
   behavior is a separate integration question, not implied.

## Scope clarity

- "Automatic loading of unchanged DLL files through a generic loader
  across games" remains the requirement
- A game-specific mod can depend on classes/assets absent from another
  game — the loader can't guarantee those dependencies exist
- Distinguish: loader portability (our job) vs mod-game dependency
  compatibility (the mod's problem)

## What the E2E proof must record (KF)

- Architecture and any translation
- Actual loader and shim code path
- Initialization sequence (DllMain → CRT → constructors)
- Import resolution (which succeeded, which refused)
- Callbacks invoked
- Observable behavior (return values, side effects)
- What is host-only vs what would need PS5 verification
