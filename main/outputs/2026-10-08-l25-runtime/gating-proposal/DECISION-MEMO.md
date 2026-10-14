# DECISION MEMO — gate the copyin initializer at admission completion (l26)

**For:** Claude (controller decision, l25 packet) · **From:** agent Z15 · 2026-10-08
**Implements:** agent-Q l25 report §4 recommendation. Code is in this packet
(`gating-patch.diff`, ready-made candidate, host-compile proof) — **NOT wired
into any build.** Nothing here touches l25's variants, instrumentation, or
receipts.

## What the change is

One logical line, bridge-side, at the end of `bridge_copyin_initializer_complete()`
(after the bank release and after Q's l25 state note): disarm the initializer
owner gate —

```cpp
::diagnostic_event("copyin_probe_gate_l26_early_release", 1u);  // primary variant only
g_copyin_initializer_owner.leave();
```

Today the gate stays armed on the init thread for the whole session (armed at
`RuntimeOwner::initialize` via `CopyinOwnerScope`, runtime_owner.cpp:91-103,
released only when `initialize` returns at shutdown). After this change the
gate closes at admission completion: `bridge_kernel_write_copyin_probe_until`
then returns `-1` at its first check (bridge:1248/1297) for **every** thread
including the init thread, for the rest of the session. The shutdown fallback
(`CopyinOwnerScope` dtor → `bridge_copyin_initializer_leave()`) becomes a
no-op via its existing `allows()` guard — proven idempotent in the compile proof.

The diff is cut against the file the build consumes: agent-Q's l25 bridge
candidate `copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp`
(f510c014…), which `l25_packaging_driver.py:100-115` hard-wires as the staged
outer_bridge (asserting the l24-shipped base 89e4b643…, pinned in
l24-package STAGED-SOURCE-PINS.json:449-450). A fallback diff against the raw
l24 lineage 89e4b643… is included in case l25 packaging defers Q's bridge
candidate (WIRING.md permits that). runtime_owner.cpp itself needs no change:
it has no admission-completion hook, and the gate object lives in the bridge TU.

## Why it is safe — Q's four reasons (l25-prep/agent-Q-copyin-probe.md)

1. **No post-admission caller exists** (Q §1): across all 374 composed units
   only `UnrealInitializer.cpp` references the copyin bridge symbols, and every
   call site is inside `consume_required_slot_seeds`; the last call
   (`copyin_cleanup.complete()`, UnrealInitializer:1206) is the very function
   this patch extends. Nothing can observe the difference between the
   retired-bank refusal (Unavailable, bridge:1256) and the gate refusal (-1) —
   both refuse before any kernel write.
2. **In-session re-admission is contractually absent** (Q §4): seed refusals
   are "terminal for this launch" (UnrealInitializer r4:736-742); pool growth
   never re-enters admission — runtime FName goes through the native ctor
   (NativeFNameAdapter.hpp:39-49); fault-ledger/capture consumers are all
   admission-time.
3. **Next-launch re-admission is unaffected** (Q §4): a new process re-runs
   module_start → `prepare()` re-opens the bank (bridge:980) → `enter()` re-arms
   the gate (gate class stores a thread id; cleared state admits any enter()).
4. **It lands WITH the runtime-phase instrumentation** (Q §4): with Q's overlay
   adopted, a gated and an ungated build produce distinguishable evidence
   either way (`copyin_probe_state` fires before the gate closes; the new
   `copyin_probe_gate_l26_early_release` is the positive proof of early close).

Host-compile proof (compile-proof/): patched region compiles (C++20, plain +
ASan/UBSan, exit 0); gating property, release idempotence (second `complete()`
and shutdown `leave()` are no-ops), foreign-thread refusal, and explicit
re-arm all asserted and passing.

## What this does NOT do

- Does **not** remove or alter Q's l25 observation instrumentation: the state
  note (`copyin_probe_state=3`, gate_armed, bank_retired, sinks/nonce/attempts)
  still fires with its pre-gating values; only the shutdown-time
  `copyin_probe_gate_released` / `copyin_probe_attempts_late_total` events
  become silent in a gated session (replaced by the admission-time early-release
  event) — expected and stated, not a regression.
- Does **not** change variant A/B (`MODBRIDGE_L25_SKIP_DELETE_LISTENER`
  untouched), the bank retirement itself, the fault ledger, captures, the ack
  sink, or any seed/admission logic.
- Adds **no new symbols or imports**; both diffs are pure additions
  (`grep -c "^-[^-]"` = 0); `patch --dry-run` applies cleanly on the pinned bases.

## Risk register

- **A future feature needs a late kernel write / code-cache check / capture
  read** (probe_until, `bridge_code_cache_context/signature`,
  `bridge_write_copyin_capture`, `bridge_copyin_range_faulted` are all
  gate-guarded): it fails **visibly and fail-closed** — probe dispatch returns
  `-1` before any kernel call; cache/capture queries return 0 ("not verified"),
  fault queries return 1 (faulted). No silent wrong data, no corruption; the
  caller's own refusal logging fires (e.g. `pool_copyin_probe_fresh 2`-class
  telemetry). Remedy is an explicit controller decision to re-arm via
  `enter()` — not a silent reliance on the resident gate.
- **Cosmetic:** in a gated session Q's shutdown events never fire; harvest
  tooling must treat `copyin_probe_gate_l26_early_release 1` as the session's
  gate-closed proof (both diffs' comments say so).
- **Not a crash fix:** Q §2 weakens the copyin probe as the CE-108255-1
  primary (bank provably retired before the crash); this is defense-in-depth
  that removes an armed-but-inert kernel-write path from the runtime phase.

## Rollback

One-line revert: delete the added block from
`bridge_copyin_initializer_complete()` (revert `gating-patch.diff`); the
shutdown `leave()` path is untouched and resumes releasing the gate.

## Why l26, not l25

l25 is an A/B attribution experiment (variants arm/skip the delete-listener
registration). Landing gating inside l25 would change runtime behavior in
BOTH arms mid-experiment and confound crash attribution — an early-gated
build can no longer distinguish "probe never fires because refused" from
"probe never fires because gated." l25 variants must keep the current
armed-gate/retired-bank behavior; gating is a separate, observable,
individually revertible l26 decision made with Q's instrumentation already
harvesting. (Same reasoning as Q §4 "why not unilateral".)

## Packet contents

`gating-patch.diff` (primary, vs f510c014…) ·
`gating-patch-lineage-fallback.diff` (vs 89e4b643…) ·
`candidate-l25base/` + `candidate-lineage/` (ready-made patched bridges) ·
`compile-proof/` (verbatim extraction + passing harness) · `MANIFEST.json`
(hashes + consumption evidence). Decision requested: approve gating in l26
(and which base), or reject with reasons.
