# ADAPTER-FRAMEWORK.md — the reusable three-layer adapter stack, mapped onto this project's actual artifacts
(worker-re-pc-ps5-comparison, 2026-10-04. Case B deliverable 1. Classification and reuse
notes only; every maturity claim is the source ledger's own, never strengthened.)

## 0. The three layers

The project's own direction entry (`ue4ss_payload_direction`) already states the target
architecture: "a shared UE4SS-compatible Lua runtime plus an engine backend is the
reusable architecture." Made concrete as a stack:

```
+--------------------------------------------------------------------------+
| L3  GAME-BUILD PROFILE        (per title+platform+update, data-only)     |
|     pinned addresses/constants + provenance + validation status          |
|     consumed by L2; regenerable by re-running the derivation tooling     |
+--------------------------------------------------------------------------+
| L2  ENGINE ADAPTER            (per engine version+platform family)       |
|     UE4SS-compatible runtime: object/FName/construct/spawn/ProcessEvent  |
|     abstractions over engine singletons and calls; slot/filling model    |
+--------------------------------------------------------------------------+
| L1  PS5 COMPATIBILITY RUNTIME (per target platform; partly host-side)    |
|     makes PS5 + a Mac host behave enough like the Windows load env that  |
|     Win64-ABI mod code can be built, verified, loaded, and driven        |
+--------------------------------------------------------------------------+
        ^                                ^                          ^
   L1 feeds L2 a callable env    L2 consumes L3 constants    L3 is per-title
```

Design law the layers enforce (from the ledger, all still true):
- No per-mod rewriting: a new mod touches nothing below the Lua surface (L2's Lua API).
- A new game title touches ONLY L3 (a new profile), provided L2's abstractions already
  cover that engine version; a new engine version extends L2, not L1.
- Every L3 constant carries a provenance class and an in-game validator design; nothing
  is "known" without its validation record (`SEED-INTEGRATION.md` law).

---

## 1. L1 — PS5 compatibility runtime (artifacts → layer, reuse notes)

| artifact (ledger entry / dir) | what it actually is | layer role | reuse note / gap |
|---|---|---|---|
| `windows_image_preparation` → `outputs/ps5-mod-bridge/windows-image/` `pe_image.c/.h` + `windows-image-original-inputs-1/` | PE image parser/loader (IAT, relocations incl. duplicate/overlapping fixes, exports), 34 focused groups, verified against the real UE4SS `dwmapi.dll` + a compiler-built fixture | L1 core: PE **loader** (host + portable) | Reusable directly for any Win64-image intake; on-target loading still unproven (EXEC pages untested). PC-comparison reuse: this parser doubles as the PE-side structural reader (what lief/Ghidra would confirm) |
| `callable_pe_runtime` → `outputs/2026-10-04-callable-pe-runtime/` | owned PE host execution: image loaded, 6 imports called, output captured, clean unload; Win64-ABI objects compiled via "FreeBSD9 C frontend → unedited win64cc IR → PS5 LLVM backend" | L1 core: PE **executor** + the Win64-ABI→PS5 **compile route** | The compile route is the stack's most reusable piece: it compiles Windows-ABI source to PS5 objects without editing IR. Gap recorded by ledger: no justified engine edit yet; game-context/ABI unproven on target |
| `windows_entry_runtime` → `outputs/2026-10-04-windows-entry-runtime/` | owned DllMain lifecycle: 4 attach/detach cases, rejected-attach detach, cleanup retry, busy-close preserved entry | L1: **entry/lifecycle** semantics | Matches the DllMain-driven UE4SS load model; lifecycle edge cases already regression-tested |
| `windows_worker_runtime` → `outputs/2026-10-04-dll-worker-runtime/` (+ SOURCE-MANIFEST) | real host PE workers under exact Win64 ABI; worker pins through join; closed-handle semantics; 16 rejected-attach controls | L1: **threading/worker** semantics | Reuse for ExecuteInGameThread-style worker queues; target-side execution still `false` in every receipt |
| `native_dll_entry_clock_notify` + `native_dll_clock_composition` → `windows-clock-1/`, `owned-dll-sdk-candidate-1/` | QPC/QPF implemented over `clock_gettime(CLOCK_MONOTONIC)`; composed into owned entry DLLs; 106 commands passed | L1: **Windows API service shim** (clock first) | Pattern for the rest of the Win64 service surface (see also `2026-10-04-windows-tls-last-error/`, `2026-10-04-windows-heap-process/`, `2026-10-04-windows-module-loading/`, `2026-10-04-dll-synchronization/`, `2026-10-04-owned-dll-clock/`, `2026-10-04-owned-dll-console/`, `2026-10-04-tick-file-owner/`, `2026-10-04-runtime-*` dirs) — each dir is one shimmed service with its receipt |
| `native_dll_candidate_and_tick_fix` → `pe-ps5-memory-1/`, `ue4ss-native-tick-affinity-1/` | direct `sceKernelMprotect` exact-range syscall adapter; composed SDK-payload ELF (crt1 `_start`); tick-affinity runtime (359 CUs) | L1: **native memory-protection adapter** | EXEC permission on target explicitly UNTESTED — the single biggest L1 unknown; reuse = the adapter code itself, not any proven target behavior |
| igf6 vmmap + window-gate facts (FNAME-VERDICT §2, SEED-TABLE base) | in-game observed: text maps **execute-only** (0x400000–0x787c000 prot=0x4), RELRO RO, RW tail; live base 0x400000 proven | L1: **target execution-environment facts** | These are the constraint that forces all L2/L3 validation to be data-side; any PC-comparison plan must respect that the PS5 side cannot read its own text in-game |

L1 summary: loading/parsing/executing Win64 images on the Mac host is VERIFIED (owned
fixtures); Win64→PS5 compilation is VERIFIED; the target-side half (EXEC pages, real
module load on PS5) is designed and compiled but never executed. L1 is reusable as the
"PS5 pretends to be Windows enough for mod code" substrate, with that honesty label.

---

## 2. L2 — engine adapter (UE4SS-compatible runtime + engine backend)

| artifact | what it actually is | layer role | reuse note / gap |
|---|---|---|---|
| UE4SS composition-6 "coherent runtime" (`ue4ss_current_engine_focus` stage22; `native_tick_mapped_connection` artifact 117 MB .o, 360 runtime units) | the ported UE4SS: 350 production C/C++ units + 17-export Rust runtime; 197 undefined imports unchanged across revisions | L2 body | The reusable UE4SS-compatible runtime itself. Gap (ledger, repeated): no real engine game-thread execution, no valid UObject lookup/lifetimes on target yet |
| Seed-table mechanism (`SEED-TABLE.json`, `SEED-INTEGRATION.md`, launch-10 armed table) | the **slot model**: L2 declares named engine dependencies (slot 0 GUA, 2 FName ctor, 3 GMalloc, 4 SCO…); L3 fills labels; initializer binds at runtime behind per-slot validators; `T-REF` = loud refutation, never silent | L2's **dependency-injection surface** | This is the single most reusable L2/L3 seam: a PC profile for the same title would fill the SAME slots with PC constants; validators transfer conceptually (PC side can read its own code pages, so PC validators are easier) |
| Lua surface (`ue4ss_payload_direction`): `mb_lua_install_require/invalid_object`, `mb_input_install/dispatch`, `mb_async_queue_bind/pump`; host providers `lba_provider`, `lba_bool_provider` | the mod-facing API + host-side stand-ins | L2 mod API | Providers are Int32/Boolean host data ONLY — explicitly not engine objects; the engine-backed replacements are the missing piece |
| Tick delivery family: `ue4ss-native-tick-affinity-1`, `ue4ss-native-tick-witness-1`, `ue4ss-native-tick-file-owner-1`, `ue4ss-native-tick-cfg-refusal`, R3 marshal tier, PLH Prospero detour layer, vtable backend seam (`worker-launch11-*`) | the whole game-thread-pump saga: CFG refusal measured, relocatable compositions, file-witness API, R3 zero-syscall pump design + diffs, detour layers behind syscall-74 verdict | L2: **game-thread execution backend** (the `ExecuteInGameThread` the Item Spawner needs) | Nothing here has executed on target; the R3 verdict says the acceptance ladder cannot complete as composed today. This is L2's critical gap AND exactly the mod's blocking need |
| FName machinery (port `NameTypes.hpp` construct_with_string/ConstructorInternal; `FNAME-VERDICT.md`; fc1 materialize/prep) | FString→FName ctor binding with silent-null hazard identified + remedy (bound ctor 0x63d590) + FC-1 spec | L2: **name resolution** | The silent-failure audit pattern (no-op ctor → ambiguous negative) is reusable on any platform; on PC the ctor is findable by equivalent anchors |
| GUA iteration + FindFirstOf (`port/unreal/src/UObjectGlobals.cpp`, NameTypes comparison-by-index) | object-array walk + FName-equality search | L2: **object lookup** | Depends on slot 0 (validated 0x97ba110) + FName ctor (slot 2) — the F7 blocker chain |
| `townfall-ps5-ue56.profile.json` (`worker-2-layout-facts`) | identity/layout/engine axes + static survey endpoints + plans of record (e.g., Kismet Conv_NameToString fallback) | L2↔L3 **profile schema** (straddles layers: schema is L2, this instance is L3) | The schema is the reusable part: a PC profile instantiates the same schema |

L2 summary: the runtime exists, is composed, and links; its engine backend is the
unproven part (tick/detour/reflection on target). The slot/filling model is the
well-defined seam where L3 plugs in.

---

## 3. L3 — game-build profile (Townfall PS5, sha-pinned instance)

| artifact | content | provenance class (as recorded) |
|---|---|---|
| `decrypted-eboot.bin` (+ prefix, rela_index.bin, region-names.json, ctor-windows.json, lea-refs.json) | the pinned image: ELF64 x86-64, e_type 65040, no section table (no symbols), 5 PT_LOADs, DT_RELA 1,661,599 relative relocs @ file 0x932F430, UE 5.6 | static capture, sha `3b6cfad4…3622fc`, re-verified by every consuming worker |
| GUA `0x93ba110` linked → runtime `0x97ba110` | GUObjectArray; layout A, 71,959 objects, 2 chunks | **validated in-game** (window gate + GUA anchor) |
| FNamePool `0x9365ec0` → `0x9765ec0` (+ interning core `0x242470`, init `0x23a5d0`, find `0x23c0b0`, add/probe `0x23c340`, hash probe pool+0x10040, 64-byte buckets, Blocks[0] at pool+0x10, ids at pool+0x14040) | FNamePool singleton + UE5 FNamePool::Init behavior (interns None/ByteProperty/IntProperty) | **derived-static** (offline, multi-site); pool facts themselves never read in-game |
| FName wchar ctor `0x23d590` → `0x63d590` (+ chain 0x23d590→0x23d600→0x23e0d0→0x242470; bytes frozen) | FName constructor (SysV sret) | derived-static; fc1 exact-byte B1/B2 spec pending runtime probe |
| SCO `0x490aa0` → `0x890aa0` (+ neighbors 0x493580/0x493530 from the 1,918-hit string-xref census) | StaticConstructObjectInternal | **recorded in-game** (runtime cited); pinned-label uniqueness owned by seed derivation |
| GMalloc `0x930add8` → `0x970add8` (+ FMallocBinned3 instance global `0x93089b8`, vtable `0x7ec26e0`, 11 FMemory thunks, 4 UTF-16 literals, Binned3 constants) | FMalloc* global pointer | derived-static-strong → **armed** (launch 10: `slot 3 gmalloc bound at 0x930add8`); in-game validator V1/V2 defined |
| GameEngineTick `0x2fac8c0` linked (+ seed CLAIM input's predicted 0x37AC8C0 figure) | UGameEngine::Tick anchor | validated-static (profile) / predicted (seed) — two records, kept distinct |
| pak/utoc/manifest/zen-header captures (`townfall-main-pak-full-2026-10-01`, `-main-utoc-`, `-manifest-capture-`, `-original-zen-headers-1`, leon intake) | content-layer profile facts (PAK v11 wrappers, utoc overlap) | static captures |
| derivation tooling (`worker-seed-table-derivation/derive*.py`, `worker-gmalloc-derivation/derive*.py`, survey/profile endpoints) | the REGENERATOR for this profile | scripts + receipts; reproduce constants from the pinned image |

L3 summary: this layer is exactly what the mission's pinned-address list names, plus its
provenance. It is regenerable, and its slot/validator discipline is the model for a
future PC-side L3 profile.

---

## 4. What a PC column adds to each layer (preview; details in COMPARISON-PROTOCOL.md)

- L1: mostly **nothing needed on PC** — Windows hosts the UE4SS release natively; the
  retained Win64 UE4SS zip IS the PC L1+L2 reference implementation. L1's PE loader/
  entry/worker/clock shims become conformance test fixtures instead of shims.
- L2: same runtime, different backend mode — on PC, UE4SS's own Win64 backend provides
  the tick/reflection execution our PS5 backend lacks; comparing the PC image tells us
  what the PS5 backend must replicate.
- L3: a parallel `townfall-pc-ue56` profile (same schema, same slots, PE/PE32+ anchors
  instead of ELF/RELA anchors). Because PC text is readable+executable, PC-side slot
  validation is *easier*, and PC evidence can pre-verify the semantic identity of each
  slot's derivation method before the PS5 in-game probes spend their budget.

The comparison the mission wants is therefore not "PC vs PS5 as separate puzzles" but
**the same slot table filled twice** — that is the reuse the seed table already enables.
