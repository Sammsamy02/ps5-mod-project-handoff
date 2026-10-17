# Agent AM — PROFILE-FIX PATCH TEMPLATE (FUObjectArray listener-zone offsets)

Status: COMPLETE — and CONSUMED IN FLIGHT: while the template was being
finalized, agent AA's receipt landed (`agent-AA-offset-verify.md`, corroborated
by `agent-AE-variant-c.md`) and a downstream agent applied the fix to the LIVE
l25 profile via `apply_engine_binary_fix.py` (staged inside the template dir),
implementing this template's PROCEDURE §8 drop path. AM's deliverables are
therefore: the pre-fix BASELINE (attribution record), the validated mechanics,
and the post-landing verification. AM made zero live edits; live state was
touched only by re-running the profile's own `tests/run_tests.py` (deterministic,
byte-identical regeneration, verified before and after the landing).

Deliverables in `outputs/2026-10-08-l25-runtime/profile-fix-template/`:
`profile-fix.diff.tmpl` (STALE vs live now — refuses loudly on apply, verified),
`PROCEDURE.md` (banner: consumed/superseded), `BASELINE.md` (pre-fix record +
landing postscript), `VALIDATION.md` (R1/R2/R3 + landing postscript),
`instantiate_profile_fix.py`, `make_template.py`, `values.example.json`,
`baseline/`, `validate/`. Note `apply_engine_binary_fix.py` +
`aa-values-engine-binary.json` in that directory are the DOWNSTREAM agent's
files (not AM's); left in place as provenance.

Note: `agent-AL-offset-consumers.md` did NOT exist in l25-prep at start; the
offset flow below was derived from the code. The derived flow matched what the
landing exercised.

## 1. Offset flow + consumers (derived)

```
AA measures engine binary → values file
  → instantiate_profile_fix.py substitutes profile-fix.diff.tmpl
    → profiles/townfall-build-facts.json      (facts + digest source; lineage-data row, not compiled)
    → units/CompiledNativeLayoutDeclarations.hpp  [INI string 0]  ★ THE runtime member-offset table
        init: NativeLayoutBinding.hpp parses it into FUObjectArray::MemberOffsets
        → generated_include/MemberVariableLayout_SrcWrapper_FUObjectArray.hpp getters
        → typescan-r4/candidate/src/UObjectArray.cpp:602 GetUObjectDeleteListeners()
          (throws std::runtime_error if the table lacks the member) → L25 registration
    → units/RuntimeBuildProfile.hpp           (compiled kRequiredMembers + kDigest; digest-gate pair)
    → units/JoinedLayoutPreflight.hpp         (host preflight keys; template adds 3 startup-required rows)
    → CHANGES.json rewritten by the helper (4 candidate_sha256 rows, profile_digest, counts)
  → build_variants.py parts ['profile','typescan-r4','thread'] consumes
    HERE/profile/CHANGES.json (HERE = l25-runtime) IN PLACE
```

**In place vs staged:** the l25 build consumes the l25-LOCAL profile dir
(`outputs/2026-10-08-l25-runtime/profile/`) directly — that dir already IS the
staged copy (l23 originals frozen + pinned; l24 is only a corroboration sibling
whose cross-checks cover FMap/FSet only). NO staging step is needed or prepared;
the fix lands in the l25 profile dir in place. Z9 law verified: content edits
only change `candidate_sha256`; `original`/`original_sha256` l23 pins stay, so
G2r classification (lineage-data / dep-universe / overlay-target) is unchanged.

## 2. Template contents (profile-fix.diff.tmpl — 5 file sections, 14 slots)

Slots: `<<CREATE_LISTENERS_DEC/HEX>>`, `<<DELETE_LISTENERS_DEC/HEX>>`,
`<<CRITICAL_SECTION_DEC/HEX>>` (new fact `FUObjectArray::UObjectDeleteListenersCritical`),
adjacent THREADSAFE_UOBJECTS-shifted `<<PRIMARY_SERIAL_DEC/HEX>>`,
`<<BSHOULD_RECYCLE_DEC/HEX>>`, `<<FUOBJECTARRAY_TOTAL_DEC/HEX>>`,
`<<SOURCE_AA>>`, `<<PROFILE_DIGEST_NEW>>` (auto-computed by the helper).
Per-file hunks: JSON — 5 in-place offset/source corrections + 1 appended entry;
INI — 5 key corrections + Critical key inserted in sorted position; RuntimeBuildProfile —
5 row corrections + 1 appended row + kDigest slot; JoinedLayoutPreflight — 3 new
Required rows + static_assert 66→69; tests/run_tests.py — prior-93 check becomes
"retained minus CORRECTED set", counts 69/56, honest prior93_retained label.
Decimal values are the single authority (hex derived) so spellings cannot diverge.
`ObjObjectsCritical` deliberately NOT covered (no consumer; add-path documented
in PROCEDURE §7). Absent-Critical drop path: PROCEDURE §8.

## 3. Instantiation step count: 8 (per PROCEDURE.md)

0 preconditions (baseline hashes intact; variants not yet executed) →
1 fill `aa-values.json` from AA's receipt → 2 dry-run + review patch →
3 apply to live l25 profile with `--allow-live` (patch → digest chain → CHANGES
rewrite → self-validation) → 4 re-run `profile/tests/run_tests.py` + outer
`tests/run_tests.py` → 5 `build_variants.py --plan` from `what/` root (then
coordinator-authorized `--execute`, then `l25_packaging_driver.py --plan`) →
6 post-fix cautions (never re-run generate_overlay.py; don't touch l24 sibling) →
7/8 conditional add/drop paths. The helper refuses: missing fields, slot-markers
in values, member ≥ UEP_TotalSize, unclean patch (fuzz/reject), live root
without `--allow-live`.

## 4. Baseline (BASELINE.md + baseline/)

Current values: Create=104/0x68, Delete=120/0x78, Critical ABSENT,
PrimarySerial=176/0xb0, bShouldRecycle=180/0xb4, TotalSize=184/0xb8; source
"retained authenticated image/static evidence"; digest
`4667891671fda00328bc61e1029c86058b1f558df4c4f4ae0922aa18ddd13317`; counts
74/24/93/5, joined 66=53/11/2. Hashes for all 5 files + l23 original pins recorded.
**Baseline test result: PASS, exit 0** —
`PASS host preflight10, INI total-size guard, digest pair+negative compile,
strict join15 controls, prior93 immutable, l24 corroborated`
(byte-identical regeneration; zero live-tree drift). `build_variants.py --plan`
baseline: 50 gates, affected_units 162, all profile-relevant gates green
(22/22 G2-part-change-pins, G2r/G2s/G2t/G12b true); the one not-ok gate
(G2q copyin-probe unrealinitializer.base) is the driver-documented non-invariant
from parallel agent edits, pre-existing and unrelated. Disclosure: that plan run
let the driver refresh its own `outputs/2026-10-08-l25-build-variants/plans/{PLAN,VARIANT-PAIR-RECEIPT}.json`
plan artifacts (designed outputs; PACKAGING-PLAN.json untouched).

## 5. Template mechanics validation (VALIDATION.md — actual behavior)

- R1 PASS: sandbox copy + placeholder values → clean patch, CHANGES remapped +
  rehashed, digest chain consistent, and the patched `tests/run_tests.py` ran
  GREEN (exit 0). One validation-only shim needed (`ws=root.parents[2]` →
  absolute `what` path; sandbox is 2 levels deeper; no comment on that
  one-line statement chain — a commented shim broke it with NameError:old).
- R2 predictable refusal: raw unsubstituted template applies textually but the
  JSON fails `json.decoder.JSONDecodeError: Expecting value: line 1099 column 17`;
  the helper refuses earlier (missing fields / unresolved non-digest slots).
- R3 refusal: member ≥ UEP_TotalSize values rejected pre-write.
- Real bug caught by validation and fixed: literal `\n` (double-escaped by
  json.dumps) in the INI insertion — now a true newline; INI guard catches any
  regression. Also: `--remap-from` must be absolute (relative silently skips
  remap — caught via hash equality); `patch -i` resolves relative to `-d`, so
  the helper uses an absolute tempfile.

## 6. Open items for the coordinator

1. **Landed values (live now, AA-verified, AE-corroborated):**
   Create 88/0x58, Delete 104/0x68, PrimarySerialNumber 144/0x90,
   bShouldRecycleObjectIndices 148/0x94, UEP_TotalSize 152/0x98;
   `UObjectDeleteListenersCritical=0x78` and `ObjObjectsCritical=0x48` verified
   but NOT published (schema-less INI keys throw at NativeLayoutBinding.cpp:74).
   New digest `d648e85e…ab135`. My template's default Critical-INI path is
   known-hazardous for this port (documented in PROCEDURE/VALIDATION banners);
   only the drop path is sanctioned.
2. Post-landing AM verification (read-only): live `profile/tests/run_tests.py`
   exit 0 — `PASS … prior88 immutable + 5 listener-zone corrected, l24
   corroborated`; deterministic; all 4 CHANGES pin rows consistent; l23
   originals intact.
3. Still outstanding (not profile-template work): run `build_variants.py
   --plan` from the `what/` root against the NEW profile state before any
   `--execute`, and `l25_packaging_driver.py --plan`; the twin variants remain
   plan-only (G3 dir-freshness applies if they get executed first).
4. Host-test gap worth remembering: the profile suite's configparser guard has
   no NativeLayoutSchema check, so an INI key that passes host tests can still
   throw in the real runtime binding — R1's sandbox passed with a Critical key
   that would have failed live binding.
5. patch(1) caveat recorded: per-file atomic only; baseline-hash precondition +
   G2/run_tests gates are the guards against partial multi-file application.
