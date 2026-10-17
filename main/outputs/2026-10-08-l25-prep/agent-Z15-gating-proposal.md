# Agent Z15 — l26 gating proposal (copyin initializer post-admission suspension)

Date: 2026-10-08. REVIEWED-READY PROPOSAL: code included, **not wired into any
build**. Mac-local only; no builds, no console, no network. Writes limited to
`outputs/2026-10-08-l25-runtime/gating-proposal/` and this report.
typescan-r4 CHANGES.json, build scripts, and every consumed file untouched
(read-only). Implements agent-Q's §4 recommendation verbatim in spirit: call
the owner-gate `leave()` at the end of `bridge_copyin_initializer_complete()`.

## Files created (all under outputs/2026-10-08-l25-runtime/gating-proposal/)

| File | sha256 (prefix) | What |
|---|---|---|
| `gating-patch.diff` | 5692775c… | PRIMARY diff: +19 lines (1 event + 1 call + comments) at end of `bridge_copyin_initializer_complete()`, cut against agent-Q's l25 bridge candidate **f510c014…** |
| `gating-patch-lineage-fallback.diff` | eec68e65… | FALLBACK diff: same logical change (+15 lines, no event — that base has no instrumentation), cut against the raw l24 lineage bridge **89e4b643…**; use only if l25 packaging defers Q's bridge candidate |
| `candidate-l25base/lifecycle/runtime_start_bridge.cpp` | 50caba3b… | ready-made patched primary (f510c014… + gating) |
| `candidate-lineage/lifecycle/runtime_start_bridge.cpp` | e5fd4efc… | ready-made patched fallback (89e4b643… + gating) |
| `DECISION-MEMO.md` | — | one-page decision item for Claude: what/why-safe (Q's four reasons, cited)/not-do/risk-register/rollback/l26-not-l25 rationale |
| `MANIFEST.json` | — | hashes + full consumption evidence (§0b style) |
| `compile-proof/{CopyinStartup.hpp, extracted_gate_functions.inc, gate_region_proof.cpp}` | b300cb59…/62823a6f…/11861cc9… | host-compile proof (see §3) |

## 1. Diff target + consumption evidence (agent-O §0b method: receipts, not assumptions)

**The change is bridge-side** — Q's concrete recommendation. `runtime_owner.cpp`
is NOT the target and needs no edit: `RuntimeOwner::initialize` has no
admission-completion hook (it arms the gate via `CopyinOwnerScope` and blocks
until shutdown), while the gate object `g_copyin_initializer_owner` lives in
the bridge TU (line 916). For completeness, the task's consumption check on
runtime_owner.cpp: the l24 runtime build consumed
`outputs/2026-10-07-l19-r3-full-build/units/runtime_owner.cpp` sha **532b8991…**
(unit `runtime_owner`, l24 pass1 COMPOSITION-RECEIPT; hash verified on disk);
**no l25 overlay row exists** for it (profile/typescan-r4/thread CHANGES.json
scanned), so l25 retains the l24 pin — an overlay there would also be the wrong
mechanism (see below).

**Bridge consumption chain (verified):**
- l24 shipped package compiled+linked the lineage bridge
  `outputs/2026-10-08-l24-package/lineage/lifecycle/runtime_start_bridge.cpp`
  sha **89e4b643…** — pinned in STAGED-SOURCE-PINS.json:449-450 and as the
  `outer_bridge` input in BUILD-INPUTS.json:252-253.
- The l25 packaging driver **hard-wires agent-Q's candidate f510c014…** as the
  staged outer_bridge (l25_packaging_driver.py:100-115: asserts the 89e4b643…
  base, pins Q's candidate, records `copyin_probe_wiring`). Therefore the file
  the l26 build consumes under the plan of record is **f510c014…** → the
  PRIMARY diff is cut against it (after Q's state-note block, so the l25
  observation still records `copyin_probe_state=3` pre-gating).
- The bridge is packaging-side, not a runtime-build unit
  (`bridge_copyin_initializer_*` are undefined in l24 pass1 undefined.stdout:23-30),
  so a typescan-r4-style CHANGES row cannot reach it; the sanctioned channel is
  the packaging lineage swap (exactly how Q's WIRING.md File 2 works).

Static diff checks: `patch --dry-run` applies cleanly against each pristine
base; pure additions (`grep -c "^-[^-]"` = 0 for both); brace balance unchanged;
no new symbols/imports (only `g_copyin_initializer_owner` + `diagnostic_event`,
both already in-TU).

## 2. Design points reviewed

- Ordering inside `complete()`: release bank → Q's l25 state note (still
  observes gate_armed=1) → NEW event `copyin_probe_gate_l26_early_release=1`
  → `owner.leave()`. In a gated session the shutdown-time
  `copyin_probe_gate_released` / `copyin_probe_attempts_late_total` events are
  silent (leave() returns at its allows() guard) — documented in the diff
  comment and memo, with the new event as the positive proof.
- `InitializerOwner::leave()` is mutex-guarded and idempotent by construction
  (CopyinStartup.hpp:30-33: clears only if initializer_ == caller; a cleared
  gate never matches). Gate class is byte-identical (b300cb59…) in the
  packaging lineage and the runtime units dir.
- Fail-closed surface after gating (memo risk register): probe_until → -1
  before any kernel call (bridge:1248 lineage / 1297 candidate);
  code_cache_context/signature → 0; write_copyin_capture → 0;
  range_faulted → 1 ("fail closed, no ledger read"). Foreign threads were
  already always refused; post-gating the init thread is refused too.

## 3. Host-compile proof result

Established harness pattern (cf. Z4): verbatim extraction, host clang++
`-std=c++20 -Wall -Wextra`, plain + `-fsanitize=address,undefined
-fno-sanitize-recover=all`. The harness compiles the ACTUAL patched primary
functions (sed 1071,1149 of the patched candidate = enter / PATCHED complete /
leave) against the authoritative gate class copy (b300cb59…), with clearly
marked shape-equivalent stand-ins for `diagnostic_event`, the directory
release, and the state-note statics.

**Result: PASS both modes, exit 0 — "ALL CHECKS PASSED: gating property holds,
release idempotent".** Assertions exercised: gate closed pre-admission for
everyone; enter() arms init thread only (foreign thread refused); complete()
releases exactly once, emits `copyin_probe_state=3` + the new early-release
event; **after complete() the init thread itself gets -1** (the gating
property); second `complete()` AND shutdown `leave()` are no-ops (no second
release, no shutdown events — the idempotence the task required); foreign
threads stay refused; explicit re-arm via `enter()` still works and the
unpatched shutdown leave() path still functions on a re-armed gate.

## 4. Honest limits

- No target compile, no full-TU compile (needs Prospero SDK headers — rules
  prohibit builds anyway); the full patched bridges' syntax is otherwise
  unchecked beyond balance/dry-run checks. Q's l25 observation candidate itself
  carries the same status until the packaging step compiles it.
- The fallback diff omits the diagnostic event (its base has no event
  vocabulary in that spot); if the lineage fallback is chosen, the gated build
  reports closure only by the ABSENCE of shutdown events — weaker evidence,
  noted in the memo.
- Decision authority rests with Claude; this packet makes no admission, no
  build_model change, and no assumption that Q's candidate is adopted (hence
  the two-base packet with explicit precedence in MANIFEST.json).
