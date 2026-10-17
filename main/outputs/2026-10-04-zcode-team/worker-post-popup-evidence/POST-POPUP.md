# POST-POPUP — evidence the controller reads after the user presses OPTIONS

Worker: worker-post-popup-evidence · 2026-10-05 · local only, read-and-derive, no canonical edits.
Mod: TownfallItemSpawner v1.0, UNCHANGED. Canonical bytes: `worker-settings-v3-package/package-v3/ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` (21,415 B, MD5 `4f15f2c79955c636eeaa38d0bf49433d`) + `shared/UEHelpers/UEHelpers.lua` (9,962 B, MD5 `d6eb453d019b8dcd1a156b1efbe6ab5b`). All 22 other 21,415 B staged copies are byte-identical — every pin below holds for any staged tree. Companion: `CLAIM.json` (pins), `FAILURE-TAXONOMY.md` (errors), `REPORT.md` (limits).

Precondition for everything below: ORACLE BEST already reached (`mods-started` + the load line
`[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` — main.lua:746).
The rungs here are G4-class in ORACLE terms: **not** launch-10 verdict inputs; they are the NEXT
rungs of the ladder and this file says exactly what log/screen evidence closes each one.

## 0. Log rendering facts (read UE4SS.log correctly)

- Every mod print renders `[Lua] [TownfallItemSpawner] <msg>` — `[Lua] ` from the print
  implementation (LuaLibrary.cpp:39,45), `[TownfallItemSpawner] ` from the mod's log tag (main.lua:5-6).
- The mod's `log()` embeds a trailing `\n` inside the print argument (main.lua:6), so UE4SS.log may
  show a trailing blank or `[Lua] `-only artifact line after each mod line. Count TAGGED lines.
- UEHelpers.lua emits **zero** lines, ever. Its failures are silent by design (returns
  `CreateInvalidObject()`, main.lua's `isValid()` rejects it and it falls back to `FindFirstOf`,
  main.lua:8-22).
- **Navigation is mute.** UP/DOWN/LEFT/RIGHT and the Quantity/Page rows print nothing. Their evidence
  is on-screen only (red `>> ` marker, `Page: n/m` text). Only scan, menu-create, spawn outcome, and
  load print.
- **Repeated OPTIONS presses are mute by design.** Reopening an already-built menu logs nothing:
  `buildUI` early-returns (main.lua:489-491) and the scan only runs when `#items == 0` (main.lua:651).
  After a map load, `RegisterLoadMapPreHook` (main.lua:731-744) resets everything, so the FIRST press
  after a map load always scans and always logs.

## 1. User instruction text (PS5Pad.ini mapping, verbatim-usable)

> 1. Load a save and get into the game world (not the main menu).
> 2. Press **OPTIONS** — a dark red panel titled "TOWNFALL : ITEM SPAWNER" appears at the
>    **top-right** of the screen. A red `>>` marker shows the selected row.
> 3. Press **D-pad UP / D-pad DOWN** to move the red marker. The first rows are
>    `Quantity:` (press CROSS to cycle 1 → 2 → 5 → 10 → 20) and `Page: n/m` and
>    `Refresh pickup list`; below them are up to 8 item rows (`---` means empty slot).
> 4. Press **D-pad LEFT / D-pad RIGHT** to change page.
> 5. Move the marker onto an **item row** and press **CROSS (X)** — the item spawns on the
>    ground ~a few meters in front of you. The STATUS row at the bottom of the panel shows
>    `Spawned <item> x<n>` on success or `Failed at <step>. See UE4SS.log.` on failure.
> 6. Walk into the spawned item to pick it up (it disappears into your inventory).
> 7. Press **OPTIONS** again to close/open the panel.

Mapping provenance: `PS5Pad.ini:35-40` — `OPTIONS = F7`, `UP = UP_ARROW`, `DOWN = DOWN_ARROW`,
`CROSS = RETURN`, `LEFT = LEFT_ARROW`, `RIGHT = RIGHT_ARROW` (`User = -1`, first logged-in user,
`PS5Pad.ini:32`); mod binds at main.lua:694-729, every callback wrapped in `ExecuteInGameThread`.
Note (documented limitation, PS5Pad.ini:45-48): keybinds are non-exclusive; while the menu is open
those inputs still reach the game. The mod compensates with `SetIgnoreLookInput/SetIgnoreMoveInput`
(main.lua:450-451) — do not be surprised if the camera/character still twitches on input while the
menu is open.

## 2. Rung-by-rung evidence table

| Ladder rung | What proves it in UE4SS.log (verbatim) | Pin | What proves it on screen | Failure shape that fails the rung |
|---|---|---|---|---|
| 1. Game-thread callback (drain runs) | ANY new `[TownfallItemSpawner]` line after the **first** OPTIONS press of a session/map: `<N> spawnable pickups found. Pages=<M>` OR `Scan failed: <err>` — the scan runs inside the drained body (main.lua:651→647→694) | main.lua:189 / :194 | the panel appears top-right | First-press produces **zero** new mod lines → keybind unreachable or drain stalled (see §3). NB: silence on press #2+ is NORMAL (main.lua:489-491, :651) |
| 2. Object query | `<N> spawnable pickups found. Pages=<M>` ⇒ `FindFirstOf("ItemDatabaseSubsystem")` hit (main.lua:64) + both DB getters called (main.lua:163,168). `Menu created.` ⇒ PlayerController hit (main.lua:21/493) + `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")` hit (main.lua:499) | main.lua:189, :643 | panel exists with real rows | `Scan failed: Item database not ready. Load a save first.` (:194+:66) = DB query failed; `WidgetBlueprintLibrary not found.` (:501) = UMG CDO query failed; PlayerController miss is SILENT (:495) — menu never appears, no log |
| 3. Harmless reflected access | `Menu created.` is itself the proof: ~dozens of UMG reflected reads/writes and UFunction calls ran clean (SetText/FText, SetPadding/Anchors/Alignment, SetColorAndOpacity, Font.Size read+write, AddToViewport, SetVisibility, SetIgnoreLook/MoveInput — full list CLAIM.json) | main.lua:643 | panel text renders, rows colored, red marker visible | `UI creation failed: <err>` (:636) = a reflected call threw during construction; `Scan failed: <err>` (:194) = DB getter threw |
| 4. Unchanged Item Spawner activation | `Spawned <name> x<Q> ID=<actualID>` — the full spawn path ran: pawn reads → ground `LineTraceSingle` → pickup-class `StaticFindObject`/`LoadAsset` → `BeginDeferredActorSpawnFromClass` → `FinishSpawningActor` → configure (main.lua:215-336) | main.lua:335 | STATUS row shows `Spawned <name> x<Q>` (:334) | `Failed at <stage>. See UE4SS.log. <err>` (:342); partial actor auto-destroyed (:344-346). Stage ∈ player / ground trace / load pickup class / spawn actor / configure pickup — see FAILURE-TAXONOMY.md |
| 5. VISIBLE SPAWN | Same `Spawned <name> x<Q> ID=<actualID>` line **plus** a captured frame showing the actor | main.lua:335 | the item lies on the ground a few meters ahead, facing the player; walking into it picks it up and it vanishes (`bAutoDestroyOnPickUp`, main.lua:326) | log line present but nothing visible → actor spawned hidden/underground elsewhere: check `Failed at` absence, quantity, and the ground-trace placement math (150 units ahead, +8 above ground — main.lua:233-275). The log line ALONE never closes this rung |

## 3. First-OPTIONS-press discrimination table (the drain probe)

After map load the mod state is reset (main.lua:731-744), so the first OPTIONS press is a live
probe of the whole open path. Exactly one of these outcomes must occur:

| Observation on first press | Diagnosis |
|---|---|
| `<N> spawnable pickups found. Pages=<M>` + `Menu created.`; panel visible | EVERYTHING WORKS: drain, object query, reflected access. Proceed to CROSS-spawn. |
| `Scan failed: Item database not ready. Load a save first.` (+ maybe `Menu created.`) | Drain works, object query ran, DB not up (main-menu or pre-save press). Load a save, press OPTIONS again — NB second press re-scans only if the previous scan left `items` empty; after a failed scan `items` is `{}` so it DOES re-scan (main.lua:651, 185). |
| `Scan failed: <other err>` | Drain works; DB getters threw at reflection level — capture err text, see FAILURE-TAXONOMY.md S-group. |
| `Menu created.` but NO scan line | Drain works; PC valid; scan skipped because `items` non-empty from an earlier successful scan (possible if the menu auto-scanned earlier in the session). Not a fault. |
| Zero new mod lines, panel does NOT appear, game otherwise normal | Keybind unreachable OR drain stalled. Discriminate: init-era `[R3] refused:` / `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` lines in UE4SS.log (API-GAP.md §3/§5) ⇒ drain stall by construction (R3 variant) — expected, not new info. No `[R3]` lines ⇒ input-source problem; also possible: the one-shot call-time throw `ExecuteInGameThread: EngineTick method requested but EngineTick hook is not available (AOB scan failed)` (LuaMod.cpp:4729) — in THIS mod it fires at the FIRST PRESS (main.lua:695), not at load. |
| Menu opened earlier, now CROSS on an item row gives neither `Spawned` nor `Failed` line | Check in order: (a) selected row is `---` (empty slot) — silent by design (main.lua:220); (b) STATUS row shows `Wait for the previous spawn.` (busy gate, main.lua:217) — wait a moment; (c) does the red marker still move on UP/DOWN? If NO → whole pump stopped mid-session (all six callbacks share one queue; partial stall impossible at queue level). If YES → capture is stale; re-check UE4SS.log tail. |
| `Failed at ground trace. See UE4SS.log. <err>` specifically | The historically highest-risk item (API-GAP.md §4): `LineTraceSingle` out-param marshalling (`hit.ImpactPoint`/`.ImpactNormal`, main.lua:264-269). Mod stays alive; retry elsewhere/on flat ground before concluding API gap. |

## 4. What the user SEE-check adds (rung 5 only)

The mod's own confirmation is the `Spawned ... ID=...` line; the SEE-check is the user/capture
confirming an actor is present at the expected place: ~150 unreal units ahead along the camera yaw
(main.lua:233-238), resting on traced ground +8 (main.lua:245-275), unhidden + collidable
(main.lua:329-330). If the user reports "nothing appeared" WITH a `Spawned` line in the log:
ask whether they were facing a wall/steep surface within 150 units (trace start Z+50 → end Z-400,
main.lua:247-248) and whether the item may have spawned behind geometry; the actor is NOT cleaned
up on success (tracked in `ownedActors`, main.lua:332) — it persists until picked up.

## 5. Quick log-grep crib (post-popup acceptance pass)

```
grep "\[TownfallItemSpawner\]" /data/plugins/UE4SS.log
```
Expected GOOD post-popup tail, in order, one OPTIONS + one CROSS:
1. `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`  (load; already BEST evidence)
2. `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<M>`                        (rungs 1+2)
3. `[Lua] [TownfallItemSpawner] Menu created.`                                                 (rung 3)
4. `[Lua] [TownfallItemSpawner] Spawned <name> x<Q> ID=<id>`                                   (rungs 4+5 mod-side)
Any `Failed at ...` / `Scan failed: ...` / `UI creation failed: ...` lines → FAILURE-TAXONOMY.md.
Honest bound: line 4 + a captured frame together close rung 5; line 4 alone closes only rung 4.
