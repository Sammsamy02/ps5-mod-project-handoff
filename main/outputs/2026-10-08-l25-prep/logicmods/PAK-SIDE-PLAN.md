# PAK-SIDE-PLAN — LogicMods (UE4SS Blueprint mods) through the generalized PS5 overlay

Agent GN, 2026-10-10 (Claude round-6 item 3d). Companion artifacts: the
host-harness runs in `runs/` (bpml.empty.log, bpml.populated.log,
bpml.no-paks.log), the stub extension in
`../host-lua-harness/stub-ue4ss.lua` ([GN] section), and the report in
`../agent-GN-logicmods.md`. Read-only inputs: GK's
`../pak-overlay/overlay_builder.py`, the composed port runtime
`../../2026-10-08-l26-runtime/typescan-r4/candidate/src/`, the vendored
upstream `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/
source-dependencies/ue4ss/`, and the console captures under
`.superpowers/native-observations/townfall-extra-pak-test-2026-10-01/`.

## 0. What LogicMods are on PC — and who actually mounts them

A UE4SS "BP mod" is a classic `.pak` carrying cooked Blueprint assets under
`Content/Mods/<ModName>/...` (a `ModActor_C` BlueprintGeneratedClass by
convention). PC layout: the pak sits in
`<Game>/Content/Paks/LogicMods/<ModName>.pak` (or in a per-mod folder with a
`config.lua`). The stock loader `BPModLoaderMod/Scripts/main.lua` (TREE-A
`townfall-ue4ss-real-script-audit-1/ue4ss-v1/ue4ss/Mods/BPModLoaderMod`,
byte-identical logic to TREE-B upstream assets copy; the two files differ in
line endings only) then:

1. `IterateGameDirectories()` (:115) → `Dirs.Game.Content.Paks.LogicMods`
   (:119); if missing, `CreateLogicModsDirectory()` (:122) and retry (:123).
2. Scans mod folders for `config.lua` and `dofile`s it (:132) to learn
   `AssetName`/`AssetPath`; loose `*.pak` files without configs get the
   default `ModActor_C` + `/Game/Mods/<name>/ModActor` mapping (:149-163).
3. Reads `Mods/BPModLoaderMod/load_order.txt` for priorities (:53-76).
4. On the INS keybind / LoadMapPost / a startup ExecuteInGameThread (:269,
   :292, :296): `AssetRegistryHelpers:GetAsset(AssetData)` (:208) →
   `World:SpawnActor(ModClass, {}, {})` (:223) → fires the actor's
   `PreBeginPlay` (:228-231); a BeginPlayPost hook fires `PostBeginPlay` for
   matching constructed actors (:275-290).

**The mounting fact (grep-proven, corrects the task brief's parenthetical):**
NEITHER the loader Lua NOR upstream UE4SS C++ mounts LogicMods paks.

- `grep -rn "MountPak\|mount_pak\|FPakPlatformFile\|MountAtPoint\|~mods"` over
  the whole vendored upstream `UE4SS/src/` + `include/` → ZERO hits; the only
  "Pak"/"LogicMods" occurrences in C++ are the directory helpers
  (`src/Mod/LuaMod.cpp:2846/2859/2982-3047` — the IterateGameDirectories
  `__files` fallback + CreateLogicModsDirectory).
- The loader Lua never opens the `.pak` bytes: `FileExists`/`io.lines` are
  only used for `load_order.txt` (:29/:44/:54); `.pak` files are matched by
  NAME in the directory scan (:153) and never read.

So on PC the pak is mounted by the ENGINE's pak platform layer, because
`Content/Paks/LogicMods` is a subdirectory of the engine's pak search root
`<Game>/Content/Paks` (the same mechanism the `~mods` convention relies on);
by the time the loader's step 4 runs, `/Game/Mods/<ModName>` already resolves
through the mounted pak. This aligns with — not contradicts — FS's finding
(API-PARITY-TABLE.md:358): "pak mounting stays out of scope" for the PORT,
because it was never in scope for upstream UE4SS either; mounting is an
engine-side behavior the port inherits from whatever the console runtime
does with files staged in its paks directory.

## 1. PS5 ground truth (what the console actually gives us)

- Paks dir (live FTP LIST, 2026-10-01,
  `native-observations/townfall-extra-pak-test-2026-10-01/
  diagnostic-runtime-paks-list.json`):
  `/mnt/sandbox/PPSA33286_000/app0/townfall/content/paks` — note lowercase
  `townfall/content`, NO PC `<Root>/<Game>/Binaries/<plat>` shape, and the
  retail content is IoStore PAIRS (`pakchunkN-ps5.pak` + `.ucas` + `.utoc`
  per chunk, plus `global.ucas/utoc`).
- A classic (non-IoStore) pak staged as
  `pakchunk9999-ModBridge…_P.pak` IS accepted by the directory
  (`owned_filename_visible: true` in the same capture) — but engine-level
  MOUNT of a classic pak was never observed: the experiment's own outcome.md
  explicitly refuses any mount claim ("Startup config reading through a new
  PAK … remain unobserved; no visible difference would be inconclusive for
  mount acceptance"). The pak dir shows `drwxrwxrwx`, so file placement is
  possible; what the Prospero runtime DOES with a classic pak there is an
  open Stage-2 question.

## 2. The three seams

### Seam 1 — bytes: classic BP pak → IoStore world (GK's builder + pinned retoc)

GK's `pak-overlay/overlay_builder.py` (read; its UNSUPPORTED.md is not
present yet — the dir holds the builder + an empty `mod-manager/`) admits
only IoStore mod archives (`.utoc`/`.ucas` pairs, admission profile in its
docstring: version 8, Indexed, unencrypted/unsigned, no compression methods,
single-partition, per-mod v5 ContainerHeader). Classic `.pak` inputs are not
in its admission set at all.

Bridge that exists locally as pinned source: retoc
(`unreal-dependency-compatibility-1/source/retoc/retoc/src/`, GK pin P4) is
exactly a classic-pak ↔ Zen/IoStore converter — `PakFileReader`
(lib.rs:129-140, repak-based) plus `zen_asset_conversion`/`zen.rs`
(lib.rs:20-21) implement legacy-package → Zen export/import rewriting, and
the historical Leon trial used it to repack a PC pak into IoStore form
(GK docstring: "the historical trial which repacked through retoc").

**Plan:** LogicMods pak content flows
`<Mod>.pak` —(retoc pak→zen, Mac, pinned source)→ `<Mod>.utoc/.ucas`
—(GK overlay_builder)→ derived `<game>.utoc` + sidecar `<stem>_s1.ucas`
staged into the console paks dir. The BP packages then exist in the IoStore
world under `/Game/Mods/<ModName>/...` exactly like any overlaid asset.
Open items owned by this seam: retoc must run on Mac for the pinned rev
(rust build — separate from this Mac-only task), and BP assets (uasset with
BlueprintGeneratedClass, bytecode exports) must survive zen conversion —
retoc's export rewriting is proven for the Leon PC mod set; BP classes were
not in that set (Stage-1 verification point).

### Seam 2 — the loader's directory scan (IterateGameDirectories)

The port's `IterateGameDirectories` (candidate LuaMod.cpp:3270) derives the
root from `get_game_executable_directory()` with the PC assumption
`<exe>/../../Content` (:3276-3278); on the PS5 app0 layout that parent chain
does not exist → the "directory structure is unknown" warning (:3280) → nil
→ the loader's own `:117` abort (reproduced in the harness before this task).
`CreateLogicModsDirectory` (:3604) has the same root assumption.

Two aligned options (recommend A):

- **A. Real staged dir + a small port root-mapping (recommended).** The
  deploy pipeline (the existing upload/manifest machinery) creates
  `…/app0/townfall/content/paks/LogicMods/` and stages per-mod
  `config.lua` + the mod list; the port gains only a PS5 root-dir mapping
  for `get_game_executable_directory`-consumers (LuaMod.cpp:3276/:3611) so
  the existing std::filesystem walk sees the real directory. Everything
  downstream (the `__files` metatable protocol, `pairs()` over mod dirs,
  `CreateLogicModsDirectory` retry) is then upstream-identical — exactly the
  behavior proven in this harness (Section 4 / runs/). The `.pak` files do
  NOT need to be on the console at all: their content already lives in the
  IoStore overlay; the scan only feeds the loader's bookkeeping (configs,
  ordering, spawn list). Stage `load_order.txt` through the existing
  `ue4ss/Mods/BPModLoaderMod/` NativeModSource keys.
- **B. Loader-side virtualization.** The port's IterateGameDirectories
  fabricates the LogicMods tree from the overlay journal/manifest. Bigger
  divergence from upstream LuaMod.cpp; only worth it if creating/reading a
  real directory under app0 at runtime is refused (the drwxrwxrwx listing
  and prior FTP staging say it won't be). The GA path-shim
  (candidate/src/LuaPathShim.cpp, host twin `../host-lua-harness/
  path-shim.lua) is the WRONG layer for this: it virtualizes Lua `io`
  paths, while IterateGameDirectories walks std::filesystem directly.

**Port gap this harness surfaced either way (finding for the port, not the
stub):** BPModLoaderMod:132 calls `dofile(ModFile.__absolute_path)` with an
ABSOLUTE path, but the port's `dofile`/`loadfile` overrides
(LuaMod.cpp:1400-1407 → native_lua_file :989-1026) read only NativeModSource
logical keys and "reject absolute paths, parent components, … and Windows
paths" (:1003-1004 comment). A populated LogicMods dir with `config.lua`
would fail at :132 on console. Fixes that preserve the law: extend the
native dofile to fall back to the patched liolib for absolute paths inside
the (root-mapped) game dir, or admit `config.lua` files as retained
NativeModSource keys (e.g. `ue4ss/Mods/BPModLoaderMod/logicmods/<mod>/…`)
and have the seam report those logical paths as `__absolute_path`. Loose
pak-only mods (no config.lua) never hit dofile — the minimal viable Stage-1
shape.

### Seam 3 — runtime events on constructed BP objects

Proven on console already: RegisterConsoleCommandHandler family and
ExecuteInGameThread dispatch (FS table §1a; spawner evidence). The BPML
path adds, in order of increasing novelty:

1. `AssetRegistryHelpers:GetAsset(AssetData)` / `GetAssetRegistry()`
   (:208/:245) — reflected calls into the AssetRegistry module; retail PS5
   cooks may strip or reorganize the asset registry; `GetAssetByObjectPath`
   finding a Zen-overlay package is UNTESTED (nearest proven neighbor:
   LoadAsset LuaMod.cpp:3970-4014 uses the same registry — also UNEXERCISED
   per FS).
2. `World:SpawnActor(ModClass, {}, {})` (:223) — reflected spawn of a
   BlueprintGeneratedClass from an overlaid package; reflection/ImportText
   family is BOUND (FT §0.1.1) but BP-class spawn is new.
3. `Actor.PreBeginPlay` / `Context.PostBeginPlay` property-read + call
   (:228-231/:281-284) — function-property read → UFunction userdata →
   `__call` → `call_ufunction_from_lua` (LuaUObject.cpp:151-300; note the
   round-6 brief's "LuaUObject.cpp:2413" lands in the sparse-delegate
   property-push marshal in the current candidate — same family, different
   line) — this is the reflected-call machinery, UNTESTED for BP assets
   whose UFunctions live in Zen-loaded packages with Blueprint bytecode.

**This is THE runtime question the ladder must answer** (marked as such;
harness chain-stubs cannot see it): does `call_ufunction_from_lua` on a
BlueprintGeneratedClass member (PreBeginPlay/PostBeginPlay and any mod-entry
K2 event) invoke correctly when the class comes from the overlay, with the
FName pool, function-param marshal and GC behavior of the R3-marshal world
all live.

## 3. Test ladder

- **Stage 0 — host harness (DONE, this task).** BPModLoaderMod runs past
  :117 to its own terminal states with a port-shaped directory tree:
  empty-dir create path + "load_order.txt not present … loading all BP mods
  in random order." (runs/bpml.empty.log), full config.lua + pak + priority
  flow with PreBeginPlay firing on all three triggers
  (runs/bpml.populated.log), and the no-Paks nil-chain at :119 execution-
  proving FT's predicted second failure (runs/bpml.no-paks.log). 27/27
  calibration byte-identical (runs/calibration-reproof.txt).
- **Stage 1 — Mac (next; needs one real sample).** No LogicMods pak exists
  anywhere in the local trees (`find .superpowers -iname '*.pak'` yields
  only game-asset/mod paks — e.g. the Leon 2K texture pak — and
  `-type d -iname LogicMods` yields nothing). GAP: a user-supplied UE4SS BP
  mod (a LogicMods `.pak` with `/Game/Mods/<name>/…` ModActor blueprint) is
  required. With one in hand: (a) retoc pak→zen conversion on Mac, inspect
  the Zen export set (BP class present?); (b) GK overlay_builder admission +
  dry-run against the game's base utoc/ucas; (c) replay the mod's
  config.lua/name through this harness driver (fixture swap) to keep the
  Lua side green. The Leon pak can dry-run (a)+(b) mechanics only — it is
  not a BP mod and cannot exercise the loader's asset path.
- **Stage 2 — console (behind Claude review; one-launch law).** Stage the
  overlay + the LogicMods bookkeeping dir; launch once; expect the loader's
  prints (dir scan, "Loading mod …", "Actor: …", "Executing 'PreBeginPlay'")
  in the UE4SS log; negative controls: no overlay (loader runs to
  no-logic-mods terminal), overlay without bookkeeping dir (create path).
  Gate order: registry GetAsset → SpawnActor → K2 fire, each with its own
  observable before the next is trusted. No rapid-fire relaunches.

## 4. Readiness verdict

| Piece | Status |
|---|---|
| Loader Lua logic (scan/config/order/spawn/fire) | READY — harness-proven end-to-end at script level (this task) |
| Port bindings the loader needs | ALL REGISTERED (FS §1a: IterateGameDirectories :3270, CreateLogicModsDirectory :3604, RegisterBeginPlayPostHook family, RegisterLoadMapPostHook :3075, ExecuteInGameThread :5836, FindFirstOf :1870, UnrealVersion :7689, FName ctor) |
| Port root-dir knowledge for the PS5 app0 layout | GAP — nil today (LuaMod.cpp:3276-3284); small contained fix (Seam 2A) |
| Port dofile for absolute config.lua paths | GAP — NativeModSource logical keys only (LuaMod.cpp:1003-1004); avoidable via loose-pak-only mods or a logical-key staging |
| Classic-pak → IoStore bytes | PATH EXISTS (pinned retoc + GK builder) but UNPROVEN for BP assets and retoc is not built for Mac yet — needs GK classic-pak support / retoc build |
| Engine mount of classic pak on PS5 | UNVERIFIED — not needed if Seam 1 is used; keep as fallback question only |
| BP K2/reflected calls on overlaid BP classes | RUNTIME RISK — the ladder's open question (Seam 3) |
| Real BP mod sample | MISSING — user-supplied LogicMods pak needed for Stage 1 |
