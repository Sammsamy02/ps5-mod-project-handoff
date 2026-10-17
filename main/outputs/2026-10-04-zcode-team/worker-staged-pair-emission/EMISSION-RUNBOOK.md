# EMISSION-RUNBOOK — staged-pair emission the moment the anchor validation lands

Team 2026-10-04-zcode-team / worker-staged-pair-emission. Kit is self-contained
under `outputs/2026-10-04-zcode-team/worker-staged-pair-emission/`
("$OUT" below; `$WS` = workspace root). Nothing outside $OUT is written except
by explicit controller-owned capture steps. The pair currently in
`$OUT/emission-synthetic/` is a PREPARATION artifact produced under the
synthetic-validated assumption: its EngineRevision/BuildId literally read
`SYNTHETIC-PREPARATION-NOT-A-SCAN-FACT`. **Never ship or stage those bytes.**

## 0. Preconditions — only these unblock emission

1. worker-anchor-validation's probe record lands with:
   - live read of 96 bytes at `0x97ba110` (= image_base `0x400000` + profile
     vaddr `0x93ba110`, single-load-bias model, checkpoint
     `zcode_v3_MAPPING_BREAKTHROUGH`) OK;
   - live Tick `0x33ac8c0` / SCO `0x890aa0` bytes equal the retained-image
     expectations (`worker-anchor-validation/expected_bytes_tick_sco.txt`);
   - vendored batch1 GUObjectArray predicate: `Validated` under exactly ONE
     hypothesis (expected **A**) and Refuted/Inconclusive for the other.
2. The controller writes the probe's actual verdict records to
   `$OUT/input/verdicts-anchor.json` in the exact worker-5 vocabulary
   (`probe`/`verdict`/`codes`/`layout_matched`...). Copy the shapes from
   `input/verdicts-synthetic-validated.json` — but with the REAL records from
   the probe receipt. If only `guobject_array` is Validated so far, emit anyway:
   the assembler emits exactly the gated subset (FUObjectArray/TUObjectArray)
   and refuses the rest until batch2's `uobject_item_chain` + `ustruct_chain`
   (+ `ffield_variant` = UE5_FField for FField/FProperty) are Validated
   in-process. Never hand-write a `Validated`.
3. Identity facts resolved for real (see section 2) into
   `$OUT/input/identity-scan-facts.json`.

## 1. Emission (the commands)

```bash
WS=/WORKSPACE/.codex/worktrees/translation-core/what
OUT=$WS/outputs/2026-10-04-zcode-team/worker-staged-pair-emission
HYPO=A            # the hypothesis the anchor probe actually matched (A expected)

# full promotable scope; the assembler still refuses every ungated class/member
python3 "$OUT/vendored/worker-layout-batch3/bridge/assemble_staged_pair_v2.py" \
  --candidates "$OUT/vendored/worker-layout-minimum/candidates/layout-candidates.profile.json" \
  --widths1   "$WS/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/coverage.json" \
  --delta     "$WS/.superpowers/native-dependency-spike/ue4ss-native-layout-widths-2/coverage-delta.json" \
  --verdicts  "$OUT/input/verdicts-anchor.json" \
  --identity  "$OUT/input/identity-scan-facts.json" \
  --scope     "$OUT/input/scope-hypothesis-A.json"   # edit "hypothesis" if B matched
  --out-dir   "$OUT/emission-live"
```

(`--scope` takes a PATH to a JSON file, not inline JSON — see input/scope-hypothesis-A.json.)

A REFUSAL exit (2, stderr reason, no output files) is the gate working:
"identity incomplete …", "predicate … not Validated", "no Validated-class
predicate exists (UFunction …)". Treat refusal as not-ready; never edit
candidates/verdicts to force emission.

## 2. Axes and identity resolution requirement (hard gate)

The assembler emits nothing until ALL FOUR identity facts are present and
non-null. The candidates profile deliberately carries `null` for them
(identity note: "must come from the live scan"). **The mapping does NOT
resolve them** — nothing in checkpoint zcode_v3_MAPPING_BREAKTHROUGH
determines build axes. Resolvers, in order of authority:

- `engine_revision` (opaque printable ASCII): the engine source revision string
  the running module reports in-process (engine version/branch scan fact), or
  the controller-recorded revision identity of the pinned image
  (`decrypted-eboot.bin`, sha `3b6cfad4…22fc`) if the module exposes it.
- `build_id` (opaque printable ASCII): the build identifier for this exact
  eboot (controller/user record for the Townfall prospero build under test).
- `non_shipping` (0|1): build-configuration axis. Resolve from an in-process
  scan fact (e.g., presence of non-shipping-only markers/config in the module)
  or explicit user confirmation of the eboot flavor. User input IS an
  acceptable resolver — record who said what in the manifest-carrying receipt.
- `stats` (0|1): same discipline (stats-build axis).

```bash
cat > "$OUT/input/identity-scan-facts.json" <<'EOF'
{"engine_revision": "<real scan fact>", "build_id": "<real scan fact>",
 "non_shipping": <0|1>, "stats": <0|1>}
EOF
```

REFUSAL POINTS (never faked — demonstrated in emission-receipt.json):
feeding `input/identity-unresolved-REFUSAL-DEMO.json` (all null) exits 2 with
`REFUSED: identity incomplete: missing engine_revision, build_id, non_shipping,
stats — these must come from the live in-process scan (controller), never
defaults`, and creates no output. Empty/non-printable-ASCII revision or
build_id is refused the same way.

## 3. Verify before anything is staged

```bash
PV="$OUT/vendored/worker-layout-batch3/dryrun/parser_verify"
"$PV" "$OUT/emission-live/MemberVariableLayout.ini" \
      "$OUT/emission-live/VTableLayout.ini" | python3 -m json.tool
# expect exit 0, ok:true, identity round-trip == identity-scan-facts.json,
# entries == manifest emitted members (63 expected on the fully-validated chain)

# negative control (must REFUSE, exit 3):
grep -v '^BuildId=' "$OUT/emission-live/MemberVariableLayout.ini" > /tmp/nc.ini
"$PV" /tmp/nc.ini "$OUT/emission-live/VTableLayout.ini"; echo "exit=$? (want 3)"
```

Regression re-check of the vendored kit (18 + 14 tests):

```bash
python3 -m unittest discover -s "$OUT/vendored/worker-layout-minimum/validation" -p 'test_*.py'   # 18 OK
python3 -m unittest discover -s "$WS/outputs/2026-10-04-zcode-team/worker-layout-batch3/bridge" -p 'test_bridge.py'  # 14 OK (original depth-sensitive location)
```

## 4. Expected emission content (hypothesis A, fully validated chain)

8 sections / 63 member entries — FUObjectArray 0xB8(10), TUObjectArray 0x20(7),
FUObjectItem 0x18(6), UObjectBase 0x28(6), UStruct 0xB0(8), UClass 0x200(13),
FField 0x30(3), FProperty 0x70(10); VTableLayout.ini is identity-only by
design. Always refused: UFunction (classifier-only predicate). Always skipped
(no compiled contract width): UStruct::SuperStruct, UClass::ClassFlags,
UClass::ClassCastFlags, UClass::ReferenceSchema, FField::Owner/Next/
NamePrivate, FProperty::PropertyFlags. With only the GUA chain validated,
expect the rung-2 subset (FUObjectArray/TUObjectArray + FUObjectItem once
`uobject_item_chain` passes) — the refusals list in the manifest names the rest.

## 5. Hand-off

Only after section 3 passes: the emitted pair replaces the bundle's
`blocked_on_layout_data` state via `PublishStagedNativeLayoutMaps`
(worker-activation-bundle, INTEGRATION-MAP SEAM 2), then acceptance follows
worker-layout-minimum/validation/RUNBOOK.md steps 4–5 (in-process proof, then
ACCEPTED.md ledger). Record artifact hashes in the acceptance ledger; this
worker's `emission-receipt.json` documents the synthetic-preparation baseline
to diff against.
