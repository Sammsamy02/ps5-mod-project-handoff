# DIFF — the launch-11 marshal consumer (single revision, distinct IDs)

Worker: worker-l11-marshal-integration, 2026-10-05.

## The 3-way derivation (mirroring worker-luamod-rebase's discipline)

| role | artifact | sha256 |
|---|---|---|
| **B — the base** (= the dry-run's verified variant-B staging copy) | `worker-luamod-rebase/staging/luamod-l10rebase/LuaMod.cpp`, byte-verified copy at `work/base-LuaMod.cpp` | `4540593ac9e66cdc0a62e56736f05c05a23ca897881a902cae55e1d51bb79b25` |
| **C — the staged consumer** (B + this revision's single edit pair) | `staging/luamod-l11-marshal-consumer/LuaMod.cpp` | `04c9d91664dd82c7831cfc7eff79b6c20c62df9e6fd6eced60d419310966ad9f` |
| **D — the revision diff** | `diffs/l11-marshal-consumer.diff` | `f595c57fb8f3ca10458d4581974fa79532167c7449d3da5cbc5fa2a45117b44f` |

Derivation proof (the round-trip, `receipts/patch-roundtrip.txt`):

1. `patch -p1 --dry-run` on a fresh byte-verified copy of B: clean, no fuzz,
   no rejects.
2. `patch -p1` applied: the result is **byte-identical** to C (`cmp` clean;
   sha `04c9d916…`). Therefore `consumer(B, D) == C` exactly; nothing was
   hand-merged; no DIFF-1 byte was touched (the diff's context lines show the
   insertions sit strictly between DIFF-1's `arm_funcptr_marshal_drain` tail
   and `ensure_engine_tick_hooked`, and inside `on_program_start`'s prologue).
3. Object-level closure of the SAME discipline: compiling B under the frozen
   argv reproduces the dry-run's verified gate object `5595feb7…`
   byte-identically; compiling C under the same argv yields `854a476b…`.

## Content of the revision (2 hunks, one file — LuaMod.cpp only)

**Hunk 1 (LM:4735 area, +58 lines):** `r3_marshal_calibration_consumer()`,
file-static, wholly inside `#ifdef __PROSPERO__`, placed between DIFF-1's
`arm_funcptr_marshal_drain` and `ensure_engine_tick_hooked` so the whole
marshal cluster stays together. Gate → ranked data-side candidate walk →
DIFF-1's own arm. Full rationale: DESIGN.md §4. Bounded: no new includes, no
new literals beyond the log/candidate strings, no LuaMod.hpp / SettingsManager /
any-other-file change (the recompiled set stays exactly ONE unit — the rebase's
§5 property, preserved).

**Hunk 2 (LM:7255 area, +7 lines):** the guarded single call at the top of
`LuaMod::on_program_start()`, AFTER `AddUObjectDeleteListener` (a listener-list
append, cannot throw) and BEFORE `RegisterLoadMapPreCallback` (the function's
first eager detour arm). Ordering obligation of R3D §4.4a; see DESIGN.md §3.

## What the revision deliberately does NOT do

- Does not modify DIFF-1's or DIFF-2's bytes, signatures, comments, or the
  arm/disarm contract; the consumer only CALLS the declared surface.
- Does not touch SettingsManager.hpp, LuaMod.hpp, the initializer, the program,
  or any other unit: the recompiled set is exactly `luamod`.
- Does not add exec-page access of any kind: `StaticFindObject` (data-side) +
  DIFF-1's existing `RegisterPostHook` → `SetFuncPtr` heap-store route only.
- Does not resolve the R3 world's remaining eager registrations (the LuaMod-side
  LoadMap arm still throws under BOTH_FAIL after the consumer runs) — owned
  elsewhere, recorded in DESIGN.md §6 and RUNBOOK §6.
- Does not implement the in-game rate calibration (R3D §4.2 steps 2-4); it is
  the bounded ranked stand-in the DIFF-1 author's handoff scoped.
