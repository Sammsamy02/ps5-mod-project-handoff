# Capability #8 notes: whole-chunk intake (Team A)

Everything here is **Mac-only** unless it is marked console-proven. No console contact, builds or deploys
happened in this work.

## 1. What the user saw, explained from the files

### (a) "Textures on Leon were checkered grey and white"

The intake ran on the unchanged Leon 2K mod container and the game's own base files.

- **All 67 of 67 mod materials have no shader the console can use.** The game's shader format is `SF_PS5`.
  This is read from the game's own shader-library file names, for example
  `ShaderArchive-Townfall_Chunk0-SF_PS5-SP_PS5.ushaderbytecode` in the base pakchunk0 TOC.
- Every material in the mod traces back to one of 12 root materials:
  - `M_Leon_Body`, `M_Leon_Skin`, `M_Leon_Hair`
  - two copies of `M_Invisible`
  - `Simon_Slot0` to `Simon_Slot7`
- Each root carries only inline PC shader code. The intake found the platform records `PCD3D_SM5` and
  `PCD3D_SM6` in each root's export data, plus DXBC/DXIL bytecode containers.
- The mod's store entries list 0 shader-map hashes, and the mod ships no shader library.
- The engine therefore has no console shader for any of them and draws its default material, a grey/white
  checker grid. The base game does ship `/Engine/EngineMaterials/WorldGridMaterial`.
- This matches what the user reported (relayed 2026-10-12). The 2026-10-02 trial record also noted a "pale
  checkerboard pattern on glasses"
  (`.superpowers/native-observations/leon-index-overlay-trial-2026-10-01/result.json`). That record is from
  an earlier session: background only, not evidence for a verdict here.
- 43 of the mod's materials *replace* Simon's materials so that Simon's parts become invisible: face and
  head (all LODs), eyes, teeth, beard, hair, eyelashes, accessories, glasses and the IV bag. All 43 point
  at the mod's own `M_Invisible`, which is also PC-only. So on the console those Simon parts are **drawn as
  checker instead of hidden**.
- 14 of the 25 meshes use at least one of these materials.
- The textures are **not** the cause. They are never sampled, because the materials fall back. Their format
  is ordinary PC DXT1/BC5 with a linear mip layout, and the intake reads them directly.
- Whether PC textures would look right on the console is still unknown. That needs base texture samples,
  which are in the fetch plan.
- **What fixes it:** this is shader work (#12), or a future "re-point to the game's own material" option,
  which would lose some of the mod's look. #8 does not hide the problem. It now states it before install.

### (b) "Outside was bright as day, you can see the monster clearly"

This is not Leon and not the runtime.

- On 2026-10-02 an `Engine.ini` settings mod (Nexus "silenthilltownfall" mod 110) was merged into the game's config
  pak `pakchunk0-ps5.pak`, into `Townfall/Platforms/PS5/Config/PS5Engine.ini`. Sources:
  `.superpowers/native-observations/engine-config-main-pak-trial-2026-10-01/result.json`, `plan.json` and
  `trial-intent.json`.
- It is still installed. The integrator's read-only pull of 2026-10-13 confirms it at file level:
  - `pakchunk0-ps5.pak` (297,483,249 bytes) differs from `pakchunk0-ps5.pak.original-9a914e38-….bak` in
    exactly one entry, `Townfall/Platforms/PS5/Config/PS5Engine.ini`.
  - Source: `outputs/2026-10-08-l25-prep/evidence/console-pulls-20261013-paks/FINDING-bright-outdoors.md`
    (with `SHA256SUMS`, `PAK-DIFF.txt` and both inis).
  - Label: **file-level, from console-pulled files (Mac analysis)**. No new FTP LIST is needed for this.
  - The independent reviewer re-ran `config_disclosure.py` on the pulled `PS5Engine.ini.current` and
    `.original`: 84 settings, same look categories. That is a second source, independent of the Nexus-ini run.
- `config_disclosure.py` on that mod (`evidence/engine-ini-mod-disclosure.txt`) lists 30 settings. The ones
  that change the look:
  - `r.VolumetricFog=0` turns fog off.
  - `r.EyeAdaptationQuality=0` turns auto-exposure off.
  - `r.ShadowQuality` 1 instead of the game's 3, plus `r.Shadow.Virtual.Enable=0`, 1 cascade, 256 px,
    no distance-field or contact shadows.
  - Lumen reflections off.
  - `r.TonemapperGamma=1.3`.
- Together these make scenes brighter, clearer and less foggy on any platform.
- The game's own `PS5Engine.ini` sets only `r.ShadowQuality=3` and streaming values in `[SystemSettings]`.
- **Finding (file level, two sources):** the bright, fog-free outdoors comes from this config mod. Only a
  launch with the original ini can make it console-proven.
- **Test:** restore the original `pakchunk0-ps5.pak` from its `.bak`. That is a console action and needs the
  user's OK. Then compare.
- The mod also sets `r.Streaming.PoolSize=1536` in `[SystemSettings]` (the game sets 1024, but in
  `[/Script/Engine.RendererSettings]`), and raises `MaxTempMemoryAllowed` from 35 to 128 on a
  memory-limited console. That is worth knowing if streaming problems appear. Since the fixes,
  `config_disclosure.py` matches settings by section and key, and lists the same key in other sections
  separately.

## 2. What was built (`PATCH/pak-intake/`)

The pipeline, in order:

1. **Format profile.** The mod TOC version and block size must equal the game's containers. Compressed,
   encrypted, signed or multi-partition containers are refused, and any other container-header or Zen
   version is refused. Leon matches the game: TOC v8, 64 KiB blocks, Indexed only, container header v5.
2. **Identity.**
   - Every chunk is checked against its TOC IoHash (BLAKE3-160). For Leon, all 162 chunks pass with
     `--verify-bulk` (`evidence/leon-intake-verify-bulk/`).
   - Every package name must hash (CityHash64 of the lower-case UTF-16 name) to its chunk id *and* its
     store id. Leon: 127 of 127.
   - Each package's imported-package names must equal its store entry's imported ids. These are two
     independent sources.
3. **Import closure.** 790 Leon imports were checked.
   - **Script imports** must exist in the game's own ScriptObjects (52,042 entries). All resolve.
   - **Package imports** resolve inside the mod or in a base container. They are checked at package level
     from the base TOCs, and at export level (the public export hash) from fetched base headers.
   - **Structural imports** (an export's class, super, template or outer) that are missing cause a refusal.
   - **Plain references** that are missing load as empty and are disclosed. Leon has one:
     `/InterchangeAssets/Functions/MF_PhongToMetalRoughness`. It is absent from all five base containers.
     Background only (earlier session, not a verdict): the 2026-10-02 trial loaded those packages with the
     game running normally (user observation in that trial's `result.json`). The #8 launch is the test.
   - **Replacements:** the mod's version of a base package must still export every public export the base
     exported, or base packages that import it break. This needs the base headers (priority-3 fetch).
4. **Visual check.**
   - Each material's parent chain is followed to its shader source. The parent is the single
     material-class package import; that is a metadata inference, which the launch checks live.
   - A material is NATIVE when it carries shaders in the game's format, or reaches a base material with
     the game's own cooked shader map. A material is FALLBACK when it needs its own shader map and ships
     only other formats. Anything else is UNKNOWN, which fails closed.
   - Textures: the pixel format and platform-data header (size, packed flags, opt-data, mip count) are read
     from export data, and bulk mip sizes are compared with the linear layout. All of this is compared
     with base texture samples once fetched.
5. **Outputs.**
   - The JSON report.
   - The exact base fetch plan. It gives the ucas file, physical offset and length ranges, and the
     expected IoHash, in priority order.
   - A generated zzintake probe. The template has no names; the data comes from the mod's own files.
     Probe choice is generic, from the mod's own import graph (fix F2): first the replaced objects that
     import the most of the mod ("linkers", e.g. a replaced mesh), then the mod's new objects that a
     linker imports. Each probe lists which replaced probes import it. A hard import loads with its
     importer, so live: if a linker is found, everything it imports must be found (else `MISSING`); an
     object nothing found imports is only "not loaded", never a FAIL. Objects reached only from
     themselves are not probed. It probes every 30 s until the game closes (fix F1).
   - Exit codes: 0 admitted; 3 admitted with visual loss (needs acceptance); 4 incomplete (including a
     fetched base chunk that fails its IoHash: re-fetch); 2 refused (a defect in the mod itself, or an
     unexpected parse error). A run that checks zero items fails. Any older report at `--out` is deleted
     first.

The tool is generic. It contains no game names or mod names. The target shader format, script classes,
base packages and texture layout all come from the base files passed in. Engine-level names are used
(`/Script/Engine.Material`, `PF_*`, `PCD3D_*`).

## 3. Independent checks and reuse of ChatGPT's cap08 work (treated as unverified)

| ChatGPT claim | Re-checked here | Result |
|---|---|---|
| 68 index overlaps: 66 in chunk0, 2 in chunk10 | own parser and own CityHash | **reproduced**: 66 + 2 |
| Base inputs: chunk10 store 416 B, `1fb37f42…` 122,393 B, `2c77facb…` 1,508 B, `953039a7…` 500,891 B, `976ba4c6…` 1,198,188 B with listed IoHashes | own TOC parser and fetch plan | **5/5 expected IoHashes identical**. `953039a7b296b2ee` = CityHash of `/Engine/EngineMaterials/WorldGridMaterial`. |
| Deluxe MI imports `M_Invisible` (hash `a48b2ffd18348a31`) | own Zen parser | consistent: the chain ends at `M_Invisible` |
| "No visual fallback admitted / 0 directives" | — | superseded: explicit per-material verdicts now exist |
| Two partial external-header captures quarantined | not used | still quarantined; the intake only uses whole chunks that pass their IoHash |

Second sources used:

- BLAKE3 is checked against the retained `overlay_builder.py` implementation (13 sizes) and the official
  empty-input vector.
- CityHash is checked against 127 package ids written by the cooker.
- The material parent is predicted from metadata and checked live by zzintake (`o.Parent`) at launch.
- The target format is the base shader-library names, plus base materials using store shader hashes.

Format sources are public and cited, not copied: retoc (MIT) `zen.rs`, `container_header.rs`,
`name_map.rs`; CUE4Parse (Apache-2.0) `FExportMapEntry.cs`, `FPackageObjectIndex.cs`,
`FIoContainerHeader.cs`, all under
`.superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/`. The local Epic source
subset (464 files) does not contain the zen or IoStore readers, so no Epic path:line could be cited for
these formats.

## 4. Evidence (in this folder; the integrator copies it into the l25-prep evidence tree)

- `evidence/leon-intake/leon-intake-report.json` and `leon-intake-summary.txt`: the full verdict for Leon.
- `evidence/leon-intake/leon-base-fetch-plan.json`: 89 chunks.
  - Priority 1: 4 container headers.
  - Priority 2: 17 chunks (closure, parents, texture samples), 3.68 MB in total with priority 1.
  - Priority 3: 68 replaced base packages, 141 MB.
- `evidence/leon-intake/diag/zzintake/`: the generated probe (v2). 21 probes (8 replaced Simon body/jacket
  meshes as linkers, 13 of the mod's new objects, all linked), 5,848 bytes, sha256 `b37fc12f…`. It passes
  `luac -p`, and the stub harness gives `state=PASS` with all objects present and `state=MISSING` when the
  mod's new objects are absent.
- `evidence/leon-intake-verify-bulk/`: the same run with every chunk (bulk too) IoHash-verified, 162 of 162.
- `evidence/engine-ini-mod-disclosure.{txt,json}`: the Engine.ini mod disclosure.
- `tests/test-output.txt`: 59 tests, 0 failing, 0 skipped, PASS (after the review fixes; see `FIXES.md`).

## 5. Open questions and limits

1. **Export-level closure and replacement export check:** pending the fetch plan. The intake currently says
   `INCOMPLETE` (exit 4) on purpose.
2. **Texture layout on the console:** unknown until base texture samples are fetched. The verdict rule:
   - Base textures with opt-data or padded mips, while the mod's are linear, gives LAYOUT-DIFFERS
     (disclosed).
   - Otherwise it gives "no difference found in headers". Pixel tiling itself cannot be proven on a Mac,
     and the launch decides.
   - Today this does not matter for Leon, because every Leon texture belongs to a fallback material.
3. **For #7:**
   - The mod TOC has no perfect-hash seed table; the base TOCs have one. Standalone mounting of a TOC like
     that is #7's question. `overlay_builder.make_chunk_lookup` exists if a derived TOC is needed.
   - The integrator's 2026-10-13 LIST (`../CONSOLE-FACTS.md`) shows `zzz_Leon_Townfall_2K_P.{pak,utoc,ucas}`
     loose in the paks folder, plus the derived `pakchunk0-ps5.utoc` and `_s1.ucas`. #7's "no game file
     changed" baseline needs those restored or removed first (see `../PRISTINE-PAKS.md`). That is a
     console action and needs the user's OK. The launch card now lists it as a precondition (fix F3).
4. **Compressed PC mods (Oodle) are refused**, fail closed. Many PC mods are compressed. A decompress and
   re-store step is future work; Leon happens to be stored.
5. **Optional-segment packages and soft references** are not analysed. The intake flags them as incomplete
   if present; Leon has none.
6. **Parent rule:** "the single material-class package import". When it is ambiguous, the result is
   UNKNOWN, never a guess. It is verified live by zzintake at launch.
7. The pure-Python BLAKE3 runs at about 1.2 MB/s. Leon's package chunks take about 90 s, and all chunks
   about 4 min. This is fine for install time; a C helper is possible later.
