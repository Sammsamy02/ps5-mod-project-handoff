# Agent BC — variant-C proposal pre-rebase onto the current tree (ea7221e8)

Date: 2026-10-08. Scope honored: wrote ONLY under
`outputs/2026-10-08-l25-runtime/variant-c-proposal/` (new files) plus this
report. typescan-r4 NOT touched — post-run check: `candidate/src/UObjectArray.cpp`
still `ea7221e8d7537e9b0dd36ddc0ac26a2c76050f343fdf7e8e2b8b03f297281f29`,
`typescan-r4/CHANGES.json` still `55b38893…` (= AU's pin). Scratch work in
`/tmp/l25-bc-rebase/` (ephemeral).

## Task 1 — Rebase: DONE, clean

Drift between AE's cut (against `4f4afddf`) and the current tree (`ea7221e8`):
AT hoisted the refusal-5/6 growth screen above `Contains` (`69463958`), then AU
extended the GMalloc snapshot struct + both GMalloc log lines with the AB §7
slot reads (`ea7221e8`). Both drift regions live INSIDE the Add `try` body —
which the rebased patch wraps as the untouched `#else` hand-rolled path — so no
hunk context overlapped the drift. The original patch applied to ea7221e8 with
zero fuzz; the rebase is a clean re-cut of the same 142 added lines against the
current base (hunk content byte-identical to AE's draft; only context windows
and `@@` line numbers moved).

**Deliverable: `variant-c-proposal/variant-c-patch-rebased.diff`**
(sha256 `11789b8a273f5f0f23a43e6506ddcd4b3458f18f515e4febb8898608d87ea6fe`)

- 8 hunks, purely additive (142 added / 0 removed), same file path headers
  (`a/`…`b/` `outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/UObjectArray.cpp`).
- Hunk map (old-file line numbers, ea7221e8): chrono include guard @8;
  VARIANT C SUPERSESSION note @419; enum `EngineListenerCallUnavailable = 7`
  @444; TU-local engine-op resolver @594; Add `#if` engine-call + `#else` @628;
  Add `#endif` @763; Remove `#if` engine-call + `#else` @778; Remove `#endif`
  @812.
- ALL instrumentation markers kept: hand-rolled markers verbatim inside `#else`
  (`l25_dl_add_begin/pre/gmalloc_pre/risk/post/gmalloc_post/add_done`,
  `remove_begin/remove_done`, `l25_dl_refused` ×16); engine path reuses
  `add_done`/`remove_done` carrying elapsed-ms (documented value-semantics
  delta) and adds `l25_dl_engine_call` (0=Add, 1=Remove). AT's pre-read guard
  (5→6 above Contains) and AU's slot reads (`+0x28/+0x30/+0x38/+0x40/+0xd0`)
  are preserved intact inside the `#else` path.

## Task 2 — Verification

**(a) Clean apply**: `patch -p1 --dry-run --fuzz=0` on a fresh copy of the
current tree file → clean (no fuzz, no rejects, exit 0); real apply → exit 0;
applied result sha256
`72d17fc8483d5426484ac3f7d5acebec7b27244922fca039212d6907c30f6782`.

**(b) Syntax compile** (`/usr/bin/clang++ -fsyntax-only`, exact
`/tmp/l25-combined-smoke` recipe incl. `-D__PROSPERO__ -D__SCE__`):

| state | exit | errors | notes |
|---|---|---|---|
| current tree control (ea7221e8) | 0 | 0 | 2 known inherited Atomic.hpp -Wswitch warnings |
| patched, switch OFF (hand-rolled active) | 0 | 0 | same 2 warnings |
| patched, `-DMODBRIDGE_L25_ENGINE_ADD_LISTENER` (engine-call active) | 0 | 0 | same 2 warnings |

Preprocessor proof the switch really selects paths: `l25_resolve_engine_listener_op`
appears in `-E` output 3× under switch ON (definition + 2 call sites), 0× under
switch OFF. Logs: `variant-c-proposal/bc-rebase-verification/`.

**(c) Switch composition + refusal codes**:

- AE's exact composition semantics preserved — the two switches live at
  different layers and compose WITHOUT nesting: variant B
  (`MODBRIDGE_L25_SKIP_DELETE_LISTENER`) guards the CALL SITES
  (UnrealInitializer.cpp:503/554/577, LuaMod.cpp:8078 — untouched by this
  patch); variant C (`MODBRIDGE_L25_ENGINE_ADD_LISTENER`) replaces the CALLEE
  body after the shared null-guard (code 1). B+C = registration never called
  (skipped); B-off+C-on = engine-side registration; B-off+C-off = hand-rolled.
  Per-function structure: shared prologue (mutex, begin marker, enter log,
  null-guard) → `#if` engine path (resolve → call → elapsed-ms log) → `#else`
  entire current hand-rolled path verbatim → `#endif` after the catch blocks.
- Engine-call path performs ZERO listener-array access: the only
  `Contains`/`Append` tokens in the `#if` blocks are inside the explanatory
  comment ("no header reads, no Contains, no Append") — verified by grep over
  the exact block spans (Add 720-747, Remove 900-921 in the patched file).
- Refusal codes coexist without collision: enum contiguous 1-7 — code 6 (AT,
  `ImplausibleDataForGrowth`, hand-rolled growth screen, `#else`-only) and
  code 7 (AE, `EngineListenerCallUnavailable`, engine address
  unresolved/misaligned, engine-path-only) are path-disjoint by construction.

## Task 3 — DECISION-MEMO.md updated

New section 8 "Current-state note (agent BC, 2026-10-08)" + deliverables entry:
rebased-against ea7221e8, verified, ready for adoption post-B per Claude's
plan; AA-corroborated addresses (Add `0x4841A0` / Remove `0x484240`, runtime
`0x8841A0`/`0x884240` at base 0x400000) pinned to image sha256 `3b6cfad4…`
(same sha as the seed table; re-derive per title). AE's sections 1-7 untouched.

## Task 4 — Packet note

**Variant C remains PROPOSAL-STAGE**: not in the twin build, not in
typescan-r4, not in CHANGES.json. Adoption (staging the candidate file,
CHANGES.json rows, third build arm / B-off+C-on production shape) is Claude's
decision; this rebase removes the drift-cycle excuse, it does not adopt.

## Artifacts

| what | path |
|---|---|
| rebased patch | `outputs/2026-10-08-l25-runtime/variant-c-proposal/variant-c-patch-rebased.diff` |
| updated memo (section 8) | `outputs/2026-10-08-l25-runtime/variant-c-proposal/DECISION-MEMO.md` |
| verification logs + receipt | `outputs/2026-10-08-l25-runtime/variant-c-proposal/bc-rebase-verification/` (3 syntax logs, apply+pp receipt) |
| scratch (ephemeral) | `/tmp/l25-bc-rebase/` |

## Final answers

- Rebase clean: **y** (`--dry-run --fuzz=0` clean on a fresh ea7221e8 copy;
  apply exit 0; deterministic result `72d17fc8…`)
- Compile verdict: **PASS** — host clang++ -fsyntax-only exit 0 / 0 errors in
  all three states (control, switch off, switch on); only the 2 known
  inherited Atomic.hpp warnings
- Switch-composition semantics preserved: **yes** — AE structure intact
  (B at call sites, C in callee; B+C = skipped; `#if` engine-call / `#else`
  hand-rolled incl. AT guard + AU slot reads / `#endif`); codes 6/7
  coexist, path-disjoint
- New diff path:
  `outputs/2026-10-08-l25-runtime/variant-c-proposal/variant-c-patch-rebased.diff`

Not done (out of scope, Claude's call): adoption/staging, prospero-clang++
cross-compile of the patched TU, SEED-TABLE.json promotion of the two labels,
profile offset fix (section 2 dependency for the `#else` path only).
