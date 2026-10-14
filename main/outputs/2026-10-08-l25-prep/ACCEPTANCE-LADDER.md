# ACCEPTANCE-LADDER — "MOD MENU UP" success criteria for the next console run

Written 2026-10-08 by agent-CK. This is the DEFINITIVE marker-by-marker
acceptance ladder for the next launch (fixed PRX + zzdiag v3). All success
markers below are from the REAL mod (TownfallItemSpawner main.lua 21,415 B,
md5 4f15f2c79955c636eeaa38d0bf49433d — agent-AI-spawn-procedure.md section 3),
NOT from any diagnostic. zzdiag v3 lines are corroborating evidence only and
can never satisfy a rung on their own.

Context: the current blocker is the nullptr UFunction resolution
(evidence/UFUNCTION-NULLPTR-FINDING.md) — `FindFirstOf` works but
`db:GetItemIDFromDeprecatedEnum("Bandage")` fails with "the UFunction is a
nullptr", so `scanItems()` cannot build the item list and the menu never
opens. The next run carries the FIXED PRX; every rung below tells you
whether the fix landed and, if not, exactly which owner owes the next fix.

Acceptance law (QUALITY-RUBRIC spirit): every rung needs the actual log
line in the harvested UE4SS.log (harvest.py shows line numbers) and, where
marked USER-VISIBLE, the player's on-screen observation. No rung may be
scored from inference. klog must be clean for the run to count.

## SUCCESS LADDER (rungs in order; each includes the previous)

### Rung 0 — admission + mod start (prerequisite, not success)
- `[l25] event loop started` and `Starting callback garbage collector!`
  (init passed :528; variant-B session shows the SKIPPED line instead of a
  registration-returned marker — that is expected and fine under variant B)
- `[l22] Lua mod started: TownfallItemSpawner (main script returned)`
- `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` (fires at script load, in the MAIN MENU, before any save)
- `[Lua] [zz] Z0 v3` — zzdiag v3 also admitted and loaded (cross-evidence mod)
- FAIL -> failure ladder F1 (admission).

### Rung 1 — scan works: THE nullptr-fix proof
- `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<P>`
  (fixture expectation N=12, P=2; any N>=1 passes).
- This is the EXACT call family that failed on 2026-10-08 night
  (`GetItemIDFromDeprecatedEnum` + `GetInteractablePickupIDActorPath`). Its
  success is the direct, in-game proof that UFunction-by-name resolution is
  fixed. zzdiag v3 on the same chord press should show
  `[Lua] [zz] A3 ok` / `B1 ok` / `B2 id=<short>` / `C1 ok` / `C2 <path>` —
  the stepwise twin of this rung.
- FAIL (`Scan failed: ...` or zz `B1 no` + `E*` chunks) -> F2 (resolution
  still null). zz `B1 ok` + `C1 no` + `F*` chunks -> F3 (marshaling).

### Rung 2 — menu opens: log proof + USER-VISIBLE panel
- `[Lua] [TownfallItemSpawner] Menu created.`
- USER-VISIBLE (player confirms on screen): dark-red panel, top-RIGHT
  corner, title `TOWNFALL : ITEM SPAWNER`, controls line, rows
  `Quantity: 1`, `Page: 1/N`, `Refresh pickup list`, 8 pickup names,
  status line. Secondary visible signal: sticks stop steering
  (SetIgnoreLookInput/SetIgnoreMoveInput on open).
- FAIL (`UI creation failed:`) -> F4 (widget/GATE-1). FAIL
  (`WidgetBlueprintLibrary not found.`) -> F5 (widget dependency).

### Rung 3 — spawn: the mission marker
- `[Lua] [TownfallItemSpawner] Spawned <ItemName> x<qty> ID=<ActualID>`
  (fixture sample: `Spawned Antibiotics x1 ID=DeprecatedItem`). Definitive
  spawn proof; harvest.py parses name/qty/ID.
- USER-VISIBLE: the pickup appears on the ground ~1.5 m ahead along camera
  facing; the status row reads `Spawned <Name> x1`.
- Input corroboration for the whole ladder (expected on the way here):
  - `[l22] keybind callback entry key=118` — the L3+R3 chord's F7 dispatch
  - `[l22] keybind callback entry key=13` — CROSS on a PICKUPS row (RETURN)
  - `[l22] ExecuteInGameThread queued ...` paired with
    `[l22] ExecuteInGameThread drained ... (callback entry)` per press
    (the R3-marshal game-thread pump working)
  - `pad_l3r3_edge` -> `pad_l3r3_f7_dispatched`, `pad_key_edge` per mapped
    button, and the `r3_marshal_pumped` heartbeat in supplier-diag.jsonl.
- FAIL (`Failed at <stage>.`) -> F6 (spawn pipeline stage; stage names the
  sub-owner). Chord fires in diag but NO `[TownfallItemSpawner]` toggle
  lines and no queued/drained pair -> F7 (pump).

### Rung 4 — klog clean (gates the whole run)
- klog capture for the session contains NO fatal blocks: no `copyin`
  error bursts, no `nonsleeping` violations (harvest.py crash-signature
  verdict), no kernel backtrace ending the session. Any CE-108255-1 crash
  -> F8 regardless of how many rungs passed before it.
- Clean exit to dashboard at the end (PS5 rule 5: clean exit resets the
  launch state; a crash requires a reboot before ANY next launch).

### ACCEPTED = "MOD MENU UP" mission pass
Rungs 0-4 all green. If only 0-2 pass: menu-up achieved, spawn blocked ->
the ladder names the owner. Anything less: use the failure ladder.

## FAILURE-MODE LADDER (shortfall -> owner)

| # | Shortfall observed | Owner | Next action |
|---|---|---|---|
| F1 | No `Loaded v1.0`, no `[l22] Lua mod started`; possibly `mfa_activate_failed code=<n>` | ADMISSION (loadlist/MFA staging, NOT the fix) | Readback UE4SS-settings.ini (1947 B), loadlist rows vs file sizes byte-exact (zzdiag row must say 2314 for v3), zzdiag/enabled.txt present; fix staging, relaunch after clean exit |
| F2 | `Scan failed:` with `UFunction ... nullptr` (or zz v3: `B1 no` + `E*` chunks; also the expected pre-swap baseline) | RESOLUTION STILL NULL (the PRX FName/UFunction fix — l26 item) | The fix did not take: capture zz E-chunk reassembly (harvest diag-zz), compare error text with the 2026-10-08 baseline, back to the fix owner |
| F3 | zz v3: `B1 ok` + `B2 id=<short>` but `C1 no` + `F*` chunks (mod may show `Scan failed:` instead of rung 1) | MARSHALING (Lua param/return marshaling of the id into the path call) | F-chunk text decides: nullptr-family -> same as F2 for the second function; type/coercion text -> marshaling owner |
| F4 | `UI creation failed:` on first open | WIDGET (GATE-1: buildUI performs FName(name) adds -> FName pool writes, the blocked copyin-oracle family) | Scan+spawn chain is proven; the widget path is the blocker. l26 GATE-1 item |
| F5 | `WidgetBlueprintLibrary not found.` | WIDGET dependency (StaticFindObject of the UMG helper class) | Object-array/class-path resolution for WidgetBlueprintLibrary |
| F6 | `Failed at player` / `ground trace` / `load pickup class` / `spawn actor` / `configure pickup` | SPAWN PIPELINE per stage: player=pawn/save state; ground trace=stand in open flat ground, retry; load pickup class=LoadAsset path for that item (try another row); spawn actor/configure pickup=runtime actor path | Menu is UP (mission threshold reached); stage names the next fix |
| F7 | Chord fires in diag (`pad_l3r3_f7_dispatched`) but NO mod toggle lines, no queued/drained pair, silent screen | PUMP (R3 FuncPtr marshal is the game-thread pump; EngineTick detour is refused in this runtime) | Pump owner; note zz v3 also pumps on F7 — if `[zz] K1/K2` appear but no `A3`, zz's own closure did not drain either (pump), if `A3 ok` appears the pump works and the mod's closure is the difference |
| F8 | Crash CE-108255-1 (or any) | Note the exact step; reboot before any next launch (PS5 rule 5). With v3 resident the crash is NOT the diag's raw probes (v3 has none; v1 crash-free precedent) — prime suspects are the widget FName adds (F4 family) or variant-B stale-UObject reads (agent-AI section 4.6) | Preserve UE4SS.log + klog, harvest, attribute by the last marker before the crash |
| F9 | klog shows `copyin`/`nonsleeping` fatal blocks | KERNEL-SIDE copyin oracle (GATE-1 family) | Capture is the evidence; pair with whichever Lua-side rung failed |

Precedence when several shortfalls coexist: F1 blocks everything; then F7
(pump) masks F2-F6 (nothing drains); with the pump alive, F2/F3 (scan) gate
F4/F5 (widget), which gate F6 (spawn). F8/F9 gate the whole run's validity.

## zzdiag v3 reading key (cross-evidence only)

`Z0 v3` loaded -> per F7 press: `K1 f7` (keybind) -> `K2 q` (queued) ->
game thread: `A3 ok`/`A3 db-nil` (FindFirstOf; db-nil = save not loaded) ->
`B1 ok`+`B2 id=<short>` / `B1 no`+`E0..En` 30-char error chunks (nullptr
family) -> `C1 ok`+`C2 <path>` / `C1 no`+`F0..Fn` chunks -> `B3 none`
flags id==""/"None" (DB content, not a code failure). v3 contains NO
GetClass/FindFunction probes (v2's crasher, removed). All payloads <= 36
chars. harvest.py auto-reassembles the chunks in its `diag-zz` verdict.
