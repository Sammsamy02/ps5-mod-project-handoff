# CENSUS.md — every FName-CONSTRUCTION-relevant fact retained in the workspace

worker-fname-construction, 2026-10-04-zcode-team. CLAIM.json was written first;
this census precedes the experiment design, adapter sketch and REPORT.

**Scope rule (P2, enforced workspace-wide):** CONSTRUCTION (text -> engine
FName / FNamePool lookup-or-insert) is tracked as its own fact family and is
NEVER satisfied by conversion evidence (FName -> string). `Conv_NameToString`,
`FNameToStringMethod = Scan`, and the absent `fname_to_string` endpoint are
conversion facts and appear here only to keep the two families separate.
Institutionalized as data in worker-5's header
(`kFNamePoolPredicateExcluded`, see E-3 below) and in worker-layout-batch2's
raw-bytes rule (E-4).

**Status vocabulary** (matches worker-2 REPORT §2 and worker-5/worker-layout-minimum
verdict conventions): `validated-static` (frozen static survey, status=0,
`elf_image_vaddr` file-image label), `candidate-only` (status=0 but evidence is
a shared encoded target / cfg feature, not an original), `absent` (status=3
no-match; `linked=0x0` is the failure sentinel, never an address),
`port-lineage` (retained coherent-port source behavior, not game-ABI proof),
`correction-receipt` (recorded ABI correction checkpoint), `unresolved`
(no evidence exists), `not-applicable` (mechanism for a different image kind).
Endpoint status semantics cited from resolver `lib.rs` via worker-2 REPORT §2:
0 success, 1 invalid args, 2 malformed image, 3 no match, 4 ambiguous, 5 other.

Primary frozen survey: `.superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/run-release-1/`
(`receipt.json` + `stdout.txt` + `stderr.txt`) run against
`.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`
(194,241,424 bytes; capture sha256 `3b6cfad4…22fc` per worker-2 REPORT §2).
Survey wiring: `ue4ss-native-file-core-survey-1/src/main.rs` (8 address
endpoints; `fname_ctor_call_candidate` bound to `ps_fname_ctor_call_file_image`).

---

## A. Static survey endpoints (origin: run-release-1/stdout.txt, line-exact)

| # | Fact | Value | Status | Origin (exact) |
|---|---|---|---|---|
| A-1 | `fname_ctor_original` | — | **absent** | stdout line 3: `fname_ctor_original status=3 linked=0x0 elapsed_ms=553`; endpoint `ps_fname_ctor_wchar_file_image` (main.rs) |
| A-2 | `fname_to_string` | — | absent (CONVERSION family — tracked separately, listed only for separation) | stdout line 4: `fname_to_string status=3 linked=0x0 elapsed_ms=10` |
| A-3 | `fname_ctor_call_candidate` | `0x23d590` (`elf_image_vaddr`) | **candidate-only** — shared encoded CALL TARGET across 5 independent anchors; "static evidence, never a callable/validated pointer" (fname_call.rs module doc, line 1-2) | stdout line 5: `fname_ctor_call_candidate status=0 linked=0x23d590 elapsed_ms=497`; endpoint `ps_fname_ctor_call_file_image` |
| A-4 | `guobject_original` | `0x93ba110` | validated-static (the anchor any name-reachability experiment must build on) | stdout line 2: `guobject_original status=0 linked=0x93ba110 elapsed_ms=953` |
| A-5 | `guobject_cfg_candidate` | `0x93ba110`, alloc `0x483b40`, free `0x4840b0`, flags 63 | candidate-only (flags recorded raw, meanings undocumented) | stdout line 10 |
| A-6 | `static_construct_object` | `0x490aa0` | validated-static (A5 consumer: its signature takes an FName) | stdout line 8 |
| A-7 | `gameengine_tick` | `0x2fac8c0` | validated-static (Signatures.cpp:238 makes Tick a requirement when a ctor Lua address exists) | stdout line 9 |
| A-8 | `engine_version` | major 5, minor 6 | validated-static (build identity axis) | stdout line 1 |
| A-9 | `gmalloc_original` / `gmalloc_deletefile_candidate` | absent `0x0` / `0x9350950` | absent / candidate-only (adjacent family; recorded for completeness — not construction evidence) | stdout lines 6-7 |

`calls_by_anchor` (the per-anchor call-site lists that produced A-3) exist in
the resolver's `Candidate` struct (fname_call.rs:8, 71-72) but are **not
serialized** in the survey stdout; materializing them needs a bounded host-side
re-run of the receipt-pinned resolver (controller decision; this worker ran
nothing). Status: candidate-metadata-not-materialized.

## B. Resolver source knowledge (origin: ue4ss-native-tick-relocation-resolver-1/crate/src; crate files hash-pinned in run-release-1/receipt.json `input_hashes`)

| # | Mechanism | Construction relevance | Status |
|---|---|---|---|
| B-1 | `resolve_ctor_elf` (fname.rs:13-96): UTF-16 anchors `Engine/Renderer/AnimGraphRuntime/Landscape/RenderCore` -> xrefs -> common root function (fLoadPreInitModules) -> pattern `ba 01 00 00 00 e8` (`MOV EDX,1; CALL`) -> rip4 target = FName ctor | The ORIGINAL construction resolver; produced A-1's no-match on this image | Ran, failed cleanly (status 3); route retained for future builds |
| B-2 | Ghidra-style reference listing in fname.rs:56-71: `RDI = &[RSP+0x28]` (stack OUTPUT), `RSI = u"Engine"`, `EDX = 1`, `CALL FName::FName` | Cross-corroborates the explicit-output (sret-style) SysV lowering from a second lineage | port-lineage + in-image static shape |
| B-3 | `resolve_ctor_pe` (fname.rs:98-182): PE-only prologue AOBs + `TGPUSkinVertexFactoryUnlimited`/`MovementComponent0` UTF-16 anchor routes | Not runnable on an ELF target | not-applicable (target image kind is ELF64 Prospero) |
| B-4 | `fname_call.rs` classifier contract (lines 24-49 `call_after`; 51-73 `resolve`): from a genuine `LEA RSI,[rip+disp32]` resolving to an anchor string, decode <=64 bytes; REQUIRE `LEA RDI,[RSP/RBP+..]` (explicit stack output), `MOV EDX, imm32 1` (EFindName Add); an intervening call/branch/RSI-clobber invalidates; collect first direct near-call target; exactly one COMMON target across all 5 anchors | The entire derivation of `0x23d590`; also the strongest in-image ABI evidence: arg0 RDI = output ptr, arg1 RSI = wchar text, arg2 EDX = find mode | candidate-only; the classifier itself is the reusable in-process predicate template for EXP-1 |
| B-5 | `resolve_to_string_elf` (fname.rs:184-195) SkySphereMesh route; `resolve_to_string_pe/void/fstring` (fname.rs:197-302) | CONVERSION family only | listed for separation; conversion is NOT this gap |

## C. Coherent port retained FName handling (origin: native-coherent-runtime-1/source/port)

| # | Fact | Origin (file:line) | Status |
|---|---|---|---|
| C-1 | FName storage shape (port): `FNameEntryId ComparisonIndex` + optional `DisplayIndex` (only `#ifdef WITH_CASE_PRESERVING_NAME`) + `uint32 Number`; class `alignas(4)` (or 8 under `FNAME_ALIGN8`) | unreal/include/Unreal/NameTypes.hpp:210-222, 207-213 | port-lineage; Townfall build flags (case-preserving? outline-number?) UNKNOWN -> game-side shape unresolved |
| C-2 | FNameEntry layout is compile-flag conditional (`WITH_CASE_PRESERVING_NAME` ComparisonId; `UE_FNAME_OUTLINE_NUMBER` FNumberedData) | NameTypes.hpp:181-200 | unresolved for this build (flags unknown; matches worker-2 profile unresolved_list) |
| C-3 | Port statics: `ToStringInternal`, `Conv_NameToStringInternal` (UFunction*), `KismetStringLibraryCDO`, and `ConstructorInternal: Function<FName(const CharType*, EFindName)>` | NameTypes.hpp:225-228 | port-lineage; `ConstructorInternal` is the seam a proven address would be assigned to |
| C-4 | `construct_with_string` copies `ComparisonIndex`/`DisplayIndex`/`Number` out of the constructor's returned value (sret consumption), with temp-address override + reset discipline | NameTypes.hpp:239-261 | port-lineage; the field-copy set is the pass-observable for EXP-3 |
| C-5 | `VerifyFNameConstructor(AddressOverride)` (UnrealInitializer.cpp:878-937): pre-scan skip; hook `RegisterFNameConstructorPostCallback`; wait on `FNameVerificationStatus` in 10ms slices under `CheckStartupWait` deadline; on stop retires ONLY its callback and resets the override ("cancellation never certifies safe unhook"); prints `FName constructor verified at 0x{:016X}` | UnrealInitializer.cpp:878-937 (INTEGRATION-MAP.md:161) | retained plan of record for in-process verification once an address exists |
| C-6 | Lua AOB route: `FName_Constructor.lua` address -> requires UGameEngine::Tick -> `VerifyFNameConstructor(address)` -> verification call `FName(STR("ByteProperty"), FNAME_Find, address)` under SEH; if `DefaultFNameToStringMethod == Conv_NameToString`, the ToString confirmation is skipped; `assign_address` only after success; errors recorded verbatim | ue4ss/src/Signatures.cpp:222-270 | retained; the Lua AOB script route is inert here (A-1 absent); the `ByteProperty`/`FNAME_Find` verification call is the model for EXP-3 |
| C-7 | `FNameToStringMethod` and all scan config flow from `UnrealInitializer::Config`; `config.ModuleSource`/`NativeScanSource` wired at UE4SSProgram.cpp:946-947 | INTEGRATION-MAP.md:157, 149-162 | retained (conversion config surface) |
| C-8 | **`fname_native_constructor_abi` correction (checkpoint):** the constructor's native lowering on this target is `void fn(ptr output, ptr text, i32 mode)` — explicit-output. Corroborated independently by B-2 and B-4 | checkpoint `fname_native_constructor_abi` via INTEGRATION-MAP.md:265-267; fname.rs:56-71; fname_call.rs:47-48 | correction-receipt (ABSENCE of this correction would make any naive by-value call corrupt the stack) |
| C-9 | Conversion do-not-regress: engine-owned Kismet `Conv_NameToString` fallback; `optional_resolver_route_correction`: FNameToString and GameEngine::Tick are OPTIONAL scan slots (required mask 0x5d); SkySphereMesh anchor absent is SAVED negative fact — do not re-investigate | INTEGRATION-MAP.md:260-264; worker-2 REPORT §"Saved negative fact" | retained (conversion family; NOT construction evidence) |

## D. Width facts (origin: compiled-port accessor coverage + worker-2 profile)

| # | Fact | Status | Origin |
|---|---|---|---|
| D-1 | **FName accessor width entries: 0** (0/…) — FName storage width has NO compiled-accessor evidence and MUST NOT be defaulted | unresolved / hard rule | worker-2 REPORT §"Compiled-port width facts" (widths-1 coverage.json 419 entries/303 available + widths-2 delta 304/419); MINIMUM-SET.md:98 `(WIDTHS FName 0/0)`; checkpoint `zcode_ue4ss_integration_rules.fname_rule` |
| D-2 | UObjectBase widths 5/5 incl. `NamePrivate` width 8 (compiled-port-accessor provenance; Townfall OFFSETS still unvalidated candidates: NamePrivate@0x18 etc.) | compiled-port width / candidate offset | worker-2 REPORT §width facts; worker-layout-minimum `min_predicates.py` UBOFF |
| D-3 | FNamePool/FNameEntry layout "entirely unresolved" — no survey endpoint exists for them; family not-surveyed rather than absent | unresolved | worker-2 REPORT §4 unresolved list; MINIMUM-SET.md:98 |

## E. Rule/consumer facts (institutional + modreqs)

| # | Fact | Origin | Status |
|---|---|---|---|
| E-1 | P2 rule: construction is its own gap; name-pool heuristics excluded | worker-5 `src/townfall_layout_probes.hpp:158-164` (`kFNamePoolPredicateExcluded = true` + reason string) | institutional |
| E-2 | Item Spawner gates on construction: A5 `StaticConstructObject(class, outer, FName)` missing-new; A6 `FName(string)` ctor with engine pool semantics — "worker-2 width/pool facts missing"; B4/B5/B6 UFunction param marshalling consumes FName slots; A3/A4 lookups consume FName keys | worker-modreqs/RUNTIME-REQUIREMENTS.md A3-A6, B4-B6, §F | consumer contract |
| E-3 | Conversion plan of record: `Conv_NameToString` (B8) unproven live; `DefaultFNameToStringMethod = Scan` in retained settings | RUNTIME-REQUIREMENTS.md B8 + §E; worker-2 REPORT §2 | conversion family (separate) |
| E-4 | `NamePrivate`/`RepNotifyFunc` are copied as RAW BYTES and never interpreted (P2 rule) | worker-layout-batch2 `src/reflection_layout_predicates.hpp:70-72` | institutional |
| E-5 | In-process predicate conventions: verdicts NotProbed/Validated/Inconclusive/Refused… (NotProbed/Validated/Inconclusive/Refuted + GOA_*/CODE_* rationale codes); classifier-only code-shape probe `probe_code_shape(spans, n, image_base, vaddr, …)`; `needs-base` refusal when base provenance absent; never-guessed bases | worker-5 REPORT + `src/townfall_layout_probes.hpp`; worker-layout-minimum `validation/min_predicates.py` header | conventions this design must match |
| E-6 | In-game experiment conventions: PRX at `/data/plugins`, ploader.ini mode-2 late entry, USER-COORDINATED launch, JSONL record envelope `{marker,schema,build_id,seq,exp,rc,errno,fields}`, records <=2KiB, absence-is-a-result-not-a-retry, G6 already vendors tlp predicates with explicit `base_provenance` | worker-ingame-probe/STAGE-PLAN.md §§1-5 | conventions |

## F. Cross-check summary (what the census establishes)

1. Construction has exactly ONE live lead: the `0x23d590` shared encoded call
   target (A-3), derived by the conservative 5-anchor classifier (B-4), whose
   derivation simultaneously encodes the explicit-output ABI (C-8). Everything
   else construction-side is absent (A-1), unresolved (C-1, C-2, D-1, D-3) or
   retained port behavior awaiting a proven address (C-3, C-5, C-6).
2. The validated-static GUObjectArray (A-4) + G6 layout predicate (E-5, E-6)
   is the only sanctioned bridge from object headers toward name-slot facts,
   and even then names stay RAW BYTES (E-4) until width facts exist (D-1).
3. No FNamePool/GNames anchor exists anywhere in the workspace as evidence;
   the only lawful path to one is a validated constructor's own bounded
   disassembly (candidate-only) or a future dedicated survey endpoint.
