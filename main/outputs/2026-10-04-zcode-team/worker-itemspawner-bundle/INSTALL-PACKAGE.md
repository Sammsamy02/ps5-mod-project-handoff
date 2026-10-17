# INSTALL-PACKAGE — console-side installation of the unchanged Item Spawner bundle

worker-itemspawner-bundle, 2026-10-04. The bundle in `bundle/` is finished and
host-validated through the vendored activation path (see `host-test/host-test.log`,
32/32 PASS). This document fixes exactly what is installed, where, with what
permissions, and how the runtime discovers it. It installs nothing itself: the
console transfer is a controller-owned step (this worker has no console access).

## 0. Preconditions

1. The staged layout pair has landed and been merged per
   AWAITING-STAGED-PAIR.md (`ue4ss/MemberVariableLayout.ini` +
   `ue4ss/VTableLayout.ini` present in `bundle/` and listed in `loadlist.txt`).
   Without them the runtime hard-stops in `setup_unreal()` BEFORE any Lua
   (UE4SSProgram.cpp:899-904).
2. The re-run host test passes with the layout keys included (T5 inverted).
3. The runtime chain itself is working (acceptance ladder rungs 1–3); this
   bundle is rung 4 — "unchanged Item Spawner activation".

## 1. Final console layout — `/data/modbridge/PPSA33286/`

Exactly this tree, nothing else (FORMATS.md:219 "Each game's mod root on the
console (`/data/modbridge/<TITLE_ID>`"):

```
/data/modbridge/PPSA33286/
  loadlist.txt                                             ← written LAST
  ue4ss/UE4SS-settings.ini                                 221 B (after pair: unchanged)
  ue4ss/Mods/TownfallItemSpawner/Scripts/main.lua          21415 B
  ue4ss/Mods/TownfallItemSpawner/enabled.txt               0 B
  ue4ss/Mods/shared/UEHelpers/UEHelpers.lua                9962 B
  ue4ss/MemberVariableLayout.ini                           <measured, when emitted>
  ue4ss/VTableLayout.ini                                   <measured, when emitted>
```

Roles as listed in `loadlist.txt`: `config` for the settings + layout INIs,
`lua_mod` for the three mod files. No `order` rows, no `native_plugin`, no
`activation_root` (Lua-only activation: root not required,
activated_mod_source.cpp:62-67). No pak, no `dlls/`, no `mods.txt`.

## 2. Permissions and file invariants

- Plain regular files, UTF-8, no BOM; readable by the game process during
  module_start. No special console attributes are required.
- Path rules: relative to the root, `/`-separated, no backslash, no control
  chars, no leading dot (loadlist `mf_path_ok`).
- Size rule: every file's real byte size MUST equal its `loadlist.txt` size
  field, and nothing may follow the listed bytes — mismatch is `MFA_E_SIZE`
  and the WHOLE activation aborts (all-or-nothing: the game then runs unmodded).
- `loadlist.txt` is written LAST. Its presence is the install-commit truth
  (FORMATS.md:418; worker-7 modinst.c:810-849). Never stage it early.
- Do not hand-edit sizes, do not add `[Overrides]` values in the settings INI
  (native activation throws, SettingsManager.cpp:112-113), do not add a
  `mods.txt`, do not substitute placeholder layout bytes.

## 3. Who writes it — the retained installer path

worker-7's `modinst` (outputs/2026-10-04-zcode-team/worker-7-installer/) is the
retained installer; it journals stage → swap → rename with crash recovery:

```
modinst --state DIR --roots BASE <command> [options]
  register-mod --id TownfallItemSpawner --title PPSA33286 --bundle FILE
  install     --title PPSA33286 --mods TownfallItemSpawner
  records     --title PPSA33286        # post-install verification
  recover                                # only if an interrupted install is reported
```

(`--bundle` form uses an .mfb bundle built with `make-bundle`; the equivalent
`--variant`/prepared-root form stages the same tree. BASE is the roots
directory that contains `PPSA33286`.) The upload/transfer of the staged root or
bundle to the console is the controller's existing bridge step — this package
changes nothing about transport.

## 4. How the runtime discovers the bundle

1. The injected runtime opens the root ONCE during `module_start` — the only
   moment `/data` is readable from the game process (modforge_activation.h:5-9).
   Install must therefore be COMPLETE BEFORE LAUNCH.
2. `mfa_activate(root, title="PPSA33286")` validates the load list, checks the
   title, and preloads every `lua_mod`/`config` file into owned memory
   (exact-size verified).
3. `MakeActivatedUE4SSModSource(activation, "PPSA33286", {})` re-checks
   title/engine(`unreal`)/framework(`ue4ss-ps5`) and maps `ue4ss/` files into
   immutable buffers (activated_mod_source.cpp:20-83).
4. `UE4SSProgram` consumes the activation in memory: settings from
   `ue4ss/UE4SS-settings.ini`, layout INIs in `setup_unreal()`, then the mod
   from `ue4ss/Mods/TownfallItemSpawner/` — enabled by the `enabled.txt`
   marker (NativeModSource.cpp:134-135) — with `require("UEHelpers")`
   resolving to `ue4ss/Mods/shared/UEHelpers/UEHelpers.lua` (LuaMod.cpp:866-876).
5. The runtime appends its own `<root>/runtime-status.txt` state lines
   (`activated`, `engine-ready`, `mods-started`, `failed`, `stopped`) — the
   install-side success oracle before any game-side evidence.

## 5. Post-install verification (in order)

1. `modinst records --title PPSA33286` → installed records present, no pending
   journal states.
2. After launch: `runtime-status.txt` reaches `mods-started` (or `failed` with
   code/detail — capture it verbatim, never replay blindly).
3. Then execute FIRST-RUN-CHECKLIST.md for the phase-1/phase-2 mod-tag
   evidence in the active retained Lua log path.
