# agent-GN — LogicMods (BPModLoaderMod) host harness + pak-side plan

Claude round-6 item 3d. Mac-only, battery-safe: local Lua 5.5.1, no builds,
no console, no network, no mod edits. Outputs:
`outputs/2026-10-08-l25-prep/logicmods/` (driver, fixture, runs, plan) + one
append-only extension to `host-lua-harness/stub-ue4ss.lua`. GK's
`pak-overlay/` read only.

## Harness terminal lines (the directive)

Stub extension ([GN] section, stub-ue4ss.lua; opt-in `opts.logicmods_fs`,
default OFF so every prior behavior is untouched): `IterateGameDirectories`
now reproduces the PORT's result shape when asked (typescan-r4
LuaMod.cpp:3270-3601 — nested dir tables, raw entries = subdir tables with
the game dir keyed "Game" (:3360-3370), per-dir metatable holding
`__name`/`__absolute_path` (:3580-3581) and an `__index` answering `__files`
with a fresh 1-based array of `{__name=, __absolute_path=}` file descriptors,
directories excluded (:3518-3521), other keys nil (:3565-3569), live walk,
depth cap 64); `CreateLogicModsDirectory` creates the fixture Paks +
LogicMods in port order (:3630-3645); fixture-mode `env.io` mirrors the open
patched-liolib (FT §0.1.3). `LoadAsset` was already stubbed (stub :520) —
untouched. Driver: `logicmods/run-bpml.lua`.

- **Empty state (runs/bpml.empty.log)** — load OK; the loader's OWN create
  path runs (`CreateLogicModsDirectory` → retry-Iterate sees it), then its
  no-mods terminal line:
  `[BPModLoaderMod] Mods/BPModLoaderMod/load_order.txt not present or no
  matching mods, loading all BP mods in random order.` INS keybind + both
  map hooks fired, 0 errors, 0 missing globals.
- **Populated state (runs/bpml.populated.log)** — config.lua dofile'd,
  loose-pak default config applied, load_order priority honored
  (`Loading mod [Priority: #1]: SampleBPMod`), and ALL THREE LoadMods
  triggers (startup ExecuteInGameThread, INS keybind, LoadMapPostHook) run
  GetAsset → SpawnActor → **"Executing 'PreBeginPlay' for mod '…'"**;
  BeginPlayPost fires its faithful early-return (no matching BP actor in a
  stub world). 0 errors, 0 missing globals, 88 recorded calls.
- **Bonus (runs/bpml.no-paks.log)** — with no Paks dir at all the loader
  fails at main.lua:119 (`index a nil value (field 'Paks')`), execution-
  proving FT's predicted second failure mode.
- **Calibration law upheld (runs/calibration-reproof.txt)** — snapshot
  before the extension, run-all.sh (26 logs) + bonus-stamina repro (27th)
  re-run: **ALL 27 LOGS BYTE-IDENTICAL** (aggregate sha256
  9d29049a3123b5cf7e6d1895db241f22563b33fc7424ca0d828eba611ad12d7e).
- Checked and dismissed: main.lua:173 `string.format("%s", table)` is safe
  on the console family too — vendored Lua 5.4.7's `luaL_tolstring`
  (lauxlib :898+) falls back to `"table: 0x…"` exactly like 5.5 (log shows
  it working). Also confirmed Lua-version honesty: harness runs 5.5.1,
  vendored upstream Lua is 5.4.7.

**Missing-API list (REAL port gaps found, not stub gaps):** every global the
loader needs is registered in the port (FS §1a). Two BEHAVIORAL gaps:
1. `IterateGameDirectories`/`CreateLogicModsDirectory` root-dir knowledge —
   PC `<exe>/../../Content` derivation (LuaMod.cpp:3276-3284/:3611-3617)
   does not match the PS5 app0 layout (`…/app0/townfall/content/paks`,
   lowercase, IoStore pairs — native-observations/townfall-extra-pak-test
   paks-list) → nil today.
2. `dofile(ModFile.__absolute_path)` at main.lua:132 — the port's
   dofile/loadfile overrides read NativeModSource logical keys only and
   reject absolute paths (LuaMod.cpp:1400-1407 → :989-1026, rejection noted
   at :1003-1004). A populated LogicMods dir with config.lua fails at :132
   on console. (Loose-pak-only mods never call dofile.)

## Pak-side verdict (logicmods/PAK-SIDE-PLAN.md)

- **Who mounts on PC (grep-corrected):** neither upstream UE4SS C++ nor the
  loader Lua mounts LogicMods paks — zero MountPak/FPakPlatformFile hits in
  the vendored upstream; the loader name-scans `.pak`s and never opens them.
  The ENGINE mounts them (Content/Paks/LogicMods is inside the engine pak
  search tree). Consistent with FS's "pak mounting out of scope" for the
  port: it was never in UE4SS's scope either.
- **PS5 route:** classic BP pak →(pinned retoc, pak→zen)→ `.utoc/.ucas`
  →(GK overlay_builder, admission profile = IoStore archives)→ derived toc +
  sidecar into the console paks dir; the loader's directory scan is then
  bookkeeping only (configs/order/spawn list) and can be fed by a real
  staged `LogicMods` dir + a small port root-mapping (recommended Seam 2A),
  not by the GA io path-shim (wrong layer: the port walks std::filesystem).
- **Ready:** loader Lua logic (harness-proven); all bindings registered.
- **Needs GK/classic-pak support:** pak→IoStore bridge for BP assets
  (retoc pinned but unbuilt on Mac; BP-class conversion unproven).
- **Needs a real BP mod sample:** NONE locally (no LogicMods dirs, no BP
  paks in any tree — only game-asset paks like Leon 2K). Stage 1 blocked on
  a user-supplied LogicMods pak.
- **Runtime risk (the ladder's open question):** AssetRegistry GetAsset on
  Zen-overlay packages, SpawnActor of an overlaid BlueprintGeneratedClass,
  and K2 PreBeginPlay/PostBeginPlay through `call_ufunction_from_lua`
  (LuaUObject.cpp:151; the brief's :2413 lands in the sparse-delegate
  marshal family in the current candidate) — untested for BP assets;
  harness chain-stubs cannot see it.

## Verdict summary

Stage 0 DONE (loader runs to its own terminal lines in the harness;
27/27 calibration byte-identical). Stage 1 needs (a) retoc-on-Mac classic→
IoStore for a real BP pak + GK admission, (b) the port root-mapping +
dofile decision. Stage 2 (console, behind Claude review) answers the BP
reflected-call question. SKIP stance on auto-running BPModLoaderMod on
console stands until those close; nothing in this task touched mods,
runner.lua, console, or GK's outputs.
