# INTEGRATION-CHECKLIST — layout-facts promotion → harmless-query milestone

Team 2026-10-04-zcode-team / worker-integration-checklist. This is the
CONTROLLER'S execution guide: an ordered, numbered chain from the moment the
layout facts are promoted to the moment the harmless-query milestone is passed.
Every step lists: **Do** (exact command or file operation), **Needs**
(prerequisite), **Evidence** (what it produces), **Failure** (what a failure
means), and **Detail** (which existing worker output specifies it).

Execution ownership: all composition, packaging, staging, console, and
collection steps are CONTROLLER serialized steps (16 GB Mac, one editor/heavy
process at a time; the controller is sole owner of TRANSPORT_CHECKPOINT.json
updates and the console action queue). The game LAUNCH is always a user
action. Workers' outputs cited below are proposal-only specs; this checklist
does not authorize any worker to execute them.

Conventions used throughout:

```sh
WS=/WORKSPACE/.codex/worktrees/translation-core/what
OUT=$WS/outputs/2026-10-04-zcode-team
HTTP=http://ps5-host-not-configured.invalid:8084      # pldmgr
FTP=ftp://ps5-host-not-configured.invalid:2121        # ftpsrv (auto-DECRYPTS FSELF on RETR)
```

Standing gates that survive every step below: never fake a verdict, a scan
fact, or an identity value; a refusal is a gate working, never an error to
bypass; no duplicate dispatch or failed-job replay; one launch per staged
build; "no crash" is not success.

---

## STEP 0 — Entry gate: promotion is actually recorded

- **Do:** Confirm the anchor-validation run landed with
  `promotion_gate_indicator=1` (summary seq 10: both code anchors
  `match=1` at Tick `0x33ac8c0` / SCO `0x890aa0`, `gua_pred_live`
  `Validated` under exactly ONE hypothesis — expected **A**), and that the
  controller has made the promotion CALL per SEGMENT-CORRESPONDENCE +
  worker-span-arbitration rules (the indicator is not the promotion). Fetch
  the igf4 log `/data/modbridge-ingame-facts.jsonl` via FTP into
  `$OUT/zcode-controller/console/` and archive it with a console receipt.
- **Needs:** igf4 probe `modbridge_ingame_facts.prx` (sha256
  `4aa624cc…762756`, build `igf4-4891869337b4`) staged and launched per the
  launch ritual; all downstream steps are blocked until this gate closes.
- **Evidence:** the igf4 JSONL (seq 1–10 records under one build_id), the
  promotion decision recorded in the ledger (new checkpoint entry, e.g.
  `zcode_anchor_validation_RESULTS` promotion addendum).
- **Failure:** `match=0` anywhere ⇒ relocation/thunking/different image this
  launch — promotion of M blocked; `gua_pred_live` `Refuted` ⇒ hard
  contradiction (wrong GUA or wrong hypothesis set) — G blocked, M untouched;
  `Inconclusive GOA_E_CHUNK_EXTENT_UNVERIFIABLE` ⇒ evidence unavailable —
  re-arbitrate with w0..w3 words; refusal records ⇒ safety gate fired (map
  untrusted / not covered / not readable) — each is a specific finding, never
  a crash. NONE of these may be promoted; restart the chain from a corrected
  probe build with a NEW build id.
- **Detail:** `worker-anchor-validation/REPORT.md` §"Full outcome analysis"
  and `STAGE-PLAN-ANCHOR.md` §4 (per-record expectations).

## STEP 1 — Axes + identity resolution (the `non_shipping`/`stats` scan step)

- **Do:** Resolve the FOUR identity facts for real and write them:
  ```sh
  cat > "$OUT/worker-staged-pair-emission/input/identity-scan-facts.json" <<'EOF'
  {"engine_revision": "<real scan fact>", "build_id": "<real scan fact>",
   "non_shipping": <0|1>, "stats": <0|1>}
  EOF
  ```
  Resolvers in order of authority (EMISSION-RUNBOOK §2): `engine_revision` =
  printable-ASCII revision the running module reports in-process (or the
  controller-recorded revision identity of the pinned image `decrypted-eboot.bin`,
  sha `3b6cfad4…22fc`); `build_id` = controller/user record for this exact
  prospero eboot; `non_shipping` (0|1) and `stats` (0|1) = build-configuration
  axes resolved from an in-process scan fact (non-shipping-only markers/config
  present in the module) or explicit user confirmation of the eboot flavor —
  **user input is an acceptable resolver; record who said what** in the
  receipt. The v3 MAPPING_BREAKTHROUGH does NOT determine these axes. The
  same facts must agree with the runtime's own later resolution
  (`SetResolvedNativeLayoutAxes(non_shipping, stats)` after the first scan
  pass, UnrealInitializer.cpp:832) or `PublishStagedNativeLayoutMaps` throws
  at STEP 10.
- **Needs:** STEP 0 promotion; candidates profile deliberately carries `null`
  identity ("must come from the live scan").
- **Evidence:** `identity-scan-facts.json` under the emission worker's
  `input/`; the resolution provenance note (who/what produced each value).
- **Failure:** null/empty/non-printable values are REFUSED by the assembler
  (exit 2, `REFUSED: identity incomplete: …`, no output files — proven live
  in `emission-receipt.json` via `identity-unresolved-REFUSAL-DEMO.json`).
  A guessed/defaulted value is forbidden and would poison every downstream
  artifact (identity round-trips into the emitted INI and the acceptance
  ledger).
- **Detail:** `worker-staged-pair-emission/EMISSION-RUNBOOK.md` §2;
  `input/anchor-assumptions.json` (`identity_resolution_state`).

## STEP 2 — Real verdict records (never synthetic, never hand-written)

- **Do:** Write the probe's ACTUAL verdict records to
  `$OUT/worker-staged-pair-emission/input/verdicts-anchor.json` in the exact
  worker-5 vocabulary (`probe`/`verdict`/`codes`/`layout_matched`…), copying
  the record SHAPES from `input/verdicts-synthetic-validated.json` but with
  the real records from the igf4 probe receipt (seq 8 `gua_pred_header`,
  seq 9 `gua_pred_live`, plus `uobject_item_chain` if batch2 ran). If only
  `guobject_array` is Validated so far, emit anyway: the assembler gates the
  subset (FUObjectArray/TUObjectArray; FUObjectItem once
  `uobject_item_chain` passes) and refuses the rest until batch2's
  `uobject_item_chain` + `ustruct_chain` (+ `ffield_variant` = UE5_FField)
  are Validated in-process.
- **Needs:** STEP 0 (real records exist); worker-5 vocabulary discipline.
- **Evidence:** `verdicts-anchor.json` (hash it and record the hash in the
  emission receipt of STEP 3).
- **Failure:** any hand-edited `Validated` invalidates the whole chain —
  the acceptance ledger requires exact verdict records per value (STEP 14);
  a Refuted/Inconclusive class simply stays ungated (subset emission), it
  must never be upgraded to make emission succeed.
- **Detail:** `EMISSION-RUNBOOK.md` §0 item 2, §4 (expected subset vs
  refusals).

## STEP 3 — Staged pair emission (real verdicts)

- **Do:** Run the assembler exactly as prepared (edit `--scope` only if the
  probe matched hypothesis B instead of expected A):
  ```sh
  HYPO=A   # the hypothesis the anchor probe actually matched
  python3 "$OUT/worker-staged-pair-emission/vendored/worker-layout-batch3/bridge/assemble_staged_pair_v2.py" \
    --candidates "$OUT/worker-staged-pair-emission/vendored/worker-layout-minimum/candidates/layout-candidates.profile.json" \
    --widths1   "$WS/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/coverage.json" \
    --delta     "$WS/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/coverage-delta.json" \
    --verdicts  "$OUT/worker-staged-pair-emission/input/verdicts-anchor.json" \
    --identity  "$OUT/worker-staged-pair-emission/input/identity-scan-facts.json" \
    --scope     "$OUT/worker-staged-pair-emission/input/scope-hypothesis-A.json" \
    --out-dir   "$OUT/worker-staged-pair-emission/emission-live"
  ```
  (`--scope` takes a PATH to a JSON file, not inline JSON.)
- **Needs:** STEPS 0–2; never ship the synthetic pair already in
  `emission-synthetic/` (its identity literally reads
  `SYNTHETIC-PREPARATION-NOT-A-SCAN-FACT`).
- **Evidence:** `emission-live/MemberVariableLayout.ini` +
  `VTableLayout.ini` + emission manifest; expected on the fully-validated
  chain: 8 sections / 63 member entries (FUObjectArray 0xB8(10),
  TUObjectArray 0x20(7), FUObjectItem 0x18(6), UObjectBase 0x28(6),
  UStruct 0xB0(8), UClass 0x200(13), FField 0x30(3), FProperty 0x70(10));
  VTableLayout.ini is identity-only by design; manifest refusals name the
  ungated rest. With only the GUA chain validated: the rung-2 subset
  (FUObjectArray/TUObjectArray + FUObjectItem) — the refusals list is part
  of the evidence.
- **Failure:** exit 2 + stderr reason + NO output files = the gate working
  ("identity incomplete…", "predicate … not Validated", "no Validated-class
  predicate exists (UFunction…)"). Treat as not-ready; never edit
  candidates/verdicts to force emission.
- **Detail:** `EMISSION-RUNBOOK.md` §1 (command), §4 (expected content).

## STEP 4 — Verify the emitted pair BEFORE anything is staged

- **Do:**
  ```sh
  PV="$OUT/worker-staged-pair-emission/vendored/worker-layout-batch3/dryrun/parser_verify"
  "$PV" "$OUT/worker-staged-pair-emission/emission-live/MemberVariableLayout.ini" \
        "$OUT/worker-staged-pair-emission/emission-live/VTableLayout.ini" | python3 -m json.tool
  # expect exit 0, ok:true, identity round-trip == identity-scan-facts.json,
  # entries == manifest emitted members (63 on the fully-validated chain)
  grep -v '^BuildId=' "$OUT/worker-staged-pair-emission/emission-live/MemberVariableLayout.ini" > /tmp/nc.ini
  "$PV" /tmp/nc.ini "$OUT/worker-staged-pair-emission/emission-live/VTableLayout.ini"; echo "exit=$? (want 3)"
  # regression re-check of the vendored kit:
  python3 -m unittest discover -s "$OUT/worker-staged-pair-emission/vendored/worker-layout-minimum/validation" -p 'test_*.py'   # 18 OK
  python3 -m unittest discover -s "$OUT/worker-layout-batch3/bridge" -p 'test_bridge.py'   # 14 OK (original depth-sensitive location)
  ```
  Record sha256 of both emitted files.
- **Needs:** STEP 3 outputs exist.
- **Evidence:** parser_verify JSON (`ok:true`, identity round-trip exact);
  negative control exit 3; 18+14 tests green; file hashes.
- **Failure:** `ok:false` or identity round-trip mismatch ⇒ the pair does
  not carry the resolved identity — stop, re-emit (STEP 3) after fixing the
  input, never hand-patch the INI. Negative control exiting 0 ⇒ verifier is
  broken — do not trust any verification until reconciled. Any unit test
  red ⇒ the vendored kit drifted — reconcile before staging.
- **Detail:** `EMISSION-RUNBOOK.md` §3 (all commands and expected exits).

## STEP 5 — Composition-3 spike-tree update (verify; apply only what is missing)

- **Do:** Confirm the ack bundles are already consumed (per checkpoint
  `zcode_supplier_composition_3` they ARE: bundle 01 applied to the canonical
  outputs bootstrap tree with 4/4 hashes verified; bundle 02 verified
  byte-identical and consumed via hash-attested staging copies —
  `worker-composition-3/replace-receipt.json`, `bootstrap-stage/`). If any
  later tree change invalidated that state, re-apply in this order:
  1. Apply bundle 01 (`worker-ack-integration-deep/controller-patch/01-bootstrap-replacement/`,
     `COPY-MANIFEST.json` + `runtime-ack.diff`) to the canonical outputs
     bootstrap tree; re-run `suite/build.sh` — it re-checks the drift gate
     first.
  2. Apply bundle 02 (`…/02-composition-wiring/`) to the spike tree per its
     `REPLACE-MANIFEST.json` (`activated_mod_source` stays untouched).
- **Needs:** composition-3 checkpoint facts; frozen-input discipline (the
  frozen `native-coherent-runtime-1` validate_inputs gate pins pre-ack spike
  hashes — use the hash-attested staging-copy route, never mutate frozen
  inputs in place).
- **Evidence:** apply receipts (hashes verified per file), `suite/build.sh`
  drift-gate pass, `replace-receipt.json`.
- **Failure:** drift-gate failure ⇒ the bootstrap tree moved since the
  receipt — STOP and reconcile; never force-apply over divergent content
  (verify hashes before replacing owned files; do not overwrite divergent or
  unowned content).
- **Detail:** `worker-ack-integration-deep/controller-patch/02-composition-wiring/WIRING-SPEC.md`
  §S5 items 1–2; checkpoint `zcode_supplier_composition_3` (applied state).

## STEP 6 — Recomposition WITH the query harness (the only composition work not already done)

- **Do:** The composed composition-3 object
  (`ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-3/run-1/coherent-tick-file-witness-runtime.o`,
  sha256 `e9679aa5…86350`, 366 units) does NOT yet contain the harness unit
  or the S2 arm wiring. Compose the NEXT runtime:
  - Author the arm wiring into the bootstrap/UnrealInitializer context per
    S2-ARM: the one-line
    `RC::Platform::QueryHarness::Global().ArmLayoutGate({Version::Major, Version::Minor, (uint8_t)(Version::NonShipping?1:0), (uint8_t)(Version::DebugBuild?1:0), <validated-profile StorageLayout>, "PublishStagedNativeLayoutMaps"});`
    as the statement IMMEDIATELY after `InitializeVersionedContainer();`,
    plus the disarm-on-throw catch-all wrapping the remainder of serialized
    startup (`catch (...) { RC::Platform::QueryHarness::Global().DisarmLayoutGate(); throw; }`).
    Field discipline: facts equal the values the publish call just passed;
    `storage` = the validated profile's GUObjectArray storage claim from
    worker-2's profile (substitute the profile's actual claim — `ChunkedArray`
    in S2-ARM is the 5.6-profile example); `source` label exactly
    `"PublishStagedNativeLayoutMaps"`.
  - Author the module_start RuntimeAckConfig supplier in the game-bound
    caller unit per WIRING-SPEC §S1–S3 (`status_root` = the EXACT
    `mfa_config::root` string passed to `mfa_activate`; `pid` = `getpid()`;
    `now_ms` = `clock_gettime(CLOCK_MONOTONIC)` ms helper — both symbols
    already in the frozen import contract); `owner->start()` / `owner->stop()`
    per S1.
  - Recompose:
    `build_runtime.py --extra-unit native-query-harness=$OUT/worker-query-harness/src/NativeQueryHarness.cpp --extend-undefined-baseline`
    (plus the ack/bootstrap-replace mechanism per the
    `build_runtime-bootstrap-replace.PROPOSAL.md` if the replaced bootstrap
    pair is not already the composed baseline).
  - Gate check: the harness unit contributes ZERO new undefined symbols
    (measured: 22 unit undefineds = 18 resolved by composed units incl. all
    four engine seams, 4 already in baseline; delta EMPTY — object
    `1eb11c7a…`, 70,816 B). Expected total added set = exactly the carried
    composition-2/3 delta `[sceKernelGetModuleInfo]` and nothing more.
- **Needs:** STEP 5 state applied; storage claim from the VALIDATED profile
  (post-promotion); `InstallDefaultEngineBindings()` wired exactly once
  before `RuntimeOwner::start()`; serialized Tick/ProcessEvent dispatch
  routed to `QueryHarness::Global().OnEngineTickDispatch()` after
  `NativeThreadDetail::Identity::publish_current()` (prerequisite wiring 2–3
  of S2-ARM §5).
- **Evidence:** new composed runtime `.o` + `undefined-delta.json` showing
  added == `[sceKernelGetModuleInfo]` only; composition receipt with unit
  list and hashes; new build id for the game-bound module.
- **Failure:** `undefined_delta.added` contains anything else ⇒ STOP and
  reconcile; never bypass the gate. A throwing publish path is CORRECT
  behavior (gate stays closed, every dispatch refused
  `RefusedLayoutGateClosed` and counted) — do not "fix" by pre-arming.
  Double arm throws `std::logic_error` — the placement makes it
  unreachable; if you find a second arm site, remove it.
- **Detail:** `worker-query-harness-integration/S2-ARM.md` §3–§6;
  `WIRING-SPEC.md` §S1–S5; checkpoint `zcode_supplier_composition_3`
  (what is already composed); probe artifact
  `worker-query-harness-integration/target-probe-supplier-composition-1/`.

## STEP 7 — PRX packaging

- **Do:**
  ```sh
  python3 $OUT/worker-prx-packaging/package_prx.py --verify-only
  python3 $OUT/worker-prx-packaging/package_prx.py --link
  ```
  Packaging inputs are treated as frozen (PACKAGE-PRX-INPUT-DECISION.md: no
  input-list change required — the ack and harness enter via the composed
  object, not bootstrap units). Then independent review of
  `verification-evidence/static-checks.json`: SDK stamp 0x11000000, import
  table equals the frozen contract (196-name extended / 7-verbatim lineage
  as applicable), `_DYNAMIC` not imported, explicit DT_INIT/DT_FINI, init/
  fini slots recorded, module_start entry-alignment byte evidence.
- **Needs:** STEP 6 composed object; no `GAPS.json` from the run.
- **Evidence:** fresh `prx-candidate-*/` dir (script refuses reuse/overwrite)
  with wrapped `modbridge_supplier_composition.prx` (fake-self magic
  `4f153d1d`), `build-receipt.json` recording sha256/bytes, static-checks
  reviewed.
- **Failure:** rc != 0 or `GAPS.json` present ⇒ reconcile first; NEVER stage
  a candidate whose run produced GAPS. Static-check deviation ⇒ do not
  stage; the SDK-stamp class of failure is known (0x80020063 history).
- **Detail:** `worker-prx-packaging/STAGING-CHECKLIST.md` §1, §6;
  `WIRING-SPEC.md` §S5 item 4; `PACKAGE-PRX-INPUT-DECISION.md`.

## STEP 8 — Staging (console, controller; user-coordinated)

- **Do:** Under the standing console authorization:
  1. Back up any currently staged module file first (reversible-op rule,
     e.g. `/data/plugins/<existing>.<tag>.bak`).
  2. Upload the wrapped candidate to a NEW `/data/plugins/<module>.prx`
     (never overwrite the frozen `modbridge_runtime_aligned_entry.prx` or
     any prior module); if reusing the `modbridge_supplier_composition.prx`
     name from its own prior generation, replace ONLY after hash-verifying
     the pre-image.
  3. `ploader.ini` (`/data/plugins/ploader.ini`):
     ```ini
     [PPSA33286]
     /data/plugins/<module>.prx = 2
     ```
     (mode 2 = LATE load; section = title id).
  4. Readback + identity: FTP RETR the staged path — ftpsrv auto-DECRYPTS
     FSELF on RETR, so expect the inner ELF: first bytes `7f 45 4c 46`,
     exact inner-ELF byte size and sha256 from the packaging receipt. A
     returned `4f 15 3d 1d` proves NO decryption (oracle FAIL).
  5. Note the expected log path from the candidate profile (e.g.
     `/data/townfall_supplier_composition.log`) and ensure it is FRESH
     (stale logs are not evidence).
- **Needs:** STEP 7 verified candidate; game CLOSED.
- **Evidence:** readback hash == receipt hash; ini line present; backup
  recorded.
- **Failure:** readback hash mismatch ⇒ stop, do not launch; re-stage and
  re-verify. Oracle returning wrapped magic ⇒ decryption oracle broken —
  stage nothing else until understood. Never improvise ini edits beyond the
  documented shape.
- **Detail:** `worker-prx-packaging/STAGING-CHECKLIST.md` §2–§4;
  `worker-anchor-validation/STAGE-PLAN-ANCHOR.md` §2 (decryption-oracle
  mechanics, same ftpsrv behavior).

## STEP 9 — Launch ritual (empirically nailed; do not improvise)

- **Do:** (1) Game fully CLOSED — no Townfall process, no suspended app.
  (2) ploader armed by the user (`ploader.elf` in Payload Manager, or
  `GET $HTTP/loadpayload:/data/pldmgr/payloads/ploader/ploader.elf`).
  (3) USER launches Townfall — never automated. (4) Expect injection
  ("injecting 2 plugins", OK/FAIL) ~1 minute in; records appear from that
  point. (5) Freshness oracle: mtime of the module's log/jsonl path.
  ONE launch per staged build; per-launch re-send requirement means mode-2
  entries must be re-staged for each subsequent launch.
- **Needs:** STEP 8 verified staging.
- **Evidence:** ploader surface state; fresh log/jsonl mtimes; loaded
  module proven from log evidence, not from the send command's return.
- **Failure:** FAIL or absent log ⇒ collect ploader surface text + log tail
  via FTP; do NOT relaunch repeatedly without a changed implementation or a
  specific open question. File absent ⇒ load or pre-open startup failure.
  Only seq-1 `entry` ⇒ load+channel OK, later failure localized.
- **Detail:** `worker-anchor-validation/STAGE-PLAN-ANCHOR.md` §3; entry
  triage §4.

## STEP 10 — In-process chain: axes resolution → pair ingestion → PUBLISH

- **Do:** Nothing external — this is the staged runtime executing serialized
  startup; the controller only observes. Expected order (all inside the
  composed module): first scan pass resolves the axes in-process ⇒
  `SetResolvedNativeLayoutAxes(non_shipping, stats)` (UnrealInitializer.cpp:832);
  `UE4SSProgram::load_unreal_offsets_from_file()` (UE4SSProgram.cpp:908)
  stages the two emitted files from the bundle keys (worker-activation-bundle
  substitutes the emitted pair for `blocked_on_layout_data` — INTEGRATION-MAP
  SEAM 2); version facts set (UnrealInitializer.cpp:695-698);
  `InitializeVersionedContainer()` selects `NativeProvidedVirtualVC` on
  Prospero (Container.cpp.patch) ⇒ `set_virtual_offsets()` ⇒
  `PublishStagedNativeLayoutMaps((uint32_t)Version::Major, (uint32_t)Version::Minor,
  Version::NonShipping, Version::DebugBuild)`.
- **Needs:** STEP 6 composition (publish reached through the existing
  production-adjacent path — no raw-API caller added); STEP 9 launch;
  emitted pair present in the bundle keys; resolved axes agreeing with the
  STEP 1 identity facts (else publish throws — by design).
- **Evidence:** staged maps become process-lifetime singletons under
  `BindingMutex`; startup proceeds past the old LAYOUT-GATE hard-stop
  (`load_unreal_offsets_from_file()` / `StageNativeLayoutTexts`) — rung-4
  pass criterion; the Item Spawner mod starts unchanged.
- **Failure:** `runtime_error` from the publisher on ANY refusal mode (not
  staged; axes unresolved; published-facts disagreement; staged-identity
  disagreement with resolved engine facts) ⇒ startup aborts BEFORE the arm
  line; every later harness dispatch will be refused
  `RefusedLayoutGateClosed` and counted — that counter channel is the
  negative-evidence proof. Only legal retry: same-bundle/same-facts
  re-publication after a FAILED publish. Startup is the ONLY publisher; no
  target-side re-arm path exists.
- **Detail:** `S2-ARM.md` §1 (seam + supporting wired facts);
  `worker-layout-minimum/validation/RUNBOOK.md` §4 (activation pass
  criteria); `worker-activation-bundle/INTEGRATION-MAP.md` SEAM 2.

## STEP 11 — Query harness arm (the S2 line executing)

- **Do:** Verify from evidence (not assumption) that the arm line ran
  immediately after a genuinely successful publish, and the gate is armed:
  engine_major/minor = `Version::Major/Minor`; `non_shipping`/`stats` = the
  same axes published; `storage` = validated-profile claim; `source` =
  `"PublishStagedNativeLayoutMaps"`. The catch-all with
  `DisarmLayoutGate()` wrapped the remainder of startup.
- **Needs:** STEP 10 publish returned (a throwing publish leaves this line
  unreachable — correct).
- **Evidence:** in the snapshot record (STEP 13): `gate_source`
  == `"PublishStagedNativeLayoutMaps"`, `refused_layout_gate_closed == 0`
  on the accepted path; `ticks_accepted` begins advancing.
- **Failure:** `refused_layout_gate_closed > 0` ⇒ the publish threw or the
  arm line was mis-placed/removed — the gate never armed; treat as an
  integration defect, reconcile the composed unit, do NOT re-arm at another
  site. `ArmLayoutGate` throwing `std::logic_error` ⇒ double arm — a wiring
  bug to fix in source, never to catch-and-continue.
- **Detail:** `S2-ARM.md` §2–§4 (placement guarantees, disarm-on-throw).

## STEP 12 — Tick delivery (identity + gate-checked dispatch)

- **Do:** Confirm the serialized game-thread Tick/ProcessEvent dispatch (the
  callback that runs after `NativeThreadDetail::Identity::publish_current()`)
  is calling `QueryHarness::Global().OnEngineTickDispatch()` on that thread,
  and that `InstallDefaultEngineBindings()` ran exactly once before
  `RuntimeOwner::start()` (binds `IsGameThreadInitialized` /
  `IsInGameThreadRaw` / `GetGameThreadIdRaw` / `UObjectArray::GetNumElements`).
  Rung-1 witness: the tick hook delivered a real callback on the game thread
  with the label mapping recorded (code-shape probe corroborates only,
  expected `Inconclusive` by design).
- **Needs:** STEP 11 gate armed; engine identity published by the first
  genuine `UGameEngine::Tick`.
- **Evidence:** accepted ticks accumulate (`ticks_accepted`); dispatch
  counters move; refusal counters (`refused_wrong_thread`,
  `refused_identity_unknown`) show LIVE non-game-thread attempts being
  refused and counted — required evidence, not noise.
- **Failure:** `refused_identity_unknown` on the game thread ⇒ tick routed
  before identity publication or on the wrong thread — ordering defect.
  Zero refusals anywhere usually means the dispatch routing never fires —
  check the wiring, not just the happy path.
- **Detail:** `S2-ARM.md` §5 (prerequisite wiring 1–3);
  `worker-layout-minimum/validation/RUNBOOK.md` §1 (rung 1 criteria).

## STEP 13 — GetNumElements query (controller driver, non-game thread)

- **Do:** On the controller driver thread (never the game thread): pull the
  terminal record with `TrySnapshot` and the refusal evidence with
  `SnapshotDispatchCounters`; emit JSONL via `FormatRecordJsonLine` into the
  existing console evidence path (`outputs/2026-10-04-zcode-controller/console/`,
  CONSOLE-LOG receipt pattern). The count itself comes from the bound
  `FUObjectArray::GetNumElements` seam across two consecutive genuine
  ticks, staged by the harness's no-leak rule.
- **Needs:** STEP 12 ticks accepted; runtime still alive (query BEFORE
  `RuntimeOwner::stop()`).
- **Evidence:** one terminal `status:"completed"` JSONL line with
  `count_source:"engine_uobject_array"`, `counts.recorded:2`, both counts,
  `monotonic` verdict, `identity_checks`, refusal counter block,
  `gate_source`, axes and engine facts as published.
- **Failure:** `status:"inconclusive_below_threshold"` (exactly 10000) or
  `decreasing` ⇒ honest negative evidence, recorded as such — it does NOT
  advance the rung and is NOT a harness failure; do not re-rate unchanged
  evidence. `refusals_between_counts > 0` ⇒ the staged count was discarded
  by design (no-leak rule) — investigate the intervening refusal.
- **Detail:** `S2-ARM.md` §5 item 3; acceptance bar
  `worker-query-harness-integration/REPORT.md` §4 items 1–2.

## STEP 14 — Acceptance: harmless-query milestone

- **Do:** Grade against the FULL rung-1→2 bar — ALL six must hold from the
  REAL Townfall process through the console evidence path:
  1. terminal `completed` record, `count_source=engine_uobject_array`,
     `counts.recorded=2`, BOTH counts strictly > 10000, `monotonic`
     equal-or-non-decreasing across two consecutive genuine ticks
     (`refusals_between_counts == 0`);
  2. `identity_checks=2` — both reads under the same non-zero
     `game_thread_id_key` consistent with the published identity;
  3. refusal counters live: ≥1 real `refused_wrong_thread` (or
     `refused_identity_unknown`) from a genuine non-game-thread attempt,
     AND `refused_layout_gate_closed == 0` on the accepted path;
  4. facts match the validated profile: engine 5.6, axes equal the resolved
     scan axes as published, storage equals the validated profile's claim,
     `gate_source="PublishStagedNativeLayoutMaps"`;
  5. lifetime preserved: `RuntimeOwner::stop()` returns true, joins, reaches
     Stopped/Failed within the bounded window; harness frozen post-terminal
     (`ticks_accepted` stays 2, no further engine reads) — the ack file at
     `<status_root>/runtime-status.txt` must show the terminal state
     (`stopped` line from `owner->stop()`, at most one);
  6. provenance chain intact: validated layout origin (multi-anchor
     correspondence incl. segment extents), bounded accessor widths,
     confidence ≥ threshold, raw provenance preserved (igf4 log, verdicts
     file hash, emitted-pair hashes, composition + packaging receipts).
  Then append the acceptance ledger rows (one per value actually used:
  exact verdict record, span_digest, image identity sha
  `3b6cfad4…22fc`, worker-5 vs mirror agreement, winning hypothesis, rung
  served) to `worker-layout-minimum/validation/ACCEPTED.md` — nothing else
  counts as acceptance.
- **Needs:** STEPS 10–13 all evidenced; runtime already stopped (item 5
  graded after stop).
- **Evidence:** ACCEPTED.md rows + the JSONL terminal record + refusal
  snapshot + ack status file + full receipt chain.
- **Failure:** any single unmet condition ⇒ milestone NOT passed; the
  record stays honest negative evidence. Never claim the milestone from
  drafts, host tests alone, or "no crash". Values whose chain is only
  Refuted/Inconclusive stay candidates; unchanged re-rating without new
  evidence is prohibited.
- **Detail:** `worker-query-harness-integration/REPORT.md` §4 (the six
  conditions, verbatim bar); `worker-layout-minimum/validation/RUNBOOK.md`
  §5 (ledger discipline); `WIRING-SPEC.md` §S3–S4 (ack file + exclusivity).

---

## Failure-escalation map (one line per gate)

| Symptom | Meaning | First action |
|---|---|---|
| Assembler exit 2 | gate refused (identity/verdict/class) | resolve the named input; never force |
| parser_verify exit 3 on negative control | correct verifier behavior | proceed (this is the WANT) |
| publisher `runtime_error` | axes/identity/staging refusal | read the refusal mode; startup correctly aborted pre-arm |
| `RefusedLayoutGateClosed` counted | gate never armed | find why publish threw or arm line missing |
| `RefusedIdentityUnknown` on game thread | ordering defect | fix tick-routing order, recompose |
| counts == 10000 or decreasing | honest inconclusive | record; needs new evidence, not re-rating |
| GAPS.json at packaging | link/import gap | reconcile before anything stages |
| RETR returns `4f 15 3d 1d` | decryption oracle fail | stop staging; investigate ftpsrv |
| log file stale/absent after launch | load/startup failure | collect surfaces; one changed attempt max |

## Already-done (do NOT redo) — as of this checklist

- Bundle 01 + bundle 02 consumed; composition-3 composed
  (`e9679aa5…86350`, added `[sceKernelGetModuleInfo]` only).
- Emission kit vendored and verified (18+14 tests), refusal points proven
  live; synthetic pair exists for diffing ONLY, never to stage.
- Harness extra-unit compile probe vs supplier-composition-1: delta EMPTY.
- igf4 anchor probe built, frozen, deterministic (run1==run2); its STAGING
  is a separate console step that precedes STEP 0.

## Explicitly out of scope of this milestone

Rung 3 commission predicates (UStruct/UClass/FField/FProperty beyond the
gated subset), FName construction gap, UFunction/spawn (rung 5), installer
E2E (`worker-e2e-console-staging/STEP-BY-STEP.md` is a separate run), and
any promotion of "loaded without crashing" as success.
