# pid 934 — the crash boot (2026-10-05 21:00)

First boot of the new candidate (twelve fixes) staged IN FSELF FORM (d6922807)
with the diagnostic loader (009445ab) — after the FSELF-form discovery closed
the loading regression.

## Proven live this boot (first-ever in-game proofs)
- Module LOADS: ploader `OK 1503ms` (FSELF form; prior failures were my
  bare-ELF/decrypted-view staging forms).
- module_start completes: runtime_start_ok +833ms, module_start_return 0;
  235 constructors individually guarded, import validation 0 missing.
- v5 publication policy LIVE: spanpub total 126→119 (image spans excluded),
  published 64, cap 1, envelope (0x11<<32)|0x3e0000 = 0x11003e0000.
- THE MOD LOOKUP FIX WORKS: "Mod is absent" — ZERO occurrences (pid 700 had
  it NUL-garbled). setup_mods widen + qualified modbridge:: calls proven.
- Pad deferral + settings mod-source unchanged-good.

## The crash
- Kernel dialog: PPSA33286 crashed 0xa002030a SYSTEM_ILLEGAL_FUNCTION_CALL.
- Window: UE4SS.log's last line = slot-3 gmalloc bound; sdiag's expected
  next artifact (pool_range_verdict_fresh, emitted only AFTER the syscall
  returns) never appears.
- Diagnosis (fold edit 13b): the raw `syscall` instruction is fatal inside
  the sandboxed game process. Linked trampoline disassembles correctly
  (run-l11-1 composed object) — not garbage execution; the instruction
  itself. Standalone-payload successes ran in homebrew context.
- Correction: mincore path REMOVED; copyin probe on NID-routed
  open/write/close/unlink (all in the 196-import contract) replaces it.
- Path-test popup "The case died while waiting" = loader source line 785:
  its readiness wait observing the game's death (causality: our crash).
