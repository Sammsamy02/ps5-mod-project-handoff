# agent-HF — capability #12 SHADER TRANSLATOR: research + design report

2026-10-10, Mac on battery. Deliverables (nothing else touched; `.superpowers`
read-only; no console, no builds, no Sony material):

- `CAP12-SHADER-DESIGN.md` — items 1-7, milestones M1-M4, fallback law
- `outputs/cap12-tools/` — open-source pins + sha256 manifest (`PINS.json`),
  research notes, verified CityHash64 port
- `CLAUDE-CAPABILITY-STATUS.md` — #12 design-only row appended (one line,
  same format) + count summary updated

## What local evidence proves TODAY

1. **The game's PS5 shader corpus is located and bounded**: base pakchunk0
   utoc holds 2 type-8 ShaderCodeLibrary manifest chunks (1.8 MB at ucas
   offsets 0 and 393,216) and 2,579 type-9 ShaderCode chunks (156 MB, sizes
   265 B-407 KB); pakchunk11/12 add small libraries. All compression None —
   raw ranged fetches.
2. **The library is content-addressed and shared**: 5,387 store
   ShaderMapHash references collapse to 1,500 unique maps (avg 3.6 packages
   per map). ZERO store-hash prefixes match any type-9 chunk id — the ids
   are IoHash(BLAKE3-20)-derived (TOC v8 ReplaceIoChunkHashWithIoHash), so
   the SHA-1->code mapping lives in the type-8 manifests. **The manifest is
   the M1 target, not the bulk bytes.**
3. **The id-derivation family is verified locally**: pure-stdlib CityHash64
   port reproduces all 6 observed container ids (mod + 5 base utocs).
   Shader-library name law open (brute-force no-hit, non-blocking).
4. **Engine side**: `FShaderCodeLibrary::OpenLibrary` + `PreInitShaderLibrary`
   strings in the shipping eboot (runtime shared-library path compiled in);
   no PS5 format-name literal, no VerifyShader string; shipping logs
   compiled out (GM's prior proof).
5. **The mod ships zero PS5-consumable shader data** (GM/HB corpus: no
   type-8/9 chunks; inline DXBC/DXIL only) — the problem #12 exists to solve,
   with the Leon DXBC containers available locally as translation test
   inputs.

## The acquisition gap (scoped honestly)

The game's shader BYTES are in the base 128 GB ucas, not on the Mac. ONE
read-only decrypt-off console session fetches: the 4 manifest chunks
(~1.8 MB), then after Mac-side manifest decode a bounded <10 MB set
(target material's chunks + 5 fallback-material maps + ~50-chunk stratified
format panel). No executables, no bulk ucas. Gated like all prior captures.

## Toolchain verdict

All open-source, no Sony material, pinned today: **dxil-spirv (MIT,
commit 8434b063…, active 2026-10-10, covers DXIL SM6 + DXBC SM5 via
dxbc-spirv submodule)**; **vkd3d-proton's vkd3d-shader (LGPL-2.1,
commit 22307558…)** as second DXBC route (tool-use only; MIT route
preferred); **Mesa ACO (MIT, raw-file pin, tag-pin at build time)** —
no PS5/gfx1034 target exists publicly, so we target gfx1030-class RDNA2
and learn the console packaging from the game's own chunks (the honest
fidelity boundary); **LLVM AMDGPU (Apache-2.0) as stage-2 fallback**.
Builds deferred to the AC window (documented plan); battery law held.

## Status

#12 = **design-only**. Next action: the one bounded fetch, then M1
(byte-identical rebuild) entirely on the Mac. Console re-opens only at M3
(one gated launch, Claude review + user OK). Until M4 passes, GM's L1/L2
fallback ladder remains the shipped install path.
