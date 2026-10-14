# WORKING-DIRECTORY.md — composed runtime working directory + file surface (PS5 first run)

worker-ue4ss-settings, 2026-10-04. All code citations are in the retained composed-runtime
tree `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/`
(that tree is the composition input: composition-3 retained 353 units from it and replaced only
runtime_owner/ue4ss_backend — see worker-composition-3 REPORT §4-5). Host-verified where marked
[VERIFIED]; everything else is static read of that code. No console contact.

## 1. Where `m_working_directory` points on PS5

- `UE4SSProgram::setup_paths(moduleFilePath)` (UE4SSProgram.cpp:749-784): on __PROSPERO__ the
  module path comes from `m_native_context.module_file_path` (UE4SSProgram.cpp:218). Then:
  - `m_root_directory = moduleFilePath.parent_path()` (:752)
  - `m_working_directory = m_root_directory` (:758, :770)
- With a native mod source the function returns EARLY at :776-785: log directory =
  working directory; `m_settings_path_and_file` is appended ("UE4SS-settings.ini") for
  diagnostics only — the comment at :779-780 states: "Configuration is selected exclusively
  from retained logical ue4ss/ keys." The filesystem settings file is never read.
- Context validation (NativeProgramContext.hpp:85-101): `module_file_path` and
  `game_executable_path` must be absolute, normalized, NUL-free paths to EXISTING regular files;
  `module_source` (owning loader module snapshot) is mandatory.
- **So on PS5: `m_working_directory` == the directory that contains the composed PRX**, exactly
  as the bootstrap caller staged it. Consequence (honest, unresolved here): the runtime opens
  `m_log_directory / "UE4SS.log"` (UE4SSProgram.cpp:256-257), i.e. it writes UE4SS.log NEXT TO
  the PRX. The PRX directory must be writable, or the caller must place module_file_path in a
  writable volume — caller/composition decision, not a settings decision.

## 2. Files the runtime actually consumes (logical keys via NativeModSource)

On the native path nothing under ue4ss/ is read from disk. `NativeModSource` validates and
indexes immutable buffers (NativeModSource.cpp:10-75: keys must start with `ue4ss/`, <=4096
bytes, no control chars/backslash/colon, UTF-8, canonical, no dup keys; <=4096 files, <=8 MiB
per file, <=64 MiB total, owning buffers required). `MakeActivatedUE4SSModSource`
(activated_mod_source.cpp:30-44) forwards EXACTLY the activation files whose path starts with
`ue4ss/` and whose role is `lua_mod`, `config`, or `script`; any other role throws; no
filesystem access happens in this adapter.

| # | Logical key | Required | Consumer (file:line) |
|---|---|---|---|
| 1 | `ue4ss/UE4SS-settings.ini` | YES — missing throws "Native activation is missing ue4ss/UE4SS-settings.ini"; NUL bytes throw | SettingsManager.cpp:60-68 |
| 2 | `ue4ss/MemberVariableLayout.ini` | YES — throws if either layout key absent | UE4SSProgram.cpp:899-906 -> StageNativeLayoutTexts |
| 3 | `ue4ss/VTableLayout.ini` | YES (pair with #2) | same |
| 4 | `ue4ss/Mods/mods.txt` | optional; rows must be `Name : 0|1` after space/tab strip; BOM tolerated; `;` lines skipped; malformed throws | NativeModSource.cpp:113-132 |
| 5 | `ue4ss/Mods/<Mod>/Scripts/main.lua` (`scripts` lowercase tolerated; mixed casing across files refused; `shared` never a mod) | YES per mod | NativeModSource.cpp:78-104 |
| 6 | `ue4ss/Mods/<Mod>/enabled.txt` | presence enables the mod | NativeModSource.cpp:134-135 |
| 7 | `ue4ss/Mods/shared/<name>.lua`, `shared/<name>/init.lua`, `shared/<name>/<name>.lua` | require() fallbacks | LuaMod.cpp:868, 1256-1257, 1387-1388 |
| 8 | `ue4ss/UE4SS_Signatures/GUObjectArray.lua`, `FName_ToString.lua`, `FName_Constructor.lua` | optional scan overrides | Signatures.cpp:171-230 (read via mod_source, :40-46) |

Mods directory is fixed to the logical `ue4ss/Mods` on native (UE4SSProgram.cpp:832-839); the
`UE4SS_MODS_PATHS` env var is refused when a mod source is present (UE4SSProgram.cpp:259-263).

## 3. Required console working-directory tree for the first run

Managed root (created by the modbridge installer; see STAGING-COMMANDS.md):

```
/data/modbridge/roots/PPSA33286/            <- mfa_activate root (loadlist.txt + staged files)
  loadlist.txt                              header modforge-loadlist 1 / title PPSA33286 / engine unreal / framework ue4ss-ps5 / mod TownfallItemSpawner 1.0.0 / file rows
  ue4ss/UE4SS-settings.ini                  <- key 1  [VERIFIED bytes, sha256 9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f, 779 B]
  ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua   <- key 5  pinned intake bytes, 21415 B, sha256 f586043a1ca5708113e8ee31532d6396cbb0866daeae473722dd8880b6559658
  ue4ss/Mods/TownfallItemSpawner/enabled.txt        <- key 6  empty (0 B), sha256 e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
  ue4ss/Mods/shared/UEHelpers/UEHelpers.lua         <- key 7 (require("UEHelpers") resolves ONLY to shared/<n>/<n>.lua) 9962 B, sha256 5a8f6609e555eee4a535f51d21fcdef768ac654bf4d4abe283c719b1c12215a7
  ue4ss/MemberVariableLayout.ini            <- key 2  BLOCKED-ON-LAYOUT-DATA (worker-2) — runtime throws without it
  ue4ss/VTableLayout.ini                    <- key 3  BLOCKED-ON-LAYOUT-DATA (worker-2) — runtime throws without it
```

Not staged, deliberately: no `ue4ss/Mods/mods.txt` (enabled.txt suffices, BUNDLE-SPEC 1.3), no
`ue4ss/UE4SS_Signatures/*` (absent -> real engine scan runs, no Lua scan override), no pak, no
dlls/, no native_plugin entries (activation root not needed for Lua-only, activated_mod_source.cpp).

Layout-pair gate: `load_unreal_offsets_from_file()` (UE4SSProgram.cpp:896-906) reads keys 2+3
from the mod source and throws "Native activation requires both member and vtable layout files"
when either is absent — this runs inside setup_unreal() BEFORE any Lua. See LAYOUT-GATE.md
(worker-activation-bundle); this worker does not synthesize layout facts.

## 4. Startup call chain this tree must survive (all in the composed object)

ctor -> deserialize (settings from key 1) -> validate_native_services (UE4SSProgram.cpp:719-746;
ini-gated, see SETTINGS-CONTENT.md) -> UE4SS.log open -> create_simple_console (throws if
SimpleConsoleEnabled, :869-873) -> init() -> setup_unreal() -> load_unreal_offsets_from_file()
(keys 2+3, :896-906) -> engine scan/init (config fed from General/Threads/Hooks keys,
UE4SSProgram.cpp:958-993, 1222-1241) -> setup_mods (LuaMod per key 5) -> start_mods (enabled via
key 6) -> mod's require("UEHelpers") -> key 7. Input: HAS_INPUT is defined in the composition
(composition run flags -DHAS_INPUT); the pad backend wires `ConfigureRetainedPS5PadInput`
(ue4ss_backend.cpp:25-26). Because `General.InputSource` defaults to "Default"
(SettingsManager.hpp:42) and is NEVER parsed from the ini (no read exists in
SettingsManager.cpp), an injected, available input source is a hard composition prerequisite:
without it validate_native_services:739-741 throws "Native input service is unavailable".
