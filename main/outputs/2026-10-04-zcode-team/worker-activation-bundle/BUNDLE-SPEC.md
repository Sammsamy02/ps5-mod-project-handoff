# BUNDLE-SPEC.md — exact activation bundle for starting the unchanged Item Spawner mod

worker-activation-bundle, 2026-10-04. All paths relative to the workspace root
`/WORKSPACE/.codex/worktrees/translation-core/what` unless absolute.
Nothing here claims engine execution: this is the host-side bundle the EXISTING
runtime (`PrepareActivatedUE4SSRuntime` -> `MakeActivatedUE4SSModSource` ->
`UE4SSProgram`) would consume. The mod's own files are immutable and byte-pinned.

## 0. Activation envelope (what `mfa_activate` consumes)

The bundle is a staged modforge mod root: a `loadlist.txt` plus the files it
lists. Records (vendored `modforge_loadlist.h` `mf_kind`; FORMATS.md "Load list"):

```
modforge-loadlist	1                          header (mf_next, loadlist.h:170-176)
title	PPSA33286                            game/profile fact
engine	unreal
framework	ue4ss-ps5
mod	TownfallItemSpawner	1.0.0
file	config	ue4ss/UE4SS-settings.ini	<size>
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua	21415
file	lua_mod	ue4ss/Mods/TownfallItemSpawner/enabled.txt	0
file	lua_mod	ue4ss/Mods/shared/UEHelpers/UEHelpers.lua	<size>
file	config	ue4ss/MemberVariableLayout.ini	<size>   BLOCKED-ON-LAYOUT-DATA
file	config	ue4ss/VTableLayout.ini	<size>           BLOCKED-ON-LAYOUT-DATA
```

Why each envelope record exists:

| Record | Traced consumer |
|---|---|
| `title PPSA33286` | `mfa_activate` title check (`MFA_E_TITLE`, modforge_activation.h:90); `MakeActivatedUE4SSModSource` re-checks `expected_title == mfa_title` (activated_mod_source.cpp:24-26). `expected_title` is caller-supplied (game/profile fact), never taken from the load list (activated_mod_source.hpp:22-23). |
| `engine unreal` | activated_mod_source.cpp:27 (`mfa_engine(...) != "unreal"` -> throw) |
| `framework ue4ss-ps5` | activated_mod_source.cpp:28 (`mfa_wants_framework(activation, "ue4ss-ps5")`) |
| `mod TownfallItemSpawner 1.0.0` | `mfa_mod_at` load order (modforge_activation.h:125-126); load-list is post-crash truth for worker-7's installed records (modinst.c:810-849) |
| `file` rows, roles `lua_mod`/`config` | default preload roles `lua_mod, config, script` (modforge_activation.h:90) — every listed file is read into owned memory with exact-size verification (`MFA_E_SIZE`), which `MakeActivatedUE4SSModSource` requires: non-preloaded `lua_mod`/`config`/`script` files throw (activated_mod_source.cpp:54-57) |
| no `order` rows | data-plugin load order only (loadlist.h:41); a Lua mod ships none |

No `native_plugin` entries exist, so the activation is Lua-only: `activation_root`
may be empty (activated_mod_source.cpp:62-67 requires the root only for native
entries; activated_mod_source.hpp:22-25 "No root is needed for Lua-only activation").

## 1. Logical `ue4ss/` key map (role -> key -> bytes -> consumer file:line)

### 1.1 `ue4ss/UE4SS-settings.ini` — role `config` — REQUIRED

- Consumer: `SettingsManager::deserialize` reads exactly this key from
  `native_mod_source()` (SettingsManager.cpp:60-63); **missing key throws**
  `"Native activation is missing ue4ss/UE4SS-settings.ini"` (SettingsManager.cpp:63).
  NUL bytes forbidden (SettingsManager.cpp:64-65).
- Content (console-representable options only; every option below is parsed by
  SettingsManager.cpp — no option outside that parser is included):

```ini
[Overrides]
; MUST stay empty: native activation throws on either override
; (SettingsManager.cpp:112-113) and on +ModsFolderPaths rows
; (UE4SSProgram.cpp:2708-2714 add_mods_directory refusal).
ModsFolderPath=
ControllingModsTxt=

[General]
bUseUObjectArrayCache=false                ; retained distro value (census 8c; SettingsManager.cpp:146)
SecondsToScanBeforeGivingUp=120            ; modern default (SettingsManager.cpp:136-145; census 8c)
DefaultExecuteInGameThreadMethod=EngineTick ; RUNTIME-REQUIREMENTS A9/E: the mod's only scheduler
                                             ; path — every keybind callback is ExecuteInGameThread
                                             ; (main.lua:695,700,705,710,716,725; parser SettingsManager.cpp:151-159)
DefaultFNameToStringMethod=Scan            ; RUNTIME-REQUIREMENTS B8; FName->string for ToString/Scan
                                             ; (main.lua:59; parser SettingsManager.cpp:160-169)

[Debug]
ConsoleEnabled=1                           ; retained distro value (census 8c; parser SettingsManager.cpp:223)
```

- Console-representable option forms: booleans are case-insensitive
  `true`/`1`/`false`/`0` (IniParser TokenParser.cpp:84-103,
  ue4ss-native-settings-port-1 patched tree); `ConsoleEnabled=1` and
  `bUseUObjectArrayCache=False` are therefore both valid spellings of the
  retained distro values (census 8c).
- Deliberately ABSENT (each would be parsed but is wrong or unnecessary here):
  `General.HotReloadKey` / `Debug.ToggleGuiKey` (key-parse risk, no benefit —
  SettingsManager.cpp:118-131, 252-265); `EngineVersionOverride.*` (engine facts
  come from the real scan, INTEGRATION-MAP Stage 4; forcing them would fake
  layout axes — see LAYOUT-GATE.md); `Hooks.HookLoadMap=false` (the mod's
  optional `RegisterLoadMapPreHook` at main.lua:731 is pcall-guarded; the hook
  default stays the runtime's own).
- Keybind registration inputs the Lua expects: NONE in the bundle. The mod
  registers six binds itself at runtime — `RegisterKeyBind(Key.F7/UP_ARROW/
  DOWN_ARROW/RETURN/LEFT_ARROW/RIGHT_ARROW, cb)` (main.lua:694,698,703,708,713,
  722) — and `Key.*` constants are runtime-provided, not files. The only
  settings-adjacent input is `General.EnableDebugKeyBindings` (SettingsManager.cpp:135;
  consumed at UE4SSProgram.cpp:743 together with the input source), left at its
  default because the retained trial registered 6/6 binds without it
  (RUNTIME-REQUIREMENTS A8).

### 1.2 `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` — role `lua_mod` — REQUIRED, byte-pinned

- Bytes = retained intake member-00000000.bin, UNCHANGED, 21415 B,
  sha256 `f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658`
  (intake.json entry 0; re-verified this session).
- Consumers: `NativeModSource` ctor enumerates `ue4ss/Mods/<name>/{Scripts,scripts}/main.lua`
  into `m_lua_mods` (NativeModSource.cpp:79-104; directory casing tolerated at
  :91, mixed casing across files refused at :93-94; `shared` excluded at :100);
  `UE4SSProgram::setup_mods` creates one `LuaMod` per entry from
  `mod_source->lua_mods()` (UE4SSProgram.cpp:1742-1748); `start_mods<LuaMod>`
  starts those in `startup_mod_names()` (UE4SSProgram.cpp:1947-1953); the chunk
  is loaded through the real luamod file functions (INTEGRATION-MAP Stage 5;
  LuaMod.cpp native dofile/module path). Mod name = directory name
  `TownfallItemSpawner` — the zip already lays this out (census 1 "File
  destinations").
- 21415 B is far below every limit: 8 MiB/file and 64 MiB total in both
  `mfa_limits` defaults (modforge_activation.h:70-75) and `NativeModSource`
  (NativeModSource.cpp:66-70); 4096-file limits likewise.

### 1.3 `ue4ss/Mods/TownfallItemSpawner/enabled.txt` — role `lua_mod` — REQUIRED, empty

- Bytes = retained intake member-00000001.bin, empty, 0 B,
  sha256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`
  (intake.json entry 1). Presence, not content, is the signal.
- Consumer: the marker enables the mod — `NativeModSource` ctor, final loop
  `if (read(mod.path + "/enabled.txt")) enable(mod.name)` (NativeModSource.cpp:134-135);
  `enable` appends to `m_startup_mod_names` (NativeModSource.cpp:106-112). This
  puts `TownfallItemSpawner` into the enabled list that
  `UE4SSProgram::get_mods_txt_entries` surfaces (UE4SSProgram.cpp:2604-2607)
  and `start_mods<LuaMod>` starts (UE4SSProgram.cpp:1947-1953).
- Optional alternative: `ue4ss/Mods/mods.txt` (role `config`) with a
  `TownfallItemSpawner : 1` row would enable it too (NativeModSource.cpp:113-132;
  rows must be exactly `Name : 0|1`, BOM tolerated, `;` comments skipped,
  malformed rows throw at :128-130). OMITTED: the shipped mod has no mods.txt of
  its own (census 1), and enabled.txt alone suffices; a mods.txt listing
  example mods the bundle does not carry would be dead weight.

### 1.4 `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` — role `lua_mod` — REQUIRED prerequisite

- Why: the mod's first engine interaction is `require("UEHelpers")` (main.lua:15-18;
  RUNTIME-REQUIREMENTS A2). On the native path `require` resolves through
  `native_lua_module`, whose search list is `<scripts_path>/<name>.lua`,
  `<scripts_path>/<name>/init.lua`, then `ue4ss/Mods/shared/<name>.lua`,
  `ue4ss/Mods/shared/<name>/init.lua`, `ue4ss/Mods/shared/<name>/<name>.lua`
  (LuaMod.cpp:866-876). For `require("UEHelpers")` the only retained supplier is
  the last candidate: **`ue4ss/Mods/shared/UEHelpers/UEHelpers.lua`**. The
  runtime's reusable pieces (`mb_lua_install_require` etc., transport-experiments
  progress `ue4ss_payload_direction.existing_reusable_symbols`) install the
  resolver; they do not embed the module's bytes, so the bundle carries them.
- Bytes = retained distro asset
  `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/assets/Mods/shared/UEHelpers/UEHelpers.lua`,
  UNCHANGED, sha256 `5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7`,
  275 lines / 23 functions (RUNTIME-REQUIREMENTS A2 "UEHelpers inventory
  unchanged, 23 functions lines 31-265").
- The `shared` directory never becomes a mod: it is skipped in lua-mod
  enumeration (NativeModSource.cpp:100), so this file adds no startup entry.

### 1.5 `ue4ss/MemberVariableLayout.ini` + `ue4ss/VTableLayout.ini` — role `config` — REQUIRED KEYS, CONTENT **BLOCKED-ON-LAYOUT-DATA**

- Consumers: `UE4SSProgram::load_unreal_offsets_from_file()` reads both keys from
  `mod_source` and **throws** `"Native activation requires both member and vtable
  layout files"` when either is absent (UE4SSProgram.cpp:899-904), then hands the
  bytes to `StageNativeLayoutTexts` (UE4SSProgram.cpp:905;
  NativeLayoutBinding.cpp:136-138). This runs inside `setup_unreal()`
  (UE4SSProgram.cpp:619 -> 936), i.e. BEFORE the engine scan and init.
- The bytes must be worker-2's validated Townfall layout facts: identity
  `format=1`, `target=prospero-x86_64-sysv`, `pointer_size=8`, `engine_major>=4`,
  non-empty revision/build id, positive `UEP_TotalSize` per member class, offsets
  inside the declared size, vtable offsets 8-aligned, accessor widths known
  (NativeLayoutBinding.cpp:60-117, 143-158). None of that content exists in any
  retained validated source -> see LAYOUT-GATE.md: this worker does NOT
  synthesize it. The builder records both keys as
  `"blocked-on-layout-data": true` with no bytes; a placeholder mode exists only
  to exercise the file-load path and is explicitly not layout facts.

## 2. Complete key list (what "the bundle" is)

| # | Key | Role | Size | Bytes source |
|---|---|---|---|---|
| 1 | `ue4ss/UE4SS-settings.ini` | config | generated | builder (options in 1.1) |
| 2 | `ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua` | lua_mod | 21415 | intake member-00000000.bin (pinned) |
| 3 | `ue4ss/Mods/TownfallItemSpawner/enabled.txt` | lua_mod | 0 | intake member-00000001.bin (pinned) |
| 4 | `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` | lua_mod | measured | retained distro asset (pinned) |
| 5 | `ue4ss/MemberVariableLayout.ini` | config | — | BLOCKED-ON-LAYOUT-DATA (worker-2) |
| 6 | `ue4ss/VTableLayout.ini` | config | — | BLOCKED-ON-LAYOUT-DATA (worker-2) |

Prerequisites beyond the archive: UE4SS itself (header comment main.lua:1-3
"requires UE4SS", census 1) — supplied by the runtime, not the bundle; and
UEHelpers (1.4), which the archive does not ship and the bundle therefore carries.
Nothing else: the mod has no pak, no dlls/ entry, no mods.txt row, no keybind
config (census 1 "Config / registration inputs").

## 3. What the runtime does with this bundle (traced, not executed)

1. `mfa_activate(root, title=PPSA33286)` validates the load list, hashes/sizes
   and preloads roles lua_mod/config/script (modforge_activation.h:47-113).
2. `MakeActivatedUE4SSModSource(activation, "PPSA33286", {})` checks
   title/engine/framework, maps every `ue4ss/` file of role lua_mod/config/script
   into immutable buffers, sees no native_plugin, needs no root
   (activated_mod_source.cpp:20-83).
3. `UE4SSProgram` ctor: settings deserialize from key 1 (SettingsManager.cpp:60-68);
   mods directory fixed to `ue4ss/Mods` (UE4SSProgram.cpp:835-838).
4. `init()` -> `setup_unreal()` -> `load_unreal_offsets_from_file()`: keys 5+6
   REQUIRED here (UE4SSProgram.cpp:899-905) — hard stop without them (LAYOUT-GATE).
5. After engine init: `setup_mods()` builds the LuaMod from key 2
   (UE4SSProgram.cpp:1742-1748); `start_mods<LuaMod>` starts it because key 3's
   marker enabled it (UE4SSProgram.cpp:1947-1953; NativeModSource.cpp:134-135);
   the mod's `require("UEHelpers")` resolves to key 4 (LuaMod.cpp:866-876).
6. Expected observable: `startup_mod_names() == ["TownfallItemSpawner"]`,
   `lua_mods() == [{name="TownfallItemSpawner", path="ue4ss/Mods/TownfallItemSpawner",
   scripts="ue4ss/Mods/TownfallItemSpawner/Scripts"}]`, `get_mods_txt_entries()
   == {"TownfallItemSpawner": true}` (UE4SSProgram.cpp:2604-2607).
