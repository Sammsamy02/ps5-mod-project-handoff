# L25 prep — UE4SS hook A/B experiment matrix (2026-10-08)

Mac-local prep only. Nothing here has been uploaded. Root handles all console/FTP work.

## Context

- Console file admission (modforge loadlist) enforces EXACT byte size for
  `ue4ss/UE4SS-settings.ini`: **1947 bytes** (size-only check, no hash).
- ORIGINAL l24 as-run (`/tmp/l24-settings-backup.ini`, 76 lines, sha256
  `b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`):
  `HookLoadMap=true` + `HookInitGameState=true` -> **CE-108255-1 crash on save load**.
- v2 (verified on console NOW): both hooks `false`, comment trimmed by 2 bytes
  (`; A-PI/FLAG-3 both keys REQUIRED 0`). v2 result is pending — this matrix
  branches on its outcome.
- Only free byte source in the file is the comment on line 60
  (36 chars in original). `true -> false` costs **+1 byte per flip**.
- File has no blank lines, no trailing whitespace, no CR, ends with `0x0a` (LF).
  All variants preserve: 76 lines, LF endings, trailing LF, only line 60 + the
  named hook line(s) differ from original (verified by diff + sha256).

## Variant set (all exactly 1947 bytes, all derived from ORIGINAL backup)

| file | HookLoadMap | HookInitGameState | byte ledger | sha256 |
|---|---|---|---|---|
| `settings-arm-loadmap-only.ini` | **false** | true  | +1 hook, -1 comment (`.` removed) | `f911e8eb47a7d3231d0e4e950daa9cad20ec5099f21678b0fbef845e7692bbb1` |
| `settings-arm-initgs-only.ini`  | true  | **false** | +1 hook, -1 comment (`.` removed) | `428f6e44e52a214c7ba2d596bc876b9b68085b55cf8e106d4b67e2ceebff9b34` |
| `settings-arm-both-on.ini` (control) | true | true | none — byte-identical to original | `b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2` |

Comment in both test arms: `; A-PI/FLAG-3: both keys REQUIRED 0` (35 chars,
trailing `.` dropped). Semicolon comment, INI parser audit of comment text
without trailing period is owned by another agent — assumed OK per brief.
Control arm exists to re-confirm the l24 crash reproduces on the current
console state (rules out a stateful/flaky crash) before trusting isolations.

## Observation procedure (identical for every run)

1. Launch game (via normal ploader/autoload path in scope for this session;
   one launch per boot cycle — respect the launch-test ladder, no rapid-fire).
2. Wait **60 s in the menu** — UE4SS inject + mod list load window.
3. **Load the same save** used in the l24 crash run.
4. Wait **30 s** past save-load.
5. Press **L3+R3** — check for mod menu (PS5Pad rebind; OPTIONS = game pause).
6. Clean-exit the game before any next launch (ShadowMount/kstuff recovery rule).

Record per run: CE-code if any, timestamp of crash vs load, L3+R3 result,
and grab `ue4ss` log off console after clean exit.

### Log markers expected (ue4ss log / console capture)

- UE4SS alive: usual init/mod-list lines appear during the 60 s menu window.
- Crash arm: log shows map/save-load beginning then **truncates mid-stream**
  (no clean shutdown lines), console shows **CE-108255-1**.
- No crash + mod starts: init + mod lines present, log survives save load,
  **L3+R3 opens the mod menu** after load.
- No crash + no mod start: init lines may appear but expected mod-start /
  Lua boot lines for the mod are absent or stop before map hook-up, log clean,
  **L3+R3 does nothing**.
- Exact UE4SS line wording: confirm against the actual log pulled from the v2
  run; do not treat the phrasings above as verbatim markers.

## Decision tree — branch on v2 (both hooks OFF) result

### v2 outcome A: crash on save load (CE-108255-1)
Both hooks were OFF, so **neither HookLoadMap nor HookInitGameState is the
crasher**; the prepared arms are moot for crash isolation. Next: optionally one
control run of `settings-arm-both-on.ini` to confirm the crash is reproducible
and not stateful; then STOP this matrix and open a new bisect over the still-on
hooks (`HookUObjectProcessEvent`, `HookEngineTick`/`EngineTickResolveMethod`,
or a non-hook cause). Do not burn launches on loadmap/initgs arms.

### v2 outcome B: no crash + mod starts
Stability regained and mod boot does NOT depend on either hook. Question
becomes which hook can be safely re-enabled (functionality recovery).
1. Run `settings-arm-loadmap-only.ini`.
   - Crashes -> **HookLoadMap is the crasher**. Revert to v2 as the stable
     config; optionally run `settings-arm-initgs-only.ini` — no crash proves
     HookInitGameState alone is safe and can stay on.
   - No crash, mod starts -> **HookLoadMap exonerated**; run
     `settings-arm-initgs-only.ini`; if it crashes, HookInitGameState is the
     crasher and final config is loadmap-on/initgs-off; if not, both are safe
     (crash needed the combination — escalate as in outcome A).

### v2 outcome C: no crash + no mod start (L3+R3 dead)
Disabling the hooks stopped the crash but killed mod startup: one of the two
hooks is REQUIRED for mod boot.
1. Run `settings-arm-loadmap-only.ini`.
   - Mod starts, no crash -> **HookLoadMap is the required-and-sufficient hook;
     HookInitGameState was the crasher**. Final config = loadmap-only arm.
   - No mod start -> run `settings-arm-initgs-only.ini`.
     - Mod starts, no crash -> **HookInitGameState is required; HookLoadMap was
       the crasher**. Final config = initgs-only arm.
     - No mod start (or crash) -> neither hook alone restores boot: both are
       jointly required, yet both-on crashes — combination bug; stop and open a
       new bisect (outcome A path), keep v2 as safe-but-degraded fallback.

## Constraints recap for root

- Upload target name: `ue4ss/UE4SS-settings.ini`, must remain 1947 bytes.
- Settings `.ini` is not a SELF/SPRX; decrypt-off `SITE SELF` toggle applies to
  executables, but still size-check (1947) after upload before launching.
- One launch per arm; clean game exit between arms; log pull after each run.
