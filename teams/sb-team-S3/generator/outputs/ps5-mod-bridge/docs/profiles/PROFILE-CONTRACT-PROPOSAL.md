# Runtime profile contract: proposal

Status: PROPOSAL, 2026-10-07. Every field, name and rule here is **proposed** until GPT (runtime owner) confirms it. Nothing is console-verified. Companion: `TOWNFALL-ORACLE.md` (the Townfall values a generator must reproduce).

One profile per (title id, content version, executable), produced by `tools/profilegen` from the decrypted executable plus `param.json`. The app shows `none` (no folder) / `generated` / `console-verified`. Only GPT's console test may produce `console-verified`.

## 1. What UE4SS already keeps per game

Read from public sources (the fetch tool summarised some pages; re-check quoted lines against the repo before relying on them):
docs https://docs.ue4ss.com/custom-game-configs.html and https://docs.ue4ss.com/guides/fixing-compatibility-problems.html; repo https://github.com/UE4SS-RE/RE-UE4SS (`assets/UE4SS-settings.ini`, `assets/CustomGameConfigs/`, `assets/MemberVarLayoutTemplates/`, `assets/VTableLayoutTemplates/`, `assets/UE4SS_Signatures/`, `UE4SS/src/Signatures.cpp`, `UE4SS/src/UE4SSProgram.cpp`). The retained PS5 port was read locally (`SettingsManager.cpp`, `ue4ss-native-layout-parser-1/NativeLayoutFiles.hpp`).

**CustomGameConfigs** (docs): one folder per game, same four members as the root install; a game-named folder, when present, becomes the working directory and settings location (`UE4SSProgram.cpp`).
```
CustomGameConfigs/<Game Name>/{UE4SS-settings.ini, MemberVariableLayout.ini, VTableLayout.ini, UE4SS_Signatures/*.lua}
```

**UE4SS-settings.ini** (assets, main; excerpt, upstream comments abbreviated). Per-game keys only; the rest is policy:
```ini
[General]
bUseUObjectArrayCache = true
SecondsToScanBeforeGivingUp = 120
DefaultExecuteInGameThreadMethod = EngineTick
DefaultFNameToStringMethod = Scan
[EngineVersionOverride]
MajorVersion =
MinorVersion =
; empty = detect, true = Development, test = Test, false = Shipping
DebugBuild =
; empty = detect, true/false force STATS on/off
Stats =
[Hooks]
HookProcessInternal = 1
HookEngineTick = 1
EngineTickResolveMethod = Scan
FExecVTableOffsetInLocalPlayer = 0x28
```

**VTableLayout.ini, upstream**: per class, an *ordered list of function names*; the byte offset is `(index + sum of base-class sizes) * 8` (`UE4SSProgram.cpp`, via the fetch summary). Excerpt of `VTableLayout_5_06_Template.ini`:
```ini
[UObject]
; void* __vecDelDtor(uint32)
__vecDelDtor
; FString GetDetailedInfoInternal() const
GetDetailedInfoInternal
```
**VTableLayout.ini, PS5 port** (what the retained parser accepts): the 9-key `[NativeLayout]` header plus `Name=0xBYTEOFFSET` (aligned 8, at most INT32_MAX); ordered lists are **refused**; duplicate sections or keys, unknown header keys, NUL are refused; limits 8 MiB, 16384 entries, 512-byte tokens.
```ini
[NativeLayout]
Format=1
Target=prospero-x86_64-sysv
PointerSize=8
EngineMajor=5
EngineMinor=6
NonShipping=0
Stats=0
EngineRevision=5.6.1-0+++UE5
BuildId=04bb4330a7d71dcefe1cc2c4d19769d5

[UObject]
ProcessEvent=0x268
```
**MemberVariableLayout.ini**: same shape upstream and on PS5 (`[Class]`, `Member = 0xOFFSET`, `UEP_TotalSize`); the PS5 file adds the identical `[NativeLayout]` header, and members may carry `offset:position[:length[:storage]]` bitfields. Excerpt of `MemberVariableLayout_5_06_Template.ini`:
```ini
[UObjectBase]
; Total Size: 0x28
ObjectFlags = 0x8
InternalIndex = 0xC
ClassPrivate = 0x10
UEP_TotalSize = 0x28
```
**UE4SS_Signatures/\<Name\>.lua** (docs; `Signatures.cpp`): `Register()` returns the AOB (hex nibbles, two per byte, `??` or a single `?` as wildcard, e.g. `"48 8B C4 57 48 83 EC 70 80 3D ?? ?? ?? ?? ?? 48 89"`); `OnMatchFound(MatchAddress)` returns the final integer address (helpers `DerefToInt32`, `print`, `LoadExport`). A script that simply returns an integer short-circuits the scan (that integer is used as the address); returning nil throws "symbol not found". Script names UE4SS looks for: `GUObjectArray`, `FName_ToString`, `FName_Constructor`, `FText_Constructor`, `StaticConstructObject`, `GMalloc` (or `FMemory_Free`), `GUObjectHashTables`, `GNatives`, `ConsoleManager`, `ProcessLocalScriptFunction`, `ProcessInternal`, `CallFunctionByNameWithArguments`, `GameEngineTick`. There is no `StaticFindObject` or `ProcessEvent` script upstream: the retained port implements `StaticFindObject` itself (`UObjectGlobals.cpp`: FUObjectHashTables when available, else GUObjectArray iteration, names built with the FName constructor), and ProcessEvent is read from the UObject vtable.

## 2. What the PS5 runtime needs on top

| Gap | Why UE4SS files do not cover it | Proposed answer |
|---|---|---|
| Executable binding | PC trusts the module it loaded | `binding`: exe sha256 and bytes, `param.json` titleId, contentVersion, contentId; hard mismatch refuses the profile |
| Absolute addresses | PS5 text is execute-only (kernel prot 4); AOB scans cannot read it | `symbols.*`: file label (ELF vaddr space) and runtime address, found offline, checked at runtime by data-side validators |
| Load base and file mapping | PC asks the OS | `load` + `elf.segments[]` (per-segment file delta; never seek a vaddr directly) |
| Build facts | UE4SS detects at run time | `build`: configuration, STATS, engine major.minor.patch, revision string, build id, each with evidence |
| Layouts and vtable slots | templates are Windows-derived; PS5 UObject vtables carry a 2-slot Itanium destructor prefix (+8) | `structs.*`, `vtables.*`; only classes/slots with accepted evidence are emitted |
| Honest provenance | an ini has no room for "how do we know" | every leaf is a value envelope (section 4); the ini files are lossy exports |

## 3. Schema `modbridge.runtime-profile/1` (proposed)

File `runtime-profile.json`, UTF-8, LF, 2-space indent, keys in the order shown, no trailing spaces, so equal inputs give byte-equal output. `V` = value envelope (section 4), `V(x)` = envelope whose value is x; all hex values are lowercase `0x` strings. `profile_digest` covers `binding, version_map, elf, load, build, symbols, structs, vtables, ue4ss` and excludes `status, verification, generated, emit, unresolved`.
```json
{
  "schema": "modbridge.runtime-profile/1",
  "profile_id": "PPSA33286/02.003.000/3b6cfad4",
  "status": "generated",
  "profile_digest": "<sha256 of the canonical JSON of the sections listed above>",
  "verification": null,
  "generated": {"utc": "...", "generator": {"name": "profilegen", "version": "...", "commit": "..."},
                "inputs": [{"role": "executable", "sha256": "...", "bytes": 194241424}, {"role": "param.json", "sha256": "..."}]},
  "binding": {"title_id": V, "content_version": V, "master_version": V, "content_id": V,
              "param_json": {"sha256": "...", "bytes": N},
              "executable": {"name": "eboot.bin", "form": "decrypted", "bytes": N, "sha256": "..."}},
  "version_map": {"ref": "modbridge.game-profile/1", "path": "examples/game-profiles/PPSA33286.json",
                  "console_content_version": "02.003.000", "pc_build": null},
  "elf": {"e_type": V, "e_machine": V, "e_phnum": V, "e_entry": V, "section_table": V(false),
          "segments": [{"i": 0, "type": "LOAD", "flags": "--X", "p_offset": "0x4000", "p_vaddr": "0x0", "p_filesz": "...", "p_memsz": "...",
                        "file_delta": "0x4000", "runtime_class": "exec-only|readable|rw|unmapped-until-proven"}],
          "notes": ["RELA-only relocations: vtable slots are zero in the file"]},
  "load": {"base": V("0x400000"), "base_policy": "confirm-at-runtime", "image_extent": {"start": V, "end": V},
           "file_offset_rule": "file = p_offset + (vaddr - p_vaddr) per PT_LOAD"},
  "build": {"configuration": V("shipping"), "non_shipping": V(false), "stats": V(false),
            "engine": {"major": V, "minor": V, "patch": V, "changelist": V, "branch": V, "revision_string": V},
            "build_id": V, "allocator_family": V("FMallocBinned3")},
  "symbols": {"<name>": {"kind": "data|code", "label": V, "runtime": V, "ps_scan_slot": "n|null", "binding": "seed|vtable|not-applicable|absent",
                         "read_class": "data-readable|call-only", "abi": {}, "alternatives": [V]}},
  "structs": {"<Class>": {"state": "validated|candidate|refused", "total_size": V, "members": {"<Member>": V}}},
  "vtables": {"<Class>": {"dtor_prefix_slots": V, "slots": {"<Function>": V}}},
  "ue4ss": {"settings": {"EngineVersionOverride": {}, "General": {}, "Hooks": {}}, "emit_signature_scripts": false},
  "emit": [{"path": "ue4ss/VTableLayout.ini", "bytes": N, "sha256": "..."}],
  "unresolved": [{"id": "...", "why": "...", "needs": "console|more-analysis|upstream-source"}]
}
```
**How the load base is known.** For Townfall it is *in-game-observed*: the console kernel map shows the image spans tiling contiguously from 0x400000, four data windows read at base+vaddr matched the file bytes (igf7), and the runtime reported `lmf_extent_base_low=0x00400000` on three runs. profilegen cannot observe a base: for a new title it predicts 0x400000 from the ELF class (confidence `medium`, class `designed`, unverified for other titles), and the runtime confirms it per launch (`base_policy: confirm-at-runtime`).

`runtime` must equal `load.base + label`; the generator writes both and a schema check enforces it. `symbols` names follow the runtime's seed slots, numbered as in `PsScanAbi.h`: `object_array` (0), `fname_ctor_wchar` (2), `gmalloc` (3), `static_construct_object_internal` (4), `engine_tick` (10, optional, vtable-bound); `name_pool` has no ps_scan slot (ZCode numbered it 1); support symbols are `gmalloc_instance`, `gmalloc_vtable`, `gmalloc_effective_ptr`, `fmemory_malloc`, `fmemory_free`, `process_event`. `static_find_object` is recorded as `not-applicable` (no game address exists to find). Conflicting candidates stay in `alternatives`; nothing is silently dropped. Two fragments showing the parts a consumer needs beyond an address:
```json
"fname_ctor_wchar": {"kind": "code", "label": V("0x23d590"), "runtime": V("0x63d590"), "ps_scan_slot": 2, "binding": "seed", "read_class": "call-only",
  "abi": {"convention": "sysv-amd64", "args": [{"reg": "rdi", "role": "out FName* (8-byte store)"}, {"reg": "rsi", "role": "const char16_t* NUL-terminated"},
         {"reg": "edx", "role": "EFindName; all 9 call sites pass 1 (FNAME_Add)"}], "prologue_32": V("554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a6690")}},
"process_event": {"kind": "code", "label": V("0x459830"), "runtime": V("0x859830"), "binding": "vtable",
  "vtable": {"class": "UObject", "slot": V(77), "offset": V("0x268")}, "read_class": "call-only"}
```

## 4. Value envelope, confidence, evidence classes

```json
{"value": "0x23d590", "confidence": "high", "method": "fname_call.five_anchor",
 "evidence": [{"class": "re-verified", "ref": "evidence/fname_ctor.json#call_sites", "sha256": "...", "note": "9 rel32 sites"}]}
```
- `confidence` (generator-assigned): **high** = two independent methods agree, or a console receipt exists; **medium** = one deterministic method with a self-check (unique match, byte compare); **low** = heuristic, population statistic, or a convention. A missing value is `{"value": null, "confidence": "low", "method": "...", "evidence": []}` plus an `unresolved[]` row; defaults are never invented.
- `evidence[].class`: `designed` (format or policy choice), `host-derived` (one method on the pinned image), `re-verified` (two or more independent offline channels agree), `in-game-observed` (console receipt). The generator may use `in-game-observed` only when `ref` resolves to a receipt file under `receipts/` written by GPT's tooling and the receipt's title, content version and exe sha256 match; it never mints one.
- `method` is a stable id (`engine.store_site_scan`, `elf.gnu_build_id`, `axes.marker_fingerprint`, ...) so tests can assert how a value was found.

## 5. Status and verification

`status` is `generated` whenever profilegen wrote the file. `verification` is `null` until GPT's runtime test fills it:
```json
"verification": {"receipt_id": "...", "date_utc": "...", "profile_digest": "<digest it verified>",
                 "runtime_build": {"build_id": "...", "sha256": "..."}, "verified": ["symbols.name_pool", "..."], "result": "pass"}
```
Rules: the generator refuses to write a non-null `verification` or `status: console-verified`; only GPT's receipt ingester sets them. The app shows `console-verified` only if `verification.profile_digest` equals the file's current `profile_digest` and the executable sha256 matches the installed game; otherwise it shows `generated` with a "stale" note. When profilegen regenerates and the recomputed digest is unchanged, it carries the existing `status` and `verification` over untouched; when the digest changes it resets them to `generated` and `null`, so a stale verification can never ride along.

## 6. Folder layout

```
profiles/<TITLE_ID>/<CONTENT_VERSION>/                 e.g. profiles/PPSA33286/02.003.000/
  runtime-profile.json
  evidence/        generator logs and extracts (sha256-addressed; inputs hashed, never copied)
  receipts/        console receipts, written only by GPT's tooling
  ue4ss/           generated: UE4SS-settings.ini, MemberVariableLayout.ini, VTableLayout.ini
  ue4ss/UE4SS_Signatures/   only if ue4ss.emit_signature_scripts is true
  export/CustomGameConfigs/<Game Name>/   optional upstream-shaped copy for PC tools
```
A second content version gets its own folder; a profile is never edited across versions. Lookup is exact (title id, content version, exe sha256); there is no "closest version" fallback.

## 7. Emitting the UE4SS files from the JSON

Deterministic, no hand edits; each emitted file's size and sha256 go into `emit[]` (the loadlist enforces sizes byte-exactly; a stale size caused the run-2 `code=-8` refusal). The existing gated assembler (`assemble_staged_pair_v2.py`) is the model: refuse when identity is incomplete, emit only classes with accepted evidence.

| Output | From | Rule |
|---|---|---|
| `ue4ss/UE4SS-settings.ini` | fixed PS5 policy template + `ue4ss.settings` | per-game keys: `[EngineVersionOverride] MajorVersion/MinorVersion`, `DebugBuild=false`/`Stats=false` only when that value is `high` (else omitted = detect); `[Hooks]` keys are policy. Only whole-line `;` comments, no section with only comments (the retained parser wedges; inline `;` corrupts values) |
| `ue4ss/MemberVariableLayout.ini` | `build` + `structs` | `[NativeLayout]` header then only `state: validated` classes, hex offsets, `UEP_TotalSize` |
| `ue4ss/VTableLayout.ini` | `build` + `vtables` | header, then only slots with accepted evidence as `Name=0x...`; identity-only is legal. Other slots are not guessed (template offset + 8 is a candidate, not a value) |
| `UE4SS_Signatures/*.lua` | `symbols.*.runtime` | off by default; when on, `return 0x<runtime>` (integer short-circuit). Risk: a shipped script turns on the Lua scan-override path |
| `[NativeLayout]` header | `build`, `binding` | `Format=1`, `Target=prospero-x86_64-sysv`, `PointerSize=8`, `NonShipping`/`Stats` as 0/1, `EngineRevision`, `BuildId` (opaque printable ASCII, at most 512 bytes) |

The seeds themselves are not an ini: the runtime reads `symbols.*` from the JSON (or a generated `SeedTable.h` equivalent). GPT chooses which; the contract only fixes the names and meanings.

## 8. PC to PS5 version map

Do not duplicate it. `version_map` points at the existing `modbridge.game-profile/1` (`examples/game-profiles/PPSA33286.json`: `builds[] = {console_content_version, pc_build}`; `pc_build` is `null` today for 02.003.000). At generation time profilegen copies the matching row and records the `param.json` contentVersion; if the title's game-profile has no row for that version it writes `pc_build: null` and an `unresolved` entry. The runtime profile never decides which Nexus file fits; `select_mod_file_for_build` keeps that job.

## 9. Mapping from ZCode's hand-built profile

Source: `outputs/2026-10-04-zcode-team/worker-l3-profile/game-profile.townfall-ps5-ue56-v1.json` (`game-profile/1`, REPORT.md, VALIDATION.md) and the staged ini files (`worker-gap-l2a/staged/VTableLayout.ini`, `worker-final-staging-rehearsal/rehearsal-root/ue4ss/`).

| ZCode field | Proposed | Why |
|---|---|---|
| `schema_id`, `profile_id`, `generated`, `generator` | `schema`, `profile_id`, `generated` | renamed; `layer_model` dropped (prose) |
| `identity.{title_id,image_bytes,image_sha256,target,pointer_size}` | `binding.executable`, `binding.title_id`, `NativeLayout` header | split binding from facts; add `param.json` fields (contentVersion, contentId) |
| `identity.elf`, `memory_geometry.segments` | `elf`, `elf.segments[]` | kept static part; observed VMMAP spans go to `receipts/` (console data, not a build fact) |
| `engine.version` (patch, revision null) | `build.engine.*` filled | `worker-layout-identity` found patch 1, branch `UE5`, build id; l3 did not ingest it |
| `engine.version_override` | `ue4ss.settings.EngineVersionOverride` | it is a settings export, not a fact |
| `engine.key_structures.*` (offsets, functions) | `symbols.*`, `structs.FNamePool`, `symbols.*.abi` | one symbol table; structure offsets become `structs` leaves |
| `addresses.translation_rule` | `load` | `slide` becomes `load.base` with `base_policy` |
| `addresses.seed_slots[0..5]`, `support_constants` | `symbols.*` (+ `ps_scan_slot`) | same data; RETIRED heap envelope dropped |
| `addresses.runtime_examples`, `known_false_claims`, `consumer_read_risks` | dropped | derived or runtime-policy prose |
| `memory_geometry.owned_read_policy`, `runtime_facts`, `api_surface`, `mod_requirements` | dropped | runtime/mod facts: they belong to the runtime manifest (`examples/runtimes/ps5-ue4ss-port.json`) and mod requirements, not the game build |
| `provenance_class`, `sources[]` | `evidence[].class`, `evidence[].ref` | per value, not per section |
| `unresolved`, `readiness` | `unresolved[]`, `status` + `verification` | `readiness.*_ready=false` becomes "no verification block" |

Errors found while mapping (details in the oracle): five FName-anchor "linked content addrs" in l3 and `SeedTable.h` are file offsets (vaddr + 0x4000), not vaddrs; l3 `instance_ctor` and `slow_path` labels are runtime values (+0x400000) mislabeled; l3 `free_vtable_slot: 8` conflicts with its own `+0x50` (slot 10); l3 lists the +0x4000 file delta for LOAD0/LOAD1 only (also LOAD3, LOAD7).

## 10. Questions for GPT

1. Does the runtime read `runtime-profile.json` at start, or does a build step turn it into `SeedTable.h`?
2. Is `load.base` fixed at 0x400000 or discovered per launch? Proposal: discovered per launch from the kernel map, compared to the profile, mismatch refuses seeds.
3. Which GMalloc variable is canonical (oracle C1)?
4. Which fields, if any, must be mandatory in v1 (proposal: `binding`, `load`, `build.engine`, `symbols.{object_array,name_pool,fname_ctor_wchar,gmalloc,static_construct_object_internal}`)?
5. Where should `receipts/` live on console, and who writes the path into `verification`?
