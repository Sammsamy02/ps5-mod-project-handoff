# worker-2-layout-facts REPORT — Unreal build/layout fact validation for Townfall

Date: 2026-10-04. Output area (the only place this worker wrote):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-2-layout-facts/`

## 1. What was produced

| File | Purpose |
|---|---|
| `CLAIM.json` | Ownership, read-only inputs with sha256, dependencies, non-goals |
| `schema/unreal-build-profile.schema.json` | Data-driven build/layout profile contract (schema v1) |
| `profiles/townfall-ps5-ue56.profile.json` | Populated Townfall profile: only evidence-traceable facts |
| `tools/validate_profile.py` | Host validator + generic build identification (stdlib only) |
| `tools/__init__.py` | package marker |
| `tests/test_validate_profile.py` | 39 tests; all pass (`python3 -m unittest discover -s tests`) |
| `REPORT.md` | this report |

CLI usage:
```
python3 tools/validate_profile.py --schema schema/unreal-build-profile.schema.json \
  --profile profiles/townfall-ps5-ue56.profile.json
# -> VALID (exit 0)
python3 -m unittest discover -s tests   # -> OK (39 tests)
```

No shared source, survey output, capture, checkpoint or credential file was
modified. No network or console contact occurred. Nothing was executed on the
PS5 or against the game image; all work is host-side fact organization.

## 2. Fact base and per-fact provenance

Primary evidence is the frozen static survey
`.superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/run-release-1/`
(receipt sha256 `042eddc2…66a25`, stdout sha256 `c7f81581…17c60`) run against
the saved capture `.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(194241424 bytes, sha256 `3b6cfad4…22fc`, read-only on-disk capture; capture.json
sha256 `7709994b…810a6`). The receipt pins the resolver sources by hash; this
worker re-verified `lib.rs` (`aebc34f8…fd40`) and `guobject_array.rs`
(`5aef7f68…95a0`) still match those pins.

Endpoint status semantics (from resolver `lib.rs`, cited as evidence E-resolver-lib):
0 success, 1 invalid args, 2 malformed image, 3 no match, 4 ambiguous, 5 other.
Every successful output is an operand-derived serialized file-image label
(vaddr in the saved ELF's section/PT_LOAD space) — "never read, called or
certified to be a native engine singleton". On status!=0 lines, `linked=0x0`
is the failure sentinel, not an address; the profile stores null there.

### Static survey facts (all addresses are `elf_image_vaddr` file-image labels)

| Family / fact | Status | Value | Exact evidence (stdout line) |
|---|---|---|---|
| EngineVersion/engine_version | validated-static | major 5, minor 6 | `engine_version status=0 major=5 minor=6` |
| GUObjectArray/guobject_original | validated-static | `0x93ba110` | `guobject_original status=0 linked=0x93ba110 elapsed_ms=953` |
| GUObjectArray/guobject_cfg_candidate | candidate-only | `0x93ba110`, alloc `0x483b40`, free `0x4840b0`, flags 63 | `guobject_cfg_candidate status=0 linked=0x93ba110 alloc=0x483b40 free=0x4840b0 flags=63` |
| FName/fname_ctor_original | absent | — | `fname_ctor_original status=3 linked=0x0 elapsed_ms=553` |
| FName/fname_to_string | absent | — | `fname_to_string status=3 linked=0x0 elapsed_ms=10` |
| FName/fname_ctor_call_candidate | candidate-only | `0x23d590` | `fname_ctor_call_candidate status=0 linked=0x23d590 elapsed_ms=497` |
| GMalloc/gmalloc_original | absent | — | `gmalloc_original status=3 linked=0x0 elapsed_ms=160` |
| GMalloc/gmalloc_deletefile_candidate | candidate-only | `0x9350950` | `gmalloc_deletefile_candidate status=0 linked=0x9350950 elapsed_ms=85` |
| UObject/static_construct_object | validated-static | `0x490aa0` | `static_construct_object status=0 linked=0x490aa0 elapsed_ms=174` |
| UObject/uobject_layout_offsets | not-surveyed | — | no numeric member offsets exist in any evidence |
| GameEngineTick/gameengine_tick | validated-static | `0x2fac8c0` | `gameengine_tick status=0 linked=0x2fac8c0 elapsed_ms=308` |
| FProperty/fproperty_static_survey | not-surveyed | — | no endpoint exists in the saved survey |
| UFunction/ufunction_static_survey | not-surveyed | — | no endpoint exists in the saved survey |

Family statuses (validator-recomputed): EngineVersion validated; GUObjectArray
validated (address corroborated by two independent endpoints); UObject
validated (symbol label only); GameEngineTick validated (label only); FName
candidate-only; GMalloc candidate-only; FProperty and UFunction not-surveyed.
`evidence_flags=63` is recorded raw: bounded CFG feature bits whose individual
meanings are undocumented — never presented as dataflow proof.

### Compiled-port width facts (accessor contract, NOT game ABI)

From `ue4ss-native-layout-widths-1/coverage.json` (sha `951925e4…89d5a`,
419 canonical entries, 303 available, "actual PS5 SDK … not runtime
execution") plus the widths-2 delta (sha `c49f4874…cec9`; its
`base_coverage_sha256` equals the widths-1 hash — cross-checked), giving
304/419. Per-family availability recorded in the profile:
UObjectBase 5/5 (ObjectFlags 4, InternalIndex_Private 4, ClassPrivate 8,
NamePrivate 8, OuterPrivate 8); FProperty 9/10 (PropertyFlags unavailable);
FField 2/5; UFunction 10/11 (Func 16 via conservative max of alternatives
[16,8]; FunctionFlags unavailable); FUObjectItem 10/10; FName 0 entries —
FName storage width has no compiled-accessor evidence and must not be defaulted.

### Saved negative fact preserved

FNameToString's old SkySphereMesh anchor is absent from this build (RUNTIME_MAP
"STATIC ENGINE SURVEY", NEXT_ACTIONS item 3). The engine-owned Kismet
Conv_NameToString fallback remains the plan of record; recorded under
`plans_of_record` with that provenance. No new no-match investigation was run.

## 3. Schema design decisions

- Values in profiles, rules in schema+tool: family/fact vocabularies, status
  enums, width policy, target registry and provenance-class lists are loaded
  from `schema/unreal-build-profile.schema.json`; `tools/validate_profile.py`
  contains zero game-specific constants.
- Origin on every fact: artifact id + exact stdout line (or receipt pointer);
  artifacts are sha256-pinned where they are files this worker hashed.
- Address model with four DISTINCT fields: `elf_image_vaddr` (only kind the
  static survey produces), `file_byte_offset` (only with a recorded
  program-header derivation — none exists, so null everywhere),
  `mapped_runtime_address`, `load_base` (only with game-runtime evidence —
  none exists, so null everywhere).
- Provenance classes: allowed {game-static-survey, compiled-port-accessor,
  game-runtime}; forbidden {pdb-default, other-game-build, assumed, guessed,
  unattributed} — rejected anywhere they appear in the document.
- Widths: allowed {1,2,4,8,16}; 16 requires the alternatives record with
  max==16; unavailable = null+reason, never 0.
- Family status aggregation is recomputed and must match the declaration;
  EngineVersion fact must equal identity axes; shared addresses between a
  validated and a candidate fact require an explicit corroboration note
  (catches silent conflicts too, since disagreeing addresses simply fail to
  corroborate and surface as separate facts).
- Readiness gating: `static_layout_ready` needs engine_revision + build_id +
  both build axes resolved (mirrors NativeLayoutBinding
  SetResolvedNativeLayoutAxes/PublishStagedNativeLayoutMaps);
  `runtime_binding_ready` cannot be true on current evidence.
- Generic build identification (`identify_build`): matches strictly by axes
  (target, pointer_size, engine_major/minor, optional title). Ambiguity is an
  error; there is no closest-profile fallback; required axes are target and
  engine_major. Demonstrated on a synthetic valid UE 4.27 profile (tests) —
  recognized without any Townfall value in code.

## 4. Explicit unresolved list (also embedded in the profile)

Engine revision/patch/changelist; staged-layout build_id; build axes
non_shipping/stats; all mapped runtime addresses and the live load base;
file-byte-offset derivations; UObject member offsets (incl. UEP_TotalSize);
GUObjectArray/FUObjectArray member layout offsets; FName storage width and
FNamePool/FNameEntry layout; FProperty/UFunction game-side symbols, offsets
and callable ABI; GMalloc original (absent); FName ctor original (absent);
live singleton validation, game-thread dispatch and hook delivery.

## 5. Integration contract for the controller

1. Treat `profiles/townfall-ps5-ue56.profile.json` as the single source of
   Townfall build facts. Consumers must read `family_status` and never use a
   `candidate-only` fact where validated evidence is required.
2. Every address in the profile is an `elf_image_vaddr`. To obtain anything
   runtime-usable, a genuine live-module supplier must (a) establish image
   identity against the capture sha, (b) prove mapped segment correspondence,
   then write `mapped_runtime_address`/`load_base` with provenance_class
   `game-runtime`; the validator will reject those fields until such evidence
   exists.
3. Engine-owned Kismet Conv_NameToString stays the FName-to-string plan of
   record; do not schedule another SkySphereMesh-anchor investigation.
4. New builds: create a new profile file (copy the shape, cite real evidence);
  add families/facts only by extending the schema vocabularies first. Run the
   validator in CI-style gating (`exit 0` required).
5. The validator is deliberately usable by other workers: stdlib-only, no
   network, read-only, ~ms runtime. Mutation guards (tests) document each
   rejection rule.

## 6. Open questions / deviations

- `file_byte_offset` values were left null even though they are derivable from
  the ELF program headers: no evidence artifact records such a derivation, and
  ad-hoc derivation would violate the no-fabrication rule. The controller may
  commission a derived-offsets artifact later; the schema already supports it
  (`file_byte_offset_derivation`).
- The binding contract (NativeLayoutBinding.cpp) requires identity
  `format==1, target=="prospero-x86_64-sysv", pointer_size==8,
  engine_major>=4, engine_revision/build_id non-empty`; this profile keeps
  those as unresolved rather than inventing them, so it is a facts profile,
  not a staged-layout bundle — the two must not be conflated.
- Only the resolver endpoints that the saved survey actually ran are recorded;
  FProperty/UFunction are marked not-surveyed (not absent) to keep "probed and
  empty" distinct from "never probed".
- Bounded pass: ~30 tool calls used; no additional executable surveying was
  attempted (per instructions).
