# Townfall oracle: values a profile generator must reproduce

Title PPSA33286 (SILENT HILL: Townfall), content version 02.003.000, executable sha256 `3b6cfad4...622fc`. Compiled 2026-10-07 from ZCode's outputs (read-only) plus a byte re-read of the pinned image on the same day (rows marked "re-read"). Companion to `PROFILE-CONTRACT-PROPOSAL.md`; the key names in section 13 are paths into the proposed `runtime-profile.json`. Nothing here is console-verified by the profile mechanism; "in-game-observed" rows cite older probe receipts.

## 0. Conventions and legends

- **label** = ELF vaddr (p_vaddr space; the image is linked at 0). **runtime** = label + 0x400000. **file offset** = `p_offset + (vaddr - p_vaddr)` per PT_LOAD: +0x4000 for ph0, ph1, ph3, ph7; -0x324000 for ph8. Seeking a vaddr directly reads wrong bytes; this produced retracted artifacts (POOL:187-189).
- **Evidence class**: `designed` (convention or policy choice), `host-derived` (one method on the pinned image), `re-verified` (two or more independent offline channels agree, e.g. capstone plus Ghidra), `in-game-observed` (console probe receipt exists).
- **Generic**: `G` = the method works on another UE5 PS5 title with no hand values; `G-hand` = method is general but was run by hand or ad hoc scripts, so it still needs automating; `S` = game-specific, a convention, or an observation.
- **Source keys** (paths relative to `/WORKSPACE/ps5-claude/outputs` unless absolute; `TEAM` = `2026-10-04-zcode-team`; `CTRL` = `2026-10-04-zcode-controller`):
  `IDRES` TEAM/worker-layout-identity/IDENTITY-RESOLUTION.md; `AXES` TEAM/worker-axes-detector/REPORT.md; `GEOM` TEAM/worker-geometry-reference/GEOMETRY.md (+ GEOMETRY.json); `VSEM` TEAM/worker-vaddr-semantics/REPORT.md; `POOL` TEAM/worker-re-fname-pool/FNAME-POOL-RE.md; `FC1` TEAM/worker-fname-fc1-materialize/FC1-CHECKS.md (+ REPORT.md, calls_by_anchor.materialized.json); `FNV` TEAM/worker-fname-ctor-audit/FNAME-VERDICT.md; `GMC` TEAM/worker-gmalloc-derivation/GMALLOC-CANDIDATES.json; `GMR` same dir REPORT.md; `REA` TEAM/worker-rea-trial/FINDINGS.md; `GAP` TEAM/worker-gap-l2a/{REPORT.md,DIFF.md,FHIT-VERDICT.md,staged/VTableLayout.ini}; `L3` TEAM/worker-l3-profile/game-profile.townfall-ps5-ue56-v1.json; `SEEDJ` TEAM/worker-seed-table-derivation/SEED-TABLE.json; `SEEDH` CTRL/launch10-build/final-units/SeedTable.h; `SURV` /WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-file-core-survey-1/run-release-1/stdout.txt; `SPL` TEAM/worker-staged-pair-live; `EFP` TEAM/worker-emit-final-pair; `PKG` TEAM/worker-loadlist-final/final-package/ue4ss; `PRED` TEAM/worker-predicate-gc-fix/REPORT.md; `SIG` TEAM/worker-all-signatures-inventory/SIGNATURES-INVENTORY.md; `TICK` TEAM/worker-tick-address-validation/REPORT.md; `MIN` TEAM/worker-layout-minimum/MINIMUM-SET.md; `PARAM` TEAM/worker-game-version/param.json; `IGF7` CTRL/console/c2-alternative-results.jsonl; `IGF6` CTRL/console/igf6-results.jsonl; `L10` CTRL/launch10-evidence/LAUNCH10-RESULTS.md.
  Decrypted executable: `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`.

## 1. Binding and image

| Key | Value | Source | Method | Class | Gen |
|---|---|---|---|---|---|
| binding.title_id | PPSA33286 | PARAM:158 | param.json `titleId` | host-derived | G |
| binding.content_version | 02.003.000 | PARAM:94 | `contentVersion`. Also `masterVersion` 01.00 (:148), `targetContentVersion` 02.002.000 (:157), `contentId` JP0101-PPSA33286_00-TOWNFALLSIEJ0000 (:93) | host-derived | G |
| binding.param_json.sha256 | c9c3a687...5834 (full value in section 13) | re-read | sha256 of the file | host-derived | G |
| binding.executable | 194,241,424 bytes (0xb93e390); sha256 `3b6cfad4...622fc` | GEOM:19-20; SEEDH:36; L3 `identity` | size and sha256, recomputed 2026-10-07 | host-derived (re-read) | G |

## 2. ELF and load

| Key | Value | Source | Method | Class | Gen |
|---|---|---|---|---|---|
| elf header | e_type 0xfe10, e_machine 62, e_entry 0x80, e_phoff 0x40, e_phentsize 56, e_phnum 14; section table absent (e_shoff 0x6f48a0a8 is past EOF) | GEOM:21-23; VSEM:30-32; re-read | ELF header parse | host-derived | G |
| elf.segments | table below | GEOM:25-46; VSEM:37-44; re-read | phdr parse | host-derived | G |
| elf.rela | DT_RELA label 0x9653430 (file 0x932f430), 0x2607ee8 bytes, 1,661,599 entries: 1,649,556 RELATIVE, 12,022 R_X86_64_64, 21 GLOB_DAT. Vtable slots are zero in the file; pointers exist only as relocations | re-read of the dynamic tags; GMC:8; POOL:5 (count differs, C7) | PT_DYNAMIC parse | host-derived | G |
| elf.note / dynamic | PT_NOTE ph9 label 0xbc5f250; PT_DYNAMIC ph10 label 0xbc5f278; both inside ph8 | VSEM:44; re-read | phdr parse | host-derived | G |
| load.base | 0x400000 | SEEDH:11; GEOM:48-82; IGF7:1418 (summary `derived_base=0x400000`, `base_matches_expected=1`); IGF6:67 (image spans tile contiguously from 0x400000); L10:58 (`lmf_extent_base_low=0x00400000`, runs 1/3/4) | kernel VMMAP walk, ELF base scan, byte matches of four data windows (IGF7:1413-1416) | in-game-observed | S |
| load.image_extent | runtime [0x400000, 0xc0602c8); vaddr end 0xbc602c8 = max(p_vaddr+p_memsz); page-rounded end 0xc064000 | GEOM:51; L10:58 | max over PT_LOAD of p_vaddr+p_memsz | in-game-observed (runtime ends, three runs); the static computation is G | G |

| ph | flags | p_offset | p_vaddr | p_filesz | p_memsz | file delta | runtime class seen on console (GEOM:56-70) |
|---|---|---|---|---|---|---|---|
| 0 | --X | 0x4000 | 0x0 | 0x7479b1c | 0x7479b1c | +0x4000 | exec-only, kernel prot 4, [0x400000, 0x787c000); reads refused (igf4) |
| 1 | R-- | 0x7480000 | 0x747c000 | 0xa3e918 | 0xa3e918 | +0x4000 | prot 1, [0x787c000, 0x82bc000) |
| 3 | RW- | 0x7ec0000 | 0x7ebc000 | 0x127e4e8 | 0x1280000 | +0x4000 | prot 1 (RELRO), [0x82bc000, 0x953c000) |
| 7 | RW- | 0x9140000 | 0x913c000 | 0x1cc5f2 | 0x4f7418 | +0x4000 | prot 3/1/3/3 in four tiles to 0x9a34000; BSS starts at vaddr 0x93085f2 |
| 8 | --- | 0x930f420 | 0x9633420 | 0x262cea8 | 0x262cea8 | -0x324000 | no span at menu state; treat as unmapped-until-proven (PT_NOTE and PT_DYNAMIC live here) |

Other phdrs: ph2 GNU_EH_FRAME, ph4 GNU_RELRO (covers ph3), ph5 TLS, ph6 type 0x61000001 (called PT_SCE_VERSION by ZCode, PT_SCE_PROCPARAM in `tools/profilegen/elf.py`), ph9 NOTE, ph10 DYNAMIC, ph11/ph12 0x6fffff00/01, ph13 NOTE.

## 3. Build facts

| Key | Value | Source | Method | Class | Gen |
|---|---|---|---|---|---|
| build.configuration, build.stats | shipping (margin 60: cost 43 vs 103); STATS off (margin 12: 43 vs 55); 54 of 244 markers present. NativeLayout `NonShipping=0`, `Stats=0` | AXES:86-92; EFP/emission-receipt.json | patternsleuth `BuildConfiguration`/`Stats` marker fingerprint (244 UTF-16 markers, CONTRADICTION 5 / OMISSION 1, MIN_MARGIN 4, MIN_MARKERS 10); the C detector and an independent Python reference agree | host-derived | G |
| build.engine.major, minor | 5, 6 | SURV:1; IDRES:81-93 | patternsleuth `engine_version` file-image resolver (status 0) and a hand decode of the same store | re-verified | G |
| build.engine.patch | 1 | IDRES:85-93; re-read `66 c7 43 04 01 00` at vaddr 0x1ef7e1 | immediate store at FEngineVersionBase+4 (resolver returns major/minor only) | host-derived | G-hand |
| build.engine.changelist | 0 | IDRES:111-114; re-read stub at vaddr 0xe0 = `55 48 89 e5 31 c0 5d c3` | the two helper calls that build the changelist word return 0 | host-derived | G-hand |
| build.engine.branch | "UE5" | IDRES:115-120; re-read accessor at vaddr 0xf0 (`lea rax,[rip+0x7826687]`) pointing at L"UE5" (vaddr 0x7826782); IGF7:1413 live read `5500450035000000` at 0x7c26782 | accessor LEA to UTF-16 string | in-game-observed (bytes); accessor link is host-derived | G-hand |
| build.engine.revision_string | "5.6.1-0+++UE5" | IDRES:21-26; EFP | assembled from the five components; the string does not exist in the image (see C6) | designed | S |
| build.engine.store_site | `mov dword [rbx],0x00060005` at vaddr 0x1ef7db (file 0x1f37db); the only `c7 03 05 00 06 00` hit in the image | re-read; IDRES:81 cites 0x1ef7de (C5) | immediate-store scan | host-derived | G |
| build.build_id | 04bb4330a7d71dcefe1cc2c4d19769d5 | IDRES:131-147; re-read note at vaddr 0xbc5f250; IGF7:1417 (window A refused `uncovered`) | ELF PT_NOTE type 3 (GNU): namesz 4, descsz 20 = 16 bytes plus 4 zero bytes. UE's own BUILD_ID value does not exist in the image (IDRES:149-166) | host-derived; static only | G for extraction, S for the convention build_id := GNU note |
| build.allocator_family | FMallocBinned3 | GMC:18-25 (four UTF-16 literals at vaddr 0x765b45a, 0x77d1b8c, 0x78788da, 0x78f7c14; re-read at file +0x4000); REA:164-175 | string anchor | host-derived | G |

## 4. Core symbols (seed slots)

`PS` = ps_scan slot number in `PsScanAbi.h` order (0 GUObjectArray, 1 FName::ToString, 2 FName ctor, 3 GMalloc, 4 StaticConstructObject, 5 FText ctor, 6 engine version, 7 hash tables, 8 GNatives, 9 console manager, 10 engine tick, 11 build configuration, 12 stats; SIG:73-85). `-` = no ps_scan slot. ZCode's `L3` numbers its own six seed slots differently (pool = 1, gmalloc instance = 5).

| PS | Key | Label | Runtime | Class | Method | Sources | Gen |
|---|---|---|---|---|---|---|---|
| 0 | object_array (GUObjectArray) | 0x93ba110 | 0x97ba110 | in-game-observed | patternsleuth GUObjectArray file-image resolver (SURV:2, cfg candidate alloc 0x483b40 free 0x4840b0 SURV:10) plus 1565 rip-relative lea/mov refs, 0 mov-imm32 (SEEDJ). Console: 96-byte read and same-value writeback at 0x97ba110 (IGF6:1105-1106) | SEEDH:31; L3:154,294; GEOM:91 | G |
| - | name_pool (FNamePool; L3 seed slot 1) | 0x9365ec0 | 0x9765ec0 | re-verified | from the FName core (0x242470) follow the lea to one global with +0x10040 buckets and an init flag; 100+ code refs; Ghidra 49/49 xrefs equal the byte scan | POOL:38-78; FNV:95-110; REA:150-175; SEEDH:12; L3:57,377 | G-hand |
| - | name_pool_init_flag | 0x9365ea0 | 0x9765ea0 | re-verified | byte at pool-0x20, tested at 0x2424be, set at 0x2424e2 | POOL:55-78 | G-hand |
| 2 | fname_ctor_wchar | 0x23d590 | 0x63d590 | re-verified | five-anchor route (section 5) | SURV:5; FC1:26-64; SEEDH:32; L3:133,315 | G |
| 3 | gmalloc | 0x930add8 | 0x970add8 | re-verified, conflicted (C1) | FMallocBinned3 literals, RELA-reconstructed vtable 0x7ec26e0, ctor 0xafd40, factory 0xb3f0, unique store at 0xb660 (re-read) | GMC:14-33; GMR:11-52; SEEDH:33; L3:175,336 | G for Binned3 titles |
| - | gmalloc_effective_ptr | 0x9350950 | 0x9750950 | host-derived; REA agrees | patternsleuth GMalloc DeleteFile route (SURV:7); operand of `lea r14,[rip+...]` in FMemory::Malloc (0xb7a2a) and FMemory::Free (0xb7a9c) (re-read) | SURV:7; REA:150-175; L3:432 | G |
| 4 | static_construct_object_internal | 0x490aa0 | 0x890aa0 | re-verified | UTF-16 anchor "NewObject with empty name can't be used to create default..." (label 0x7999084, one hit, re-read at file 0x799d084), 1918 xrefs, terminal near-call of the params-ctor pair (0x493530, 0x493580) in 744 windows; prologue `554889e54157415641554154534881ec` | SURV:8; SEEDH:34; SEEDJ:121-122; L3:240,356 | G |
| 10 | engine_tick (UGameEngine::Tick) | 0x2fac8c0 | 0x33ac8c0 | re-verified; not seeded | patternsleuth `gameengine_tick` (SURV:9): unique UTF-16 `causeevent=` xref at label 0x2facbae, nearest padding boundary; prologue `554889e54157415641554154534881ec880000004c8b2d`. Runtime binds from the live vtable; vtable slot unknown. Code never read in-game (igf4 refusal covered_not_readable) | SEEDJ:137-140; TICK:13-14; GEOM:84-92; L3:257,444 | G |
| - | process_event | 0x459830 | 0x859830 | host-derived | content of the function at slot 77 of 5,571 UObject-family vtable runs (x2193); see section 8 | GAP/FHIT-VERDICT:83-99; GAP/DIFF:60-69 | G-hand |
| - | static_find_object | not applicable | - | - | no game address is needed: the port implements it over FUObjectHashTables/GUObjectArray plus the FName constructor | PROFILE-CONTRACT-PROPOSAL.md section 1; MIN:88-91 | - |
| 6 | engine_version | settings-owned | - | designed | `[EngineVersionOverride] MajorVersion=5, MinorVersion=6` | SIG:79; TEAM/worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini:24-26 | - |

No value and no consumer on the Item Spawner path: PS 5 `ftext_fstring`, 7 `fuobject_hash_tables_get` (graceful fallback), 8 `gnatives` (warning only), 9 `console_manager_singleton`. PS 1 `fname_tostring_fstring` is absent (SURV:4 status 3); the runtime falls back to `Conv_NameToString`. PS 11/12 are the axes in section 3 (SIG:73-85).

## 5. FName constructor and pool

**Calling convention** (PS5, SysV AMD64): `void FName::FName(FName* out /*rdi*/, const char16_t* name /*rsi*/, EFindName mode /*edx*/)`. The constructor stores one 8-byte handle (`id | number << 32`) through rdi; a null `name` is tolerated (`test rsi,rsi` at 0x23d59c). All 9 call sites load `edx = 1` (`mov edx,1`, so FNAME_Add) and pass `lea rdi,[rbp-X]` as the out slot. Whether the third argument is honoured inside the callee is not decoded. Sources: FC1:26-64, FNV:95-100, REA:1-40 (REA lists only two parameters; the call sites show three). Prologue (32 bytes, re-read): `554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a6690`.

**Five-anchor route** (method `G`): UTF-16 module names `Engine`, `Renderer`, `AnimGraphRuntime`, `Landscape`, `RenderCore` (content vaddrs 0x782706e, 0x799db12, 0x75eefc0, 0x763151e, 0x79f02e2; xref census 10284/5/2/18/2) are each followed, in one registration function (0x3a17b7e..0x3a17ca6), by `lea rdi,[rbp-X]; mov edx,1; call rel32`; the single common call target across all anchors is the constructor. Nine call sites (re-read, each rel32 resolves to 0x23d590): 0x174d6af, 0x174d6d8, 0x2f07077, 0x2f85635, 0x3a17b91, 0x3a17bbc, 0x3a17be7, 0x3a17c50, 0x3a17ca6.

**FNamePool** (all offsets from the pool label; POOL:12-35, 38-130): CurrentBlock +0x8 (u32, **0-based index of the open block, 0 is valid**), CurrentByteOffset +0xC (u32), Blocks[8192] +0x10 (u64), 256 hash buckets of 0x40 bytes at +0x10040, NoneId +0x14040, ByteProperty id +0x14044 (IntProperty +0x14048, FloatProperty +0x1404c). Blocks are 0x20000 bytes from `FMemory::Malloc(0x20000, 2)`; id = block<<16 | offset/2; entry stride 2; header u16 LE: bit 0 wide, bits 5:1 probe hash, bits 15:6 length; 236 default names; entry 0 of Blocks[0] is ANSI "None". Functions (label): init 0x23a5d0, find-or-store core 0x242470, intern 0x23d600, reserve 0x246090, default-name store 0x23c0b0 (init, core, intern and reserve re-read as function starts; 0x23c0b0 not re-read; the getter label 0x243240 quoted by L3 is mid-function). Class: re-verified. Gen: `G-hand`.

## 6. Allocator

| Item | Label | Runtime | Notes |
|---|---|---|---|
| gmalloc (seed value) | 0x930add8 | 0x970add8 | unique bootstrap store `mov [rip+...],rbx` at 0xb660 after the factory builds the instance (re-read); 11 rip-relative loads in 11 FMemory thunks (GMR:33-52) |
| gmalloc_instance | 0x93089b8 | 0x97089b8 | in-place global built by 0xafd40; V1 expects `[0x970add8] == 0x97089b8` (never run in-game) |
| gmalloc_vtable | 0x7ec26e0 | 0x82c26e0 | RELA-reconstructed (ph3 RELRO, readable in-game); V2 expects the instance vptr to equal it |
| gmalloc_effective_ptr | 0x9350950 | 0x9750950 | read by FMemory::Malloc and FMemory::Free; a wrapper chain is installed here (decorator 0xbeb20, vtable 0x7ec3f20, optional 0x28-byte thread forwarder vtable 0x7ec4030) (REA:164-175) |
| fmemory_malloc | 0xb7a20 | 0x4b7a20 | `call [vtbl+0x30]` (FMalloc slot 6); re-read |
| fmemory_free | 0xb7a90 | 0x4b7a90 | `call [vtbl+0x50]` (slot 10, not 8); re-read |
| malloc impl / slow path | 0xb48e0 / 0xb1460 | 0x4b48e0 / 0x4b1460 | fast path `size < 0x20001 && align < 0x11`, 76 pool tables x 0x78 bytes (POOL:132-157). L3 mislabels the slow path (C3) |
| instance ctor / factory | 0xafd40 / 0xb3f0 | 0x4afd40 / 0x40b3f0 | L3 mislabels the ctor (C3) |

## 7. UObject array layouts (the staged `MemberVariableLayout.ini`)

Only these two classes are staged (PKG/MemberVariableLayout.ini, 588 bytes). Class: in-game-observed for the structure as read from the live array at 0x97ba110 (layout A, hypothesis FUObjectArray outer), with caveats in C9. Method: UE4SS 5_06 template defaults (MIN:44-72) accepted by the worker-5 predicate chain after the GC-order fix (PRED:12-30); inputs in SPL/input/gua-evidence-live.json:14-23. Gen: `G` for the method, `S` for validation (per snapshot).

| FUObjectArray | offset | | TUObjectArray | offset |
|---|---|---|---|---|
| ObjFirstGCIndex | 0x0 | | Objects | 0x0 |
| ObjLastNonGCIndex | 0x4 | | PreAllocatedObjects | 0x8 |
| MaxObjectsNotConsideredByGC | 0x8 | | MaxElements | 0x10 |
| OpenForDisregardForGC | 0xC | | NumElements | 0x14 |
| ObjObjects | 0x10 | | MaxChunks | 0x18 |
| UObjectCreateListeners | 0x68 | | NumChunks | 0x1C |
| UObjectDeleteListeners | 0x78 | | UEP_TotalSize | 0x20 |
| PrimarySerialNumber | 0xB0 | | elements per chunk | 0x10000 |
| bShouldRecycleObjectIndices | 0xB4 | | | |
| UEP_TotalSize | 0xB8 | | | |

Refused by the final pair (EFP/emission-receipt.json): FUObjectItem, UObjectBase, UStruct, UClass, FField, FProperty (predicate `uobject_item_chain` absent), UFunction (no Validated-class predicate exists). Template candidates (not facts, not emitted): UObjectBase 0x8/0xC/0x10/0x18/0x20 (total 0x28); FUObjectItem stride 0x18; FField total 0x30; FProperty Offset_Internal 0x44 (total 0x70); UStruct 0x40/0x48/0x50/0x58/0x5C (total 0xB0); UFunction FunctionFlags 0xB0 .. Func 0xD8 (total 0xE0) (MIN). Host corroboration for two of them: FProperty.Offset_Internal at +0x44 is read by the game's own thunk helper at 0x4577d0 (`movsxd rcx,[rdx+0x44]` at 0x4577de, FHIT-VERDICT:38-44); FunctionFlags byte +0xB1 is tested inside ProcessEvent.

## 8. VTable

| Key | Value | Source | Method | Class | Gen |
|---|---|---|---|---|---|
| vtables.UObject.dtor_prefix_slots | 2 (every virtual after slot 1 is +8 versus the Win64 templates) | GAP/DIFF:60-69; FHIT-VERDICT:83-99 | adjacent paired functions at slots 0/1 across 5,571 vtable runs (0x29c800/0x29c810 x943, 0x1370fd/0x1370fe x638, 0x222169/0x22216a x188): Itanium complete/deleting dtors | host-derived | G-hand |
| vtables.UObject.ProcessEvent | 0x268 (slot 77); Win64 template says 0x260, which holds the empty stub 0xb290 (x2514, re-read `554889e55dc3cccc`) | same | identified by content: ObjectFlags guard, FUNC_Native test at `[ufunction+0xB1]&4`, virtual calls to `[vtbl+0x270]`/`[vtbl+0x278]` (GetFunctionCallspace/CallRemoteFunction, themselves +8) | host-derived | G-hand |
| symbols.process_event | label 0x459830, runtime 0x859830 (x2193 runs); prologue (re-read) `554889e54157415641554154534883e4e04881ec20010000` | same | dominant value of slot 77 | host-derived | G-hand |
| vtables.FMalloc | Malloc +0x30, Free +0x50 | section 6 | decode of FMemory::Malloc/Free | host-derived (re-read) | G |

Not verified, so not staged: GetFunctionCallspace/CallRemoteFunction (+0x270/+0x278 expected), AActor::Tick, UGameViewportClient::Tick, ProcessConsoleExec, UStruct::Link, and the UGameEngine::Tick slot. The staged `VTableLayout.ini` with `ProcessEvent=0x268` exists only in GAP/staged; the package that was actually delivered (PKG) is identity-only (C8).

## 9. Golden files (byte-exact targets)

| File | Bytes | sha256 | Path |
|---|---|---|---|
| VTableLayout.ini, identity only | 186 | `9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86` | PKG/VTableLayout.ini |
| VTableLayout.ini, with `[UObject]` `ProcessEvent=0x268` | 216 | `37a7d86036106d1042c10c10e11f66edef4ba7c2b2a2948c8aaeea719ee9a8a5` | GAP/staged/VTableLayout.ini |
| MemberVariableLayout.ini | 588 | `f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32` | PKG/MemberVariableLayout.ini |
| UE4SS-settings.ini (v3, policy plus `[EngineVersionOverride] 5/6`, `HookProcessInternal=0`, `HookProcessLocalScriptFunction=0`) | 1448 | `d8faa4eca2ed2f22c02693b6841b7939412b4dd60c218a26438f985152d67038` | TEAM/worker-settings-v3-package/package-v3/ue4ss/UE4SS-settings.ini |

All files use LF, a trailing newline, a blank line between sections, and the 9-key `[NativeLayout]` header in the order of section 13 (`emit.NativeLayout.header`). The settings policy keys are `designed`, not facts.

## 10. Conflicts and corrections the generator must not reproduce

- **C1 GMalloc has two candidate variables.** Seeded value 0x930add8 comes from the unique bootstrap store; FMemory::Malloc and FMemory::Free both load GMalloc from 0x9350950 (re-read), which is also what patternsleuth's GMalloc resolver returned (SURV:7). The rejection of 0x9350950 (GEOM:101-109) compares it against the seed value 0x970add8, whose own lineage is a static derivation (GEOM row 10 of the adjudication table), not a console observation. V1/V2 have never run in-game. The generator should emit both (`gmalloc` plus `gmalloc_effective_ptr`) and let the console test decide.
- **C2 FName anchor addresses.** `L3` and `SEEDH:32` quote the five anchor strings as 0x782b06e, 0x79a1b12, 0x75f2fc0, 0x763551e, 0x79f42e2; those are file offsets (vaddr + 0x4000). The vaddrs are in section 5 (re-read: L"Engine" sits at file 0x782b06e).
- **C3 L3 label slips.** `instance_ctor` 0x4afd40 and `slow_path` 0x4b1460 (L3:202,219) are runtime values; the labels are 0xafd40 and 0xb1460. `free_vtable_slot: 8` contradicts its own `+0x50` (slot 10).
- **C4 Segment deltas.** L3 states +0x4000 for LOAD0/LOAD1 only; ph3 and ph7 are also +0x4000. IDRES:196 prints ph8 as "+0x324000"; it is -0x324000 (its own fact table, :206, is right).
- **C5 IDRES offsets.** The version store is at vaddr 0x1ef7db, not 0x1ef7de (IDRES:81-93 lists the third byte); the GNU note header has namesz 4 (including NUL), not 3.
- **C6 Revision string.** `5.6.1-0+++UE5` joins five verified components, but its three pluses match Epic's usual form, where the branch name begins with `++` (for example `++UE5+Release-5.6`) and `ToString` adds one `+`; this image's branch is `UE5`, so UE's own `ToString` would likely print a single `+` (recalled from UE source, not verified here). EFP/REPORT.md and emission-receipt.json also mention a `+Release-5.6` suffix that the image does not contain (IDRES:153). The string is an opaque staging tag (never runtime-compared); test components, not the join, and keep the staged value for byte-equal output.
- **C7 RELA counts.** POOL:5 says 1,663,596 RELAs; GMC:8 calls 1,661,599 "relative". Re-read: 1,661,599 total, 1,649,556 relative (DT_RELACOUNT 0x192b94).
- **C8 Delivered VTableLayout is identity-only.** The loadlist package (186 bytes) lacks `ProcessEvent=0x268`; it lives only in GAP/staged (216 bytes). On the console the Win64 slot 0x260 (empty stub) would be called until that file is staged.
- **C9 GUA evidence status.** igf4 recorded `gua_validated_any_cfg=0` (GEOM:91, IGF6:1109) with the pre-fix predicate; the layout-A acceptance rests on the GC-order fix and consistency checks on one 96-byte capture (PRED:12-30, SPL/input/gua-evidence-live.json). The 17 offsets are template defaults confirmed per snapshot, not independently measured.
- **C10 UFunction.Func.** Template 0xD8, but ProcessEvent reads the native Func pointer at +0xC8 (GAP/REPORT.md:78, FHIT-VERDICT:96). Do not stage either until resolved.
- **C11 Retracted values.** Tick 0x37ac8c0 (a runtime value mislabeled as a label; TICK, SEEDJ:140); `CurrentBlock >= 1` (FNV:144-146; correct predicate is `id>>16 <= CurrentBlock`, POOL:159-178); heap envelope [0x8018c000, 0x100118000) (retired, GEOM:111-128); GMalloc interface candidate treated as "refuted" in L3.
- **C12 Stale null fields.** `L3` `engine.version.patch/revision` and `build_id_staged_layout` are null, but IDRES (earlier, 2026-10-04) resolved them.

## 11. Missing, unknown or not applicable

- **StaticFindObject**: no address is needed (implemented in the port). ZCode's comparison notes list the game's own StaticFindObject path as not derived (TEAM/worker-re-pc-ps5-comparison/REPORT.md, key finding 2a); nothing in the runtime consumes it. `FUObjectHashTables::Get` has no derived address either.
- **Never run on the console**: FC-1 constructor probe, GMalloc V1/V2, pool predicates (Phase F), SCO/constructor/tick code reads (exec-only pages refuse reads). No receipt exists for `symbols.name_pool`, `fname_ctor_wchar`, `gmalloc`, `static_construct_object_internal`.
- **Unresolved**: UGameEngine::Tick vtable slot; ProcessInternal/ProcessLocalScriptFunction addresses (hooks forced off); FText ctor, FName::ToString, GNatives, ConsoleManager addresses; FHitResult member offsets (ImpactPoint/ImpactNormal; only the 0xF0-byte envelope and default-ctor writes at +0x00/+0x04/+0xA4/+0xD0 are known, GAP/REPORT.md:38-44); all UObject/UStruct/FField/FProperty/UFunction member layouts beyond section 7; FName storage width in layout files; Blocks[0] storage class (static chain says OS heap, a launch-9 observation placed it in the LOAD7 BSS tail, POOL:191-197); whether the allocator thread-forwarder is installed; FEngineVersion singleton address (needed for a live read of the engine version); `pc_build` for 02.003.000 (null in `examples/game-profiles/PPSA33286.json`); UE `BUILD_ID` (absent from the image).

## 12. Observed runtime facts (context for tests, not generator outputs)

GUObjectArray at 0x97ba110 (single capture): ObjFirstGCIndex 35477, ObjLastNonGCIndex 35476, MaxObjectsNotConsideredByGC 35477, OpenForDisregardForGC 0, Objects** 0x13800220a0, PreAllocated null, MaxElements 2162688 (0x210000, = 33 x 0x10000), NumElements 71959, MaxChunks 33, NumChunks 2 (SPL/input/gua-evidence-live.json:14-24). Kernel image spans: 7 intersect the image, 6 readable (GEOM:56-70, IGF6:67). Launch-10 runs 1/3/4: extent [0x400000, 0xc0602c8), `lmf_anchor_count=7`, `lmf_intervals_found=1` (L10:58-60); no run reached seed arming.

## 13. Machine-readable oracle

Keys are paths into the proposed `runtime-profile.json`; compare the leaf `value`. `kind`: `address` = hex string (label or runtime, as the key says; `elf.*` and `*.label` keys are in vaddr space), `offset` = unsigned integer as hex string (also used for sizes, counts, enum values), `bool`, `string`. `tolerance` is always `exact`. Rule for tests: every `symbols.X.runtime` must equal `symbols.X.label + load.base`. Convention rows (`emit.*` goldens, `ue4ss.*` policy, `build.engine.revision_string`) are `designed`: assert them against the golden files, not against the image.

```json
{
  "binding.title_id": {"value": "PPSA33286", "kind": "string", "tolerance": "exact"},
  "binding.content_version": {"value": "02.003.000", "kind": "string", "tolerance": "exact"},
  "binding.master_version": {"value": "01.00", "kind": "string", "tolerance": "exact"},
  "binding.content_id": {"value": "JP0101-PPSA33286_00-TOWNFALLSIEJ0000", "kind": "string", "tolerance": "exact"},
  "binding.param_json.sha256": {"value": "c9c3a6877db124b2a805f9317318ed4e0fd91011039f5c37011e0055e89c5834", "kind": "string", "tolerance": "exact"},
  "binding.executable.bytes": {"value": "0xb93e390", "kind": "offset", "tolerance": "exact"},
  "binding.executable.sha256": {"value": "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc", "kind": "string", "tolerance": "exact"},
  "elf.e_type": {"value": "0xfe10", "kind": "offset", "tolerance": "exact"},
  "elf.e_machine": {"value": "0x3e", "kind": "offset", "tolerance": "exact"},
  "elf.e_entry": {"value": "0x80", "kind": "address", "tolerance": "exact"},
  "elf.e_phoff": {"value": "0x40", "kind": "offset", "tolerance": "exact"},
  "elf.e_phentsize": {"value": "0x38", "kind": "offset", "tolerance": "exact"},
  "elf.e_phnum": {"value": "0xe", "kind": "offset", "tolerance": "exact"},
  "elf.section_table_present": {"value": false, "kind": "bool", "tolerance": "exact"},
  "elf.dt_rela.label": {"value": "0x9653430", "kind": "address", "tolerance": "exact"},
  "elf.rela.entries": {"value": "0x195a9f", "kind": "offset", "tolerance": "exact"},
  "elf.rela.relative_entries": {"value": "0x192b94", "kind": "offset", "tolerance": "exact"},
  "elf.rela.size_bytes": {"value": "0x2607ee8", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph0.p_flags": {"value": "0x1", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph0.p_offset": {"value": "0x4000", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph0.p_vaddr": {"value": "0x0", "kind": "address", "tolerance": "exact"},
  "elf.segments.ph0.p_filesz": {"value": "0x7479b1c", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph0.p_memsz": {"value": "0x7479b1c", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph1.p_flags": {"value": "0x4", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph1.p_offset": {"value": "0x7480000", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph1.p_vaddr": {"value": "0x747c000", "kind": "address", "tolerance": "exact"},
  "elf.segments.ph1.p_filesz": {"value": "0xa3e918", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph1.p_memsz": {"value": "0xa3e918", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph3.p_flags": {"value": "0x6", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph3.p_offset": {"value": "0x7ec0000", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph3.p_vaddr": {"value": "0x7ebc000", "kind": "address", "tolerance": "exact"},
  "elf.segments.ph3.p_filesz": {"value": "0x127e4e8", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph3.p_memsz": {"value": "0x1280000", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph7.p_flags": {"value": "0x6", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph7.p_offset": {"value": "0x9140000", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph7.p_vaddr": {"value": "0x913c000", "kind": "address", "tolerance": "exact"},
  "elf.segments.ph7.p_filesz": {"value": "0x1cc5f2", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph7.p_memsz": {"value": "0x4f7418", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph8.p_flags": {"value": "0x0", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph8.p_offset": {"value": "0x930f420", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph8.p_vaddr": {"value": "0x9633420", "kind": "address", "tolerance": "exact"},
  "elf.segments.ph8.p_filesz": {"value": "0x262cea8", "kind": "offset", "tolerance": "exact"},
  "elf.segments.ph8.p_memsz": {"value": "0x262cea8", "kind": "offset", "tolerance": "exact"},
  "elf.note.gnu_build_id.label": {"value": "0xbc5f250", "kind": "address", "tolerance": "exact"},
  "elf.dynamic.label": {"value": "0xbc5f278", "kind": "address", "tolerance": "exact"},
  "load.file_offset_rule": {"value": "file = p_offset + (vaddr - p_vaddr) per PT_LOAD; delta +0x4000 for ph0,ph1,ph3,ph7; -0x324000 for ph8", "kind": "string", "tolerance": "exact"},
  "load.base": {"value": "0x400000", "kind": "address", "tolerance": "exact"},
  "load.image_extent.start": {"value": "0x400000", "kind": "address", "tolerance": "exact"},
  "load.image_extent.end": {"value": "0xc0602c8", "kind": "address", "tolerance": "exact"},
  "load.image_extent.vaddr_end": {"value": "0xbc602c8", "kind": "address", "tolerance": "exact"},
  "build.configuration": {"value": "shipping", "kind": "string", "tolerance": "exact"},
  "build.non_shipping": {"value": false, "kind": "bool", "tolerance": "exact"},
  "build.stats": {"value": false, "kind": "bool", "tolerance": "exact"},
  "build.configuration_margin": {"value": "0x3c", "kind": "offset", "tolerance": "exact"},
  "build.stats_margin": {"value": "0xc", "kind": "offset", "tolerance": "exact"},
  "build.axes_markers_present": {"value": "0x36", "kind": "offset", "tolerance": "exact"},
  "build.axes_markers_total": {"value": "0xf4", "kind": "offset", "tolerance": "exact"},
  "build.engine.major": {"value": "0x5", "kind": "offset", "tolerance": "exact"},
  "build.engine.minor": {"value": "0x6", "kind": "offset", "tolerance": "exact"},
  "build.engine.patch": {"value": "0x1", "kind": "offset", "tolerance": "exact"},
  "build.engine.changelist": {"value": "0x0", "kind": "offset", "tolerance": "exact"},
  "build.engine.branch": {"value": "UE5", "kind": "string", "tolerance": "exact"},
  "build.engine.revision_string": {"value": "5.6.1-0+++UE5", "kind": "string", "tolerance": "exact"},
  "build.engine.store_site.label": {"value": "0x1ef7db", "kind": "address", "tolerance": "exact"},
  "build.engine.branch_string.label": {"value": "0x7826782", "kind": "address", "tolerance": "exact"},
  "build.build_id": {"value": "04bb4330a7d71dcefe1cc2c4d19769d5", "kind": "string", "tolerance": "exact"},
  "build.allocator_family": {"value": "FMallocBinned3", "kind": "string", "tolerance": "exact"},
  "symbols.object_array.label": {"value": "0x93ba110", "kind": "address", "tolerance": "exact"},
  "symbols.object_array.runtime": {"value": "0x97ba110", "kind": "address", "tolerance": "exact"},
  "symbols.name_pool.label": {"value": "0x9365ec0", "kind": "address", "tolerance": "exact"},
  "symbols.name_pool.runtime": {"value": "0x9765ec0", "kind": "address", "tolerance": "exact"},
  "symbols.name_pool_init_flag.label": {"value": "0x9365ea0", "kind": "address", "tolerance": "exact"},
  "symbols.name_pool_init_flag.runtime": {"value": "0x9765ea0", "kind": "address", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.label": {"value": "0x23d590", "kind": "address", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.runtime": {"value": "0x63d590", "kind": "address", "tolerance": "exact"},
  "symbols.static_construct_object_internal.label": {"value": "0x490aa0", "kind": "address", "tolerance": "exact"},
  "symbols.static_construct_object_internal.runtime": {"value": "0x890aa0", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc.label": {"value": "0x930add8", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc.runtime": {"value": "0x970add8", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_instance.label": {"value": "0x93089b8", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_instance.runtime": {"value": "0x97089b8", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_vtable.label": {"value": "0x7ec26e0", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_vtable.runtime": {"value": "0x82c26e0", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_effective_ptr.label": {"value": "0x9350950", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_effective_ptr.runtime": {"value": "0x9750950", "kind": "address", "tolerance": "exact"},
  "symbols.fmemory_malloc.label": {"value": "0xb7a20", "kind": "address", "tolerance": "exact"},
  "symbols.fmemory_malloc.runtime": {"value": "0x4b7a20", "kind": "address", "tolerance": "exact"},
  "symbols.fmemory_free.label": {"value": "0xb7a90", "kind": "address", "tolerance": "exact"},
  "symbols.fmemory_free.runtime": {"value": "0x4b7a90", "kind": "address", "tolerance": "exact"},
  "symbols.engine_tick.label": {"value": "0x2fac8c0", "kind": "address", "tolerance": "exact"},
  "symbols.engine_tick.runtime": {"value": "0x33ac8c0", "kind": "address", "tolerance": "exact"},
  "symbols.process_event.label": {"value": "0x459830", "kind": "address", "tolerance": "exact"},
  "symbols.process_event.runtime": {"value": "0x859830", "kind": "address", "tolerance": "exact"},
  "symbols.fnamepool_init.label": {"value": "0x23a5d0", "kind": "address", "tolerance": "exact"},
  "symbols.fname_intern.label": {"value": "0x23d600", "kind": "address", "tolerance": "exact"},
  "symbols.fname_core.label": {"value": "0x242470", "kind": "address", "tolerance": "exact"},
  "symbols.fnamepool_reserve.label": {"value": "0x246090", "kind": "address", "tolerance": "exact"},
  "symbols.fname_default_name_store.label": {"value": "0x23c0b0", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_instance_ctor.label": {"value": "0xafd40", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_factory.label": {"value": "0xb3f0", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_bootstrap_store_site.label": {"value": "0xb660", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_malloc_impl.label": {"value": "0xb48e0", "kind": "address", "tolerance": "exact"},
  "symbols.gmalloc_slow_path.label": {"value": "0xb1460", "kind": "address", "tolerance": "exact"},
  "symbols.sco_params_ctor.label": {"value": "0x493530", "kind": "address", "tolerance": "exact"},
  "symbols.sco_init.label": {"value": "0x493580", "kind": "address", "tolerance": "exact"},
  "symbols.process_event_empty_stub_slot76.label": {"value": "0xb290", "kind": "address", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.abi": {"value": "sysv-amd64; rdi=out FName*; rsi=const char16_t*; edx=EFindName", "kind": "string", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.find_arg_at_callsites": {"value": "0x1", "kind": "offset", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.prologue_32": {"value": "554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a6690", "kind": "string", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.callsites": {"value": "0x174d6af,0x174d6d8,0x2f07077,0x2f85635,0x3a17b91,0x3a17bbc,0x3a17be7,0x3a17c50,0x3a17ca6", "kind": "string", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.anchor_modules": {"value": "Engine,Renderer,AnimGraphRuntime,Landscape,RenderCore", "kind": "string", "tolerance": "exact"},
  "symbols.fname_ctor_wchar.anchor_string_labels": {"value": "Engine=0x782706e,Renderer=0x799db12,AnimGraphRuntime=0x75eefc0,Landscape=0x763151e,RenderCore=0x79f02e2", "kind": "string", "tolerance": "exact"},
  "symbols.static_construct_object_internal.prologue_16": {"value": "554889e54157415641554154534881ec", "kind": "string", "tolerance": "exact"},
  "symbols.static_construct_object_internal.anchor_string_label": {"value": "0x7999084", "kind": "address", "tolerance": "exact"},
  "symbols.engine_tick.prologue_23": {"value": "554889e54157415641554154534881ec880000004c8b2d", "kind": "string", "tolerance": "exact"},
  "symbols.engine_tick.anchor_xref_label": {"value": "0x2facbae", "kind": "address", "tolerance": "exact"},
  "symbols.process_event.prologue_24": {"value": "554889e54157415641554154534883e4e04881ec20010000", "kind": "string", "tolerance": "exact"},
  "symbols.static_find_object.address_applicable": {"value": false, "kind": "bool", "tolerance": "exact"},
  "symbols.fname_to_string.resolved": {"value": false, "kind": "bool", "tolerance": "exact"},
  "structs.FName.size": {"value": "0x8", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.current_block": {"value": "0x8", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.current_byte_offset": {"value": "0xc", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.blocks": {"value": "0x10", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.hash_buckets": {"value": "0x10040", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.hash_bucket_count": {"value": "0x100", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.hash_bucket_stride": {"value": "0x40", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.none_id": {"value": "0x14040", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.byte_property_id": {"value": "0x14044", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.block_size": {"value": "0x20000", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.max_blocks": {"value": "0x2000", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.block_offset_bits": {"value": "0x10", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.entry_stride": {"value": "0x2", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.default_name_count": {"value": "0xec", "kind": "offset", "tolerance": "exact"},
  "structs.FNamePool.init_flag_offset": {"value": "-0x20", "kind": "string", "tolerance": "exact"},
  "structs.FNameEntry.header": {"value": "u16 LE: bit0 wide; bits5:1 probe hash; bits15:6 len", "kind": "string", "tolerance": "exact"},
  "structs.FUObjectArray.ObjFirstGCIndex": {"value": "0x0", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.ObjLastNonGCIndex": {"value": "0x4", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.MaxObjectsNotConsideredByGC": {"value": "0x8", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.OpenForDisregardForGC": {"value": "0xc", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.ObjObjects": {"value": "0x10", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.UObjectCreateListeners": {"value": "0x68", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.UObjectDeleteListeners": {"value": "0x78", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.PrimarySerialNumber": {"value": "0xb0", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.bShouldRecycleObjectIndices": {"value": "0xb4", "kind": "offset", "tolerance": "exact"},
  "structs.FUObjectArray.UEP_TotalSize": {"value": "0xb8", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.Objects": {"value": "0x0", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.PreAllocatedObjects": {"value": "0x8", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.MaxElements": {"value": "0x10", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.NumElements": {"value": "0x14", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.MaxChunks": {"value": "0x18", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.NumChunks": {"value": "0x1c", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.UEP_TotalSize": {"value": "0x20", "kind": "offset", "tolerance": "exact"},
  "structs.TUObjectArray.elements_per_chunk": {"value": "0x10000", "kind": "offset", "tolerance": "exact"},
  "vtables.UObject.dtor_prefix_slots": {"value": "0x2", "kind": "offset", "tolerance": "exact"},
  "vtables.UObject.ProcessEvent": {"value": "0x268", "kind": "offset", "tolerance": "exact"},
  "vtables.UObject.ProcessEvent_slot_index": {"value": "0x4d", "kind": "offset", "tolerance": "exact"},
  "vtables.FMalloc.Malloc": {"value": "0x30", "kind": "offset", "tolerance": "exact"},
  "vtables.FMalloc.Free": {"value": "0x50", "kind": "offset", "tolerance": "exact"},
  "emit.NativeLayout.header": {"value": "[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=6\nNonShipping=0\nStats=0\nEngineRevision=5.6.1-0+++UE5\nBuildId=04bb4330a7d71dcefe1cc2c4d19769d5\n", "kind": "string", "tolerance": "exact"},
  "emit.VTableLayout.identity_only.bytes": {"value": "0xba", "kind": "offset", "tolerance": "exact"},
  "emit.VTableLayout.identity_only.sha256": {"value": "9c20f7c9b1a1ea9a665db12df82ee7e62694fe4cd207cf53a44116e124fbad86", "kind": "string", "tolerance": "exact"},
  "emit.VTableLayout.with_process_event.bytes": {"value": "0xd8", "kind": "offset", "tolerance": "exact"},
  "emit.VTableLayout.with_process_event.sha256": {"value": "37a7d86036106d1042c10c10e11f66edef4ba7c2b2a2948c8aaeea719ee9a8a5", "kind": "string", "tolerance": "exact"},
  "emit.MemberVariableLayout.bytes": {"value": "0x24c", "kind": "offset", "tolerance": "exact"},
  "emit.MemberVariableLayout.sha256": {"value": "f291d552418dda916ec28675bd9e40d78bf1d3e9f75ce0c36df6b95dcfa63f32", "kind": "string", "tolerance": "exact"},
  "ue4ss.settings.EngineVersionOverride.MajorVersion": {"value": "0x5", "kind": "offset", "tolerance": "exact"},
  "ue4ss.settings.EngineVersionOverride.MinorVersion": {"value": "0x6", "kind": "offset", "tolerance": "exact"},
  "ue4ss.settings.Hooks.HookProcessInternal": {"value": false, "kind": "bool", "tolerance": "exact"},
  "ue4ss.settings.Hooks.HookProcessLocalScriptFunction": {"value": false, "kind": "bool", "tolerance": "exact"}
}
```
