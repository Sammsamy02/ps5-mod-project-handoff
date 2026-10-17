# cap13 task 1b — Simplest UE4SS-C++-API mod through the #9 system: feasibility

Agent KB (ZCode), 2026-10-13, cap13 packet. **Mac-only analysis + staged
evidence. No console contact, no builds of the PS5 tree.** Task: "Pick the
simplest mod that only uses UE4SS functions (no game-code patching). Deliver
either (i) a PE-loader/translation path that loads the UNMODIFIED DLL, or (ii)
the exact list of what blocks it, with evidence."

Evidence labels (inherited from CAP9/CAP13):
- **[LOCAL-SOURCE]** — file in this worktree, path + line.
- **[LOCAL-VERIFY]** — fact produced on this Mac today; command + artifact quoted.
- **[TRAINING-KNOWLEDGE]** — version-sensitive external fact; re-verify before
  anything depends on it.
- **[INFERENCE]** — my reasoning.

---

## 0. Verdict (read this first)

**(ii). The unmodified-DLL route cannot deliver a running mod today.** The
blocker list is `blocker-list.md` (9 items, each with evidence, unblock step
and effort). Nothing in the list is a dead end; three of the nine are closed
today by this analysis (the census, the exact host-service gap, the shim
row list for one concrete mod).

Headline facts, all evidenced below:

1. **The task premise needs a correction**: upstream ships CheatManagerEnabler
   and ConsoleEnabler as **Lua mods**, not C++ mods [LOCAL-SOURCE: vendored
   `ue4ss-v1/ue4ss/Mods/{CheatManagerEnablerMod,ConsoleEnablerMod}/Scripts/main.lua`
   — only `main.lua`, no sources]. No C++ variant of either exists anywhere in
   this worktree. The "simplest C++ mod" had to be reconstructed; I ported the
   upstream Lua logic 1:1 to the upstream C++ API shape and labeled it a
   reconstruction everywhere.
2. **A real mod PE's import census is now measurable, and for the simplest mod
   it is small**: 18 distinct undefined symbols — **13 UE4SS.dll C++ symbols
   (all 13 verified present in the real staged UE4SS.dll export table,
   string-for-string), 2 kernel32, 3 CRT** [LOCAL-VERIFY:
   `REPORT/evidence/simplest-mod-census/` — Apple clang
   `-target x86_64-pc-windows-msvc -c` on a reconstructed mod source, COFF
   symbol table dumped with llvm-nm, checked against JG's
   `REPORT/evidence/UE4SS.dll.analysis.json`]. This is the per-symbol evidence
   the shim (`ue4ss_compat`) needs: the whole UE4SS surface for the first mod
   is **7 imported functions + 6 vtable-referenced virtuals** — not 4256.
3. **The #9 host C table cannot express the chosen mod's core action today**:
   CheatManagerEnabler calls `?StaticConstructObject@UObjectGlobals@Unreal@RC@@…`
   (verified export), and `mbm_host_v1` has **no construct service**
   [LOCAL-SOURCE: cap09 `sdk/include/mbm/NativeModAbi.h:203-240` — find/object/
   property/call/hook services only]. BUT the port runtime **already
   implements** `Unreal::UObjectGlobals::StaticConstructObject`
   (signature-resolved to the game's native function, engine-version-branched)
   [LOCAL-SOURCE: typescan-r4/candidate/src/UObjectGlobals.cpp:25-90; used by
   the Lua route src/Mod/LuaMod.cpp:2303]. So this blocker is an **ABI v1.1
   exposure problem with an admission policy**, not an engine problem.
4. **JH's loader design is architecturally right and its layers are the
   correct decomposition** (PE map → reloc → imports → bridge → shim →
   ue4ss_compat → C table) [LOCAL-SOURCE: DESIGN/native-dll-loader.md], but
   every layer between the PE bytes and the C table is **design-grade, never
   executed**: the bridge assembly is unverified, `ue4ss_compat` is a sketch
   with pinned-layout placeholders, and the loader has not run end-to-end even
   against the synthetic fixture on this Mac. Console unknown J1 (executable
   anonymous memory) is unprobed and gates the whole route.
5. **Minimum viable integration** (§6): one host service (construct_object),
   the M0/M1 loader milestones run on this Mac against JG's fixture + my
   census, one console probe (J1). With those, this mod is exactly the
   envelope mod JH described (design §7d) — headless, no TLS, no game-code
   patching, 7-symbol UE4SS surface.

---

## 1. The mod, precisely

**Chosen: CheatManagerEnabler (upstream-logic reconstruction in C++ API shape).**
Why it is the simplest:

- Its entire behavior is: hook `PlayerController:ClientRestart`, read two
  properties, construct one object, write one property [LOCAL-SOURCE: upstream
  Lua `Mods/CheatManagerEnablerMod/Scripts/main.lua` lines 3-32].
- ConsoleEnabler needs strictly more surface: FName lookups ("Tilde"/"F10"),
  `TArray` element append + nested member writes on `InputSettings.ConsoleKeys`,
  and `ExecuteInGameThread` — the C table has no array-index property addressing
  and no FName-pool lookup service [LOCAL-SOURCE: upstream Lua
  `Mods/ConsoleEnablerMod/Scripts/main.lua`; NativeModAbi.h service list]. It
  is mod #2, not mod #1.
- No game-code patching anywhere (property write only), no file IO, no
  threads, no GUI — inside JH's headless envelope by construction.

## 2. What the mod imports — measured, not guessed [LOCAL-VERIFY]

Source: `REPORT/evidence/simplest-mod-census/cme_upstream_shape.cpp` (the
reconstruction; every dllimport declaration decoded from a verified real
export mangle). Command:

```
clang++ -std=c++20 -target x86_64-pc-windows-msvc -fno-exceptions \
  -c cme_upstream_shape.cpp -o cme_upstream_shape.obj
xcrun llvm-nm --undefined-only cme_upstream_shape.obj
```

Result (18 distinct symbols; full machine-readable form:
`cme_upstream_shape.imports.json`; raw dump: `cme_upstream_shape.undef.txt`):

### 2a. UE4SS.dll — imported functions (7, all `__imp_` thunks, all verified in the real EAT)

| # | mangled symbol (prefix `__imp_`) | real EAT | meaning | #9 mapping |
|---|---|---|---|---|
| 1 | `??0CppUserModBase@RC@@QEAA@XZ` | yes | base-class ctor | ue4ss_compat: shim-construct base subobject at pinned MSVC layout (design §6b); metadata from manifest. No host call. |
| 2 | `??1CppUserModBase@RC@@UEAA@XZ` | yes | base virtual dtor | ue4ss_compat: destroy shim-side fields; free via guest allocator pairing (design §6b). |
| 3 | `??0FStaticConstructObjectParameters@Unreal@RC@@QEAA@PEBVUClass@12@PEAVUObject@12@@Z` | yes | params struct ctor (2-arg) | ue4ss_compat: shim-side struct, host-defined layout. |
| 4 | `?StaticConstructObject@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@AEBUFStaticConstructObjectParameters@23@@Z` | yes | **construct an object** | **NO mbm_host_v1 service today — blocker B2**; port runtime implementation exists (see §4). |
| 5 | `?FindFirstOf@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@AEBV?$basic_string@_W…@@Z` | yes | object lookup by class name | `host->find_first_of` [LOCAL-SOURCE: NativeModAbi.h:216]. |
| 6 | `?GetValuePtrByPropertyNameInChain@UObject@Unreal@RC@@QEAAPEAXPEB_W@Z` | yes | property address (searched through class chain) | `host->property_address` [LOCAL-SOURCE: NativeModAbi.h:227]. |
| 7 | `?RegisterHook@UObjectGlobals@Unreal@RC@@YA?AU?$pair@HH@std@@AEBV?$basic_string@_W…V?$function@$$A6AXAEAVUnrealScriptFunctionCallableContext@Unreal@RC@@PEAX@Z@5@1PEAX@Z` | yes | per-UFunction pre/post hook | `host->register_ufunction_hook` [LOCAL-SOURCE: NativeModAbi.h:233]; family `MBM_HOOK_UFUNCTION`. |

Census note: RegisterHook's `std::function` parameters are constructed BY THE
MOD at MSVC layout; `ue4ss_compat` receives opaque 0x38-0x40-byte function
objects and must invoke them through the bridge — layout pinned by
`tools/pin_msvc_layout.sh`, never from memory (adds requirement R1 in
blocker-list.md B6). Return value `std::pair<int,int>` is the 8-byte
aggregate that Win64 returns in RAX and SysV splits RAX:RDX — the
guest→host thunk must re-pack; today the bridge refuses aggregates
[LOCAL-SOURCE: win64_bridge.h "PER-SIGNATURE HONESTY" block] — blocker B5.

### 2b. UE4SS.dll — vtable-referenced virtuals (6 direct refs)

`?on_update@CppUserModBase@RC@@UEAAXXZ`, `?on_ui_init@…`, `?on_program_start@…`,
`?on_dll_load@…V?$basic_string_view@_W…@Z`, `?render_tab@…`,
`?on_cpp_mods_loaded@…`. (The mod overrides `on_unreal_init`, so that slot
points at mod-local code and is NOT imported — visible in the dump.) These
are the slots the host fires through the adapter's virtual-slot dispatch
(pe_adapter `cb_on_update`-style rows, design §4b). Under MSVC these become
local COMDAT copies of dllimport inline virtuals [TRAINING-KNOWLEDGE];
clang emits direct undefined refs — same symbols, same shim rows either way.

### 2c. kernel32 (2) and CRT (3)

`DisableThreadLibraryCalls`, `GetModuleHandleW` → win_api_shim T1/T3 rows
(NOOP-log TRUE / module-registry handle). `??2@YAPEAX_K@Z` (operator new),
`??3@YAXPEAX@Z` (operator delete), `??_7type_info@@6B@` (RTTI type_info
vtable — mods build with `/GR` by default) → CRT tier: `/MT` builds carry
these statically inside the mapped image (run via DllMainCRTStartup, need
shim heap/time rows like `HeapCreate`/`GetSystemTimeAsFileTime`); `/MD`
builds import the MSVCP140/VCRUNTIME140/api-ms-win-crt-* family — the same
family already censused on the framework image (634 imports / 29 DLLs)
[LOCAL-SOURCE: UE4SS.dll.analysis.json `imports`]. Not exceptions, not TLS.

## 3. C++ features the mod actually uses (task question 4)

| feature | used? | evidence | consequence |
|---|---|---|---|
| virtual functions | yes — derived class + base vtable | census §2b; CppUserModBase upstream header (14 virtuals, single inheritance) [LOCAL-SOURCE: upstream `include/Mod/CppUserModBase.hpp:44-217`] | MSVC bare-array vtable, pinned slots — design §4b, feasible |
| exceptions | no live paths | mod body never throws; compiled `-fno-exceptions` [LOCAL-VERIFY]; BUT an `/EHsc` MSVC build inlines std::function machinery with throw paths — envelope states "no executed throw" with T4 fail-closed stubs (design §4c) | refused-then-fail-closed, not a blocker for THIS mod |
| RTTI | tables present, not exercised | `??_7type_info@@6B@` in census; no dynamic_cast/typeid in source | dead data unless executed; no COL crossing (design §4c) |
| STL at the boundary | yes — `std::wstring` by const& (SSO read), `std::function` by value (constructed by mod, invoked by shim), `std::pair` by value (returned) | census symbols 5-7 | layout-pinned shim handling; B5/B6 |
| static TLS | no | no TLS directory in a mod this simple — classifier verifies per mod (design §3e, §10 asks) | refusal guard only |
| multiple inheritance / virtual bases | no | single inheritance by construction (upstream base header) | no adjustor thunks needed |

## 4. Does it exist in our PS5 port? (task question 2, per function)

| mod call | PS5 port reality | evidence |
|---|---|---|
| RegisterHook (ufunction pre/post) | EXISTS behind the host C table; host services implemented on top of the port's own UE4SS runtime (`fn->UnregisterHook(rec.pre)`, `UE4SSProgram::get_program().register_keydown_event(...)`) | [LOCAL-SOURCE: typescan-r4/candidate/src/Mod/NativeModEngineServices.inc:413-449] |
| FindFirstOf | EXISTS as `find_first_of` service with live-object liveness guard | [LOCAL-SOURCE: NativeModEngineServices.inc:276 + :100 comment] |
| GetValuePtrByPropertyNameInChain | EXISTS as `property_address` service | [LOCAL-SOURCE: NativeModEngineServices.inc:337] |
| StaticConstructObject | **IMPLEMENTED IN THE RUNTIME, NOT EXPOSED IN THE C TABLE**: signature-resolved `StaticConstructObjectInternal{Base,411,426}` with per-engine-version wrappers + null-class diagnostics | [LOCAL-SOURCE: typescan-r4/candidate/src/UObjectGlobals.cpp:25-90; consumed by the Lua route src/Mod/LuaMod.cpp:2303] |
| CppUserModBase ctor/dtor (MSVC layout) | EXISTS for the #9 recompile route at SysV layout (sdk CppUserModBase); the MSVC-layout shim subobject is design-stage (pin tool verified, output not yet baked) | [LOCAL-SOURCE: cap09 sdk/include/Mod/CppUserModBase.hpp; cap13 ue4ss_compat.cpp:50-60 kLayoutPinned=false; tools/pin_msvc_layout.sh LOCAL-VERIFY in design §8] |
| kernel32 pair | SHIM row classes exist, individual rows generated from census in M1 | [LOCAL-SOURCE: win_api_shim.h tier policy] |

## 5. Answers to the verdict questions

**If the PE loader + ABI bridge could work, what SPECIFICALLY is missing from
JH's design?** The design is complete at the architecture level; what is
missing is execution-grade completion, plus two concrete design gaps this
analysis adds: (a) the `std::function`-by-value invocation and
`std::pair<int,int>` return re-pack are real crossing shapes the current
bridge explicitly refuses (B5, R1); (b) the shim needs a *census-driven
ue4ss_compat row list* — which this task now supplies for the first mod
(§2a table): 7 functions + 6 vtable slots + 2 kernel32 rows + CRT tier.

**Minimum viable #9 integration** — see §6 and blocker-list.md §Unblock-order.

## 6. Minimum viable integration (the concrete path to deliverable (i))

1. **M0 (Mac, no console):** run JH's staged mapper against JG's x64 fixture
   (`FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll`): map → relocate → resolve
   `start_mod`/`stop_mod` + the 2 imports → DllMain invoke. Bridge thunk
   round-trip tests. Exit: green on Mac. (B3/B4, days.)
2. **ABI v1.1, one row:** `construct_object(ctx, mod, class_object, outer_object, mbm_object* out)`
   backed by the existing runtime implementation, with an admission row
   (`MBM_ROUTE_DATA_ONLY_PARTIAL` at minimum: creation is a state change, not
   data-only — the admission text must say so honestly; the integrator may
   prefer REFUSED-until-review). This is a host change owned by the #9 team,
   NOT the PE route alone. (B2, days.)
3. **M1 (Mac):** bake `pin_msvc_layout.sh` output; generate the §2a row table;
   aggregate-return thunk for `RegisterHook`; std::function invocation rows.
   Synthetic end-to-end on Mac: map a mod-shaped PE → DllMain → start_mod →
   virtual fires → RegisterHook lands in a stub host table. (B5/B6, 1-2 weeks.)
4. **J1 probe (console, one rung, needs authorization):** mmap+mprotect RX in
   game process. If NO → route dead, census/classifier work remains useful for
   refusal diagnostics. (B1, one rung.)
5. **First real mod:** download ONE real UE4SS C++ DLL mod (none exists on
   this machine — B7), run pe_analyzer, compare its census against my
   reconstruction's 13 UE4SS rows, classify, then the one-launch ladder.

## 7. What would NOT be needed for this mod (honest scoping)

user32/gdi32/d3d11 (GUI mods), delay-load machinery beyond the three
LoadLibrary-family rows, full UE4SS.dll export parity (4256 exports vs 7
rows), Lua interop, EH/SEH dispatcher, TEB fabrication. The cap13 law "no
unused surface before the first mod works" holds: this mod needs none of it.

## 8. Staleness register (most stale first)

1. My reconstruction's exact mangles are verified against ONE UE4SS.dll build
   (sha256 94c2ebd3…). A real mod built against a different UE4SS version may
   import different overloads (e.g. `RegisterHook(UFunction*,…)`,
   `StaticFindObject` overloads) — per-mod census decides (B7).
2. MSVC dllimport-inline-virtual behavior (local COMDAT copies) is
   [TRAINING-KNOWLEDGE]; the census was produced with clang, which emits
   direct references instead. Same symbols either way; the pinning tool is the
   authority for the MSVC-built shapes.
3. Upstream header drift (on_lua_start signatures etc.) affects only
   ue4ss_compat rows NOT used by this mod.
