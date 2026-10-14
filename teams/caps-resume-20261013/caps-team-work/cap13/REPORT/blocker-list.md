# cap13 task 1b — Blocker list: unmodified UE4SS C++ DLL mod through the #9 system

Agent KB (ZCode), 2026-10-13. Companion to `simplest-mod-feasibility.md`.
Every blocker carries: the evidence (local paths; no guesses), the unblock
step, and an effort estimate. Ordering = unblock order, not severity.

Target mod: the CheatManagerEnabler reconstruction (simplest UE4SS-C++-API
mod; upstream ships it as Lua — see feasibility report §1). Its measured
census: 7 UE4SS.dll imports + 6 vtable virtuals + 2 kernel32 + 3 CRT
[LOCAL-VERIFY: `evidence/simplest-mod-census/cme_upstream_shape.imports.json`,
13/13 UE4SS symbols verified against the real UE4SS.dll export table].

Evidence labels: [LOCAL-SOURCE] worktree file; [LOCAL-VERIFY] produced on this
Mac today; [TRAINING-KNOWLEDGE] re-verify before dependence; [INFERENCE].

---

## B1. Executable anonymous memory in the game process — UNPROBED (route gate)

- **Evidence**: the PE route maps PE images into anonymous memory and executes
  code there, plus stamping guest→host trampolines into executable memory
  [LOCAL-SOURCE: DESIGN/native-dll-loader.md §3b, win64_bridge.h BridgeTramp].
  Whether the PS5 game process permits `mmap` + `mprotect PROT_EXEC` (and RWX
  for mixed pages) at all has never been probed — "no payload-sdk sample
  proves it in game-process context" [LOCAL-SOURCE: design §9 J1].
- **Unblocks**: everything. If NO, the entire PE route is dead (census and
  classifier work still pay off as refusal diagnostics).
- **Effort**: one console rung (tiny payload PRX doing mmap+mprotect+call),
  requires Claude/integration authorization per the design's console ladder.
  Not a Mac work item.

## B2. `mbm_host_v1` has no object-construction service (mod's core action)

- **Evidence**: the chosen mod's central call is
  `?StaticConstructObject@UObjectGlobals@Unreal@RC@@YAPEAVUObject@23@AEBUFStaticConstructObjectParameters@23@@Z`
  — verified present in the real UE4SS.dll EAT
  [LOCAL-VERIFY: census symbol 4]. The host C table exposes find/object/
  property/call/hook services only [LOCAL-SOURCE:
  cap09/PATCH/sdk/include/mbm/NativeModAbi.h:203-240 — no construct row;
  `MBM_HOOK_STATIC_CONSTRUCT_OBJECT` (line 127) only observes construction].
- **Key fact**: the PS5 runtime ALREADY IMPLEMENTS it — signature-resolved
  `StaticConstructObjectInternal{Base,411,426}` with per-engine-version
  wrappers and null-class diagnostics [LOCAL-SOURCE:
  typescan-r4/candidate/src/UObjectGlobals.cpp:25-90], consumed by the Lua
  route [LOCAL-SOURCE: …/src/Mod/LuaMod.cpp:2303]. This is an ABI-exposure
  gap, not an engine gap.
- **Unblock**: add one v1.1 row `construct_object` to `mbm_host_v1`
  (tables-only-grow rule permits it), implement behind the existing runtime
  function in `NativeModEngineServices.inc` (504-line file with the exact
  pattern at :276/:337/:352), and give it an honest admission row — creation
  is a state change, so `MBM_ROUTE_DATA_ONLY` is not claimable; integrator
  decides between PARTIAL-with-documentation and REFUSED-until-review.
- **Effort**: days (host team), plus ABI version-bump review. Alternative if
  refused: pick a simpler first action mod (none of the upstream "simplest"
  set avoids construction or array writes — CheatManagerEnabler stays the
  target with this one row).

## B3. Win64 bridge assembly is hand-written and UNVERIFIED

- **Evidence**: "The assembly in win64_bridge.S is hand-written and UNVERIFIED
  until the M0 host unit tests run… Do not trust it before that"
  [LOCAL-SOURCE: native-loader/src/win64_bridge.h:20-22]. No test exists yet.
- **Why it matters**: every crossing (DllMain, start_mod, virtual slots,
  RegisterHook callbacks, std::function invocation) rides these thunks; a
  wrong shadow-space or XMM6-15 save corrupts silently.
- **Unblock**: M0 on this Mac — macOS is SysV x86_64; drive the thunks from C
  with crafted Win64 register states; verify remap/shadow-space/non-volatile
  preservation. Design §8 M0 defines the exit.
- **Effort**: days.

## B4. pe_loader is design-grade and has never loaded anything

- **Evidence**: staged sources are labeled "design-grade; not built"
  [LOCAL-SOURCE: native-loader/src/pe_loader.cpp header; design §8 table].
  JG's real fixture exists for it
  [LOCAL-SOURCE: FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll — x64 PE32+,
  exports start_mod/stop_mod, imports UE4SS.dll!is_ue4ss_initialized +
  KERNEL32.dll!DisableThreadLibraryCalls] and the cross-tool agreement test
  (§10 of the design) has not been run.
- **Unblock**: run the staged loader against the fixture on this Mac: parse →
  map (16 KiB permission partition) → relocate → resolve the 2 imports →
  export lookup → DllMain invoke. Byte-identity law (file never written) is
  checkable by hash before/after.
- **Effort**: days. Note: the loader cannot be *fully* proven on the Mac for
  the load step of a real game process — that is B1's rung; everything up to
  mapping is Mac-testable.

## B5. The bridge refuses two crossing shapes this mod needs (aggregate return, by design)

- **Evidence**: `RegisterHook` returns `std::pair<int,int>` (8-byte aggregate:
  Win64 packs in RAX, SysV splits RAX:RDX) and receives two
  `std::function` objects BY VALUE that the shim must later invoke.
  The bridge explicitly refuses aggregates and mixed shapes pending the
  per-signature generator [LOCAL-SOURCE: win64_bridge.h "PER-SIGNATURE
  HONESTY" block; ue4ss_compat.cpp:105-113 M1 list item 3].
- **Unblock**: add these two documented signatures to the M1 thunk generator:
  (a) aggregate-8 re-pack (EDX→RAX high dword) for RegisterHook's return;
  (b) an "invoke MSVC std::function" row: read the guest-constructed function
  object at its pinned MSVC layout, call its stored invoker through the bridge
  with the right `this` — layout baked from `tools/pin_msvc_layout.sh`
  [LOCAL-VERIFY that the pinning mechanism works: design §8].
- **Effort**: days, inside M1.

## B6. ue4ss_compat is a sketch: no pinned layouts, no export rows, no trampolines

- **Evidence**: `static constexpr bool kLayoutPinned = false;` with the M1
  refusal commented out; the export map is an empty illustration; the file's
  own closing list says what M1 must complete [LOCAL-SOURCE:
  native-loader/src/ue4ss_compat.cpp:60-114].
- **What this task adds**: the concrete row list for the first mod now EXISTS
  (feasibility report §2a: 7 functions + 6 vtable slots, each mapped to a host
  service or shim class) — M1 is generation work, not discovery work.
- **Unblock**: bake pin_msvc_layout.sh output (CppUserModBase subobject at
  MSVC layout incl. 32-byte SSO-8 wstrings; MSVC std::function invoke path;
  vftable slot order), generate the 13 rows + trampolines (exec memory per
  B1), wire pe_adapter's placeholder `cb_on_update` dispatch to the pinned
  slot table [LOCAL-SOURCE: pe_adapter.cpp:64-71 placeholder].
- **Effort**: 1-2 weeks focused; the largest single work item (agrees with
  the design's own assessment, §8 table).

## B7. No real C++ DLL mod exists on this machine — the census is a reconstruction

- **Evidence**: the real-mod intake corpus is all Lua mods / UE paks / config;
  "No real Townfall DLL mod exists on this machine. Evidence gap confirmed."
  [LOCAL-SOURCE: REPORT/mod-selection.md verdict + inventory table]. Upstream
  CheatManagerEnabler/ConsoleEnabler are Lua mods
  [LOCAL-SOURCE: vendored Mods/ tree]. My reconstruction is labeled as such in
  the source, the JSON and the report.
- **Why it matters**: a real mod may import different overloads
  (`RegisterHook(UFunction*,…)`, `StaticFindObject` string-view overloads),
  a different CRT tier, or extra framework surface. The 13-row shim list is
  verified against the real UE4SS.dll EAT, but the MOD side is verified only
  for the reconstruction.
- **Unblock**: download ONE real UE4SS C++ DLL mod with provenance; run
  `TOOL/pe_analyzer.py`; diff its import census against
  `cme_upstream_shape.imports.json`; extend rows from the diff. Mac-only.
- **Effort**: hours per mod once downloadable (download requires user/WebFetch
  authorization).

## B8. CRT tier of a real mod build is unconfirmed (/MT vs /MD)

- **Evidence**: my census shows only operator new/delete + type_info vtable
  because the reconstruction is freestanding. A real `/MD` mod imports the
  api-ms-win-crt-*/MSVCP140/VCRUNTIME140 family (the framework image carries
  207 MSVCP140 + ~130 ucrt imports [LOCAL-SOURCE:
  UE4SS.dll.analysis.json imports]); an `/MT` mod instead runs its own static
  CRT, which needs shim heap/time rows (`HeapCreate`,
  `GetSystemTimeAsFileTime`, …) — the T1/T2 rows exist as classes, the exact
  row list is census-driven [LOCAL-SOURCE: win_api_shim.h tier policy; design
  §5b]. Upstream warns C++ mods break on CRT mismatch with the framework
  [LOCAL-SOURCE: upstream include/Mod/CppUserModBase.hpp:29-30 comment] — on
  the PE route the mod carries its own CRT, sidestepping that failure class.
- **Unblock**: classifier emits the CRT tier per mod (import DLL names are
  already in JG's analyzer output); shim row generation per tier in M1/M3.
- **Effort**: small per mod; bounded by the two known tiers.

## B9. Host-side routing for a PE-named mod is manifest-only today

- **Evidence**: the host refuses PE at the `MZ` marker and refuses
  start_mod-only modules by design, and the route keeps that refusal — the PE
  route is expressed by naming `pe_adapter.sprx` in the mod manifest while
  `main.dll` sits untouched [LOCAL-SOURCE: DESIGN/native-dll-loader.md §1 and
  the staged optional patch patches/cppmod_pe_route.patch, proposal-only].
  No host code accepts a PE-named row today.
- **Unblock**: integrator decision — manifest naming (no host change) or the
  staged patch. Either way this is packaging + a decision, not engineering
  risk.
- **Effort**: hours + integration review.

---

## Explicit non-blockers for this mod (with evidence)

- **Exceptions/SEH**: no executed throw paths in the mod body
  [LOCAL-VERIFY: compiles clean under -fno-exceptions]; T4 fail-closed stubs
  cover accidental raises (design §4c). An `/EHsc` mod whose inlined
  std::function machinery throws at runtime would stop THE MOD, not the game.
- **Static TLS**: no TLS directory in a mod this simple; presence is a
  load-time refusal (design §3e) — verified guard, not a gap.
- **Game-code rewriting**: the mod writes only an object property
  (`PlayerController.CheatManager = created`), inside the envelope; no
  VirtualProtect/WriteProcessMemory anywhere in its census.
- **GUI/user32/d3d11**: not imported by this mod; out of scope by cap13 law.
- **Cross-module C++ between host and mod**: the entire reason the #9 host
  uses the C table [LOCAL-SOURCE: NativeModAbi.h:5-18]; the PE route never
  lets C++ objects cross — only pinned-layout opaque bytes handled by
  ue4ss_compat shims (allocator-pairing rule, design §6b).

## Unblock order (dependency-true)

1. B3 + B4 (Mac, no dependencies) — do first, they gate everything local.
2. B2 (host ABI v1.1 row; independent of 1, owned by the #9 team).
3. B6 consumes 1+2 outputs; B5 inside B6's M1 scope.
4. B1 (console rung) once M0/M1 are green — before any real-mod spend.
5. B7 (one real mod download) any time after B1 passes; B8 rides B7's census;
   B9 at packaging time.
