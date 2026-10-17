# MERGE.md — worker-init-v4r3 (2026-10-05)

Three-way merge that resolves the launch-11 A3 lineage collision: one initializer
carrying BOTH the launch-10-final v4 lineage (base A) and the R3 armed fold (base B).
Machine-readable record: `receipts/derivation.json`. Applied diff: `staging/` +
`diffs/`; result `staging/UnrealInitializer-v4r3.cpp` (sha `22f06286…`, 2375 lines).

## 1. Lineages

| id | file | sha256 (16) | lines | content |
|---|---|---|---|---|
| O (common ancestor) | worker-launch10-prebuild/staging/initializer-launch10/UnrealInitializer.cpp | `c85c42f7aaab0630` | 2044 | launch-10 prebuild armed staging (v2-class pool check) |
| A | launch10-build/final-units/UnrealInitializer.cpp | `4a2b9400b70e61ec` | 2224 | launch-10-final fold + v4 RE-correction (pool predicate, GMalloc V1/V2 validators, read-path guards, value-printing refusals) |
| B | worker-launch10-r3-armed/staging/initializer-l10-r3/UnrealInitializer.cpp | `8b0c7e9905ce4a85` | 2195 | O + DIFF-2 rev B + arming flip (R3 armed fold), v2-class pool check — THE COLLISION |

A = O + (launch-10-final fold `X` + v4 diff `3c45a1ac`) — confined to **O-lines
614–852** (SeedArming namespace). B = O + **rev B `68d0c006`** + **flip** — confined
to the R3 sites. Headers differ per side: A ships `SeedValidation.hpp f09ec9b3` +
`SeedTable.h d183e0c0` (armed `59effe6e` + GMalloc V1/V2 constants, which A's
validators reference — `59effe6e` alone does NOT satisfy A); B's staging carried the
older `b8d851e9` / `59effe6e`. The merged staging ships the **A-world headers**
(required invariant — see §5).

## 2. Task 1 — the extracted patch series (from B)

Series = `[DIFF-2 rev B, arming flip]`, validated by reconstruction:

1. `O + revB.diff (patch -p1)` → `72247480…` — byte-identical to B's kept pre-flip
   copy `UnrealInitializer.cpp.r3fallback0` (`72247480be0567b9…`).
2. `72247480 + flip` (line 179 `#define UE4SS_R3_MARSHAL_MODE 0` → `1`) → `8b0c7e99…`
   — byte-identical to B.

Both steps `cmp`-exact: the series IS the complete B delta. Copies:
`diffs/r3-initializer-r3-marshal-mode.revB.diff` (`68d0c006…`, verbatim from
worker-r3-diff2-rebase) and `diffs/r3-initializer-arming-flip.diff`
(`006e18f3…`, recorded here; round-trip applied clean).

rev B hunks (old-side O coordinates): H1 `@158` (+43: R3MarshalGate namespace +
guarded mode switch), H1b `@201` (`[[maybe_unused]]` on `HookedEngineTick`), H2
`@223` (`[[maybe_unused]]` on `QueryHarnessEngineTickDispatch`), H3 `@1312` (+16:
`VerifyFNameConstructor` R3 early-return), H4 `@1782` (+20/−1: EngineTick
registrations refuse/defer + `#else`/`#endif`), H5 `@2041` (+71: the three
`R3Marshal*` bridges at file tail).

## 3. Overlap map (rev B vs A's v4 changes)

| rev-B hunk | O coords | lands at (merged) | A's nearest change (O coords) | overlap |
|---|---|---|---|---|
| H1 R3MarshalGate + mode switch | 158–201 | 158–201 | none before 614 | NONE |
| H1b `[[maybe_unused]]` HookedEngineTick | 201–206 | 201–206 | none before 614 | NONE |
| H2 `[[maybe_unused]]` QueryHarness dispatch | 223–228 | 269–274 | none before 614 | NONE |
| H3 FName-verification early-return | 1312–1330 | 1541–1556 | A ends at O:852 | NONE (495+ untouched lines between) |
| H4 EngineTick refuse/defer | 1782–1802 | 2022–2049 | A ends at O:852 | NONE |
| H5 bridges (file tail) | 2041–2044 | 2246–2375 | A ends at O:852 | NONE |
| flip | line 179 | 179 | none | NONE |

A's v4 region in merged coordinates: 615–1032 (`read_bytes_at` 779,
`consume_required_slot_seeds` 833, GMalloc validators 861+, v4
`check_pool_shape(words.current_block, words.none_id, …)` call 991,
`check_none_anchor` 1014–1015, `bSeedBoundFNameCtor = true` 1062, seed consumption
call 1178).

Measured application: `patch -p1` of rev B onto A → exit 0, **zero fuzz, zero
offset, no .rej** (exact context match at original coordinates). The A→merged
unified delta is line-for-line identical to the O→B delta modulo the flip line
(151 changed lines both sides; the only textual difference is the flip itself).

### Conceptual near-misses (checked, no conflict)

- H3's comment cites `SeedArming::bSeedBoundFNameCtor` coexistence. v4 did not
  touch that symbol (merged:1062) — the statement stays true.
- A renamed the SeedValidation publication to `publish_readable_spans` (and the
  controller deleted the retired alias in `f09ec9b3`). rev B never references it.
- The flip's `#ifndef UE4SS_R3_MARSHAL_MODE` guard block is rev-B-inserted, so the
  flip could not collide with anything A-side (A had no such macro).

## 4. Task 2 — resolution (apply to A)

```
A (4a2b9400) + revB (patch -p1, clean) = 8f1ad71a…   (pre-flip merged)
8f1ad71a + flip (line 179, 0 -> 1)     = 22f06286…   staging/UnrealInitializer-v4r3.cpp
```

No hunk needed manual placement; both semantics preserved verbatim — the fold
compiles out the detour verification path, the v4 predicate governs pool ADMISSION.
**No genuine semantic conflict found; nothing STOPPED.**

Semantic composition (why they compose):

- The v4 predicate lives in the SEED path (`SeedArming::consume_required_slot_seeds`,
  called at merged:1178 under `__PROSPERO__` when `bSeedsArmed`). R3 mode does not
  compile it out — a valid pool still ADMITS under the engine-CurrentBlock law,
  span-membership-either-class and the entry-0 None anchor.
- If the seed walk admits, it binds the ctor (`bSeedBoundFNameCtor`, merged:1062)
  and the detour verifier is skipped entirely. If the verifier is still reached
  (seed refused/unbound), the R3 early-return (merged:1541–1556) settles
  `FNameVerificationStatus` and refuses loudly instead of arming the PLH detour.
- H4 (merged:2022–2049) compiles out BOTH EngineTick registrations, sets
  `pump_pending`, prints the deferral line; the pump drains via the LuaMod DIFF-1
  consumer (l11 merged luamod `dc687602`), which odr-uses
  `R3MarshalPumpPending` / `R3MarshalPublishGameThreadIdentity` — both DEFINED in
  the merged object (V4R3-5), satisfying the plan-D pairing law.
- `[Seeds] REFUTED` remains a possible early terminal (unchanged, as the r3-armed
  worker recorded).

## 5. Required header invariant (recorded honestly)

The merged initializer references `SeedTableNS::kGMallocInstanceLabel` /
`kGMallocVtableLabel` (merged:849/855) and the v4 `rc_seed` API — these exist ONLY
in the A-world headers. The merged staging therefore ships
`SeedValidation.hpp f09ec9b3` + `SeedTable.h d183e0c0` (final-units pair); B's
older staging headers would NOT compile against the merged source. This is a
required invariant of the staging dir, not an optional companion. (Mission phrasing
"armed SeedTable 59effe6e" for A is recorded as loose: the shipped launch-10-final
table is `d183e0c0` = `59effe6e` + the GMalloc constants.)

## 6. Gates (all measured; details in CLAIM.json / receipts/)

1. Series byte-exactness: §2. PASS.
2. Compile gate (190-entry argv, `LLVM_CONFIG=…LLVM-21.1.8…`): control
   final-units v4 → `a437861a…` EXACT (== v4 worker's gate object); merged →
   `fe5d26d9…` (1,830,904 B), two byte-identical passes; warning multiset
   (6) == control == v4 baseline. PASS.
3. Object census: v4 strings present / retired absent; R3 strings + 3 bridges
   DEFINED only in merged; Phase-2 verify string compiled out in merged; armed
   banner in both; 0 `R3Marshal*` UND. PASS.
4. Host predicate suite: 69/69 exit 0 against the merged staging (adapted AL1 call
   site, one token — the controller's own alias removal made the original AL1
   unbuildable against `f09ec9b3`); unmodified suite 69/69 baseline repro;
   receipts byte-identical. PASS.
