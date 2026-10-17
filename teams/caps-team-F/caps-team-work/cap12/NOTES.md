# Capability #12 (shaders) — research notes, Team F

2026-10-12. **All findings here are Mac-only** (read from the game's and the
mod's own files already on the Mac). Nothing was run on the console, and there
are no new console results in this note.

## 1. Plain answer

- **Why Leon looks grey/white checkered:** every one of the Leon mod's 67
  materials has shader code for Windows PCs only (`PCD3D_SM5`/`PCD3D_SM6`) and
  none for PS5 (`SF_PS5`). When a material has no shader for the platform, the
  engine draws its built-in default material instead, which is the grey/white
  checker grid. The mod also ships zero shader libraries. This explains the
  checker on its own, whatever happens with the textures (the default material
  never samples the mod's textures). Mac-only. Earlier agent GM reached the
  same conclusion; my count is 67, not GM's 59 (GM left out 8 `Simon_Slot0..7`
  materials).
- **Why it is bright outside:** this is a different cause and not a shader
  problem. The console's `pakchunk0-ps5.pak` has a changed
  `Platforms/PS5/Config/PS5Engine.ini` with volumetric fog off, auto exposure
  off, a brighter tonemapper and weaker shadows. Another team found this
  (`l25-prep/evidence/console-pulls-20261013-paks/FINDING-bright-outdoors.md`).
  I spot-checked it: the current pak has one more `r.VolumetricFog=0` string
  than the `.bak`, plus an extra `r.EyeAdaptationQuality=0` and
  `OverrideSwapChainDX12` (Mac-only string counts on the pulled files).
- **Can #12 fix the checker without a PS5 shader compiler?** Partly, yes: the
  mod's materials can be pointed at the game's own compiled PS5 materials
  (section 6). Translating the mod's own PC shaders into PS5 shaders is not
  feasible under the project rules.

## 2. Where Townfall's PS5 shader code lives (Mac-only, game's own files)

| What | Where | Size |
|---|---|---|
| Shader library manifests (IoStore chunk type 8) | `pakchunk0-ps5.ucas` physical `[0, 367536)` = **Global**; `[367536, 1795544)` = **Townfall_Chunk0**; `pakchunk11-ps5.ucas [0,3060)` = Townfall_Chunk11; `pakchunk12-ps5.ucas [0,4700)` = Townfall_Chunk12 | 1.80 MB in all |
| Shader code groups (chunk type 9) | pakchunk0: 2,579 groups / 155,866,613 B; pakchunk11: 3 / 153,939 B; pakchunk12: 8 / 306,282 B | 156 MB |
| Which packages use which shader maps | container header (type 6) store entries: 38,439 packages, 5,387 shader-map refs, 1,500 unique maps (local `container-header.bin`, IoHash verified against the TOC) | 2.27 MB |
| Global shader metadata | `Engine/GlobalShaderCache-SP_PS5.bin` (in `pakchunk0-ps5.pak`); `ShaderTypeInfo-{Global,Townfall_Chunk0}-SF_PS5-SP_PS5.stinfo`; `PipelineCaches/PS5/Townfall_SF_PS5.stable.upipelinecache` | small |

All containers use compression method None (64 KiB blocks), so every piece is a
plain byte range. Physical ranges come from my own TOC reader
(`tools/utoc.py`). They agree with ChatGPT's catalogue, and they reproduce the
offset of the earlier real container-header capture (128,238,104,864 = capture
receipt `physical_offset`; IoHash also matches).

**Correction to agent HF's fetch plan (CAP12-SHADER-DESIGN.md 1b):** HF gave
"393,216" as a ucas offset, but that is the logical offset. The physical bytes
start at **367,536**. A fetch of `[393216, 1821224)` would return the wrong
bytes. The IoHash check would catch it, but it would cost a wasted fetch.

## 3. Library identity law: solved (two independent sources)

The type-8 chunk id is `CityHash64(lower(UTF-16LE("<Library>-<Format>-<Platform>")))`,
followed by index 0 and type byte 8. For Townfall the input string is
`"<Lib>-SF_PS5-SP_PS5"`. The names come from the game's own `.stinfo` file
names, and the ids from the game's own TOCs. All 4 type-8 ids are reproduced
(Global, Townfall_Chunk0/11/12). retoc's own comment explains it:
UnrealPak splits `ShaderArchive-<Lib>-<Fmt>-<Plat>` at the last `-`
(`public-retoc-source/shader_library.rs:1086-1097`). HF and ChatGPT both
recorded this as unknown ("brute-force no-hit" / "preimage remains unknown").
It is now closed. `tools/shader_inputs.py` applies the law generically: library
names come from the game's own side files plus `<Project>_Chunk<N>` from the
TOC names, and no names are hard-coded.

Supporting laws I verified (Mac-only):

- Package id = CityHash64(lower UTF-16 path): 127/127 Leon, 13/13 chunk11,
  and more than 36,000 base `/Game` and `/Engine` packages.
- Public export hash = CityHash64(lower UTF-16 export name): 127/127 Leon
  exports.
- Name-batch hashes: my writer reproduces the original header bytes exactly
  for 127/127 Leon packages.
- TOC IoHash = BLAKE3 truncated to 20 bytes: matches the spec vector and every
  small Leon chunk.
- The store entry's import order equals the order of the header's
  `ImportedPackageNames` for 127/127 Leon packages.

## 4. Manifest and group format (public retoc facts, own implementation)

- Type 8, version 1 (`tools/shader_library.py`): a u32 version, then 7 arrays,
  each with a u32 count: map hashes, shader hashes, group chunk ids, map
  entries, packed shader entries (4-bit frequency, 30-bit group index, 30-bit
  offset in group), group entries (index offset, count, uncompressed size,
  compressed size), and the index array.
- Type-9 group id = `SHA1(Σ member hash ‖ u32le member size ‖ format string)[0:11]`
  plus type byte 9. Following retoc's split rule, the format string should be
  `SP_PS5`. The verifier tries the candidates derived from the library name
  and accepts only one that reproduces **every** group id. This gives a third,
  internal check of the decoder on real data.
- Not yet run on real bytes: the type-8 payloads are only on the console.
- The group compression codec is unknown, but reuse does not need it.

## 5. What a PC mod's material needs on PS5

A material renders on PS5 only if a shader map for SF_PS5/SP_PS5 exists and
the engine can find it. That means one of:

- (a) the mod's MaterialInstance has no shaders of its own, and its parent
  chain ends at a **base-game** material whose shader maps are in the game's
  libraries, or
- (b) the mod ships its own PS5 shader library.

PC mods never do (b): their material packages carry only inline PC shader
maps. They manage (a) only if they reuse base parents.

Leon audit (`evidence/leon-material-audit.json`, `tools/mod_material_audit.py`):

| Verdict | Count | Meaning |
|---|---|---|
| NO_TARGET_SHADERS | 13 Materials | `M_Leon_Body/Skin/Hair`, 2× `M_Invisible`, `Simon_Slot0..7`: PC shader data only |
| INHERITS_MISSING | 54 MICs | all chain to one of the 13 above |
| OK_BASE_SHADERS | 0 | none |

43 of the 67 override base-game packages, and 24 of those had their own PS5
shader maps in the base game. Overriding them removes those shader maps. The
Simon hair, beard and eyelash card meshes in the mod import
`/Engine/EngineMaterials/WorldGridMaterial` directly (the checker material
itself, which has PS5 shaders).

Mod textures are PC-cooked: `PF_DXT1` ×22, `PF_BC5` ×11, `PF_DXT5` ×1.
Whether PS5 can sample them as they are is a separate #8/Team A question. It
becomes visible only once the materials are fixed.

## 6. Feasibility verdict for #12

| Route | Verdict | Why |
|---|---|---|
| Translate the mod's PC shaders (DXBC/DXIL) into PS5 shaders | **Not feasible now (BLOCKED)** | It would need the PS5 GPU binary format, the PS5 RHI's resource-binding layout, and the exact PS5 permutation set. All three are Sony-platform-private. Learning them only from the game's binaries means reverse-engineering a proprietary GPU and driver ABI, where failures are GPU hangs with no debugger. The project rules forbid Sony SDK material. The mod's PC maps also hold only SM5/SM6 permutations. HF's DXIL→SPIR-V→RDNA2 plan stays research-only. I recommend not scheduling it before Stellar Blade. |
| **Restore base** for overridden base materials (overlay `hide`, already in overlay_builder) | **Feasible now** | Base wins, so its PS5 shader maps come back. The mod's intent is lost for those slots (for example, Simon's hidden parts become visible again). |
| **Re-parent** mod MaterialInstances to a base material that has PS5 shaders | **Feasible, staged** | Data-only. `reparent_plan.py` picks the base parent by parameter-name match and fails closed. `mic_reparent.py` writes the derived package: only the import hash and import name change, and export bytes are identical (tested on real Leon packages, including parameter renames with header relayout). The overlay must ship derived package bytes (APPLY.md step 3). Leon's MIC parameters are plain PBR (`BaseColor`, `Normal`, `ORM`, plus `HairAO`), so a base character master (for example, one of the base `Characters/Materials/M_*` packages, which have PS5 shader maps) is a likely target. The final choice is made automatically from the fetched base headers. The look will be close but not identical: disclosed visual loss. |
| Mod UMaterials used directly by meshes (`Simon_Slot0..7`), and the 43 parameterless MICs under the two `M_Invisible` | **No staged fix: `known-checker`** (after review F4) | A Material graph cannot be rebuilt without a shader compiler, and a parameterless MIC gives nothing to match. "Invisible" cannot be proven from the files, so pointing these at a visible base material could show parts the mod hides. `reparent_plan.py` lists them as `known-checker`; restore-base only on request (`--allow-restore-base`, mod intent lost). |

## 7. Open questions and what unblocks each

1. **The real type-8 bytes.** These are 4 read-only range fetches (1.8 MB)
   listed in `needs_from_integrator`. After fetching, run `verify_fetch.py`;
   it must PASS (IoHash, structure, every group id recomputed), and it
   reports how many of the 1,500 store maps it finds (expected: all).
2. **The base materials' parameter names.** These come from round 1 (68 base
   packages, 2.88 MB) and round 2 (their PS5-shader imports). Then run
   `reparent_plan.py`.
3. **Does the engine resolve a re-parented import at load?** Mac-only
   reasoning says yes once both the header hash and the store entry agree.
   Only a launch proves it, and that launch is the #12 card.
4. **Do PC-cooked BC textures display on PS5** once the material works? This
   is #8/Team A's question, answered by the same launch: textures will look
   either correct or garbled.
5. Townfall's Shipping build has no engine log strings (GM's earlier eboot
   search). Proof therefore comes from our probe (`zzmatdiag` `[mat12]`
   lines) plus what the user sees.

## 8. Prior work re-checked

| Claim (source) | Status |
|---|---|
| 2 + 2,579 type-8/9 rows in chunk0; 1/3 and 1/8 in chunk11/12 (ChatGPT, HF) | Reproduced |
| 5,387 store refs, 1,500 unique (HF) | Reproduced |
| Library name preimage unknown (ChatGPT, HF) | **Superseded**: solved, see section 3 |
| Manifest at ucas offset 393,216 (HF) | **Wrong** (that is the logical offset); physical is 367,536 |
| Leon: 59 material-family, all broken (GM) | All broken holds; the count is **67** (GM missed `Simon_Slot0..7`) |
| `closest-match` repoint fixes the parent (overlay HI) | **Incomplete**: it changes only the store entry; the header hash must change too (APPLY.md step 3) |
| Leon has no type-8/9 chunks (all) | Reproduced |
| Suspect files `leon-replay-drift.json` and `leon-substitutions-fixture.json` | Not used |

## 9. Evidence index (this folder; copy to the durable tree per APPLY.md)

- `evidence/townfall-shader-inputs.json`: libraries, ranges, IoHashes, fetch
  plan (shader_inputs.py PASS, 5 TOCs, 4,699 names, 4/4 type-8 rows
  resolved).
- `evidence/leon-material-audit.json`: 127 packages, 67 materials, per-material
  verdict and why.
- `evidence/fetch-round1-overridden-base-packages.json`: 68 rows, 37 whole
  chunks (IoHash-verifiable) and 31 header-only (name-hash-verifiable).
- `tests/test-output.txt`: 39 Python tests OK, plus the Lua probe test (see FIXES.md for the review fixes).

Inputs (read-only):

- `.superpowers/native-observations/{townfall-main-utoc-2026-10-01, townfall-original-container-header-1, townfall-manifest-capture-2026-10-01, townfall-global-scriptobjects-1, leon-real-mod-intake-2026-10-01}`
- `.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted` (the game's pak file names)
- retained public retoc sources (MIT)

No Epic or Sony source was used or copied.

## Integrator, 2026-10-13: real-data run
All fetches done and verified. Default plan: 0 reparent (67 known-checker). --allow-partial picks a translucent
liquid material for all 11 Leon instances, which is not usable. See
outputs/2026-10-08-l25-prep/evidence/cap12-20261012/FINDING-reparent-real-data-20261013.md for the 3 design items
(blend-mode check, wider pool, role-based param mapping).

## Team F, 2026-10-13: round 2 (Mac-only)
The parent chooser now reads each candidate's domain / blend / lit from two sources: the game's AssetRegistry.bin
and its own compiled PS5 shader types, compared with leave-one-out peers. It matches the mod master's blend group,
read by two readers. Parameters map by exact name (only for a proven texture slot) or by a two-source role rename.
The liquid material is MASKED by both sources and is now refused.
On real data the default result is 0 reparents: Leon's body and skin are "opaque or masked, not provable", so the
tool fails closed (the reviewer's blocker). The hair is provably masked.
With the user decisions `--allow-partial --accept-unproven-opaque`, 10 of 11 Leon instances get the opaque lit
`M_MRO_Cloth_NonVT` (ORM lost, disclosed). The hair stays a known checker.
Optional round-3 fetch plan: evidence/fetch-round3-masters.json (356 rows, 21.0 MB; priority 1: 5 rows, 294 KB).
51 host tests pass. Details: FIXES-ROUND2.md.
