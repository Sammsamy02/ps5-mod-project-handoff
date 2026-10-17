# VERIFY-CHECKLIST.md — post-staging verification of the merged activation package

worker-settings-bundle-merge, 2026-10-04. Run AFTER STAGING-COMMANDS.md completes.
Conventions: `HTTP=http://ps5-host-not-configured.invalid:8084`, `FTP=ftp://ps5-host-not-configured.invalid:2121` (anonymous),
root `/data/modbridge/PPSA33286`. RETR readback is the proven verification direction.
Capture every check's actual output; never assume, never replay a stale pass.

## 1. File exists + size matches loadlist field

For every row: RETR the file, record its byte size, and require size == loadlist field.
`mfa_activate` verifies the same sizes in-memory (exact-size preload); an on-disk mismatch
is `MFA_E_SIZE` and the whole activation aborts — the game then runs unmodded.

```sh
FTP=ftp://ps5-host-not-configured.invalid:2121
ROOT=/data/modbridge/PPSA33286
for f in ue4ss/UE4SS-settings.ini ue4ss/PS5Pad.ini \
         ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua \
         ue4ss/Mods/TownfallItemSpawner/enabled.txt \
         ue4ss/Mods/shared/UEHelpers/UEHelpers.lua loadlist.txt; do
  curl -s "$FTP$ROOT/$f" | wc -c | xargs printf '%s  %s\n' "$f"
done
```

Expected sizes (loadlist rows are the authority):

| file | bytes | loadlist field |
|---|---|---|
| `ue4ss/UE4SS-settings.ini` | 779 | `file config ue4ss/UE4SS-settings.ini 779` |
| `ue4ss/PS5Pad.ini` | 2960 | `file config ue4ss/PS5Pad.ini 2960` |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | 21415 | `file lua_mod … 21415` |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | 0 | `file lua_mod … 0` |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | 9962 | `file lua_mod … 9962` |
| `loadlist.txt` | 360 | (the manifest itself, staged LAST) |

## 2. Hash identity (file contents survived transport)

```sh
for f in <same six paths>; do
  printf '%s  %s\n' "$(curl -s "$FTP$ROOT/$f" | shasum -a 256 | cut -d' ' -f1)" "$f"
done
```

Expected sha256 (must match STAGING-COMMANDS.md §0 exactly):

- `ue4ss/UE4SS-settings.ini` → `9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f`
- `ue4ss/PS5Pad.ini` → `1d919fbfac4976f84e6c621970aaddc61493858fc5564c9481c90f94055eca1e`
- `main.lua` → `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`
- `enabled.txt` → `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`
- `UEHelpers.lua` → `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7`
- `loadlist.txt` → `9eef4dbba3130b8f46c41da4c30479cd5758904bc9eb8971bc7c41c85fe5170f`

Any mismatch → re-stage that one file from `staged-package/`, then re-check. Never edit
bytes in place on the console.

## 3. Settings parse clean (gate checks)

Host-side (already executed by this worker, evidence in `host-verification.txt`):
the staged settings bytes fed through the VENDORED settings-port IniParser with PS5
char16_t semantics + replayed `SettingsManager::deserialize` reads +
`validate_native_services` gates → `PASS (all gates + expected values OK)`, exit 0.
Re-run any time against the local package:

```sh
outputs/2026-10-04-zcode-team/worker-ue4ss-settings/host-test/settings_parse_test \
  outputs/2026-10-04-zcode-team/worker-settings-bundle-merge/staged-package/ue4ss/UE4SS-settings.ini
```

Console-side readback spot-checks (RETR and eyeball; all five native gates depend on these):

- `[Debug] ConsoleEnabled=0` (the corrected value; `1` would abort startup)
- `[Debug] GuiConsoleEnabled=0`, `GuiConsoleVisible=0`, `RenderMode=ExternalThread`
- `[CrashDump] EnableDumping=0`
- `[General]` keys present: `bUseUObjectArrayCache=false`,
  `SecondsToScanBeforeGivingUp=120`, `DefaultExecuteInGameThreadMethod=EngineTick`,
  `DefaultFNameToStringMethod=Scan`
- NO `[Overrides]` section (non-empty `ModsFolderPath`/`ControllingModsTxt` throws,
  SettingsManager.cpp:112-113; a comments-only section also wedges the parser)
- File is UTF-8, no BOM, no NUL bytes (missing/NUL settings abort consumption)

## 4. Manifest sanity

- `loadlist.txt` header first line: `modforge-loadlist\t1`
- exactly one `title PPSA33286` / `engine unreal` / `framework ue4ss-ps5` /
  `mod TownfallItemSpawner 1.0.0` row each
- exactly five `file` rows, TAB-separated, sizes equal to §1, no `order` rows
- NO `MemberVariableLayout.ini` / `VTableLayout.ini` rows yet (pair is gap-marked,
  NOT stageable — see STAGING-COMMANDS.md §1; adding rows for absent files aborts activation)
- nothing extra staged under `$ROOT` beyond §1's six files

## 5. Activation oracle (launch, controller-owned)

1. Launch the game. The runtime opens the root ONCE during `module_start`; install must be
   complete BEFORE launch.
2. Read `$ROOT/runtime-status.txt`:
   - expect the appended state lines to reach `activated` → `engine-ready` → `mods-started`;
   - if `failed`, capture code/detail VERBATIM and stop — diagnose before any re-stage,
     never blindly re-stage and retry.
3. KNOWN LIMIT, honest expectation: WITHOUT the staged layout pair the run stops inside
   `setup_unreal()` ("Native activation requires both member and vtable layout files",
   UE4SSProgram.cpp:899-904) BEFORE any Lua — i.e. `runtime-status.txt` will NOT reach
   `mods-started` until the gap-closed pair is staged per STAGING-COMMANDS.md §1. This is
   the designed pair dependency, not a transport failure; `activated` + the setup_unreal
   stop is the correct intermediate observation for this package as staged.
4. Only after `mods-started`: execute worker-itemspawner-bundle/FIRST-RUN-CHECKLIST.md for
   the phase-1/phase-2 mod-tag evidence in the retained Lua log path.

## 6. Negative checks (do not "fix" these)

- `enabled.txt` is 0 bytes BY DESIGN (presence-only marker) — do not add content.
- No `mods.txt` anywhere under `ue4ss/` (enabled.txt alone is the enable mechanism).
- The pair must never be staged from `emission-synthetic/` or any
  `SYNTHETIC-PREPARATION-NOT-A-SCAN-FACT` bytes — placeholders are forbidden.
