# agent-FA — zzdiag v9: Claude's MENU DRY RUN

Date: 2026-10-10 (task date 2026-10-08-l25-prep wave). Deliverables:
`zzdiag-v9/main.lua`, `zzdiag-v9/LOADLIST-ROW.txt`, this report. No console
contact, no uploads, no launches — staging only.

## What v9 is

Claude spec (verbatim): "Menu dry run in zzdiag (runs once at the main menu,
after the pump arms; no user input): FindAllOf('PlayerController') count +
IsPlayerController(); StaticFindObject('/Script/UMG.Default__WidgetBlueprintLibrary');
widgetLib:Create(pc, UserWidget class, pc); StaticConstructObject of one
TextBlock + SetText(FText('test')). Log PASS/FAIL per step; do NOT
AddToViewport."

v9 = v8's chassis (6 s T auto-probe, F7 manual rerun — kept verbatim in
token contract) + one automatic dry run of the ENTIRE menu path at +18 s,
executed on the game thread through `ExecuteInGameThread` (drained by the
VMT tick pump / R3 carriers), + a drain watchdog at +45 s.

## Files

- `zzdiag-v9/main.lua` — 3066 bytes (< 3072 limit),
  sha256 `909be10dd1d356b293e22225e2f697dd4f7e1bd4543949218da13db86df0909a`,
  `luac -p` clean.
- `zzdiag-v9/LOADLIST-ROW.txt` — size-exact admission sidecar; live row
  change is ONLY `ue4ss/Mods/zzdiag/Scripts/main.lua` 1488 -> 3066
  (reference full 13-row loadlist included; pull live first).

## Dry-run steps (in execution order, all on the game thread)

| Step | Call (exact form) | PASS when | Detail codes |
|---|---|---|---|
| D1 | `pcall(FindAllOf, "PlayerController")` + `#` | count > 0 | `n=<c>`; `n=-1` = call error |
| D2 | `pc:IsPlayerController()` (reflected bool) | returns true | `ipc=1/0`; `inv` = wrapper invalid; `skip` = no PC |
| D3 | `StaticFindObject("/Script/UMG.Default__WidgetBlueprintLibrary")` | wrapper resolves IsValid | `y`/`n`; `e` = call error |
| D4 | `StaticFindObject("/Script/UMG.UserWidget")` | resolves | `y`/`n`/`e` |
| D5 | `wl:Create(pc, uc, pc)` | returned widget IsValid | `y`/`n`/`e`; `skip` = D3/D4/PC missing |
| D6 | `StaticFindObject("/Script/UMG.TextBlock")` | resolves | `y`/`n`/`e` |
| D7 | `StaticConstructObject(tb, w or pc, FName("zzdry9"))` | object IsValid | `y`/`n`/`e`; `skip` |
| D8 | `tbo:SetText(FText("test"))` | pcall clean | `ftext`; `skip` = no D7 object |
| D9 | (attestation) | always | `no-viewport` — AddToViewport is NEVER called |

Wrappers-never-nil law honored (agent-CX v7 lesson): every lookup is judged
by `pcall obj:IsValid()`, never by nil. `FindAllOf` returns REAL nil (A1
contract) — D1 says `n=0`/`n=-1`. No `GetClass`/`FindFunction`/class-children
calls anywhere (v2 crash lesson). Failures cascade as `skip`, never abort.

## Call-form provenance

- `wl:Create(pc, userWidgetClass, pc)` — 3 params (object, class, object):
  TownfallItemSpawner MOD:509-511 + agent-CI-marshaling-audit.md section
  1.4 (WidgetBlueprintLibrary::Create = CreateWidget entry).
- `StaticConstructObject(class, outer, FName(name))` — Name param accepts
  FName userdata (LuaMod.cpp:2200-2308, FName ctor LuaFName.cpp:48-101,
  default FNAME_Add; "zzdry9" is deliberately short for the wide-char
  writer). Outer = created widget when D5 passed, else pc (the mod's own
  fallback shape for its root, MOD:516).
- `tbo:SetText(FText("test"))` — FText ctor from string + TextProperty
  param (agent-CI audit; UNREAL/src/FText.cpp:147-156).
- `IsPlayerController` — UEHelpers.lua:75 reflected call, taken bare in a
  pcall per spec (UEHelpers' `IsLocalPlayerController` fallback omitted;
  spec names exactly one call).

## Timing / threading design

- `LoopAsync(18000, ...)` fires ONCE 18 s after mod start (inside the
  task's 15-20 s window) and returns `false` — the port's
  `process_delayed_actions` (original/src/LuaMod.cpp, ActionType::Loop)
  removes a loop whose callback returns non-true, so this is a true
  one-shot; the same contract gives the 6 s T-probe immortality (`true`).
- The callback only ENQUEUES: `pcall(ExecuteInGameThread, dry)`. All
  engine calls run on the game thread, exactly like the real mod's menu
  path (MOD:695-728 F7 -> ExecuteInGameThread). If the pump is not
  draining, nothing runs and nothing crashes.
- Drain watchdog: a second one-shot `LoopAsync(45000, ...)` checks the
  `ran` flag (set as the FIRST statement of the game-thread body).
  `[DRY] WD no-drain` therefore splits "pump dead / queue never drained"
  from "steps ran and failed" — the exact discrimination the menu-path
  blocker needs. Absence of WD plus absence of D-lines is impossible:
  `ran` flips before any engine call.
- F7 manual rerun enqueues the same `dry` (guarded K1/K2 queue markers,
  v7/v8 shape).

## Expected log markers (verbatim strings)

Load/queue: `[zz] Z0 v9`, `[DRY] Q 18s`, `[zz] K1 f7`, `[zz] K2 q`.
Dry run (one line per step, all prefixed `[DRY] `):
`[DRY] D1 PASS n=2` / `[DRY] D1 FAIL n=0` / `[DRY] D1 FAIL n=-1`;
`[DRY] D2 PASS ipc=1` / `[DRY] D2 FAIL ipc=0` / `... FAIL skip` / `... FAIL inv`;
`[DRY] D3 PASS y` / `... FAIL n` / `... FAIL e` (same for D4, D6);
`[DRY] D5 PASS y` / `... FAIL e` / `... FAIL skip`;
`[DRY] D7 PASS y` / `... FAIL e` / `... FAIL skip`;
`[DRY] D8 PASS ftext` / `... FAIL e` / `... FAIL skip`;
`[DRY] D9 PASS no-viewport`; watchdog `[DRY] WD no-drain`.
Error detail lines: 3-space indent, first 24 chars after stripping the
`file:line:` prefix. Auto-probe unchanged from v8:
`[zz] T<n> db=<v|n|i|e> pc=<v|n|i|e>` every 6 s.

## Verification (host, no console)

- `luac -p` clean; 3066 B; sha256 above.
- Pure-Lua host harness (stub env: FindAllOf/FindFirstOf/StaticFindObject/
  StaticConstructObject/FName/FText/LoopAsync/ExecuteInGameThread), four
  cases, all observed:
  1. HAPPY — D1..D9 all PASS in order, exact strings above.
  2. LOOKUP-NIL (FindAllOf -> nil) — D1 FAIL n=0, D2/D5/D7/D8 FAIL skip,
     D3/D4/D6/D9 still evaluated (independent legs).
  3. TRUE NO-DRAIN (ExecuteInGameThread no-op) — only Q/Z0/WD lines +
     T tokens; `[DRY] WD no-drain` fires; zero D-lines (nothing ran).
  4. CREATE-THROW — `D5 FAIL e` + truncated error detail line; D7 still
     attempted with outer=pc; D9 attests no-viewport.

## Risks / honest limits

- Reflected UFunction calls (`IsPlayerController`, `Create`, `SetText`,
  `StaticConstructObject` internal ProcessEvent) are console-UNPROVEN
  (agent-CI: no live marshaled call has ever completed in this lineage);
  pcall catches Lua-level errors but cannot fence a native crash — that
  risk is inherent to the spec's purpose (this is the probe for it).
- The Lua-string->engine seam (1-char truncation, CLAUDE-SUMMARY
  RUNTIME-DECODE) may make D1-D6 resolve nothing at runtime even with a
  live menu; the [FFO] query-fallback markers in the PRX remain the
  seam-side evidence, unchanged by this script.
- 18 s assumes the menu is up and the pump armed by then; F7 reruns on a
  later menu state. T tokens confirm arrival independently.

## Consumption law

v8 (in /tmp) read as the base; STATE-L26-WAVE.md section 6/7 for live
loadlist state; agent-CI-marshaling-audit.md for call forms; UEHelpers
source-cache for IsPlayerController; original/src/LuaMod.cpp for the
LoopAsync boolean continuation contract. No source files modified.
