# WITNESS-FINAL — launch-10 post-build witness checklist (final unit set: launch10-final-b-and-v4)

Worker: worker-witness-checklist-final · 2026-10-05 · LOCAL ONLY, read-only
outside this directory. Workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`.

Supersedes the pre-registered post-build checks in
`worker-launch10-build-witness/WITNESS.md` §"Pre-registered post-build checks"
(those were written against launch10-final-a: 5 seed-guard strings, no v4, no
walkguard, no fresh-unit set, 4-artifact receipt). This checklist is what the
witness execution reads when the controller's `launch10_one_pass.py --full`
pass COMPLETES. It COMPLEMENTS `launch10_one_pass.py::verify_accessors` — it
never re-proves what the verifier already proves.

Precondition to execute at all: §4 (v4 fold) is confirmed folded by the
controller FIRST. If §4 is unconfirmed, the witness does not run.

Object under witness (both passes):
`.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/
ue4ss-native-supplier-composition-6/run-l10-{1,2}/coherent-tick-file-witness-runtime.o`
(= the run receipt's `runtime_object`; `ld -r` link of ALL units, so every
composed unit's strings appear in it). String tool: the pinned
`outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-strings`
— plain ASCII pass for narrow literals; `-e l` (UTF-16LE) for `STR()` wide
literals. Encoding class is stated per row; a string counted in the wrong
encoding is NOT evidence.

---

## 1. Composed-object string checks (run-l10-1 AND run-l10-2)

### 1a. Seed-class guard refusals — 7 strings, all PRESENT (narrow ASCII)

(Line numbers are the pre-v4-fold launch10-final-b references; the v4 fold
shifts the later ones — the FRAGMENTS are the check, not the offsets.)

| # | string (stable fragment to grep) | source of truth |
|---|----------------------------------|-----------------|
| 1 | `slot 0 g_uobject_array 0x{:X}: seed class {} (expected data-readable); refusing reads` | final-units/UnrealInitializer.cpp:779 |
| 2 | `slot 1 fname pool 0x{:X}: seed class {} (expected data-readable); refusing reads` | :850 (v4 fold keeps it verbatim) |
| 3 | `slot 3 gmalloc 0x{:X}: seed class {} (expected data-readable); refusing reads` | :799 |
| 4 | `slot 2 FName ctor 0x{:X}: seed class {} (expected exec-only call-only); refusing bind` | :951 |
| 5 | `slot 4 static_construct_object 0x{:X}: seed class {} (expected exec-only call-only); refusing bind` | :977 |
| 6 | `unmapped/prot-none gap (refused)` (SeedAddressClass::Refused name, SeedValidation.hpp) | SeedValidation.hpp seed_class_name |
| 7 | `[Seeds] REFUTED: {}. Terminal for this launch; derive afresh with new evidence, never replay.` (terminal seed_refused wrapper) | UnrealInitializer.cpp:682 |

Expected object counts: `refusing reads` = 3, `refusing bind` = 2,
`seed class` = 5 (same counting method as the launch10-build-witness gate
object measurement, where the gmalloc baseline had 0/0/0). The 7 strings =
the 5 slot refusal messages + the classifier's Refused name + the REFUTED
terminal wrapper. All 7 must be PRESENT; a composed object missing any one is
a FAIL (wrong initializer folded).

### 1b. Observed-CurrentBlock value-printing — PRESENT (both encodings)

| fragment | encoding | role |
|---|---|---|
| `(observed CurrentBlock {}, Blocks[0] 0x{:X}; published readable spans {})` | narrow | pool-shape refusal prints the OBSERVED CurrentBlock/Blocks[0]/published-count values (v4 wording; v3 said "published non-image readable spans") |
| `inside a published readable span (image or non-image) of` + `'None' anchor ok` | **UTF-16LE** (`-e l`) | success line `[Seeds] FName pool validated at ...` prints the observed values (v4 wording) |

PRESENCE of the v4 forms is the verdict; the v3 form is covered by the
retired-string absences in 1f.

### 1c. lmf extent / anchor / intervals diag events — 6 tags PRESENT (narrow)

`lmf_extent_base_low`, `lmf_extent_base_high`, `lmf_extent_end_low`,
`lmf_extent_end_high`, `lmf_anchor_count`, `lmf_intervals_found` — each
PRESENT >= 1. Source fact (runtime_start_bridge_r14_diag_spanpub.cpp:1059-1068
refusal path, :1097-1099 success path): all 6 are emitted on BOTH the
conversion-refusal and the success path through the existing
`diagnostic_event` channel; presence in the object is the witness check (which
path fires at runtime is console evidence, not a build claim).

### 1d. Supplier-diag marker — PRESENT (narrow)

- `{"marker":"modbridge.supplier-diag/v1","schema":1,"seq":` (marker+schema prefix)
- `/data/modbridge-supplier-diag.jsonl` (SUPPLIER_DIAG_PATH, diagnostic.c:171/173)

### 1e. WALKGUARD W1/W2 refusal strings — PRESENT (W1 wide, W2+reasons narrow)

| string | encoding | meaning |
|---|---|---|
| W1: `[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict (LAUNCH10-MATRIX D2). Fold RC_ASM_WALK_RX_GUARD=1 only for W_WORKS/W_FAILS_R_WORKS rows; reason in thrown message.` | **UTF-16LE** (`-e l`) | the Output::send Error line |
| W2: `[WalkRxGuard] refused: ` (thrown message prefix, followed by reason + `(walk text read not permitted; no read attempted; detail=`) | narrow | the runtime_error surfacing as the loud failed(1,<msg>) ack chain |
| reason: `guard_off_no_rx_verdict` | narrow | the ONLY runtime-reachable refusal at fold-0 |

**Reachability discipline (the negative half of this check):** the four other
reason strings — `anomaly_latched_earlier_restore_failed`,
`page_range_invalid`, `guard_depth_exceeded`, `rx_transition_denied` — WILL
appear in the object as bytes (the code is linked). They are
UNREACHABLE-AT-FOLD-0: three sit behind the `RC_ASM_WALK_RX_GUARD` gate
(entered only after the fold-on transition), and `page_range_invalid` is
unreachable from the sole call site (fixed `kWalkReadLen`, valid text
pointer). The witness must record them PRESENT-DEAD-AT-FOLD-0 and must NEVER
write them as reachable/active/armed claims. The only verdict sentence allowed
about runtime behavior is: at fold-0 every walk text read refuses loudly with
reason `guard_off_no_rx_verdict`, zero syscall text in the unit (R9 property
of WST's fold-0 gate).

Object-identity anchors for the fold (from the run receipt
`selected_runtime_units['asmhelper-asmhelper']`, see §2):
`object_sha256` MUST equal the fold-0 gate object
`81dd832bd59fab43a57c0feb0c9c15b756b2e62f74e667e320e666a2c66b4bb6`
(byte-identical reproduction now measured 5x independently) and MUST NOT
equal the fold-1 object `6604ce2d423c5d0dee4de6a089bd8141126e0fb02e7538ae08a4076d9b5af8d5`
(a fold-1 object in the composed build = inadmissible activation without a D2
verdict = witness FAIL).

### 1f. v4 validator strings — new PRESENT, v3 retired ABSENT (narrow unless noted)

PRESENT (v4): `None entry block index exceeds CurrentBlock (engine invariant
id>>16 <= CurrentBlock` · `Blocks[0] entry 0 is not the ANSI 'None' anchor
(wrong pool or corrupt block)` · `span table unavailable (bridge published no
readable spans this invocation)` · `bridge published 0 readable spans` ·
`entry block index exceeds CurrentBlock (engine invariant` · `entry extends
past CurrentByteOffset of the open block` · success span wording per 1b.

ABSENT (v3 retired — any hit = stale v3 fold = FAIL): `CurrentBlock < 1 (pool
not initialized` · `at or past CurrentBlock` · `inside the image extent
(wrong-base` · `non-image readable spans`.

| # | Check | Expected | Verdict (pass time) |
|---|-------|----------|---------------------|
| 1a | 7 seed-class guard strings | all PRESENT; 3/2/5 fragment counts | _recorded at pass completion_ |
| 1b | observed-CurrentBlock value-printing | v4 forms PRESENT (success line UTF-16LE) | _recorded at pass completion_ |
| 1c | 6 lmf diag event tags | all PRESENT | _recorded at pass completion_ |
| 1d | supplier-diag marker + path | PRESENT | _recorded at pass completion_ |
| 1e | W1/W2 + `guard_off_no_rx_verdict` | PRESENT; 4 dead reasons PRESENT-DEAD-AT-FOLD-0 only; asmhelper object == fold-0 `81dd832b…`, != fold-1 `6604ce2d…` | _recorded at pass completion_ |
| 1f | v4 strings / v3 retired strings | new PRESENT, retired ABSENT | _recorded at pass completion_ |

---

## 2. Fresh-unit verification (from the run receipts; both passes)

Source: each composition run's `receipt.json` (run-l10-{1,2}), fields
`freshly_compiled_units`, `retained_units_replaced`,
`selected_runtime_units[name]` (`retained_replaced`, `source_sha256`,
`object_sha256`, `argv`), plus `base_receipt` (+`base_receipt_sha256`) for the
argv-drift recomputation.

| # | Check | Expected |
|---|-------|----------|
| 2a | `retained_units_replaced` (each run) | contains EXACTLY the RETAINED_REPLACEABLE redirects of the current compose_argv: {`luamod`, `cppmod`, `settings`, `asmhelper-asmhelper`, `program`} — the task's fresh set {cppmod, luamod, settings, asmhelper-asmhelper} must ALL be present (program is the §3.2 retained replacement, expected alongside) |
| 2b | `freshly_compiled_units` (each run) | contains the 4 freshen units + `program` + the affected replacements incl. `unreal-unrealinitializer` (v4-folded), the 6 lmf/query extras, and the 2 bootstrap replacements |
| 2c | per-unit flags | `selected_runtime_units[n].retained_replaced == true` for each of the 4 (+program), with fresh `source_sha256`/`object_sha256` recorded; asmhelper source sha `694af0163e2738469b050565b9e82e5dc562066fc5c607221a1c7ded6a6ffa6e` (worker-launch10-walkguard-prep/staging/asmhelper-guard/ASMHelper.guarded.cpp, byte-identical to WST's) |
| 2d | freshen argv drift ONLY source/-o/-MF | witness RECOMPUTES from receipts: per unit, diff `selected_runtime_units[n].argv` (run receipt) vs the SAME unit's argv in the base receipt (`receipt.json:base_receipt` = `.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/receipt.json`): same length, drifted index set exactly {source index, `-o`+1, `-MF`+1}. The GATE already enforces this (fixed edition SEC32-3 refuses any other drift — measured precedent: run-l10-1-failed4-argvdrift); the witness records the recomputed drift sets as evidence, it does not merely trust the print |
| 2e | initializer freshness | `unreal-unrealinitializer` freshly compiled from the v4-folded final-units source (source_sha256 == the folded file's sha; see §4), object fresh in BOTH runs and byte-identical across runs (with 2d/e feeding the §3 determinism check) |

Verdict discipline: a unit in `retained_units_replaced` whose recomputed argv
drift exceeds source/-o/-MF, or a missing fresh unit, = FAIL (the pass should
have died first; a receipt claiming otherwise is a reconciliation matter, never
a re-run).

---

## 3. Verifier complement (NOT a duplicate of verify_accessors)

What `launch10_one_pass.py::verify_accessors` ALREADY proves (witness does NOT
re-run these): (1) bridge object DEFINES the three spanpub accessors
(llvm-nm exit-0-hardened); (2) combined-supplier.o does not leave them
undefined; (3) final packaging is receipt-TIED: package-profile.json
`combined_object_sha256` vs on-disk bytes; artifacts-receipt-recorded
`bridge_object`/`final_prx`/`bare_original`/`bare_sdk` hash+size enforced with
paths constrained inside the candidate; the recorded final-chain artifact's
undefined IMPORT surface excludes the accessors. Receipts:
`receipts/accessor-verification-prx-candidate-l10-{1,2}.json`, verdict PASS.

The witness ADDITIONALLY checks (each PASS/FAIL recorded per candidate):

| # | Check | Expected |
|---|-------|----------|
| 3a | Gate v2 GO x2 | `launch10-build/gate-receipt-l10-prx-candidate-l10-1.json` and `…-l10-2.json` both exist, `verdict == "GO (offline gate)"`, `failed_checks == []`, all 15 checks PASS (W1 W2 W3 I1 D1 F1 S1 P1 M0 M1 I1+ B1 B2 B3 D2) — the accessor verifier does not read the gate receipts |
| 3b | Determinism sha equality | witness recomputes sha256 of both runs' `coherent-tick-file-witness-runtime.o`: equal to EACH OTHER, to each run receipt's `runtime_sha256`, and to the one-pass print; both receipts `status == NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED` |
| 3c | Stage-header receipt pins | `worker-comp6-driver/staging/overlay-manifest.json` (rewritten fresh by build_overlay every pass): `headers['live_module_facts.hpp']` measured `sha256` == attested pin == `67170b21fc3b00947d848fa94337c94c9b324155e27010259a192bfd32bedad5` (capture capacity), AND the anchor-cap lineage `53b1188f4fef…` (anchor cap, superseded anchor-cap-only pin) recorded in the receipt chain (EDITION.json edit #4 provenance text + worker-anchor-cap-analysis + LAUNCH-DAY-10.md rev-b bullet), AND the overlay copy carries BOTH constants: `kMaxAnchorMappings == 16` (8→16) and `kMaxVmMappings == 16384` (1024→16384); `headers['SeedValidation.hpp']` pin == the FOLDED v4 sha (see §4: fixture-tested staged v4 `b84f8585…` or its documented alias-deleted derivative — the witness records which and verifies the delta vs `b84f8585…` is confined to the RETIRED-NAME alias block) |
| 3d | undefined-delta.json shape (both runs) | `expected_bootstrap_resolutions` == sorted [`bridge_capture_envelope`, `bridge_readable_span_at`, `bridge_readable_span_count`]; RAW `added` PRESERVED == those 3 + `sceKernelGetModuleInfo`; `removed == []`; `changed_bindings == {}`; `providers` marks the 3 as `<linker-provided>` and sceKernelGetModuleInfo with its pinned libkernel providers; `baseline_extended == true`. (The raw-delta preservation + separate bootstrap field is the l10 fixed-edition edit; earlier runs' delta files lack the field — only the FINAL run dirs' files count) |
| 3e | artifacts-receipt.json complete | EXISTS in BOTH candidate dirs (`prx-candidate-l10-{1,2}/artifacts-receipt.json`) with ALL 5 records non-null, each {path, sha256, bytes}: `final_prx`, `bare_original`, `bare_sdk`, `bridge_object`, `combined_object`; paths inside the candidate. (verify_accessors hash-enforces 4 of these via ties; the witness additionally asserts ALL 5 records EXIST and are non-null — combined_object via the receipt record, which the verifier checks only against package-profile) |

---

## 4. v4-PENDING marker — controller preconditions BEFORE the witness runs

The launch10-final-b units on disk at briefing time are still PRE-v4
(final-units/SeedValidation.hpp `6e8f8c4a…` carries the v3 strings; the v4
staging hashes differently). The witness executes ONLY after the controller
confirms ALL of:

1. **Engine predicate string folded** — `final-units/SeedValidation.hpp`
   replaced by worker-fname-validator-v4's v4 (DIFF.md §0): `check_pool_shape`
   with `none_id` inserted 2nd param, `base` removed, engine invariant
   `none_id>>16 <= current_block` (refusal `None entry block index exceeds
   CurrentBlock (engine invariant id>>16 <= CurrentBlock; pool not initialized
   or wrong pool/base)`), `check_entry_bounds` open-block correction, image-
   INCLUSIVE `publish_readable_spans`; retired strings absent (§1f list).
   Staged v4 sha at briefing: `b84f8585bc70ee369b7ebd1b40d70a13f4ab4926b3c0701d9bc83f29da809d28`.
2. **Entry-0 None anchor folded** — `final-units/UnrealInitializer.cpp`
   replaced by the v4 staging (7 hunks confined to the SeedArming seed path):
   `read_bytes_at` bounded 6-byte anchor read + `check_none_anchor` after the
   shape passes, terminal refusal `Blocks[0] entry 0 is not the ANSI 'None'
   anchor (wrong pool or corrupt block)`. Staged v4 sha at briefing:
   `4a2b9400b70e61ec1e2c508591d2ff4a539fff8463e5acfa8d14bab0064e979a`.
   Companion fold (bridge TU, ONE line + stale comments): call site switched
   to `publish_readable_spans`; completion marker: ZERO references to
   `publish_nonimage_readable_spans` in the folded overlay copies (if the
   alias-deletion step is deferred, the shipped state must be recorded and
   §3c's SeedValidation sha reconciliation covers it — AL1 proved the alias
   delegates byte-for-byte, so behavior is identical either way; the fold
   instruction's terminal state is zero references).
3. **Candidate-image-resident fixture PASS receipt confirmed** —
   worker-fname-validator-v4 `receipts/PASS-pool-bounds-v4-fixtures.json`:
   69 passed / 0 failed, suite `host-test/test_pool_bounds_host.cpp` sha
   `674897f05b5969cc8bf8be0381801ed05ef690127bd39e8419af3e8c6ca6d326`
   (NOTE: the PASS receipt's suite sha field reads `98774321…` — stale; the
   on-disk suite hashes `674897f0…` as REPORT.md §2 records; reconcile the
   receipt field at fold review), driving the REAL staged v4 header; MUST-PASS
   positives V1/V1b/END-TO-END/H6 = the SYNTHETIC candidate-image-resident
   shape (labeling law: region-class mapping observation, NOT a captured
   Blocks[0] pointer). The controller confirms the FOLDED header bytes are
   the fixture-tested bytes (or the documented alias-deleted derivative).
4. **Pins refreshed so the runner admits the fold** —
   `launch10_one_pass.py --stage-header SeedValidation.hpp=…=<v4 sha>` and
   EDITION.json (initializer edit sha → v4; bridge sha → post-rename value;
   edition bumped to the final-b-and-v4 identity). The runner refuses drift,
   so an unrefreshed pin fails BEFORE composition — that failure mode is
   "fold not done", not a witness matter.

Related unit set at briefing (all verified on disk, unchanged by the v4 fold):
geometryfix core `440a5786…` · diagnostic sink `f5e96c41…` · staged header
`67170b21…` · bridge r14 `663e7ce9…` (pre-rename; changes at fold item 2) ·
SeedTable.h `d183e0c0…` · ASMHelper.guarded.cpp `694af016…`.

---

## 5. What the witness must NOT expect (SPAWN-CAPABILITY.md discipline)

The composed candidate is LOAD-CAPABLE, NOT SPAWN-CAPABLE (SPAWN-CAPABILITY.md,
source-established): the EngineTick pump arm needs an exec-only text read that
fold-0 walkguard REFUSES LOUDLY. Consequences for witness verdict language:
first-OPTIONS-press silence after a successful load is EXPECTED for this
candidate, not a drain stall; the W1/W2 refusal strings in the object are the
designed NO-CRASH posture (the GOOD-class oracle outcome), never a defect
finding; visible spawn is NOT a launch-10 verdict input (oracle G4). The
witness makes build-evidence claims only — no in-game, verdict, or quality
claims, and never a ">8" quality statement.

## 6. Witness receipts (written by the pass-time execution, this directory)

- `receipts/witness-final-{run1,run2}.json` — §1 string counts (per fragment,
  per encoding), §2 fresh/retained/argv-drift recomputation, §3a-3e results.
- `receipts/witness-final-strings-{run1,run2}.txt` — raw llvm-strings narrow +
  UTF-16LE greps (bounded to the checked fragments).
- `VERDICT.md` — the §1-§3 table filled with PASS/FAIL and the exact
  composed-object shas; any FAIL quotes the failing fragment/receipt verbatim.
