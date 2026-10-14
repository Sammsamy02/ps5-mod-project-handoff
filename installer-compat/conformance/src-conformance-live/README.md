# UE4SS Live-Game API Conformance — `conformance_live.lua`

## Provenance / scope (Mac-only, host-tested)

- Authored and staged entirely on the Mac host in `/tmp/sideproject/conformance-live/`.
- **No FTP, no payloads, no game launches, no console contact.** Nothing under
  `launch11-combined/units`, `outputs/2026-10-07-l1*`, or the mod manager branch
  was read-modified or written. No file outside `/tmp/sideproject/conformance-live/`
  was touched (a local `brew install lua` was used only for host-side validation).
- Host verification performed (see "Host verification" below): `luac -p` parse
  check, plus two mock-harness runs in plain local Lua stubbing the UE4SS
  environment (no game). The script itself has **not** been run against any game
  from this Mac.
- **Rev 2 (2026-10-07, Mac-only / host-tested):** fixed against the port source
  (`native-coherent-runtime-1/source/port/ue4ss`, cross-checked against the
  upstream citation tree): TArray battery rewritten to the real members (no
  Lua-side TArray constructor exists), the fictional `Loop` table replaced with
  the real scheduling globals, `UClass:GetFunction` replaced with
  `ForEachFunction`/`GetSuperStruct`/`IsChildOf`/`GetCDO`, FString/FText rows
  moved from nonexistent `__tostring`/`__concat` to `:ToString()`/`:Append()`,
  hook validators fixed (Register* return no value) and RegisterHook/UnregisterHook
  arity fixed (two ids; unregister takes path+pre+post), plus gated no-op calls
  for every previously-untested hooking global and self-owned
  scheduling/cancel cycles. Catalog + coverage updates:
  `catalog/globals.json` (104 entries), `catalog/types.json` (TArray/UClass
  notes), `docs/COVERAGE-MAP.md`, `docs/coverage.json`.

## Files

| File | Purpose |
|---|---|
| `conformance_live.lua` | The live-game conformance script (drop-in UE4SS mod script) |
| `README.md` | This document |

## Installation (on the target machine — manual, NOT performed from this Mac)

1. Copy `conformance_live.lua` to `<Game>/Mods/conformance-live/Scripts/main.lua`
   (standard UE4SS mod layout).
2. Enable the mod: add the line `conformance-live : 1` to `<Game>/Mods/mods.txt`.
3. Log output: every row is `print()`ed to the UE4SS console/`UE4SS.log`, and —
   when `io` is available — written to `conformance_live.log`, resolved as:
   1. `$CONFORMANCE_LIVE_LOG_DIR/conformance_live.log`
   2. `./conformance_live.log` (game process working directory)

## When it runs (startup delay and in-game gating)

The script never tests on the main menu and never tests before the game is
initialized:

1. **Startup delay**: 10 s after mod load (the global `ExecuteWithDelay`; falls back to
   `ExecuteInGameThread` tick counting at ~60 fps if it is unavailable). Rev 2 note: there
   is no `Loop` table in this UE4SS port — scheduling is registered as plain globals.
2. **In-game poll**: every 2 s, checks `FindFirstOf("Pawn")` and
   `FindFirstOf("PlayerController")`. Waits up to 240 s.
3. Once both exist: 5 s post-join grace, then the whole battery runs inside
   `ExecuteInGameThread` (game thread).
4. **Timeout (DEGRADED mode)**: if the player is still not in-game after 240 s,
   the battery still runs — it is read-only and safe — and every
   game-state-dependent test emits SKIP. The report header shows
   `mode: DEGRADED`.
5. Manual re-trigger for developers: global function `ConformanceLiveRun()`
   (no-op if the battery already ran).

## Exact prerequisite list (logged as PREQ rows at the top of every report)

| # | PREQ name | Condition | Gates |
|---|---|---|---|
| 1 | `lua_env` | `pcall` + `print` available | everything (always MET if the script is running) |
| 2 | `game_thread_access` | `ExecuteInGameThread` callable | battery scheduling, fallback timing |
| 3 | `engine_instance` | `FindFirstOf("Engine") ~= nil` | Engine-instance object-system + reflection rows |
| 4 | `pawn_instance` | `FindFirstOf("Pawn") ~= nil` | Pawn rows (player in-game) |
| 5 | `player_controller_instance` | `FindFirstOf("PlayerController") ~= nil` | PlayerController rows |
| 6 | `in_game` | PREQ 4 AND PREQ 5 | the in-game gate itself (mode IN-GAME vs DEGRADED) |

## Expected skip conditions

| Test rows | Skip reason (result column) | Condition |
|---|---|---|
| `FindFirstOf/FindAllOf class=Engine`, `UObject.GetFullName/GetAddress/IsValid/GetClass/GetOuter/IsA(userdata)` on Engine | `prerequisite not met: FindFirstOf('Engine') returned nil` | no Engine instance (extreme) |
| `FindFirstOf/FindAllOf class=Pawn`, `UObject.IsA(string)` x2 on Pawn | `prerequisite not met: FindFirstOf('Pawn') returned nil (main menu or not in-game)` | main menu / degraded run |
| `FindFirstOf class=PlayerController` | `prerequisite not met: FindFirstOf('PlayerController') returned nil (main menu or not in-game)` | main menu / degraded run |
| Property reads on Pawn (`Owner`) / PlayerController (`AcknowledgedPawn`) | same as above | same as above |
| `FName.ToString`, `FName.Equals`, `FName.__eq(==)` | `dependency unavailable: FName construction errored or returned nil` | `FName(...)` constructor absent |
| `FString.tostring`, `FString.__concat` | `dependency unavailable: FString construction errored or returned nil` | `FString(...)` constructor absent |
| `FText.tostring` | `dependency unavailable: FText construction errored or returned nil` | `FText(...)` constructor absent |
| `TArray.__index(element-read)` / `TArray.ForEach` | `array empty (#arr=0); an OOB read would AddZeroed-grow live game memory` | length-guarded reads (rev 2) |
| `TArray.__newindex(element-write)` / `TArray.Empty` | `excluded by the read-only contract: no in-memory TArray constructor exists in this port, so a write/clear mutates live game memory` | permanent exclusions (rev 2) |
| `UnregisterHook` | `dependency unavailable: RegisterHook returned no id` | hook id not returned |

## Log format

Fixed format, one row per call, tab-separated (identical shape to the static
conformance script):

```
CALL<TAB>api_name<TAB>params<TAB>result_type<TAB>result<TAB>status
```

- `result_type`: Lua type of the first return value (`nil`, `boolean`, `number`,
  `string`, `table`, `userdata`, `function`, `none`) or `error` / `skip`.
- `result`: sanitized string of the return value (control chars replaced by
  spaces so columns never break; truncated to 200 chars; tables render as
  `table(count=N)`; `FindAllOf` rows therefore show the instance count).
- Status semantics:
  - `PASS` — call returned without a Lua error and the (optional) validator
    accepted the result.
  - `FAIL` — the call raised an error (including "API not present", e.g.
    `attempt to call a nil value (global 'FName')`), or a validator rejected
    the result. This is conformance data, not game damage.
  - `SKIP` — a prerequisite/dependency was not met (never a failure).
- Metadata rows: `PREQ<TAB>name<TAB>expected<TAB>state<TAB>actual<TAB>MET|NOT_MET`.
  Comment lines start with `#` (headers, mode, summary). A summary block at the
  end reports totals and the timing mechanism used.

## Safety guarantees

- **Read-only**: no spawning, no destroying, no property writes, no
  `ProcessEvent` calls, no asset loads, no game-state modification of any kind.
- **Registrations are no-ops and never deliberately triggered**:
  keybind `LEFT_CONTROL+LEFT_SHIFT+F8`, console command `conformance.noop` and
  `conformance.noop.global`, custom event `ConformanceNoopEvent` (unregistered
  again via `UnregisterCustomEvent`), `NotifyOnNewObject` for a class name that
  never instantiates, and the full set of lifecycle/exec pre+post hooks — all
  with empty callbacks.
- `RegisterHook` uses a pass-through no-op pre-callback (returns nothing, so the
  original function executes unchanged even if the game calls it mid-test) and
  is **unregistered immediately** via `UnregisterHook(path, pre_id, post_id)`
  with both ids the port returns.
- Every call is wrapped in `pcall`; a FAIL records the error string and cannot
  crash the game. The whole bootstrap is also pcall-guarded.
- Rev 2: there is NO Lua-callable TArray constructor in this port; the battery discovers a real
  array property via `ForEachProperty` on UClass(Actor) and reads it from the Pawn, then exercises
  the real members (`GetArrayNum`, `#a`, in-bounds `a[1]`, `GetArrayMax`, `IsValid`,
  `GetArrayAddress`, immediate-stop `ForEach`) read-only.
- Left registered for the remainder of the session (harmless no-ops, listed
  above); UE4SS exposes no unregister API for keybinds/console/custom events.

## Coverage map (categories in report order)

| Category | APIs exercised |
|---|---|
| Object System | `FindFirstOf` (Engine/Pawn/PlayerController), `FindAllOf` (Engine/Pawn), `StaticFindObject` (UClass, UFunction, negative probe), `UObject:GetFullName`, `GetAddress`, `IsValid`, `GetClass`, `GetOuter`, `IsA` (userdata + string forms) |
| Reflection | property read (`Engine.bUseFixedFrameRate`, `Pawn.Owner`, `PlayerController.AcknowledgedPawn`), `UObject:GetPropertyValue`, `UClass.ForEachFunction/GetSuperStruct/IsChildOf/GetCDO` (rev 2: the old `UClass:GetFunction` name does not exist in this port) |
| Names | `FName(...)` creation, `string.toFName` probe, `FName:ToString`, `FName:Equals`, `FName ==` |
| Strings | `FString(...)` creation, `:ToString`, `:Len`, `:Append` (in-memory copy) + a pure-Lua concat control row, `FText(...)` creation + `:ToString` (rev 2: the string types register NO metamethods in this port, so `tostring()`/`..` rows were wrong and removed) |
| Containers | live array view via `ForEachProperty` discovery + `UObject.__index` array-property read; `GetArrayNum`, `#a` (`__len`), in-bounds `a[1]` (`__index`), `GetArrayMax`, `IsValid`, `GetArrayAddress`, immediate-stop `ForEach` |
| Input | `RegisterKeyBind` (registered, never triggered), `IsKeyBindRegistered` |
| Threading | `ExecuteInGameThread` (queued no-op + deferred execution flag verification) |
| Hooks (rev 2: every cataloged hooking global now has a gated call) | `RegisterLoadMapPostHook`/`RegisterLoadMapPreHook`, `RegisterInitGameStatePre/PostHook`, `RegisterBeginPlayPre/PostHook`, `RegisterEndPlayPre/PostHook`, `RegisterProcessConsoleExecPre/PostHook`, `RegisterCallFunctionByNameWithArgumentsPre/PostHook`, `RegisterULocalPlayerExecPre/PostHook`, `RegisterConsoleCommandHandler`, `RegisterConsoleCommandGlobalHandler`, `RegisterCustomEvent` + `UnregisterCustomEvent`, `NotifyOnNewObject` (never-spawning class), `RegisterHook` (pass-through no-op) + `UnregisterHook` |
| Scheduling (rev 2) | `ExecuteWithDelay` (+ deferred flag verification), and self-owned register/cancel cycles: `ExecuteInGameThreadWithDelay`, `RetriggerableExecuteInGameThreadWithDelay`, `LoopInGameThreadWithDelay`, `LoopInGameThreadAfterFrames` each paired with `CancelDelayedAction`; `MakeActionHandle`. `LoopAsync` is intentionally NOT called (perpetual repeating callback, no handle, no cancellation API — documented in a SKIP row) |

## Interpretation notes

- Mainline UE4SS bridges Lua string <-> FString and may not expose
  `FName`/`FString`/`FText`/`TArray` constructors, `IsKeyBindRegistered`, or
  `UObject:GetPropertyValue`. Absence is recorded as a FAIL row **by design** —
  that is the conformance signal — and dependent rows SKIP rather than cascade.
- A property-read row may legitimately return `nil` (null value); "property
  missing" vs "null value" are not distinguishable without a class dump, so
  no-error counts as PASS.
- `IsA` is tested with both parameter forms (string class name and UClass
  userdata); a build supporting only one form will show it.
- Rev 2: `TArray` element indexing is 1-based per the port source; out-of-bounds
  reads would AddZeroed-grow live game memory, so reads are length-guarded and
  only in-bounds `arr[1]` is probed. `string.toFName` is a labeled legacy-API
  absence probe (not in the port source; FAIL is the expected informative result).
- UE4SS object results are `userdata`; row result strings will show
  `userdata: 0x...` addresses.

## Host verification (Mac-only, no game)

- `luac -p` (Lua 5.5.1 via Homebrew): syntax OK, re-checked after every edit.
  The script uses only Lua 5.4-compatible constructs (`table.pack`/`table.unpack`).
- Rev-2 host re-verification (2026-10-07): `tests/src-tester/lua_syntax.py`
  (the repo's Lua 5.3 parser, the designated `lua53 -p` equivalent) passes, and a
  fresh full-battery mock run (all APIs stubbed, Pawn+PC present) emits exactly
  **86 CALL rows / 6 PREQ rows: 82 PASS / 1 FAIL / 3 SKIP** (the FAIL is the
  intentional `string.toFName` absence probe; the SKIPs are LoopAsync
  unsafe-by-design plus the two TArray read-only exclusions), all rows exactly
  6 tab-separated fields. Supersedes the rev-1 mock figures below, which are
  retained for history:
- Mock harness 2 (constructors absent, no Pawn/PC so the run degrades):
  **27 PASS / 7 FAIL / 18 SKIP** — the 7 FAILs are exactly the absent-API probes
  (FName.new, string.toFName, FString.new, FText.new, TArray.new x2,
  IsKeyBindRegistered), and all game-state rows SKIP with the documented reasons.
- Scratch harness outputs were removed after verification; only the two
  deliverables remain in this directory.
