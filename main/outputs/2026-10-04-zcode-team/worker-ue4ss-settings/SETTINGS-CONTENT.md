# SETTINGS-CONTENT.md — exact UE4SS-settings.ini, every key sourced from retained code

worker-ue4ss-settings, 2026-10-04. Consumption path (retained composed runtime, native-coherent-
runtime-1/source): `SettingsManager::deserialize(path, receiver)` PROSPERO branch
(SettingsManager.cpp:48-81) reads key `ue4ss/UE4SS-settings.ini` from `receiver.native_mod_source()`;
missing key throws; NUL bytes throw. Every REGISTER_* macro swallows per-key exceptions, so an
absent key silently keeps the struct default (SettingsManager.hpp) — that default can itself be
fatal on native (validate_native_services, UE4SSProgram.cpp:719-746). The content below is
[VERIFIED]: the exact bytes were parsed host-side through the vendored
ue4ss-native-settings-port-1 IniParser/ParserBase (PS5 char16_t semantics, FORCE_U16) replaying
all deserialize reads + all native gates — result PASS, see host-test/PARSE-TEST-OUTPUT.txt.

## Canonical bytes (staged/ue4ss/UE4SS-settings.ini, 779 B, sha256 9f3fa1277218331432294fcdac88097d5268a2db6007b72b4266f45a17730f7f)

```ini
; UE4SS-settings.ini - composed native runtime (PS5) first-run configuration.
; Consumed ONLY from the logical activation key "ue4ss/UE4SS-settings.ini"
; (SettingsManager.cpp:60-68). Every key below is parsed by the retained
; SettingsManager::deserialize; anything unlisted keeps its code default.
; NOTE: no [Overrides] section: native activation refuses non-empty
; ModsFolderPath / ControllingModsTxt (SettingsManager.cpp:112-113) and the
; retained parser wedges on a section that contains only comments.

[General]
bUseUObjectArrayCache=false
SecondsToScanBeforeGivingUp=120
DefaultExecuteInGameThreadMethod=EngineTick
DefaultFNameToStringMethod=Scan

[Debug]
ConsoleEnabled=0
GuiConsoleEnabled=0
GuiConsoleVisible=0
RenderMode=ExternalThread

[CrashDump]
EnableDumping=0
```

## Per-key documentation

| Key | Value | Parsed at | Code default | Why this value |
|---|---|---|---|---|
| General.bUseUObjectArrayCache | false | SettingsManager.cpp:146 | true | Retained distro value for this game (census 8c, BUNDLE-SPEC 1.1); feeds config.bUseUObjectArrayCache (UE4SSProgram.cpp:962) |
| General.SecondsToScanBeforeGivingUp | 120 | :136 | 120 (30 coerced to 120 at :137-145) | Explicit modern default; avoids relying on the coercion branch; feeds config (:961) |
| General.DefaultExecuteInGameThreadMethod | EngineTick | :151-159 | EngineTick (hpp:45) | RUNTIME-REQUIREMENTS A9: every mod keybind callback is ExecuteInGameThread (main.lua:695,700,705,710,716,725); explicit pin against default drift |
| General.DefaultFNameToStringMethod | Scan | :160-169 | Scan (hpp:46) | RUNTIME-REQUIREMENTS B8: FName->ToString path (main.lua:59); explicit pin |
| Debug.ConsoleEnabled | 0 | :223 (-> Debug.SimpleConsoleEnabled) | **true** (hpp:87) | MANDATORY: true throws twice on native — validate_native_services:723 "Native simple console service is unavailable" and create_simple_console:872 |
| Debug.GuiConsoleEnabled | 0 | :224 | **true** (hpp:88) | MANDATORY: true throws "Native debugging GUI service is unavailable" (:725) |
| Debug.GuiConsoleVisible | 0 | :225 | **true** (hpp:89) | MANDATORY: same throw (:725) |
| Debug.RenderMode | ExternalThread | :239-251 | ExternalThread (hpp:93) | validate_native_services:726 requires ExternalThread; pinned explicitly |
| CrashDump.EnableDumping | 0 | :268 | **true** (hpp:99) | MANDATORY: true throws "Native crash dumping service is unavailable" (:721); crash dumper path is also non-PROSPERO-only (:238-248) |

Keys deliberately absent (each would parse but is wrong/unnecessary — BUNDLE-SPEC 1.1 rationale
carries over, all verified against SettingsManager.cpp): Overrides.ModsFolderPath /
ControllingModsTxt (native refuses non-empty, :112-113; defaults are empty so absence is
compliant; see parse finding 2 below for why even an empty [Overrides] section is omitted);
General.HotReloadKey / Debug.ToggleGuiKey (key-parse risk, :118-131, :252-265);
EngineVersionOverride.* (:171-200 — would fake layout axes, LAYOUT-GATE); Hooks.HookLoadMap
(:281; mod's RegisterLoadMapPreHook main.lua:731 is pcall-guarded); General.EnableDebugKeyBindings
(:135, default false — retained trial registered 6/6 binds without it); General.InputSource
(**not parsed anywhere in SettingsManager.cpp** — cannot be set from the ini at all; default
"Default" makes an injected input source a composition prerequisite, WORKING-DIRECTORY.md §4).

## Measured parser findings (host test, vendored ue4ss-native-settings-port-1 parser)

1. **Prior bundle ini FAILS startup.** worker-activation-bundle/build/stage-root's generated ini
   (221 B, sha256 f6d06cada43d43d0674c2986152b72a4dddecf29331acb9cd85e47172889a37f: `ConsoleEnabled=1`, no CrashDump/GuiConsole keys) parses cleanly
   but trips THREE native gates: crash dumping (:721), simple console (:723), debug GUI (:725) —
   UE4SS construction would abort before any mod work. Any bundle using it must be regenerated.
2. **A section containing only comments wedges the parser**: `[Overrides]` + comment lines +
   next section header throws "Syntax error: Expected state NewLineStarted or StartOfFile, got
   StoreSectionKey" (probe-emptysection.ini). Leading comment blocks before the first section and
   comment lines AFTER a key line are fine. Hence the canonical file has no empty [Overrides].
3. **Inline `;` comments corrupt values**: `bUseUObjectArrayCache=false ; trailing` fails
   get_bool (value token includes the comment) -> silent default true. The doc snippets in
   BUNDLE-SPEC 1.1 / FIRST-RUN-CONFIG carry inline `;` comments; their generated bytes do not.
   Never copy the doc snippets verbatim into bytes.
4. `False` casing and empty values (`ModsFolderPath=`) parse correctly (bools case-insensitive;
   prior-bundle ini parsed with cache=false). Full-line `;` comments and blank lines are safe.
5. `get_list("Overrides")` on a file with no [Overrides] section does not throw
   (SettingsManager.cpp:86 is safe with the section absent) — verified in-harness.

## Verification artifact

host-test/settings_parse_test.cpp + PARSE-TEST-OUTPUT.txt: compiles the vendored patched
IniParser + ParserBase sources (plus retained port/first PosixFile + fmt) with host clang++
-std=c++23 -DFORCE_U16 (PS5 char16_t StringType), replays the exact deserialize reads with
SettingsManager.hpp defaults, evaluates the five validate_native_services throws in order, and
asserts the ten expected values. Canonical file: PASS ("all gates + expected values OK").
Host-only; no engine, no console, no network.
