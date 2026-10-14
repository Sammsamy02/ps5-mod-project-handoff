# TownfallItemSpawner Compatibility Grade

**MAC-ONLY / HOST-TESTED** — static Lua analysis on the macOS host.
No FTP, no payloads, no launches, no console contact, no Lua execution.
The runtime and the mod manager branch (`origin/claude/townfall-item-spawner-hyj9eg`)
were read, never written.

Machine-readable result: `compat/townfall-item-spawner-grade.json`
(contains the FULL checker scan JSON, not a summary).

## Verdict

```
GRADE:   unknown
REASON:  unknown - runtime status not filled (9 cataloged API(s) this mod
         calls have an empty ps5_runtime_status); KNOWN PS5 DIFFERENCE:
         PC-only input dependency - keyboard keybinds (Key.DOWN_ARROW,
         Key.F7, Key.LEFT_ARROW ... +3 more) need runtime/config
         controller mapping on PS5 (PS5 has no keyboard; see PS5Pad.ini)
```

**Why unknown (round-5 grade policy).** Claude's round-5 review reversed the
round-4 policy: when ANY API the mod calls resolves to a catalog entry with
a blank `ps5_runtime_status`, NO verdict is possible — the grade is
`unknown - runtime status not filled`. The mod's 9 core runtime APIs
(`RegisterKeyBind` itself, `ExecuteInGameThread`, `StaticFindObject`, ...)
are all still blank (599/599 merged catalog entries), so no compatibility
claim — not even "partly-compatible" — can be made for the rest of the mod.

**The keyboard difference stays, as a KNOWN PS5 DIFFERENCE inside the
result.** The six `Key.*` keybind constants (`F7` menu,
`UP/DOWN/LEFT/RIGHT_ARROW` navigation, `RETURN` confirm) remain cataloged
(`globals.json` v4) and detected in the dedicated `pc_only_input` bucket,
and the report carries them in a `known_ps5_differences` section WITHIN the
unknown result: the constants exist and are readable (pure Windows VK
integers, `needs_live_game=false`), but the PS5 has no keyboard — every one
of these keybinds requires runtime/config-side controller mapping (current
approach: `PS5Pad.ini` chords, e.g. L3+R3 = F7). A confirmed difference is
information the user gets; it does not upgrade the grade while the mod's
core APIs are unverifiable.

**Grade history (documented, not hidden):**
1. `unknown - runtime status not filled` (first grade: all statuses blank,
   6 `Key.*` constants "not in catalog").
2. `partly-compatible - PC-only input dependency` (round 4: the
   Key/ModifierKey catalog entries turned the six constants into a graded
   PC-only input category, which then outranked unverifiability).
3. `unknown - runtime status not filled` (round 5, current: any blank
   status among called APIs forces unknown; the keyboard difference rides
   along as a known PS5 difference, never a separate grade).

**Caveat.** The 9 APIs the mod calls whose `ps5_runtime_status` is still
empty remain unverifiable and are listed under status blank. This grade is
not a pass and not a fail for those APIs — filling the catalog is what
would enable a real verdict.

## What was graded

| Item | Value |
|---|---|
| Mod | TownfallItemSpawner v1.0 ("Clean Nexus release build for UE4SS", F7 menu) |
| Mod source | `git show` (read-only) from worktree `/WORKSPACE/.codex/worktrees/translation-core/what`, branch `origin/claude/townfall-item-spawner-hyj9eg` @ `2e1ef8be267c4bf811f83f2da168cd52e6b0e16b` |
| Canonical path | `outputs/2026-10-04-zcode-team/worker-loadlist-final/final-package/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` |
| Identity | 25 of 26 branch copies byte-identical, sha256 `f586043a1ca57081…b6559658`, 746 lines; the one divergent copy is the intentionally corrupted `root-badsize` host-test fixture (excluded) |
| Staged copy | `compat/townfall-item-spawner-mod/Scripts/main.lua` |
| Out of scope | `Mods/shared/UEHelpers/UEHelpers.lua` ships outside the mod dir; the mod loads it defensively (`pcall(require, "UEHelpers")` + `FindFirstOf` fallback). Not part of this grade. |

## Catalog used (real, combined, v4)

The checker takes one catalog file, so the two real files are merged by
`compat/merge_catalogs.py` into `compat/catalog-real-merged.json`
(599 entries; sources untouched):

- `catalog/globals.json` — **106 entries** (v4: 104 through v3 + the new
  `Key` and `ModifierKey` TYPE entries; 3 string-ctor globals fuse into their
  types → 103 in the merge).
- `catalog/types.json` — 42 types flattened to 42 `type` + 454 `method`
  entries (456 raw method entries − 2 collapsed override dupes `AActor:GetWorld`,
  `UScriptStruct:IsValid`; methods inherit their type's status; metamethods
  not flattened — see `docs/COUNTS.md` for every formula).
- **All 599 entries have `ps5_runtime_status: ""`** (599/599 blank). The
  pc-only-input classification of `Key.*`/`ModifierKey.*` comes from the new
  `pc_only_input: true` flag, not from a filled status.

### New: Key / ModifierKey TYPE entries (globals.json v4)

| Entry | kind | category | constants | flagged |
|---|---|---|---|---|
| `Key` | type | input | 165 (registration order, port-verified) | `pc_only_input: true` |
| `ModifierKey` | type | input | 3 active: `SHIFT` `CONTROL` `ALT` | `pc_only_input: true` |

Source (read-only verification, both trees):

- Port: `source/port/ue4ss/src/Mod/LuaMod.cpp:446-627` `register_input_globals`
  — `key_table.make_global("Key")` at `:614`, `modifier_key_table.make_global("ModifierKey")`
  at `:626`; registered for every true-mod Lua state (`:6310`) and the GUI
  console executor (`:6727`).
- Upstream: `UE4SS/src/Mod/LuaMod.cpp:438-618` `register_input_globals`
  (`make_global("Key")` at `:606`, `make_global("ModifierKey")` at `:618`).
- Values are Windows virtual-key codes (`Input::Key` enum,
  `deps/first/Input/include/Input/KeyDef.hpp`: `RETURN=0x0D`,
  `LEFT_ARROW=0x25`, `UP_ARROW=0x26`, `F7=0x76`; ModifierKey literals
  `SHIFT=0x10 CONTROL=0x11 ALT=0x12`). The 165-constant list was verified
  identical between port and upstream. The six `LEFT_SHIFT`…`RIGHT_ALT`
  ModifierKey variants are commented out (`/* */`) in BOTH trees and are
  therefore NOT registered (documented in the entry notes).
- `pc_only_behavior_notes`: "Windows virtual-key codes; PS5 has no keyboard -
  keybinds require runtime-side controller mapping (see PS5Pad.ini)".
- `needs_live_game: false` — the constants are pure integers, readable in any
  Lua state without a game.

## Checker changes for PC-only input (distinct category)

Claude's review required the Key usage to be reported as a PC-only input
dependency, not "not in catalog". Fixed in `compat/checker.py` (host tool):

- `bucket_for_entry()`: an entry flagged `pc_only_input: true` (or filled with
  the dedicated `pc-only-input` status) buckets as **pc-only-input**, winning
  over an EMPTY `ps5_runtime_status` — distinct from `missing` (API absent)
  and from `status-blank`/unknown (unverifiable).
- Type-ref resolution through a TYPE entry reports the **specific constant**
  (`Key.F7`), not just the table name.
- New `pc_only_input` array in the JSON result and a `PC-ONLY INPUT` section
  in the text summary, with the suggestion
  "keyboard keybinds → needs runtime/config controller mapping on PS5
  (PS5 has no keyboard; map controller chords, e.g. L3+R3 = F7, via
  PS5Pad.ini or an equivalent runtime translation layer)".
- Grading (round-5 policy): ANY blank `ps5_runtime_status` among the APIs
  the mod calls ⇒ `unknown - runtime status not filled` (this outranks
  everything, including PC-only input); pc-only input dependencies are
  listed as known PS5 differences INSIDE the unknown result
  (`known_ps5_differences`) and no longer upgrade the grade to
  `partly-compatible`. With NO blank statuses anywhere, pc-only input still
  grades `partly-compatible` with a dedicated reason; never `unsupported`
  by itself (that stays reserved for a central *missing* API).

## Scan highlights (full detail in the JSON)

Calls: total 66 | catalog-matched 32 | unknown 0 | unresolved 34 | lua-builtins 85.

**PC-ONLY INPUT dependencies (PS5 difference; all central — top-level
`RegisterKeyBind` args):**

| API | Site |
|---|---|
| `Key.F7` | `main.lua:694` (open/close menu) |
| `Key.UP_ARROW` / `Key.DOWN_ARROW` | `:698` / `:703` (navigate) |
| `Key.RETURN` | `:708` (confirm/spawn) |
| `Key.LEFT_ARROW` / `Key.RIGHT_ARROW` | `:713` / `:722` (category cycle) |

Cataloged but UNVERIFIABLE (empty `ps5_runtime_status`) — 9 APIs; the mod's
core runtime calls:

| API | Calls | centrality |
|---|---|---|
| `RegisterKeyBind` | 6 | central (registration, top level; the consumer of the Key.* constants) |
| `ExecuteInGameThread` | 6 | central (key handlers) |
| `StaticFindObject` | 4 | central |
| `FText` | 2 | central |
| `FName` | 2 | peripheral |
| `FindFirstOf` | 2 | peripheral |
| `StaticConstructObject` | 2 | peripheral |
| `LoadAsset` | 1 | peripheral |
| `print` | 1 | peripheral (newly visible: the v3 globals catalog registers the UE4SS `print` override; the merged catalog had been a stale v2 build) |

Not in the catalog: **none** (previously the six `Key.*` constants; now
cataloged and reported above).

34 method calls on untyped variables (e.g. `pc:K2_GetActorLocation`,
`actor:PickupInteractable`, UMG widget calls) are statically unattributable —
typically game-side Blueprint APIs, deliberately not counted against the grade.

## Next actions

1. Runtime/config-side (the graded difference): provide controller mapping for
   the six keybinds — `PS5Pad.ini` chords (L3+R3 = F7 is the current approach)
   or a general runtime translation layer (open question for GPT in
   `docs/ANSWERS.md`).
2. Catalog-side: fill `ps5_runtime_status` in `catalog/globals.json` /
   `catalog/types.json` for at least the 9 blank APIs above so the rest of the
   mod can be graded for real.
3. Re-run: `python3 compat/merge_catalogs.py && python3 compat/checker.py
   compat/townfall-item-spawner-mod compat/catalog-real-merged.json`.

## Artifacts (all Mac-only / host-tested)

- `compat/townfall-item-spawner-grade.json` — verdict + provenance + FULL scan JSON
- `compat/townfall-item-spawner-grade.md` — this summary
- `compat/townfall-item-spawner-mod/Scripts/main.lua` — staged mod source (read-only copy)
- `compat/catalog-real-merged.json` — merged real catalog v4 (checker input)
- `compat/merge_catalogs.py` — reproducible catalog merge (host tool)
- `compat/checker.py`, `compat/test_checker.py` — checker (v1.1: round-5
  grade policy + heuristic Lua syntax check "does not parse", finding #15)
  + tests (`python3 -m unittest test_checker` in `compat/`: **63/63 OK** —
  55 prior + 8 new/changed: `TestDoesNotParse` (7),
  `test_blank_status_outranks_pc_only_input`,
  `test_blank_forces_unknown_missing_still_listed` (renamed from
  `test_blank_does_not_mask_real_missing`);
  `test_townfall_grade_partly_compatible_pc_only_input` superseded by
  `test_townfall_grade_unknown_with_keyboard_note`)
