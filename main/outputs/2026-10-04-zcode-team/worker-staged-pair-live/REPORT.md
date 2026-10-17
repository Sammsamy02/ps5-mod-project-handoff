# REPORT — worker-staged-pair-live (2026-10-04)

## Status: DELIVERED — first LIVE-DATA staged pair emitted, ParsePair-verified, gaps explicitly marked

Label: **LIVE-GUA-VALIDATED (rung 1->2 subset only)**. This is not the
synthetic rehearsal: the predicate input is the REAL validated GUA data from
the anchor validation. Everything not established by that validation is
refused in-band by the assembler and listed as an open gap here.

Everything written by this worker is under
`outputs/2026-10-04-zcode-team/worker-staged-pair-live/` (CLAIM.json first,
this report last). All other files read-only.

## 1. Vendored chain (unmodified)

From `worker-staged-pair-emission/vendored/`, 18 files copied byte-for-byte
into `vendored/`:

- `worker-layout-minimum/validation/` — base assembler
  (`assemble_staged_pair.py`, hard gate), `min_predicates.py`, tests,
  example-emission fixture.
- `worker-layout-minimum/candidates/layout-candidates.profile.json` —
  candidate offsets (identity fields null by design).
- `worker-layout-batch3/bridge/` — `assemble_staged_pair_v2.py` (predicate
  inventory gate + compiled-width authority), `width_contract.py`,
  `test_bridge.py`.
- `worker-layout-batch3/dryrun/` — `parser_verify` (the ParsePair binary) +
  `promotable-set.json`.

Width authority used read-only from
`.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/coverage.json`
+ `widths-2/coverage-delta.json`.

## 2. Real predicate input (rung 1->2 minimum promotable set)

`input/verdicts-live-gua.json` — exactly ONE record, real data:
`guobject_array = Validated, layout A` at GUA `0x97ba110`
(`0x400000 + 0x93ba110`, single-load-bias, checkpoint
`zcode_v3_MAPPING_BREAKTHROUGH`), validated under the corrected invariant
(worker-predicate-gc-fix). Its digest in the manifest is
`97a16867…25072`.

`input/gua-evidence-live.json` — the validated field data (ObjFirstGCIndex
35477, ObjLastNonGCIndex 35476, MaxObjectsNotConsideredByGC 35477,
OpenForDisregardForGC 0, Objects** 0x13800220a0, PreAllocated NULL,
MaxElements 2162688, NumElements 71959, MaxChunks 33, NumChunks 2) with the
internal-consistency checks (Last = First-1, Max == First, 71959 <= 2162688,
2 <= 33, chunked form: capacity 2162688/33 = 65536 exact).

batch2 (`ustruct_chain`, `ffield_variant`) and batch3 walks are intentionally
ABSENT — absent means refused; nothing was inferred or hand-written.

## 3. Emission result — exactly what the GUA validation establishes

`emission-live/` (hypothesis A, exit 0, "emitted 2 class section(s)"):

- **FUObjectArray** @UEP_TotalSize 0xB8, 10 keys: ObjFirstGCIndex 0x0,
  ObjLastNonGCIndex 0x4, MaxObjectsNotConsideredByGC 0x8,
  OpenForDisregardForGC 0xC, ObjObjects 0x10, UObjectCreateListeners 0x68,
  UObjectDeleteListeners 0x78, PrimarySerialNumber 0xB0,
  bShouldRecycleObjectIndices 0xB4.
- **TUObjectArray** @UEP_TotalSize 0x20, 7 keys: Objects 0x0,
  PreAllocatedObjects 0x8, MaxElements 0x10, NumElements 0x14, MaxChunks 0x18,
  NumChunks 0x1C.
- `VTableLayout.ini` identity-only by design (relocation hooks).
- The emitted FUObjectArray/TUObjectArray offsets are precisely the words the
  live probe validated — the pair and the validation agree by construction.

Manifest `refused` list = the honest boundary: FUObjectItem, UObjectBase,
UStruct, UClass, FField, FProperty ("predicate 'uobject_item_chain' not
Validated (absent)") and UFunction (no Validated-class predicate exists —
permanent). `skipped_no_width` is empty; the 9 width-contract-vs-transcription
mismatches are recorded for transparency and affect none of the emitted
subset. Diff vs the synthetic rehearsal: synthetic carried 8 class sections
(63 members under a fully-validated assumption); live carries exactly 2.

## 4. Verification evidence (all passing)

1. **Live refusal demo** (`input/live-refusal-demo.stderr`): feeding the
   honestly-unresolved identity (all null) exits 2 with
   `REFUSED: identity incomplete … never defaults`, no output directory
   created — the gate works on live inputs, not just fixtures.
2. **ParsePair** (`emission-live/parsepair-result.json`): exit 0, `ok:true`,
   17 keys parsed, identity round-trips EXACTLY (including the two GAP
   marker strings), vtables `[]` as designed.
3. **Negative control** (`emission-live/parsepair-negative-control.json`):
   BuildId stripped -> exit 3, `refused: Required NativeLayout metadata field
   is missing`.
4. **Regression**: layout-minimum suite 18 tests OK (vendored copy); batch3
   bridge suite 14 tests OK at the original depth-sensitive location per
   EMISSION-RUNBOOK (the vendored copy of `test_bridge` has a documented
   import-path sensitivity — the shipped v2 assembler itself ran and emitted,
   so this is a test-harness path property, not a code defect).

## 5. CRITICAL — identity fields: what the binding REQUIRES vs what we HAVE

The binding requires identity at two layers:

- **Offline assembler** (`build_identity`): all four facts non-null;
  EngineRevision/BuildId nonempty printable ASCII; otherwise REFUSED
  (demonstrated live in 4.1).
- **Runtime binding** (`NativeLayoutBinding.cpp:172+`):
  `PublishStagedNativeLayoutMaps(major, minor, non_shipping, stats)` throws
  unless the staged INI's EngineMajor/EngineMinor/NonShipping/Stats agree
  with the runtime-resolved engine facts. EngineRevision/BuildId are NOT
  runtime-compared; they are staging provenance (ParsePair round-trip +
  refusal discipline).

What we HAVE as real game evidence (worker-2-layout-facts
`townfall-ps5-ue56.profile.json`): EngineMajor=5, EngineMinor=6,
target=prospero-x86_64-sysv, pointer_size=8 (game-static-survey provenance).

What remains a **GAP** (no evidence exists anywhere in the workspace):
- `engine_revision` — no engine revision string in any survey, capture, or
  in-game log. INI value: `GAP-NOT-A-SCAN-FACT-engine-revision-unresolved`.
- `build_id` — the capture sha `3b6cfad4…22fc` is saved-image provenance
  ONLY, explicitly NOT the binding-required build_id (worker-2's own note).
  INI value: `GAP-NOT-A-SCAN-FACT-build-id-unresolved`.

**Consequence**: the pair is ParsePair-valid but NOT stage-ready; staging
before the identity gaps close is forbidden (same discipline as the synthetic
runbook: placeholder-identity bytes are never shipped). The gap markers are
deliberately un-mistakeable for scan facts.

## 6. Axes (non_shipping, stats) — auto-resolved, documented

Confirmed in source: axes resolve automatically at runtime —
`SetResolvedNativeLayoutAxes(non_shipping, stats)` after the first scan pass
(UnrealInitializer.cpp:832 flow per worker-query-harness-integration
S2-ARM.md; the build_configuration scan — 0 not detected / 1 shipping /
2 development / 3 test — sets `Version::NonShipping` with a stats fallback
plus StatsMode override, UnrealInitializer.cpp ~649-630s region). The runtime
publish call then refuses any staged-vs-resolved disagreement, so a wrong
offline declaration can never silently mis-bind. The INI's offline
declaration here is 0/0, explicitly pending that automatic resolution; no
offline evidence guesses were made (worker-2 records the axes as
"unresolved-no-evidence" offline — the resolution is the runtime's job).

## 7. Code-anchor gap (explicit, per task)

The Tick `0x33ac8c0` / SCO `0x890aa0` 16-byte expectations are **NOT yet
byte-confirmed** — that probe is in flight. The GUA validation promotes the
mapping for the data segment via span arithmetic + the corrected predicate;
exec-segment correspondence remains open. The R3 read (UObject vtable at
base+0x400000 bias) will corroborate, but never substitute for, the byte
anchors.

## 8. Included vs needs-more-reads

| Content | Status |
|---|---|
| guobject_array Validated-A predicate -> FUObjectArray + TUObjectArray sections | INCLUDED (real data) |
| Identity 5/6, prospero-x86_64-sysv, pointer_size 8 | INCLUDED (real survey evidence) |
| EngineRevision / BuildId | GAP — explicit markers; needs in-process scan fact or controller/user record |
| non_shipping / stats | AUTO-RESOLVED at runtime; offline 0/0 pending, publish refuses disagreement |
| FUObjectItem members | Needs R1+R2 reads at Objects**=0x13800220a0 (RUNG3-REQUIRED-READS.md) |
| UObjectBase/UStruct/UClass | Needs R3+R4 (live item -> ClassPrivate -> SuperStruct chain) |
| FField/FProperty | Needs R5 variant settlement (UE5_FField required for the SET-5_06 table) |
| children/childproperties/uclass walks | R6, strengthening only |
| UFunction | Permanently refused (classifier-only) |
| Code anchors | Probe in flight, not byte-confirmed |

## 9. Hand-off

When R1..R5 land as Validated (and identity facts resolve), re-run the same
command from EMISSION-RUNBOOK section 1 against this worker's
`input/verdicts-*.json` successor to emit the full 63-member pair; then
`PublishStagedNativeLayoutMaps` (SEAM 2) + acceptance per
worker-layout-minimum/validation/RUNBOOK.md steps 4-5. Until then this pair
is evidence, not a stageable artifact.

Artifact hashes are pinned in `emission-live/emission-receipt.json`.
