# HOWTO-GPT — runtime owner guide

**Audience:** GPT, owner of the PS5 runtime (UE4SS composition PRX, ploader
injection, engine ABI).

**Scope of this project: Mac-only / host-tested.** Everything here was
authored and verified on a Mac with zero console contact. The console-side
steps below are deployment instructions for you — none of them have been
executed by this project's authors.

Your three jobs vis-a-vis this project:

1. Fill in `ps5_runtime_status` in the API catalogs after console testing.
2. Run the conformance scripts in the real runtime and bring back logs.
3. Confirm (or correct) the config formats and load-order assumptions the
   installer writes (see `docs/CONTRACT-PROPOSAL.md` Section 6).

## 1. Filling in `ps5_runtime_status`

### Where the field lives

Two real catalog files carry the field (edit these; never hand-edit the
merged file — it is regenerated):

- `catalog/globals.json` — 106 global entries (98 functions + 6 Rev-2
  conformance additions + the `Key`/`ModifierKey` TYPE tables); each entry
  has its own `"ps5_runtime_status"` field.
- `catalog/types.json` — 42 type entries; the status lives at the **type**
  level. Every method of a type inherits that type's status when the
  checker catalog is merged (methods have no own status field).

### Allowed values (exact strings)

| value | meaning to the checker |
|---|---|
| `implemented+console-proven` | counts as working (green side) |
| `implemented-untested` | counts as working (green side) |
| `missing` | counts against the grade; central use → `unsupported` |
| `behaves-differently` | counts against the grade; listed separately with your `notes`/`workaround` text |
| `""` (empty or absent) | **unverifiable**: any mod call that resolves to it forces the grade `unknown` |
| anything else | treated as `behaves-differently` AND flagged in the report notes as an unrecognized status |

Example entry:

```json
{
  "name": "RegisterHook",
  "category": "hooking",
  "ps5_runtime_status": "implemented+console-proven",
  "workaround": "",
  "notes": "Hook ids returned; UnregisterHook also verified."
}
```

Optional fields you may populate: `workaround` (shown verbatim to users as
the runtime-side workaround) and `notes` (context; used as the fallback
suggestion for missing/behaves-differently entries).

### After editing the catalogs

Rebuild the merged checker catalog, then re-grade a mod to see the effect:

```bash
python3 compat/merge_catalogs.py
python3 compat/checker.py <mod.lua | mod-dir> compat/catalog-real-merged.json
```

`merge_catalogs.py` propagates each type's status to all of its methods,
fuses the string-ctor globals with their types, and writes
`compat/catalog-real-merged.json` (599 entries today). The merge refuses if
the two files' `catalog_version` metadata disagree — bump the version
whenever you change statuses, so badge caches can invalidate
(proposal Section 4.6).

Current state: **all 599 merged entries have an empty status** — every real
mod grades `unknown` until you fill them in.

## 2. Running the conformance scripts on console

Both suites are read-only: no spawning, no property writes, no
ProcessEvent; registrations use no-op callbacks. Every call is wrapped in
`pcall`, so a missing API records a FAIL row instead of crashing.

### Static suite (`conformance/conformance_static.lua`)

353 calls over game-state-independent APIs (strings, names, thread queries,
constants, version queries). Deploy as a regular UE4SS mod:

1. Copy `conformance_static.lua` to
   `<UE4SS>/Mods/ConformanceStatic/Scripts/main.lua` (exact case).
2. Create an empty `<UE4SS>/Mods/ConformanceStatic/enabled.txt`.
3. Launch the game with UE4SS attached.
4. Extract the machine-readable rows:
   `grep $'^CALL\t' UE4SS.log > run_static_ps5.log`

### Live suite (`conformance/conformance_live.lua`)

52 calls over live-game APIs (FindFirstOf/FindAllOf, reflection reads,
FName/FString/FText/TArray, hooks, keybind query). Deploy:

1. Copy `conformance_live.lua` to
   `<Game>/Mods/conformance-live/Scripts/main.lua` (standard UE4SS layout).
2. Add the row `conformance-live : 1` to `<Game>/Mods/mods.txt`.
3. Behavior: 10 s startup delay, then polls every 2 s for a Pawn AND a
   PlayerController (up to 240 s); once in game it waits 5 s and runs the
   whole battery on the game thread. On timeout it still runs in DEGRADED
   mode and game-state rows record SKIP. Manual re-run: call the global
   `ConformanceLiveRun()` from the console.
4. Log: every row is `print()`ed to the UE4SS console/`UE4SS.log`, and when
   `io` is available also written to `conformance_live.log` (first at
   `$CONFORMANCE_LIVE_LOG_DIR/`, else the game working directory).
5. Extract: `grep $'^CALL\t' UE4SS.log > run_live_ps5.log`

Row format (both suites, 6 tab-separated fields):
`CALL<TAB>api<TAB>params<TAB>result_type<TAB>result<TAB>status`.

## 3. Comparing PC vs PS5 results with the diff tool

Run on the Mac host (pure Python 3, no deps) against two extracted logs.
The baseline (log A) is typically a PC UE4SS run of the same suite; log B
is your PS5 run. Both must be extracted the same way, because rows are
keyed by `(api_name, occurrence_index)` in emission order.

```bash
python3 conformance/diff_results.py run_static_pc.log run_static_ps5.log \
    --json pc_vs_ps5.diff.json --verbose
```

- Statuses: **MATCH** (identical params/result/type/value/ok),
  **MISMATCH** (same call key, differing fields — listed per row),
  **MISSING** (baseline only), **EXTRA** (PS5 only).
- Output: summary table with per-section breakdown on stdout; JSON diff
  (input sha256s, summary, full detail) at `--json` or
  `<a>_vs_<b>.diff.json` by default.
- Exit codes: `0` equivalent, `1` differences found, `2` usage/parse error.
- Caveat: a deleted mid-log line shifts later same-api occurrences, which
  surface as a paired MISMATCH+MISSING rather than one MISSING.
- Worked examples on fixture logs: `conformance/src-conformance-static/samples/`.

## 4. What GPT needs to confirm for the contract

From `docs/CONTRACT-PROPOSAL.md` Section 6, with the exact bytes the
installer writes today:

1. **Fill in `ps5_runtime_status`** for every catalog entry after console
   testing (values above).
2. **loadlist.txt format.** Header line `modforge-loadlist\t1`, then
   `title\t<TITLE_ID>` and `engine\tunreal` rows, then per mod a
   `mod\t<mod_dir>\t<version>` row followed by
   `file\t<role>\t<install_rel>\t<size>` rows, where `<size>` is the file's
   EXACT byte size and `<role>` is one of `lua`/`config`/`asset`. Confirm
   the runtime requires the true byte size.
3. **ploader.ini format.** Section `[<TITLE_ID>]` containing
   `<prx_name>=<mode>` (installer default
   `modbridge_supplier_composition.prx=2`). Confirm existing format.
4. **mods.txt + enable marker.** The installer writes BOTH a row
   `Name : 1` in `<ue4ss>/Mods/mods.txt` (spaces stripped at parse time,
   `;` comments, load order = row order, rows start before marker mods)
   AND an empty `Mods/<Name>/enabled.txt` marker. Confirm this matches the
   runtime's start conditions.
5. **Read scope.** Confirm the runtime never reads mod files outside
   `ue4ss/Mods/<Name>/` — the installer keeps its manifests under
   `<roots>/<TITLE>/manifests/` and settings overrides under
   `ue4ss/overrides/<mod>.ini`.
6. **UE4SS-settings.ini ownership.** It is yours; the installer never
   writes it. Mod-requested settings go to the overrides file in point 5.

Open contract questions that need your input (load-order enforcement,
catalog-version bump ownership) are listed in CONTRACT-PROPOSAL Section 8.
