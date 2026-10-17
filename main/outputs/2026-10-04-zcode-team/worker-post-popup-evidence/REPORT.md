# REPORT — worker-post-popup-evidence · 2026-10-05

## What was done

Read the unchanged mod COMPLETELY and built the post-popup evidence set:

- `main.lua` (21,415 B, MD5 `4f15f2c79955c636eeaa38d0bf49433d`) — read in full, 746 lines, from the
  staged settings-v3 package. Verified byte-identical across all 22 other staged copies under
  `outputs/**` (only the deliberately-corrupted 21,414 B `worker-itemspawner-bundle/host-test/root-badsize`
  loadlist fixture differs; it is not a candidate).
- `shared/UEHelpers/UEHelpers.lua` (9,962 B, MD5 `d6eb453d019b8dcd1a156b1efbe6ab5b`) — read in full;
  all copies identical. Key derived fact: it emits ZERO runtime log lines; its failure mode is silent
  by design (returns `CreateInvalidObject()`, main.lua rejects it via `isValid()` and falls back to
  `FindFirstOf`, main.lua:8-22).
- `PS5Pad.ini` (staged package-v3 copy, MD5 `c0d3592e80c993bddc41de883ec660d7`, byte-identical to
  worker-real-input-pad's) — bindings OPTIONS→F7, UP/DOWN/LEFT/RIGHT→arrows, CROSS→RETURN, User=-1.
- Cross-checked ORACLE.md (G4 row, BEST clauses), READINESS.md (§5 timing/input, §6 oracle),
  API-GAP.md (§3 drain variants, §5 error shapes) — and verified two runtime facts directly in the
  composed C++ rather than trusting the docs: `global_print` prefixing (LuaLibrary.cpp:39,45) and the
  `ExecuteInGameThread` call-time availability check (LuaMod.cpp:4729).

## Deliverables

- `CLAIM.json` — lifecycle-ordered runtime print inventory (8 emission sites), on-screen-only status
  strings, input-flow map, per-rung exact evidence, API-GAP drain cross-check, all line-pinned.
- `POST-POPUP.md` — rung-by-rung evidence table, first-OPTIONS-press discrimination table, the
  copy-paste user instruction text (OPTIONS → D-pad → CROSS), log-grep crib.
- `FAILURE-TAXONOMY.md` — six groups: load-time, menu/scan, spawn-by-stage (5 stages, 16 distinct
  `error()` payloads pinned), silent-by-design presses, runtime-level drain/input signatures,
  controller decision shortcut.

## Key findings

1. **Only 8 log emissions exist post-load** (main.lua:189, :194, :335, :342, :501, :636, :643, :746).
   Navigation, page flips, quantity, close, and reopen are mute — evidence for those is on-screen only.
   Repeat OPTIONS presses on a built menu are ALSO mute by design (main.lua:489-491, :651) — log silence
   there is not a stall.
2. **The first OPTIONS press after load/map-load is a free drain probe**: the scan runs inside the
   drained body whenever `items` is empty (main.lua:651), so ≥1 new mod line MUST appear. Zero lines on
   press #1 = keybind-unreachable or drain-stalled; the `[R3] refused:` init lines discriminate which.
3. **"Menu opens but spawn never executes" is structurally impossible from the queue**: all six
   callbacks share one `m_engine_tick_actions` queue (LuaMod.cpp:4774-4790). The observable decomposes
   into: silent `---`-row press (:220), busy gate (:217), spawn-executed-and-failed (:342 line present),
   or a mid-session pump death that freezes the WHOLE menu (marker stops moving — that is the tell).
4. **API-GAP cross-check confirms**: RegisterKeyBind path and the EngineTick drain are in the READY set
   on the readpath-guard lineage; the R3-armed fold-1 variant is the only drain blocker, with its own
   named init signature. One correction vs API-GAP §5 wording: the `ExecuteInGameThread: … not available
   (AOB scan failed)` error fires at the mod's FIRST PRESS (main.lua:695 is the first call site), not at
   load — API-GAP's "immediate at registration" phrasing holds only for mods that call it during load.
5. **Ground trace is the highest-risk spawn stage** (matches API-GAP §4): `Failed at ground trace.
   See UE4SS.log.` is the expected shape if `LineTraceSingle` out-param marshalling
   (`hit.ImpactPoint`/`.ImpactNormal`, main.lua:264-269) is broken; the mod stays alive either way.

## Honest limits

- Source-derived only. NOTHING here is an outcome promise; every in-game rung (1-5) stays UNKNOWN
  until a real console capture. No rung is claimed advanced by this work.
- Visible-spawn evidence requires the log line AND a captured frame; I state that requirement but
  cannot produce or judge frames.
- Exact UE4SS.log rendering of the `log()` trailing-`\n` artifact (blank vs `[Lua] `-only line) is
  unpinned — controllers should count `[TownfallItemSpawner]`-tagged lines.
- Exact numeric values (N pickups, M pages, IDs, addresses) are runtime facts, pattern-marked, never
  hard-gated.
- UEHelpers.lua contains many APIs the mod never exercises (`GetWorld`, `GetGameStateBase`, FName
  helpers, hit-result actor resolution); they were read but contribute no runtime lines in this mod.
- The user instruction text assumes the composed-runtime pad bridge (PS5Pad.ini present + provider
  healthy). If the ini is absent at start there is NO pad bridge at all (NativePadInput.cpp:14) — the
  whole instruction is then inapplicable, and that failure is startup-visible, not popup-visible.
- ORACLE G4 discipline respected: visible spawn is NOT a launch-10 verdict input; this set is for the
  rungs AFTER the BEST verdict's terminal evidence.
