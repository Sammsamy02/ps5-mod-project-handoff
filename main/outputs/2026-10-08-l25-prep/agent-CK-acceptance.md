# agent-CK — "MOD MENU UP" acceptance package (2026-10-08)

Deliverables for the next console run (fixed PRX + zzdiag v3). Everything
written under outputs/2026-10-08-l25-prep/ only. Inputs consumed:
evidence/UFUNCTION-NULLPTR-FINDING.md, agent-AI-spawn-procedure.md,
harvest/markers.json + harvest.py, the console-resident zzdiag v2
(/tmp/zzdiag/Mods/zzdiag/Scripts/main.lua, 1,107 B, loadlist row verified)
and the v2 session evidence (evidence/zzdiag-v2-session.log, /tmp/zzdiag2.log).

## Files created / changed

| File | What |
|---|---|
| zzdiag-v3/main.lua | NEW. Safe probe: v1's A/B/C chain + v2's 30-char chunked error tails. NO GetClass/FindFunction probes (v2's crasher removed). luac -p clean; mocked-runtime behavioral test passed for all 3 paths (nullptr fail / full ok / db-nil). 2,314 bytes, sha256 2670e493d1c89d1f1499d82aad5f9dbd7d1f0fc85aa8798d5e9983f639fa8228 |
| zzdiag-v3/LOADLIST-ROW.txt | NEW sidecar: exact v3 size, the two loadlist rows, full 9-row expected loadlist, removal state |
| zzdiag-v3/loadlist-zzdiag-v3.txt | NEW staged loadlist (545 B; verified byte-identical to the verified postboot copy except zzdiag row 1107 -> 2314) |
| ACCEPTANCE-LADDER.md | NEW. Rungs 0-4 + failure table F1-F9 with owners |
| harvest/harvest.py | EDITED (additive, backward compatible): new `diag-zz` verdict — reassembles A1..E/F chunk lines, survives the console writer's mid-line [l22] collisions and wide-char NUL interleaving (proven against the real v2 session: all 12 E chunks recovered, 0 gaps, nullptr text intact), per-press chain states, wired into report + final verdict line |
| RUNBOOK-MOD-MENU-UP.md | NEW. Phase 0-5 console-file sequence |

## zzdiag v3 probe sequence (per F7 = L3+R3 chord)

`Z0 v3` at load -> `K1 f7` -> queued `K2 q` -> on the game thread:
`A3 ok|db-nil` (FindFirstOf) -> `B1 ok|no` + `B2 id=<short>` (`B3 none`
if ""/"None") for GetItemIDFromDeprecatedEnum -> `C1 ok|no` + `C2 <path>`
for GetInteractablePickupIDActorPath; errors re-emitted as `E0..En` /
`F0..Fn` 30-char chunks. Every payload <= 36 chars. Works before the PRX
swap (re-baselines the nullptr) and after (cross-checks the fix); dual-F7
coexistence with the real mod was already proven in the v2 session.

## harvest.py diag-zz verification

- Real v2 session log: verdict PRESENT:B-FAIL, 12/12 E chunks reassembled,
  "nullptr ... in method" recovered, 8 truncated chunks honestly flagged.
- Synthetic v3 CHAIN-OK / C-FAIL+F-chunks / multi-press best-of / merged
  single-line payloads / truncated series letters: all pass.
- Backward compat: existing harvested logs (harvest-l25-B-final/live1)
  read ABSENT; no [zz] lines can never produce a false verdict; --dry-run
  unchanged (plan + skeleton only, zero writes).

## Key loadlist law (repeat)

MFA admission is size-exact: uploading v3 without updating the row
`file lua_mod ue4ss/Mods/zzdiag/Scripts/main.lua 2314` fails admission
(mfa_activate_failed family = ladder F1). Keep v3 during the test
(cross-evidence on every chord press), remove after (Phase 5).

## The ladder (short form)

1. Rung 0 admission+start: `[l22] Lua mod started: TownfallItemSpawner` +
   `[TownfallItemSpawner] Loaded v1.0 ...` (+ `[zz] Z0 v3`) — else F1 admission.
2. Rung 1 scan = THE fix proof: `[TownfallItemSpawner] <N> spawnable
   pickups found. Pages=<P>` — else F2 resolution-still-null (zz `B1 no`+E) /
   F3 marshaling (zz `B1 ok` but `C1 no`+F).
3. Rung 2 menu: `Menu created.` + USER-VISIBLE dark-red top-right panel
   (sticks freeze) — else F4 widget/GATE-1 (`UI creation failed:`) or F5
   (`WidgetBlueprintLibrary not found.`).
4. Rung 3 spawn: `Spawned <name> x1 ID=<id>` + user sees the pickup ~1.5 m
   ahead, corroborated by `key=118`/`key=13` keybind entries + queued/drained
   pairs + `r3_marshal_pumped`; `Failed at <stage>` = F6 per-stage owner;
   silent screen with dispatched chord = F7 pump.
5. Rung 4 klog clean (no copyin/nonsleeping fatal blocks; F8 crash / F9
   kernel) gates the whole run. All green = "MOD MENU UP" accepted.
