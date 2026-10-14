# Leon on PS5: grey/white checker and bright outdoors (diagnosis)

Team A, 2026-10-12. Mac-only work: no console contact, no build, no deploy.
Each claim is labelled **Mac-only** (from files on the Mac) or
**console-proven** (with the log line it comes from). The two user observations
are quoted from the launch 13/14 reports and the user's message. They are
observations, not log lines.

## Plain summary

1. **Checker pattern on Leon: cause found (strong Mac-only evidence).**
   The Leon mod contains no PS5 shader code. Every one of its 67 materials was
   built for PC DirectX only (`PCD3D_SM5` / `PCD3D_SM6`, DXBC/DXIL code). When
   the PS5 finds no usable shader for a material, Unreal draws the engine's
   default material instead. That default material is the grey/white checker
   (`WorldGridMaterial`). Leon's textures have nothing to do with the checker:
   the default material ignores them.
2. **Bright outdoors: the Leon files are probably not the cause.** The mod
   changes no light, fog, sky, post-process or exposure asset. None of the game
   assets that depend on the mod's files is a scene-wide asset either. The launch-14
   log shows the runtime's LoadMap detour stayed off (console-proven); that the
   runtime has no other engine detours rests on the build-time no-detour gate
   (Mac-only). The Lua mods make no lighting calls.
   **Update 2026-10-13 (integrator, Mac-only analysis of files pulled from the console):**
   the brightness is explained by a changed `PS5Engine.ini` inside the modified
   `pakchunk0-ps5.pak` (fog, auto-exposure and most shadows off, gamma raised). See
   `../CONSOLE-FACTS.md`. LAUNCH-CARD.md is now a clean "Leon off" baseline check.
3. **Textures (your colour/format question):** the mod's textures use ordinary
   PC formats (DXT1, BC5, one DXT5) in the standard PC layout. The sRGB use looks
   normal. The game's own PS5 textures are stored differently: most are virtual
   textures, and the rest carry a ~64 KiB packed "mip tail". So once the shader
   problem is fixed, Leon's textures may still look scrambled. That is the next
   question, and one launch will answer it.

## H1: grey/white checker = no PS5 shader for any Leon material (rank 1, strong)

Evidence (Mac-only; `tools/texture_diag.py` → `out/texture-diag-report.json`; tests in `tests/`):

| Fact | Value | Source |
|---|---|---|
| Shader platform the game ships | `SF_PS5` / `SP_PS5` only (`Engine/GlobalShaderCache-SP_PS5.bin`, `ShaderTypeInfo-*-SF_PS5-SP_PS5.stinfo`) | game's own `pakchunk0-ps5.pak` listing |
| Shader platform in every mod material | `PCD3D_SM5` + `PCD3D_SM6` strings, DXBC/DXIL shader code in all 13 base materials (`M_Leon_Body`, `M_Leon_Hair`, `M_Leon_Skin`, `Simon_Slot0..7`, 2× `M_Invisible`) | mod export payloads |
| PS5 shader-platform tokens in the mod | 0 | all 127 mod packages |
| Shader-map hashes in the mod's package store | 0 of 127 entries (the game's own store has PS5 hashes for its materials) | mod ContainerHeader vs base ContainerHeader |
| Shader library chunks in the mod | 0 (`ShaderCodeLibrary`/`ShaderCode` absent; the game has 2 + 2,579) | TOC chunk types |
| Mod materials judged | **67 / 67** with no PS5 shader: 13 have PC-only code themselves, 54 are instances of those 13 | class names resolved through the game's own ScriptObjects |
| Same result for the 4K variant | yes, 67 / 67 | `zzz_Leon_Townfall_4K_P` |
| Engine fallback material on PS5 | `/Engine/EngineMaterials/WorldGridMaterial` is present with 1 PS5 shader hash | base TOC + store |

Leon's visible surfaces all route to these materials. Every body mesh imports
`MI_Leon_{UpperBody,LowerBody,Props,Knife,Pistol,Watches,…}` → parent
`M_Leon_Body`/`M_Leon_Skin`/`M_Leon_Hair` (PC-only), plus `Simon_Slot0..7`
(PC-only). So the whole character renders with the default checker. This
matches the user's report.

Not locally pinned: the exact engine branch that swaps in the default
material. The local Epic source subset has no material/shader code, and the
shipping executable logs no UE log lines (agent GM, earlier work). The
fallback-to-`WorldGridMaterial` behaviour is general UE knowledge that the
observation agrees with. Two earlier claims are superseded:
- An older note said Leon's own materials "appeared acceptable" in a screenshot.
  That screenshot was lost and its origin is unknown. The user's current
  report contradicts it.
- The earlier classifier counted 5 PC-only base materials. Resolving the real
  classes finds **13**: the 8 `Simon_Slot*` materials were missed because their
  names do not start with `M_`.

Blast radius beyond Leon (Mac-only, from the game's own store imports): game
assets that use the mod's overridden materials will also show the checker. These
include the enemy mesh `TheWall/SK_TheWall_SimonHead` (eye materials), the canula
props (`M_Medical_Liquid`), `Simon_Bodywarmer_Bed`, the IV-bag VFX
(`P_IV_BagLiquid`, `Bottle_Liquid`), and several Chapter 4/8 cinematics.

## H2: bright outdoors (rank: not the pak, pending one A/B launch)

| Check | Result |
|---|---|
| Mod assets outside Leon's own folder | 78 packages, every one of them Simon character content (meshes, grooms, MetaHuman face/eye/teeth/hair materials, IV-bag/medical materials, `MI_Liquid_Oil_01`) (Mac-only) |
| Any post-process / fog / sky / light / exposure / LUT / material-parameter-collection / level asset in the mod | **none** (class-based check, `scene_wide_class` empty for all 78) (Mac-only) |
| Game assets that import the 68 overridden packages | characters, props, cinematics, groom bindings, 1 CCTV BP, 1 interaction zone. No lighting or fog asset (Mac-only, pakchunk0 store; pakchunk10's store is not captured and has 2 overlaps, both Simon outfit items) |
| Engine detours from our runtime | LoadMap detour off: `[R3] LoadMap detour registrations deferred … InitGameState/BeginPlay/… stay unregistered`, and `LoadMap deferred arm refused` (console-proven, launch 14 `ue4ss.decoded.log` lines 794 and 941). Those lines show only the LoadMap detour; "no other detours" rests on the build-time no-detour gate (Mac-only) |
| Lua mods (7 started, console-proven line 824-904) calling fog/exposure/post-process/console `r.*` | none in CheatManagerEnabler, ConsoleEnabler, SpeedHack, ItemSpawner, MiniMap. ueselftest only runs a no-op `st2_noop_cmd` (Mac-only grep) |

What remains, all unproven:
- (a) The game's normal PS5 look, or the in-game brightness/HDR setting,
  differs from the user's expectation.
- (b) A second pak mod is present in `~mods`. We have no listing of the
  console's paks folder.
- (c) A weak mod-linked effect: Simon's head, hair, glasses and IV-bag parts
  were meant to be invisible. They now draw as solid checker geometry next to
  the camera. That can change shadows and exposure near the player, but it does
  not plausibly light up a whole outdoor scene.

The user first mentioned brightness in launch 13, the first launch where the
data-only world-event hooks ran. Those hooks only observe objects after the
event and patch nothing, so that is a weak correlation. The A/B launch tests
(a)-(c) at once.

## H3: texture colour space / format / layout (rank: not the checker cause; likely a follow-up problem)

Mod textures (Mac-only, 34 per variant, all parsed and self-validated):

| Kind | Format | Sizes (2K pak) | sRGB (inferred) |
|---|---|---|---|
| `_D` base colour | `PF_DXT1` (one `PF_DXT5`) | 2048², 1024², 512², 128² | on (default, not overridden) |
| `_N` normal | `PF_BC5` | same | off (property 19 = 0) |
| `_ORM` / `_AO` masks | `PF_DXT1` | same | off |

The sRGB inference reads unversioned property indices. Property names need the
class schema, which is not on the Mac (pattern `4,5,8,15,19`; values N=`01 01 00`,
ORM=`02 00`). Every mod mip is an exact linear, unpadded BC size: 34/34 match.
The top 5 mips sit in `.ubulk` and the rest sit inline (about 3 KB).

The game's own PS5 textures (Mac-only, TOC sizes for 8,001 base textures; `out/base-texture-layout.json`):
- **6,574** have tiny exports (<4 KiB) and bulk sizes that are not linear. The
  four largest size groups (6,050 textures) fit a virtual-texture tile model
  exactly: 128 px tiles, 4 px border, 16-byte blocks, and an identical
  129,464 + 4×levels byte remainder in every group. That makes them **virtual
  textures** (a generic UE format).
- **736** have linear-sized bulk data but a **60-70 KiB** export. The mod's
  textures have 2.4-6.5 KiB exports. This points to one ~64 KiB packed mip-tail
  block per texture, which is typical of a GPU-tiled layout.
- Base D/N/MRO textures of the same size have identical bulk sizes, so the game
  uses 16-byte-block formats for all three (BC7/BC5-class). The mod uses
  8-byte DXT1 for D/ORM. Both are valid BC formats, so this difference alone
  breaks nothing.

Conclusion: the textures do not cause the checker. Whether PC-layout texture
data displays correctly on PS5 is still **open**. The first launch with a fixed
material will show it. The fetch of `T_Shirt_D` (below) shows the PS5 header
fields (tail size, extra flags) on the Mac first.

## Found on the way (affects the staged #8 route)

- **The existing "closest-match" repoint (overlay_builder) cannot work as
  written.** Mac-only, verified on all 127 real packages:
  - `PackageId = CityHash64(UTF-16LE(lowercase package name))`.
  - The store entry's import list equals the package's own `ImportedPackageNames`,
    hashed, in the same order.
  - An import points at the parent by `PublicExportHash = CityHash64(UTF-16LE(lowercase export name))`.

  A store-only repoint therefore leaves the package asking for `M_Invisible`'s
  (or `M_Leon_Body`'s) export inside a base package that does not have it. The
  parent cannot be resolved. Staged fix: a fail-closed guard (PATCH) plus
  `tools/zen_import_rewrite.py`, which makes the needed derived package copy.
  Both are tested, and the patched suite passes 32/32.
- **The two SUSPECT files are reproducible test outputs.** `leon-replay-drift.json`
  and `leon-substitutions-fixture.json` are rewritten by `test_overlay_builder.py`
  on every run (lines 757 and ~1465 write into its own folder). A relocated run
  of the unmodified suite reproduces both files byte for byte (sha256 `63ceec0e…`
  and `597af39a…`). They are still not used as evidence here.

## Fix plan, split by capability

**#7: pak mounting / overlay (no PS5 code change)**
1. Run `texture_diag.py` (material gate) at install / first launch of any pak
   mod. Report "N materials have no PS5 shader → will draw as checker" before
   mounting. It is count-gated and generic.
2. Apply the closest-match guard (PATCH/overlay_builder-closest-match-guard.diff).
3. Stop the overlay test suite from rewriting files in the evidence folder:
   done in the staged diff. Both writes are gone from its own folder (the
   drift receipt goes to `$OVERLAY_TEST_OUT` or a temp folder).

**#8: material compatibility, data-only derived overlay (mod files untouched)**

`tools/material_plan.py` → `out/material-plan.json` chooses everything from the
game's own data:
- **A, 43 overrides of the game's own Simon materials.** The existing "hide"
  directive suppresses the override, so the game's own PS5 material comes back.
  The catch: Simon's hair, beard, glasses and IV bag reappear on Leon. The
  better variant repoints them to the game's own hide material. That material is
  found from data: `/Game/MetaHumans/Common/Materials/M_Hide`, which the base
  face mesh itself imports. It uses `zen_import_rewrite.py`, and the parameters
  must be checked first.
- **B, 11 new Leon material instances.** (Static-permutation check, Mac-only: all 11 have
  0 own shader tokens and 0 own store hashes, so none carries its own permutation and
  reparenting can give them a shader map; see `static_permutation_check` in the plan.) Reparent them to the master that the
  game itself uses for the meshes this mod replaces:
  `/Game/Townfall/Characters/Materials/M_MRO_Cloth_NonVT` (32 votes). It is a
  non-virtual-texture master, which suits Leon's non-VT textures. The VT-based
  `M_SkinBasic_Hands` probably does not suit them. Leon's parameter names are
  `BaseColor`, `Normal`, `ORM` (+`HairAO`). The base master's names are still
  unknown, so this is **blocked on the fetch below**. If the names differ,
  rename the FName in the derived copy. Disclosed visual loss: cloth shading
  everywhere (skin and hair too), and ORM vs MRO channel order may differ.
- **C, 13 new base materials used directly by mod meshes** (`Simon_Slot0..7`,
  `M_Leon_*`, `M_Invisible`). Rewrite the mesh imports the same way, or leave
  them (checker). Slot usage is not known on the Mac.

**#12: translation (blocked / out of bounds)**
- PC DXIL → PS5 shader translation is not possible within the rules: no
  platform compiler, no Sony material. The B route avoids it.
- PC texture layout → PS5 layout: only needed if the #8 launch shows scrambled
  textures. It can only be derived from the game's own files, so it needs a
  matched pair of the same texture in PC and PS5 form. We have none. Blocked.

## Exact console files needed (read-only; the integrator fetches)

| # | What | Where (console) | Bytes | Why |
|---|---|---|---|---|
| 1 | LIST (names + sizes) | the game's whole `content/paks/` folder, subfolders included | listing | which Leon variant (2K/4K) is installed, and whether any other pak mod is present (H2-b) |
| 2 | raw range | `…/paks/pakchunk0-ps5.ucas` offset 106,040,048 len 66,639 (IoHash20 `6ef425ef7e97…`) | 66,639 | `T_Shirt_D` PS5 non-VT texture header: tail and extra fields (H3) |
| 3 | raw range | same file offset 384,114,640 len 1,349 (`96d5d2326d12…`) | 1,349 | `T_Simon_Body_FirstStage_D`, to confirm the VT hypothesis |
| 4 | raw range | same file offset 522,552,000 len 31,028 (`1f95f7c1d629…`) | 31,028 | `M_MRO_Cloth_NonVT` parameter names (#8 B) |
| 5 | raw range | same file offset 522,743,488 len 23,578 (`1bf0569b2e88…`) | 23,578 | `MI_Simon_Shirt`, which parameters the game binds on that master |
| 6 | raw range | same file offset 1,148,314,144 len 21,188 (`55738e6c2560…`) | 21,188 | `M_Hide`, the game's own hide material (#8 A variant) |
| 7 | raw range | `…/paks/pakchunk10-ps5.ucas` offset 113,396,304 len 416 (IoHash20 `c5e339d2dab5…`) | 416 | pakchunk10 ContainerHeader (the 2 overlaps' base store entries) |

Rows 2-6 come from `out/base-fetch-plan.json`, with full IoHashes. All blocks
are uncompressed. Check each fetched file with:
`python3 tools/base_fetch_plan.py verify --utoc <pakchunk0-ps5.utoc> --toc-index <i> --fetched <file> --global-utoc … --global-ucas …`.
