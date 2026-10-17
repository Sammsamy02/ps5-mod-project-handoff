# P1 (PARKED) — fault-ledger desaturation — NOTES

- **Owner:** agent FX (ZCode), 2026-10-10
- **Status:** PARKED PROPOSAL PATCH. NOT applied to the live tree, drivers,
  or CHANGES. Mac on battery during authoring: analysis + patch files only —
  no builds, no `--execute`, no console contact.
- **Patch:** `01-fault-ledger-desaturation.patch` (4 hunks, apply from the
  worktree root with `/usr/bin/patch -p1 --fuzz=0`)
- **NEVER-APPLY-WITHOUT:** this patch changes admission-ADJACENT behavior
  (the fault-evidence input to the `bridge_copyin_range_faulted` admission
  gate). It must be applied only together with a planned console session
  carrying the ToString-bind context (the [Seeds][L27] Phase 4c label,
  image 0x63FA00/32B) so the validation signature below can actually be
  observed. Do NOT apply "cold" into a build whose launch plan cannot check
  the `[ST]` line and the admission sequence.

## 1. Why this patch exists (evidence chain)

### 1.1 The false positive (EO §0 quotes)

From `outputs/2026-10-08-l25-prep/agent-EO-parallel-candidate.md` §0 (the
15:36Z session forensics):

> **Phase 4c REFUSED**: `[Seeds][L27] ToString bind refused: prologue
> signature: <wide-leak>; legacy Kismet route retains FName::ToString`.
> The reason is the NativeFNameChecks.hpp `signature()` gate's
> line-17 pre-check — de-widened it is uniquely
> **"constructor prefix has recorded copyin fault"**: `ops.known_fault`
> → `bridge_copyin_range_faulted(0x63FA00,32)` returned TRUE, exiting
> BEFORE code_identity/query (confirmed: no `[Seeds][L18] code-signature`
> and no `[Seeds][L14] VirtualQuery needed=[0x63FA00…` line exists for the
> ToString label — only slot 2's pair is present).

> the fault ledger (`g_copyin_faults`, runtime_start_bridge.cpp, sha
> 94f81a17 matches the MANIFEST pin — not a composition drift) records
> ONLY on EFAULT inside a copyin probe (line 1386), yet the L25 state line
> in the SAME log reports `oracle-probes=0 used-copyin=0` […] So
> known_fault was TRUE for [0x63FA00,0x63FA20) with zero recorded probes
> this session. `l13_sink::FaultLedger::overlaps()` (CopyinSink.hpp:34-40)
> also returns true when `saturated` — and `record()` saturates on an
> INVALID range — a candidate mechanism EM should check first.

### 1.2 What the mechanism ACTUALLY was (root cause already landed in-tree)

Agent EM's root-cause work (recorded in
`outputs/2026-10-08-l26-runtime/typescan-r4/CHANGES.json`, key
`l27_ei_stringfix`, by agent-EM 2026-10-09) **disproved the ledger-saturation
attribution for the 15:36Z session specifically**: the true mechanism was the
Z15/EE L26 early-release gating — `bridge_copyin_initializer_complete()`
disarmed the copyin owner gate between slot-2 admission and Phase 4c, and
`bridge_copyin_range_faulted` (runtime_start_bridge.cpp:1198-1202) **fails
closed on the owner gate before ever reading the ledger**:

```cpp
extern "C" int bridge_copyin_range_faulted(uint64_t addr, uint64_t len)
{
    if (!g_copyin_initializer_owner.allows()) return 1; // fail closed, no ledger read
    return g_copyin_faults.overlaps(addr, len) ? 1 : 0;
}
```

The fix for THAT mechanism is already in the tree (copyin_cleanup.complete()
moved inside consume_required_slot_seeds immediately after Phase 6, EM/EP
placement repair). **This patch does not touch that line or that fix** — the
fail-closed owner-gate semantics are preserved byte-for-byte.

### 1.3 What this patch fixes (the remaining latent mechanism)

EO's candidate remains a REAL defect class in `FaultLedger`
(CopyinSink.hpp:22-41, sha 2018c9bd, identical in every l13→l26 lineage):

1. `record()` **saturates the whole ledger on a single INVALID range**
   (line 28: `if (!valid_range(addr, len)) { saturated = true; return; }`).
2. `overlaps()` **returns TRUE for every query once saturated** (line 36),
   indistinguishable from a real recorded fault at the gate.
3. `record()` also saturates at capacity (64 ranges) with the same
   all-poisoning effect.

So ANY future `record()` caller with an invalid range (a short-write
recorder, a host test, a future probe variant) — or 64 genuinely distinct
faulted ranges — permanently converts "known_fault" into "true everywhere",
reproducing exactly the "known_fault TRUE with zero recorded probes"
symptom. Today the only call site (line 1386) sits behind a
`valid_range` check (line 1339), so the invalid-range arm is currently
unreachable — this patch is defense-in-depth that makes the entire class
unrepresentable, not a fix for tonight's specific refusal.

## 2. What the patch changes

Per Claude's spec (a)/(b)/(c):

- **(a)** `record()` returns `RecordResult` (`Recorded` / `AlreadyKnown` /
  `Full` / `InvalidRange`). An INVALID range is REJECTED and counted
  (`rejected_invalid`), never recorded, never saturating. The bridge call
  site logs it once, loudly (`copyin_fault_ledger_reject_invalid`).
- **(b)** `overlaps()` no longer consults `saturated`. Capacity saturation
  is queryable ONLY via the new explicit accessor `saturated_capacity()`,
  which the admission gate (`bridge_copyin_range_faulted`) deliberately
  does not call. An invalid QUERY range still fails closed (`return true`)
  — unchanged semantics. Real recorded faults still refuse — unchanged.
  Capacity-full is logged once loudly at the record site
  (`copyin_fault_ledger_full`) so the semantic change (a full ledger no
  longer auto-poisons queries) is never silent.
- **(c)** Regression self-test hook, twice:
  - compile-time: `FaultLedger::self_test_mask()` (constexpr) +
    `static_assert(... == 0xF)` in the header — runs in every TU that
    includes CopyinSink.hpp, so a regression fails the BUILD;
  - runtime `[ST]` line: `bridge_fault_ledger_self_test()` emits
    `copyin_fault_ledger_st` (expected 15 = all four mask bits) and
    `copyin_fault_ledger_st_pass` (expected 1) exactly once, from
    `bridge_copyin_directory_prepare()` (startup, before any admission
    probe activity). Local ledger only — `g_copyin_faults` is never touched.

Hunk map (all in `01-fault-ledger-desaturation.patch`):

| # | file | change |
|---|------|--------|
| 1 | NEW `copyin-probe/candidate/CopyinSink.hpp` (sha 0b113b1c…) | desaturated FaultLedger + RecordResult + saturated_capacity() + constexpr self-test + static_assert |
| 2 | `copyin-probe/candidate/lifecycle/runtime_start_bridge.cpp` (94f81a17 → a35c7f91) | call-site result handling + one-shot loud events + `[ST]` hook; fail-closed gate line untouched |
| 3 | `copyin-probe/MANIFEST.json` | bridge candidate pin refresh + new `base_files.copyinsink` (base 2018c9bd lineage-overlay-2 file, candidate 0b113b1c) + `l27_fledger_note` chain step |
| 4 | `l26_packaging_driver.py` | EQ-pattern `lineage_overlays` destination `CopyinSink.hpp` (hard error when the manifest entry is missing — never silently stage the frozen pre-desaturation header) + `lineage_source_pins` repoint for the staged sink |

The header reaches the PRX through the packaging stage: the controller
copytrees the frozen lineage root (which carries the OLD CopyinSink.hpp at
`stage/CopyinSink.hpp`) and then writes `lineage_overlays` destinations —
the new overlay row shadows the frozen file before the bridge TU compiles.
Without hunk 4 the candidate header would sit unused and the PRX would keep
the saturating ledger — hunk 4 is what makes the patch real.

## 3. How to validate on console (after root applies + builds + packages)

Expected signature in `supplier-diag.jsonl` (bridge diagnostic events):

1. Early, one-shot, BEFORE any admission/probe activity:
   `copyin_fault_ledger_st=15` and `copyin_fault_ledger_st_pass=1`.
   Anything else = the desaturated header did not reach the PRX (check the
   packaging BUILD-INPUTS `lineage_overlays` row + `fault_ledger_overlay`
   receipt section) — stop and reconcile.
2. The 15:36Z-class `[Seeds][L27] ToString bind refused … constructor prefix
   has recorded copyin fault` line should NOT recur for a healthy bind
   (with EM's owner-gate sequencing fix already in the tree, both known
   false-positive mechanisms are closed). If a `[Seeds][L27]` refusal DOES
   fire, the new evidence distinguishes the causes:
   - `copyin_fault_ledger_reject_invalid` present → a caller passed an
     invalid range to record() (new, named, countable);
   - `copyin_fault_ledger_full` present → capacity saturation (named);
   - neither present → a REAL recorded fault range overlaps the label
     address: check the `pool_copyin_fault_addr_*` events for the actual
     faulted address, or the owner-gate fail-closed path (gate disarmed
     mid-admission — recheck the EM sequencing).
3. Real faults still refuse: any genuinely EFAULTing probe range still
   lands in the ledger and `bridge_copyin_range_faulted` still returns 1
   for overlapping queries; the fail-closed owner-gate line is unchanged
   (verified byte-identical in the patch construction).
4. Host-side: the new header alone compiles clean under
   `clang++ -std=c++20` and its runtime self-test exits 0 (verified during
   authoring); the static_assert fires at build time if anyone reintroduces
   the saturating semantics.

## 4. Downstream touches root must reconcile when applying

- `copyin-probe/MANIFEST.json` — updated BY this patch (bridge pin
  94f81a17→a35c7f91; new copyinsink entry; l27_fledger_note).
  `build_variants.py` G2q checks the MANIFEST pins — they stay green because
  the patch updates pin and file together.
- `gating-proposal/CHANGES.json` — its composition chain ends at
  "94f81a17 CURRENT candidate" (documentation only, no driver consumes it).
  Root should append the a35c7f91 step or annotate; NOT gate-fatal.
- `NEXT-BUILD.md` §"94f81a17 flows through the existing outer_bridge pin
  path" — documentation; update the sha mention when convenient.
- Historical build/package receipts referencing 94f81a17 — immutable
  provenance, never edit.
- The l26_packaging_driver.py hunk is authored against sha
  086e5149… (current). P2's `03-packaging-driver.patch` is authored ON TOP
  of this P1 hunk — apply P1 first, then P2 (documented in P2 NOTES).
- After applying: run `build_variants.py --plan` then
  `l26_packaging_driver.py --plan` and confirm the
  `lineage_overlays`/`fault_ledger_overlay` wiring appears; only then
  `--execute` (needs `BRIDGE_SNAPSHOT_ID` per the FG stamp gate).

## 5. Rollback

Single patch → single revert: `/usr/bin/patch -R -p1` restores all four
files; delete the new `copyin-probe/candidate/CopyinSink.hpp` if reverting
by hand. No tree state outside the four files changes. The runtime `[ST]`
events and the new diagnostic tags are additive; a reverted build simply
stops emitting them.
