# CAP12 M1 feasibility — "decode one of Townfall's own PS5 shaders and rebuild it byte-identical"

Prep summary, 2026-10-12 (Mac-only, read-only). Sources: `../CAP12-SHADER-DESIGN.md`,
`../agent-HF-cap12.md`, `../../cap12-tools/` (PINS.json, cap12-research-notes.md),
plus a fresh worktree search for shader/pak data (file sizes + header peeks only;
no large binary was decoded).

## Is Townfall PS5 shader data already on the Mac? YES — more than the design states

The design docs say the shader BYTES are not local (only `.utoc` metadata) and
that M1 needs a console fetch of the base `.ucas`. That is TRUE for the material
**ShaderCodeLibrary** (type-8/9 IoStore chunks). It is NOT the whole picture:
the game's **engine/global** PS5 shaders are already extracted on the Mac, from
the game's own classic pak. All sizes/hashes below are Mac-only observations
from this session.

Present locally (all under `.superpowers/native-dependency-spike/…/extracted|independent-extraction/`,
unpacked from `townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak`,
source sha `ee145cbc…`):

| File | bytes | sha256 (first16) | what it is |
|---|---|---|---|
| `Engine/GlobalShaderCache-SP_PS5.bin` | 4,547,889 | `ed52f157bd00b4ee` | serialized PS5 global shader map (ASCII shows `FGlobalShaderMapContent` ×492, `FShader*` type names, mixed metadata + embedded microcode; entropy low overall with sparse high-entropy code blobs). **Genuine SP_PS5 shader bytes.** |
| `Townfall/Content/ShaderTypeInfo-Townfall_Chunk0-SF_PS5-SP_PS5.stinfo` | 391,088 | `cd7edb52900f0b84` | shader type-info sidecar (PS5) |
| `Townfall/Content/ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo` | 113,524 | `39bd2613d96589a3` | shader type-info sidecar (PS5) |
| `Townfall/Content/PipelineCaches/PS5/Townfall_SF_PS5.stable.upipelinecache` | 5,334 | `cba8a34d74b12abb` | PSO cache, header `HCACEPIP` |
| `townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak` | 297,572,462 | `ee145cbc808f9650` | full classic pak (loose files) |

Also local: the five base `.utoc` files (metadata only) and `global.ucas`
(3.4 MB script objects). NOT local: the 128 GB base `.ucas` that holds the
2 type-8 manifest chunks (~1.8 MB) and 2,579 type-9 ShaderCode chunks
(~156 MB) — confirmed by re-parsing the captured `.utoc` headers this session
(pakchunk0 = 59,478 entries: 2 type-8, 2,579 type-9; chunks map to ucas file
offsets up to ~128 GB, which is why only the `.utoc` was captured).

## Tools that exist (`../../cap12-tools/`)

- `cityhash64.py` (sha `497ddab0…`) — verified id oracle; **present, works**.
- `dxil-spirv/` — cloned, but its submodules (`dxbc-spirv`, SPIRV-Tools/Cross/
  headers) are EMPTY (0 files) and dxc not fetched → NOT buildable as-is.
- `vkd3d-proton/` — cloned source only.
- `mesa-aco-pin/` — 2 raw `.cpp` + LICENSE only (not a tree, not buildable).
- No compiled artifacts anywhere (battery law held). The translation toolchain
  is therefore pins-only, nothing built. **M1 does not need any of it** — M1 is
  decode + re-serialize (pure Python), no translation.

## Can M1 be done Mac-only right now?

**Partially YES, and sooner than the design assumed — using the local
`GlobalShaderCache-SP_PS5.bin` as the M1 substrate.** It is unambiguously
"one of Townfall's own PS5 shaders" in PS5 (SP_PS5) format, fully on the Mac.
M1's success test ("decode a shader container and re-encode it byte-identical
to the captured sha") can be attempted against this 4.5 MB file today: decode
the FGlobalShaderMap serialization + embedded code records, re-serialize, assert
sha == `ed52f157…`. No console, no fetch, no network, no build. This is a
Mac-only claim and it UPDATES the design's "base shader bytes not local" line
(which was scoped to the material library, not the global cache).

Caveat (do not overclaim): the global shader cache is the ENGINE/global shader
map, serialized by the GlobalShaderCache path — NOT the IoStore type-8/9
ShaderCodeLibrary that the material/mod pipeline (M2–M4) ultimately targets.
Proving byte-identical rebuild of the global cache proves the SP_PS5 shader
record format and the serializer, which is most of M1's value, but it does not
prove the type-8 manifest / type-9 chunk layout. Treat it as "M1a (global cache,
Mac-only, doable now)" feeding "M1b (material library, needs the fetch)".

## What input is still missing for the full material-library M1, and how to get it (generic, read-only)

Missing: the 2 type-8 ShaderCodeLibrary manifest chunks and the specific type-9
ShaderCode chunks for one material — all inside the base `.ucas` not on the Mac.
Obtaining them (generic steps; PROPOSAL, needs user OK; no action taken here):

1. ONE read-only, decrypt-off console session (per AGENTS.md / CAP12 §1b).
2. Ranged reads of the type-8 manifest file ranges first — computed this session
   from the captured `.utoc` block table as contiguous, uncompressed (method 0,
   64 KiB blocks): pakchunk0 manifests at ucas file offsets [0, 367,536) and
   [367,536, 1,795,544); the small libraries pakchunk11 [0,3,060) and
   pakchunk12 [0,4,700). Total ~1.8 MB.
3. Decode the manifest on the Mac → shadermap-hash → {chunk id, offset/len} map.
4. Only then fetch a bounded (<10 MB) type-9 set for ONE material + a small
   format panel (again contiguous ranges). Never the 128 GB bulk, never any
   executable. Each range sha-recorded at capture.

Until that session, full material-library M1 is BLOCKED on the fetch — but the
global-shader-cache M1a is available Mac-only immediately and should go first.
