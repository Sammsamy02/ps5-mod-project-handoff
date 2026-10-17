# Replacement for InfiniteStamina: candidate mods

Date: 2026-10-12. Track: replacement-mod (round 8 prep). Mac and web research only.
No console contact, no mod downloaded from the web, no mod file edited.
Labels: **Mac-only** = read from files on this Mac or from web pages. **console-proven** = a line in
the B4 launch log (`evidence/launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log`, cited as `D:<line>`).

---

## 1. Plain verdict

**I could not find any Townfall mod with written proof that it works on the current game version.**
Nexus Mods blocks my web tools (HTTP 403 on every page), so I could not read any Nexus comments,
"last updated" fields or bug reports. The mirror site VGTimes shows no game version and no comments
for any of these mods. So nothing below is "known to work". What follows is the best-supported
choice plus the checks that would turn a guess into proof.

**The brief said "prefer mods that need no RegisterHook". The only candidate I could read in full does NOT meet that.**
Here is the honest ranking, with the hook cost stated up front:

1. **TownfallModMenu v6** (Nexus mod 71, file 266). Already on this Mac, byte-checked, and I read all of its source.
   It changes values (God Mode, movement speed, healing). Movement speed needs no hook to work (Mac-only reading).
   **God Mode's health write uses a hard-coded memory offset taken from the PC build (+0x67C) that nothing confirms for the PS5**,
   so I would test Speed first and keep God Mode off until a second source confirms the offset (section 5.1, 5.2).
   **But it is a hook-heavy mod:** 11 `RegisterHook` call sites in the code (19 distinct hook targets, 4 of them dead code),
   plus 5 map-load hook calls. **10 of its hook targets are Blueprint functions, which our PS5 setting refuses today**
   (`HookProcessInternal=0`), and its 5 map-load callbacks are never delivered (`D:922`). Five of those refused hooks
   (the "pace" hooks) are tried **at startup with no button pressed**, up to three times (at load, +3 s, +7 s).
   - **It must not be run on B4 or earlier.** Every refused hook leaks the lock that blocks the pad/hotkey thread (the B4 failure). The mod wraps every
     hook call in `pcall`, but before the B5 fix a caught refusal still leaks the lock. Run it only on B5/B6 or later (where refusals unwind).
   - It is a heavy, many-feature test. A failure is harder to pin on one cause. Complete hook list: section 5.2.
   - Ranked first only because it is the single candidate whose full source I could audit today, and because it doubles as a stress test for
     the refused-hook list. It is not the "quiet, simple" test the brief preferred.
2. **More Speed and Infinite Stamina** (azan22, Nexus mod 10; folder `SpeedHackMod`). Small (6 KB). **Hook use is unknown until I read the file.** If it
   turns out to need no RegisterHook it becomes the better first choice (quiet, obvious effect: the world visibly speeds up). Only its speed keys
   would count (the stamina half is the idea that is broken in InfiniteStamina). Needs a download.
3. **SILENT HILL Townfall Comfort Camera** (Nexus mod 59, changelog up to 1.0.5). Passive (no hotkeys): 15% faster walk/run/sneak and FOV 110.
   Hook use unknown. Needs a download.

The Lua API list is complete only for ModMenu (section 5). For candidates 2 and 3 it is **unknown** and I say so rather than guess (sections 4.2, 4.3).

**Two facts that matter for "current version":**

- The newest public patch is **1.4.0, released 8 Oct 2026** (menu shows v1.5.154194). Every candidate
  was uploaded **22-30 Sep 2026, before that patch**. None says it was tested on 1.4.0.
- **Our PS5 game is probably NOT on 1.4.0.** The B4 launch checked the game's executable and it matched the
  copy we saved on 2 Oct (same sha `3b6cfad4...`; `D:28`, `D:34`, console-proven). The 1.4.0 patch came six days after
  that copy. So the PS5 build is very likely the launch-era build, which is what these Sep-2026 mods were written
  against. This is an inference: the PS5 menu version number would settle it (section 7).

**One more practical point:** ModMenu and the item spawner both answer D-pad Up/Down and Cross (`PS5Pad.ini` maps them to the same keys).
Each one ignores those keys while its own menu is closed, so there is no clash as long as **only one of the two menus is open at a time**
(section 5.4).

**What the user must do:** see section 7. Short version: nothing for InfiniteStamina (the original is
already here); for the new mod, either approve the ModMenu for B5/B6+ now, or download one Nexus file and tell Claude where it is.

---

## 2. The InfiniteStamina original is already on this Mac

The user does not need to supply it.

| Item | Value |
|---|---|
| Nexus page | https://www.nexusmods.com/silenthilltownfall/mods/19 (Framecore, "Infinite Stamina", v1, 1 KB, 22 Sep 2026) |
| File page | https://www.nexusmods.com/silenthilltownfall/mods/19?tab=files&file_id=30 (from `.superpowers/native-observations/nexus-real-mods-2026-10-01/stamina-download-receipt.json`) |
| Original zip (2 copies, identical) | `/WORKSPACE/Downloads/Infinite Stamina 19 1 2026-09-22T19-18Z OPrGe3tIa.zip` and `.superpowers/native-observations/nexus-real-mods-2026-10-01/infinite-stamina-v1.zip` |
| zip sha256 | `61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5` (1,435 B) |
| `Scripts/main.lua` sha256 | `7667fed7e4d7b4cfa9c9f0ed2a794ac815f2121218f43f41192147f62294e335` (2,811 B, 106 lines) |
| Deployed copy matches | the copy in the morning spool (`.../spool/rows/ue4ss/Mods/InfiniteStamina/Scripts/main.lua`) has the same sha (Mac-only, re-hashed today) |

Why it does nothing (Mac-only, re-read today): `local InfiniteStaminaActive = false` (line 11); the only code
that reads it is the `LoopAsync(1500, ...)` block at lines 89-103, and nothing ever sets it to true
(no keybind, no hook). So on PC it is also inert, matching the reviewer's note.
The property it would write is `bStaminaEnabled` on the player's gait component.

---

## 3. Current game version (what "current" means)

| Fact | Source | Notes |
|---|---|---|
| Base game released 24 Sep 2026 on PS5 and PC | [Game Rant](https://gamerant.com/silent-hill-townfall-update-patch-notes/), [PSU](https://www.psu.com/news/silent-hill-townfall-update-1-4-0-brings-key-changes-to-the-weight-including-improved-ai-removing-instant-death-grab-attack-full-patch-notes-listed/) | Nexus UE4SS file is dated 22 Sep, so early copies existed. |
| **Patch 1.3.1, 30 Sep 2026**, main menu shows **v1.4.153829**. Single fix: shader stutter. | [TwistedVoxel](https://twistedvoxel.com/silent-hill-townfall-patch-1-3-1/) (opened); platform split from a search summary only (Steam + Epic) | Not stated for PS5. |
| **Patch 1.4.0, 8 Oct 2026**, PS5 + Steam + Epic, main menu shows **v1.5.154194** | [The Click](https://www.theclick.gg/silent-hill-townfall-update-1-4-0/) (opened); Game Rant article dated 8 Oct 2026 | Another site printed "24 Sep"; that is the game's launch date, not the patch. |
| 1.4.0 contents: The Weight grab no longer instant-death on Story/Normal, enemy AI less sensitive, puzzle manual, out-of-bounds and glass fixes, frame pacing and streaming stutter. **No change listed to stamina, speed, health or items.** | [TwistedVoxel 1.4.0](https://twistedvoxel.com/silent-hill-townfall-update-1-4-0/), [PSU](https://www.psu.com/news/silent-hill-townfall-update-1-4-0-brings-key-changes-to-the-weight-including-improved-ai-removing-instant-death-grab-attack-full-patch-notes-listed/) | "Not listed" is not "not changed". Enemy-AI mods (e.g. The Weight freeze) are the most exposed. |
| Steam news pages for 1.3.1 and 1.4.0 | [1.3.1](https://store.steampowered.com/news/app/1636440/view/685266627108798510), [1.4.0](https://store.steampowered.com/news/app/1636440/view/685267261521396615) | Opened, but my tool returned only the page title (no body, no date). |
| Engine | Nexus UE4SS page text (via search): "UE 5.6, experimental-latest, stable 3.0.1 will not work, custom StaticConstructObject signature" ([mod 4](https://www.nexusmods.com/silenthilltownfall/mods/4), [VGTimes copy, opened](https://vgtimes.com/games/silent-hill-townfall/files/98689-ue4ss.html)) | PC pages say "5.6" only. **No source states PC is 5.6.1.** Our PS5 image is 5.6.1 (STATE-L26-WAVE.md:257, Mac-only). Lua mods do not depend on the minor version, but class and property names do (section 6). |

Our PS5 package: PPSA33286, image sha `3b6cfad47dd17fea17d429bce80ae374...`, matched to the profile at launch
(`D:28`, `D:34` console-proven). Same sha as the 2 Oct capture `.superpowers/native-observations/townfall-static-executable-2026-10-02/capture.json`.

---

## 4. Top recommendations in detail

### 4.1 TownfallModMenu v6 (recommended first)

| | |
|---|---|
| Page | https://www.nexusmods.com/silenthilltownfall/mods/71 (chadi7bark, "TOWNFALL MOD MENU") |
| Exact file | `TownfallModMenu`, **file v6**, file_id 266: https://www.nexusmods.com/silenthilltownfall/mods/71?tab=files&file_id=266 (26 KB; page version still "1"; the 30 Sep 2026 date comes only from the zip file name `2026-09-30T03-22Z`, not from a Nexus page) |
| Already here | `/WORKSPACE/Downloads/TownfallModMenu 71 6 2026-09-30T03-22Z AW6IuGRZU.zip`, sha256 `a9257d8e23c1a377d9acddee36ef82c624aece7920fa6f92a41e21e0690456ca` (matches the intake receipt `mod-menu-v6-receipt.json`). Contents: `TownfallModMenu/Scripts/main.lua` (128,009 B, sha `2ff1fa5395f668ee7539b6fb3b993c79293a37350889a3aa287f9c104ff39883`, 3,198 lines) and an empty `enabled.txt`. No config file. |
| Version evidence | The file is labelled v6 (zip name says 30 Sep 2026; Nexus is blocked for me, so I could not re-check). VGTimes mirror (26 Sep, 16.87 kB, i.e. an earlier build) shows 0 comments and no stated game version. (An earlier read of that page showed a download counter of about 295; I did not re-check it today and do not rely on it.) In-file tag: `v0.66-weight-invisibility` (main.lua:5). **No compatibility statement and no post-1.4.0 comment found.** |
| What it changes | Menu (Backspace, Up/Down, Enter): God Mode, One Hit Kill, healing multiplier, invisibility to enemies, infinite ammo, infinite weapon durability, infinite items, movement speed 1x-5x, fog off. Source: [VGTimes page, opened](https://vgtimes.com/games/silent-hill-townfall/files/98440-mod-menu-god-mode-infinite-ammo-ohk-fog.html) and main.lua. |
| Features that need NO hook to work (Mac-only, main.lua) | **Speed** = writes to the game's pace data (`Pace_Target`), kept up by the 100 ms game tick (lines 2476-2562); its 5 pace hooks (2587-2606) are extras, tried at startup, and **refused today**. **God Mode** = `player.bCanBeDamaged = false` by reflection (193-217) **plus a health write at hard-coded offset +0x67C** via `RegisterCustomProperty` (155-187), taken from the PC build with no second source for PS5 (section 5.1). |
| Features that need hooks | Infinite ammo and durability (`GetAmmoCountForItemID` / `GetWeaponDurability`, 1530-1565) hook native functions and need a **return-value override that was never exercised**. Invisibility needs 4 Blueprint hooks that are refused today, plus one native one. Infinite Items needs a Blueprint hook (refused) and a native one. Full list of all 19 hook targets with line numbers, when each is tried, and status: section 5.2. |
| Why the effect is checkable | God Mode and speed are visible in play. The log prints "Movement gait source found" / "Loaded v0.66" style lines (main.lua log() calls). |

**Honest cons:** it is a large UMG menu, so a failure is harder to pin on one cause than with a 100-line mod.
It is a poor "simple" test but a good "real mod" test. It does **not** meet the "prefer no RegisterHook" preference (11 call sites, 10 refused Blueprint targets, 5 undelivered map-load callbacks), it must run only on B5/B6 or later, it must not share an open menu with the spawner, and God Mode should stay off until the hard-coded offset is confirmed (section 5).

**Suggested first test if ModMenu is chosen** (Mac-only proposal): Movement speed 2x only. Check: the player visibly walks faster, the B6 `[PERF]` lines show no new hitch, and the log shows `Movement gait source found` or the pace-struct messages. Then one feature at a time. God Mode, Healing and One Hit Kill wait for the offset check.

### 4.2 More Speed and Infinite Stamina ("SpeedHackMod")

| | |
|---|---|
| Page | https://www.nexusmods.com/silenthilltownfall/mods/10 (azan22) |
| Exact files (from `.superpowers/native-observations/townfall-mod-candidates-2026-10-01.md`, Nexus files tab, 1 Oct) | **`SilentHillTownfall SpeedHack ALL Character` v1.1, 6 KB, 23 Sep** (this one); older `SilentHillTownfall MoreSpeed` v1, 5 KB, 22 Sep |
| Version evidence | Search snippet: Nexus "Last Update 23 Sep 2026", uploaded 22 Sep. VGTimes copy (opened, 5.98 kB, 23 Sep): 6 downloads, 0 comments, **"game version compatibility: not stated"**. |
| What it changes | F1/Num1 player speed 1.0 to 5.0x; F2/Num2 world speed (time dilation 1.5x/2x/3x/0.5x); F3/Num3 infinite stamina; F4/Num0 reset. Banners on screen. |
| Lua APIs | **Unknown until downloaded** (not on this Mac); I cannot fill the support table. What the page text implies (not read from code): `RegisterKeyBind` x8 (registration works on PS5, `D:828-833`; delivery needs 8 pad chords but only 9 are free and ModMenu or others may also need some), property writes (**no console line proves any property Set**, section 5.1), a game-time-dilation call (`SetGlobalTimeDilation`, name present in the PS5 image; calling a native function from Lua is console-proven only for other calls), on-screen text. Whether it uses `RegisterHook` is **unknown**; if it does not, it is the preferred candidate. Folder layout `SpeedHackMod/Scripts/main.lua`. |
| Why it is attractive | Small, no menu, value changes are obvious (the world visibly speeds up). The names it needs exist in the PS5 build (section 6): `SetGlobalTimeDilation`, `MaxWalkSpeed`. |
| Cons | Stamina half likely inert like the one we are replacing (same `bStaminaEnabled` idea, "turns on automatically during boosted sprints"). Player speed may be overwritten by the game's own pace system (ModMenu comments at main.lua:2460 say the game keeps deriving walk speed from pace data; Comfort Camera's 1.0.4 changelog says a plain max-speed change was "over-strong" and was reworked to scale base speeds), so **world speed (F2) is the likelier feature to work**. Eight keys may not fit the free pad chords (PS5Pad.ini lists 9 free chord slots; Mac-only). |

### 4.3 SILENT HILL Townfall Comfort Camera

| | |
|---|---|
| Page | https://www.nexusmods.com/silenthilltownfall/mods/59 |
| Version evidence | Search snippet of the page: changelog up to **1.0.5** ("speed boost reworked to uniform x1.15 for walk, run, sneak; `SPEED_SCALE = 1.0` disables"; 1.0.4 "scaled base speeds instead of max speed"; 1.0.3 `TARGET_FOV`). Page text says "Comfort Camera v1.14 loaded", which does not match 1.0.5, so version labels are inconsistent. **Upload/update dates not obtained.** |
| What it changes | FOV locked at 110, no camera shake/head bob/sway, quick zoom-out after aiming, walk/run/sneak speed x1.15. Passive: no hotkeys needed, so it is the easiest to run blind. |
| Lua APIs | **Unknown until downloaded; support table not fillable.** Changelog implies property writes on camera and movement data (the Set family is unproven on console, section 5.1), probably a periodic loop (`LoopAsync` or `LoopInGameThreadWithDelay`, see section 5.1 for their status) and possibly `NotifyOnNewObject`. Hook use unknown. |
| Cons | Search-snippet evidence only; cannot tell whether it uses hooks. Two value changes at once. |

---

## 5. ModMenu v6 against our runtime (APIs, complete hook list, key clash, speed)

LuaMod.cpp = `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp` (current line numbers, Mac-only).
Call counts = whole-word occurrences in non-comment lines of ModMenu `main.lua` (sha `2ff1fa53...`), re-counted today.
`D:n` = line n of `evidence/launch-l26-B4-launch1-20261012/decoded/ue4ss.decoded.log` (console-proven, B4 launch).

### 5.1 Lua APIs

| API (ModMenu calls) | Registered at | PS5 status |
|---|---|---|
| `FindFirstOf` (6), `FindAllOf` (10), `StaticFindObject` (3) | :1869, :1920, :1758 | **console-proven at startup.** Lua-level checks `lua-ffo Player PASS`, `lua-ffo ItemDatab PASS`, `lua-sfo /Script/Engine PASS`, `lua-sfo /Script/UMG.Default_ PASS`, `lua-fao Player PASS` (`D:926-930`); native-level `sfo`/`ffo`/`fao` PASS (`D:647-657`). Timing: the last completed self-test (1e49188b, 2026-10-11) put FindFirstOf/FindAllOf at **78 ms, SLOW** (B4 report section 4); B4 itself took no timings. |
| `StaticConstructObject` (2) | :2516 | **console-proven**: non-null returns (`D:995-996`, and the CheatManager object `D:867`). |
| `RegisterKeyBind` (4: Backspace, Up, Down, Enter) | :2203 | Registration of other keys console-proven (keys 118, 38, 40, 13, 37, 39 returned, `D:828-833`). One keybind callback entered (`D:2410`, key 118), then the lock hang. **Backspace (key 8) is not in the keymap**: it needs a pad row from the generator (`PS5Pad.ini` has no BACKSPACE line, Mac-only). The keyboard cannot reach it (B4 report section 5). |
| `ExecuteInGameThread` (10) | :6077 | **queue and drain console-proven before the lock leak** (queued `D:809-810`, `D:817`, `D:915-916`; drained `D:923`, `D:935`, all 06:59:15-33). After 06:59:34 it hung behind the lock leak. B5 is expected to fix that (Mac-only). |
| `LoopInGameThreadWithDelay` (1 call, preferred) | :6835 | **Not directly shown by any log line.** API-PARITY-TABLE.md:377-378 lists it as unexercised. The minimap uses it and B4 report section 5 reads `D:994` as its first tick, but `D:994` itself is only a name-conversion line, so treat this as unproven. ModMenu falls back to `LoopAsync` (:3865, "PROVEN (spawner)" per API-PARITY-TABLE.md:77) only if the first call errors, not if it silently never ticks. |
| `ExecuteInGameThreadWithDelay` (18), `ExecuteWithDelay` (4), `ExecuteAsync` (6) | :6364, :3834, :3813 | Registered, **never exercised on console**. ModMenu checks `type(...) == "function"` and has fallbacks. |
| `NotifyOnNewObject` (5 registrations: lines 2037, 2650, 2663, 2677, 2689) | :2994 | Registration works for another mod (`D:839`, `/Script/Townfall.Interactable`). **No log line shows a callback firing.** Three of the five targets are Blueprint classes (SimonOS widget, two Motionless classes). |
| `RegisterHook` | :5946 | See 5.2. Native registration console-proven (`D:807`, `D:818`, ClientRestart). Blueprint registration refused. |
| `RegisterLoadMapPreHook` (3) / `RegisterLoadMapPostHook` (2) | :3124, :3151 | **Refused / never delivered** (`D:922`). |
| Property read | LuaUObject | Reads console-proven (e.g. ConsoleEnabler printing `ConsoleKey[1]: Tilde`, `D:887`). |
| Property write (`bCanBeDamaged`, nested pace-struct floats) | LuaUObject | **No console line shows any property Set.** API-PARITY-TABLE.md:373-375 lists object/bool Set as unexercised. The CheatManager mod prints "Enabled CheatManager" (`D:873`) after its own assignment, which is suggestive but not a write line. God Mode and speed both depend on this unproven family (Mac-only). |
| `RegisterCustomProperty` (2 calls, main.lua:155 and 161) with `PropertyTypes` | :2651 (`PropertyTypes` global :1346) | **Unexercised on console, and it uses hard-coded offsets** `TMM_Health` = +0x67C and `TMM_IsPlayerMarker` = +0x650 on the native TownfallCharacter class. These came from the PC build. **No second source confirms them for the PS5 image** (the project's own rule: wrong values crash; missing values fail closed). Used by God Mode's health write (`writeHealth`, 187; `applyGodState`, 193; `applyHealing`, 219), the healing multiplier and One Hit Kill. The project already skips a mod that hard-codes an offset (ActorDumper 0x98, API-PARITY-TABLE.md:68, agent-FO-mod-ladder.md:26); the earlier census of ModMenu (MOD-CENSUS.md:110-132) did not list this call. |
| `ForEachUObject` (1, one-time fallback), `ForEachProperty` (1) | :2918 | **Unexercised** on console; ForEachUObject failed its self-test last time (FAIL 164 ms, B4 report section 4), B3 fixed it but B3 was not exercised (B4 report section 7). |
| `FText`, `FName` | globals set at :7976 (FText), :7967 (FName) | FName conversion console-proven (many `[LUA-FN]` decodes, e.g. `D:866-880`). FText: API-PARITY-TABLE.md:111 says proven via SetText (not re-checked in B4 lines). |
| `require("UEHelpers")` | shared/UEHelpers | **No log line found.** ModMenu wraps it in `pcall` and falls back to `FindFirstOf("PlayerController")` (main.lua:34-48), so a failure costs speed, not function. |

### 5.2 Complete hook inventory (ModMenu v6, `main.lua`)

Every `RegisterHook` site is inside `pcall` (verified for all 11 lines, see 5.3). `BP` = Blueprint function (path starts `/Game/`), refused on PS5 today
(`HookProcessInternal=0` and `HookProcessLocalScriptFunction=0` in `UE4SS-settings.ini`; B4 report section 5; the B4 log has only two "Registered" lines in the whole session, `D:807`, `D:818`, both for other mods, so no Blueprint hook registered - Mac-only reading plus console absence).
`native` = `/Script/...` function (registration works, `D:807`, `D:818`; **callback delivery and return-value override never shown**).

| # | Target | Kind | main.lua | When it is tried | Status on PS5 today | What it is for | Data-only route? (Mac-only opinion) |
|---|---|---|---|---|---|---|---|
| 1-5 | `BP_TownfallPlayerGaitComponent_C:SetPace`, `:ChangePace`, `:OnWorldMarkupPaceChanged`, `:UseAimingPace`, `:EndAimingPace` | BP | 2587-2606 | **At startup** (3193), again +3 s and +7 s (3195-3196), and on every speed change (2877) | **Refused** (up to 15 refused attempts per run) | Re-apply the speed multiplier when the game recomputes pace | A `UFunction::Func` pointer swap on the Blueprint function objects is the candidate route; whether it works for Blueprint (bytecode) functions is for the refused-hooks track to decide. Not needed for the base speed feature (tick re-applies every 500 ms). |
| 6 | `BP_TestAIAgent_C:SpawnGroundMotionlessCollider` | BP | 1163 helper, call at 1175 | Invisibility toggle, retried every 5 s while on (2386, 2636, 2832) | Refused | Disable a spawned AI collider | Same Func-swap route. Feature is enemy-AI, optional. |
| 7 | `BP_MotionlessTrigger_C:OnBoxBeginOverlap` | BP | 1163 helper, call at 1189 | Invisibility toggle | Refused | Calm "Motionless" enemy trigger | Same. |
| 8 | `BP_TheMotionless_C:BndEvt__...ComponentBeginOverlapSignature__DelegateSignature` | BP | 1163 helper, call at 1198 | Invisibility toggle | Refused | Same, standalone actor | Same. |
| 9 | `BTT_WidowAttackDecision_C:ReceiveExecuteAI` | BP | 1234 | Invisibility toggle | Refused | Clear queued Widow attack | Same. |
| 10 | `WBP_SimonOS_C:Construct` | BP | 1933 | Infinite Items toggle, **retried every 20 ticks (about 2 s) for as long as the feature is on** | Refused (each retry is another refused attempt) | Learn when the in-game menu widget is built | NotifyOnNewObject (line 2037) already covers this widget, so the hook is redundant if that callback fires. |
| 11 | `/Script/Townfall.TownfallPlayerCharacter:IsCheatHiddenFromAIPerception` | native | 1250 | Invisibility toggle | Registration expected to work; **post-hook return override `true` never exercised** | Make enemies not see the player | Native Func swap is the existing mechanism. |
| 12 | `/Script/Townfall.TownfallInventoryComponent:GetAmmoCountForItemID` | native | 1539 | **Automatically at +3 s** (2451), and on Infinite Ammo toggle | Registration expected to work; **return override (99) never exercised** | Infinite ammo | Native Func swap. |
| 13 | `.../TownfallInventoryComponent:GetWeaponDurability` | native | 1559 | **Automatically at +3 s**, and on toggle | same, return override 100.0 | Infinite durability | Native Func swap. |
| 14 | `/Script/Townfall.TownfallPlayerCharacter:OnPlayerThrewObject` | native | 2038 | **At load** | Registration expected to work; callback never shown | Re-check inventory after a throw | Native Func swap. |
| 15 | `/Script/Engine.PlayerController:ClientRestart` | native | 2734 | **At load** | **Registration console-proven for other mods** (`D:807`, `D:818`); ModMenu's own would be a third | Re-apply state after respawn | Already data-only. |
| 16-19 | `ExponentialHeightFogComponent:SetFogDensity`, `:SetSecondFogDensity`, `:SetVolumetricFogExtinctionScale`, `:SetVolumetricFog` | native | 2112-2149 | **Never in v0.66**: only installed for fog mode "Less" (2309), but the menu only offers "Normal" and "Off" (`fogModes`, line 23) | Dead code in this file (Mac-only) | Reduce fog; uses `Value:set()` argument override | n/a |
| - | `RegisterLoadMapPreHook` x3 (2039, 2753, 3153), `RegisterLoadMapPostHook` x2 (2045, 2773) | LoadMap | as listed | **At load** | **Refused, callbacks never delivered** (`D:922`) | State reset on map change; remove the menu widget on map change | The refused-hook track should say. Loss means stale widget/state after a level change (not tested). |

Counts: 11 `RegisterHook` call lines, 19 distinct hook targets (15 reachable, 4 dead); **10 Blueprint targets refused today** (5 at startup without any input, 5 only after the Invisibility or Infinite Items toggle); 5 map-load callbacks never delivered.
For the minimap's 3 and the self-test's 12 refused hooks, see the separate refused-hooks track (`round8-prep/refused-hooks/`); this table is ModMenu's contribution to that queue.

What happens with only Speed on (the feature I would test first): the Blueprint hooks that remain are the 5 startup pace hooks (tried 3 times); native startup hooks #12-15 are also tried. None of that should block Speed, because it works by direct property writes into the game's pace data plus a 500 ms tick (Mac-only reading, lines 2476-2562). The pace field names carry Blueprint GUID suffixes and are **not checkable** from the files we have, so a wrong name just means "no effect" (reads return nil inside `pcall`, so it fails closed). That is unproven until a launch.

God Mode is two writes: `bCanBeDamaged = false` by reflection (fails closed if the name is wrong; `bCanBeDamaged` is present as a string in the PS5 image, section 6), plus a health write at the **hard-coded offset** above (a wrong offset would write 100.0 into the wrong 4 bytes of the player object). So **God Mode should not be switched on until the +0x67C offset is confirmed from a second source** (for example read from the live TownfallCharacter class property list on the PS5, or from another independent layout source). Healing and One Hit Kill share that offset.

### 5.3 The pcall question is settled, and the parity table is wrong about it

- All 11 `RegisterHook` lines are inside `pcall` (1163 helper, 1234 at 1230-1235, 1250 at 1248-1252, 1539 at 1537-1546, 1559 at 1557-1566, 1933 `pcall(RegisterHook, ...)`, 2038 `pcall(RegisterHook, ...)`, 2120 at 2118, 2143 at 2142, 2602 at 2598, 2734 at 2733). All 5 LoadMap lines and all 5 NotifyOnNewObject lines are in `pcall` too. Re-read today (Mac-only).
- `API-PARITY-TABLE.md:370-371` says "ModMenu has UNGUARDED RegisterHook sites (:1234/:1250/:1539+) where an install-time failure errors the whole script". **That is wrong for the v6 file on disk** (same line numbers, but they are guarded). I cannot edit that file (outside my folder); it should be corrected on the fix queue.
- The guard does not remove the lock risk: before B5, a refused hook still leaks the lock even though `pcall` catches the Lua error (B4 report section 5, Mac-only code reading). After B5/B7 a caught refusal unwinds properly.
- `RegisterKeyBind` (4) and `RegisterLoadMap` are not required to be guarded; the four keybind calls at 3175-3189 are not in `pcall` but registration of other keys is console-proven (`D:828-833`).

### 5.4 Key clash with the spawner

- ModMenu binds BACKSPACE (open), UP_ARROW, DOWN_ARROW, RETURN (`main.lua:3175-3189`). TownfallItemSpawner binds F7 (open), UP_ARROW, DOWN_ARROW, RETURN, LEFT_ARROW, RIGHT_ARROW (`TownfallItemSpawner/Scripts/main.lua:694-722`, deployed copy in the morning spool).
- `PS5Pad.ini` (morning spool, generated) serves UP/DOWN/CROSS/LEFT/RIGHT from the same pinned rows (`UP = UP_ARROW`, `DOWN = DOWN_ARROW`, `CROSS = RETURN`), and L3+R3 = F7. It already lists `UP_ARROW`, `DOWN_ARROW`, `RETURN` as "keys already served by pinned rows" for the spawner, so ModMenu would share them and add only one new row (BACKSPACE).
- Each callback starts with `if not menuOpen then return end` (ModMenu 3179, 3183, 3187; spawner 699, 704, 709), each against its **own** `menuOpen`. So a d-pad press while neither menu is open does nothing in either mod. If **both menus are open**, one press moves both selections and Cross activates a row in both (and also reaches the game, because UE4SS keybinds are non-exclusive: PS5Pad.ini header). Rule for the test: open one menu at a time; close the spawner (L3+R3) before opening ModMenu. The earlier mod-ladder plan already says this (agent-FO-mod-ladder.md:40-41: "RUN-7 ModMenu is RegisterHook-saturated and MUST run spawner-free (nav co-fire)").
- Backspace needs a free chord from the generator. 9 free slots remain (`TOUCH_PAD+L1`, `TOUCH_PAD+R1`, `TOUCH_PAD+L2`, `TOUCH_PAD+R2`, `L1+R2`, `L1+L2`, `R1+L2`, `R1+R2`, `L2+R2`). The user would have to be told which chord opens ModMenu. Not yet chosen.

### 5.5 Performance (acceptance criterion: no stutter)

Mac-only reading; nothing measured.
- Idle (all features off): the 100 ms game-thread tick (`gameplayTick`, 2364) returns at the first lines: `II_Update` exits at once when Infinite Items/Durability are off (1938-1943), and the rest exits at 2408 when God Mode, healing, One Hit Kill and Fog are off. Cost is a few counters per tick.
- God Mode on: one player lookup (`UEHelpers.GetPlayerController` or `FindFirstOf("PlayerController")`) every 500 ms (2400, 2410).
- Speed on: a cached-object property pass every 500 ms (2551-2558); if the cached player or gait object is invalid it searches again every 2 s.
- Infinite Items / Invisibility / One Hit Kill add `FindAllOf` and inventory scans (10 `FindAllOf` call sites in the file). Avoid them in the first test.
- Risk: `FindFirstOf`/`FindAllOf` were 78 ms SLOW in the last completed self-test, and `LoopInGameThreadWithDelay` is unproven. A 78 ms lookup twice a second is a visible hitch, so "no stutter" must be judged from the B6 `[PERF]` lines, not assumed.

---

## 6. Do the names these mods touch exist in the PS5 build? (Mac-only, one source)

I searched the saved PS5 executable copy (`.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin`, sha `3b6cfad4...`,
same image as the launch) and the script-object table (`.../townfall-global-scriptobjects-1/global.ucas`) for the exact strings.
This proves the name is present in the game's data, not that the mod works. It is one source only, so it is not enough to ship anything.

| Name | eboot hits | global.ucas hits | Used by |
|---|---|---|---|
| `bStaminaEnabled` | 1 | 0 | InfiniteStamina (the one being replaced) |
| `bCanBeDamaged` | 1 | 0 | ModMenu God Mode |
| `MaxWalkSpeed` | 2 | 0 | speed mods |
| `SetGlobalTimeDilation` | 1 | 1 | world-speed mods |
| `CustomTimeDilation` | 3 | 2 | speed mods |
| `GetAmmoCountForItemID` / `GetWeaponDurability` | 1 / 1 | 1 / 1 | ModMenu ammo, durability (native functions) |
| `OnPlayerThrewObject` | 1 | 1 | ModMenu, Infinite Items style mods |
| `TownfallPlayerCharacter`, `TownfallEnemyCharacter` | 5, 2 | 6, 2 | most mods |
| `TFPlayerGaitComponent` | 0 | 2 | InfiniteStamina, ModMenu |
| `Pace_Target`, GUID-suffixed pace field names | 0 | 0 | ModMenu speed; these live in Blueprint assets, so **not checkable here** |

So the native names used by the god-mode (`bCanBeDamaged`) and world-speed paths exist on the PS5 build. The Blueprint-only parts (the pace struct fields)
are not checkable from these files. **The ModMenu health offsets (+0x67C health, +0x650 marker) are not names at all and cannot be checked this way**; they need a separate layout source (section 5.1).

---

## 7. What the user must do

**Nothing is needed for InfiniteStamina** (original already here, section 2).

For the replacement, choose one:

- **Option A (no download):** approve TownfallModMenu v6 as the replacement **for B5/B6 or later only**. I then build the
  expected-behavior checklist, and the pad-key row for Backspace goes through the normal keymap generator (one of the 9 free chords). The test plan: Speed only first; God Mode stays off until the +0x67C offset has a second source; one menu open at a time (not together with the spawner). You should know it makes 10 refused Blueprint-hook attempts on its own at startup (that is useful for the refused-hook list but is not a quiet test).
- **Option B (small download):** in a logged-in browser, download **only** this file, do not unzip or edit it, and tell Claude the path:
  - Nexus mod 10, https://www.nexusmods.com/silenthilltownfall/mods/10?tab=files , file **`SilentHillTownfall SpeedHack ALL Character` (v1.1, 6 KB)**.
  - The browser saves it into `/WORKSPACE/Downloads/` with a name like `... 10 1.1 <date> <id>.zip`; just say "it is in Downloads".
- **Optional, 2 minutes, makes the choice evidence-based:** on the Nexus page of the chosen mod, open the **Posts / Comments** tab and the **Files** tab,
  and copy to Claude (1) the "last updated" date and (2) any comment dated **8 Oct 2026 or later** (after patch 1.4.0) saying it works or is broken.
  Nexus blocks my tools, so only a logged-in person can read this.
- **Please also read the PS5 main-menu version** (bottom-right corner, "v1.x.xxxxxx"). `v1.5.154194` means patch 1.4.0 is installed; `v1.4.153829` or lower means older.
  I expect the older one (section 1), and I cannot confirm it without the console.

After any download I will, on the Mac only: hash it, list the Lua APIs it calls, compare with section 5, and report before any console action.

---

## 8. Other candidates found (not recommended now)

| Mod | Type | Why not (or why later) |
|---|---|---|
| Infinite Items (AeonGreyh, Nexus [mod 9](https://www.nexusmods.com/silenthilltownfall/mods/9), v1.1.0) | UE4SS, ~2 KB | Likely hook-based (ModMenu's equivalent hooks `OnPlayerThrewObject` and inventory functions), so it depends on native hooks. Good for the refused-hook list. 1.1.0 added throwables (search summary). Source not seen. |
| Infinite Ammo ([mod 13](https://www.nexusmods.com/silenthilltownfall/mods/13)), Infinite Durability ([mod 7](https://www.nexusmods.com/silenthilltownfall/mods/7)), Block Cancels Attacks ([mod 14](https://www.nexusmods.com/silenthilltownfall/mods/14)), all AeonGreyh, v1 | UE4SS, 1.6-2 KB | Same: probably hooks. VGTimes copies (opened) show no game version, 0 comments.  Block Cancels is an animation change, not a value. |
| Kill Count Always 0 ([Nexus mod 58](https://www.nexusmods.com/silenthilltownfall/mods/58), ZArkadia, 24 Sep) and View Kill Count LIVE ([mod 60](https://www.nexusmods.com/silenthilltownfall/mods/60), F4) | UE4SS | Passive and tiny, but the effect is only visible via a log or the companion mod, and the mechanism is unseen. A decent no-input option later. |
| The Weight Stops Stalking ([mod 84](https://www.nexusmods.com/silenthilltownfall/mods/84), 29 Sep) | UE4SS, 18 kB | AI mod; patch 1.4.0 changed The Weight's AI, so it is the most patch-exposed. Not a value change. |
| Show Enemy Health Bars (ZArkadia, 4 Oct on VGTimes) | UE4SS, 19 kB | Display only, not a value change; newest upload (after 1.3.1, before 1.4.0). |
| Save Anywhere ([mod 109](https://www.nexusmods.com/silenthilltownfall/mods/109), DavidWallacee), Automatically Unlock Password Locks | UE4SS | Not value-changing. Same author as the minimap that already works on PS5 (B4 report section 3), a mild plus. |
| No Clip ([mod 111](https://www.nexusmods.com/silenthilltownfall/mods/111), F8, WASD) | UE4SS Lua (search summary) | Needs WASD/keyboard movement keys; no keyboard source on PS5. Park with input work. |
| Mod Menu and Ultimate Trainer ([mod 112](https://www.nexusmods.com/silenthilltownfall/mods/112)) | Injected DLL | Not a Lua mod. Out of scope. |
| More Powerful Weapons ([mod 25](https://www.nexusmods.com/silenthilltownfall/mods/25)), Healing Items Recover More ([mod 26](https://www.nexusmods.com/silenthilltownfall/mods/26)), Gameplay Tweaks / PlayerMovementSpeed 2x ([mod 28](https://www.nexusmods.com/silenthilltownfall/mods/28)), Triple Capacity Gun, High Durability Melee | **pak / asset** (VGTimes pages say "pak-based, not UE4SS"; mod 28 is in Downloads, see section 9) | Excluded by the "Lua mod only" requirement. They belong to the later pak work (#7, #8, #12). |
| Cheat Engine tables, trainers | Not UE4SS | Out of scope. |

---

## 9. Local inventory of Townfall mod archives (read-only; hashes computed today)

| File | sha256 | Type | Role |
|---|---|---|---|
| `~/Downloads/Infinite Stamina 19 1 2026-09-22T19-18Z OPrGe3tIa.zip` | 61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5 | UE4SS Lua | the original being replaced |
| `~/Downloads/TownfallModMenu 71 6 2026-09-30T03-22Z AW6IuGRZU.zip` | a9257d8e23c1a377d9acddee36ef82c624aece7920fa6f92a41e21e0690456ca | UE4SS Lua | recommended 4.1 |
| `~/Downloads/TownfallMiniMap 3.1 39 3.1 2026-09-28T13-48Z kMsyPwQHz.zip` | cf3c353b4e8ab0d49214ceaeae5a8989cf58d1b2ce5758673a360d2e463df377 | UE4SS Lua | in test set |
| `~/Downloads/TownfallItemSpawner V1.0 95 1 2026-09-26T16-33Z pZAtDj5g8.zip` | a9f7ed004dd053e02f6206d8ed6886174f9b5aaecf55244447a2c5fb9d6fa7fb | UE4SS Lua | in test set |
| `~/Downloads/UE4SS for SILENT HILL Townfall 4 1 2026-09-22T04-56Z JNCU6WyIQ.zip` | 58a6b9d096c30b357f50d4becf74b6d21623f0582a05b8cd250c2c58c5119194 | UE4SS runtime + stock mods | reference |
| `~/Downloads/PlayerMovementSpeed-2x-P-v.0.1 28 0.1 2026-09-23T03-17Z R1VxEpgZ0.zip` (Nexus 28, file 42) | 3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad | **pak/utoc/ucas** | not a Lua mod |
| `~/Downloads/Release 1.0 56 1 2026-09-24T14-35Z XaqN9sMTf.zip` | 27db7e1b6ea2bf5b574a68a1fa6d7d89ccd5795fd11ac98b4549c2e69132e06e | pak ("Universal Performance") + Engine.ini | not a Lua mod |
| `~/Downloads/Leon Townfall 2K v1.1 107 ... UX5JlhkeF.zip` | 661aabf0e6a1cb91e9164501108b5d19db13c2ca21cee651f676e0706f3feef9 | pak/utoc/ucas | not a Lua mod |
| `~/Downloads/Leon Townfall 4K v1.1 107 ... bt3zF8ndX.zip` | 17f04e8f03592c2c2c07061deb3e75287c9b1bcb6e590fd80cf432477dcac87b | pak/utoc/ucas | not a Lua mod |
| `~/Downloads/Third-person view and character beautification 62 1 2026-09-24T18-35Z iFcgTCNEt.rar` | ac511becb74c5e7d874806051b56e314e68896e6d29b0d510d5394412890c022 | RAR with pak/utoc/ucas (`1_999_P`, per `.superpowers/native-dependency-spike/townfall-62-rar-intake-1/intake-profile.json`) | not a Lua mod |
| `~/Downloads/Engine 110 1 2026-09-29T21-36Z iFcgTCNBr.rar` | 24ea77e211aa95c7800035de62fde127fd514454c3f7cb9957a672a038afb906 | RAR, not opened by me (name suggests an Engine.ini tweak; unchecked) | not examined |

No SpeedHackMod, Comfort Camera, Infinite Items/Ammo/Durability or Kill Count archive exists locally (searched Downloads, `outputs/`, `.superpowers/`).
Mod-menu extraction used for reading was made in the scratchpad, not in the repo.

---

## 10. Uncertain, and how to settle each

1. **No candidate is proven to work on the current version.** Needs a logged-in read of the Nexus Posts tab (section 7). Until then it is "unproven", not "works".
2. **PS5 game version is inferred, not read.** Executable identical to the 2 Oct copy (console-proven) implies pre-1.4.0, but a patch could in principle leave the executable unchanged. The menu version number settles it.
3. **Dates of the Nexus uploads come from search snippets, earlier notes and zip file names.** Nexus returned 403 to every direct fetch. VGTimes dates are the mirror's upload time and can be later than Nexus.
4. **Lua APIs and hook use of SpeedHackMod and Comfort Camera are unknown** until their source is read. Only ModMenu has a complete API and hook list (section 5). Settling this needs the user to download one file.
5. **(Settled in this revision.)** Every ModMenu `RegisterHook` site is inside `pcall`; the parity table entry saying otherwise (API-PARITY-TABLE.md:370-371) is wrong for this file (section 5.3). What stays open is not the guard but the pre-B5 lock leak, which `pcall` does not prevent.
6. **Return-value override in hooks** (ModMenu ammo hook returns 99 from the post-hook, durability 100.0, invisibility `true`) depends on the `__OriginalReturnValue` contract, which is bound but not exercised on console. ModMenu does not use `__OriginalReturnValue` itself (0 occurrences); it relies on a plain `return` in the post callback.
7. **Name-presence evidence (section 6) is one source and Mac-only.** Presence of a string does not prove the property is reachable the way the mod uses it.
8. **ModMenu hard-coded offsets (+0x67C, +0x650) have no PS5 source.** They were chosen by the mod author for the PC build. Settle by reading the live TownfallCharacter property list on the PS5 (or another independent layout source) before God Mode, Healing or One Hit Kill are ever enabled.
9. **Stale state after a level change:** ModMenu's map-load reset hooks are never delivered on PS5 (`D:922`). I did not check what breaks (menu widget handles, cached pace/player objects); the code re-validates with `IsValid` in most places. Mac-only guess, untested.
10. **Property Set family, `ExecuteInGameThreadWithDelay`, `NotifyOnNewObject` callbacks, `ForEachProperty`, `RegisterCustomProperty` and `LoopInGameThreadWithDelay`** are all unexercised on console (section 5.1). ModMenu needs most of them, so a failed launch will have several possible causes; the B6 diagnostics should be read first.
11. **Any Nexus mod can be re-uploaded.** The local ModMenu file is v6 downloaded 1 Oct; the live Nexus page may now hold a newer version. Compare the file_id (266) on the Files tab.
12. **Download counts and rankings from VGTimes** (e.g. ModMenu "about 295") were seen on earlier reads and not re-verified today. They are not used for any decision.

---

## 11. Sources

Web (opened unless noted): [Game Rant 1.4.0](https://gamerant.com/silent-hill-townfall-update-patch-notes/), [The Click 1.4.0](https://www.theclick.gg/silent-hill-townfall-update-1-4-0/),
[TwistedVoxel 1.4.0](https://twistedvoxel.com/silent-hill-townfall-update-1-4-0/), [TwistedVoxel 1.3.1](https://twistedvoxel.com/silent-hill-townfall-patch-1-3-1/),
[PSU 1.4.0](https://www.psu.com/news/silent-hill-townfall-update-1-4-0-brings-key-changes-to-the-weight-including-improved-ai-removing-instant-death-grab-attack-full-patch-notes-listed/),
Steam news [1.3.1](https://store.steampowered.com/news/app/1636440/view/685266627108798510) and [1.4.0](https://store.steampowered.com/news/app/1636440/view/685267261521396615) (title only),
VGTimes mod pages: [Townfall file list](https://vgtimes.com/games/silent-hill-townfall/files/), [All-time top](https://vgtimes.com/games/silent-hill-townfall/files/top-files/),
[More Speed and Infinite Stamina](https://vgtimes.com/games/silent-hill-townfall/files/98305-more-speed-and-infinite-stamina.html), [Mod Menu](https://vgtimes.com/games/silent-hill-townfall/files/98440-mod-menu-god-mode-infinite-ammo-ohk-fog.html),
[Infinite Items](https://vgtimes.com/games/silent-hill-townfall/files/98301-.html), [Infinite Ammo](https://vgtimes.com/games/silent-hill-townfall/files/98300-.html), [Infinite Durability](https://vgtimes.com/games/silent-hill-townfall/files/98299-.html),
[Infinite Stamina](https://vgtimes.com/games/silent-hill-townfall/files/98302-.html), [Kill Count](https://vgtimes.com/games/silent-hill-townfall/files/98442-kill-count-always-0-best-ending.html),
[Weight Stops Stalking](https://vgtimes.com/games/silent-hill-townfall/files/98528-the-weight-stops-stalking-freeze.html), [More Powerful Weapons (pak)](https://vgtimes.com/games/silent-hill-townfall/files/98406-more-powerful-weapons-increased-weapon.html),
[Healing Items (pak)](https://vgtimes.com/games/silent-hill-townfall/files/98409-healing-items-recover-more-increased.html), [UE4SS (4 Oct)](https://vgtimes.com/games/silent-hill-townfall/files/98689-ue4ss.html).
Nexus pages (search snippets only, direct fetch blocked): mods 4, 7, 9, 10, 13, 14, 19, 25, 26, 28, 58, 59, 60, 71, 84, 109, 111, 112.
Local: `outputs/2026-10-08-l25-prep/evidence/launch-l26-B4-launch1-20261012/REPORT-...md` and `decoded/ue4ss.decoded.log`, `outputs/2026-10-08-l25-prep/API-PARITY-TABLE.md`,
`outputs/2026-10-04-zcode-team/worker-modreqs/MOD-CENSUS.md`, `.superpowers/native-observations/townfall-mod-candidates-2026-10-01.md`, `.superpowers/native-observations/nexus-real-mods-2026-10-01/*receipt.json`,
`outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/LuaMod.cpp`, `outputs/2026-10-08-l25-prep/agent-FO-mod-ladder.md`, `outputs/2026-10-08-l25-prep/evidence/morning-readonly-20261012T114008Z/spool/rows/ue4ss/PS5Pad.ini` and `.../Mods/TownfallItemSpawner/Scripts/main.lua`, `outputs/2026-10-08-l25-prep/round7-staging/b5-lock-raise/` (B5/B6, read-only), and ModMenu `main.lua` extracted from the Downloads zip into the session scratchpad (sha `2ff1fa53...`).
