# CAP12 M1a — Townfall PS5 global shader cache: decode + byte-identical rebuild (Mac-only)

2026-10-12. **Mac-only.** No network, no console contact, no builds. Decoder/encoder
written by a research agent from the file's own bytes (no Unreal source copied);
the round trip was then **re-run independently by me**: same sha256.

## Input
`Engine/GlobalShaderCache-SP_PS5.bin` extracted from Townfall's own `pakchunk0-ps5.pak`
(`.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/`;
four more identical copies exist). 4,547,889 B, sha256
`ed52f157bd00b4ee810344ddcac29e9b90d1a154728095903254beb94c1db809`.

## Verdict: byte-identical = YES
- decode -> JSON model -> encode reproduces the file exactly (same size, same sha256).
  The encoder sees only the decoded model, never the input bytes.
- 3 of 3 negative controls: changing one decoded field moves the first differing byte to
  exactly that field (the codec is driven by the fields, not copying bytes).

## What the file is
UE5's cooked **global shader map** for SP_PS5: 492 sections (one per global shader source
file), each a frozen "memory image" of one global shader map + its shader objects.
**It contains no shader microcode:** every section stores only a 20-byte hash into the shared
Global shader code library, which lives in the base `.ucas` (not on the Mac). This corrects
CAP12-M1-FEASIBILITY.md.

## What was decoded / proven
- 492 sections tile the file from offset 4 to EOF with no gaps; 14,645 shaders (1-919 per map),
  1,385 unique shader-type hashes, 492 unique resource hashes.
- Internal consistency checks pass in 100% of sections (array lengths agree, every shader type
  appears in the section's pointer table, the platform-name patch hits its slot, sizes agree).
- Independent cross-check: the sidecar `ShaderTypeInfo-Global-SF_PS5-SP_PS5.stinfo` decodes to
  EOF; its 1,385 unique type hashes match the cache's 1,385 exactly (its 9,388 entries are
  probably the unique code entries in the Global library).

## Still unknown (kept as raw bytes; still round-trips)
The 3.74 MB of per-shader object fields (parameter bindings, ResourceIndex into the code table),
a 23-bit type-index field in frozen pointers, LayoutFlags = 1, and how the resource hash is
computed. The inline-code form (`bShareCode = 0`) never occurs here and is not implemented.

## M1b (the actual shader code)
Needs the IoStore ShaderCodeLibrary chunks (type-8 manifests, type-9 code) from the base `.ucas`:
one gated, read-only, decrypt-off fetch per CAP12-SHADER-DESIGN.md §1b, **with your explicit OK**.
This run supplies ready-made decode checks for them (492 resource hashes, 1,385 type hashes,
~9,388 expected entries). Next Mac-only step: decode the per-shader ResourceIndex.

## Files
`gsc_codec.py` (stdlib only; no per-game constants) — `roundtrip <bin> <receipt.json>`,
`decode <bin> <model.json>`, `encode <model.json> <bin>`; `roundtrip-receipt.json`,
`decoded-summary.json`, `negative_control.py`, `negative-controls.json`.
