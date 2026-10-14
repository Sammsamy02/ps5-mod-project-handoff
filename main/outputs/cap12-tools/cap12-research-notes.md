# Capability #12 — local research notes (agent HF, 2026-10-10, Mac/battery, offline)

Raw derivations behind CAP12-SHADER-DESIGN.md. Every number below was
computed this session from retained local files (paths cited); nothing was
fetched except the open-source pins recorded in PINS.json.

## 1. Base TOC shader-chunk census (captured utocs, 16-byte FTP banner stripped)

Method: FIoStoreToc v8 layout (banner 16B; version=8, header_size=144 incl.
banner, chunk-id array at file offset 144, 12 B/id; offset/length array
follows, 10 B/row, 5-byte offset + 5-byte length, each BIG-endian — encoding
verified against the mod utoc-analysis.json ground truth rows: idx0 = (0,792),
idx16 = (10878976,796)).

| TOC | entries | type 1 export | type 2 bulk | type 6 ContainerHeader | type 8 ShaderCodeLibrary | type 9 ShaderCode |
|---|---|---|---|---|---|---|
| pakchunk0-ps5 | 59,478 | 38,439 | 18,457 | 1 | **2** | **2,579** |
| pakchunk1-ps5 | 197 | 196 | 0 | 1 | 0 | 0 |
| pakchunk10-ps5 | 16 | 10 | 5 | 1 | 0 | 0 |
| pakchunk11-ps5 | 27 | 13 | 9 | 1 | **1** | **3** |
| pakchunk12-ps5 | 30 | 12 | 8 | 1 | **1** | **8** |

pakchunk0 type-8 chunks (id, ucas offset, length):
- `833fa5945502d89000000008` — off 393,216, len 1,428,008
- `9a85490aa44231b400000008` — off 0, len 367,536

pakchunk0 type-9: 2,579 chunks, len range 265..406,928 B, TOTAL 155,866,613 B
(~156 MB). pakchunk11: 3 chunks (manifest 3,060 B); pakchunk12: 8 chunks
(manifest 4,700 B). Grand total type-9 across captured TOCs: 2,588 (~156.3 MB).

## 2. Store hashes vs shader chunks — the id-law discovery

Base pakchunk0 container header (container-header.bin, parsed with GM's
schema): 38,439 store entries, 5,387 ShaderMapHash references over only
**1,500 unique 20-byte hashes**. Hashes-per-package distribution:
{0: 33,222 | 1: 5,114 | 2: 65 | 3: 27 | 4: 3 | 5: 2 | 6: 4 | 8: 2}.

Cross-check: **ZERO of the 1,500 store hash prefixes match any of the 2,588
type-9 chunk id prefixes.** Also the known glasses hash
`233d367d112a9b677e3e29f960b6d633a97cddf4` has no type-9 chunk.

Conclusions (evidence-bounded):
1. Shader map hashes are SHARED across packages (5,387 refs / 1,500 unique =
   content-addressed library; 3.6 refs per map average).
2. Type-9 chunk ids are NOT retoc's `create_shader_code_chunk_id(FSHAHash)`
   (hash-prefix) form — this is a UE5.6 ReplaceIoChunkHashWithIoHash cook:
   chunk ids are IoHash-derived (BLAKE3-20 per GK's builder pin P6). The
   store's SHA-1 ShaderMapHashes must therefore be resolved to code through
   the **type-8 ShaderCodeLibrary manifest** (the library index), which is
   exactly the M1 acquisition target.
3. Consequence for M1: acquiring ONLY type-9 chunks whose ids we can see is
   not enough to know which store hash they serve; the 2 type-8 manifest
   chunks (1.8 MB total) are the mandatory first fetch.

## 3. CityHash64 verification oracle (outputs/cap12-tools/cityhash64.py)

Port verified against 6 independent container-id oracles:
- mod `zzz_Leon_Townfall_2K_P` -> 9039560806956a15 (== utoc-analysis
  container_id_le)
- base pakchunk{0,1,10,11,12}-ps5 -> all 5 match the ContainerId field in
  each captured TOC header (8f8bb2ccc07563e0, f4c0cfa40e8d6087,
  8d988b9bf60b165b, 34b343b8c75a63f1, 8099615cdecd56d6).

Shader-library name brute-force (candidates global/pakchunkN/townfall/engine
x ps5/sp_ps5/ps5_sm5/ps5_sm6/gfx1034/obf/..., ~500 name forms, utf16-lower
cityhash): **no hit** on the 4 observed type-8 ids. Open unknown; NOT needed
for M1 (TOC ids are the lookup keys). Expect the true library+format name
strings to appear inside the type-8 manifest payload at acquisition time,
which would close this by direct read.

## 4. Engine-side evidence (local decrypted eboot grep, whole file)

- `FShaderCodeLibrary::OpenLibrary` present (0x7b2bca0 context block) — the
  runtime shared-shader-library path is compiled into the shipping engine.
- `ShaderCodeLibrary.cpp` / `GlobalShader.cpp` source-path strings survive;
  `PreInitShaderLibrary` boot-phase string present.
- NO `SP_PS5`/`SF_PS5`/`PS5_SM`/`Gnm`/real `AGC`/`GNF` format-name strings
  (GNF/AGC hits are OpenSSL CIPHER-list noise). The PS5 shader-format name
  is not a printable literal in the eboot — learn it from the type-8 manifest
  (its payload carries the format string per UE's FShaderCodeLibrary layout
  [TRAINING-KNOWLEDGE]) or leave it unnamed (ids suffice).
- Shipping log strings for material/shader-miss are compiled out (GM §1c
  already proved: zero LogMaterial/LogShaders hits).

## 5. mod utoc-analysis.json confirmations reused

- Mod container: types {1: 127, 2: 34, 6: 1} — the mod ships NO type-8/9
  chunks at all (all its shader bytes are inline DXBC/DXIL in type-1 exports
  per GM's token evidence). This is why the PS5 has nothing to key on.
- Compression: all None, 64 KiB blocks, single partition — ranged fetches of
  shader chunks are raw byte ranges (no decompression needed).
