# cap13 — mod selection report (STEP 1)

Capability #13: run downloaded PC DLL mods unchanged on PS5.
Scope: first two steps of `CAP13-DLL-MODS-ZCODE-HANDOFF.md` — (1) choose a real
Townfall DLL mod or report the evidence gap, (2) format-aware discovery.
Mac-only; nothing was executed, downloaded, installed, or sent to console.

## Verdict

**No real Townfall DLL mod exists on this machine. Evidence gap confirmed.**
Per the handoff ("If no suitable real Townfall DLL mod exists, report that
evidence gap and use a clearly labeled Windows DLL fixture for preparation; do
not call that a real-mod pass"), preparation proceeds with a clearly labeled
hand-crafted DLL fixture. This is **not** a real-mod pass.

## Search procedure and what was searched

| Location (absolute) | Content |
|---|---|
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/nexus-real-mods-2026-10-01/` | The real mod intake: 4 mod archives + UE4SS runtime + receipts |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/item-spawner-real-mod-intake-1/` | Item Spawner intake (proving-ladder mod) |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/` | Movement speed intake |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/performance-pak-real-mod-intake-1/` | Performance pak intake |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/engine-config-real-mod-intake-2026-10-01/` | Engine.ini intake |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/leon-real-mod-intake-2026-10-01/` | Leon 2K pak analysis |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/townfall-ue4ss-real-script-audit-1/` | The mod audit: extracted Lua scripts + PE header observations |
| `/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/automatic-unchanged-mod-corpus-2026-10-01/` | Per-archive inventory JSONs of the same corpus |
| `caps-resume-20261013` worktree + `what` outputs | `find` for `*.dll`, `*.zip`, `*mod*` archives — all DLLs are UE4SS runtime, project-built fixtures, or Mono test assemblies |

## Complete inventory of downloaded Townfall mods on this machine

Every real mod archive found, with recorded provenance (hashes as recorded in
the intake receipts; archives verified in place, never moved):

| Mod | Nexus source (recorded) | Delivered format | Archive sha256 | DLL mod? |
|---|---|---|---|---|
| TownfallItemSpawner V1.0 | `mods/95?file_id=218` | UE4SS Lua (`main.lua` 21,415 B + `enabled.txt`) | `a9f7ed004dd053e02f6206d8ed6886174f9b5aaecf55244447a2c5fb9d6fa7fb` | No |
| TownfallMiniMap v3.1 | `mods/39?file_id=256` | UE4SS Lua (`main.lua` 196,743 B) | `cf3c353b4e8ab0d49214ceaeae5a8989cf58d1b2ce5758673a360d2e463df377` | No |
| TownfallModMenu v6 | `mods/71?file_id=266` | UE4SS Lua (`main.lua` 128,009 B) | `a9257d8e23c1a377d9acddee36ef82c624aece7920fa6f92a41e21e0690456ca` | No |
| Infinite Stamina v1 | `mods/19?file_id=30` | UE4SS Lua (`main.lua` 2,811 B) | `61cd9c819c0165db114436c04626021599f9830d486e1d5b6855c486990c16c5` | No |
| Leon 2K v1.1 | `mods/107?file_id=271` | UE pak/ucas/utoc (asset replacement) | `661aabf0e6a1cb91e9164501108b5d19db13c2ca21cee651f676e0706f3feef9` | No |
| PlayerMovementSpeed 2x v0.1 | `mods/28?file_id=42` | UE pak/ucas/utoc (`~mods/`) | `3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad` | No |
| UniversalPerformance 1.0 + FG Enabler | `mods/56?file_id=131` | UE pak + `Engine.ini` | `27db7e1b6ea2bf5b574a68a1fa6d7d89ccd5795fd11ac98b4549c2e69132e06e` | No |
| Engine (config tweak) | `mods/110?file_id=264` | `Engine.ini` only | `24ea77e211aa95c7800035de62fde127fd514454c3f7cb9957a672a038afb906` | No |
| UE4SS (runtime, not a mod) | `mods/4?file_id=4` (game's dependency page) | `ue4ss/UE4SS.dll` (20,572,160 B) + proxy `dwmapi.dll` (279,552 B) | zip `58a6b9d096c30b357f50d4becf74b6d21623f0582a05b8cd250c2c58c5119194` | No — this is the runtime mods depend on |

License: none of the intake receipts recorded a license field; that must be
captured from the mod pages when an actual DLL mod is downloaded. Every intake
was marked `executed: false / installed: false` and originals are preserved in
place — nothing was copied out of or removed from the staging trees.

The prior audit (`townfall-ue4ss-real-script-audit-1/pe-header-observations.json`)
reached the same surface conclusion: the only PE images it observed belong to
the UE4SS distribution (`dwmapi.dll`, `ue4ss/UE4SS.dll`), machine `0x8664` —
the runtime, not a mod.

## Evidence for what a real DLL mod will require (from the real UE4SS.dll)

The staged `UE4SS.dll` (sha256 `94c2ebd3ca9f4c4212898d727df5482937f586771c67502635ebd4367422052a`,
x64 PE32+, GUI subsystem) was analyzed with the new `pe_analyzer.py`:

- **4256 named exports** (MSVC-mangled C++ symbols, `RC::`/`Unreal::` API surface),
  0 forwarders, ordinal base 1.
- Exactly **6 unmangled C-API exports**: `call_script_function`,
  `execute_lua_in_mod`, `get_lua_state_by_mod_name`, `is_ue4ss_initialized`,
  `set_script_variable_default_data`, `set_script_variable_int32`.
- 29 imported DLLs incl. `MSVCP140.dll` (207 functions), `VCRUNTIME140.dll`,
  `api-ms-win-crt-*`, `USER32.dll`, `KERNEL32.dll` — a real MSVC runtime
  dependency chain that any host of the DLL itself must also satisfy.
- UE4SS.dll itself does **not** export `start_mod`; per the handoff,
  `start_mod` is the entry point a native mod **provides** (the interface #9's
  host currently refuses: "start_mod-only UE4SS binaries").
- The companion `dwmapi.dll` (sha256 `4e3e5324e68e033e372193d2420367614eb13016cb98b22b8e85e9844f5c0c9a`)
  is a classic proxy: exports the full real `dwmapi` surface (198 EAT entries,
  e.g. `DwmDefWindowProc`) while resolving the real system DLL at runtime via
  `KERNEL32!LoadLibraryW` / `GetProcAddress`. Proxy-style load order is how
  UE4SS gets into the game process.

So the binary interface contract for the first DLL mods is: export
`start_mod` (+ lifecycle), import UE4SS.dll functions (mangled C++ or the six
C-API names), and import the MSVC/Win32 runtime DLLs listed above.

## Fixture decision and justification

`FIXTURES/cap13-ue4ss-native-mod-FIXTURE.dll` — hand-crafted with
`TOOL/make_pe_fixtures.py` (Python `struct` only; no compiler and no
cross-toolchain exists on this Mac — `x86_64-w64-mingw32-gcc` absent). Labeled
a fixture in three places: filename suffix `FIXTURE`, an embedded string in
`.rdata` ("CAP13 FIXTURE: hand-crafted minimal PE ... not a real mod; not for
distribution."), and `FIXTURES/README.md`.

Fidelity choices (evidence-driven, not invented):

- **Exports `start_mod` (ord 1) / `stop_mod` (ord 2)** — the lifecycle entry
  convention named in the handoff.
- **Imports `UE4SS.dll!is_ue4ss_initialized`** — a symbol that verifiably
  exists in the real staged UE4SS.dll export table (one of the six C-API
  exports), so the fixture's import contract is checkable against the real
  runtime.
- **Imports `KERNEL32.dll!DisableThreadLibraryCalls`** — the canonical
  DllMain hygiene call.
- **DllMain calls both imports** (per the task: "a simple DllMain that calls a
  UE4SS function"); `start_mod` also calls `is_ue4ss_initialized`. Real mods
  defer heavy work past DllMain because of the loader lock — noted as a caveat
  in the design report; the fixture exists to exercise format discovery, not
  loader-lock etiquette.
- Deterministic bytes (TimeDateStamp 0, fixed layout): rebuild reproduces the
  identical file (verified in the test suite).

Second fixture `FIXTURES/cap13-native-x86-FIXTURE.dll` (PE32, machine i386)
exercises the `unsupported-arch` classification path; `FIXTURES/NOT-A-PE-FIXTURE.dll`
(text with a `.dll` name) exercises the `unknown` path.

## What a real-mod pass will need (next steps, not done here)

1. Download an actual Townfall DLL mod (Nexus page fetch was out of scope for
   this Mac-only assignment; the analyzer now exists to triage candidates on
   arrival). Record source URL, version, license, hash at download time.
2. Verify with `pe_analyzer.py`: expect `native-x64`, UE4SS.dll imports, and
   the mod's export set; feed the report into the cap13 loader-route design
   (step 3 of the handoff, which requires real image loading and is explicitly
   **not** authorized or implemented here).
3. Keep this fixture set as the regression baseline for step 2's discovery
   tooling regardless of which real mod is later selected.
