# UE4SS Lua Static Conformance Suite

**Scope: Mac-only, static-only.** Everything in this directory was authored and
tested on the host Mac. No FTP, no payloads, no launches, no console contact.
The Lua suite itself is intended to be loaded by UE4SS as a regular mod at some
future point (in a game session); it has **not** been executed in a real
UE4SS/game session by this author. All testing reported here is
**Mac-only / host-tested** (plain Lua 5.4 interpreter with a mock UE4SS
environment, plus Python 3.9 for the diff tool).

## Deliverables

| File | Purpose |
|---|---|
| `conformance_static.lua` | UE4SS mod script exercising every game-state-independent Lua API |
| `diff_results.py` | Structured diff of two conformance logs (summary table + JSON) |
| `hosttest_mock_env.lua` | Mac-only host-test harness: mock UE4SS env to run the suite locally |
| `samples/sample_a.log` | Fixture log (mock variant A) |
| `samples/sample_b.log` | Fixture log (mock variant B: value changes, 2 lines removed, 1 line added) |
| `samples/sample_b_full.log` | Fixture log (mock variant B, no line surgery: pure value mismatches) |
| `samples/*.diff.json` | JSON diffs produced by the tested commands below |

## API provenance

The call list was derived by static reading of (tree root
`.superpowers/native-dependency-spike/native-coherent-runtime-1`):

- `source/port/ue4ss/src/Mod/LuaMod.cpp` — global functions (`FString`,
  `FUtf8String`, `FAnsiString` constructors, thread queries, delayed-action
  queries, `IsKeyBindRegistered`, `CreateInvalidObject`), namespaces
  (`UE4SS`, `UnrealVersion`, `FPackageName`), constant tables (`Key`,
  `ModifierKey`, `EObjectFlags`, `EInternalObjectFlags`, `EFindName`,
  `PropertyTypes`, `EGameThreadMethod`, capability booleans)
- `source/port/ue4ss/include/LuaType/LuaUnrealString.hpp` — the full
  FString/FUtf8String/FAnsiString method set
- `source/port/ue4ss/src/LuaType/LuaFName.cpp` — FName constructor
  metamethods, `ToString`, `GetComparisonIndex`, `Equals`, `__eq`, `type`
- `source/port/ue4ss/src/LuaType/LuaFText.cpp` — FText `ToString`/`type`
  (`ToFString` is null-guarded, so the empty global instance is safe to read)
- `source/port/ue4ss/src/LuaType/LuaModRef.cpp` — `ModRef:GetSharedVariable`
  (read-only path)
- `source/port/ue4ss/src/LuaType/LuaThreadId.cpp` — ThreadId `ToString`,
  `__eq`, `type`
- `source/port/ue4ss/include/LuaType/LuaUObject.hpp` — wrapper-level
  `GetAddress`/`IsValid` (safe on the null-backed invalid object)
- Calling conventions verified against the pinned LuaMadeSimple dependency
  (`is_string`/`get_string`/`discard_value` default to stack index 1):
  **dot** call for `FPackageName.*` (impl registers no self-discard) and
  **colon** call for userdata methods and `UnrealVersion:Is*` (impl discards
  the self table).

Note: this source revision exposes **no Lua settings-reader API** (no
`GetSettingsValue`-style registration in `LuaMod.cpp`); the "settings/constant
reader" role is filled by the constant tables and version queries above.

## What is tested (353 calls, grouped by `# SECTION:`)

| Section | Contents |
|---|---|
| `env_info` | `_VERSION`, `jit.version/arch/os` (guarded), `UE4SS.GetVersion` (3 ints), `UnrealVersion.GetMajor/GetMinor/IsEqual/IsAtLeast/IsAtMost/IsBelow/IsAbove` + error path, `EGameThreadMethod` values, `EngineTickAvailable`, `ProcessEventAvailable` |
| `global_presence` | `type()` of ~60 registered globals (side-effecting APIs are presence-checked only, never called) |
| `fname` | ctor (string / integer / find-miss), `ToString`, `GetComparisonIndex` (normalized), `type`, `Equals`, `__eq`, `NAME_None` identity, 3 deterministic error paths |
| `fstring` / `futf8string` / `fansistring` | ctor, `ToString`, `Len`, `IsEmpty`, `Find` (hit/miss/1-based), `StartsWith`, `EndsWith`, `ToUpper`, `ToLower`, `Append` (string / userdata / error), `Empty`, `Clear`, `type`, ctor error paths |
| `ftext` | `type`, `ToString` on the global empty instance |
| `fpackagename` | `IsShortPackageName`, `IsValidLongPackageName` (true/false cases + error path) |
| `modref` | `type`, `GetSharedVariable` of a never-set key (nil) |
| `threads` | `GetCurrentThreadId`/`GetMainModThreadId`/`GetAsyncThreadId`/`GetGameThreadId` (normalized), ThreadId `type`/`ToString`/`__eq`, `IsInMainModThread` (true), `IsInAsyncThread`/`IsInGameThread` (false) |
| `delayed_actions` | `MakeActionHandle` (normalized + uniqueness), read-only queries on a never-allocated handle (`IsValid…`, `Is…Active/Paused`, `TimeRemaining`=-1, `Elapsed`, `Rate`) + error path |
| `keybind_query` | `IsKeyBindRegistered(Key.F6)` (read-only) + out-of-range error path |
| `constants` | `EFindName.*`, `ModifierKey.*`, all 31 `EObjectFlags.*`, all 10 `EInternalObjectFlags.*`, 19 `Key.*` spots + key count, per-property `PropertyTypes.<T>.Name/Size/FFieldClassPointer/StaticPointer` |
| `invalid_object` | `CreateInvalidObject` + `type`/`IsValid` (false)/`GetAddress` (0) — wrapper-level only |
| `misc` | `__OriginalReturnValue` (nil), `ue4ss_loaded_modules`, one `print` smoke call |

## What is deliberately NOT tested (and why)

- **Game-object APIs** — `FindFirstOf`, `FindAllOf`, `StaticFindObject`,
  `FindObject`, `FindObjects`, `LoadAsset`, `StaticConstructObject`,
  `ForEachUObject`, `NotifyOnNewObject`, and all `UObject`/`UClass`/`UWorld`/
  `TArray`/`TMap`/`TSet` instance methods (results depend on live game
  state; TArray/TMap/TSet have no standalone constructor).
- **Persistent side effects** — `RegisterHook`, `RegisterKeyBind(/Async)`,
  `RegisterCustomProperty`, `RegisterCustomEvent`, all
  `Register*Pre/PostHook`, `RegisterConsoleCommand*`, `NotifyOnNewObject`,
  `ModRef:SetSharedVariable` (persists across mods in the session).
- **Scheduling/execution** — `ExecuteAsync`, `ExecuteWithDelay`, `LoopAsync`,
  all `ExecuteInGameThread*`/`Loop*`/delayed-action setters/cancellers.
- **Destructive/heavy** — `RestartMod`, `RestartCurrentMod`, `UninstallMod`,
  `UninstallCurrentMod`, `DumpAllObjects`, `GenerateSDK`, `GenerateLuaTypes`,
  `GenerateUHTCompatibleHeaders`, `DumpStaticMeshes`, `DumpAllActors`,
  `DumpUSMAP`, `DumpJMAP`, `IterateGameDirectories` (recursive walk of the
  game Content tree: heavy I/O, install-dependent output).
- **Noisy** — `LoadExport` call (emits a `[Fatal]` line; presence-checked
  only).

Accepted, harmless, non-persistent side effects: constructing FName strings
adds entries to the process name pool (uniquely prefixed
`ConformanceStatic*`); `MakeActionHandle` bumps a monotonic counter;
`Empty`/`Clear`/`Append` mutate only locally created string objects.

## Log format

One record per call (tab-separated, machine-parseable):

```
CALL<TAB>api_name<TAB>params_summary<TAB>result_type<TAB>result_value_or_error<TAB>ok_or_fail
```

- Section markers: `# SECTION: category_name` (all other `#` lines are
  comments; the suite also prints a `# SUMMARY: total=… ok=… fail=…` footer).
- Every call is wrapped in `pcall`; a raised error is recorded as
  `result_type=error` with the sanitized (tab/newline-escaped, 300-char)
  message in the value column — one failure never stops the suite.
- `ok_or_fail`: `ok` = call succeeded **and** its deterministic expectation
  (when one exists) held; `fail` = call raised **or** expectation not met.
  Error-path rows invert this: `ok` when the call *did* raise as expected.
- **Normalization** (keeps logs diffable across runs/builds): thread ids,
  addresses, dynamic FName indices, action handles and pointer hashes are
  rendered as stable markers (`int>0`, `int!=0`, `int>=0`, `userdata<Name>`,
  `table(n=…)`). Deterministic values are logged raw.
- Strings are logged quoted; numeric result types are labeled
  `integer`/`float`.

## Deploying the suite

1. Copy `conformance_static.lua` to `<UE4SS>/Mods/ConformanceStatic/scripts/main.lua`.
2. Create `<UE4SS>/Mods/ConformanceStatic/enabled.txt` (empty file).
3. Launch the game with UE4SS attached (future work — not done here).
4. Extract `grep $'^CALL\t' UE4SS.log > run_<label>.log` and diff runs with
   `diff_results.py`.

## Diff tool

```
python3 diff_results.py LOG_A LOG_B [--json OUT.json] [--verbose] [--no-json]
```

- **MATCH** — same call key in both logs, identical `params`, `result_type`,
  `value`, and `ok` fields.
- **MISMATCH** — same key, differing fields (listed per-row in verbose/JSON).
- **MISSING** — key present in A (baseline) only.
- **EXTRA** — key present in B only.
- Call key = `(api_name, occurrence_index)` — the Nth call to that api within
  a log. The suite emits calls in fixed order, so indexes align across runs.
  Caveat: if a line is deleted mid-log, later same-api occurrences shift and
  surface as a paired MISMATCH+MISSING rather than a lone MISSING.
- Output: summary table (totals + per-section breakdown) on stdout; JSON
  diff (inputs with sha256, summary, per-section, full detail list) written
  to `--json` path or `<a>_vs_<b>.diff.json` by default; `--verbose` prints
  every call line with its status.
- Exit codes: `0` logs equivalent; `1` differences found; `2` usage/parse
  error.

## Host test results (Mac-only, this machine)

Interpreter: the repo no longer bundles one — install Lua on the host with
`brew install lua` (host-side Lua is used only for syntax-checking and mock
runs; nothing here touches the console). The recorded results below were
produced with the formerly bundled arm64 `Lua 5.4.7` binary
(`bin/lua`, removed from the repo 2026-10-07). Python 3.9.6.

**Mac-only / host-tested after the removal:** both mock variants were
re-verified with the system Homebrew interpreter (`lua` = Lua 5.5.1) —
identical totals to the 5.4.7 runs (variant A 353/353 ok; variant B 352 ok,
1 expected fail).

```
lua hosttest_mock_env.lua conformance_static.lua            # variant A
MOCK_VARIANT=b lua hosttest_mock_env.lua conformance_static.lua   # variant B
```

- Suite runs to completion: **353 calls, 0 unexplained failures** under the
  mock (variant A: 353/353 ok). Two genuine suite bugs were found and fixed
  by this host test (double-wrapped structured record on `UE4SS.GetVersion`;
  wrong `:ToLower` round-trip expectation) plus one mock gap (`Key.F6`).
- Diff tool verified:
  - `sample_a.log` vs itself → `353 MATCH`, verdict EQUIVALENT, exit `0`.
  - `sample_a.log` vs `sample_b_full.log` (value-only changes) →
    `3 MISMATCH` (`UE4SS.GetVersion`, `FText:ToString` incl. ok-column
    change, `PropertyTypes.BoolProperty.Size`), exit `1`.
  - `sample_a.log` vs `sample_b.log` (changes + 2 deletions + 1 addition) →
    `347 MATCH, 4 MISMATCH, 2 MISSING, 1 EXTRA`, exit `1`; per-section
    table flags env_info/fname/ftext/constants/misc.
  - `--verbose` prints every line with both sides and differing fields.
  - JSON output validated with `json.load` (top-level
    `tool/tool_version/inputs/summary/details`; sha256 of both inputs).
  - Nonexistent input file → stderr message, exit `2`.

## Caveats

- The mock environment approximates the C++ semantics read from source; it
  is **not** the engine. Mock-generated fixture logs contain mock values
  (e.g. `table(n=…)` where real UE4SS yields `userdata<…>`) and exist to
  test the harness and diff tool, not to state engine behavior.
- `IsKeyBindRegistered(Key.F6)` is only *practically* deterministic (false
  unless some other mod bound F6 in the same session).
- `FName("…", FNAME_Find)` find-miss behavior is recorded without an
  expectation (UE semantics yield `None`; the row captures actual behavior).
- The suite targets Lua 5.1/LuaJIT/5.4 syntax compatibility (no `//`,
  `goto`, or bitwise operators).
