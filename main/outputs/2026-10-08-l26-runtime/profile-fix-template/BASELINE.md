# BASELINE — l25 profile state before the FUObjectArray listener-zone fix

> **POSTSCRIPT (2026-10-08 ~20:50 UTC): the fix has LANDDED on the live profile
> while this template was being finished.** A downstream agent applied agent-AA's
> binary-verified values via `apply_engine_binary_fix.py` (staged in this very
> directory, implementing PROCEDURE.md §8's drop path): Create 104→88 (0x58),
> Delete 120→104 (0x68), PrimarySerialNumber 176→144 (0x90),
> bShouldRecycleObjectIndices 180→148 (0x94), UEP_TotalSize 184→152 (0x98);
> `UObjectDeleteListenersCritical=0x78` and `ObjObjectsCritical=0x48` VERIFIED
> BUT NOT PUBLISHED (NativeLayoutSchema.inc has no rows for them;
> NativeLayoutBinding.cpp:74 throws "Unknown native member" on schema-less INI
> keys — publishing my template's default Critical key would have broken the
> runtime binding; the drop path was correct). New profile_digest
> `d648e85ef79caa207025a8123c093a67f47ef12ff5ef9eb0801ba9c3b52ab135`; joined
> preflight stays 66 (no listener rows added); live `tests/run_tests.py` now
> asserts the corrected values in a `CORRECTED={...}` dict. Post-landing
> verification (AM, read-only): `run_tests.py` exit 0 — `PASS host preflight10,
> INI total-size guard, digest pair+negative compile, strict join15 controls,
> prior88 immutable + 5 listener-zone corrected, l24 corroborated`; all four
> CHANGES pin rows consistent (l23 originals intact). This document therefore
> records the TRUE PRE-FIX baseline (values/hashes/test output below) for
> attribution of that landing. Receipts:
> `outputs/2026-10-08-l25-prep/agent-AA-offset-verify.md` (sha256
> 9d73f4a51282d9a829f6d53fb955db6cbe0085d7033ef4b6abe2cd96484b9311) corroborated
> by `outputs/2026-10-08-l25-prep/agent-AE-variant-c.md`.

Recorded 2026-10-08 ~20:32 UTC by agent AM (Mac-local, read-only). Purpose: every
post-fix diff must be attributable against these exact values, hashes and test
outputs. Live tree touched by nothing except re-running the profile's own
`tests/run_tests.py` (byte-identical regeneration, drift check below).

## 1. Which profile the l25 build consumes (in place — no staging needed)

`build_variants.py` (line 81) reads its parts from `HERE/part/CHANGES.json` with
`HERE = outputs/2026-10-08-l25-runtime`, parts `['profile','typescan-r4','thread']`.
So the l25 build consumes **`outputs/2026-10-08-l25-runtime/profile/` in place** —
that directory already IS the staged l25 copy (l23 originals stay frozen under
`outputs/2026-10-08-l23-runtime/profile/`, pinned by each row's `original_sha256`;
the l24 dir is only a corroboration sibling). The fix therefore edits the l25
profile dir in place; no l24→l25 re-staging step exists or is needed.

## 2. Current FUObjectArray values (the values AA's measurement will replace)

| field | JSON facts (decimal) | INI `[FUObjectArray]` (hex) | RuntimeBuildProfile.hpp | JoinedLayoutPreflight.hpp |
|---|---|---|---|---|
| UObjectCreateListeners | 104 | 0x68 | 104 (kRequiredMembers) | (no row) |
| UObjectDeleteListeners | 120 | 0x78 | 120 | (no row) |
| UObjectDeleteListenersCritical | — absent — | — absent — | — absent — | — absent — |
| PrimarySerialNumber | 176 | 0xb0 | 176 | (no row) |
| bShouldRecycleObjectIndices | 180 | 0xb4 | 180 | (no row) |
| FUObjectArray::UEP_TotalSize | 184 | 0xb8 | 184 | (no row) |
| ObjFirstGCIndex / ObjLastNonGCIndex / MaxObjectsNotConsideredByGC / OpenForDisregardForGC / ObjObjects | 0 / 4 / 8 / 12 / 16 | 0x0 / 0x4 / 0x8 / 0xc / 0x10 | same | only ObjObjects (16) has a preflight row |

Source string on all six rows: `"retained authenticated image/static evidence"`.
Adjacent-field rationale: THREADSAFE_UOBJECTS=1 (vendored ObjectMacros.h:33)
inserts `mutable FTransactionallySafeCriticalSection UObjectDeleteListenersCritical`
after `UObjectDeleteListeners` (vendored UObjectArray.h:1432/:1434), so
PrimarySerialNumber / bShouldRecycleObjectIndices / UEP_TotalSize all shift when
the true offsets land. `ObjObjectsCritical` also exists in the engine layout but
has no consumer in this port; not covered by the template (see PROCEDURE.md §7).

Current counts: required_members 74, required_vtables 24, prior_facts_retained 93,
new_promoted_facts 5 (the l25 FMap/FSet container promotion), joined preflight
keys 66 = 53 required / 11 conditional / 2 deferred. profile_digest
`4667891671fda00328bc61e1029c86058b1f558df4c4f4ae0922aa18ddd13317` (= sha256 of
the canonical JSON dump, mirrored in CompiledNativeLayoutDeclarations.hpp string
[3], RuntimeBuildProfile.hpp `kDigest`, and CHANGES.json `profile_digest`).

## 3. Baseline hashes

See `baseline/baseline-hashes.txt` (authoritative). Summary:

```
9ade2598cf24260a0d8db7b9fef82f41b5c55177c5fa8778b9665ac81db82700  profile/profiles/townfall-build-facts.json
b158080f50564716163b100601dd8d6b7a639a962233f75c34f9708b83545dc5  profile/units/CompiledNativeLayoutDeclarations.hpp
39a56800d202b28e6754364b602a3580d1ef2a80b799cfbbf6707afcde695d7b  profile/units/RuntimeBuildProfile.hpp
1646a99e2184ea576eece3303e762132a7835b3782c3cee0545ba1fe03525a2c  profile/units/JoinedLayoutPreflight.hpp
76f8e39435419795ac023a7454ca264f78b9c066681850f22209511d7c538761  profile/CHANGES.json
d0590e22bf1e1f39090e675017601d38f5e914a12142796f0496421443fc600e  profile/tests/run_tests.py
4c12753508bd32cabfbcdb9bf17ecc5e9c10e7a3f7bd9c9062b9da871421c12e  profile/tests/RESULTS.json
```

Frozen l23 originals pinned by CHANGES.json (must never change):
```
056cf45a…1466d82  profiles/townfall-build-facts.json
7a289aba…25e4cdd  units/RuntimeBuildProfile.hpp
900bc7ac…d41eab1  units/CompiledNativeLayoutDeclarations.hpp
32c9856c…42424d4a  units/JoinedLayoutPreflight.hpp
```

## 4. Baseline test output (run read-only, 2026-10-08 20:33 UTC)

`python3 outputs/2026-10-08-l25-runtime/profile/tests/run_tests.py`
→ exit 0, stdout (full capture in `baseline/run_tests-baseline-stdout.txt`):

```
PASS host preflight10, INI total-size guard, digest pair+negative compile, strict join15 controls, prior93 immutable, l24 corroborated
```

Determinism check: all five test artifacts (RESULTS.json, preflight.cpp, preflight,
digest_gate.cpp, digest_gate) re-generated byte-identical; live tree unchanged
(backups in `baseline/`). RESULTS.json status `PASS_HOST_ONLY_MAC`, 13 commands.

`build_variants.py --plan` baseline (run 2026-10-08 ~20:44 UTC from the `what/`
root; full gate log in `baseline/build_variants-plan-baseline.txt`):

```
PLAN {"mode": "plan", "affected_units": 162, "token_files": 2, "steps": 662, "gates": 50}
```

All 22 `G2-part-change-pins` rows TRUE (profile CHANGES pins consistent),
`G2r-rows-consumed-by-base` / `G2s-unit-source-rows-affected` /
`G2t-listener-units-affected` / `G12b-header-consumers-recompiled` all TRUE
(Z9 consumption law satisfied for the current rows — this is the state the fix
must restore after editing). One reported not-ok gate is pre-existing and
documented as a non-invariant by the driver itself:
`G2q-copyin-probe-pin-unrealinitializer.base` (the UE4SS base legitimately
drifted when the parallel listener/phase-tag agents revised
UnrealInitializer.cpp; plan mode tolerates it). CWD note: the driver resolves
BASE pin paths relative to the current directory — run it from `what/`, or G1
fails with `FileNotFoundError: '.superpowers/.../live_module_base_scan.cpp'`
(a CWD artifact, not tree damage). This plan run let the driver refresh its own
`outputs/2026-10-08-l25-build-variants/plans/{PLAN,VARIANT-PAIR-RECEIPT}.json`
artifacts (their designed plan outputs; PACKAGING-PLAN.json untouched).

## 5. Consumers of the values (for attribution after the fix)

1. `units/CompiledNativeLayoutDeclarations.hpp` INI string [0] — the RUNTIME member
   offset table. Parsed at init by `NativeLayoutBinding.hpp` into per-class
   `MemberOffsets` maps; `FUObjectArray::GetUObjectDeleteListeners()` throws
   `std::runtime_error` if the table lacks `UObjectDeleteListeners`
   (generated_include/MemberVariableLayout_SrcWrapper_FUObjectArray.hpp;
   typescan-r4/candidate/src/UObjectArray.cpp:598-602 registers the L25 delete
   listener through it). This is the file the fix exists for.
2. `profiles/townfall-build-facts.json` — evidence lineage + digest source
   (digest = sha256 of canonical JSON dump).
3. `units/RuntimeBuildProfile.hpp` — compiled-in `kRequiredMembers` + `kDigest`
   (regenerated form is produced by the l19-r4 `generate_profile.py` machinery).
4. `units/JoinedLayoutPreflight.hpp` — host preflight key table (no listener rows
   today; template adds three).
5. `CHANGES.json` — pin rows consumed by `build_variants.py` G2/G2r/G12b gates and
   by `l25_packaging_driver.py` (`l25_profile_candidates()` requires ≥3 rows and
   copies the candidates for packaging; profile_digest/new_promoted_facts ride the
   manifest notes).
