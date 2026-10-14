# DIFF.md — worker-init-v4r3 staging (2026-10-05)

What the staging adds to the shipped launch-10-final initializer, and the exact
patch series it carries. Result: `staging/UnrealInitializer-v4r3.cpp` =
`staging/UnrealInitializer.cpp` (byte-identical composition name, both `22f06286…`,
2375 lines).

## 0. File map

| file | sha256 (first 16) | role |
|---|---|---|
| `staging/UnrealInitializer-v4r3.cpp` | `22f062863fe2d599` | mission-named deliverable |
| `staging/UnrealInitializer.cpp` | `22f062863fe2d599` | byte-identical copy under the composition basename (runner regen_receipts + basename discipline expect this name); ALWAYS verify equal hashes before use |
| `staging/SeedValidation.hpp` | `f09ec9b319985cf8` | A-world seed classifier (final-units pin; bridge TU world uses the SAME sha) |
| `staging/SeedTable.h` | `d183e0c03e4ec756` | A-world armed table 4/4 + GMalloc V1/V2 constants (final-units pin) |
| `diffs/r3-initializer-r3-marshal-mode.revB.diff` | `68d0c0064de69284` | verbatim copy of worker-r3-diff2-rebase rev B |
| `diffs/r3-initializer-arming-flip.diff` | `006e18f3fd174373` | the one-line arming flip, recorded as a patch |
| `diffs/A-to-v4r3.unified.diff` | — | full unified diff A (4a2b9400) → v4r3 (22f06286), 157 lines |
| `host-test/test_pool_bounds_host.v4r3.cpp` | `78e20890ee7af9bf` | v4 suite + exactly one call-site token (AL1), see §3 |

Derivation: `A (4a2b9400) + revB (patch -p1, clean) → 8f1ad71a + flip → 22f06286`.
Full input sha table: `receipts/derivation.json`. Overlap map: `MERGE.md` §3.

## 1. The applied series (the R3 armed fold, from B)

1. **DIFF-2 rev B** (`68d0c006…`, 6 hunks):
   - H1 `@158` `#ifndef UE4SS_R3_MARSHAL_MODE` fallback define + `R3MarshalGate`
     namespace (`kR3MarshalMode`, `pump_pending`, `refuse_registration_site`).
   - H1b/H2 `[[maybe_unused]]` on `HookedEngineTick` / `QueryHarnessEngineTickDispatch`
     (they become unused under the armed fold).
   - H3 `@1312` in `VerifyFNameConstructor`: under `defined(__PROSPERO__) &&
     UE4SS_R3_MARSHAL_MODE` — settle `FNameVerificationStatus`, loud refusal, early
     return (compiles out the PLH detour verification path incl. the
     `Phase 2: Verifying FName constructor...` string).
   - H4 `@1782`: under `UE4SS_R3_MARSHAL_MODE` — refuse BOTH EngineTick
     registrations, print the deferral line, `pump_pending.store(true)`;
     `#else` keeps the original registrations.
   - H5 `@2041`: the three bridges `R3MarshalPublishGameThreadIdentity` /
     `R3MarshalQueryHarnessDispatch` / `R3MarshalPumpPending`, defined under
     `defined(__PROSPERO__) && UE4SS_R3_MARSHAL_MODE` (pairing law, linker-enforced).
2. **Arming flip**: line 179 `#define UE4SS_R3_MARSHAL_MODE 0` → `1`
   (ARM-DISARM §1.2 in-file fallback; == B's staged state).

## 2. What the staging preserves from A (unchanged bytes)

The whole launch-10-final v4 world: the v4 RE-corrected pool predicate
(`check_pool_shape` with `none_id` + published spans + engine CurrentBlock law),
`check_none_anchor` entry-0 'None' anchor, `read_bytes_at` bounded reads, GMalloc
V1/V2 slot-3 validators, read-path guards, value-printing refusals, and the armed
banner. Zero A-side bytes were altered by the merge (patch clean, no fuzz).

## 3. Suite adaptation note (the only deviation from the v4 worker's suite)

The unmodified v4 suite (sha `674897f0…`) references the retired
`publish_nonimage_readable_spans` alias (fixture AL1). The controller's own
final-units v4 fold REMOVED that alias from `SeedValidation.hpp f09ec9b3`
(fold note 2 of worker-fname-validator-v4: "switch that one bridge call site … and
delete this alias; behavior-identical"), so the unmodified suite cannot build in
the alias-free world. `host-test/test_pool_bounds_host.v4r3.cpp` = unmodified suite
with EXACTLY one token changed (line 611: the AL1 call now goes through the primary
`publish_readable_spans`). AL1 is degenerate-by-design in the alias-free world
(primary-vs-primary equality). Measured: both the adapted suite (vs this staging)
and the unmodified suite (vs the v4 worker's own staging) pass 69/69 exit 0 with
byte-identical receipts.
