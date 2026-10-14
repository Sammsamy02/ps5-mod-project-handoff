# COMPARATOR-REGEN-PROCEDURE.md
worker-luamod-lineage · 2026-10-05 · LAUNCH-11 combined build system
Scope: the EXACT sanctioned procedure to (i) refresh unit pins in
`launch11-combined/EDITIONS.json` and (ii) regenerate the LUAMOD comparator with a
DECLARED divergence after deliberate source edits, so the next composition run
passes with zero silent drift. Host-side only; no game claims.

Provenance originals captured BEFORE the controller edits (sha256,
`provenance-originals/SHA256SUMS.txt`):
- `LuaMod.cpp` 3c39a86b84428e0672a658441108dd546dc8d4cd1484fb107f6a1059927c4874
- `SeedValidation.hpp` b3d81b0f57188d0c18ff4d308890035c253625c9f3dc36d16038d38eb6552a01
- `UE4SSProgram.cpp` 75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574

---

## 0. The hash function and the pin/derived split

Hash function everywhere: `sha256(file bytes)` (hex). In the fixed edition:
`def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()`
(build_runtime_composition6_fixed_l11.py:94). Same function in the runner (`sh`),
driver (`sha256_file`), regen scripts, and finalizer.

### PINS (enforced by gates — must be refreshed after deliberate edits)
| Pin | Enforced by | Where |
|---|---|---|
| `EDITIONS['units'][*].sha256` + `EDITIONS['bridge_cli'].sha256` | `launch11_one_pass.py::verify_units()` — dies on ANY drift with the message "refresh EDITIONS.json first — never edit pins" | first gate of every runner mode (verify and --full) |
| `EDITIONS['compose']['luamod_merged'].sha256` (source pin) | `luamod_comparator_gate()` — `unit.source_sha256 != comp['sha256']` dies | runner post_build, per pass |
| `EDITIONS['comparators']['luamod_object']` (object comparator) | `luamod_comparator_gate()` — dies against receipt `object_sha256` AND re-measured object bytes; message: "plan D: STOP, never widen the comparator silently" | runner post_build, per pass |
| `retained_units_replaced ⊇ {luamod, program, native-pad-input, cppmod, settings, asmhelper-asmhelper}` | `luamod_comparator_gate()` | runner post_build |
| `EDITIONS['admissions']['pad_defer'].files{path:sha256}` | `admission_gate()` when status == ADMITTED | runner, pre --full |
| `--stage-header REL=SRC=SHA` pins inside the runner's `the_pass()` (SeedValidation.hpp b3d81b0f…, live_module_facts.hpp 917f13a7…, String.hpp 7c983db4…, NativeLoaderContext.hpp/lmf hpp measured live) | `build_overlay()` in the driver: `staged header drift: measured != pinned` refuses; `build_overlay` also refuses to shadow overlay paths | driver, per pass |
| driver `check_sha` pins: COMPOSE6_FIXED_SHA 67b005ef…, PKG_FIXED_SHA e6fbe6c5…, diagnostic staging f5e96c41…, entry c97055f4…, bridge CLI sha (from EDITIONS bridge_cli) | driver main / package_phase | driver, per run |
| 31-header DEFAULT_STAGE_HEADERS attestations (only those with a sha; the Resolution-B trio + ODR header are unpinned/ODR-gated) | `build_overlay()` | driver, per pass |
| `--replace-receipt` / `--extra-receipt` files maps | regen scripts re-measure at generation; fixed edition `load_replace_receipt` + `validate_inputs_rr` + fresh-.d coverage gates re-verify at compose | both |
| base receipt unit pins (`source_sha256`/`object_sha256` per unit in run-3 receipt) + `input-hashes.json` originals | fixed edition `check()` everywhere; rr files are the ONLY sanctioned override path (SEC32-1/2/3) | fixed edition, per compose |

### DERIVED / measured-at-pass-time (never hand-pinned)
- Per-unit `object_sha256` in the run receipt (output of the compile).
- `receipt.json` `runtime_sha256` (composed object) — the two passes measure it
  fresh and must agree (determinism gate; `EDITIONS.comparators.composed_object`
  records the policy: NO pre-pinned composed comparator).
- `undefined-delta.json` (measured each compose; added must be ∅ or
  SDK-provider-covered via --extend-undefined-baseline).
- `change-manifest.finalized.json` — regenerated per pass by
  `finalize_manifest.py` from `comp6-integration/change-manifest.json` + run
  receipt + candidate (PENDING fields filled from measurement; full-sha
  declarations must EQUAL measurement or the tool refuses).
- Informational EDITIONS fields NOT gate-enforced (documented for honesty):
  `comparators.luamod_source` (stale M-0 value dc687602…, superseded by
  `compose.luamod_merged.sha256` after the codecvt fix), `compose.initializer_armed.files`
  (regen_receipts re-measures the armed staging trio), `stage_header_pins_carried_from_l10`
  (l10 bookkeeping; the operative l11 SeedValidation pin is the runner argv one),
  the driver/fixed-edition comment pins (comments only; EDITIONS is authoritative).

---

## 1. Sanctioned procedure — refresh EDITIONS unit pins (task A-i)

Trigger: any deliberate edit of a file listed in `EDITIONS['units']` (or
bridge_cli / admissions files). Rule (from the verify_units refusal text):
"if a staging file moved or was folded, refresh EDITIONS.json first — never
edit pins" — refresh means re-measure the NEW bytes and record them with a
reason; never paste a hoped-for value.

Steps, in order:
1. Confirm the edit is deliberate and bounded (diff vs provenance; see §3).
2. Re-measure: `shasum -a 256 <file>` (== `hashlib.sha256(bytes)`).
3. Update EXACTLY the changed units' entries in `EDITIONS['units']`
   (match by `file` path; do not touch other rows). Update the `what` note to
   name the delta (e.g. "widen-loop PROSPERO setup_mods fix").
4. If the file is also pinned elsewhere in EDITIONS, refresh those fields too:
   - luamod merged source: `EDITIONS['compose']['luamod_merged'].sha256`
   - pad-defer program staging: `EDITIONS['admissions']['pad_defer'].files`
   - refresh the free-text notes (`diagnostic_build_note` style) — precedent:
     "pad_fold_note", "l11_fold_refresh", "codecvt_fix" entries.
5. Never touch: fixed/driver/runner script pins (those files themselves did not
   change), base receipt pins, other units.

## 2. Sanctioned procedure — LUAMOD comparator regen (task A-ii)

The failure mode being avoided: the runner's `luamod_comparator_gate` dies with
"LUAMOD COMPARATOR DRIFT … plan D: STOP, never widen the comparator silently".
Regeneration is legitimate ONLY as a declared divergence: new source sha +
newly MEASURED (never predicted) object comparator + a recorded reason, BEFORE
the next compose.

Comparator lineage (all recorded):
1. `813b249d…` — worker-l11-merge M-0 comparator, measured from merged source
   dc687602… under the unit's own pinned argv (only source/-o/-MF redirected).
2. `fe4c8113…` — current EDITIONS pin (`comparators.luamod_object` +
   `receipts/luamod-comparator.json`), measured from the codecvt-fix source
   3c39a86b… (local `modbridge::normalize_path_for_lua` override) by the GO
   pass run-l11-2 (candidate prx-candidate-l11-2, gate GO, determinism proven).
3. NEXT — the post-edit comparator, obtained EXACTLY as in 2 (steps below).

### 2.1 Measure the new object comparator (offline object-hash mode)
The build system's sanctioned measurement is the SEC32-3 retained-replacement
compile: the unit's OWN pinned argv from the latest verified run receipt, with
ONLY source/-o/-MF redirected. This is byte-deterministic on this host
(determinism-twin proven; path-independence proven by worker-l11-merge argv
tokens 165/168/171; re-proven 2026-10-05 by worker-luamod-lineage: offline
recompile of source 3c39a86b reproduced `fe4c8113…` EXACTLY — see
`offline-proof/sanity-current-source/measurement.json`).

```
python3 outputs/2026-10-04-zcode-team/worker-luamod-lineage/offline_object_hash.py \
  --run-dir .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-6/run-l11-2 \
  --unit luamod \
  --source outputs/2026-10-04-zcode-team/worker-l11-merge/staging/luamod-l11-merged/LuaMod.cpp \
  --label luamod-<delta-name>
```
(The tool passes `LLVM_CONFIG` from the receipt unit — the same env class the
fixed edition's `run()` uses — and writes only under
`worker-luamod-lineage/offline-proof/<label>/`.)

REQUIRED evidence set for the declared divergence:
- new `source_sha256` and new `object_sha256` from the measurement;
- UND-set proof: `llvm-nm --extern-only --undefined-only` old vs new object —
  added ∅ / removed ∅ expected (a call-site qualification adds no imports);
  the fixed edition's undefined-delta gate would refuse anything else;
- consumer symbols untouched: `_ZN2RC6Unreal20R3MarshalPumpPendingEv`,
  `_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv`,
  `_ZN2RC6Unreal9UFunction10GetFuncPtrEv` still referenced from the new object
  (plan D pairing law, re-checked live by `consumer_und_gate` at pass time).

### 2.2 Record the declared divergence (EDITIONS + receipts)
1. `EDITIONS['comparators']['luamod_object']` ← new object sha.
2. `EDITIONS['comparators']['luamod_object_note']` ← append the new lineage
   entry (edit name, what changed and why, old→new source sha, measurement
   provenance = offline_measurement label + run receipt argv identity).
3. `EDITIONS['comparators']['luamod_source']` ← refresh to the new source sha
   (currently stale dc687602; it is informational, but leaving a second stale
   sha in the comparator block is exactly how silent drift breeds).
4. `EDITIONS['compose']['luamod_merged'].sha256` ← new source sha (gate-enforced).
5. `EDITIONS['units']` luamod row ← new source sha (gate-enforced).
6. Copy the measurement into `launch11-combined/receipts/luamod-comparator.json`
   shape (the runner overwrites this at the next successful pass with the
   composed bytes — the offline file is the pre-build declaration; preserve a
   copy as `luamod-comparator.prebuild-<label>.json` so the declaration stays
   auditable after the pass).
7. Comment pins (controller-owned, comments only — EDITIONS is authoritative):
   driver L11-E3 block (source sha256 PIN / expected unit OBJECT) and the fixed
   edition header (`813b249d` reference) should be updated by the controller in
   the same fold, or annotated as superseded-by-EDITIONS.
8. NEVER valid shortcuts: re-using `8593884e…` (enqueue-only) or `3eeba02f…`
   (loadmap-r2-only) — rollback comparators only; re-using the pre-edit
   comparator; editing the runner to skip the gate; widening the
   retained_units_replaced expectation.

### 2.3 The replace-receipt regen (runner-automatic, but must cover the edit)
`launch11_one_pass.py::regen_receipts()` regenerates
`receipts/l11-replace-receipt.json` via `worker-comp6-driver/regen_replace_receipt.py`
at --full time: `--file` admissions for the merged LuaMod.cpp, CppMod.cpp,
SettingsManager.cpp, the program staging (`--program-source`), freshen-deps.txt
walk, the two W/include headers, the two scan-provider headers + ps-scan-abi
header, then merges the armed initializer trio (re-measured). Hashes are
measured AT REGEN TIME, so no manual receipt edit is needed for source edits —
the requirement is only that the edited files stay in the `--file` lists (they
already are: LuaMod.cpp via `compose.luamod_merged.file`, program via
`--program-source`). SeedValidation copies enter via freshen-deps.txt /
`launch11-combined/units/SeedValidation.hpp` --file and the initializer-dir
merge. If a fresh .d walk ever adds NEW files, append them to
`launch11-combined/freshen-deps.txt` (the fixed edition refuses unpinned fresh
deps loudly — extend, never bypass). Receipt regen renames prior receipts to
`.prev` (preserved, never deleted).

## 3. Edit verification procedure (diffs)
For each deliberate edit: `diff provenance-originals/<file>.orig <live file>`;
record hunk list + byte counts in the regen receipt; confirm the hunk matches
the declared intent exactly (no drive-by edits). Then run §1/§2.

## 4. Per-file wiring map (what each edit actually touches in the l11 build)

### 4.1 UE4SSProgram.cpp — CRITICAL WIRING FACT
The l11 runner passes `--program-source
worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp` (7b5d4636…), NOT the
worker-comp6-driver `staging/comp6-program/UE4SSProgram.cpp` (75abf2ab…) that
EDITIONS['units'] pins and the driver defaults to. The wpd1 copy = comp6-program
copy + the diagnostic-sink additions (extern `diagnostic_event` + two targeted
diagnostic blocks). The finalized manifest already records this as a CONTENT
DIVERGENCE for `changed_units[program]`.
Consequences:
- An edit that lands ONLY in comp6-program does NOT reach the l11 compose; the
  program.o / composed object stay byte-identical; only the EDITIONS units[] pin
  for comp6-program needs refreshing (verify_units).
- For the edit to take effect, the same hunk must ALSO land in the wpd1 staging
  (controller-owned file), and then `EDITIONS['admissions']['pad_defer'].files`
  must be refreshed (admission_gate re-verifies it), the replace-receipt regen
  picks it up via `--program-source`, and the new program.o can be pre-measured
  offline the same way (unit `program`, same receipt argv).
- change-manifest.json `sha256_after` for `program` references the comp6-program
  path; the loop-side manifest re-point should carry both paths or be annotated.

### 4.2 LuaMod.cpp — direct compose input
Edited file IS the compose input (`--replace-runtime-unit
luamod=…/luamod-l11-merged/LuaMod.cpp`). Regeneration = §2 in full. Also in the
regen `--file` list automatically.

### 4.3 SeedValidation.hpp — THREE byte-identical copies, only some gate the l11 build
Current world (all b3d81b0f…):
a. `launch10-build/final-units/SeedValidation.hpp` — EDITIONS units[] pin
   (verify_units). NOT read by the l11 build.
b. `launch11-combined/units/SeedValidation.hpp` — the bridge-TU-world overlay
   copy, pinned by a HARDCODED `--stage-header SeedValidation.hpp=…=b3d81b0f…`
   in `launch11_one_pass.py::the_pass()` (controller-owned executable pin) and
   admitted in regen_receipts `--file` (re-measured).
c. `worker-luamod-rebase/staging/initializer-l10-r3/SeedValidation.hpp` — the
   armed-initializer TU world (EDITIONS units[] pin + regen_receipts merge,
   re-measured).
Consequences of a policy-only edit (same struct sizes) to (a) ONLY:
- l11 build bytes unchanged; refresh the (a) pin and STOP — and REPORT that the
  policy change is not wired into the l11 build.
For it to take effect: mirror to (b) AND update the runner's hardcoded stage-
header sha (controller edit), and decide consciously whether (c) (initializer
world) carries it too (its own units[] pin + re-measured regen entry; no
initializer object comparator is pinned, so no comparator regen — but the
initializer.o changes, hence the composed object changes; both passes re-measure
it fresh and must agree).

## 5. Post-refresh pre-flight order (before the controller runs composition)
1. `python3 launch11_one_pass.py verify` (HOLD-PASS): verify_units +
   driver --verify-only (ODR + receipt coverage + edited-landed + entry
   pre-flight + 14-chain symbols). Dies on any missed pin.
2. Comparator pre-agreement: offline measurement label matches the new
   EDITIONS `luamod_object` byte-for-byte; `receipts/luamod-comparator.prebuild-*.json`
   present.
3. admission_gate preconditions: pad_defer files (incl. refreshed wpd1 sha if
   mirrored) + initializer_fold status unchanged.
4. Only then `--full`: regen_receipts (fresh receipts, .prev preserved) →
   preflight_and_verify → pass 1 → post_build gates (luamod comparator vs the
   NEW pin; consumer UND; ODR; finalize-manifest fills PENDING; verify_accessors;
   gate-v2, D1-only failure tolerated on candidate-1) → pass 2 → determinism
   twin must equal pass 1.
5. change-manifest re-point (loop-side per PLAN-E NOTE): program entry
   sha256_after + declared source annotation for the widen-loop edit; luamod
   entry added/updated (merged staging, provenance diffs, empty import_delta);
   `finalize_manifest.py` then re-fills `change-manifest.finalized.json` at pass
   time (PENDING fill or exact-sha match; refusals recorded in the .receipt.json).

## 6. What was already verified before the edits landed (worker-luamod-lineage)
- Offline object-hash mode EXISTS and is faithful: sanity measurement
  reproduced `fe4c8113e20c6739f665d015ae848ada2c2bf2e5f49332aa80a29c1f9b85465a`
  from source 3c39a86b… under the run-l11-2 pinned 172-token argv
  (`offline-proof/sanity-current-source/measurement.json`,
  `matches_receipt_pin: true`).
- Prior-state GO receipts on disk: gate-receipt-l11-prx-candidate-l11-2.json GO
  (offline gate); candidate-1 D1-only (determinism twin pending at that time) —
  both per the runner's accepted pattern.
- The three provenance originals are byte-archived with sha256 receipts.
