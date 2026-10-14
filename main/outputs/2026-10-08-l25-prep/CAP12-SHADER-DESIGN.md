# CAPABILITY #12 — SHADER TRANSLATOR (PC shader effects -> exact PS5 rendering)

Agent HF, 2026-10-10. Mac-only, battery-safe, offline except bounded
open-source pins (recorded in `outputs/cap12-tools/PINS.json` with sha256s).
Design status: **design-only**. No console work, no game-executable work, no
Sony material, originals untouched. Raw derivations behind every local claim:
`outputs/cap12-tools/cap12-research-notes.md`.

Pipeline (user-specified law): **PC shader bytecode (DXBC/DXIL) -> SPIR-V
(dxil-spirv / vkd3d-shader) -> AMD RDNA2 machine code (Mesa ACO or LLVM
AMDGPU; public AMD ISA docs) -> the PS5 shader package format, learned ONLY
from the game's own shipped shaders.** Generated at install time as derived
overlay entries; original mod files and base game files never modified.

Milestones: M1 decode+rebuild byte-identical one of the game's shaders (Mac)
/ M2 translate a simple PC solid-color shader through our format checks (Mac)
/ M3 that material renders in-game via overlay (console, gated) / M4
emissive glow then a real mod's effect, starting with the Leon set. Fallbacks
until M4 pass: L1 hide/restore + L2 closest-match (GM's ladder).

---

## ITEM 1 — PS5 shader package format: what local files already prove

### 1a. Proven from the retained captures (all re-derived this session)

The game's shader code lives in **IoStore shader-library chunks inside the
base .ucas**, addressed by the base .utoc — the exact structure the pinned
retoc/CUE4Parse schema describes (`EIoChunkType::ShaderCodeLibrary = 8`,
`ShaderCode = 9` in UE5+ numbering; `retoc/src/lib.rs:1275-1276`).

| Fact | Evidence |
|---|---|
| Base pakchunk0 utoc (59,478 chunks) contains **2 type-8 ShaderCodeLibrary manifest chunks and 2,579 type-9 ShaderCode chunks**; pakchunk11/12 add 1+3 and 1+8 | chunk-id census over the five captured utocs (banner 16B stripped; ids at file offset 144; verified against mod ground truth (0,792)/(10878976,796)) |
| The two pakchunk0 manifests sit at **ucas offsets 0 (367,536 B) and 393,216 (1,428,008 B)** — the manifest pair totals ~1.8 MB | offset/length rows for the type-8 chunk ids `833fa5945502d890…08` / `9a85490aa44231b4…08` |
| pakchunk0 shader code totals **156 MB in 2,579 chunks, sizes 265 B..407 KB** — the game's own PS5 shader corpus and our format teacher | sum over type-9 offset/length rows |
| The **store's 5,387 ShaderMapHash references collapse to 1,500 unique hashes** (shared content-addressed library; avg 3.6 material-packages per shader map) | re-parse of `container-header.bin` with GM's schema; distribution {1:5114, 2:65, 3:27, 4:3, 5:2, 6:4, 8:2} |
| **Type-9 chunk ids do NOT carry the store hash prefix** (0 of 1,500 match; the glasses hash `233d367d…` has no type-9 chunk either) | set intersection, research notes §2 |
| Therefore: this is a **ReplaceIoChunkHashWithIoHash cook** (TOC v8 — the pinned GK/HB name for it): chunk ids are IoHash(BLAKE3-20)-derived, and the **type-8 manifest is the map from SHA-1 shader-map hash -> shader code entry**. The manifest is the true M1 target | inference from the zero-intersection + v8 header flag + GK's builder pin P6 (BLAKE3 IoHash) |
| The mod container carries **zero type-8/9 chunks** (types {1:127, 2:34, 6:1}) — all its shader bytes are inline DXBC/DXIL in type-1 exports | `leon-real-mod-intake-2026-10-01/utoc-analysis.json` + GM token evidence |
| Compression is **None, 64 KiB blocks, single unbounded partition** for every captured container — shader chunk bytes are raw ranges, no decompression needed at fetch or rebuild | utoc-analysis.json header block + base TOC block table profile |
| Per-chunk TOC meta carries a 20-byte hash per chunk (IoHash class); GK's builder already recomputes exactly this class of hash for the merged header chunk and its round-trips pass | GK `overlay_builder.py` capability matrix + Leon integration replay |
| `FShaderCodeLibrary::OpenLibrary` + `PreInitShaderLibrary` + `ShaderCodeLibrary.cpp` path strings exist in the shipping eboot — the runtime shared-library path is compiled in | whole-file grep of `decrypted-eboot.bin` (research notes §4) |
| `FIoContainerHeader` v5 store-entry layout: per package, CArrayView FPackageId imports + CArrayView 20-byte FSHAHash ShaderMapHashes | pinned `FFilePackageStoreEntry.cs:23-25`, GM's 15/15 pinned checks |
| CityHash64(lower(name)-utf16le) reproduces **all 6 observed container ids** (mod + 5 base) — the id-derivation family is fully in hand for container-scale ids | `outputs/cap12-tools/cityhash64.py` (verified port, MIT reference pinned) |

What local files do NOT contain: the shader bytes themselves (base .ucas is
128 GB and not on the Mac — metadata only, FZ-GAME2 class), the type-8
manifest payload (its decode is M1 work), and the PS5 shader-format name
string (no SP_PS5-class literal in the eboot; zero hits — research notes §4).

### 1b. Honest acquisition gap + bounded fetch plan (the console session that M1 needs)

ONE read-only console session, decrypt-off law (`curl -Q "SITE SELF"` per
connection; PS5 CONSOLE RULES 1), user's own game, no writes to game data:

1. **Fetch the 4 type-8 manifest chunks first** (~1.8 MB + 7.8 KB): pakchunk0
   ucas ranges [0, 367536) and [393216, 1821224); pakchunk11 [0,3060);
   pakchunk12 [0,4700). Ranged FTP RETR via the proven bounded-reader path
   (same session class as the container-header capture that produced
   `container-header.bin`).
2. **Decode the manifest on the Mac** to produce the shadermap-hash ->
   {chunk id, entry offset/length, per-shader descriptors} table (M1 step A;
   format learned by correlation with the 1,500 known SHA-1 store hashes and
   the 2,588 known chunk ids — both sets already local).
3. **Then choose the bounded type-9 fetch set** (do NOT pull 156 MB blindly):
   - minimum corpus: every chunk named by ONE decoded material's shader map
     (the M1 rebuild target — expect tens of chunks, a few MB), plus
   - the fallback-material maps (`WorldGridMaterial`, `DefaultMaterial`,
     `M_FluxSurfaceInvisible`, `RemoveSurfaceMaterial`,
     `DefaultDeferredDecalMaterial` — the five GM candidates, each 1 store
     hash), plus
   - a fixed ~50-chunk stratified sample across the size range (265 B..407 KB)
     as the format-diversity panel for header decode validation.
   Fetch = block-aligned ranges (64 KiB blocks, all-None compression), each
   range sha256-recorded at capture; total bounded to <10 MB by construction.
4. **Never fetched**: the 128 GB ucas at large, any executable, any file not
   named by the TOC's shader rows. The session rides existing gating (same
   user-OK class as every prior capture; no new authority requested).

### 1c. M1 procedure (decode one game shader end-to-end + rebuild byte-identical)

1. From the manifest table, take the glasses' own map (`233d367d…`) and one
   fallback-material map; fetch their type-9 chunks per 1b.3.
2. Decode each chunk: entry headers (per-shader descriptor: frequency/stage,
   hash/uuid, sizes, flags) + payload. The container level is partially
   known from PC-side structure (UE packs library entries with a small fixed
   header + code; exact fields = learned here, from the game's bytes only).
3. Write the re-encoder: manifest row -> chunk bytes.
4. **PASS = byte-identical rebuild**: decoded fields re-encoded reproduce
   the fetched chunk sha256 exactly, for every chunk in the sample panel;
   and a synthetic manifest+chunk pair (fresh ids) round-trips through the
   game's own TOC writer path (GK builder integration, derived container
   only).
5. FAIL = any mismatch: recorded, no guessing — expand the sample panel, one
   hypothesis per iteration (evidence-driven loop law).

M1 is pure Mac work after the one 1b fetch; no translation involved; it
proves the format law "learned only from the game's own shipped shaders".

---

## ITEM 2 — Resource binding layout the game's renderer expects

### 2a. What we can read locally

- **The PC side is fully readable**: the Leon mod's inline DXBC/DXIL
  containers (M_Invisible: 296 DXBC + 205 DXIL + SFI0/ISG1/OSG1 chunks;
  M_Leon_Body: 15+10) carry the D3D input/output signature and resource
  binding tables (ISG1/OSG1 = stage signatures; SFI0 = feature info). These
  enumerate, per stage: texture/SRV slots, samplers, CBVs (b-registers),
  UAVs — the binding surface the material's shaders actually consume.
- **The graph side**: `unreal-mod-local-dependency-closure-1/proof-3/graph.json`
  gives the Leon material parameter structure (texture parameters per
  MI_Leon_* instance; scalar/vector params; parent chain) — what the uniform
  side must expose.
- **The store side**: 1,500 unique shader-map hashes = the library's real
  keying granularity (ITEM 1); the manifest decode will show whether entries
  carry binding metadata (UE5's FShaderCodeLibrary entries carry shader
  names/ids per stage, not full descriptors — the PS5 RHI owns descriptor
  layout, which is exactly the console-private part).

### 2b. [TRAINING-KNOWLEDGE] UE5's public binding model (labeled)

UE5 material shaders follow a fixed contract at the RHI-agnostic level:
permutation of (vertex factory x shading model x pass) consumes (a) the View
uniform buffer, (b) the material's own uniform cell (FMaterialResource
parameters), (c) its declared texture/sampler bindings in fixed slots. The
engine-side binding is resolved through FShaderParameterMap recorded at
compile time; on consoles the descriptor layout is Sony-backend-private
(the one thing public sources genuinely do not document). Consequence:
**the binding layout must be learned by correlation, not assumed**: our
translated shader keeps the PC-side slot structure from ISG1/OSG1 (stage
I/O + resource counts) and the game's own PS5 shaders (M1 corpus) tell us
what the engine expects at load (their descriptor-adjacent headers, if any,
are in the type-9 payloads we decode).

### 2c. Verification plan

1. Static: rebuilt M1 chunks re-parse; their per-stage I/O signature must
   equal the PC signature of the same material stage (slot-for-slot).
2. Local oracle: dxil-spirv translation of the SAME Leon DXIL, then SPIR-V
   reflection (slot/usage report) == DXBC signature report (Mac, repeatable).
3. Console (rides M3's single gated launch): the material renders = the
   binding contract held. A wrong binding cannot half-work (engine either
   resolves slots or falls back); the M3 A/B visual gate is therefore also
   the binding acceptance test.

---

## ITEM 3 — Permutations per material + generation strategy

### 3a. [TRAINING-KNOWLEDGE] UE5 permutation model (labeled)

A material's shader set = FMeshMaterialShaderMap: for each (vertex factory
type x shader frequency [VS/PS/GS/HS/DS/CS]) x (shading path: deferred/
forward x quality level x feature level x material permutation id), one
FShader. A "shader map hash" (our 1,500 unique hashes) keys one such
completed set. Cooks with shared material shader code store the sets in the
library; inline PC cooks embed them per material resource.

### 3b. What the game's own files show (local counts)

- Store hash multiplicity per package: {1: 5,114 | 2: 65 | 3: 27 | 4: 3 |
  5: 2 | 6: 4 | 8: 2} packages of 38,439 — most packages reference ONE map
  (single material, one permutation set); multi-hash packages = multi-
  material or permutation-split packages (peak 8).
- PC-side stage counts inside ONE mod material package (inline evidence of
  permutation volume): M_Invisible 296 DXBC + 205 DXIL ≈ 500 stage compiles;
  M_Leon_Body 15+10; the 5,387-vs-1,500 sharing ratio shows heavy cross-
  material map reuse (the library exists precisely to dedup this).
- The manifest decode (M1) turns these into exact per-material permutation
  lists — the generation matrix becomes data, not estimates.

### 3c. Generation strategy

1. **Enumerate** the permutations the target material needs from its PC
   payload (DXBC/DXIL container count per stage x the material's usage set:
   which passes the game actually runs for it — learned from the base
   material occupying the same slot, e.g. the 24 overlap materials with base
   hashes; HB's OBSERVATION-MAP regions define the visible subset to target
   first: accessories/glasses + face LODs).
2. **Translate per stage** through the ITEM-5 toolchain; keep permutation id
   mapping 1:1 with the PC set.
3. **Emit** one derived shader map (new unique hash id, IoHash-keyed chunk
   ids per the v8 law) + manifest rows in a derived type-8 chunk + derived
   store-entry hashes for the repointed material — all inside GK's derived
   overlay (originals untouched; revert = row removal).
4. Bound the first generation target to M3's single material (one map,
   ~10-50 stages), NOT the Leon set (that is M4+ scale, decided by the M3
   verdict).

---

## ITEM 4 — Does the console or game verify shader binaries before use?

Evidence inventory (local, today):

- **Container signing**: all captured TOCs have signed_flag CLEAR (mod +
  base; utoc-analysis "unknown_signed_flag_clear"). GK refuses signed
  containers on principle; none observed in this title. The Leon overlay
  trial already ran with a derived utoc — the index layer demonstrably is
  not signature-gated in practice (capability #7 receipt).
- **Chunk-level content addressing**: every chunk row (including every
  shader chunk) carries a 20-byte IoHash in TOC meta — content-addressing
  exists at the storage layer. Whether the engine VALIDATES payload-vs-hash
  on every read is a runtime unknown; the proven Leon trial passed with
  GK's correctly-recomputed hashes, and our pipeline recomputes them by the
  same BLAKE3-20 law (GK P6), so we are verify-compatible either way. We
  never ship a mismatched hash to "test" the checker (no destructive
  probes).
- **Engine-side shader-map validation**: the store hash arrays are lookup
  keys (content-addressing — 5,387/1,500 sharing proves key semantics, not
  cert semantics). No `VerifyShader`-class string exists in the eboot
  (0 hits). Shipping log strings are compiled out entirely (GM §1c), so no
  log line will ever confirm/deny — by design we do not rely on logs.
- **What remains genuinely unknown (console-side)**: whether the PS5 RHI
  validates shader microcode structure at pipeline creation (plausible:
  malformed code should fail pipeline create rather than pass silently) and
  whether GNF-class object headers carry their own integrity fields. Both
  are answered by the SAME observation: the M3 one-launch gated test — a
  correct translation renders (no verification wall), a rejected one
  produces the documented fallback (verification wall exists; we then study
  the reject path from the M1 corpus headers before any second attempt).
  No dedicated console probe is warranted beyond riding M3 (one launch per
  test, Claude review + user OK).

---

## 5. TOOLCHAIN SURVEY (availability / pin-ability TODAY; nothing built on battery)

All four stages are open-source; no Sony material anywhere. Pins + sha256s
in `outputs/cap12-tools/PINS.json`.

| Stage / tool | License | Pin status | Notes |
|---|---|---|---|
| **dxil-spirv** (HansKristian-Work) — DXIL SM6 -> SPIR-V; via dxbc-spirv submodule also DXBC SM5 | **MIT** (Valve/H-K Arntzen) | PINNED commit `8434b063…` 2026-10-10, shallow clone 20 MB | Active upstream (vkd3d-proton's production shader path). Builds with submodules only; small-LLVM-subset built in. **Preferred stage 1.** |
| **vkd3d-shader / vkd3d-compiler** (vkd3d-proton) — DXBC TPF -> SPIR-V CLI (`vkd3d_shader_compile_dxbc`) | **LGPL-2.1** | PINNED commit `22307558…` 2026-10-06, shallow clone 60 MB | Second stage-1 route. LGPL is fine for an offline tool whose output is data; never static-link into distributed binaries. Tool-use only. |
| **Mesa ACO** — NIR -> RDNA2 machine code | **MIT** | RAW-FILE PIN (LICENSE.MIT, aco README, aco_interface.cpp, aco_assembler.cpp, sha256-recorded); full tree NOT cloned (size); pin an exact release tag at build time | gfx10 family supported; **no PS5/gfx1034 target in public Mesa** — we target gfx1030-class RDNA2 ISA (public AMD RDNA2 ISA manual, cited not copied) and learn the console packaging from the M1 corpus. This is the honest fidelity boundary. |
| **LLVM AMDGPU** (llc -mtriple=amdgcn--amdhsa -mcpu=gfx1030) | Apache-2.0 WITH LLVM-exception | NOT pinned (repo 4.6 GB); release tarball at build time | Stage-2 FALLBACK: standalone, easiest to host; codegen quality below ACO for RDNA (documented tradeoff, affects perf not correctness). |
| CityHash64 reference + our verified port | MIT | pinned (`city.cc` sha `9cd2e21d…`, `cityhash64.py` sha `497ddab0…`) | Id-derivation oracle for container-scale ids (6/6 verified). |
| dxc (DirectXShaderCompiler, MIT) | MIT | not pinned | fetched by dxil-spirv's `checkout_dxc.sh` at build time. |

**Mac build plan (for the AC-power window, in order):**
1. dxil-spirv (CMake, submodules) + dxc checkout -> `test_shader` CLI on the
   Leon DXIL corpus (the mod's 205 DXIL + 296 DXBC containers are local test
   inputs TODAY).
2. spirv-dis (Khronos spirv-tools, Apache-2.0) for intermediate inspection.
3. Mesa minimal build targeting `src/amd/compiler` (ACO static lib) with a
   gfx1030 target; NIR input from dxil-spirv output via
   spirv-to-nir (Mesa's src/compiler). If the in-tree build proves too
   entangled, LLVM route first (llc standalone), ACO second.
4. Our PS5 package encoder (pure python to start, mirroring GK's builder
   style) consuming M1's decoded format.
None of these steps touches console or Sony material; all outputs are data.

---

## 6. MILESTONES M1-M4 + fallback law

**Law (all milestones):** output generated at install time as derived
overlay entries only (GK builder carries them); original mod files and base
game files untouched; revert = row removal. Until M4 passes, every mod
install continues to run GM's L1/L2 ladder (Option C hide / Option A
substitute) — the translator NEVER blocks a working install.

| M | Gate | Pass | Fail |
|---|---|---|---|
| **M1 format proof** | Mac only (after the one 1b fetch session) | decode + byte-identical rebuild of one fetched game shader chunk (sha256-equal), sample panel 100% re-encode clean, synthetic map round-trips the derived TOC | any mismatch: recorded, panel widened, one hypothesis per iteration; M1 blocked = whole capability stays design-only |
| **M2 translate (Mac)** | Mac only | a trivial PC solid-color shader: DXBC -> SPIR-V -> RDNA2 code -> OUR package format -> passes M1's re-encoder + static checks (signature parity per ITEM 2c.1-2) | toolchain gap logged with the exact unsupported opcode/feature; no forced output |
| **M3 in-game render** | CONSOLE: Claude review + user OK, ONE launch, riding the existing gated ladder (no extra session for a negative) | the M2 material renders correctly in-game via the derived overlay (visual gate; A/B against L2 closest-match control per HB's sheet) | documented fallback appearance = engine rejected the binary; reject-path studied from M1 headers before any retry; L2 remains the shipped answer |
| **M4 real effect** | CONSOLE, same gating; starts with emissive glow, then the Leon set (17 added-broken materials, HB's regions) | mod's new shader effect renders with its PC look (3-pass visual law across restarts per the standing acceptance rule) | partial passes recorded per-region; L1/L2 ladder keeps the mod playable |

Dependency order is strict: M1 -> M2 -> M3 -> M4; each fails closed into the
fallback ladder. No deadline-driven skipping.

---

## 7. DEPENDENCIES on capabilities 1-8 + new caps

- **#8 (GM's data)**: the entire analyzer substrate — 5,387-hash store
  parse, broken-material classification, fallback candidates, and the L1/L2
  ladder #12 falls back into. GM's Option B refusal (Sony-licensed cook)
  stands; #12 does NOT cook with Epic/Sony backends — it compiles from
  public tools and packages in the game's own format, which is the
  licensing-clean route the user specified.
- **#7 (overlay, GK's builder)**: M3/M4 delivery vehicle — derived TOC +
  sidecar + store-entry surgery + BLAKE3 IoHash recomputation (P6) are
  exactly the writer #12 needs for new type-8/9 rows; the Leon trial is the
  standing acceptance precedent.
- **Scanner (#2)**: per-title shader-library shape varies; the TOC census
  step (type-8/9 inventory) becomes a scanner profile field for each new
  game.
- **#1/#3/#4/#5/#6**: no hard dependency; #5 (crash symbolization) is the
  post-mortem tool if M3 ever faults the engine; #4's early-load residency
  is irrelevant to install-time generation.
- **Other new caps**: none of the other new capabilities consume #12; #12
  consumes the pinned CUE4Parse/retoc schema and the retained capture
  corpus (GM/HB/GK/FZ-GAME2 metadata) as cited.

---

## Provenance

Read-only inputs: all files cited in-text (GM `shader-fallback/`, HB
`OBSERVATION-MAP.md`, GK `pak-overlay/`, `leon-real-mod-intake-2026-10-01/`,
`townfall-main-utoc-2026-10-01/`, `townfall-original-container-header-1/`,
`townfall-static-executable-2026-10-02/decrypted-eboot.bin`, pinned
CUE4Parse/retoc sources, FZ-GAME2 metadata). Created: this file,
`outputs/cap12-tools/{PINS.json, cap12-research-notes.md, cityhash64.py,
city.cc, dxil-spirv/, vkd3d-proton/, mesa-aco-pin/}`, the #12 status row,
and `agent-HF-cap12.md`. No console contact, no builds, no game files
modified.
