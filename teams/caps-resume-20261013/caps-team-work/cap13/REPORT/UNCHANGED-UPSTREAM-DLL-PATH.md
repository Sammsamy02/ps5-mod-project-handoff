# cap13 — The unchanged-upstream-DLL path: what exists, what executes, and the one-row ABI gap

Agent KH (ZCode), 2026-10-13, cap13 packet. Mac-only analysis. No console
contact, no console artifacts touched, nothing executed from any analyzed
binary. Responds to the reviewer directive: *"Prioritize one unchanged
upstream DLL through the actual compatibility path."*

Evidence labels (cap13 convention):
- **[LOCAL-SOURCE]** — file in this worktree, path (+ line/offset where cited).
- **[LOCAL-VERIFY]** — fact produced on this Mac today; method + artifact quoted.
- **[TRAINING-KNOWLEDGE]** — version-sensitive external fact; re-verify before dependence.
- **[INFERENCE]** — my reasoning from cited evidence.

---

## 0. Verdict (read this first)

1. **No genuine unchanged upstream UE4SS C++ DLL *mod* exists on this Mac.**
   The complete inventory (§1) is: the UE4SS **framework** itself (unchanged
   upstream, but it is the host runtime, not a mod), its `dwmapi.dll` loader
   proxy, 12 hand-built cap13 `mbm_mod_entry_v1` fixture DLLs, 3 struct-built
   PE fixtures, and KB's census **object file** (never linked into a DLL,
   never executed). Nothing here has ever been tested through the
   compatibility path, and nothing here can be honestly called "an unchanged
   upstream DLL mod run."
2. **The closest artifact to a real upstream mod is KB's reconstruction**
   (`cme_upstream_shape`), and its import census is fully verified against the
   real framework binary: **13/13 UE4SS.dll symbols present, string-for-string,
   in the actual export table** — I re-verified all 13 today by parsing the
   export directory of `UE4SS.dll` (sha256 `94c2ebd3…`) directly from file
   bytes, independent of the analysis JSON (§2, §3).
3. **The decisive gap is one missing host row: object construction.** The mod
   calls `RC::Unreal::UObjectGlobals::StaticConstructObject(const
   FStaticConstructObjectParameters&)` (verified in the real EAT, ordinal
   3562). The PS5 port **already implements** that exact function
   ([LOCAL-SOURCE] typescan-r4 `src/UObjectGlobals.cpp:146-197`), but
   `mbm_host_v1` exposes **no construct service** ([LOCAL-SOURCE]
   `BUILD/src/sdk/mbm/NativeModAbi.h:203-240`) and the host services file has
   no construct override ([LOCAL-SOURCE] `NativeModEngineServices.inc` service
   list). This is an ABI-exposure gap, not an engine gap.
4. **The staged #9 workaround for that gap cannot work.**
   `BUILD/src/port_cheat_manager_enabler.cpp` constructs via
   `h->call_function(controller, "StaticConstructObject", …)`, but
   `call_function` resolves names through engine reflection only
   (`GetFunctionByNameInChain`, `NativeModEngineServices.inc:352-356`) and
   `StaticConstructObject` is a UE4SS C++ global, not a UFunction — that call
   deterministically returns `MBM_E_NOT_FOUND`. [LOCAL-SOURCE: both files;
   INFERENCE on resolution, certain from the implementation.]
5. **The one-row fix** (§3.4): add `construct_object(ctx, mod, class_object,
   outer_object, mbm_object* out)` to `mbm_host_v1`, appended at table end per
   the header's own only-grow rule, backed by the existing runtime
   dispatcher, with an honest admission row (creation is a state change —
   `MBM_ROUTE_DATA_ONLY` is not claimable).

---

## 1. STEP 1 — Inventory: every Windows PE with a UE4SS interface on this Mac

Complete enumeration of candidate binaries, with their true nature:

| # | Artifact | Path | Nature | Upstream? | Mod? |
|---|---|---|---|---|---|
| 1 | `UE4SS.dll` (20,572,160 B) | `what/.superpowers/native-dependency-spike/windows-image-original-inputs-1/UE4SS.dll` | Real x64 PE32+ framework, 4256 named exports, 634 imports / 29 DLLs, sha256 `94c2ebd3ca9f4c4212898d727df5482937f586771c67502635ebd4367422052a` [LOCAL-VERIFY: direct PE parse today; sha256 from `cap13/REPORT/evidence/UE4SS.dll.analysis.json`] | **Yes — unchanged upstream** | **No — this is the framework the mods plug into** |
| 2 | `dwmapi.dll` (279,552 B) | same directory | Upstream loader proxy: exports the real dwmapi surface, resolves the system DLL via `LoadLibraryW`/`GetProcAddress` [LOCAL-SOURCE: `REPORT/mod-selection.md` "Evidence" section] | Yes — unchanged upstream | No — load-order shim for the framework |
| 3 | 12 `cap13-mod-*.dll` (SAMPLE/) | `cap13/SAMPLE/` | #9-ABI fixtures. Verified example `cap13-mod-cheat-manager-enabler.dll`: exports **only** `mbm_mod_entry_v1`; imports only `kernel32!{GetProcessHeap,HeapAlloc,HeapFree}` + `vcruntime140!{__CxxFrameHandler3,__std_terminate}`; analyzer record says `imports_ue4ss_dll: false` [LOCAL-VERIFY: `llvm-objdump -p` today + `REPORT/evidence/cap13-mod-cheat-manager-enabler.json`] | No — Apple clang builds of PS5-side ports | Mod-shaped, but against OUR C ABI, not the upstream UE4SS C++ ABI |
| 4 | 3 `FIXTURES/*.dll` | `cap13/FIXTURES/` | Hand-crafted struct-built PEs, deterministic, "clearly labeled fixture … not a real-mod pass" [LOCAL-SOURCE: `REPORT/evidence/fixture-build-manifest.json`; `REPORT/mod-selection.md` verdict] | No | No |
| 5 | `cme_upstream_shape.obj` | `cap13/REPORT/evidence/simplest-mod-census/` | KB's CheatManagerEnabler **reconstruction**, compiled to COFF object with Apple clang `-target x86_64-pc-windows-msvc -fno-exceptions`; **never linked into a DLL, never executed** [LOCAL-SOURCE: the `.cpp` header comment lines 1-27; `cme_upstream_shape.imports.json` "compiler" field] | No — labeled reconstruction in its own source | Mod-shaped, upstream API surface, but a census artifact |

Also searched and confirmed empty: the vendored upstream `Mods/` tree
(`what/.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/ue4ss-v1/ue4ss/Mods/`)
contains **8 Lua mods** (BPML_GenericFunctions, BPModLoaderMod,
CheatManagerEnablerMod, ConsoleCommandsMod, ConsoleEnablerMod, Keybinds,
LineTraceMod, SplitScreenMod per `mods.txt`) and **zero DLLs** [LOCAL-VERIFY:
`find … -name "*.dll"` returned nothing]. `EventViewerMod` /
`KismetDebuggerMod` (upstream's C++ examples) are not vendored anywhere in
this worktree and could not be compiled here anyway (no Windows SDK /
MSVC STL) [LOCAL-SOURCE: reconstruction `.cpp` header line 20: "no MSVC
STL/CRT headers on this Mac"].

**Answer to the reviewer's premise:** the only unchanged upstream binary that
uses the UE4SS API is the framework itself. "Prioritize one unchanged upstream
DLL" can mean either:

- **(a) Stress the loader with the framework**: map `UE4SS.dll` through the PE
  path. Legitimate loader test (29-DLL import chain, 4256 exports), but it
  proves nothing about the mod ABI — UE4SS.dll *provides* the mod surface, it
  does not *consume* it, and it is explicitly not a mod (`start_mod` is a
  mod-provided entry; UE4SS.dll does not export it [LOCAL-SOURCE:
  `REPORT/mod-selection.md` §"binary interface contract"]).
- **(b) Run the closest mod-shaped artifact**: KB's reconstruction census
  defines the exact upstream-shape consumer the shim must satisfy. This is
  what the rest of this report maps.

Both are honest; neither is "an unchanged upstream DLL mod executed," and no
report may claim otherwise.

---

## 2. STEP 2 — The actual compatibility path for the KB reconstruction

### 2a. Every import, mapped to what our PS5 port has

Census: 18 distinct undefined symbols [LOCAL-SOURCE:
`REPORT/evidence/simplest-mod-census/cme_upstream_shape.imports.json`;
raw dump `cme_upstream_shape.undef.txt`]. All 13 UE4SS symbols re-verified
today against the real export table by direct parse of `UE4SS.dll` bytes
(`llvm-objdump`/struct-walk; method and ordinals below). [LOCAL-VERIFY]

**UE4SS.dll imported functions (7, all `__imp_` thunks):**

| # | Mangled export (verified ordinal in real UE4SS.dll) | Demangled meaning | What our PS5 port has | Layer that must provide it on the PE route |
|---|---|---|---|---|
| 1 | `??0CppUserModBase@RC@@QEAA@XZ` (ord 45) | `RC::CppUserModBase::CppUserModBase()` | SysV-layout SDK base exists (cap09 sdk); **MSVC-layout shim subobject is design-stage**, `kLayoutPinned = false` [LOCAL-SOURCE: `native-loader/src/ue4ss_compat.cpp:50-61`] | ue4ss_compat: value-init base subobject at pinned MSVC layout, install shim vftable image |
| 2 | `??1CppUserModBase@RC@@UEAA@XZ` (ord 192) | virtual dtor | same as above | ue4ss_compat: destroy shim fields; free via guest allocator pairing [LOCAL-SOURCE: ue4ss_compat.cpp "allocator pairing"] |
| 3 | `??0FStaticConstructObjectParameters@Unreal@RC@@QEAA@PEBVUClass@12@PEAVUObject@12@@Z` (ord 120) | `FStaticConstructObjectParameters(const UClass*, UObject*)` | upstream 2-arg ctor exists in the mod-facing header [LOCAL-SOURCE: `ue4ss-unreal-source-access-1/upstream/include/Unreal/UObjectGlobals.hpp:127`] | ue4ss_compat shim-side struct (shim owns layout; mod treats it opaquely — see §3.3 caveat) |
| 4 | `?StaticConstructObject@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@AEBUFStaticConstructObjectParameters@23@@Z` (ord 3562) | **construct an engine object** | **IMPLEMENTED in the port runtime** [LOCAL-SOURCE: typescan-r4 `src/UObjectGlobals.cpp:146-197`]; **NOT reachable**: no `mbm_host_v1` row, no `ue4ss_compat` row (grep for `construct_object`/`StaticConstructObject` in `native-loader/src/ue4ss_compat.cpp` returns nothing) — **the decisive gap (§3)** | ue4ss_compat row → new `construct_object` host row → existing runtime dispatcher |
| 5 | `?FindFirstOf@UObjectGlobals@Unreal@RC@@YA…basic_string@_W…` (ord 1149) | `UObject* FindFirstOf(const std::wstring&)` | `host->find_first_of` implemented with liveness guard [LOCAL-SOURCE: `NativeModEngineServices.inc:276`] | ue4ss_compat: read guest MSVC `wstring` (SSO-8, 32 B) → UTF-8 → `host->find_first_of` |
| 6 | `?GetValuePtrByPropertyNameInChain@UObject@Unreal@RC@@QEAAPEAXPEB_W@Z` (ord 2289) | `void* UObject::GetValuePtrByPropertyNameInChain(const wchar_t*)` | `host->property_address` implemented [LOCAL-SOURCE: `NativeModEngineServices.inc:337`] | ue4ss_compat: wchar* → UTF-8 → `host->property_address` |
| 7 | `?RegisterHook@UObjectGlobals@Unreal@RC@@YA?AU?$pair@HH@std@@…function@…` (ord 2908) | `std::pair<int,int> RegisterHook(const std::wstring&, std::function<…>, std::function<…>, void*)` | `host->register_ufunction_hook` implemented [LOCAL-SOURCE: `NativeModEngineServices.inc:393` `install_ufunction_hook`] | ue4ss_compat + bridge: **aggregate-8 return re-pack (Win64 RAX vs SysV RAX:RDX) and MSVC `std::function` by-value invocation — both explicitly refused by the current bridge** [LOCAL-SOURCE: `native-loader/src/win64_bridge.h` "PER-SIGNATURE HONESTY" block; feasibility §2a note] |

**UE4SS.dll vtable-referenced virtuals (6 direct refs)** — the mod overrides
`on_unreal_init` (mod-local, not imported); the other six are imported direct
symbols: `?on_update@CppUserModBase@RC@@UEAAXXZ`, `?on_ui_init@…`,
`?on_program_start@…`, `?on_dll_load@…basic_string_view…`,
`?render_tab@…`, `?on_cpp_mods_loaded@…` [LOCAL-SOURCE: undef.txt lines 4-9;
all verified ordinals 4100-4192]. Host side: `mbm_mod_v1` already carries
every one of these lifecycle rows [LOCAL-SOURCE:
`BUILD/src/sdk/mbm/NativeModAbi.h:255-260`]; PE side: `pe_adapter.cpp`
currently returns `MBM_E_UNAVAILABLE` from the slot dispatch placeholder
[LOCAL-SOURCE: `native-loader/src/pe_adapter.cpp:64-71`] — pinned slot table
pending (`tools/pin_msvc_layout.sh` verified as a mechanism, output not yet
baked).

**kernel32 (2):** `DisableThreadLibraryCalls` (T1 NOOP-log TRUE row),
`GetModuleHandleW` (T3 module-registry row; `GetModuleHandleW("UE4SS.dll")`
must resolve to the ue4ss_compat surface) [LOCAL-SOURCE:
`native-loader/src/win_api_shim.h:3-11` tier policy, T3 registration path].

**CRT tier (3):** `??2@YAPEAX_K@Z` (operator new), `??3@YAXPEAX@Z` (operator
delete), `??_7type_info@@6B@` (RTTI vtable, `/GR` default). A real `/MD` mod
instead imports the `MSVCP140`/`VCRUNTIME140`/`api-ms-win-crt-*` family (the
framework image itself carries 207 MSVCP140 imports [LOCAL-SOURCE:
`REPORT/evidence/UE4SS.dll.analysis.json` imports]); an `/MT` mod statically
links its CRT and needs shim heap/time rows. Tier classification is
census-driven [LOCAL-SOURCE: blocker-list B8].

### 2b. The layer stack that must jointly deliver the above

[LOCAL-SOURCE: `cap13/DESIGN/native-dll-loader.md`; staged sources in
`cap13/native-loader/src/`, all design-grade/unbuilt except where noted]

```
pe_analyzer.py        census + refusal classification (BUILT, evidence JSONs exist)
pe_loader.{h,cpp}     PE32+ map → 16 KiB permission partition → relocs → import resolve  (design-grade)
win64_bridge.{S,h}    SysV <-> Win64 thunk pairs; refuses aggregates/std::function today    (asm UNVERIFIED)
win_api_shim.{h,cpp}  T1/T2/T3 kernel32 rows + T4 fail-closed EH + T5 refusal               (design-grade)
ue4ss_compat.cpp      "UE4SS.dll" export surface: 13 rows for this mod — SKETCH, zero rows  (design-grade)
pe_adapter.cpp        guest vtable slots -> mbm_mod_v1 lifecycle; placeholder dispatch      (design-grade)
mbm_host_v1           the #9 C table -> NativeModEngineServices.inc implementations         (BUILT in port; no construct row)
```

Mac gates before any of this reaches the console: bridge round-trip tests +
loader-vs-analyzer agreement (blockers B3/B4), then J1 (executable anonymous
memory in the game process) which gates the whole route [LOCAL-SOURCE:
`REPORT/blocker-list.md` B1-B4 and unblock order].

---

## 3. STEP 3 — The decisive gap, source-located

### 3.1 Where the call appears in the mod's compiled binary

Artifact: `cap13/REPORT/evidence/simplest-mod-census/cme_upstream_shape.obj`
(COFF x86-64, Apple clang 17, `-fno-exceptions`; census-only artifact — its
code has never been linked or executed; honesty per its own header).

[LOCAL-VERIFY: `xcrun llvm-objdump -d` + `-r` on the object today]

- Function: `?enable_cheat_manager@?A0xC310C454@@YAXAEAVUnrealScriptFunctionCallableContext@Unreal@RC@@PEAX@Z`
  (anonymous-namespace callback registered as the ClientRestart post-hook),
  `.text` section offset **0x210**.
- The construction sequence:
  - `.text+0x2D4`: `mov r8, [rsp+0x60]` — outer = `ctx.Context` (PlayerController)
  - `.text+0x2D9`: `mov rdx, [rsp+0x48]` — class = CheatManager class object
  - `.text+0x2DE`: `lea rcx, [rsp+0x37]` — params struct slot
  - `.text+0x2E3`: `call qword ptr [rip+disp]`, **relocation at `.text+0x2E5` →
    `__imp_??0FStaticConstructObjectParameters@Unreal@RC@@…`** (params ctor)
  - `.text+0x2E9`: `lea rcx, [rsp+0x37]`
  - **`.text+0x2EE`: `call qword ptr [rip+disp]` (bytes `ff 15 …`), relocation
    at `.text+0x2F0` → `__imp_?StaticConstructObject@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@AEBUFStaticConstructObjectParameters@23@@Z`**
    — the decisive call. Result checked non-null at `.text+0x2F9`; the returned
    pointer is then written through `GetValuePtrByPropertyNameInChain(L"CheatManager")`
    at `.text+0x303-0x32C`.
- Source of record: `cme_upstream_shape.cpp:232-234`
  (`UObject* created = Unreal::UObjectGlobals::StaticConstructObject(
  Unreal::FStaticConstructObjectParameters(cheat_manager_class,
  player_controller));`), mirroring upstream Lua
  `Mods/CheatManagerEnablerMod/Scripts/main.lua` (`StaticConstructObject(CheatManagerClass, PlayerController)`).

### 3.2 Where the PS5 port implements it

[LOCAL-SOURCE: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UObjectGlobals.cpp`]

- **`src/UObjectGlobals.cpp:24-26`** — native-function slots:
  `GlobalState::StaticConstructObjectInternal{Base,411,426}` (`Function<>` holders).
- **`:28-33`** — `SetupStaticConstructObjectInternalAddress(void*)`: binds the
  signature-scanned engine address into all three slots (the 426 signature is
  the resolution key; wrappers adapt below it).
- **`:35-143`** — four engine-version wrapper families (`Below411`,
  `Below426`, `Below56`, `Above55`) marshalling the generic params struct into
  each engine ABI's native argument list; `Below56`/`Above55` add null-class
  diagnostics and null-return logging ("native function is not ready" /
  "class is null" / "returned null").
- **`:146-197`** — the public dispatcher:
  `auto StaticConstructObject(const FStaticConstructObjectParameters& GenericParams) -> UObject*`,
  branching on `Version::IsBelow(4,11)/(4,26)/(5,6)`.
- Consumed in-tree by the Lua route: **`src/LuaMod.cpp:2940-2949`** —
  `Unreal::FStaticConstructObjectParameters params{param_class, param_outer};
  … Unreal::UObjectGlobals::StaticConstructObject(params);` inside the
  registered `"StaticConstructObject"` Lua function (`:2844`). (KB's report
  cited `LuaMod.cpp:2303` from an earlier tree revision; in the current
  typescan-r4 tree the call is at `:2949` — same function, drifted line.)

The gap is therefore precisely located: implementation exists **below** the
host boundary; the **boundary** (`mbm_host_v1`, `ue4ss_compat`) has no row
that reaches it.

### 3.3 The exact signature both sides expect

**Mod / real-UE4SS side (Win64, verified export, ordinal 3562):**

```cpp
// ?StaticConstructObject@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@AEBUFStaticConstructObjectParameters@23@@Z
namespace RC::Unreal::UObjectGlobals {
    UObject* __cdecl StaticConstructObject(const FStaticConstructObjectParameters& params);
}
// Win64: RCX = &params (single pointer argument), RAX = UObject* return.
```

```cpp
// ??0FStaticConstructObjectParameters@Unreal@RC@@QEAA@PEBVUClass@12@PEAVUObject@12@@Z  (ordinal 120)
namespace RC::Unreal {
    FStaticConstructObjectParameters::FStaticConstructObjectParameters(const UClass* InClass, UObject* InOuter = nullptr);
    // Win64: RCX = this, RDX = InClass, R8 = InOuter.
}
```
[LOCAL-SOURCE for C++ shape: mod-facing upstream header
`ue4ss-unreal-source-access-1/upstream/include/Unreal/UObjectGlobals.hpp:90-153`
(struct `RC_UE_API`, 2-arg ctor at `:127`, free function declared at `:153`)]
[LOCAL-VERIFY for presence/mangles/ordinals: direct export-table parse of
`UE4SS.dll` today — ordinal 3562 at function RVA `0x1224524`, ordinal 120 at
RVA `0x10c526`, ordinal 1149 (FindFirstOf) at RVA `0x125d8a`]

**PS5 port side (SysV, same logical signature):**

```cpp
// typescan-r4/candidate/src/UObjectGlobals.cpp:146
namespace RC::Unreal::UObjectGlobals {
    auto StaticConstructObject(const FStaticConstructObjectParameters& GenericParams) -> UObject*;
}
// SysV: RDI = &params, RAX = UObject* return.
```

**Params struct (generic variant, fields per upstream header;
`:291-309` in the audit copy `c33e1de0eefe-UObjectGlobals.hpp`):**
`Class, Outer, FName Name, EObjectFlags SetFlags, EInternalObjectFlags
InternalSetFlags, bCopyTransientsFromClassDefaults, bAssumeTemplateIsArchetype,
Template, InstanceGraph, ExternalPackage` — plus 5.0+ `PropertyInitCallback`
and `SubobjectOverrides`; the `Above55` variant (`:313-341`) additionally has
`PropertyInitCallback[0x30]` padding, `SerialNumber`, optional
`FRemoteObjectId`/`RemoteSubObjectOverrides`.

**Layout ownership — the fact that makes the shim tractable:** the mod never
touches the struct's fields; it allocates stack space (`lea rcx,[rsp+0x37]`)
and lets the **imported ctor** fill it, then passes the address to the
**imported** StaticConstructObject. Layout is therefore owned by
UE4SS.dll on PC — and by `ue4ss_compat` on our path, as long as the ctor row
and the construct row agree (write Class@0x00, Outer@0x08, zero the rest;
read them back). **Caveat for real mods** [INFERENCE from upstream usage,
e.g. `LuaMod.cpp:2940-2948` assigning `params.Name`, `params.SetFlags`, …]:
a real mod compiled against upstream headers may write fields *directly* —
then the guest-baked MSVC layout must match what the shim reads, per the
SDK version the mod targeted (`ModIntendedSDKVersion`), i.e. the
`pin_msvc_layout.sh` pin must include this struct, not just `CppUserModBase`.
KB's reconstruction does not exercise field writes, so the census alone does
not pin this struct.

### 3.4 The `mbm_host_v1` row that closes the gap

[LOCAL-SOURCE for the table and its law: `cap13/BUILD/src/sdk/mbm/NativeModAbi.h`
(`mbm_host_v1` at `:203-240`; header rule 2 at `:23-24`: "Tables only grow at
the end; a reader never touches a field past struct_size")]

```c
/* Construct an engine object (upstream UObjectGlobals::StaticConstructObject
 * with default params: Name=None, RF_NoFlags, no template). Game-thread only.
 * class_object must be a UClass* (MBM_E_TYPE otherwise); out receives the
 * new UObject* or null if the engine refused. Creation is a state change:
 * MBM_ROUTE_DATA_ONLY is NOT claimable for this row. */
mbm_status (*construct_object)(void* ctx, uint64_t mod,
                               mbm_object class_object,
                               mbm_object outer_object,
                               mbm_object* out);
```

- Appended **after** the existing `gui` pointer (end-of-table growth),
  `struct_size` bumped; older readers never see it, newer readers check
  `abi_version` 1.1 / `struct_size` per rule 2 — no renumbering, no redesign.
- Host implementation: one ~15-line override in
  `NativeModEngineServices.inc` following the `find_first_of` pattern
  (`:276`), calling
  `RC::Unreal::UObjectGlobals::StaticConstructObject(FStaticConstructObjectParameters{cls, outer})`
  with the existing liveness guard, then `MBM_E_NOT_FOUND`/`MBM_E_TYPE` on
  null/class-mismatch.
- Admission: `MBM_ROUTE_DATA_ONLY_PARTIAL` with honest text ("state change:
  creates an object in the game process"), or `MBM_ROUTE_REFUSED` until
  review — integrator's call [LOCAL-SOURCE: blocker-list B2's analysis;
  `NativeModAbi.h:135-142` route enum].
- The same row simultaneously unblocks the **PE route**: the `ue4ss_compat`
  export row for ordinal-3562 demarshals `(RCX=&params)` → reads
  Class/Outer at the shim-owned layout → calls `host->construct_object`
  (or calls the port runtime directly — one decision, one implementation).
- ABI version review: this is a host change owned by the #9 team, not the PE
  route alone [LOCAL-SOURCE: feasibility §6 step 2].

### 3.5 Practical consequence of the missing row

Without it, everything up to the last step of the mod works — the
ClientRestart hook fires, `GetValuePtrByPropertyNameInChain` reads succeed,
`FindFirstOf` locates the CheatManager class — and then the core action is
impossible: **no CheatManager instance can ever be created, so the cheat
manager stays absent and the mod's entire purpose (enabling cheats) is
dead on arrival.** There is no substitute path:

- `call_function(controller, "StaticConstructObject", …)` — the staged
  workaround in `BUILD/src/port_cheat_manager_enabler.cpp:44` — returns
  `MBM_E_NOT_FOUND` by construction, because `call_function` resolves through
  `GetFunctionByNameInChain` [LOCAL-SOURCE: `NativeModEngineServices.inc:352-356`]
  and no engine UFunction of that name exists on PlayerController
  [INFERENCE from engine reflection; the staged port would fail closed with a
  logged refusal, not crash].
- `property_set` cannot help: there is no pre-existing CheatManager object to
  assign — that is the mod's reason to exist.

This is why the gap is "one row", and why that one row is the difference
between a loader demo and a working mod.

---

## 4. STEP 4 — What "unchanged upstream DLL" actually means for Townfall

1. **No genuine Townfall C++ DLL mod exists anywhere on this Mac.**
   [LOCAL-SOURCE: `REPORT/mod-selection.md` verdict + full inventory table:
   every real downloaded Townfall mod is Lua, pak/ucas/utoc, or INI;
   `townfall-ue4ss-real-script-audit-1/ue4ss-v1` has 8 Lua mods, zero DLLs;
   the only PE images in the whole corpus are the framework pair
   `UE4SS.dll` + `dwmapi.dll`.] Every DLL labeled "mod" in this worktree is
   our own fixture (§1 rows 3-5). **No unchanged upstream DLL has been tested
   through any part of the compatibility path, and none can be claimed as
   such.**
2. **The reconstruction is the best proxy that exists**, and it is a
   well-instrumented one: its 13 UE4SS imports are string-for-string verified
   against the real framework export table (§2a), so the shim row list it
   implies is the *actual* upstream surface for this mod shape — not 4256
   exports, but 7 functions + 6 vtable slots. What it cannot certify is the
   mod side of a *real* build (compiler behavior, CRT tier, EH tables) —
   blocker B7 [LOCAL-SOURCE: blocker-list B7].
3. **A real downloaded DLL stresses the path differently.** The
   analyzer/loader/shim must handle a genuinely foreign object, not one we
   compiled. Expected differences, each with a concrete handling requirement:

   | Dimension | Our reconstruction | A real downloaded DLL | Consequence for the path |
   |---|---|---|---|
   | Link product | COFF `.obj` only, never linked, never run | Full PE image: DllMainCRTStartup, `.pdata`/unwind, export dir, possibly TLS | Loader must run CRT init before mod code (DllMain → CRT → constructors, per reviewer's E2E checklist [LOCAL-SOURCE: `CORRECTIONS-FROM-REVIEWER.md` "What the E2E proof must record"]) |
   | Compiler | Apple clang 17, `-target x86_64-pc-windows-msvc`, `-fno-exceptions` | MSVC (`link.exe`), `/EHsc` typically on | Real mods carry `__CxxFrameHandler4`/EH machinery and unwind info; clang emitted direct virtual refs where MSVC often makes local COMDAT copies of dllimport inline virtuals [TRAINING-KNOWLEDGE; same symbols, per KB feasibility §2b note] — shim rows are by *symbol*, so this is tolerable, but the pinning tool is the authority for MSVC-built shapes |
   | CRT | 3 stand-in symbols; freestanding | `/MD` (MSVCP140/VCRUNTIME140/ucrt family) or `/MT` (static CRT + heap/time imports) | Classifier must emit the tier; shim row generation per tier (blocker B8); `/MT` needs HeapCreate/GetSystemTimeAsFileTime-class rows |
   | std templates | Mangling-compatible stand-ins, never executed | Real MSVC STL objects (32-byte wstring SSO-8, real `std::function` with invoke/clone/manage pointers) | Guest-constructed objects must be read/invoked at pinned MSVC layout — B6, the largest single work item |
   | API breadth | Exactly 7 functions + 6 virtuals | Whatever that mod's author used: `StaticFindObject` overloads, `RegisterHook(UFunction*,…)`, `execute_in_game_thread`, GUI tabs, key events | Census-driven row generation per mod (design's stated law: no unused surface before the first mod works) |
   | Params struct | Ctor-only (opaque) | Possible direct field writes (`params.Name = …`) | Shim-owned layout breaks unless the pin includes `FStaticConstructObjectParameters` per SDK version (§3.3 caveat) |
   | Hardening | none | possible `/GUARD:CF` (guard dispatch), Spectre libs, incremental-link thunks | Classifier flags; CFG thunks would need a T-row decision [TRAINING-KNOWLEDGE] |
   | Game dependencies | none (engine-only surface) | A game-specific mod may reference classes/assets absent from Townfall | Loader portability is our job; mod-game dependency compatibility is the mod's problem — the loader can only refuse honestly, not guarantee [LOCAL-SOURCE: reviewer scope clarification in `CORRECTIONS-FROM-REVIEWER.md`] |

4. **Honest bottom line for Townfall:** today the project can (a) analyze any
   real DLL dropped on the Mac (`pe_analyzer.py`, evidence JSONs exist),
   (b) state the exact upstream surface the first mod needs (this report),
   and (c) name the single missing host row that turns the target mod from
   "loads and hooks" into "does its job". It cannot yet execute any of it
   anywhere — not on the Mac (bridge unverified, loader never mapped a real
   image end-to-end, KF's E2E in progress) and not on the PS5 (J1 unprobed).
   Until one unchanged upstream DLL — framework stress or reconstruction
   binary — has been mapped, relocated, import-resolved, and run through
   DllMain on the Mac, every compatibility claim stays at "dependency
   coverage", exactly as the reviewer relabeled it.

---

## 5. STEP 5 — Updated #13 handover text

The authoritative handover (`/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/INTEGRATOR-HANDOVER.md`)
is KC's staged artifact and lists SHA256s; I have not mutated it. The text
below is the **drop-in replacement** for its #13 entries — specifically the
§1D table row, the §2 "#13:" bullet, and the §5 "#13 native-loader…" bullet.
It supersedes the previous wording everywhere it is pasted.

---

> **#13 (native/PE DLL-mod route) — status, 2026-10-13 (agent KH):**
>
> **DLL taxonomy — do not conflate the three classes:**
> (1) *Unchanged upstream DLLs on this Mac*: only the UE4SS framework itself
> (`windows-image-original-inputs-1/UE4SS.dll`, sha256 `94c2ebd3…`, 4256
> exports) and its `dwmapi.dll` loader proxy. The framework is a legitimate
> PE-loader stress input but is NOT a mod (it *provides* the mod ABI; it does
> not export `start_mod`).
> (2) *Our fixture DLLs*: 12 `SAMPLE/cap13-mod-*.dll` (Apple-clang, export
> `mbm_mod_entry_v1` only, import no UE4SS symbols) + 3 struct-built
> `FIXTURES/*.dll`. Useful for loader/classifier tests; they say nothing
> about upstream compatibility.
> (3) *Reconstruction*: `REPORT/evidence/simplest-mod-census/cme_upstream_shape.obj`
> — CheatManagerEnabler logic in the upstream C++ API shape; census
> artifact only, never linked, never executed; all 13 UE4SS imports verified
> string-for-string against the real framework export table (re-verified
> 2026-10-13 by direct export-directory parse; StaticConstructObject =
> ordinal 3562, RVA 0x1224524).
>
> **No real upstream UE4SS C++ DLL mod exists on this machine and none has
> been tested through any part of the loader/shim path.** All real downloaded
> Townfall mods are Lua/pak/config. Compatibility claims remain "dependency
> coverage", not execution.
>
> **Decisive gap (source-located), one host row:**
> the target mod's core call is
> `?StaticConstructObject@UObjectGlobals@Unreal@RC@@…` (real export, ordinal
> 3562). The port runtime already implements it (typescan-r4
> `src/UObjectGlobals.cpp:146-197`; consumed at `src/LuaMod.cpp:2949`), but
> `mbm_host_v1` (`sdk …/mbm/NativeModAbi.h:203-240`) has no construct row, and
> the staged workaround `call_function(controller,"StaticConstructObject",…)`
> (`BUILD/src/port_cheat_manager_enabler.cpp:44`) deterministically returns
> `MBM_E_NOT_FOUND` (`NativeModEngineServices.inc:352-356` resolves through
> engine reflection only). Call site in the compiled census object:
> `enable_cheat_manager`, `.text+0x2EE`, reloc `.text+0x2F0` →
> `__imp_?StaticConstructObject@…`. Fix: append
> `construct_object(ctx, mod, class_object, outer, mbm_object* out)` at table
> end (rule-2 growth), backed by the existing dispatcher; admission must be
> honest (state change — DATA_ONLY not claimable). Full analysis:
> `caps-team-work/cap13/REPORT/UNCHANGED-UPSTREAM-DLL-PATH.md`.
>
> **Test order:** Mac PE-loader E2E first (KF's host harness, in progress:
> `cap13/e2e/`) — record architecture, translation environment, loader/shim
> path, DllMain→CRT→ctors, per-import resolution verdicts, callbacks,
> observable behavior, host-only vs PS5-untested (reviewer checklist). The
> single console fact that gates the native route remains **J1** (executable
> anonymous memory in the game process); no console spend before the Mac E2E
> is green and J1 is authorized.
>
> **Integrator's mbm_host_v1 addition (ABI v1.1):** the `construct_object`
> row above + `NativeModEngineServices.inc` override (pattern: `:276`) +
> admission row + version-bump review. Nothing else in the table changes;
> rule 2 (tables only grow at the end) makes the append binary-compatible
> for existing readers.

---

## 6. Verification appendix (what I ran today)

1. **13/13 census symbols vs the real binary** — direct export-directory walk
   of `UE4SS.dll` (DOS header → COFF → optional header data directory 0 →
   export names/ordinals/EAT), confirming name+ordinal+function RVA for all
   13 UE4SS symbols of `cme_upstream_shape.imports.json`, notably
   StaticConstructObject (ord 3562, RVA `0x1224524`), params ctor (ord 120,
   RVA `0x10c526`), FindFirstOf (ord 1149, RVA `0x125d8a`). Method:
   python struct parse of the file bytes (script quoted in the session
   transcript; no file modified). Note: the `rva` fields inside
   `UE4SS.dll.analysis.json` repeat one stale value across entries — presence
   and ordinals in that JSON are still correct; use direct parse for RVAs.
2. **Call-site disassembly** — `xcrun llvm-objdump -d` and `-r` on
   `cme_upstream_shape.obj`; relocation table maps `.text+0x2E5` → params
   ctor import and `.text+0x2F0` → StaticConstructObject import, inside
   `?enable_cheat_manager@?A0xC310C454@@…` (function base `.text+0x210`).
3. **Fixture-vs-reconstruction classification** — `llvm-objdump -p` on
   `SAMPLE/cap13-mod-cheat-manager-enabler.dll` (exports `mbm_mod_entry_v1`
   only; imports kernel32 trio + vcruntime140 pair) vs analyzer JSON
   (`imports_ue4ss_dll: false`).
4. **Host-gap reads** — `BUILD/src/sdk/mbm/NativeModAbi.h` (no construct row;
   `MBM_HOOK_STATIC_CONSTRUCT_OBJECT = 8` at `:122` is an *observation* hook
   family, not a service); `NativeModEngineServices.inc` service override
   list (find/property/call/hook only); `call_function` implementation
   (`:352-388`, reflection-only resolution); staged workaround read at
   `BUILD/src/port_cheat_manager_enabler.cpp:40-50`.
5. **Port implementation reads** — typescan-r4 `src/UObjectGlobals.cpp`
   (slots `:24-26`, address setup `:28-33`, wrappers `:35-143`, dispatcher
   `:146-197`), `src/LuaMod.cpp:2844/2940-2949`, upstream mod-facing header
   `UObjectGlobals.hpp:90-153`, params struct `:291-341` (audit copy).
