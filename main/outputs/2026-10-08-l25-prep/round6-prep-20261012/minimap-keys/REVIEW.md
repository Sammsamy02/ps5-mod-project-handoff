# REVIEW — minimap-keys track, round 2 (adversarial, 2026-10-12)

All Mac-only. No console contact, no console logs, no PRX builds, no git
changes. Only this file was changed. The checker was re-run with every file
write blocked (check-result.json not touched). The round-1 review is kept
unchanged at the bottom.

## Verdict: NOT refuted

The answer holds. The minimap needs no button press to appear. Its 8 keys
(F5, F6, Insert, Delete, PageUp, PageDown, Home, End) can't be reached by any
PS5Pad.ini change. The right change for this launch is none: PS5Pad.ini stays
1736 B / `957d3e25…ed453` and its loadlist row stays at 1736.

## What I re-checked (Mac-only)

| Claim | Result |
|---|---|
| Pinned main.lua 196,743 B `d8f8d64f…c920f`; PS5Pad.ini 1736 B `957d3e25…` (2 local copies) | match rowcheck-pins.json |
| 0 RegisterKeyBind / Key.* / ModifierKey.*; engine key polls at 3271-3272, 3972-3973, 3994, 4043 | confirmed by grep (only RegisterHook 515 and NotifyOnNewObject 4248, neither is input) |
| PS5PadSource.cpp:164 `allow && subscribed[b.key]`; Handler.cpp:155-162 callbacks only, 191/208 subscriptions | confirmed; source shas 6a0633ca / 16fcfe49 match both l26-A pass1 (0f5e2745) and pass2 (e6234ec7) receipts |
| B4 sha d044cf0b from 2 sources | BUILD-LEDGER line 9 and the frozen `.prx.sha256` agree |
| Call graph into saveSettings (3427/3435/3443 onKey, 3535 M.changed, 3830 togglePause, 4214 first show) | confirmed by grep |
| Stutter mechanism: pollKeys fail -> `S.pc=nil` (4045) -> FindAllOf next tick (305-307) | confirmed; self-test part5.lua (pin 9ba4300f) times one FindAllOf("PlayerController") |
| Shim: write marker 851-855, read marker 871-875, read order mod-root -> sandbox -> spill | confirmed |
| Candidate 1669 B `6f81b8e2…`; loadlist stays 1772 B, new sha `5483769c…` | recomputed: identical |
| Checker, real tree | PASS, 9 checks, 66,171 items (636 from files), exit 0, reproduced |
| Negative runs | all 9 FAIL with 0 items, exit 1 (from the saved output) |
| Writes outside the track folder | none found (no outside.json, no scratchpad leftovers, no /tmp paths, no console IP) |

## Problems found (all minor, none change the answer)

1. **The "full" list of show gates leaves out the first gate.** The tick
   only runs if `LoopInGameThreadWithDelay(TICK_MS, safeTick)` succeeds
   (main.lua:4267-4270, after `loadSettings()` at 4266). On PS5 that call
   throws when the marshal pump is armed but its drain is dead, or when no
   pump is available (runtime LuaMod.cpp:6862-6872 and the later rungs). If it
   throws, the script stops: no tick, no map, and no settings.txt write. The
   driver rule ("no marker proves nothing") still covers this, but the list
   should start with this gate.
2. **The show formula is shortened.** The code is
   `S.enabled and (covered and S.hasSnapshot or S.blank or W.active) and hudOk`
   (main.lua:4188). README writes `snapshot`, without `covered and`.
3. **The candidate file header names the wrong build.** It says "current
   runtime (1e49188b, B1-B4)". The README and the checker tie everything to B4
   `d044cf0b`. This is only a comment in a do-not-upload file.
4. **"Cannot work on PS5 with today's runtime" is a little too absolute.** It
   is true for the controller and for any PS5Pad.ini change. Whether a real
   USB keyboard reaches the game's own key state on PS5 was not looked at.
   That is unknown (Mac-only), and testing it would be a console action.
5. **Note, not the worker's fault:** the self-test has no IsInputKeyDown probe.
   So gate 2, the biggest risk, has no direct signal in the log except the
   one-time settings.txt write marker.

Rule check: no console contact, no mod edits (sha unchanged), no runtime edits,
no Epic source, claims labelled Mac-only, every gate fails on zero items. The
LuaPathShim.cpp:211 "MiniMap" comment the worker flagged is real; it's only a
comment.

Reviewer's own slip: I briefly copied the round-1 review text to the
session scratchpad to keep it, then deleted it. Nothing else was written.

---

# REVIEW — minimap-keys track (adversarial, 2026-10-12)

All Mac-only. No console contact, no PRX builds, no git changes. The only file
written is this one. The worker's checker was re-run with every file write
switched off (check-result.json unchanged).

## Verdict: NOT refuted

The main answer holds. The minimap is built to appear with no button press.
Its 8 keys (F5, F6, Insert, Delete, PageUp, PageDown, Home, End) cannot be
reached by any PS5Pad.ini change. The right change for this launch is none:
PS5Pad.ini stays 1736 B, `957d3e25…ed453`, and the loadlist row stays the same.

## What I re-checked (all Mac-only)

| Claim | Result |
|---|---|
| main.lua copy is 196,743 B, sha `d8f8d64f…c920f`; PS5Pad.ini is 1736 B, sha `957d3e25…` | match the rowcheck-pins.json entries |
| 0 RegisterKeyBind / Key.* calls in the minimap; it reads engine key state at 3271/3272/3972/3973/3994/4043 | confirmed by grep |
| What each key does (onKey 3411-3445), Tab (3263/3279), menu keys (3958-3959), settings.txt holds no keys (248-302) | confirmed |
| Shows by default (78, 149, 1996, 4181-4188), help box for 140 ticks × 50 ms (14, 4213) | confirmed |
| Pad source: chord fires at 154; event only if `allow && subscribed[b.key]` at 164; push at 168 | confirmed in PS5PadSource.cpp sha 6a0633ca |
| Handler: subscriptions set only at 191/208; only key-bind callbacks run (113, 155, 162) | confirmed in Handler.cpp sha 16fcfe49 |
| Parser rules (PS5PadConfig.cpp 42-46, 55, 69, 77, 91, 117, 119) and KeyDef names | confirmed; the mirror parser matches the real one |
| 1147 legal chords / 21 within the policy buttons / 9 free pairs10 slots | recomputed by hand: same numbers |
| FV1 slot placement of the 8 future rows | recomputed independently: same 8 chords, TOUCH_PAD+R2 left free |
| Candidate 1669 B, sha `6f81b8e2…`; loadlist stays 1772 B, new sha `5483769c…` | recomputed: identical |
| keymap-gen.py --check (7 mods incl. minimap, sha-verified copies) | pinned: 0 violations / 0 warnings; candidate: 0 violations / 8 orphan warnings (reproduced) |
| Checker on the real tree | PASS, 8 checks, 66,096 items, exit 0 (reproduced) |
| Checker with a missing root | FAIL, exit 1 (reproduced) |
| The B4 PRX (d044cf0b) has the parser and the pad poll | symbols.json: parse_ps5_pad_config ×7, PS5PadSource::poll ×1 |
| No engine-input injection in the runtime | my wider scan of the l26-A composed sources plus the l26 candidate (431 files) found only NativeLayoutWidths "InputComponent" layout rows, no input calls |

## Problems found (all minor, none change the answer)

1. **Pins checked against the older receipt.** The checker compares pad and
   input units with the l25-A COMPOSITION-RECEIPT. The PRX for this launch (B4,
   d044cf0b) was built from the l26-A receipts
   (l26-package-A/BUILD-INPUTS.json:20,25). The pad and input unit shas are the
   same in both, which I checked for pass1 and pass2, so nothing changes today.
   The checker should still pin against l26-A.
2. **Negative-run wording is too strong.** "Every check fails with zero items
   checked" is not quite true. Check "4 freechords" still counts 65,535 made-up
   button combinations with no input files. It fails only on its hard-coded
   "expected 9" test. Also, 65,535 of the 66,096 "items checked" are those
   made-up combinations, not files.
3. **"Shows whenever hudOk" is too simple.** The map also needs a
   PlayerController (304-310), `pawn.MapRenderComponent` (312-317), a built
   widget, and `S.enabled`. A missing risk: if IsInputKeyDown fails, `S.pc=nil`
   (4045), so `FindAllOf("PlayerController")` runs again every 50 ms. That
   could cause stutter, which matters for the "no stutter" acceptance test.
4. **Today's B4 launch-1 harvest may already settle the main risk.** This is
   a hint only. Mac-only, read from the harvest report text and not decoded by
   me. HARVEST-l26-B4-launch1-20261012.md:2752 shows a minimap settings.txt
   **write** at 07:01:25. With no minimap key reachable, the only main.lua path
   that writes without a key press is the first-show hint (4211-4214). That
   suggests the map was shown and IsInputKeyDown did not fail in that launch.
   That harvest was made at 12:04Z, while the worker was still running. Under
   the rules, this launch still needs its own decoded log lines.
5. **Small writes outside the allowed folder.** The worker left `emptyroot/` and
   `check-result.good.json` in the shared session scratchpad. They are
   temporary files, not evidence.

No rule breaks found: no console contact, no mod edits, no runtime code
changes, no Epic source, no console IP in the worker's files, and its claims
are labelled Mac-only.
