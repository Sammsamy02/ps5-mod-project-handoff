# Team J: save-game mods and UE5 asset-type mods (Mac-only research, design, prototype)

Date: 2026-10-13. Everything here is **Mac-only**. Team J ran no console step and read no console log.
Earlier launches run by the integrator are cited as "earlier launch, user-visual"; they are not new
console proof (CLAUDE.md section 5). Inputs were only read: the game's own files already on the Mac,
three unchanged PC mods, and the project's existing readers. Nothing in the runtime tree, the
integrated tools or other teams' folders was changed.

## In plain words

- **Most of a PC mod is the same on PS5.** A cooked UE5 package has the same outer format on PC and
  PS5 (same header, same flags, same classes). Only three kinds of content are built differently per
  platform:
  1. **Pictures (textures)**: the PS5 stores them in a GPU tile order. We already re-tile them
     (console-tested in an earlier launch; the user saw the textures).
  2. **Shaders** (inside materials, and inside GPU particle effects): PC shaders cannot run on PS5.
     We point the mod's materials at the game's own PS5 shaders (console-tested for clothes in an
     earlier launch, user-visual). GPU
     particle effects have no fix yet.
  3. **Sound banks** (this game uses FMOD): the game ships separate PS5 banks. Not checked yet.
- **Character models: same layout on PC and PS5** (one decoder reads both), and Leon's PC body showed
  on the console in an earlier launch (what the user saw, not checked per mesh), so "likely as-is".
  Skeletons, physics bodies, data tables, Blueprints and animations look like plain data with nothing
  platform-specific; they are "likely as-is" or "unknown" until one console test each.
- **Blueprint logic mods (UE4SS "LogicMods") do not work yet.** The loader script itself runs in the
  host test, but four small runtime gaps and pak mounting stop it on the console. One build and one
  launch with a real LogicMods mod would test it.
- **Saves:** the PS5 game keeps saves in the console's **system save storage**, not as loose files.
  Inside, the save is the engine's normal save format, which is the same as on PC. A safe import would:
  check the PC file on the Mac, record the current saves, then let the **game's own save functions**
  write it, never touching the storage directly. The inside format should be the same as on PC (engine
  code, not proven on a real save yet). There is no file-level undo. Not built yet; steps below.
- **New safety finding:** one of the PC mods we have (third-person view) **writes its own save slot**
  through the engine. On PS5 that writes into the user's real save storage. The classifier now flags
  such mods so the user can be asked first.
- **Prototype:** `tools/classify_mod.py` reads a PC mod and says, per file: loads as-is / needs
  converter X / unsupported / unknown. It ran on three real PC mods and on the game's own files
  (26 host tests pass; one independent review, all findings fixed: REVIEW.md).

## 1. Asset types: PC-cooked vs PS5-cooked

How it was compared (Mac-only): `tools/pair_compare.py` reads PC packages (Leon 127, third-person 23,
movement-speed 1) and the game's own PS5 packages already fetched read-only (cap08 + cap12 fetches,
re-identified by name hash and TOC IoHash), groups them by class and compares: package flags,
bulk-data map flags, shader-format tokens, texture layout, and (skeletal meshes) an independent decoder
(Team A's `mesh_sections.py`, sha256 recorded). 68 packages are same-path pairs (Leon overrides the
game's own file). Every console file used was re-identified: its chunk id is in the game's TOC, its
length matches, and BLAKE3-20 equals the TOC IoHash (all files, 0 refused). Every PC sample's flags and
bulk-flag signature must be seen on a console sample of the same class. The console's shader target
`SF_PS5-SP_PS5` is read from the game's own pak entry `ShaderTypeInfo-...-SF_PS5-SP_PS5.stinfo`.
Evidence: `evidence/pair-compare.json` / `.txt`, `evidence/target.txt`.

Facts true for **every** PC package seen (151 packages, 3 mods): zen header without versioning,
package flags cooked + unversioned properties + editor data filtered (`0x80002200`, like the game's;
one widget package also has `0x40000`, "needs localization gather"); IoStore container v8, no compression, no encryption; and **every script import
(class, struct, function) resolves in the PS5 build's own ScriptObjects** (0 unresolved).

| Asset type | Verdict | Platform-specific data? (evidence) | Converter | Provable from the game's files? |
|---|---|---|---|---|
| Skeletal mesh | **likely as-is** | No. 25 same-path pairs: same flags, one inline bulk payload with flags `0x0` on both sides, no shader code. The same independent decoder reads PC 26/26 and PS5 21/25 with the same layout (material table, reference skeleton, LOD sections, index and position buffers); the 4 PS5 misses ("no LOD section table found") are the cloth outfit, the 822-morph-target face mesh and 2 face meshes (DNA rig). Earlier launch H: the user saw Leon's PC body with his textures (user-visual; no log line counts meshes, `[pk] T1 found=8/8` counts 8 MICs). | none | yes (Mac done; one per-mesh console check missing) |
| - morph targets | likely as-is | Same class in game meshes (PS5 face mesh has 822); PC sample has 30. Not console-tested. | none | yes |
| - cloth | unknown | The game's PS5 outfit mesh carries `ClothingAssetCommon` + `ChaosClothConfig`; no PC cloth sample yet. Expected portable (CPU simulation data). | none expected | needs a PC cloth sample |
| - face rig (DNAAsset) | likely as-is | Rides inside Leon's face meshes (earlier launch H, user-visual; face animation not checked; the decoder misses the 2 console face meshes). | none | yes |
| Texture 2D | **CONVERT** | Yes: PS5 textures carry PackedData bit 30 (HasOptData) + a tiling mode (96/96 probed console textures, cap12 probe); all 38 PC textures are linear. Same bulk flags (`0x10501`, `0x48`) on both. | `texture-ps5-tiling` (exists; earlier launch H, user-visual "mostly PASS", 30 textures) | yes (rule from the game's textures) |
| Cube / volume / array textures | unsupported | Same tiling question; the converter handles 2D only. | not written | yes, with console samples (F1 has 2 cube maps) |
| Material, material instance | **CONVERT** | Yes: PC materials carry `PCD3D_SM5`/`PCD3D_SM6` inline shader maps (18/18); game materials and static-permutation instances carry `SP_PS5` maps (game shader libraries `SF_PS5`). | `material-reparent` (exists; earlier launch H, user-visual for instances; Mac-only for mod-only base materials) | yes (game's own shader maps) |
| - Nanite / instanced draw | constraint | A re-pointed material must have PS5 shaders for the mesh's draw path; the game's hide material has none for Nanite/instanced (the remaining #12 checker). | converter must check usage | yes (shader map hashes per game material) |
| Static mesh (incl. Nanite) | unknown | No PC sample on the Mac. Game: 8,884 static meshes, 7,859 Nanite (registry `NaniteEnabled`); PS5 config runs Nanite and `RayTracingMode=Full`. Expected: Nanite cluster data and distance fields are portable (generic encoder output, decoded by shaders); open question: precomputed ray-tracing data in the PS5 form. | probably none; maybe RT data rebuild | yes: F1 fetches 2 Nanite + 2 non-Nanite PS5 samples; needs one PC sample |
| Physics asset | unknown -> likely | PC samples (2): only primitive bodies (`SkeletalBodySetup`, constraint templates), no bulk data, no cooked collision geometry, no shader code. PS5 sample in F1. | none expected | yes (F1) |
| Skeleton | likely as-is | Same shape as 4 PS5 skeletons (flags, no bulk, no shader code). | none | yes (done, Mac-only) |
| Animation sequence / montage / blend space / pose | unknown | No samples yet. Codec classes the game uses (ACL: `AnimBoneCompressionCodec_ACL`, `AnimCurveCompressionCodec_ACL`) exist in the PS5 build, so ACL-compressed PC animations pass the class check. | none expected | yes (F1) |
| Anim Blueprint | likely as-is | Same shape as 3 PS5 anim Blueprints. | none | yes (done, Mac-only) |
| Blueprint / widget Blueprint | unknown -> likely | Script bytecode + property data; 5 PC samples, PS5 samples in F1. As **LogicMods** they also need the loader (section 2). | none (data) | yes (F1) |
| Niagara system | unknown / unsupported for GPU | GPU emitters (29 of the game's 76 systems) carry compiled compute shaders = platform-specific; CPU emitters are VM bytecode (portable); renderers use materials (material route). The #12 "liquid" checker is a material (`MI_Liquid_Oil_01`), not Niagara data. | `shader-route-niagara` not written | yes (F1 has GPU and CPU samples) |
| Sound | unknown | This game's audio is **FMOD**: 2,769 FMOD events; only 22 UE SoundWaves and 2 SoundCues (engine/debug). Real sound is in `.bank` files outside the paks: PC `FMOD/Banks/Desktop`, PS5 `FMOD/Banks/PS5` (game's `PS5Game.ini`). Banks are built per platform (codec). | maybe a bank re-encoder | yes: F2 reads 2 PS5 bank headers |
| Data table / curve / struct / enum | unknown -> likely | Pure property data. The movement-speed mod replaces exactly the game's `Paces_DT`; F1 fetches the PS5 copy, so a byte compare can show that only the changed values differ. | none expected | yes (F1) |
| Localization | likely as-is (format) | UE `.locres` (the game's console copy starts with the standard FTextLocalizationResource magic GUID) and the game's own text (`Dialoc_Base.dloc`, 15.6 MB plain UTF-8 XML) carry no platform data (`evidence/localization-check.txt`). The console also loads an extra `PS5/Dialoc/Dialoc_PS5.dloc` (8.9 KB) a PC translation will not cover. Route: classic pak (#7). | none | yes |
| Shader libraries, PSO caches, `.stinfo` | unsupported | Platform-specific by design (`ShaderTypeInfo-...-SF_PS5-SP_PS5.stinfo` in the game's paks). Replaced by the material route. | n/a | n/a |
| Maps (World, built data) | unsupported | Lightmaps are textures (tiling), plus world-partition data; untested. | not written | partly |
| Groom, geometry collection, level sequence, fonts, media | unknown | No PC samples; F1 has PS5 samples. | ? | F1 |

Converter proofs come only from the game's own files (texture rule from the game's console textures;
shader maps from the game's own container headers and libraries). No Sony SDK material is used.

## 2. Blueprint "logic" mods (UE4SS BPModLoaderMod)

**Status today: not supported** (Mac-only, from the candidate tree and the launch loadlists):

- BPModLoaderMod is in no launch loadlist; the blind-test kit rejects `BPML_GenericFunctions`.
- The loader's own Lua runs to its end lines in the host harness (agent-GN, 27/27 calibration).
- Runtime gaps in `typescan-r4/candidate/src/LuaMod.cpp`:
  1. **Directory keys keep the on-disk case.** `IterateGameDirectories` now finds the PS5 flat layout
     (B3, :3741-3800; self-test 134 only checks that a table comes back), but the console paths are
     lower-case (`/app0/townfall/content/paks`), so the loader's `Dirs.Game.Content.Paks.LogicMods`
     is nil and it stops in `LoadModConfigs`. Fix: case-insensitive key lookup for directory tables.
  2. **`CreateLogicModsDirectory` still uses the PC path** (`<exe>/../../Content`, :4132-4140) and
     throws on the console. Fix: same flat-layout root as (1); never create folders at runtime (the
     installer stages `paks/logicmods`).
  3. **`dofile(<absolute path>)`** for a mod's `config.lua` is refused by the native mod source
     (absolute paths rejected, :1231). Only mods with a `config.lua` hit this.
  4. **The BP paks must be mounted.** The console build does not pick up extra pak files by itself;
     use the #7 paktx route (merge) or #7 step 2 (runtime mount, `RuntimePakMount.cpp`, in progress).
  5. Known limits, not blockers: BeginPlay / LoadMap hooks are data-only emulations (spawn + 50 ms;
     streamed-level actors not reported), so `PostBeginPlay` timing is approximate; `Key.INS` needs a
     pad mapping.
- The BP packages themselves are plain cooked data (Section 1: "likely as-is").

**What one test needs:**
1. A real LogicMods mod for this game in IoStore form (none on the Mac; the user supplies one; the
   third-person mod's Blueprints are not suitable, one of them writes saves).
2. Runtime patch for gaps 1-3 (generic, no game names), one independent review, gates, one build.
3. Install the BP pak with the #7 route and stage `paks/logicmods/<mod>` (exact mirror, row-check).
4. One launch. Pass markers: `[BPModLoaderMod] Loading mod: <name>`, `Actor: ...`,
   `Executing 'PreBeginPlay' for mod '<name>'`, no Lua errors, self-test unchanged.

## 3. Save-game mods

### 3.1 Where the PS5 build keeps saves (Mac-only, from the game's own files)

- The executable imports `libSceSaveData` and `libSceSaveDataDialog` (module names in its import strings).
- The game's PS5 config sets the save-container size: `Townfall/Platforms/PS5/Config/PS5Engine.ini`
  `[/Script/PS5PlatformEditor.PS5TargetSettings] DefaultSaveSizeBytes=25000000`.
- The game ships a save icon staged for PS5 (`PS5Game.ini +DirectoriesToAlwaysStageAsUFS=PS5/SaveIcons`,
  `Townfall/Content/PS5/SaveIcons/SaveData.png`), and the executable names that path.
- So saves are **system save-data containers**, not files the game writes into app or data folders.
  The engine's generic file save code (`%sSaveGames/%s.sav`) is linked too, as in every build; which one
  is active is decided by the platform layer (not checked further: no platform-extension material).
- The game's save logic is a plugin, `NCSaveGame` / `NCSaveGameCore`, on top of the engine's standard
  functions. Its PS5 ScriptObjects list `SaveGameSubsystem` (SetSaveGameSlot, GetNumPlayerSaveSlots,
  HasSaveData, ClearSaveData, ...), `SaveData` (the save class), `SaveHelperComponent`, and the engine's
  `LocalPlayerSaveGame` / `GameplayStatics` save functions (LoadGameFromSlot, SaveGameToSlot,
  DoesSaveGameExist, DeleteGameInSlot, CreateSaveGameObject).

### 3.2 PC save mods

A PC save mod is a `.sav` file dropped into `%LOCALAPPDATA%/<Game>/Saved/SaveGames/`. The file is the
engine's `SaveGameToMemory` output ("GVAS"): header (magic, save-file version, package version,
engine version, custom versions, save class name) and the save object's **tagged** properties (names
and types written out, unlike cooked packages).

### 3.3 Is it platform-identical?

**Yes for the format (Mac-only inference):** `SaveGameToSlot` -> `SaveGameToMemory` is generic engine
code; the bytes handed to the platform save system are the same GVAS on PC and PS5 (both
little-endian 64-bit). The platform layer only decides **where** the bytes are stored. Not proven:
whether the PS5 save layer wraps or compresses them (does not matter for an in-process import, which
goes through the same engine functions), and whether the game adds its own checks (the save class
carries a data version, `LocalPlayerSaveGame.GetSavedDataVersion`).

### 3.4 In-process import (design, not built)

The engine's byte-level loader (`LoadGameFromMemory`, `SaveDataToSlot`) is **C++-only**: these names
are absent from the executable's reflection strings, so Lua cannot call them. The reflected functions
always go through a slot (LoadGameFromSlot / SaveGameToSlot / DoesSaveGameExist / DeleteGameInSlot).
So: "read a PC save without touching the container" is possible; "make it the player's save" always
writes through the engine into the container (the safe way, the same path the game uses).

Steps (S1-S5 are console actions: each needs the user's explicit OK for that action):

| Step | Where | What | Writes? |
|---|---|---|---|
| S0 | Mac | `tools/gvas.py <file.sav> --expect-engine 5.6.1 --schema <S1 dump>`: GVAS magic, engine version, save class = the game's, every property exists with the same type; refuse anything else. | no |
| S1 | console (user OK: uploads a diagnostic C++ mod and needs a launch; game at the main menu, no save in progress) | Read-only dump: find `SaveGameSubsystem`, read its slot properties, `DoesSaveGameExist` + `LoadGameFromSlot` for each slot, and log every property name, type **and value** of each loaded save object. The values are the **logical backup** of every existing slot. Needs a property-list helper (UE4SS C++ API `ForEachProperty` in a C++ mod, #9 proven; the Lua port has no `ForEachProperty`). | no |
| S2 | console -> Mac (read-only FTP, user OK, "game closed?" first) | Copy the title's save-data folder as the console exposes it, with a sha256 manifest, into the evidence tree. This is a record only: nothing shows such a copy can be put back, so it is **not** an undo. If the console's own save-data menu offers a copy, the user makes one too. | no |
| S3 | console (user OK) | Import: `CreateSaveGameObject(<save class>)` (or load the target slot), apply the PC values decoded on the Mac (a derived Lua data file; the `.sav` is untouched) through reflection, `SaveGameToSlot(<slot>, user 0)`. Default target: a **new** slot name, only if `DoesSaveGameExist` says it is free. Replacing an existing slot is a separate user decision, made after S1/S2. | yes, via the engine |
| S4 | console | Read back: `LoadGameFromSlot(<slot>)`, compare every applied property. On a mismatch in a **replaced** slot: write back the in-memory original object loaded in S3 and report. On a mismatch in a **new** slot: stop and report; the extra slot stays (nothing is ever deleted) and the user decides. | only on a replaced-slot failure |
| S5 | console | Normal game load of that slot; the user checks it. | no |

**Undo:** there is no file-level undo. A replaced slot can only be brought back by importing its S1
logical backup again through S3 (user OK), or from a copy the user made with the console's own menu.

Fail closed when: no GVAS magic, engine/minor version differs (a different game version is out of
scope), unknown save class, a property the game class does not have or with another type, a type the
reflection applier does not support (maps of structs, object references, FText are refused at first),
`SaveGameToSlot` returns false, or any read-back mismatch.

### 3.5 Safety rules

- Never write the save storage directly; never upload or delete files in the save folder; never
  delete a slot.
- Before any import: S1 logical backup of every slot and the S2 record, kept in the evidence tree.
- One slot per import, one import per launch, "game closed?" before every log or save-folder read.
- Mods that touch saves themselves are flagged by the classifier (G8: engine slot functions, any
  function of a `*Save*` class, `Save/Delete/Clear/...(Game|Data|Slot)` functions, save-game classes).
  Example: the third-person mod's `BP_star` calls `CreateSaveGameObject` / `SaveGameToSlot` /
  `LoadGameFromSlot` with slot `"111"`, and `BP_SAVE` is its own save class (a `LocalPlayerSaveGame`
  subclass with 10 position fields). On PS5 that would create a new slot in the user's save data:
  ask the user before installing it.

## 4. Prototype: `tools/classify_mod.py`

Generic (only engine/plugin class names), read-only, fail-closed. Checks per package:
G1 identity + flags, G2 every script import exists in the PS5 build, G3 every imported package is in
the mod or the game, G4 structure + bulk flags, G5 no PC shader code outside materials, G6 class rule
(`tools/asset_rules.py`) + deep check (texture layout via `texture_package.layout`; materials via
`mod_material_audit.audit_packages`; skeletal-mesh / data / Blueprint classes are "likely" only with
same-shape pair evidence and a bulk signature seen on a console sample; every other export class must
be in the kind's allowlist), G7 a replaced game asset must have a provable, equal class (registry),
G8 script that touches save data. `--target` is required (run_all reads it from the game's `.stinfo`). Uses the existing
readers by import and records their sha256.

Results (Mac-only; `evidence/classify-*.md`):

| Input | Packages | AS-IS | LIKELY | CONVERT | UNKNOWN | UNSUPPORTED |
|---|---|---|---|---|---|---|
| Leon (PC outfit) | 127 | 0 | 26 (25 skeletal meshes, 1 skin profile) | 93 (34 tiling, 59 material-reparent) | 8 | 0 |
| Third-person view (PC) | 23 | 0 | 4 | 6 | 13 | 0 |
| Movement speed 2x (PC data table) | 1 | 0 | 0 | 0 | 1 (no PS5 data-table sample yet) | 0 |
| Control: the game's own PS5 DLC container | 10 | 5 (textures already in PS5 layout) | 5 | 0 | 0 | 0 |

- Leon's 8 UNKNOWN: the `Simon_Slot0..7` placeholder materials import
  `/InterchangeAssets/Functions/MF_PhongToMetalRoughness`, which is in neither the mod nor the game
  (the loader skips it with a warning; Team A showed these are 1 mm hidden triangles).
- Third-person UNKNOWN: 3 packages put a **Material under the name of a game static mesh** (IV bag
  parts: a PC hide trick, G7); Blueprints / physics / IK rig wait for F1 samples; `BP_star` writes saves.
- Control: the game's own files are never marked unsupported; its textures read as already in PS5
  layout (AS-IS). Without the registry the same files all become UNKNOWN (G7 cannot prove the class).
- Tests: `python3 -I tests/test_teamj.py` -> 26 tests OK (rules, tokens, pak footer, shape logic incl.
  per-sample signatures / unreadable structure / decoder rule, GVAS read and refusals, bank-name
  filtering, real Leon counts with and without pair evidence, mutations that must fail closed: editor
  flag, unknown class, wrong chunk id, injected PC shader code, uncovered sub-object class, class change,
  missing registry; generic save-function flags; control; fetch plan).
- Known limits: G3 checks imported packages, not each imported public export hash; LIKELY rests on
  structure plus earlier user-visual launches, never on decoded property values.
- Re-run everything: `./run_all.sh` (first run about 6 minutes for BLAKE3 of every console file, then
  about 2.5 minutes with the receipt; writes only `evidence/` and `fetch-plans/`).

## 5. Fetch plans (read-only; the integrator runs them)

- **F1** `fetch-plans/F1-type-samples.json`: 72 whole chunks, 4.6 MB, 61 game packages from
  `pakchunk0-ps5.ucas` (REST + RETR only), each with the TOC IoHash. First rows: the game's own copies
  of the assets the non-Leon PC samples replace (`Paces_DT`, the 3 IV-bag static meshes,
  `BP_StreamingSource`; taken from the classifier reports, not hand-written); then for each open class the smallest and a median-size sample (static mesh Nanite/non-Nanite, physics asset,
  data table, Blueprint, widget, animation, montage, codec settings, Niagara GPU/CPU, sound wave/cue,
  FMOD stubs, blend space, pose, curves, struct, enum, physical material, geometry collection, groom,
  level sequence, cube texture, font, material function).
  Run: `harvest/ftp_range_fetch.py --plan F1-type-samples.json --remote-dir <paks dir> --out <evidence dir>`,
  then `tools/make_sample_plan.py verify --plan ... --dir <evidence dir>` (length, BLAKE3 IoHash, name
  hash), then `SAMPLES=<evidence dir> ./run_all.sh`.
- **F2** (audio banks): first a read-only FTP LIST of the game's PS5 bank folder (game closed), then
  `tools/make_sample_plan.py bank-plan --list LIST.txt --out F2.json` plans the first 64 KiB of the two
  smallest `.bank` files (codec header).

## 6. Next steps

1. Integrator: run F1 (read-only), verify, re-run `run_all.sh` -> data tables, physics, Blueprints,
   animations, static meshes, Niagara get evidence-based verdicts (Mac-only).
2. Ask the user for one PC mod of each missing kind: a static-mesh/prop replacement, an animation
   replacement, a LogicMods Blueprint mod, an audio mod, a PC `.sav` save mod (same game version).
3. One console test per "likely" class, riding on an existing capability launch (data table: the
   movement-speed mod via the #7 route is the cheapest).
4. Blueprint logic: runtime patch for gaps 1-3 + one reviewer + one build + one launch (section 2).
5. Saves: S1 read-only diag first; build S0-S4 only after S1 shows the save class and slots.
6. Wire `classify_mod.py` into the mod manager as the first step of install (refuse UNSUPPORTED /
   UNKNOWN by default; ask the user for G8 save writers).

## 7. Files

- `DESIGN.md` (this file), `run_all.sh`
- `tools/jlib.py` (shared readers glue), `tools/asset_rules.py` (rule table), `tools/classify_mod.py`
  (classifier), `tools/pair_compare.py` (PC vs PS5 evidence), `tools/make_sample_plan.py` (F1/F2 plans +
  verify), `tools/gvas.py` (PC save checker, not yet run on a real save)
- `tests/test_teamj.py`
- `evidence/` (pair compare + IoHash receipt, 4 classifier reports, target, localization check, plan
  log), `fetch-plans/F1-type-samples.json`
- `REVIEW.md` (the independent review and the fix for each finding)
