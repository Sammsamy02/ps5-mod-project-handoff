# REPORT — worker-launch10-acceptance-criteria (limits, patterns, unpinned items)

Worker: worker-launch10-acceptance-criteria, 2026-10-05. LOCAL ONLY: no console contact, no
executions, no nested subagents, nothing outside this worker directory modified. Deliverables:
`ORACLE.md` (state table + verdicts), `CLAIM.json` (83 predictions, JSON-validated), this
REPORT. The oracle is written against the STAGED sources read directly; it is evidence-shape
prediction, not an outcome claim.

## 1. What was read and pinned directly (verbatim-capable)

- `worker-launch10-readpath-guard/staging/UnrealInitializer.cpp` — all five
  `consume_required_slot_seeds` phases, arming gate, every `Output::send` and `seed_refused`
  format string, vtable-walk lines, PostInitialize lines (line-pinned throughout).
- `worker-launch10-readpath-guard/staging/SeedValidation.hpp` / `SeedTable.h` — all pure
  failure strings, class names, labels; runtime addresses derived from the uniform slide
  runtime = file + 0x400000 (slot 0 = 0x97ba110, GMalloc = 0x970add8 -> instance 0x97089b8,
  vtable 0x82c26e0, pool 0x9765ec0, ctor 0x63d590, SCO 0x890aa0).
- `worker-facts-geometry-fix/staging/runtime_start_bridge_r13.geometrydiag.cpp` — every
  `diagnostic_event` tag, both conversion paths, `copy_detail` strings, MRS codes.
- `worker-lease-coverage-fix/runtime_start_bridge_r14.cpp` — L1–L5 gate tags and the
  `lease coverage gap: requested [` detail string (marked CONDITIONAL: r14 ships only if it
  landed compile-clean; the worktree r14 copy does NOT contain the lmf_extent* events — the
  launch-10 rebuild must merge geometrydiag + r14; the oracle lists each tag's owning delta).
- `worker-module-start-wiring/runtime_module_entry.c` (+ `runtime_module_entry_r4.c`
  freshness-prefix variant) and `runtime_start_bridge.h` — entry prologue events, failure
  line format, MRS codes.
- `worker-minimal-path-test/src/path_test_entry.c` + generated `pt_config.h` — the two
  terminator lines, fallback path.
- `worker-f-r2-fix/composition/vendored/modforge_activation.c` — status grammar verbatim
  (header/session/state lines, O_TRUNC semantics, 512-B line cap, sanitization).
- `worker-runtime-ack/host-test/apply/.../runtime_owner.cpp` — the five state emit strings
  and their codes.
- `worker-geometry-reference/GEOMETRY.md`, `worker-anchor-cap-analysis/REPORT.md`,
  `worker-injection-latency/LATENCY-ANALYSIS.md`, `worker-final-ini-layout/FINAL-INI-LAYOUT.md`,
  `worker-diagnostic-sink/REPORT.md`+`ANALYSIS.md`, `worker-loadlist-final/final-package/loadlist.txt`,
  controller `RETURN-SESSION-COMMANDS.md` — span facts, ploader cap, ini order, sink
  decision, loadlist head bytes.

## 2. Lines marked as PATTERNS (runtime values fill them; not hard gates)

- Every line containing an address of a runtime object (Kismet/CDO/vtable-walk `address
  0x<…>`, `[n: <…>]` ids) except the five `[Seeds]` bind lines and the
  `Post-initialization: GMalloc` line, whose values are DERIVED constants (derived from
  SeedTable.h labels + base 0x400000, not runtime guesses).
- `mfa_activated_low/high`: preloaded byte count ≈ 35,890 + loadlist overhead — plausible
  range only.
- `bridge_vmentry_entries`: 0 < v ≤ 16384; boots observed 9,966–22,964 records; the count
  is boot-dependent.
- `bridge_kve_*`, `bridge_exepath_sig`, `entry_incoming_sp_mod16`, `entry_call_sp_mod16`,
  `constructor_count`: shapes only.
- Build-configuration/Stats lines (F5): settings-dependent, one-of set given.
- `Using engine version: <maj>.<min>`: profile-named 5.6 is plausible, not pinned in this
  workspace.
- PATH_TEST ns stamp (~1.79e18 shape), `[t=<ms>ms pid=<pid>]` values, status `session
  <pid> <ms>` values.
- The null rendering in `GameEngine::Tick address (vtable: 0x33ac8c0; scan: …)` — see §3.

## 3. Things I could NOT pin, stated as such

1. **`scan:` null rendering in F29.** The armed build leaves `TickInternal` not-ready, so
   `scan_address` is null (`UI:1798-1800` comment); how fmt renders that null pointer
   (`0x0` vs `0` vs empty) is unpinned. Consequence documented: the controller CP2 pin
   `scan: 0x33ac8c0` (`RETURN-SESSION-COMMANDS.md:56`) can only be true if a tick scan arm
   ran — on the armed build that is a composition-identity flag, not automatically a
   failure. The controller should decide at gate time whether CP2 needs an armed-build
   re-pin.
2. **`EngineTickResolveMethod`** (which of `Scan failed, falling back to vtable address…`
   vs `Using vtable address for GameEngine::Tick` fires) — lives in the v3 settings bundle,
   not read here. Both variants listed.
3. **`SecondsToScanBeforeGivingUp`** (the pool-retry budget, `UI:1008`) — v3 settings
   value not read; the "interned ids still zero after the wait budget" refusal depends on
   it.
4. **The exact composition constructor count** (`constructor_count`) and the exact
   `mfa_activated` byte split — not derivable from the sources read.
5. **Program-side UE4SS lines** (banner block, `Setting up mods...`, `start_mods<LuaMod>`,
   `[Lua] ` prefix, LuaMod error strings) are pinned INDIRECTLY through
   `worker-first-run-playbook` into UE4SSProgram.cpp/LuaMod.cpp line numbers; the playbook
   itself flags an entry-sha reconciliation item. Marked `indirect: true` in CLAIM.json.
6. **kMaxAnchorMappings cap state.** The worktree header still reads 8
   (`tick-file-owner-1/include/live_module_facts.hpp:88`); the controller directed the 8→16
   fold per worker-anchor-cap-analysis. The oracle gives BOTH interpretation tables and
   requires confirming the folded cap at the composition gate. With cap 8, `anchor_count`
   saturates at 8 and 9+ fragments are unreadable (silent-truncation signature pinned from
   CAP-ANALYSIS:53-62).
7. **Whether the supplier-diag.jsonl sink shipped** — controller decision was opportunistic
   ("in time + compile-gated → include; else proceed"); the oracle documents both channels
   and prefers the jsonl when present.
8. **Whether r14 lease hardening is in the launch-10 rebuild** — per task scope it is
   optional-if-compile-clean. All r14-only tags/rows are marked CONDITIONAL; the
   `Owned read range includes unregistered bytes` throw (C13) is DORMANT in both cases and
   its recurrence is REGRESSION + reconcile, capping the verdict at MINIMAL-ACCEPT (it does
   not by itself negate the geometry claim, but it IS a launch-9 repeat, not new info).
9. **fmt pointer rendering details** (leading zeros, case) for the `Output::send` pointer
   args — the oracle writes lowercase no-pad forms (fmt default) and the reader should
   match numerically, not byte-for-byte, for those specific values.
10. **Post-ready failure acks** (`stopped`/`failed` after `mods-started`) — emit strings
    pinned (`OWN:176,187`) but their interaction with a game shutdown vs unload was not
    staged here; S1/S2 cover the designed shapes only.

## 4. Coordinator supersede chain (all folded before finalization)

1. **Pool-shape heap window is vintage-transient** (worker-geometry-reference): kHeapLo/
   kHeapHi [0x8018c000,0x100118000) absent from the igf3 7-span walk; the
   `Blocks[0] outside the readable heap span` refusal classified EXPECTED-REFUSAL(new info),
   verdict cap GOOD (row F15, §3 quick table). I verified the interim claim "the observed
   Blocks[0] value is NOT printed" against the then-current source — TRUE at read time —
   and recorded it as an honest finding.
2. **Extent end 0xc0602c8** (not 0xfc602c8 — comment typo, GEO fact 4); anchor_count=7 /
   intervals predictions; uniform slide — folded into D1-D4 and derived_constants.
3. **Initializer v2 (launch10-b) value-carrying pool refusal** — superseded the value-blind
   format: `UI:846-847` on disk now reads `…{} (observed CurrentBlock {}, Blocks[0] 0x{:X}
   vs span [0x{:X},0x{:X}))`; verified on disk before folding; F15 and the §3 table updated;
   the refusal is now value-carrying and feeds worker-fname-heap-runtime-bounds if it fires.
4. **intervals=1, not 6** (worker-anchor-cap-analysis): the six readable igf3 records are
   pairwise adjacent and the adapter coalesces — predicted pair is `lmf_intervals_found=1`
   with `bridge_lease_reg_spans=6`; `intervals=6` indicates page gaps the pinned records do
   not show; the fixture suite's intervals=6 assumption is a known divergence, NOT a fix
   failure. Folded into D4 (and the D4 note keeps >0 as the hard gate). C8/C9 remain 6/6.

## 5. Verdict-bounds honesty

- BEST requires the FULL armed seed set + engine-ready + mods-started + a mod load attempt;
  a post-load mod error explicitly does NOT demote it (task clause, row G3).
- GOOD caps at any named post-V1/V2 refusal; F15 specifically caps GOOD (heap-window class).
- MINIMAL-ACCEPT isolates the geometry claim (conversion passes, intervals>0, diagnostic
  events present); C13/C10/C11 recurrences raise a REGRESSION flag without negating the
  geometry claim — but they are launch-9 repeats, not new info, and must be reconciled
  before launch-11.
- FAIL is exactly the task definition (geometry refusal recurrence, crash) plus the two
  no-claim cases (rebuild-dead B4, silent no-op F0).
- The 30s ploader cap row (S-30S) is judgment-law: durable sinks outrank toasts; an over-cap
  completion is a latency record (worker-injection-latency R3), not a functional failure.
- Nothing in this oracle advances a verdict by itself; only the real console launch's
  runtime-status.txt / supplier-diag.jsonl (or observer) / UE4SS.log evidence does.
