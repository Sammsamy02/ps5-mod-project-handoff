# Patch acceptance limits (design-only)

Data-only pointer swaps and instruction patches are separate capabilities. Existing UFunction Func swaps demonstrate dispatch through engine data; they do not demonstrate code-byte reading/writing, detours, the refused VMT pump or arbitrary protected pages.

Proposed data-only sequence, after capability 1 prerequisites and recovery review: establish a known original value on an owned test data object; define an expected transition and callback; independently verify the changed value and effect; restore the original and verify restoration; exit cleanly. Fresh artifacts/logs must identify exactly which object/path was admitted and which guards refused invalid targets. Three clean passes across separate restarts are required for any new kernel-mediated rung. A single working callback is insufficient.

No game-text patch step is authorized. PHASEB-PLAN main-body aarch64/cache-flush sketch is superseded by its GP appendix (x86-64). Its write-channel availability is not a proven safe write contract. Neither fact overrides CLAUDE.md prohibition on game code patching/detours. New kernel-resident code and modified resident payloads remain outside this prep. No executable, payload, memory address, patch bytes or exploit implementation is supplied.

Receipt gaps: exact service ABI and source/binary pin, owned-memory boundaries, expected original/restore values, failure handling, pre-proven recovery for the proposed write risk and per-restart evidence. Console scheduling follows the human's #1+#3 slot, not this document.
