# REBASE — DIFF-1 (r3-luamod-funcptr-marshal) onto the composed luamod lineage

worker-luamod-rebase, 2026-10-04. LOCAL ONLY. Originals untouched; everything
staged under `outputs/2026-10-04-zcode-team/worker-luamod-rebase/`. All shas are
sha256; the complete pin list lives in `receipts/sha-manifest.json`.

## 1. The three versions

| role | file | sha256 (first 16) |
|---|---|---|
| **V1 — DIFF-1 base** (port tree) | `native-coherent-runtime-1/source/port/ue4ss/src/Mod/LuaMod.cpp` | `a2d07f7c233a59d0` |
| **V2 — tick-affinity overlay** | `ue4ss-native-tick-affinity-1/src/LuaMod.cpp` | `24e2cf9c91ee9697` |
| **V3 — notify-cancel overlay = THE COMPOSED UNIT'S SOURCE** | `ue4ss-native-notify-cancel-1/src/LuaMod.cpp` | `0f0697a3a5ac9928` |

V3 is the source the composition actually compiles for the `luamod` unit:
attested three ways — `ue4ss-native-notify-cancel-1/plan.json`
`overlay_source_sha256`, the build-1 base receipt
(`bcd23a2a...`, `selected_runtime_units.luamod.source_sha256`), and the on-disk
bytes. The base receipt also pins the unit's 172-token argv, object
`2371839b3d6fe0b4` (6,902,176 B) and `LLVM_CONFIG`.

DIFF-1 itself: `worker-r3-diffs-authoring/diffs/r3-luamod-funcptr-marshal.diff`
= `a8d1d6b4f277fe1a`, two files / 14 hunks (1 on SettingsManager.hpp, 13 on
LuaMod.cpp), authored on V1 + `a9b4abbe26f0cc1c` (SettingsManager.hpp).

## 2. Overlay deltas (V1 → V3), measured

Exactly two small inserts; both patches are the spike dirs' own
`LuaMod.cpp.patch` files:

- **ΔTA (tick-affinity)**, patch `b3539286d159b2c9`: 6 lines inserted at the top
  of `engine_tick_hook`'s body (after `#ifdef __PROSPERO__`, before
  `NativeLuaActionDispatchScope ...`): the per-callback `IsInGameThreadRaw()`
  gate. V1+ΔTA = V2 (`24e2cf9c...`).
- **ΔNC (notify-cancel)**, patch `081f1ba2080fb798`: 4 lines inserted in
  `process_pending_notify_on_new_object_callbacks`'s exec loop
  (`if (callbacks_to_remove.contains(exec.callback_id)) { continue; }`).
  V2+ΔNC = V3 (`0f0697a3...`).

**Collision analysis vs DIFF-1's hunk anchors:** ΔNC (base ~line 4217) sits
between DIFF-1 hunk 2 (@4109) and hunk 3 (@4302) — pure line-offset. ΔTA
(base ~line 4466) sits inside `engine_tick_hook`, which DIFF-1 never touches
(DIFF-1's big hunk 4 region is 4435–4441, the `process_event_hook` tail; its
inserted `funcptr_marshal_hook` borrows the original tail bytes —
`engine_tick_hook` starts at ~4463 and is untouched). **Zero textual overlap.**

## 3. The rebase (V3 → staged variant A content)

Mechanical: `patch -p2` with the luamod-only diff
`diffs/r3-luamod-funcptr-marshal.luamod-only.diff` (`cdec043502c685b0` =
DIFF-1's LuaMod.cpp section byte-identical, header hunks excluded — file header
`a/source/port/...` needs `-p2` in this tree layout).

Result: **`bc082c724776ea97`** (`work/dryrun-B/port/ue4ss/src/Mod/LuaMod.cpp`).
Dry-run clean, apply clean, no fuzz, no rejects (`receipts/patch-B.log`).

### Anchored cross-check (order independence — the 3-way proof)

1. Full DIFF-1 (`a8d1d6b4...`) applied to V1 reproduces the authoring worker's
   anchored patched copy **byte-exactly**: `ce61f02a9557e441`
   (`worker-r3-diffs-authoring/work/patched/LuaMod.cpp`) — `receipts/patch-A.log`.
2. ΔTA then ΔNC applied onto that copy (explicit file-target patch application)
   yields `bc082c724776ea97` again (`work/rebased-crosscheck.cpp`).

Therefore **rebase(DIFF-1, V3) == overlay(DIFF-1(V1))** byte-for-byte. The
staged content is the SAME code regardless of application order; nothing was
hand-merged; every DIFF-1 hunk body survives byte-identical.

## 4. The L10LM formulation edit (variant A content → variant B, PRIMARY)

DIFF-1 hunk 1 appends `FuncPtrMarshal` LAST to
`enum class GameThreadExecutionMethod { ProcessEvent, EngineTick }` in
SettingsManager.hpp `a9b4abbe...` → value **2**. Landing that hunk would stale
`settingsmanager`/`program`/`luamodref` (BLOCKER 3). The delivered formulation
removes the dependency inside the single replaced TU
(`diffs/l10lm-executor-tag-formulation.diff`, `70bcb1ab6ada4631`):

- insert one file-local constant before the `[R3] FuncPtr-marshal state` block:
  `static constexpr auto kFuncPtrMarshalExecutor = static_cast<GameThreadExecutionMethod>(2);`
- replace all **13** qualified references
  `GameThreadExecutionMethod::FuncPtrMarshal` with `kFuncPtrMarshalExecutor`
  (count-checked 13 → 0; the references are: the delayed-action executor filter
  ×2, the drain's `process_delayed_actions<...>` instantiation, the Lua table
  entry, and the nine ladder rung assignments/comparisons). The Lua table KEY
  string `"FuncPtrMarshal"` is a string literal and is untouched (count 1
  before and after). The base file contains **zero** occurrences of the
  qualified name, so the substitution cannot reach non-DIFF-1 code.

Result: **`4540593ac9e66cdc`** =
`staging/luamod-l10rebase/LuaMod.cpp` — the PRIMARY staged deliverable.

### Equivalence proof (variant B vs variant A)

- **Object-level (strongest):** variant A (`bc082c72...` compiled with the
  patched header shadowing the port one) and variant B (`4540593a...` compiled
  over the untouched header) produce the **byte-identical object**
  `5595feb785206c00` under the same 172-token argv. A named appended enumerator
  and a constexpr alias of the same value are indistinguishable to codegen here.
- **Value-level:** the appended enumerator's value would be 2 (appended last
  per hunk 1; `ProcessEvent`=0, `EngineTick`=1 in `a9b4abbe...`); the alias is
  `static_cast<GameThreadExecutionMethod>(2)`. All 13 uses are value-level
  (comparisons, a template non-type argument, `static_cast<int>` for
  `lua_pushinteger`).
- **Surface-level:** the Lua-visible name comes from the string key (unchanged);
  the five ladder throw messages are byte-identical strings.

Variant A files are staged for the record ONLY
(`staging/luamod-l10rebase-variantA-full/`: `bc082c72...` +
`port-ue4ss-include/SettingsManager.hpp` `5b7c50f458bd4c2a`); they are NOT
admitted by the delivered driver and would additionally require the documented
4-unit recompile set (luamod + settingsmanager + program + luamodref).

### Single-step controller diff

`diffs/r3-luamod-funcptr-marshal.overlay-rebase.combined.diff`
(`a06101ec4b434837`) = V3 → variant B in one patch (verified: applies `-p2` to a
fresh byte-verified copy of V3 and reproduces `4540593a...` exactly).

## 5. What the rebase deliberately does NOT do

- Does not touch SettingsManager.hpp, LuaMod.hpp, or any other file: the
  recompiled set is exactly ONE unit (`luamod`).
- Does not renumber or reorder the existing enumerators (values 0/1 unchanged;
  the alias equals the would-be appended value).
- Does not alter DIFF-1's comments, strings, or the arm/disarm contract; hunk
  bodies remain the authoring worker's bytes.
- Does not resolve DIFF-1's link-level inertness (the drain cluster is
  file-static and unreferenced — see REPORT.md §5): that is a DIFF-1 design
  decision belonging to the controller/authoring worker, recorded here as a
  measured fact, not silently changed.
