# P2 (PARKED) — variant B / dead R3 code-path removal — NOTES

- **Owner:** agent FX (ZCode), 2026-10-10
- **Status:** PARKED PROPOSAL PATCH SET. NOT applied to the live tree,
  drivers, or CHANGES. Mac on battery during authoring: analysis + patch
  files only — no builds, no `--execute`, no console contact.
- **NEVER-APPLY-WITHOUT:** this set changes BUILD TOPOLOGY (one fewer
  default variant at build AND packaging). Apply only when root can follow
  with a FULL rebuild + `l26_packaging_driver.py --plan` (and, for an
  upload-grade run, packaging `--execute` + the P5-P9 acceptance set for
  variant A). Do NOT apply piecemeal behind an in-flight build.

## 0. Apply order (atomic set)

```
1. 01-build-variants-drop-b.patch     (build_variants.py)
2. 02-tree-dead-code.patch            (UnrealInitializer.cpp, LuaMod.cpp, typescan-r4/CHANGES.json)
3. 03-packaging-driver.patch          (l26_packaging_driver.py; authored ON TOP OF P1's packaging hunk — apply P1 FIRST)
```

All three + P1 were dry-run AND really applied sequentially in a /tmp
validation tree: every hunk applies with `--fuzz=0`, zero rejects, and the
resulting files hash-match the constructed intent exactly:

| file | post-P2 sha256 |
|---|---|
| `build_variants.py` | 53739fcf33628ee43cd47eae6c164bc77e4bd34fd9f614e6ecdedc347a423c2e |
| `typescan-r4/candidate/src/UnrealInitializer.cpp` | 6a9087b55207d69d820ae7c15ec5e6fcef61281fcad9876ad16f008d350d6197 |
| `typescan-r4/candidate/src/LuaMod.cpp` | 5a6a06e6f2ece6fb220391b3de198212d0ad66d3b83fdbfa1cbce55e60ac9aa0 |
| `typescan-r4/CHANGES.json` | 8fb0d3ef61b9312808961424072c98eacb61717ce2a84fc9e45b1b3408ce2f60 |
| `l26_packaging_driver.py` (after P1+P2) | dabe31ed8c35851c7f30b29cf08bb63f42038d40bce590370ce565870ecdab09 |

Agent-Q's copyin-probe overlay patch was additionally verified to apply
CLEANLY (`/usr/bin/patch --dry-run`, rc=0, no fuzz) on the patched
UnrealInitializer.cpp — the G2q composition path is intact.

## 1. What was investigated and what is actually dead

### 1.1 Variant B (l26-B, MODBRIDGE_L25_SKIP_DELETE_LISTENER)

Token census BEFORE this patch (compiled sources only): exactly 4 sites —
`UnrealInitializer.cpp` ×3 (G3 cache-block gate :551; class-delete-listener
guard :602; shutdown-listener guard :625) and `LuaMod.cpp` ×1 (Lua-mod
delete-listener guard :8455). `UObjectArray.cpp/.hpp` carry NO B arm (only
the CTOKEN variant-C arms — production, untouched; the task's "B arm in
UnrealInitializer/UObjectArray" map resolves to UnrealInitializer only).

Variant B compiles an UNSAFE runtime (listener registrations skipped, stale
object pointers never cleared), and its diagnostic purpose ended when
variant C's engine-call registration was PROVEN on console (agent-EE/EP
session: the engine's own AddUObjectDeleteListener called and returned).
B is dead weight in every default build/package cycle.

**Design:** B is dropped from the default VARIANTS in both drivers and is
env-opt-in only (`L26_BUILD_B=1` build, `L26_PACK_B=1` packaging; default
off). The env-APPEND mechanism itself is kept so EO's NB control
(`parallel-candidate/02`/`03`) appends its variant exactly the same way.
With the tree arms removed (patch 02), the token disappears from every
compiled source, so an opt-in B `--execute` fails LOUDLY at
`G4-token-present` ("token … not found in the tree … refusing to build a
variant B that would silently equal variant A") — opt-in-dead, never a
silent no-op. To make B buildable again: revert patch 02's UnrealInitializer
+ LuaMod hunks AND patch 01 together.

### 1.2 Dead R3 marshal paths — the investigation result

Verified against the CURRENT tree (`typescan-r4/candidate`), with callers
grepped:

**Provably dead — REMOVED by patch 02:**

1. **The ranked-natives carrier loop** in `r3_marshal_calibration_consumer`
   (`k_r3_marshal_carrier_candidates` = "Actor:ReceiveTick" /
   "AnimInstance:BlueprintUpdateAnimation" resolved via StaticFindObject).
   Dead BY CONSTRUCTION: both names are BlueprintImplementableEvents, so
   the FUNC_Native precheck refused them on every possible world — the
   code's own comment admitted it ("The ranked names are
   BlueprintImplementableEvents (no FUNC_Native), so the precheck above
   refuses them by construction"). AND dead on console: the lookups cannot
   resolve (GATE-1 numbered duplicates; the readfix-session evidence:
   "marshal carrier candidate not found: Actor:ReceiveTick /
   AnimInstance:BlueprintUpdateAnimation (not interned, or class not
   constructed yet)"; the ET nul-throw-armor history: this lookup was the
   exact throw site that killed mod start).
2. **`arm_funcptr_marshal_drain` + `r3_marshal_carrier_precheck`** — their
   ONLY caller was that ranked loop (grep-verified); dead with it.
3. **`R3MarshalQueryHarnessDispatch`** — declared (LuaMod.cpp:302) and
   defined (UnrealInitializer.cpp:3299) but never called by any TU
   (agent-H R3 audit finding, re-verified in the current tree: zero call
   sites).

**Console-dead but DELIBERATELY KEPT (flagged for root/Claude):**

4. The **Blueprint-override event-carrier family**
   (`r3_marshal_arm_event_carriers`, `arm_event_carrier_locked`,
   `r3_event_carrier_precheck`) + `funcptr_marshal_hook` +
   `r3_marshal_lazy_arm`. On console TODAY these cannot arm either — the
   base lookup uses the same long-name StaticFindObject path that GATE-1
   kills (bases.empty() → "no engine per-frame event base function found
   yet" → the readfix session's "carriers ALL failed to arm"), and the
   proven pump is the VmtPump (EU FIX4, the UEngine::Tick vtable-copy
   primary). HOWEVER the EU FIX4 integration comment in the tree is an
   explicit, Claude-approved decision: "The R3 FuncPtr-marshal carriers
   REMAIN the fallback pump (untouched)". Deleting the designated fallback
   is a resilience-policy decision, not dead-code hygiene — this patch set
   does NOT remove it. If Claude wants the fallback gone too, that is a
   one-hunk follow-up on top of this set (the removal boundary is now clean:
   everything between the event-carrier banner and
   `r3_marshal_calibration_consumer`).

**Live — untouched:** `R3MarshalPublishGameThreadIdentity` (called by BOTH
the VmtPump tick entry and funcptr_marshal_hook), `R3MarshalPumpPending`,
`engine_tick_hook`/`process_event_hook` + `ensure_*_hooked` (the safety-net
registrations called from ~30 enqueue sites; not safely removable),
`m_funcptr_marshal_carrier_id`/`callback_id` (still used by the event
carriers).

## 2. Patch-by-patch contents

### 01-build-variants-drop-b.patch (build_variants.py, base 5618dd51…)

- `VARIANTS` default = A only; `B_VARIANT` env-gated via `L26_BUILD_B=1`.
- `prepare_incremental` retained-key loop derives from the ACTIVE variant
  set (a dropped B can no longer fail `G-inc-base-build-B` on a build that
  never happens).
- `pair_receipt` (G9/G10 + `VARIANT-PAIR-RECEIPT.json`) runs only on A+B
  runs; A-only runs record both gates as planned-skip with an explicit note
  and write NO pair receipt file.
- `G4-token-present` enforces the B token only when B is active; the
  refusal message names the P2 tree patch as a possible cause.
- Docstring gains the P2 paragraph. The NB env-append mechanism (EO patch
  02) is unaffected in mechanics; see §4 rebase notes.

### 02-tree-dead-code.patch (tree + CHANGES.json)

- Removes the 4 variant-B skip arms (production registrations become
  unconditional); every site keeps a `[L26][P2]` tombstone comment that
  deliberately does NOT spell the full token (so the token census is
  honestly zero).
- Removes the ranked-natives loop, `arm_funcptr_marshal_drain`,
  `r3_marshal_carrier_precheck`, and `R3MarshalQueryHarnessDispatch`
  (decl + def), each with a tombstone; refreshes the three stale comments
  that referenced them.
- `r3_marshal_calibration_consumer` now: pump_pending check → event
  carriers → warning. (Its `[R3] launch-11` call site in
  `on_program_start` is untouched.)
- `typescan-r4/CHANGES.json`: `candidate_sha256` refreshed for BOTH edited
  files in BOTH `changes[]` and `files[]` rows (the G2 pin discipline) +
  `fx_p2_row_sync` note documenting the whole change.

### 03-packaging-driver.patch (l26_packaging_driver.py, after P1's hunk)

- `import os` (also fixes the implicit `os.environ` use in EO's NB patch 03).
- `VARIANTS` default = A only; `B_PKG_VARIANT` env-gated `L26_PACK_B=1`.
- `pair_packaging_receipt` (P10/P11 + `VARIANT-PAIR-PACKAGING-RECEIPT.json`)
  runs only when both A and B were packaged; A-only runs record P10/P11 as
  planned-skip and write no pair file.
- `--plan` expected-gates list includes P10/P11 only when B is active.
- Docstring updated; LANDMARK comment left where EO's NB packaging entry
  should re-anchor.

## 3. Census / import receipt implications (every downstream touch)

Build side (`build_variants.py`, default A-only runs):

- `VARIANT-PAIR-RECEIPT.json` — no longer written (pair gates planned-skip).
- `EXECUTION-RECEIPT.json`/`PLAN.json` `variants[]` — A only (automatic).
- G6 link census (391+len(NEW_UNITS)) and G7 import-delta — per-variant,
  unchanged for A.
- G8 twin identity within variant A — unchanged.
- Incremental mode: `resolve_base_build('A')` unchanged; no B base-build
  resolution attempted.
- Existing `l26-B*` build dirs under `2026-10-08-l26-build-variants/` —
  frozen provenance, untouched (nothing deletes them; `--execute` refuses
  to overwrite existing dirs anyway).

Packaging side (`l26_packaging_driver.py`, default A-only runs):

- `FINAL-RECEIPT-VARIANT-B.json` — no longer written on default runs.
- `VARIANT-PAIR-PACKAGING-RECEIPT.json` — no longer written on default runs.
- `FINAL-RECEIPT-VARIANT-A.json`, per-variant `PREPARATION-RECEIPT.json`,
  `BUILD-INPUTS.json`, `STAGED-SOURCE-PINS.json`, P0–P9 gates — unchanged
  shape for A.
- Existing `2026-10-08-l26-package-B*` dirs — FROZEN provenance (the
  frozen known-good record documents the B sibling PRX aa29c765…); never
  delete.
- The `P8` constructor-array census expectation (239/2 after the root
  rebaseline) is per-variant — unchanged.

Tree/CHANGES side:

- `typescan-r4/CHANGES.json` — updated BY patch 02 (both rows + sync note);
  G2/G2r/G2s pins stay green because file and pin move together.
- The composed copyin-probe overlay (`staging-copyin-probe/
  UnrealInitializer.cpp`) — regenerated by the next build from the patched
  tree; Q's patch verified to rebase cleanly (see §0).
- `copyin-probe/candidate/src/UnrealInitializer.cpp` (Q's frozen candidate
  copy) still contains the OLD tree bytes and thus the skip-token STRING —
  but it is not a compiled source for any unit and `.json`/candidate-copy
  token hits cannot arm a B build: with the arms gone the token defines
  nothing, and an opted-in B fails at G4 (token not in any compiled
  source) — the refusal text explains exactly this.
- Historical receipts and `FROZEN-KNOWN-GOOD-20261010.md` references to
  variant B — immutable provenance; the freeze doc already marks B as
  "context, NOT part of this freeze".

## 4. EO NB patch rebase notes (parallel-candidate/01-05)

- `02-build-variants-nb-env.patch` was authored against build_variants.py
  549b54dd (pre FF/FG/EZ); the current driver is 5618dd51 and P2/01 moves
  the B entry out of the `VARIANTS=(...)` literal. Rebase: EO's hunk that
  appends `NB_VARIANT` after the tuple still applies conceptually — anchor
  it after the `B_VARIANT`/`L26_BUILD_B` block (same file region); its
  `VARIANTS+(NB_VARIANT,)` append pattern is exactly the mechanism P2/01
  preserves.
- `03-packaging-nb-env.patch` anchored on the removed B entry — re-anchor on
  the `B_PKG_VARIANT`/`L26_PACK_B` landmark block; it also implicitly
  relied on `import os`, which P2/03 now provides.
- EO's G11/G12 control gates live in `main()` regions P2 does not touch.

## 5. Rollback

Reverse order, each patch is a clean `/usr/bin/patch -R -p1`:

1. `03-packaging-driver.patch -R`
2. `02-tree-dead-code.patch -R` (restores the skip arms + R3 code + old
   CHANGES pins)
3. `01-build-variants-drop-b.patch -R` (restores the A/B default pair in
   both drivers)

After rollback the tree is byte-identical to the pre-P2 state (verified by
the hash round-trip in §0 — reverse of the same hunks). No receipt files
were created/removed by the patch set itself (those effects only occur on
the next build/package runs).

## 6. Honest limits of this analysis

- Static analysis only: token census, caller greps, preprocessor balance,
  brace balance, Q-overlay dry-run, sequential apply + hash round-trip.
  NO target compile, NO build, NO packaging, NO console run (Mac on
  battery; also no prospero toolchain invocation).
- The "event carriers are console-dead" claim rests on the readfix-session
  console evidence + GATE-1 documented state, not on a fresh launch; the
  KEEP decision is policy-based (EU FIX4 fallback designation) and is
  flagged for an explicit Claude decision if the fallback should go too.
- Root must run `build_variants.py --plan` and
  `l26_packaging_driver.py --plan` after applying (before any `--execute`);
  the plan gates will name any drift this analysis could not see.
