# agent-AJ — mod-side harvest markers (TownfallItemSpawner) — 2026-10-08

Completed the harvest marker catalog with the Lua mod's own log strings per
LAUNCH-PLAN.md v3.3 + agent-AI-spawn-procedure.md. Edited ONLY
`harvest/markers.json` and `harvest/harvest.py` (additive, backward
compatible). No console contact; all testing local (analysis functions +
`--dry-run`).

## 1. Marker count: 54 -> 64

- BEFORE: 54 entries (agent-Z14 catalog).
- AFTER: 64 entries. 10 new patterns added; ZERO duplicates; every source
  "cited" (no PENDING).

| New pattern (verbatim) | Phase | Source (verified line) |
|---|---|---|
| `[TownfallItemSpawner] Loaded v1.0` | mod-start | main.lua:746 (log fn :5-6) |
| `spawnable pickups found` | menu-open | main.lua:189 (status :188) |
| `Scan failed:` | menu-open (failure) | main.lua:194 |
| `[TownfallItemSpawner] Menu created.` | menu-open | main.lua:643 |
| `UI creation failed:` | menu-open (failure, GATE-1 suspect) | main.lua:636 |
| `WidgetBlueprintLibrary not found.` | menu-open (failure) | main.lua:501 (LAUNCH-PLAN v3.3 cites :502 — one-line drift; string identical, noted in the entry) |
| `[TownfallItemSpawner] Spawned ` (trailing space) | spawn — SPAWN PROOF | main.lua:335 (status :334) |
| `[TownfallItemSpawner] Failed at ` (trailing space) | spawn (failure, stage in msg) | main.lua:342 (stage :341) |
| `[l22] keybind callback entry key=118` | menu-path (F7 dispatch) | LuaMod.cpp:2040 `key={} ref={}`; l24 ladder step 9 |
| `[l22] keybind callback entry key=13` | menu-path (RETURN dispatch) | LuaMod.cpp:2040; PS5Pad.ini CROSS->RETURN (agent-AI 1.3) |

- `Starting callback garbage collector!` ALREADY EXISTED (agent-Z14 entry)
  — NOT duplicated; its source was enriched in place with the v3.2/v3.3
  mod-context (`UnrealInitializer.cpp:542` — in variant B the first proof
  init passed :528).
- Every string re-verified against the provenance main.lua copy
  (`outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/
  harvest-20261008T174239Z-25929d/main.lua`, 21,415 B, 746 lines) and the
  fx04 fixture rendering (`[Lua] [TownfallItemSpawner] ...`).
- **One deliberate deviation from the task brief, evidence-forced:** the
  brief listed the scan-success marker as `[TownfallItemSpawner] spawnable
  pickups found`, but the real rendered line inserts the count between tag
  and phrase — `[Lua] [TownfallItemSpawner] 12 spawnable pickups found.
  Pages=2` (fx04 fixture; main.lua:188-189 builds `"<N> spawnable pickups
  found."`). A tag-prefixed substring can NEVER match; the pattern is
  `spawnable pickups found` (documented inside the entry). The synthetic
  test caught this before it shipped.

## 2. New harvest.py verdict fields (backward compatible)

`verdict()` gains an additive `modside` key from a new `mod_verdict()`
(placed beside `listener_verdict()`); every pre-existing field, function
signature, and exit code is untouched:

- **mod menu opened** — YES when `[TownfallItemSpawner] Menu created.`
  OR `spawnable pickups found` present (the F7 toggle itself logs
  nothing; this pair is the only in-log menu-open evidence).
- **item spawned** — YES when `[TownfallItemSpawner] Spawned ` present;
  parses `Spawned <name> x<qty> ID=<id>` (regex `MOD_SPAWN_RE`; multi-word
  names handled). `Failed at <stage>` lines are collected into
  `failed_at` (stages: player / ground trace / load pickup class /
  spawn actor / configure pickup) without flipping the verdict.
- **menu failure mode** — WHICH failure marker hit, precedence
  `ui-creation-failed` > `widgetbp-missing` > `scan-failed`, else `none`;
  all hit families recorded with line numbers.
- UNKNOWN (never a false positive) when UE4SS.log is unavailable; rendered
  in the live VERDICT section, the dry-run skeleton, and the final
  stdout summary line (`menu-opened= item-spawned= menu-failure=`).

## 3. Regression — /tmp/l24-ab2-ue4ss.log (old vs new module, exit 0)

Old snapshot `/tmp/aj-old/` (pre-change copies) vs live files:
- `analyze_sessions` identical (602 lines, 1 banner, last ts 14:07:39 @602).
- All five old verdict fields identical: init=NO, mod-started=NO,
  crash=UNKNOWN, admission=OK, listener registered/refused/variant_b/
  engine_called all NO.
- All 54 old patterns retained verbatim; their scan results identical.
- All 10 new patterns: 0 hits on the l24 log; new fields read
  menu-opened=NO, item-spawned=NO, menu-failure=none (and UNKNOWN on a
  missing log).
- Test: `/tmp/aj-regression.py` (log `/tmp/aj-regression-out.txt`).

## 4. Synthetic-log test (built in /tmp, exit 0)

- `/tmp/aj-synth-ue4ss.log` (success path, fixture-grounded formats):
  menu-opened=YES, item-spawned=YES, item names PARSED —
  `Antibiotics x1 ID=DeprecatedItem` (line 21) and multi-word
  `First Aid Kit x5 ID=Consumable_FirstAidKit` (line 22); menu-failure=
  none; keybind key=118/key=13, `Starting callback garbage collector!`,
  and all new markers hit in the scan.
- `/tmp/aj-synth-fail-ue4ss.log` (failure branches, bonus): menu=NO,
  spawn=NO with `failed_at stage='ground trace'` captured, menu-failure
  mode=`ui-creation-failed` by precedence with all three failure families
  recorded.
- Test: `/tmp/aj-synth-test.py`.

## 5. --dry-run proof

`python3 harvest.py --label aj-dryrun-proof --dry-run` -> **exit 0**;
64-entry marker table (all cited); the three new VERDICT skeleton rows
render (output `/tmp/aj-dryrun-out.txt`); zero files written (no
HARVEST-*.md in the harvest dir), zero network calls.

## 6. Honest gaps

- `agent-AI-spawn-procedure.md` cites `WidgetBlueprintLibrary not found.`
  at main.lua:501 while LAUNCH-PLAN v3.3 cites :502; provenance grep says
  :501. Recorded in the marker entry; not silently reconciled.
- The `[Lua] ` render prefix comes from fixtures/worker reports, not from
  a console-harvested UE4SS.log (the mod has never started on console);
  all new patterns are prefix-agnostic substrings, so the question is
  moot for matching.
