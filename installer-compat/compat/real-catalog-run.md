# Catalog attribution verification + compat checker run on the REAL catalog

**MAC-ONLY / HOST-TESTED** (2026-10-07). Static source analysis on the macOS host.
No FTP, no payloads, no launches, no console contact, no Lua execution.
All work confined to `/WORKSPACE/Projects/ps5-installer-compat/` plus read-only
inspection of the source trees under
`/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/`.

Trees used for attribution (both read-only):

| Role | Path | Version |
|---|---|---|
| PORT (source root the catalog agents read) | `native-coherent-runtime-1/source/port/ue4ss/` | PS5/Prospero port |
| VENDORED UPSTREAM (build dependency baseline) | `ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/` | `version.cache` = 3.0.1.0.0 (UE4SS 3.0.1) |
| UPSTREAM clone (cross-check) | `ue4ss-binding-research-1/upstream/RE-UE4SS/UE4SS/` | upstream |

---

## Part 1 — Catalog source verification

### 1a/1b. "FName → FNameUserdata" is NOT a port rename — it is upstream's own convention

The type-catalog agent's claim ("the port renames FName → FNameUserdata", recorded in
`catalog/src-catalog-types/SUMMARY.md`) is **false**. The vendored upstream UE4SS 3.0.1
tree contains the identical metatable-name literals at the identical line numbers; the
port kept upstream naming unchanged. Byte-level evidence (`auto metatable_name = "..."`):

| Metatable name | Port file:line | Upstream 3.0.1 file:line | Verdict |
|---|---|---|---|
| `FNameUserdata` | `src/LuaType/LuaFName.cpp:17` | `src/LuaType/LuaFName.cpp:17` | identical — UPSTREAM convention |
| `FSoftObjectPathUserdata` | `LuaFSoftObjectPath.cpp:17,39` | `LuaFSoftObjectPath.cpp:17,39` | identical — upstream |
| `TSoftObjectPtrUserdata` | `LuaTSoftObjectPtr.cpp:17,40` | `LuaTSoftObjectPtr.cpp:17,40` | identical — upstream |
| `FURLUserdata` | `LuaFURL.cpp:13` | `LuaFURL.cpp:13` | identical — upstream |
| `ThreadIdUserdata` | `LuaThreadId.cpp:18` | `LuaThreadId.cpp:18` | identical — upstream |

The only port-vs-upstream diff in `LuaFName.cpp`/`LuaFSoftObjectPath.cpp` is an include
tweak (`DynamicOutput/Output.hpp` vs `DynamicOutput/DynamicOutput.hpp`) — unrelated to
naming. Other `*Userdata`-style names in the catalog (`UClass`, `Property`,
`ObjectProperty`, `RemoteUnrealParam`, ...) come from `ClassName::ToString()` structs and
are likewise upstream. The false claim has been corrected in
`catalog/src-catalog-types/SUMMARY.md` (marked CORRECTION 2026-10-07).

### 1c. Every file:line reference verified — all point at the PORT; attribution notes added

Programmatic verification of every reference (Python, read-only):

| Catalog | Entries | file:line refs checked | resolve in port tree | also exist in vendored upstream |
|---|---|---|---|---|
| `catalog/types.json` | 42 | 523 | 523/523 (100%) | 523/523 paths exist (content mostly identical; `LuaUObject.hpp`/`LuaUnrealString.hpp` are port-overridden) |
| `catalog/globals.json` | 98 | 238 | 238/238 (100%) | 238/238 paths exist |

- All line numbers are in-range; precise per-binding line claims spot-checked correct
  (e.g. `LuaMod.cpp:1521` = `register_function("print", ...)`, `:1522` = LoadExport,
  `:2032` UnregisterHook, `:2567` ForEachUObject, `:2933` IterateGameDirectories,
  `:3618` LoadAsset, `:6113/6184/6286` make_global UE4SS/UnrealVersion/FPackageName,
  `:6190-6193` FName/NAME_None preconstructed globals, `LuaFName.cpp:119/127/135`
  ToString/GetComparisonIndex/Equals, `LuaUStruct.cpp:57` GetSuperStruct,
  `LuaUObject.cpp:2432` RemoteUnrealParam:Get).
- Registration-site refs are function-level (e.g. `LuaMod.cpp:1521` = start of
  `setup_lua_global_functions_internal`), which is why they do not contain each
  registered name on the cited line itself — per-name binding lines are cited in
  `notes` where they matter and were the ones spot-checked above.
- **Conclusion: every reference points at the PORT tree** (the port tree is the catalog
  agents' documented source root; a few cited headers are port-modified copies of
  upstream files). Per the task rule, every entry now carries the note
  **"Source attribution: verified against port; upstream reference: TBD"** — 42/42
  type entries and 98/98 global entries (verified post-edit). Port findings are no
  longer presented as upstream facts.

### 1d. ps5_runtime_status is "" everywhere

Verified programmatically, pre- and post-edit: all 42 type entries and all 98 global
entries have `ps5_runtime_status: ""`. Methods inside types.json carry no own status
field (they inherit their type's status — preserved as-is). No pre-filled status
anywhere.

### 1e. Port-specific info moved out of wrong fields (Python json read/write, no manual text edits)

`pc_only_behavior` is a PC-behavior field; port/PS5/Prospero behavior was mixed into it
in 12 `globals.json` entries. Moved to `notes` under a
"Port/PS5-specific (moved from pc_only_behavior):" prefix, leaving only PC behavior
behind:

CreateLogicModsDirectory, DerefToInt32, ExecuteInGameThread,
ExecuteInGameThreadWithDelay, IsKeyBindRegistered, LoadExport, RegisterKeyBind,
RegisterKeyBindAsync, dofile, loadexport, loadfile, print.

`dofile`/`loadfile` remain honestly labeled port additions in their `ue4ss_version`
field (that field's purpose). types.json `pc_only_behavior_notes` fields were audited —
no port-specific content found there (port details already live in `notes`).
The two real catalog files were rewritten via `json.load`/`json.dump` only.

---

## Part 2 — Compat checker on the REAL catalog

### 2a. Merge

The checker takes one catalog JSON; the real catalog is split. `compat/merge_catalogs.py`
(written during this session by a parallel worker on the same task; reviewed and kept)
merges read-only: `catalog/globals.json` (98 entries passthrough) + `catalog/types.json`
(42 types flattened + 456 method entries; methods inherit their type's status;
metamethods skipped; FString/FUtf8String/FAnsiString constructor globals fused with
their type entries; intra-type override dupes collapsed) → `compat/catalog-real-merged.json`
(591 entries, 591/591 with EMPTY ps5_runtime_status). **Re-run after the Part-1
attribution edits** so the merged file carries the new notes.

### 2b. Empty ps5_runtime_status handling (fixed; reconciled with the parallel worker)

Old behavior: empty status fell into the generic `unknown-status` bucket → treated as
behaves-differently → mis-graded real-catalog runs `partly-compatible`, plus a giant
"unrecognized status" note listing all blank entries. It did not crash, but it treated a
blank as a filled status.

Current behavior (in `compat/checker.py`, covered by `TestBlankRuntimeStatus` in
`compat/test_checker.py`): blank/missing/whitespace `ps5_runtime_status` gets a
dedicated **`status-blank`** bucket → per-call output "unknown - runtime status not
filled", grade **`unknown`** with reason "runtime status not filled (...)", and a
Catalog-side suggestion (fill the status from real evidence, then re-run). A genuinely
`missing` API still outranks blanks (partly-compatible/unsupported are not masked).

One further gap found and fixed during this run: `print`/`dofile`/`loadfile` calls were
skipped as "Lua builtins" even when the catalog registers them (UE4SS replaces `print`;
the port replaces `dofile`/`loadfile`). Classification now checks the catalog FIRST —
cataloged stdlib overrides are graded against the catalog (blank status included);
names not in the catalog keep the builtin assumption. Regression test added
(`test_cataloged_stdlib_override_beats_builtin_assumption`). Full suite: **43/43 pass**
(`python3 compat/test_checker.py`).

### 2c. Sample catalog labeled as TEST FIXTURE

`compat/sample_catalog.json` → renamed **`compat/sample_catalog_TEST_FIXTURE.json`**
and wrapped as `{"catalog_version": "TEST FIXTURE ONLY ...", "_comment": "...",
"entries": [...]}` stating the statuses are INVENTED for bucket testing and that real
data is `catalog-real-merged.json`. `compat/test_checker.py` updated accordingly.
(The frozen provenance copy under `compat/src-compat-checker/` was left untouched.)

### 2d. Real-catalog run on the test mod

Command (host only):

```
python3 compat/checker.py compat/sample_mod.lua compat/catalog-real-merged.json \
        --json compat/sample-mod-real-catalog-report.json
```

Result (full machine-readable report at `compat/sample-mod-real-catalog-report.json`;
human summary shown above in the terminal capture): **GRADE: UNKNOWN — "unknown -
runtime status not filled
(16 cataloged API(s) this mod calls have an empty ps5_runtime_status) and 9 API(s)
not in the catalog at all"** — exactly the expected outcome: every cataloged call is
"runtime status not filled" because the status is blank, not because the API is
unknown.

- Calls: total 32 | catalog-matched 17 | unmatched 15 (unknown 10, unresolved 5) | lua-builtins 5.
- STATUS BLANK (16 APIs, 17 call sites — FindFirstOf twice):
  FindFirstOf, FindAllOf, FName, FName:ToString, FString, FString:Len,
  UObject:IsValid, UObject:GetAddress, AActor:IsValid (via base chain from
  APlayerController), ExecuteInGameThread, ExecuteAsync, RegisterHook,
  RegisterKeyBind, NotifyOnNewObject, RegisterConsoleCommandHandler, **print**
  (print now correctly surfaces because the real catalog registers it — the fix in 2b).
- UNKNOWN — genuinely absent from the REAL catalog (9): ImGui.Begin / ImGui.Text /
  ImGui.SetNextWindowSize, EInputEvent.IE_Pressed, GetPlayerController,
  GetUnrealVersion, Loop (the real API is LoopAsync — "Loop" exists only in the test
  fixture), ModConfig_Load, ModServices.Manager. These are fixture-mod inventions or
  APIs this port really does not register; under the real catalog they are unknowns,
  which also forces the unknown grade.
- UNRESOLVED (5, do not affect grade): untyped-param / game-BP methods
  (`obj:IsValid`, `obj:GetFullName`, `o:IsValid`, `AFairyItemBase:GetActorLocation`,
  `AFairyItemBase:K2_SetActorLocation`).
- Lua builtins excluded (5): string.format, pairs, tostring, table.insert, math.floor.

A second real-catalog run against the actual TownfallItemSpawner mod was produced in
parallel and is documented in `compat/townfall-item-spawner-grade.md` (same unknown /
runtime-status-not-filled verdict).

---

## Files changed

| File | Change |
|---|---|
| `catalog/globals.json` | attribution note on all 98 entries; port-specific text moved out of `pc_only_behavior` into `notes` (12 entries); statuses verified `""` (Python json rewrite) |
| `catalog/types.json` | attribution note on all 42 entries; statuses verified `""` (Python json rewrite) |
| `catalog/src-catalog-types/SUMMARY.md` | false "port renames FName → FNameUserdata" claim corrected with upstream evidence |
| `compat/checker.py` | (parallel worker) status-blank bucket + unknown grade for empty ps5_runtime_status; (this pass) cataloged stdlib overrides (print/dofile/loadfile) graded against the catalog instead of skipped as builtins |
| `compat/test_checker.py` | TestBlankRuntimeStatus suite (parallel worker) + stdlib-override regression test; fixture path updated |
| `compat/sample_catalog_TEST_FIXTURE.json` | renamed from `sample_catalog.json` + TEST-FIXTURE comment wrapper |
| `compat/catalog-real-merged.json` | regenerated from the edited real catalogs (591 entries, all blank) |
| `compat/sample-mod-real-catalog-report.json` | checker JSON report of the 2d run |
| `compat/real-catalog-run.md` | this report |

## Reproduce (Mac host only)

```
python3 compat/merge_catalogs.py                                  # rebuild real merged catalog
python3 compat/test_checker.py                                    # 43 tests
python3 compat/checker.py compat/sample_mod.lua compat/catalog-real-merged.json --json /tmp/out.json
```
