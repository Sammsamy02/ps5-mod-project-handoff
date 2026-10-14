# REPORT — worker-emit-final-pair (2026-10-04)

## Status: DELIVERED — FINAL staged pair emitted with ALL-REAL identity, ParsePair-verified, negative controls pass

Label: **LIVE-EVIDENCE-EMITTED**. Code-anchor byte confirmation is STILL
PENDING (Tick `0x33ac8c0` / SCO `0x890aa0` 16-byte expectations, igf6 rebuild
in flight) — noted in the pair's manifest metadata and receipt; per task
directive it does NOT block emission. Once that confirmation lands, this pair
IS stageable: no other blocker remains.

Everything written by this worker is under
`outputs/2026-10-04-zcode-team/worker-emit-final-pair/` (CLAIM.json first,
this report last). All other files read-only.

## 1. Vendored chain (unmodified)

Copied byte-for-byte from `worker-staged-pair-live/vendored/` (itself
unmodified from `worker-staged-pair-emission/vendored/`):

- `worker-layout-minimum/validation/` — base assembler with hard gate
  (`assemble_staged_pair.py`), `min_predicates.py`, 18-test suite,
  example-emission fixture.
- `worker-layout-minimum/candidates/layout-candidates.profile.json`.
- `worker-layout-batch3/bridge/` — `assemble_staged_pair_v2.py` (predicate
  inventory + compiled-width authority), `width_contract.py`, `test_bridge.py`.
- `worker-layout-batch3/dryrun/` — `parser_verify` (the ParsePair Mach-O
  arm64 binary, sha256 `e9ef3d36…52d6`) + `promotable-set.json`.

## 2. GAP markers replaced with REAL values — no gaps left

`input/identity-final.json`:

| Fact | Value (REAL game evidence) | Provenance |
|---|---|---|
| engine_revision | `5.6.1-0+++UE5` | worker-layout-identity IDENTITY-RESOLUTION.md §3 — FEngineVersionBase store-site components (Major=5, Minor=6, Patch=1, branch `+++UE5+Release-5.6`, changelist 0), UE `FEngineVersion::ToString` shape; every component statically verified |
| build_id | `04bb4330a7d71dcefe1cc2c4d19769d5` | worker-layout-identity §4 — ELF `NT_GNU_BUILD_ID` note (`namesz=3, descsz=0x14, type=3, name "GNU"`), 16 significant bytes, of the exact eboot |
| non_shipping | 0 | worker-axes-detector 244-marker cost model — SHIPPING, `build_configuration=1`, margin 60 |
| stats | 0 | worker-axes-detector — all 12 `WithStats` markers absent, STATS OFF, margin 12 |

Engine = **5.6.1** (Major=5, Minor=6, Patch=1); target
**prospero-x86_64-sysv**, pointer_size **8** (worker-2-layout-facts
`townfall-ps5-ue56.profile.json`). `gap_markers_present: false` in the
receipt. The runtime still re-resolves the axes and
`PublishStagedNativeLayoutMaps(major, minor, non_shipping, stats)` refuses any
staged-vs-resolved disagreement at bind time — the offline declaration now
matches the detector verdict instead of being an unresolved 0/0 placeholder.

## 3. Predicate input — rung 1->2 ONLY

`input/verdicts-live-gua.json` inherited byte-for-byte from
worker-staged-pair-live: the single REAL record —
`guobject_array = Validated, layout A` at GUA `0x97ba110`
(`0x400000 + 0x93ba110`, single-load-bias, checkpoint
`zcode_v3_MAPPING_BREAKTHROUGH`), digest `97a16867…25072`.
`input/gua-evidence-live.json` carries the validated fields (71959 objects,
2/33 chunks, MaxElements 2162688, Objects** `0x13800220a0`, PreAllocated
NULL, GC-index triple consistent under layout A).

batch2/batch3 predicates (`uobject_item_chain`, `ustruct_chain`,
`ffield_variant`) are intentionally ABSENT — absent means refused. No
UStruct/FField/UFunction content was emitted; those need the rung-3 live
reads at `Objects**=0x13800220a0` (enumerated in
worker-staged-pair-live/RUNG3-REQUIRED-READS.md).

## 4. Emission result — exit 0, exactly the GUA-gated subset

Command (from EMISSION-RUNBOOK §1, paths redirected to this worker):

```
python3 $OUT/vendored/worker-layout-batch3/bridge/assemble_staged_pair_v2.py \
  --candidates $OUT/vendored/worker-layout-minimum/candidates/layout-candidates.profile.json \
  --widths1    $WS/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/coverage.json \
  --delta      $WS/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/coverage-delta.json \
  --verdicts   $OUT/input/verdicts-live-gua.json \
  --identity   $OUT/input/identity-final.json \
  --scope      $OUT/input/scope-hypothesis-A-live.json \
  --out-dir    $OUT/emission-final
# -> "emitted 2 class section(s) ... (hypothesis A)", exit 0
```

`emission-final/`:

- **FUObjectArray** @UEP_TotalSize 0xB8, 10 keys: ObjFirstGCIndex 0x0,
  ObjLastNonGCIndex 0x4, MaxObjectsNotConsideredByGC 0x8,
  OpenForDisregardForGC 0xC, ObjObjects 0x10, UObjectCreateListeners 0x68,
  UObjectDeleteListeners 0x78, PrimarySerialNumber 0xB0,
  bShouldRecycleObjectIndices 0xB4.
- **TUObjectArray** @UEP_TotalSize 0x20, 7 keys: Objects 0x0,
  PreAllocatedObjects 0x8, MaxElements 0x10, NumElements 0x14, MaxChunks
  0x18, NumChunks 0x1C.
- **VTableLayout.ini** identity-only by design (relocation hooks).
- Header carries the REAL identity: `EngineMajor=5 / EngineMinor=6 /
  NonShipping=0 / Stats=0 / EngineRevision=5.6.1-0+++UE5 /
  BuildId=04bb4330a7d71dcefe1cc2c4d19769d5`.
- `skipped_no_width` empty; the 9 width-contract-vs-transcription mismatches
  are recorded (affect none of the emitted subset); refused list = FUObjectItem,
  UObjectBase, UStruct, UClass, FField, FProperty (`uobject_item_chain`
  absent) + UFunction (permanent).

Diff vs the previous LIVE-GUA-VALIDATED pair: identical structure and
offsets; the only content change is the header identity — the two
`GAP-NOT-A-SCAN-FACT-*` strings replaced by the real revision/build-id.

## 5. Verification evidence (all passing)

1. **ParsePair** (`emission-final/parsepair-result.json`): exit 0, `ok:true`,
   **17 members** parsed across the 2 sections, vtables `[]` as designed,
   and the identity **round-trips EXACTLY**: format 1, pointer_size 8,
   engine_major 5, engine_minor 6, non_shipping 0, stats 0,
   target prospero-x86_64-sysv, engine_revision `5.6.1-0+++UE5`, build_id
   `04bb4330a7d71dcefe1cc2c4d19769d5`. All keys present.
2. **Negative control — null identity** (assembler gate): feeding
   `input/identity-unresolved-refusal-demo.json` exits **2** with
   `REFUSED: identity incomplete: missing engine_revision, build_id,
   non_shipping, stats — …never defaults`; no output directory created
   (`negative-controls/null-identity.stderr`).
3. **Negative control — stripped BuildId** (parser gate): `grep -v
   '^BuildId='` copy parsed by `parser_verify` exits **3** with
   `{"ok":false,"refused":"MemberVariableLayout.ini:30: Required NativeLayout
   metadata field is missing"}` (`negative-controls/stripped-buildid.stderr`).
4. **Regression**: layout-minimum suite **18 tests OK** (vendored copy);
   batch3 bridge suite **14 tests OK** at the original depth-sensitive
   location `worker-layout-batch3/bridge` (per runbook; the vendored
   `test_bridge` has the documented import-path sensitivity — a test-harness
   path property, not a code defect; the shipped v2 assembler itself ran).

## 6. Code-anchor status (explicit, non-blocking)

The Tick `0x33ac8c0` / SCO `0x890aa0` 16-byte expectations are **not yet
byte-confirmed** — the igf6 rebuild probe is in flight. Recorded in:
`input/assembly-manifest-final.json` (`code_anchor_status`),
`emission-final/assembly-manifest.json` context, this report, and the
receipt. The GUA mapping stands on span arithmetic + the corrected predicate;
exec-segment correspondence remains open. Anchor confirmation completes the
last precondition; it does not change the pair's bytes.

## 7. Exact staging commands (what goes on the console once anchors confirm)

Verification/staging station steps (`$WS` = workspace root, `$OUT` =
`$WS/outputs/2026-10-04-zcode-team/worker-emit-final-pair`):

```bash
WS=/WORKSPACE/.codex/worktrees/translation-core/what
OUT=$WS/outputs/2026-10-04-zcode-team/worker-emit-final-pair

# 1. re-verify the pair parses (expect exit 0, ok:true)
PV="$OUT/vendored/worker-layout-batch3/dryrun/parser_verify"
"$PV" "$OUT/emission-final/MemberVariableLayout.ini" \
      "$OUT/emission-final/VTableLayout.ini" | python3 -m json.tool

# 2. stage the pair where the runtime binding expects it
#    (controller-owned SEAM 2 step; identity/ownership/lease guards apply)
cp "$OUT/emission-final/MemberVariableLayout.ini" <STAGED_DIR>/MemberVariableLayout.ini
cp "$OUT/emission-final/VTableLayout.ini"         <STAGED_DIR>/VTableLayout.ini

# 3. runtime publish with the resolved facts — MUST match the INI header or it refuses
#    PublishStagedNativeLayoutMaps(major=5, minor=6, non_shipping=0, stats=0)

# 4-5. acceptance per worker-layout-minimum/validation/RUNBOOK.md steps 4-5
```

Rung 3 (reflected access: FUObjectItem, UObjectBase, UStruct, UClass, FField,
FProperty) still requires the live reads at `Objects**=0x13800220a0` — when
those land as Validated, re-run the same emission command with the successor
verdicts file to emit the full 63-member pair.

Artifact hashes are pinned in `emission-final/emission-receipt.json`.
