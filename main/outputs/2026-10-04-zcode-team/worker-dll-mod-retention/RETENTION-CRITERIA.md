# RETENTION-CRITERIA — what makes a third-party DLL mod "representative" (2026-10-04)

## Criteria (all must hold; verify each at acquisition time — no network used here)

1. **Target engine family**: host game is UE4 or UE5, with a public
   NexusMods section that contains DLL-shipping mods. (Our shim families are
   ranked from Win32/UE runtime surface; a UE-family mod exercises the same
   surface our PS5 runtime must satisfy.)
2. **Installs a runtime native DLL** into the game directory — proxy-hook DLL
   (e.g. `dxgi.dll`, `dinput8.dll`, `version.dll`) or a mod DLL loaded by a
   mod loader at process start. NOT asset-only (.pak/.uasset) and not a
   save/editor EXE, .NET-only assembly, or script/Lua-only mod.
3. **Popularity / maintenance**: established download base and a maintained or
   historically significant release, so the import census reflects what real
   mods actually require, not a one-off toy.
4. **Source-available or at least documented requirements**: public source repo
   OR a mod page/README documenting required runtime components (VC++
   redistributables, loader version, OS). Documented requirements are the whole
   point of retention.
5. **License permits retention for interoperability testing**: open license
   (MIT/BSD/GPL-class with redistribution terms) or an explicit permission note;
   otherwise record license ambiguity in the manifest and keep retention
   internal-only. Never strip attribution.
6. **One acquisition = one representative**: prefer one candidate from each of
   the two load shapes we must support — (a) proxy-DLL hook loaded by the OS
   loader, (b) mod DLL loaded by a mod loader (UE4SS-style) — to cover both
   import profiles.

## Proposed candidates (well-known, publicly documented examples)

| # | Mod | Nexus game (engine) | DLL it ships | Why representative |
|---|---|---|---|---|
| 1 | **FF7RFix** (Lyall) | Final Fantasy VII Remake Intergrade (UE4.27) | `dxgi.dll`/`version.dll` proxy-hook DLL | Archetype of the proxy-DLL load shape; source on GitHub (MIT-style); documents exact runtime requirements; targets a UE4 game with a large Nexus section |
| 2 | **P3RFix** | Persona 3 Reload (UE4.27) | `dinput8.dll` proxy + plugins | Second proxy-hook shape on another UE4 title; source-available; active maintenance history; exercises input/d3d/console imports |
| 3 | **SML (Satisfactory Mod Loader) bootstrap** | Satisfactory (UE4 → UE5 since 1.0) | native bootstrap DLL (`dinput8.dll` proxy) that loads mod assemblies | Long-lived, heavily downloaded, open-source loader on a UE4/UE5 game; the bootstrap itself is a native runtime DLL with documented requirements |
| 4 | **ReShade** (per-game Nexus pages, incl. UE titles) | e.g. any UE4/UE5 game section | `dxgi.dll`/`d3d11.dll`/`d3d9.dll` injection DLL | The most widely installed runtime DLL "mod" in existence; source-available; enormous import surface (d3d11/dxgi/dcomp/shcore) — stress-tests census breadth; verify per-build redistribution terms at acquisition |
| 5 | **A UE4SS-hosted native C++ mod for Palworld** | Palworld (UE5) | mod `.dll` loaded by UE4SS | The loader-based load shape on the largest current UE5 modding scene; UE4SS documents Palworld as a target; pick the top-downloads mod whose file list contains a native `.dll` at acquisition time (exact pick must be verified live; do not pre-commit a name) |

Ranking for a single first acquisition: #1 or #2 (proxy shape, small archive,
source-available, cleanly documented) — then #5 (loader shape) as the second
acquisition once the first census lands. #3/#4 are breadth alternates.

## Non-criteria (explicitly out)

- Anti-cheat / online-competitive games (EAC/BattlEye titles): legal and
  runtime-hostile; wrong signal for a console mod-manager track.
- Trainers/trainers-EXE, save editors, external tools: not runtime DLL mods.
- UE3 titles (different ABI/loader era than our UE4/UE5 target family).
