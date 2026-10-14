# REPORT — worker-r3-zerosyscall-tier (R3 zero-syscall tier design)

2026-10-04. PROPOSALS ONLY: nothing built, staged, or executed; no console contact; no
nested subagents. Deliverable order honored: CLAIM.json first, R3-TIER-DESIGN.md second,
REPORT.md (this file) last. Read-order contract honored:
`TEAM:worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md` read before all other analysis.

## Verdict (one paragraph)

The R3 tier (UFunction::SetFuncPtr) is real, complete in-tree, and platform-neutral — the
"Windows-only" hypothesis is false, and the Lua RegisterHook native branch ALREADY routes
through it on the PS5 build. The Item Spawner's only interception dependency is A9
ExecuteInGameThread; everything else in its chain (keybind, reflection, spawn, UI) is
direct port API needing zero hooks. R3 can carry A9 via a new "FuncPtr-marshal" (a
calibration-selected native UFunction whose SetFuncPtr hook drains the existing
m_game_thread_actions queue), but the ladder can NOT complete on R3 as composed today
because init fatally throws on the EngineTick registrations before any Lua runs — two
small diffs (LuaMod.cpp + staged initializer) are prerequisites. With those diffs, spawner
visible-menu+spawn completion on R3 is conditional on exactly one new in-game fact (F-1: a
viable carrier UFunction exists), for which this design provides a falsifiable calibration
protocol. A10/LoadMap and all global-visibility features stay dead on R3.

## Task 1 findings (SetFuncPtr + consumers) — key file:line evidence

- The write: `SRC:port/unreal/src/UFunction.cpp:33-36` — one pointer store via `GetFunc()`;
  Func field = `MemberOffsets["Func"]`, default 0xD8 for 5_04/5_06/5_07/5_08
  (`generated_include/FunctionBodies/5_06_MemberVariableLayout_DefaultSetter_UFunction.cpp:45-49`),
  loud throw if absent (`generated_include/MemberVariableLayout_SrcWrapper_UFunction.hpp:194-211`).
- Hook install = `UFunction.cpp:78-89` (map-or-create, original saved :85,
  `SetFuncPtr(&UnrealScriptFunctionHook)` :86); registration surface :91-132
  (UnregisterAllHooks restores :129). Hook body `UFunctionStructs.cpp:163-333`:
  pre/post callbacks, original invocation :287, out-parm copyback :300-312,
  GNatives_Internal-guarded re-framing (:210 — degraded-but-safe path :207),
  ProcessInternal fallback via ExecuteUbergraph :198-202.
- Coverage: every reflected call of that ONE function, game- AND port-initiated (port
  calls go `IMPLEMENT_UNREAL_VIRTUAL_WRAPPER` `UObject.cpp:448-451` → game ProcessEvent →
  Func field). NOT EngineTick; NOT global ProcessEvent/ProcessInternal; the Lua
  script-hook path drains via ProcessLocalScriptFunction/ProcessInternal detours
  (`LuaMod.cpp:7670-7675`) — dead under both-fail.
- Port support: NO platform guards in UFunction.cpp/UFunctionStructs.cpp. Live consumers:
  Lua RegisterHook native branch `LuaMod.cpp:4618-4632` (RegisterPreHook/PostHook at
  :4624-4625) and C++ `UObjectGlobals.cpp:1122-1133`. Native RegisterHook keeps working
  even if FLAG-3 (ProcessInternal address) stays unresolved — `:4619`'s `func_ptr &&`
  passes for real native pointers.

## Task 2 findings (spawner dependency map)

- Keybind `RegisterKeyBind` (`IS:694-729`) → port-owned input (`LuaMod.cpp:1949-1956` →
  `port/first/src/Input/Handler.cpp:105`): no interception.
- `ExecuteInGameThread` (`IS:695,700,705,710,716,725`) = THE hook dependency; today
  EngineTick/ProcessEvent detour drain (`LuaMod.cpp:4743-4765`; default EngineTick,
  staged ini `worker-itemspawner-bundle/bundle/ue4ss/UE4SS-settings.ini:8`).
- Reflection (`FindFirstOf`/`StaticFindObject`/`StaticConstructObject`/property access/
  UFunction calls/spawn via `BeginDeferredActorSpawnFromClass` `IS:294-303`): all direct
  port API — NO interception. A10 `RegisterLoadMapPreHook` (`IS:731`) is pcall-guarded
  and dies under R3-only (stale-menu hazard recorded).
- Trap found: `EngineTickAvailable`/`ProcessEventAvailable` Lua globals reflect AOB-scan
  address-availability only (`UE4SSRuntime.cpp:10-24`), not detour arming — a mod could
  see TRUE and still fail at first registration.

## Design delivered (R3-TIER-DESIGN.md)

FuncPtr-marshal: reuse `process_simple_actions` (`LuaMod.cpp:4113`) + `IsInGameThreadRaw`
gate (`:4418-4425`) behind a `RegisterPostHook` on a calibration-selected carrier
(enumeration → bounded trial hooks → rate/thread measurement → select/demote → recorded
outcome = F-1 falsifier). The init-time EngineTick dependencies (game-thread identity
publication `SINI:152-180`; query-harness dispatch `SINI:1390-1397`; FName detour
`SINI:945-958` per VSD §5 C1) are re-routed/dead-coded explicitly. Composition: NO diff
for `unreal-ufunction` / `unreal-flat-ufunctionstructs`; DIFF 1 = `luamod`
(enumerate/availability/enum/settings); DIFF 2 = initializer replace-unit; argv unchanged
(pinned recipes; `unreal-ufunction` argv captured verbatim from compile-recipes.json);
composition-4's pin-walk lesson (`worker-composition-4/REPORT.md:39-52,105-113`) carried
in.

## Honest limits

- F-1 is OPEN: no retained evidence names any native UFunction's live invocation rate on
  Townfall; the calibration protocol is the falsifier, not a promise.
- Query-harness semantics change under the marshal (counts carrier invocations, not
  engine ticks) — instrument relabeling is a controller-level acceptance note.
- Static analysis only; every "expected" statement is a prediction from pinned sources
  and prior workers' retained evidence. No acceptance is claimed for any rung.
- Line numbers cite this session's reads of SRC/SINI/IS/TEAM files; if any file is
  restaged, cites must be re-verified before execution.

## Tool-call budget

~30 tool calls (limit 40-70): under budget; the surplus was not needed — the in-tree
completeness of the R3 route (zero-diff core) collapsed the search space early.

## Artifacts

- `outputs/2026-10-04-zcode-team/worker-r3-zerosyscall-tier/CLAIM.json` (written first)
- `outputs/2026-10-04-zcode-team/worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md`
- `outputs/2026-10-04-zcode-team/worker-r3-zerosyscall-tier/REPORT.md` (this file, last)
