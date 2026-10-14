# COVERAGE-MAP.md — Conformance coverage of the UE4SS API catalog

Generated 2026-10-07 (rev 3), Mac-only, static analysis of the project tree. No console contact.
Rev 3 maps the **catalog v4** additions: the `Key` (165 constants) and `ModifierKey`
(3 active constants) TYPE entries added to `catalog/globals.json` after the v3-era rev-2
snapshot. The conformance scripts are **byte-identical to rev 2** (verified by mtime and
content diff against the `src-conformance-*` copies), so per-row coverage changed only for
the two new entries — both were already exercised by the static suite's constant spot-checks
(19 `Key` constants + `Key.count`, 3 `ModifierKey` constants), which rev 2 had counted as
*orphan* CALL rows; v4 gave them catalog counterparts and rev 3 moves them into Table 1a.
The 353 static + 86 live = 439 CALL-row totals are unchanged. Rev 2 reflected the
conformance_live.lua name-mismatch fixes (TArray battery, `UClass:GetFunction`,
the fictional `Loop` table, FString/FText metamethod rows, hook validator/arity bugs), the new
no-op gated calls for every previously-untested hooking global, the self-owned scheduling
register/cancel cycles, and 6 catalog additions (`EGameThreadMethod`, `EngineTickAvailable`,
`ProcessEventAvailable`, `ue4ss_loaded_modules`, `__OriginalReturnValue`, `NAME_None`).

Maps **every catalog entry** to the conformance script that calls it, or the reason it is not called.

- Catalog: `catalog/globals.json` (106 globals after the 2026-10-07 rev-2 (+6) and
  v4 (+`Key`/`ModifierKey` input TYPE entries) additions) +
  `catalog/types.json` (42 types, 456 method entries + 20 metamethods = 476 rows). Note: 10 of
  the globals are dotted entries (`UE4SS.GetVersion`, the seven `UnrealVersion.*`, the two
  `FPackageName.*`) that duplicate type-method rows in `types.json`; they are listed consistently
  in both tables below but counted once per list, so 106 + 476 = 582 table rows = 572 distinct
  APIs + 10 duplicates.
  (Counts by unit as defined in `docs/COUNTS.md`: 456 raw methods, 476 = methods + metamethods.
  The rev-1 figures were 98 globals / 574 coverage-API rows / 591 merged-catalog entries / 616 raw
  catalog rows; after catalog rev 2026-10-07-v3 (+6 globals) and the rev-2 map they were
  104 globals / 580 coverage-API rows; after catalog v4 (+2 input TYPE containers) and this
  rev-3 map they are 106 globals / 582 coverage-API rows. Quote the unit, not just the number.)
- Scripts: `conformance/conformance_static.lua` ("static") and `conformance/conformance_live.lua` ("live").
- `covered_by` = which script emits a matching `CALL` row: `static`, `live`, `both`, or `not-covered`.
- `how` distinguishes **invoked** (the API is actually called) from **presence-only** (static's
  `global_presence` section emits `CALL\tpresence:<name>` rows that only index `_G[name]` and
  check `type()` — existence proof, never an invocation).
- `reason_if_not_covered` is filled for every row that is not invoked somewhere (not-covered rows
  and presence-only rows alike).

## Headline numbers

| scope | entries | invoked anywhere | presence-only (static) | any CALL row | not covered |
|---|---|---|---|---|---|
| globals (106) | 106 | 70 | 13 | 83 | 23 |
| type methods (456) + metamethods (20) | 476 | 80 | 0 | 80 | 396 |
| **total** | **582** | **150** | **13** | **163 (28.0%)** | **419** |

**163 of 582 catalog rows (28.0%) have a matching CALL
line; 150 (25.8%) are actually invoked.** The 353 static + 86 live = 439 CALL
rows are heavily concentrated: string batteries and constant-table spot checks account for most
of the static volume, while 396 of the 476 type-method rows (83%) have no call site at all.

## Call-count reconciliation (the rev-1 "~405 calls" claim vs the rev-2 439)

| script | literal call sites | loop-expanded | runtime total | matches claim |
|---|---|---|---|---|
| conformance_static.lua | 76 direct (86 source sites minus 10 loop templates) | 279 (presence 58, string-ctor batteries 3x21, EFindName 3, ModifierKey 3, EObjectFlags 31, EInternalObjectFlags 10, Key 20, PropertyTypes 23x4) | **353** (the EGameThreadMethod if/else contributes exactly 2 of its 4 sites per run) | yes — REVIEW-PACKAGE says 353 |
| conformance_live.lua (rev 2) | 72 | 0 (prerequisite-gated rows flip call<->skip 1:1, never lost) | **86** on every run path (host-mock-verified happy path: 82 PASS + 1 FAIL [the intentional string.toFName absence probe] + 3 SKIP [LoopAsync unsafe-by-design; TArray __newindex/Empty read-only exclusions]) | supersedes the rev-1 "52" figure |
| total | | | **439** | 353 static + 86 live |

Rev 3 adds **zero** new CALL rows: the `Key` (20) and `ModifierKey` (3) constant rows counted
above already existed in rev 2 as *orphan* rows (no catalog counterpart); catalog v4 gave them
counterparts, which is why covered rows grew 161 → 163 while the call totals stayed 439.

## Required honesty statement

> **Pass under mock only proves the scripts run against our own stand-ins, not against the real runtime.**

Every "PASS" in the host/mock runs (353/353 static; live under the tester's mock environments)
exercises the scripts against mock environments authored on this same Mac
(`conformance/src-conformance-static/hosttest_mock_env.lua`, the tester's `hostile_mock.lua`).
No catalog entry has been proven against a real PS5 UE4SS runtime — `ps5_runtime_status` is `""`
in 106/106 globals and 476/476 type rows. Mock agreement is evidence the scripts are syntactically
valid, crash-safe and log-format-correct; it is **not** evidence of runtime conformance. The
name-mismatch defects the rev-1 coverage analysis found (TArray battery, `UClass:GetFunction`,
the fictional `Loop` table, FString/FText metamethod rows, `v == true` hook validators on
no-return registrations, single-id `UnregisterHook`) were exactly the kind of defect mocks cannot
catch — the mocks matched the scripts instead of the runtime — and are all FIXED in the rev-2
live script against the port source (`native-coherent-runtime-1/source/port/ue4ss`), with the
same names re-verified in the upstream citation tree. The fixed calls still have zero real-runtime
executions; that proof remains outstanding.

---

## Table 1a — Globals (106 rows, grouped by category)


### Category: `object-management`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `CreateInvalidObject` | static | invoked | — | CreateInvalidObject (+ :type/:IsValid/:GetAddress) L835-849 |
| `FindAllOf` | both | static=presence-only; live=invoked | — | live: class=Engine, class=Pawn |
| `FindFirstOf` | both | static=presence-only; live=invoked | — | live: Engine/Pawn/PlayerController probes + PREQ rows |
| `FindObject` | static | presence-only | script author missed it — flag for follow-up (live covered equivalent finders FindFirstOf/FindAllOf/StaticFindObject) | presence:FindObject (global_presence section, L314-351) |
| `FindObjects` | static | presence-only | script author missed it — flag for follow-up (same shape as the covered finders) | presence:FindObjects (global_presence section, L314-351) |
| `ForEachUObject` | static | presence-only | needs specific game state (iterates every live UObject; static excludes it by design) — flag for follow-up | presence:ForEachUObject (global_presence section, L314-351) |
| `LoadAsset` | static | presence-only | destructive / mutates game state — excluded by the suites' read-only contract (asset load) | presence:LoadAsset (global_presence section, L314-351) |
| `StaticConstructObject` | static | presence-only | destructive / mutates game state — excluded by the suites' read-only contract (spawns a UObject) | presence:StaticConstructObject (global_presence section, L314-351) |
| `StaticFindObject` | both | static=presence-only; live=invoked | — | live: UClass, UFunction, negative probe (3 rows) |

### Category: `reflection`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `RegisterCustomProperty` | static | presence-only | script author missed it — flag for follow-up (must run at mod startup with a property-list builder; no no-op form authored) | presence:RegisterCustomProperty (global_presence section, L314-351) |

### Category: `string`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `FAnsiString` | static | invoked | — | 21-row ctor/method/error battery (section 'fansistring') |
| `FString` | both | static=invoked; live=invoked | — | static: 21-row battery; live: ctor + tostring + concat |
| `FUtf8String` | static | invoked | — | 21-row ctor/method/error battery (section 'futf8string') |
| `NAME_None` *(new in catalog rev 2)* | static | invoked | — | NAME_None:ToString / :GetComparisonIndex / FName(0)==NAME_None rows + presence row (FName-instance global) |

### Category: `file-io`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `CreateLogicModsDirectory` | not-covered | — | script author missed it — flag for follow-up (host-side mkdir into the mod tree; safe to test) | — |
| `dofile` | not-covered | — | script author missed it — flag for follow-up (arbitrary-file Lua execution; also may be sandboxed away in UE4SS) | — |
| `IterateGameDirectories` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `ue4ss_loaded_modules` *(new in catalog rev 2)* | static | invoked | — | misc section run_call: reads the table, reports table(n=..) + presence row |
| `loadfile` | not-covered | — | script author missed it — flag for follow-up (arbitrary-file Lua load) | — |

### Category: `input`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `IsKeyBindRegistered` | both | static=invoked; live=invoked | — | static: Key.F6 + 999 error path; live: combo query |
| `Key` *(new in catalog v4)* | static | invoked | — | 19-constant spot-check list (LEFT_MOUSE_BUTTON…RIGHT_SHIFT) + `Key.count` row (constants section; TYPE entry, 165 constants, `pc_only_input: true`) |
| `ModifierKey` *(new in catalog v4)* | static | invoked | — | SHIFT/CONTROL/ALT constant reads (constants section; TYPE entry, 3 active constants, `pc_only_input: true`) |
| `RegisterKeyBind` | both | static=presence-only; live=invoked | — | live: CTRL+SHIFT+F8 no-op combo, never triggered |
| `RegisterKeyBindAsync` | not-covered | — | script author missed it — flag for follow-up (sync RegisterKeyBind is covered live; async variant overlooked) | — |

### Category: `hooking`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `NotifyOnNewObject` | both | static=presence-only; live=invoked | — | live: no-op callback for '/Script/Engine.ConformanceNeverSpawned' (class never instantiates, so the callback can never fire; no unregister API exists — registration lives for the mod lifetime) |
| `RegisterBeginPlayPostHook` | live | invoked | — | no-op callback registration (port returns no value; success-without-error is the criterion) |
| `RegisterBeginPlayPreHook` | live | invoked | — | same no-op pattern |
| `RegisterCallFunctionByNameWithArgumentsPostHook` | live | invoked | — | same no-op pattern |
| `RegisterCallFunctionByNameWithArgumentsPreHook` | live | invoked | — | same no-op pattern |
| `RegisterConsoleCommandGlobalHandler` | live | invoked | — | no-op handler for 'conformance.noop.global' |
| `RegisterConsoleCommandHandler` | live | invoked | — | no-op handler for 'conformance.noop' (rev-2: dropped the bogus `v == true` validator — the port returns NO value) |
| `RegisterCustomEvent` | both | static=presence-only; live=invoked | — | live: 'ConformanceNoopEvent' no-op handler (rev-2: dropped the bogus `v == true` validator) |
| `RegisterEndPlayPostHook` | live | invoked | — | same no-op pattern |
| `RegisterEndPlayPreHook` | live | invoked | — | same no-op pattern |
| `RegisterHook` | both | static=presence-only; live=invoked | — | live: no-op pass-through on GetAllActorsOfClass; rev-2 FIX captures BOTH returned ids (pre, post) per the port signature |
| `RegisterInitGameStatePostHook` | live | invoked | — | same no-op pattern |
| `RegisterInitGameStatePreHook` | live | invoked | — | same no-op pattern |
| `RegisterLoadMapPostHook` | live | invoked | — | no-op callback (rev-2: dropped the bogus `v == true` validator — the port returns NO value, `return 0`) |
| `RegisterLoadMapPreHook` | live | invoked | — | same no-op pattern |
| `RegisterProcessConsoleExecPostHook` | live | invoked | — | same no-op pattern |
| `RegisterProcessConsoleExecPreHook` | live | invoked | — | same no-op pattern |
| `RegisterULocalPlayerExecPostHook` | live | invoked | — | same no-op pattern |
| `RegisterULocalPlayerExecPreHook` | live | invoked | — | same no-op pattern |
| `UnregisterCustomEvent` | live | invoked | — | unregisters the suite's own ConformanceNoopEvent immediately after RegisterCustomEvent |
| `UnregisterHook` | live | invoked | — | rev-2 FIX: called as UnregisterHook(path, pre_id, post_id) per the port signature (was wrongly single-id) |
| `__OriginalReturnValue` *(new in catalog rev 2)* | static | invoked | — | misc section run_call: read expecting nil (nil-initialized in both source trees, never runtime-written; verified full-text) + presence row |

### Category: `threading`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `CancelDelayedAction` | live | invoked | — | rev 2: self-owned cycle — cancels the suite's own delayed/looping actions (port: only the calling mod's own actions are cancellable); 4 rows |
| `ClearAllDelayedActions` | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (would cancel OTHER mods' delayed actions) | — |
| `ExecuteAsync` | static | presence-only | destructive / mutates game state — excluded by the suites' read-only contract (spawns async work; static header excludes Execute*/Loop*) | presence:ExecuteAsync (global_presence section, L314-351) |
| `ExecuteInGameThread` | both | static=presence-only; live=invoked | — | live: queued no-op + deferred-flag verification (2 rows) |
| `EGameThreadMethod` *(new in catalog rev 2)* | static | invoked | — | EGameThreadMethod.EngineTick/ProcessEvent rows (if/else pair; 2 of 4 sites per run) + presence row (reading the constant table IS the invocation) |
| `EngineTickAvailable` *(new in catalog rev 2)* | static | invoked | — | boolean capability global read row (reading is the invocation) |
| `ProcessEventAvailable` *(new in catalog rev 2)* | static | invoked | — | boolean capability global read row (reading is the invocation) |
| `ExecuteInGameThreadAfterFrames` | not-covered | — | script author missed it — flag for follow-up (the sibling LoopInGameThreadAfterFrames IS now covered live via a self-owned cancel cycle) | — |
| `ExecuteInGameThreadWithDelay` | live | invoked | — | rev 2: self-owned cycle — 30s no-op action (never fires) -> handle -> immediate CancelDelayedAction |
| `ExecuteWithDelay` | both | static=presence-only; live=invoked | — | live rev 2: explicit 50ms no-op CALL row (deferred-flag verification) + it is the bootstrap delay mechanism (replaced the fictional Loop.ExecuteWithDelay) |
| `GetAsyncThreadId` | static | invoked | — | GetAsyncThreadId() + :type (section 'threads') |
| `GetCurrentThreadId` | static | invoked | — | GetCurrentThreadId() + :type (section 'threads') |
| `GetDelayedActionRate` | static | invoked | — | invalid-handle |
| `GetDelayedActionTimeElapsed` | static | invoked | — | invalid-handle |
| `GetDelayedActionTimeRemaining` | static | invoked | — | invalid-handle (expect -1) |
| `GetGameThreadId` | static | invoked | — | GetGameThreadId() + :type (section 'threads') |
| `GetMainModThreadId` | static | invoked | — | GetMainModThreadId() + :type (section 'threads') |
| `IsDelayedActionActive` | static | invoked | — | invalid-handle (expect false) |
| `IsDelayedActionPaused` | static | invoked | — | invalid-handle (expect false) |
| `IsInAsyncThread` | static | invoked | — | expect false |
| `IsInGameThread` | static | invoked | — | expect false |
| `IsInMainModThread` | static | invoked | — | expect true |
| `IsValidDelayedActionHandle` | static | invoked | — | invalid-handle + error path |
| `LoopAsync` | static | presence-only | unsafe by design, NOT "not written yet": schedules a callback repeating FOREVER on the shared async thread (ActionType::Loop, port LuaMod.cpp:3390); returns no handle and has NO cancellation API (CancelDelayedAction never searches m_pending_actions) — calling it would leave permanent background work for the mod's lifetime | presence:LoopAsync (global_presence section, L314-351) |
| `LoopInGameThreadAfterFrames` | live | invoked | — | rev 2: self-owned cycle — 100000-frame no-op loop (never fires) -> handle -> immediate CancelDelayedAction; no loop survives the battery |
| `LoopInGameThreadWithDelay` | live | invoked | — | rev 2: self-owned cycle — 30s no-op loop (is_looping) -> handle -> immediate CancelDelayedAction |
| `MakeActionHandle` | both | static=invoked; live=invoked | — | static: MakeActionHandle x2 + uniqueness check; live: fresh self-owned handle for the retriggerable cycle |
| `PauseDelayedAction` | not-covered | — | needs an actively scheduled action — script author missed it — flag for follow-up | — |
| `ResetDelayedActionTimer` | not-covered | — | needs an actively scheduled action — script author missed it — flag for follow-up | — |
| `RetriggerableExecuteInGameThreadWithDelay` | live | invoked | — | rev 2: self-owned cycle — MakeActionHandle -> 30s no-op retriggerable -> CancelDelayedAction |
| `SetDelayedActionTimer` | not-covered | — | needs an actively scheduled action — script author missed it — flag for follow-up | — |
| `UnpauseDelayedAction` | not-covered | — | needs an actively owned paused action; the rev-2 cycle cancels its actions instead (pause/unpause of a to-be-cancelled no-op adds no evidence) — flag for follow-up if a longer-lived self-owned action is ever authorized | — |

### Category: `debug`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `DerefToInt32` | not-covered | — | needs specific game state (not testable without a running game) (raw memory deref of a valid live address) | — |
| `dereftoint32` | not-covered | — | needs specific game state (not testable without a running game) (lowercase alias, same) | — |
| `DumpAllActors` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `DumpAllObjects` | static | presence-only | heavy IO / full-game dump — excluded by design (static header lines 40-49) | presence:DumpAllObjects (global_presence section, L314-351) |
| `DumpJMAP` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `DumpStaticMeshes` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `DumpUSMAP` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `GenerateLuaTypes` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `GenerateSDK` | static | presence-only | heavy IO / full-game dump — excluded by design (static header lines 40-49) | presence:GenerateSDK (global_presence section, L314-351) |
| `GenerateUHTCompatibleHeaders` | not-covered | — | heavy IO / full-game dump — excluded by design (static header lines 40-49) | — |
| `print` | static | invoked | — | print smoke call |
| `Print` | not-covered | — | script author missed it — flag for follow-up (uppercase variant; static covers lowercase print only) | — |
| `UE4SS.GetVersion` | static | invoked | — | UE4SS.GetVersion -> 3 ints (also types.json UE4SS:GetVersion) |
| `UnrealVersion.GetMajor` | static | invoked | — | GetMajor variants incl. error path (7/7 methods) |
| `UnrealVersion.GetMinor` | static | invoked | — | GetMinor variants incl. error path (7/7 methods) |
| `UnrealVersion.IsAbove` | static | invoked | — | IsAbove variants incl. error path (7/7 methods) |
| `UnrealVersion.IsAtLeast` | static | invoked | — | IsAtLeast variants incl. error path (7/7 methods) |
| `UnrealVersion.IsAtMost` | static | invoked | — | IsAtMost variants incl. error path (7/7 methods) |
| `UnrealVersion.IsBelow` | static | invoked | — | IsBelow variants incl. error path (7/7 methods) |
| `UnrealVersion.IsEqual` | static | invoked | — | IsEqual variants incl. error path (7/7 methods) |

### Category: `other`

| api_name | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|
| `FPackageName.IsShortPackageName` | static | invoked | — | 2 positives/negatives + error path |
| `FPackageName.IsValidLongPackageName` | static | invoked | — | 2 positives/negatives |
| `LoadExport` | static | presence-only | by design: call emits [Fatal] log noise; presence-checked only (static header line 48-49) | presence:LoadExport (global_presence section, L314-351) |
| `loadexport` | not-covered | — | script author missed it — flag for follow-up (lowercase alias of LoadExport) | — |
| `RestartCurrentMod` | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (restarts the running conformance mod itself) | — |
| `RestartMod` | static | presence-only | destructive / mutates game state — excluded by the suites' read-only contract (restarts a running mod) | presence:RestartMod (global_presence section, L314-351) |
| `UninstallCurrentMod` | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (would remove the running mod) | — |
| `UninstallMod` | static | presence-only | destructive / mutates game state — excluded by the suites' read-only contract (removes a mod) | presence:UninstallMod (global_presence section, L314-351) |

---

## Table 1b — Type methods and metamethods (456 + 20 rows, grouped by type)


### Type: `RemoteObjectBase`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `RemoteObjectBase.GetAddress` | method | both | static=invoked; live=invoked | — | static: CreateInvalidObject:GetAddress (expect 0); live: engine:GetAddress() |
| `RemoteObjectBase.IsValid` | method | both | static=invoked; live=invoked | — | static: CreateInvalidObject:IsValid (false); live: engine:IsValid (true) |
| `RemoteObjectBase.type` | method | static | invoked | — | CreateInvalidObject:type() (also FName:type / FText:type / ModRef:type / ThreadId:type rows) |

### Type: `UObject`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UObject.GetAddress` | method | both | static=invoked; live=invoked | — | static: CreateInvalidObject:GetAddress; live: UObject.GetAddress |
| `UObject.GetFullName` | method | live | invoked | — | engine:GetFullName() |
| `UObject.GetFName` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.GetClass` | method | live | invoked | — | engine:GetClass() -> IsA dependency |
| `UObject.GetOuter` | method | live | invoked | — | engine:GetOuter() |
| `UObject.IsAnyClass` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.Reflection` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.GetPropertyValue` | method | live | invoked | — | engine:GetPropertyValue('bUseFixedFrameRate') |
| `UObject.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UObject.IsClass` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.GetWorld` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.CallFunction` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (invokes a UFunction) | — |
| `UObject.IsA` | method | live | invoked | — | IsA(userdata) + IsA(string) x2 on Pawn |
| `UObject.HasAllFlags` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.HasAnyFlags` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.HasAnyInternalFlags` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.ProcessConsoleExec` | method | not-covered | — | script author missed it — flag for follow-up (an Engine/Pawn receiver was already in hand in the live battery) | — |
| `UObject.IsValid` | method | both | static=invoked; live=invoked | — | static: CreateInvalidObject:IsValid; live: engine:IsValid |
| `UObject.__index` | metamethod | live | invoked | — | 4 read-only property reads (Engine/Pawn/PlayerController + the discovered array property that creates the TArray view) |
| `UObject.__newindex` | metamethod | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UObject.__call` | metamethod | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (invokes a UFunction) | — |
| `UObject.__eq` | metamethod | not-covered | — | script author missed it — flag for follow-up (invalid==invalid was one line away in static) | — |

### Type: `UStruct`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UStruct.GetAddress` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetFullName` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetFName` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetClass` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetOuter` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.IsAnyClass` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.Reflection` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetPropertyValue` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UStruct.IsClass` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetWorld` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.CallFunction` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.IsA` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.HasAllFlags` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.HasAnyFlags` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.IsValid` | method | not-covered | — | needs specific game state (no UStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UStruct.GetSuperStruct` | method | live | invoked | — | rev 2: UClass(Actor) receiver (the UClass metatable chains UStruct member functions — LuaUClass.cpp UClass::construct calls UStruct::construct) |
| `UStruct.ForEachFunction` | method | live | invoked | — | rev 2: UClass(Actor) receiver, immediate-stop callback (returns true on first UFunction; bounded, read-only) |
| `UStruct.ForEachProperty` | method | live | invoked | — | rev 2: UClass(Actor) receiver, discovery callback that stops at the first ArrayProperty (drives the TArray battery) |

### Type: `UClass`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UClass.GetAddress` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetFullName` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetFName` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetClass` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetOuter` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.IsAnyClass` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.Reflection` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetPropertyValue` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.SetPropertyValue` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.IsClass` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetWorld` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.CallFunction` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.IsA` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.HasAllFlags` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.HasAnyFlags` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.IsValid` | method | not-covered | — | needs specific game state (no UClass-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UClass.GetSuperStruct` | method | live | invoked | — | rev 2: UClass(Actor) -> super struct (non-nil validated) |
| `UClass.ForEachFunction` | method | live | invoked | — | rev 2: UClass(Actor) receiver, immediate-stop callback (replaces the fictional UClass.GetFunction row — there is NO GetFunction on UClass in either tree) |
| `UClass.ForEachProperty` | method | live | invoked | — | rev 2: via the UStruct-chained metatable; discovery callback for the TArray battery |
| `UClass.GetCDO` | method | live | invoked | — | rev 2: UClass(Actor):GetCDO() non-nil |
| `UClass.IsChildOf` | method | live | invoked | — | rev 2: UClass(Actor):IsChildOf(itself) -> true |

### Type: `UInterface`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UInterface.GetAddress` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetFullName` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetFName` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetClass` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetOuter` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.IsAnyClass` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.Reflection` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetPropertyValue` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UInterface.IsClass` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetWorld` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.CallFunction` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.IsA` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.HasAllFlags` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.HasAnyFlags` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.IsValid` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetSuperStruct` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.ForEachFunction` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.ForEachProperty` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.GetCDO` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UInterface.IsChildOf` | method | not-covered | — | needs specific game state (no UInterface-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |

### Type: `UFunction`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UFunction.GetAddress` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetFullName` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetFName` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetClass` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetOuter` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.IsAnyClass` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.Reflection` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetPropertyValue` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UFunction.IsClass` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetWorld` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.CallFunction` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.IsA` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.HasAllFlags` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.HasAnyFlags` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.IsValid` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetSuperStruct` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.ForEachFunction` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.ForEachProperty` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.GetFunctionFlags` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.SetFunctionFlags` | method | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UFunction.__call` | metamethod | not-covered | — | needs specific game state (no UFunction-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |

### Type: `UEnum`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UEnum.GetAddress` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetFullName` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetFName` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetClass` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetOuter` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.IsAnyClass` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.Reflection` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetPropertyValue` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UEnum.IsClass` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetWorld` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.CallFunction` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.IsA` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.HasAllFlags` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.HasAnyFlags` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.IsValid` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetNameByValue` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.ForEachName` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.GetEnumNameByIndex` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.InsertIntoNames` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.EditNameAt` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.EditValueAt` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UEnum.RemoveFromNamesAt` | method | not-covered | — | needs specific game state (no UEnum-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |

### Type: `UScriptStruct`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UScriptStruct.GetAddress` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetFullName` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetFName` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetClass` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetOuter` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.IsAnyClass` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.Reflection` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetPropertyValue` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UScriptStruct.IsClass` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetWorld` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.CallFunction` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.IsA` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.HasAllFlags` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.HasAnyFlags` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.IsValid` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetSuperStruct` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.ForEachFunction` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.ForEachProperty` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetBaseAddress` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetStructAddress` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetPropertyAddress` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.IsValid` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.IsMappedToObject` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.IsMappedToProperty` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.GetProperty` | method | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.__index` | metamethod | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UScriptStruct.__newindex` | metamethod | not-covered | — | needs specific game state (no UScriptStruct-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |

### Type: `UWorld`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UWorld.GetAddress` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.GetFullName` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.GetFName` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.GetClass` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.GetOuter` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.IsAnyClass` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.Reflection` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.GetPropertyValue` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.SetPropertyValue` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.IsClass` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.GetWorld` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.CallFunction` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.IsA` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.HasAllFlags` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.HasAnyFlags` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.IsValid` | method | not-covered | — | needs specific game state (no UWorld-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UWorld.SpawnActor` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (spawns an actor) | — |

### Type: `AActor`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `AActor.GetAddress` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetFullName` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetFName` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetClass` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetOuter` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.IsAnyClass` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.Reflection` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetPropertyValue` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.SetPropertyValue` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.IsClass` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetWorld` | method | not-covered | — | script author missed it — flag for follow-up (a Pawn receiver was in hand) | — |
| `AActor.CallFunction` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.IsA` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.HasAllFlags` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.HasAnyFlags` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.IsValid` | method | not-covered | — | needs specific game state (no AActor-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `AActor.GetWorld` | method | not-covered | — | script author missed it — flag for follow-up (a Pawn receiver was in hand) | — |
| `AActor.GetLevel` | method | not-covered | — | script author missed it — flag for follow-up (a Pawn receiver was in hand) | — |

### Type: `UDataTable`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UDataTable.GetAddress` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetFullName` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetFName` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetClass` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetOuter` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.IsAnyClass` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.Reflection` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetPropertyValue` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.SetPropertyValue` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (property write) | — |
| `UDataTable.IsClass` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetWorld` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.CallFunction` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.IsA` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.HasAllFlags` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.HasAnyFlags` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.HasAnyInternalFlags` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.ProcessConsoleExec` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.IsValid` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetRowStruct` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetRowMap` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.FindRow` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.AddRow` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.RemoveRow` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.EmptyTable` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetRowNames` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.GetAllRows` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.ForEachRow` | method | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `UDataTable.__len` | metamethod | not-covered | — | needs specific game state (no UDataTable-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |

### Type: `FName`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FName.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (FName instances are constructed throughout static) | — |
| `FName.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (same) | — |
| `FName.ToString` | method | both | static=invoked; live=invoked | — | static: FName:ToString x2 + NAME_None:ToString; live: FName.ToString |
| `FName.GetComparisonIndex` | method | static | invoked | — | FName:GetComparisonIndex + NAME_None:GetComparisonIndex (expect 0) |
| `FName.Equals` | method | both | static=invoked; live=invoked | — | static: same-name/different-name; live: FName.Equals |
| `FName.__call` | metamethod | both | static=invoked; live=invoked | — | static: ctor battery (string/int/error paths); live: FName.new |
| `FName.__eq` | metamethod | both | static=invoked; live=invoked | — | static: 3 rows incl. FName(0)==NAME_None; live: FName.__eq(==) |

### Type: `FString`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FString.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (instances are constructed throughout the static battery) | — |
| `FString.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (instances are constructed throughout the static battery) | — |
| `FString.ToString` | method | both | static=invoked; live=invoked | — | static: roundtrip expect 'Hello Conformance'; live rev 2: FString('conformance_fstring') roundtrip + post-Append roundtrip (replaced the bogus tostring() row — string types have NO __tostring metamethod in the port) |
| `FString.Empty` | method | static | invoked | — | then IsEmpty/Len/ToString |
| `FString.Clear` | method | static | invoked | — | then IsEmpty/Len |
| `FString.Len` | method | both | static=invoked; live=invoked | — | static: expect #sample; live rev 2: expect #'conformance_fstring' |
| `FString.IsEmpty` | method | static | invoked | — | false/true after Empty |
| `FString.Append` | method | both | static=invoked; live=invoked | — | static: string/userdata/error x4; live rev 2: Append('_suffix') on the Lua-owned in-memory copy + ToString cross-check (replaced the bogus `..` concat row — FString has NO __concat metamethod in the port) |
| `FString.Find` | method | static | invoked | — | hit at 7 / nil miss / error path |
| `FString.StartsWith` | method | static | invoked | — | true/false |
| `FString.EndsWith` | method | static | invoked | — | true |
| `FString.ToUpper` | method | static | invoked | — | 'HELLO CONFORMANCE' |
| `FString.ToLower` | method | static | invoked | — | roundtrip |

### Type: `FUtf8String`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FUtf8String.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (instances are constructed throughout the static battery) | — |
| `FUtf8String.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (instances are constructed throughout the static battery) | — |
| `FUtf8String.ToString` | method | static | invoked | — | roundtrip expect 'Hello Conformance' |
| `FUtf8String.Empty` | method | static | invoked | — | then IsEmpty/Len/ToString |
| `FUtf8String.Clear` | method | static | invoked | — | then IsEmpty/Len |
| `FUtf8String.Len` | method | static | invoked | — | expect #sample |
| `FUtf8String.IsEmpty` | method | static | invoked | — | false/true after Empty |
| `FUtf8String.Append` | method | static | invoked | — | string/userdata/error x4 |
| `FUtf8String.Find` | method | static | invoked | — | hit at 7 / nil miss / error path |
| `FUtf8String.StartsWith` | method | static | invoked | — | true/false |
| `FUtf8String.EndsWith` | method | static | invoked | — | true |
| `FUtf8String.ToUpper` | method | static | invoked | — | 'HELLO CONFORMANCE' |
| `FUtf8String.ToLower` | method | static | invoked | — | roundtrip |

### Type: `FAnsiString`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FAnsiString.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (instances are constructed throughout the static battery) | — |
| `FAnsiString.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (instances are constructed throughout the static battery) | — |
| `FAnsiString.ToString` | method | static | invoked | — | roundtrip expect 'Hello Conformance' |
| `FAnsiString.Empty` | method | static | invoked | — | then IsEmpty/Len/ToString |
| `FAnsiString.Clear` | method | static | invoked | — | then IsEmpty/Len |
| `FAnsiString.Len` | method | static | invoked | — | expect #sample |
| `FAnsiString.IsEmpty` | method | static | invoked | — | false/true after Empty |
| `FAnsiString.Append` | method | static | invoked | — | string/userdata/error x4 |
| `FAnsiString.Find` | method | static | invoked | — | hit at 7 / nil miss / error path |
| `FAnsiString.StartsWith` | method | static | invoked | — | true/false |
| `FAnsiString.EndsWith` | method | static | invoked | — | true |
| `FAnsiString.ToUpper` | method | static | invoked | — | 'HELLO CONFORMANCE' |
| `FAnsiString.ToLower` | method | static | invoked | — | roundtrip |

### Type: `FText`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FText.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (minor) | — |
| `FText.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (minor) | — |
| `FText.ToString` | method | both | static=invoked; live=invoked | — | static: global default instance, expect '' (null-guarded ToString); live rev 2: FText('ConformanceLiveText'):ToString() (replaced the bogus tostring() row — no __tostring metamethod) |
| `FText.__call` | metamethod | live | invoked | — | FText.new('ConformanceLiveText') |
| `FText.__eq` | metamethod | not-covered | — | script author missed it — flag for follow-up (minor: two FTexts were constructible in live) | — |

### Type: `TArray`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `TArray.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (the rev-2 live TArray view was in hand) | — |
| `TArray.GetArrayAddress` | method | live | invoked | — | rev 2: live array view (Pawn.<discovered array property>): address > 0 |
| `TArray.GetArrayNum` | method | live | invoked | — | rev 2: live array view: GetArrayNum() >= 0 |
| `TArray.GetArrayMax` | method | live | invoked | — | rev 2: live array view: Max >= Num cross-check |
| `TArray.GetArrayDataAddress` | method | not-covered | — | script author missed it — flag for follow-up (the rev-2 live TArray view was in hand) | — |
| `TArray.Empty` | method | not-covered | — | excluded by the read-only contract: Empty() clears live game memory (there is no in-memory TArray in this port to empty) | — |
| `TArray.ForEach` | method | live | invoked | — | rev 2: live array view, immediate-stop callback (returns true) — also dodges the known large-array stack-corruption TODO in source |
| `TArray.IsValid` | method | live | invoked | — | rev 2: live array view: true |
| `TArray.__index` | metamethod | live | invoked | — | rev 2, two rows: (1) reading the discovered array-typed UObject property creates the TArray view; (2) in-bounds element read arr[1] only when #arr>=1 — OOB reads AddZeroed-grow live memory (LuaTArray.cpp prepare_to_handle), so reads are length-guarded |
| `TArray.__newindex` | metamethod | not-covered | — | excluded by the read-only contract: NO in-memory TArray constructor exists in either tree (TArray userdata is only a view of live game memory, created on array-property reads), so an element write (auto-grows) always mutates the game | — |
| `TArray.__len` | metamethod | live | invoked | — | rev 2: #arr must equal GetArrayNum() |

**Rev-2 note (TArray):** the rev-1 battery called `TArray()` / `:Length()` / `:Add()` / `:Get()` — none of
which exist. The port has no Lua-callable TArray constructor at all (TArray userdata is created only when
an array-typed UObject property is read — `src/LuaType/LuaUObject.cpp` `push_arrayproperty`
`Operation::Get`); the real members are `GetArrayNum`/`GetArrayMax`/`GetArrayAddress`/
`GetArrayDataAddress`/`Empty`/`ForEach`/`IsValid` plus `__index`/`__newindex` (1-based) and `__len` (`#a`).
The rev-2 battery discovers a real array property dynamically (`ForEachProperty` on UClass(Actor) until
`prop:type() == "ArrayProperty"`, name via `prop:GetFName():ToString()`) and reads it from the Pawn.

### Type: `TMap`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `TMap.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.Find` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.Add` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.Contains` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.Remove` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.Empty` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.ForEach` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TMap.__len` | metamethod | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |

### Type: `TSet`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `TSet.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.Add` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.Contains` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.Remove` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.Empty` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.ForEach` | method | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |
| `TSet.__len` | metamethod | not-covered | — | script author missed it — flag for follow-up (same live-array-view approach as the rev-2 TArray battery would work: find a reflected TMap/TSet property via ForEachProperty) | — |

### Type: `FWeakObjectPtr`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FWeakObjectPtr.GetAddress` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `FWeakObjectPtr.IsValid` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `FWeakObjectPtr.Get` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `FWeakObjectPtr.get` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |

### Type: `FSoftObjectPath`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FSoftObjectPath.GetAddress` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `FSoftObjectPath.IsValid` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `FSoftObjectPath.GetAssetPathName` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `FSoftObjectPath.GetSubPathString` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |

### Type: `TSoftObjectPtr`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `TSoftObjectPtr.GetAddress` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `TSoftObjectPtr.IsValid` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `TSoftObjectPtr.GetWeakPtr` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `TSoftObjectPtr.GetTagAtLastTest` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |
| `TSoftObjectPtr.GetObjectID` | method | not-covered | — | needs specific game state (not testable without a running game) (requires a live object handle / soft path) | — |

### Type: `FOutputDevice`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FOutputDevice.GetAddress` | method | not-covered | — | needs specific game state (no FOutputDevice-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `FOutputDevice.IsValid` | method | not-covered | — | needs specific game state (no FOutputDevice-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `FOutputDevice.Log` | method | not-covered | — | script author missed it — flag for follow-up (Engine's output device was reachable from the live battery) | — |

### Type: `FURL`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FURL.GetAddress` | method | not-covered | — | needs specific game state (no FURL-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `FURL.IsValid` | method | not-covered | — | needs specific game state (no FURL-typed receiver was ever exercised; only Engine/Pawn/PlayerController UObjects and one UClass(Actor) appear in live) | — |
| `FURL.__call` | metamethod | not-covered | — | catalog itself marks this metamethod a stub ('todo in source') | — |

### Type: `ThreadId`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `ThreadId.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (minor: ThreadId instances were in hand) | — |
| `ThreadId.IsValid` | method | not-covered | — | script author missed it — flag for follow-up (minor: ThreadId instances were in hand) | — |
| `ThreadId.ToString` | method | static | invoked | — | ThreadId:ToString non-empty check on GetCurrentThreadId() |
| `ThreadId.__eq` | metamethod | static | invoked | — | ThreadId.__eq self-comparison |

### Type: `XProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.GetFName` | method | live | invoked | — | rev 2: first ArrayProperty on UClass(Actor) -> GetFName():ToString() names the property the TArray battery reads (inside the ForEachProperty discovery callback) |
| `XProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XArrayProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XArrayProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XArrayProperty.GetInner` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XBoolProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XBoolProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetByteMask` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetByteOffset` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetFieldMask` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XBoolProperty.GetFieldSize` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XEnumProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XEnumProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XEnumProperty.GetEnum` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XInterfaceProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XInterfaceProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XInterfaceProperty.GetInterfaceClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XObjectProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XObjectProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XObjectProperty.GetPropertyClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XStructProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XStructProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XStructProperty.GetStruct` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XFieldClass`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XFieldClass.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XFieldClass.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XFieldClass.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XDelegateProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XDelegateProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XDelegateProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XMulticastDelegateProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XMulticastDelegateProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.Add` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.Remove` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.Broadcast` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (fires the delegate) | — |
| `XMulticastDelegateProperty.GetBindings` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastDelegateProperty.Clear` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `XMulticastSparseDelegateProperty`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `XMulticastSparseDelegateProperty.GetAddress` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.IsValid` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.GetClass` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.GetFullName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.GetFName` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.GetOffset_Internal` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.IsA` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.ContainerPtrToValuePtr` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.ImportText` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.Add` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.Remove` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.Broadcast` | method | not-covered | — | destructive / mutates game state — excluded by the suites' read-only contract (fires the delegate) | — |
| `XMulticastSparseDelegateProperty.GetBindings` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |
| `XMulticastSparseDelegateProperty.Clear` | method | not-covered | — | needs specific game state (reflected FProperty receiver required; note the PropertyTypes constant table IS covered statically — the X* method tables are not) | — |

### Type: `RemoteUnrealParam`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `RemoteUnrealParam.Get` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `RemoteUnrealParam.get` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `RemoteUnrealParam.set` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `RemoteUnrealParam.Set` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `RemoteUnrealParam.type` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |

### Type: `LocalUnrealParam`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `LocalUnrealParam.Get` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `LocalUnrealParam.get` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `LocalUnrealParam.Set` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `LocalUnrealParam.set` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |
| `LocalUnrealParam.type` | method | not-covered | — | by design — Remote/LocalUnrealParam values only reach Lua inside hook or UFunction callbacks, which the read-only suites deliberately never trigger | — |

### Type: `ModRef`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `ModRef.GetAddress` | method | not-covered | — | script author missed it — flag for follow-up (minor) | — |
| `ModRef.SetSharedVariable` | method | not-covered | — | by design: persists across mods (static header line 48 documents 'Set intentionally not called') | — |
| `ModRef.GetSharedVariable` | method | static | invoked | — | ModRef:GetSharedVariable('...Unset') -> nil |
| `ModRef.__newindex` | metamethod | not-covered | — | script author missed it — flag for follow-up (mod.OnUnload registration was one line away) | — |

### Type: `UE4SS`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UE4SS.GetVersion` | method | static | invoked | — | UE4SS.GetVersion -> major\|minor\|hotfix |

### Type: `UnrealVersion`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `UnrealVersion.GetMajor` | method | static | invoked | — | GetMajor variants incl. error path |
| `UnrealVersion.GetMinor` | method | static | invoked | — | GetMinor variants incl. error path |
| `UnrealVersion.IsEqual` | method | static | invoked | — | IsEqual variants incl. error path |
| `UnrealVersion.IsAtLeast` | method | static | invoked | — | IsAtLeast variants incl. error path |
| `UnrealVersion.IsAtMost` | method | static | invoked | — | IsAtMost variants incl. error path |
| `UnrealVersion.IsBelow` | method | static | invoked | — | IsBelow variants incl. error path |
| `UnrealVersion.IsAbove` | method | static | invoked | — | IsAbove variants incl. error path |

### Type: `FPackageName`

| api_name | kind | covered_by | how | reason_if_not_invoked_or_covered | evidence (CALL row) |
|---|---|---|---|---|---|
| `FPackageName.IsShortPackageName` | method | static | invoked | — | IsShortPackageName positives/negatives (+error path for IsShort) |
| `FPackageName.IsValidLongPackageName` | method | static | invoked | — | IsValidLongPackageName positives/negatives (+error path for IsShort) |

---

## Orphan CALL rows (in the scripts, not in the catalog)

These script rows have **no catalog counterpart**. They are conformance data too — but they
reveal script/catalog drift, and each one is a follow-up item for one side or the other.

| script | CALL row(s) | what it is | verdict |
|---|---|---|---|
| static | `_VERSION`, `jit.version`, `jit.arch`, `platform` | Lua/LuaJIT environment probes (env_info section) | intentional; not UE4SS APIs |
| static | `EObjectFlags.*` (31), `EInternalObjectFlags.*` (10), `EFindName.*` (3), `PropertyTypes.*` (92) | constant-table spot checks (136 rows — the bulk of static's volume) | intentional; catalog counts functions only — but note the volume these add to the 353 figure |
| live | `string.toFName(legacy-probe)` | string-library extension probe | **RESOLVED as a probe (rev 2)**: full-text search of the port AND upstream trees finds no `toFName` registration — it is not part of this UE4SS. The row is intentionally retained as an absence probe (FAIL = expected, informative); nothing to catalog |
| live | `FString.concat(lua)` | pure-Lua control row | intentional baseline, not a UE4SS API |
| live | `ExecuteInGameThread.executed`, `ExecuteWithDelay.executed` | deferred-flag verification rows | intentional meta-rows, not API calls |
| ~~live~~ | ~~`TArray.new`, `TArray.Length`, `TArray.Add`, `TArray.Get`~~ | rev-1 in-memory container battery | **FIXED (rev 2)**: none of those names exist (no Lua constructor at all); battery rewritten to the real members against a live array view — see the TArray table above |
| ~~live~~ | ~~`UClass.GetFunction`~~ | rev-1 UFunction lookup on UClass(Actor) | **FIXED (rev 2)**: no GetFunction exists on UClass in either tree (LuaUClass.cpp registers GetCDO/IsChildOf/type only); replaced with ForEachFunction/GetSuperStruct/IsChildOf/GetCDO rows |
| ~~live~~ | ~~`Loop.ExecuteWithDelay`, `Loop.CreateLoop`, `Loop.DestroyLoop`~~ | rev-1 timing/bootstrap layer | **FIXED (rev 2)**: there is NO `Loop` table in either tree (verified against the full register_function list); the script now uses the real globals ExecuteWithDelay/ExecuteInGameThreadWithDelay/LoopInGameThread*/Retriggerable* + MakeActionHandle/CancelDelayedAction; `LoopAsync` intentionally not called (see its row) |
| ~~static~~ | ~~`EGameThreadMethod`, `EngineTickAvailable`, `ProcessEventAvailable`, `ue4ss_loaded_modules`, `__OriginalReturnValue`, `NAME_None`~~ | former catalog gaps | **FIXED (catalog rev 2026-10-07-v3)**: all six added to globals.json with port+upstream registration sites; presence in both trees verified |
| ~~static~~ | ~~`Key.*` (19+count), `ModifierKey.*` (3)~~ | former catalog gaps (uncataloged input constant tables; ANSWERS.md Q2) | **FIXED (catalog v4, mapped in rev 3)**: `Key` (165 constants) and `ModifierKey` (3 active) TYPE entries added to globals.json with `pc_only_input: true`; the existing static spot-check rows now map to them (Table 1a, input category) |

## Follow-up flags (condensed, rev 3)

1. **TArray battery FIXED (rev 2)**: rewritten to the real members (`GetArrayNum`, `#a`, in-bounds
   `a[1]`, `GetArrayMax`, `IsValid`, `GetArrayAddress`, immediate-stop `ForEach`) against a live
   array view discovered via `ForEachProperty`; element writes and `Empty` are excluded by the
   read-only contract with the specific reason (writes always mutate live game memory — there is
   no in-memory TArray in this port).
2. **`UClass:GetFunction` FIXED (rev 2)**: removed (no such method in either tree); replaced with
   `ForEachFunction`/`GetSuperStruct`/`IsChildOf`/`GetCDO`.
3. **`Loop` table FIXED (rev 2)**: proven nonexistent; the bootstrap and battery now use the real
   scheduling globals, and the previously-never-called `ExecuteInGameThreadWithDelay`,
   `RetriggerableExecuteInGameThreadWithDelay`, `LoopInGameThreadWithDelay`,
   `LoopInGameThreadAfterFrames`, `MakeActionHandle` and `CancelDelayedAction` are all covered by
   self-owned register/cancel cycles. `LoopAsync` remains intentionally uncalled with the specific
   reason: perpetual repeating async-thread callback, no handle, no cancellation API.
   `ExecuteInGameThreadAfterFrames` remains an open trivial miss.
4. **Untested hooking globals FIXED (rev 2)**: all previously-uncovered hooking globals
   (BeginPlay/EndPlay/InitGameState/LoadMapPre/CallFunctionByNameWithArguments/ProcessConsoleExec/
   ULocalPlayerExec pre+post, `RegisterConsoleCommandGlobalHandler`, `UnregisterCustomEvent`,
   `NotifyOnNewObject`) now have live no-op gated calls. Hook validators fixed: the port's
   Register* functions return NO value, so success-without-error is the criterion (the old
   `v == true` validators would have FAILed every hook row on the real runtime).
   `RegisterHook`/`UnregisterHook` arity fixed (two ids; unregister takes path+pre+post).
5. **Delayed-action mutators**: `CancelDelayedAction` now covered; `Pause/Unpause/Set/Reset/Clear`
   remain uncovered (Set/Reset/ClearAll need live owned actions and ClearAll would cancel OTHER
   mods' actions — excluded; Pause/Unpause add no evidence over the cancel cycle).
6. **Trivially-available misses (still open)**: `UObject.GetFName/IsClass/IsAnyClass/GetWorld/flag
   checks`, `AActor.GetLevel`, `Print` vs `print`, `loadexport` vs `LoadExport`,
   `RegisterKeyBindAsync`, `FName/FString/...:GetAddress`+`IsValid`, `FText.__eq`,
   `ModRef.__newindex`, `TArray.GetAddress`/`GetArrayDataAddress`, `ExecuteInGameThreadAfterFrames`.
7. **Catalog gaps FIXED (rev 2026-10-07-v3)**: `EGameThreadMethod`, `EngineTickAvailable`,
   `ProcessEventAvailable`, `ue4ss_loaded_modules`, `__OriginalReturnValue`, `NAME_None` added to
   globals.json (verified present in BOTH the port tree and the upstream citation tree, with
   file:line from each). `string.toFName` proven absent from both trees (retained as a labeled
   absence probe, not cataloged). The FString/FText `__tostring`/`__concat` orphan rows were
   script bugs (no such metamethods exist — `setup_metamethods` is empty in
   `include/LuaType/LuaUnrealString.hpp`), fixed by switching the rows to `:ToString()`/`:Append()`.
   A fictional `Loop` table was deliberately NOT cataloged (it does not exist); TArray/UClass type
   entries gained factual notes documenting the no-constructor and no-GetFunction findings.
8. **Provenance caveat** (from the independent tester, still relevant): file:line references in the
   older catalog entries pointed at a tree the tester could not find; the rev-2/rev-3 catalog work
   cites BOTH the port tree (`native-coherent-runtime-1/source/port/ue4ss`) and the upstream
   citation tree (`ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS`), each verified
   present on this host on 2026-10-07. Older entries' line numbers remain unverified.
9. **Key/ModifierKey cataloged (v4, mapped in rev 3)**: the input constant tables are now catalog
   entries (`kind: "type"`, `category: "input"`, `pc_only_input: true` — Windows virtual-key
   codes; PS5 has no keyboard, keybinds need runtime/config controller mapping, e.g. PS5Pad.ini
   chords). Their 23 static spot-check rows moved from the orphan list to mapped rows
   (covered 161 → 163 of 580 → 582). `EObjectFlags`/`EInternalObjectFlags`/`EFindName`/
   `PropertyTypes` remain intentionally uncataloged orphans (136 rows).

## Machine-readable companion

Every row above, with identical fields, is in `docs/coverage.json` (rev 3: 582 rows — 106
globals incl. the v4 `Key`/`ModifierKey` entries + 476 type rows; per-entry `covered_by`, `how`,
`reason_if_not`, `evidence`, plus the tally block — 163 covered / 150 invoked / 419 not-covered
— and the 439-call reconciliation: 353 static +
86 live; the live figure counts 72 literal call() sites that always emit exactly one CALL-format
row each — call() or a branch skip() — on every run path).
