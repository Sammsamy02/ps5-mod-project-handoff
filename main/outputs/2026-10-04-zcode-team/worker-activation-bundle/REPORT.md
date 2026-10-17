# REPORT.md — worker-activation-bundle

2026-10-04. Scope executed: the exact activation bundle the EXISTING runtime
consumes to start the unchanged Item Spawner Lua mod, host-tested end to end
short of engine execution. No nested subagents, no network/console, no mod Lua
executed, no engine-execution claims. All deliverables are inside this
directory; no existing file was modified (verified: every Write/Edit targeted
`outputs/2026-10-04-zcode-team/worker-activation-bundle/` or `/tmp`; the dirty
worktree, intake members and `work/nexus-test.*` were only read).

## Deliverables (this dir)

| File | What |
|---|---|
| `BUNDLE-SPEC.md` | every logical key + envelope record, each traced to its consuming file:line |
| `build_bundle.py` | builder: intake/distro byte-pinned inputs -> `build/bundle.json` (logical key -> bytes map) + staged mod roots |
| `test_activation.c` (+ `test_activation-bin`) | host test through vendored `modforge_activation.c` + `modforge_loadlist.h`; 24 checks, ALL PASS |
| `test_records.py` | downloaded/prepared/installed state fixture per worker-7's modinst.c model; 6 checks, ALL PASS |
| `LAYOUT-GATE.md` | StageNativeLayoutFiles/Texts requirements + the hard-stop determination; **BLOCKED-ON-LAYOUT-DATA** |
| `build/` | `bundle.json`, `stage-root/` (strict, no layout keys), `stage-root-full/` (placeholder layout keys, explicitly NOT layout facts), `records-fixture/` |

## The bundle (summary; full trace in BUNDLE-SPEC.md)

Envelope `loadlist.txt`: header `modforge-loadlist 1`; `title PPSA33286`,
`engine unreal`, `framework ue4ss-ps5`, `mod TownfallItemSpawner 1.0.0`;
six `file` rows (roles lua_mod/config), sizes exact. Lua-only activation:
no `native_plugin` entries, so `activation_root` stays empty
(activated_mod_source.cpp:62-67).

| Key | Role | Bytes | Consuming site |
|---|---|---|---|
| `ue4ss/UE4SS-settings.ini` | config | generated (5 options) | SettingsManager.cpp:60-68; missing -> throw :63 |
| `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | lua_mod | intake member-00000000.bin, 21415 B, sha f586043a…658, UNCHANGED | NativeModSource.cpp:79-104; UE4SSProgram.cpp:1742-1748, 1947-1953 |
| `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | lua_mod | intake member-00000001.bin, 0 B, sha e3b0c442…855 | NativeModSource.cpp:134-135 (marker -> startup_mod_names) |
| `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | lua_mod | retained distro asset, 9962 B, sha 5a8f6609…15a7 | LuaMod.cpp:866-876 (`require` search list ends at `shared/UEHelpers/UEHelpers.lua`) |
| `ue4ss/MemberVariableLayout.ini` | config | **BLOCKED-ON-LAYOUT-DATA** | UE4SSProgram.cpp:899-905 -> NativeLayoutBinding.cpp:136-158 |
| `ue4ss/VTableLayout.ini` | config | **BLOCKED-ON-LAYOUT-DATA** | same |

Settings carry exactly the options the mod's path depends on
(`DefaultExecuteInGameThreadMethod=EngineTick`, `DefaultFNameToStringMethod=Scan`,
`bUseUObjectArrayCache=False`, `SecondsToScanBeforeGivingUp=120`,
`ConsoleEnabled=1`) with the `[Overrides]` section empty — either override or a
`+ModsFolderPaths` row makes native activation throw (SettingsManager.cpp:112-113;
UE4SSProgram.cpp:2708-2714). No keybind inputs belong in the bundle: the mod
registers its six binds itself (main.lua:694-729) against runtime `Key.*`
constants; `EnableDebugKeyBindings` stays default (trial registered 6/6 without
it). `mods.txt` is deliberately omitted (shipped mod has none; enabled.txt
suffices — NativeModSource.cpp:113-132 vs :134-135). The one prerequisite the
archive does not ship is UEHelpers, hence key 4; `shared/` never registers as a
mod (NativeModSource.cpp:100).

## Host test results (via the vendored components)

- T1 loadlist parse (vendored `modforge_loadlist.h`): exact record set,
  roles/sizes verified. PASS
- T2 `mfa_activate(stage-root-full)` MFA_OK; title/engine/framework facts. PASS
- T3 byte-identity of all three pinned files vs their retained sources
  (re-hashed this session, both intake pins match intake.json); settings
  content checks. PASS
- T4 NativeModSource enable-logic oracle: `lua_mods == [TownfallItemSpawner]`,
  `startup_mod_names == [TownfallItemSpawner]` via the enabled.txt marker;
  no mods.txt. PASS
- T5 strict root (no layout keys) activates but lacks both REQUIRED layout
  keys; placeholder variant's layout bytes are explicitly marked NOT layout
  facts. PASS
- T6 refusals: wrong title (MFA_E_TITLE), truncated main.lua (MFA_E_SIZE),
  missing loadlist (MFA_E_NO_LOADLIST). PASS
- Records fixture per modinst.c: downloaded -> prepared (journal step; crash
  before loadlist-write keeps prepared) -> installed (loadlist present, named);
  unnamed-installed drops to prepared; never-in-transaction stays downloaded.
  ALL PASS.

Run: `python3 build_bundle.py --layout-placeholders`; `cc -Wall -Wextra
-D_DARWIN_C_SOURCE -I../worker-7-installer/vendored -o test_activation-bin
test_activation.c ../worker-7-installer/vendored/modforge_activation.c`;
`./test_activation-bin <stage-root-full> <stage-root> <intake main.lua> <intake
enabled.txt> <distro UEHelpers.lua>`; `python3 test_records.py`.

## Key findings / gates

1. **Layout hard stop (LAYOUT-GATE.md): BLOCKED-ON-LAYOUT-DATA.** Without both
   layout keys the runtime throws inside `setup_unreal()`
   (UE4SSProgram.cpp:899-904) BEFORE the engine scan — engine init, Lua mod
   start and `on_engine_initialized` are unreachable. Staged content must carry
   identity `format=1/target=prospero-x86_64-sysv/pointer_size=8/engine_major>=4`
   + revision + build_id and pass width/total-size/8-alignment validation
   (NativeLayoutBinding.cpp:60-158). No layout content was synthesized.
2. **Second block (controller-owned):** `PublishStagedNativeLayoutMaps` has no
   production caller (INTEGRATION-MAP SEAM 2 GAP) — even validated bytes stay
   staged-unpublished until the controller wires the publish call with the
   scan-resolved version facts.
3. **UEHelpers must ride the bundle**: the native `require` resolver's only
   retained candidate is `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua`
   (LuaMod.cpp:866-876); `mb_lua_install_require` installs the resolver, not
   the module bytes.
4. Mod files remained immutable throughout; `original.zip` untouched; only
   copies under this worker's `build/` were hashed (f586043a…658 /
   e3b0c442…855 / 5a8f6609…15a7 all re-verified this session).

## Explicit non-claims

No Unity/engine/UE4SS execution; `StageNativeLayoutTexts` was never run (the
placeholder INI would fail its identity check by construction); no in-game or
game-thread behavior was tested; runtime behavior statements are source-traced,
not observed. The activation layer (mfa) host tests above are the extent of
executed verification.

## Unified diffs for existing files

None required. No existing file needed changes for this milestone; the
controller's only follow-ups are the two already-recorded external gates
(worker-2 layout facts; publish-call wiring per INTEGRATION-MAP SEAM 2), which
are not edits this worker could make inside its scope.
