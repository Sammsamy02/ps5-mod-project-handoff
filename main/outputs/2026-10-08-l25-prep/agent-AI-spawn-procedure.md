# agent-AI — TownfallItemSpawner spawn-test procedure (variant B launch)

Read-only documentation of the UNMODIFIED Nexus mod (main.lua 21,415 B) so the
variant-B launch can verify menu + spawn in-session. Written 2026-10-08.

## 0. Sources read (all verified identical across the lineage)

| Artifact | Path (authoritative copy) | Hash / size |
|---|---|---|
| mod main.lua | `outputs/2026-10-08-l23-package/console-test-20261008T171655Z-964d76/harvest-20261008T174239Z-25929d/main.lua` | md5 4f15f2c79955c636eeaa38d0bf49433d, 21,415 B — byte-identical in l21/l22-r2/menu-complete/menu-path-audit copies |
| UEHelpers.lua | `outputs/2026-10-08-menu-path-audit/source-cache/UEHelpers.lua` | md5 d6eb453d019b8dcd1a156b1efbe6ab5b, 9,962 B |
| PS5Pad.ini | `outputs/2026-10-08-l23-package/.../harvest-20261008T174239Z-25929d/PS5Pad.ini` | md5 5510caf11fb9307931fe26602245205d, 3,718 B — identical in l22-r2 harvest |
| UE4SS-settings.ini (as-run l23) + l24 nohooks + l25 arms | `outputs/2026-10-08-l25-prep/settings-arm-*.ini`, `evidence/l24-settings-nohooks-1947.ini` | 1,947 B each |
| loadlist.txt (activation) | l23 harvest `loadlist.txt` | rows: main.lua 21415 / enabled.txt 0 / UEHelpers.lua 9962 |
| Runtime log vocabulary | `outputs/2026-10-08-l25-prep/LAUNCH-PLAN.md` §3.2, `evidence/l24-ue4ss-4.log`, `source-cache/LuaMod.cpp` (l21 snapshot) | — |

The mod has NEVER actually started on console yet (no `[TownfallItemSpawner]`
line exists in any harvested UE4SS.log; l24 ended at `Adding shutdown listener`
followed by `mfa_activate_failed code=-8`). The `[Lua] [TownfallItemSpawner] …`
log format below is corroborated by two independent evidence-pipeline fixtures
(`worker-evidence-pipeline-rehearsal/fixtures/s01_full_success`, 
`worker-runbook-v2/fixtures/fx04-admitted-copyin-full`), which also give the
expected scan results (12 pickups, Pages=2) and a sample spawn line.

## 1. Input mapping

### 1.1 Mod-side keybinds (main.lua:694-729) — six `RegisterKeyBind`, no modifiers

| PC key | Line | Handler (all wrapped in `ExecuteInGameThread`) | Action |
|---|---|---|---|
| `Key.F7` | :694 | `toggleMenu` | open/close the menu (toggle) |
| `Key.UP_ARROW` | :698 | `moveSelection(-1)` | selection up (wraps to last row) |
| `Key.DOWN_ARROW` | :703 | `moveSelection(1)` | selection down (wraps to first row) |
| `Key.RETURN` | :708 | `activateSelection` | **activate selected row** (see below) |
| `Key.LEFT_ARROW` | :713 | page = (page-2)%pageCount()+1 | previous page |
| `Key.RIGHT_ARROW` | :722 | page = page%pageCount()+1 | next page |

`activateSelection` on RETURN dispatches to the selected row's `activate()`
(main.lua:378-422):
- row 1 `Quantity: N` — cycles 1→2→5→10→20→1
- row 2 `Page: n/N` — cycles page
- row 3 `Refresh pickup list` — rescans the item database
- rows 4-11 (8 per page) — **`spawnSelected(item)` = the spawn trigger**
- row 12 `status` — no activate (dead row, selection can land on it)

**D-pad role: navigation and paging ONLY — it never confirms.** Confirm/spawn
is RETURN (pad: CROSS). Default selection on open = row 1 (Quantity), so a
fresh-open + CROSS changes quantity, it does NOT spawn — you must move DOWN
to a PICKUPS row first.

### 1.2 UEHelpers chain (main.lua:14-41 → UEHelpers.lua:69-82)

The mod uses UEHelpers for exactly one thing: `getPlayerController()`.
Chain: `pcall(require,"UEHelpers")` → `UEHelpers.GetPlayerController()` →
`FindAllOf("PlayerController") or FindAllOf("Controller")` → first entry
passing `IsPlayerController()`/`IsLocalPlayerController()`. On any failure the
mod falls back to `FindFirstOf("PlayerController")` (main.lua:21). Pawn is
resolved via `pc.Pawn` / `pc:GetPawn()` / `pc:K2_GetPawn()`. UEHelpers has NO
input helpers — all input is the mod's own `RegisterKeyBind` above; the pad
bridge lives entirely in the runtime (PS5Pad.ini profile).

### 1.3 PS5Pad.ini combo→key table (3,718 B staged copy, parser PS5PadConfig.cpp)

| DualSense | Produces | Mod effect |
|---|---|---|
| `L3+R3` (chord) | `F7` | open/close menu — rising-edge conjunction, hold one and tap the other, one F7 per chord completion |
| `UP` (D-pad) | `UP_ARROW` | selection previous |
| `DOWN` (D-pad) | `DOWN_ARROW` | selection next |
| `CROSS` | `RETURN` | activate row / **spawn** |
| `LEFT` (D-pad) | `LEFT_ARROW` | page previous |
| `RIGHT` (D-pad) | `RIGHT_ARROW` | page next |

- `OPTIONS` is deliberately UNBOUND (game's own pause; plan §2 step 5).
- Pad events are NON-EXCLUSIVE: L3/R3/D-pad/CROSS still reach the game while
  the menu is open. The mod compensates with
  `pc:SetIgnoreLookInput(true)` + `pc:SetIgnoreMoveInput(true)` on open
  (main.lua:446-452, called at :656/:663) and releases them on close.

## 2. On-screen behavior — what the user sees on PS5

**No `NotifyAtTop`, no `DrawText`, no console prints.** The menu is a real UMG
widget tree (main.lua:488-645): `UserWidget` → `WidgetTree` → `CanvasPanel` →
red `Border` frame → near-black `Border` → `VerticalBox` of `TextBlock`s,
`AddToViewport(9999)` (topmost).

- Position: top-RIGHT corner (anchors/alignment X=1.0, position -24,+24), size
  570x500. Colors: dark-red frame, near-black inner, bright-red title.
- Title: `TOWNFALL : ITEM SPAWNER` (21 px), red dashed divider, then the
  controls line (gray): `UP/DOWN SELECT | ENTER SPAWN | LEFT/RIGHT PAGE | F7 CLOSE`
- Sections `[ OPTIONS ]` / `[ PICKUPS ]` / `[ STATUS ]`; rows `Quantity: 1`,
  `Page: 1/N`, `Refresh pickup list`, 8 pickup names per page, status line
  (initially `Load a save, then open with F7.`).
- Selected row gets a `>> ` prefix and turns bright red; others `   ` gray.
- Secondary visible confirmation the open-handler ran: the game's
  look/move input freezes (sticks stop steering) while open.
- The game is NOT paused. On successful spawn the status row becomes
  `Spawned <Name> x<qty>`; on failure `Failed at <stage>. See UE4SS.log.`

## 3. Log markers — exact strings

`log(s)` = `print("[TownfallItemSpawner] " .. s)` (main.lua:5-6); rendered in
UE4SS.log as `[Lua] [TownfallItemSpawner] <s>` (fixture-corroborated format).

Startup (fires at mod script load, in the MAIN MENU, before any save):
- `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` (main.lua:746)
- Runtime: `[l22] Lua mod started: TownfallItemSpawner (main script returned)` (LuaMod.cpp:7557 per LAUNCH-PLAN §3.2)

Menu open (first L3+R3):
- `[Lua] [TownfallItemSpawner] <N> spawnable pickups found. Pages=<P>` (:189) — e.g. `12 spawnable pickups found. Pages=2`
- `[Lua] [TownfallItemSpawner] Menu created.` (:643)
- Failure variants: `… Scan failed: <err>` (:194; e.g. `Item database not ready. Load a save first.`), `… WidgetBlueprintLibrary not found.` (:501), `… UI creation failed: <err>` (:636)

Spawn (CROSS on a PICKUPS row):
- **`[Lua] [TownfallItemSpawner] Spawned <ItemName> x<qty> ID=<ActualID>`** (:334-335) — fixture sample: `Spawned Antibiotics x1 ID=DeprecatedItem`
- Failure: `… Failed at <stage>. See UE4SS.log. <err>` (:341-342), stage ∈ {player, ground trace, load pickup class, spawn actor, configure pickup}

Per-press runtime/diag evidence (supplier-diag.jsonl + UE4SS.log):
`pad_l3r3_edge` → `pad_l3r3_f7_dispatched` (chord), `pad_key_edge` (each
mapped button), `r3_marshal_pumped` heartbeat; `[l22] ExecuteInGameThread
queued method=…` paired with `[l22] ExecuteInGameThread drained … (callback entry)`.

## 4. Dependencies — attributing a silent menu in variant B

1. **Game-thread pump (biggest silent-failure risk).** Every one of the six
   keybind callbacks is wrapped in `ExecuteInGameThread` (main.lua:694-729).
   In the l24/l25 runtime the EngineTick detour is refused
   (`[R3] refused: EngineTick pre-callback registrations`, `[R3] EngineTick
   detour unavailable; deferring game-thread pump to R3 marshal` — seen in
   both l23 and l24 logs) → the R3 FuncPtr-marshal IS the pump. If it never
   pumps: the chord dispatches (diag shows `pad_l3r3_f7_dispatched`) but there
   is no queued→drained pair, no `[TownfallItemSpawner]` lines, and no menu —
   a perfectly SILENT menu means the pump, not mod logic.
2. **GATE-1 / FName pool admission.** `buildUI` → `construct()` calls
   `FName(name)` for every widget it creates (root, tree, canvas, frame,
   inner, vbox, title, divider, controls, sections, rows — UE4SS `FName(str)`
   defaults to FNAME_Add → FName pool writes). This is exactly the blocked
   copyin-oracle path. If GATE-1 still fails, expect
   `UI creation failed: …` on first open. The spawn path itself performs NO
   FName adds (StaticFindObject/LoadAsset/GameplayStatics only) — but it is
   unreachable until the menu builds.
3. **HookLoadMap.** `RegisterLoadMapPreHook` (:731, pcall-wrapped, non-fatal)
   only resets state on map change. Needs `HookLoadMap=true`; the console's
   current v2 nohooks file has it false — plan §1.2 restores the 1,947 B
   original (HookLoadMap=true + HookInitGameState=true) before launch. If
   skipped: no effect within a single save session; stale widget refs across
   map transitions only.
4. **UEHelpers + object array.** `require("UEHelpers")` needs
   `Mods/shared/UEHelpers/UEHelpers.lua` staged (it is — loadlist row 12);
   `FindAllOf`/`FindFirstOf` need UObject-array init complete (i.e. runtime
   init passed :528). `bUseUObjectArrayCache=false` is fine.
5. **In-game state.** `scanItems` needs `FindFirstOf("ItemDatabaseSubsystem")`
   — exists only after a save is loaded; spawn needs a valid pawn. Open the
   menu only after save-load (or use the Refresh row).
6. **Variant B specifics.** B skips `AddUObjectDeleteListener` → stale
   UObjects are never invalidated. The mod holds long-lived refs
   (`ownedActors`, `ui.*` widgets) and its `isValid()` checks may read stale
   memory — a B-only crash during menu/spawn points at the missing delete
   listener, not at the mod.
7. **Admission before any of this.** If `[l22] Lua mod started` and the
   `Loaded v1.0` line are absent, the mod never started at all (MFA/loadlist —
   l24's `mfa_activate_failed code=-8` size mismatch made that run unmodded).
   Verify the 1,947 B settings readback first; a silent menu with no Loaded
   line is NOT an input problem.

## 5. Variant-B spawn-test procedure (DualSense)

Preconditions (LAUNCH-PLAN §1 done): reboot; variant B PRX resident with
decrypt-OFF magic+sha256 verified; 1,947 B original UE4SS-settings.ini
readback-verified; ploader re-armed per §1.3; klog tap connected (§1.4);
receipt states variant B is diagnostic-only, never left resident.

| # | Action (DualSense) | Maps to | Expected on-screen | Expected evidence |
|---|---|---|---|---|
| 1 | Launch game; stay in main menu 90 s (covers pad-borrow ≥10 s uptime) | — | nothing | UE4SS.log: post-:528 markers, `Starting callback garbage collector!`, `[l22] Lua mod started: TownfallItemSpawner (main script returned)`, `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` If absent → mod never started (admission/init), STOP. |
| 2 | Load the SAME save; wait 30 s | — | normal save load | runtime-status.txt started-ack; no crash (if B crashes at save-load → listener-invocation path, per plan §4.3) |
| 3 | **Hold L3, tap R3** (one chord) | F7 → toggleMenu | dark-red panel top-right: `TOWNFALL : ITEM SPAWNER`, controls line, rows `Quantity: 1`, `Page: 1/N`, `Refresh pickup list`, 8 item names, status; sticks stop steering (input lock) | diag: `pad_l3r3_edge` → `pad_l3r3_f7_dispatched`; UE4SS.log: ExecuteInGameThread queued→drained pair, `[Lua] [TownfallItemSpawner] 12 spawnable pickups found. Pages=2`, `… Menu created.` |
| 4 | Tap **DOWN** ×3 | DOWN_ARROW | `>>` marker: Quantity → Page → Refresh → **first pickup row** (bright red) | diag: `pad_key_edge` ×3; row highlight moves each tap |
| 5 | (opt.) tap **CROSS** on Quantity row | RETURN | `Quantity: 2` | status/quantity row text changes |
| 6 | **Press CROSS once** on a pickup row | RETURN → spawnSelected | item pickup appears on the ground ~1.5 m ahead of the player along camera facing (ground-snapped, +8 cm); status row: `Spawned <Name> x1` | **`[Lua] [TownfallItemSpawner] Spawned <ItemName> x1 ID=<ActualID>`** — the definitive spawn proof |
| 7 | (opt.) **LEFT / RIGHT** | arrows | `Page: n/N` changes; 8 names swap | page row text + row swap |
| 8 | **Hold L3, tap R3** again | F7 | panel disappears; sticks re-activate | input lock released (visible) |
| 9 | Clean exit via in-game quit | — | back to dashboard | harvest per plan §3; then RESTORE variant A / l24 composition — never leave B resident |

Failure triage during the run:
- Step 3 chord fires in diag but no menu, no `[TownfallItemSpawner]` lines →
  game-thread pump (R3 marshal) dead (§4.1).
- `UI creation failed: …` on open → GATE-1 FName pool admission still blocked (§4.2).
- `Scan failed: …Item database not ready…` → opened before save settled; wait,
  select `Refresh pickup list` (row 3) with CROSS.
- `Failed at player` → no pawn (menu/main-menu context); `Failed at ground
  trace` → stand in an open flat area, retry; `Failed at load pickup class` →
  LoadAsset path issue for that item — try another row.
- Any crash: note code (expected signature CE-108255-1) and the exact step;
  reboot required before any next launch.

## 6. Final quick-reference

Minimal successful sequence: **load save → hold L3 + tap R3 → DOWN ×3 →
CROSS → hold L3 + tap R3 (close)**.

Definitive markers:
- Mod alive: `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`
- Menu proven: `[Lua] [TownfallItemSpawner] Menu created.` (+ `N spawnable pickups found. Pages=P`)
- **Spawn proven: `[Lua] [TownfallItemSpawner] Spawned <ItemName> x<qty> ID=<ActualID>`**
