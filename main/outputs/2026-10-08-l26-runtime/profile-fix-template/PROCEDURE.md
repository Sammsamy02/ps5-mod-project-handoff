# PROCEDURE — instantiating the profile fix once agent AA's true offsets land

> **STATUS: CONSUMED / SUPERSEDED (2026-10-08 ~20:45 UTC).** AA's values landed
> (`outputs/2026-10-08-l25-prep/agent-AA-offset-verify.md`) and a downstream
> agent applied them to the live profile via `apply_engine_binary_fix.py`
> (staged next to this file), which implements §8's drop path for
> `UObjectDeleteListenersCritical` — verified 0x78 but NOT published, because
> `NativeLayoutSchema.inc` has no row for it and `NativeLayoutBinding.cpp:74`
> throws "Unknown native member" on schema-less INI keys. **Consequence: the
> default path in `profile-fix.diff.tmpl` (publishing the Critical INI key) is
> known-hazardous for this port and must not be used as-is**; only the §8 drop
> path is sanctioned, exactly as applied. This `profile-fix.diff.tmpl` is now
> STALE against the live tree (verified: `/usr/bin/patch` refuses with .rej
> files on the drifted hunks — loud failure, though note patch is per-file
> atomic, not multi-file atomic). The steps below remain the record of how the
> landing was mechanized. Do not re-run them against the live profile.

Everything lives in `outputs/2026-10-08-l25-runtime/profile-fix-template/`.
The live profile, `CHANGES.json`, and build scripts are untouched by this
template; the steps below are what the coordinator runs AFTER AA's measurement
receipt exists. Total: **8 steps** (step 0 is a precondition, not a step).

## 0. Preconditions

- AA's receipt (true FUObjectArray offsets measured from the engine binary) is
  on disk and read; the receipt path + sha256 go into the values file.
- Baseline intact: `profile-fix-template/BASELINE.md` hashes still match
  (`shasum -a 256` the five files listed there). If they drifted, REGENERATE the
  template first: `python3 profile-fix-template/make_template.py` (reads the live
  profile read-only and rebuilds `profile-fix.diff.tmpl` context).
- The twin variants have NOT been executed yet (`outputs/2026-10-08-l25-build-variants/`
  contains only `plans/` at template time). If they HAVE been executed by the time
  you read this, the rebuild needs a coordinator decision on the existing variant
  dirs (G3 requires them absent); do not silently reuse old receipts.

## 1. Fill the values file (the only place numbers are typed)

Copy `values.example.json` → `aa-values.json` (same directory). Replace the six
decimal offsets with AA's measured truth and fill `source` + `provenance`
(receipt path, sha256, method). Decimal is the single authority — the INI hex
slots are derived automatically (`hex(dec)`), so the JSON/INI/hpp spellings can
never diverge. If AA measured `ObjObjectsCritical` too, see §7 (not covered by
default). If AA reports `UObjectDeleteListenersCritical` absent/unmeasurable,
see §8 (drop path).

## 2. Review the instantiated patch (dry run, no writes to the profile)

```
cd outputs/2026-10-08-l25-runtime/profile-fix-template
python3 instantiate_profile_fix.py --values aa-values.json \
  --root <ABSOLUTE path to outputs/2026-10-08-l25-runtime/profile> \
  --dry-run --emit-patch review.patch
```

Inspect `review.patch`: six offset corrections + three new-fact insertions
(JSON entry, INI key, RuntimeBuildProfile row), three new joined-preflight rows
with `static_assert` 66→69, and the `tests/run_tests.py` expectation updates
(prior-93 check becomes "retained minus corrected set"; counts 69/56).
`<<PROFILE_DIGEST_NEW>>` stays unresolved at this stage by design — it is
computed from the final JSON in step 3.

## 3. Apply to the live l25 profile

```
python3 instantiate_profile_fix.py --values aa-values.json \
  --root <ABSOLUTE path to outputs/2026-10-08-l25-runtime/profile> --allow-live
```

The helper (in order): substitutes slots; applies the patch with
`/usr/bin/patch --forward --no-backup-if-mismatch -p1` (clean-apply only — any
reject or fuzz aborts, mirroring the driver's `apply_q_patch` discipline);
computes `profile_digest = sha256(json.dumps(p, sort_keys=True,
separators=(',',':')))` from the patched JSON; writes that digest into
`units/CompiledNativeLayoutDeclarations.hpp` (string [3]) and
`units/RuntimeBuildProfile.hpp` (`kDigest`); then **rewrites `CHANGES.json`**:

- all four `files[*].candidate_sha256` rows recomputed (JSON + 3 headers);
  `original`/`original_sha256` stay the frozen l23 pins (Z9 law: rows must key
  on originals the base build actually consumed — unchanged by content edits);
- `profile_digest` updated; `required_members` 74→75; `prior_facts_retained`
  93→88 (= 93 − 5 corrected in place); new keys `listener_corrected_facts`,
  `listener_new_facts`, `listener_fix_provenance`; `joined_counts`
  69/56/11/2; `scope` gains the correction sentence; `regenerate` is replaced by
  a SUPERSEDED warning (see §6).

It then self-validates: JSON parses, INI `configparser` guard (every member
offset < its section `UEP_TotalSize`, all three listener keys present in
`[FUObjectArray]`), digest present in all three locations, 69 joined rows,
`run_tests.py` hunks present, and prints the new candidate hashes.

## 4. Re-run the profile's own tests (now expecting the corrected profile)

```
python3 outputs/2026-10-08-l25-runtime/profile/tests/run_tests.py
```

What it validates about these fields: the INI `[FUObjectArray]` values are parsed
and guarded (`NativeLayoutBinding::Prepare` rule: populated section needs
positive `UEP_TotalSize` with every member below it — the corrected offsets and
the new Critical key are inside this guard); the compiled preflight binary
evaluates the three new Required rows against fixtures built from the INI
(69 keys / 56 required now); the digest gate compiles the INI header against
`RuntimeBuildProfile.hpp` (`static_assert` "Compiled native declarations must
match the selected runtime profile digest") plus its negative control; the
retained-facts check now asserts "93 minus the corrected set" byte-identity vs
the l23 original; CHANGES pin rows re-verified against the files just written.
Expected stdout ends: `PASS host preflight10, INI total-size guard, digest pair+negative compile, strict join15 controls, prior93 immutable, l24 corroborated`
(the suite's summary line is unchanged; `RESULTS.json` records
`prior93_retained: "88 untouched + 5 FUObjectArray listener-zone corrected in place"`).
Also run the outer suite listed in CHANGES `test_commands[1]`:
`python3 outputs/2026-10-08-l25-runtime/tests/run_tests.py`.

## 5. Prove the build picks it up (profile part row → overlay, Z9 law)

From the `what/` root (CWD matters — BASE pin paths are relative):

```
cd <repo>/what
python3 outputs/2026-10-08-l25-runtime/build_variants.py --plan
```

Consumption path (verified in code): the driver reads
`HERE/profile/CHANGES.json` (`HERE`=l25-runtime) → the four rows' `candidate`
paths become `replacements` + `header_specs` (name, l23-digest) → the vfs
overlay (`merged_aliases`) redirects every dep-universe header with a matching
(name, digest) to the l25 candidates → units depending on them land in
`affected` and are recompiled in both variants → `header_route_check` proves
every reachable include spelling resolves to the candidate. The JSON row is
`lineage-data(not-compiled)` under the G2r Z9 no-op guard; the three header
rows classify via dep-universe/overlay-target exactly as before the fix (the
edit changes only `candidate_sha256`, which gate G2 re-verifies). Nothing needs
re-staging: the l25 build consumes the l25 profile dir in place (BASELINE.md §1).
Then, with coordinator authorization, `--execute`, and afterwards
`l25_packaging_driver.py --plan` (its `l25_profile_candidates()` copies ≥3
candidates + the digest into BUILD-INPUTS; manifest notes gain the new
`listener_new_facts` count).

## 6. Post-fix cautions

- **Never re-run `profile/generate_overlay.py` after the fix** — it regenerates
  from the l23 originals + Track-B receipts and would erase the correction. The
  helper already rewrites CHANGES `regenerate` to say this.
- Do not "fix" the l24 sibling profile in the same pass: the l24 build's own
  receipts own it; the profile tests only cross-check FMap/FSet + container
  rows against it, which the listener fix does not touch. A sibling correction
  is a separate, explicitly-scoped decision.
- The runtime only gains behavior through the INI (member-offset table).
  `UObjectDeleteListenersCritical`'s offset is recorded as a fact; no code
  consumes it yet (the port cannot take the engine lock — see
  typescan-r4/candidate/src/UObjectArray.cpp:391-401). Recording it is
  future-proofing + preflight consistency, not a locking fix.

## 7. If AA also measured `ObjObjectsCritical` (not covered by default)

Add it in the same three places after step 3, before step 4, following the
template's own pattern: JSON entry appended after
`FUObjectArray::UObjectDeleteListenersCritical` (offset, `width: null`,
`<<SOURCE_AA>>`-style source); INI key `ObjObjectsCritical=0x…` inserted in
sorted position (between `ObjObjects=0x10` and `OpenForDisregardForGC=0xc`);
RuntimeBuildProfile row appended; then recompute the digest + CHANGES exactly as
the helper does (or extend the FIELDS tables in `make_template.py` +
`instantiate_profile_fix.py` and regenerate — the tables are the single source).
Add `FUObjectArray::ObjObjectsCritical` to the run_tests `CORRECTED` set only if
you also add a preflight row for it (then bump 69/56 counts again).

## 8. If AA reports `UObjectDeleteListenersCritical` absent

Drop every line mentioning it from the instantiated patch BEFORE applying
(they are isolated: one JSON entry block, one INI key, one RuntimeBuildProfile
row, one preflight row) and adjust: static_assert and run_tests counts 69→68,
56→55; CHANGES `listener_new_facts` 0, `required_members` 74. Easiest correct
path: edit the `FIELDS` tables in `make_template.py` + the count constants in
`instantiate_profile_fix.py`, regenerate, revalidate on a sandbox copy exactly
as VALIDATION.md describes. Do not hand-nudge counts in two places without
re-running the profile tests.

## Rollback

The touched set is exactly the five patched files + `CHANGES.json` +
regenerated test artifacts. Restore from `profile-fix-template/baseline/`
hashes (JSON, 3 headers, run_tests.py) and re-run
`outputs/2026-10-08-l23-runtime/profile/generate_overlay.py`-equivalents is NOT
needed — the l25 candidates can be restored by re-copying from the recorded
hashes in `baseline/baseline-hashes.txt` only if you also have byte backups; if
not, `generate_overlay.py` regenerates the pre-fix l25 profile state faithfully
(it is deterministic from l23 + Track-B evidence; verify `profile_digest`
4667891671fda00328bc61e1029c86058b1f558df4c4f4ae0922aa18ddd13317 and the
BASELINE.md hashes after running it).
