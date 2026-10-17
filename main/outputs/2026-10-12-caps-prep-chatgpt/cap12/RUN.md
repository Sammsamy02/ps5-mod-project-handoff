# Local metadata hypothesis census

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap12 -p test_resource_candidates.py -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap12/resource_candidates.py .superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/extracted/Engine/GlobalShaderCache-SP_PS5.bin --codec outputs/2026-10-08-l25-prep/cap12-m1a-20261012/gsc_codec.py --bound-hypothesis 9388 --out outputs/2026-10-12-caps-prep-chatgpt/cap12/resource-index-candidates.json
```

No writes beyond the requested cap12 JSON, no pycache, no fetch/compiler. Scans aligned u32 values in at most 96 bytes from each shader-object start, bounded by the next distinct start/image end. Records min/max/unique/degenerate and sample completeness for every tested offset. The upper bound 9,388 is a stated hypothesis from the earlier type-info report, not a proven library entry count. Multiple bounded integers are expected in metadata; matching the range proves no field identity. Unknown subdata boundaries and pointer pieces remain possible. This is metadata evidence only, zero shader instructions decoded.

## Source-hashed shader chunk catalogue

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap12 -p 'test_*.py' -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap12/shader_catalogue.py --parser outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py --enum-source .superpowers/native-dependency-spike/unreal-library-container-survey-1/parser/source/CUE4Parse/CUE4Parse/UE4/IO/Objects/FIoChunkId.cs --out outputs/2026-10-12-caps-prep-chatgpt/cap12/shader-chunk-catalogue.json .superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk{0,1,10,11,12}-ps5.utoc .superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc
```

Uses existing v8 TOC parser (reserved header bytes relaxed by its strict_admission=False option; full EOF/format admission still applies) and retained public CUE4Parse EIoChunkType5 enum: ShaderCodeLibrary=8, ShaderCode=9. Retained retoc get_chunk_type semantics masks high version bits; raw byte is preserved alongside normalized type.

Added value over older census prose: an executable, exact-ID catalogue with source/parser/enum hashes, 2,594 per-row logical ranges, block geometry and per-chunk metadata digests. A fixture intentionally maps one logical span to discontiguous physical block offsets to guard against treating logical offsets as physical offsets. No UE/Sony source is copied into outputs.

Per-file type8/type9 counts: chunk0 2/2579; chunk1 0/0; chunk10 0/0; chunk11 1/3; chunk12 1/8; Leon 0/0. Total type9=2590, correcting the old notes' 2588 arithmetic typo. Metadata digests are not shader-map/resource hashes. Type8 IDs are exact lookup keys, but the library-name/format-name preimage remains unknown; global-cache resource hash→manifest and shader-map hash→code chunk relationships remain unresolved. No physical payload bounds, payload digest, instruction boundaries or bytes were verified. No retrieval plan is generated.

Actual local full/slice payload intake and run commands are now documented in INTAKE.md. No real shader payload was found; the executable is tested with explicitly synthetic inputs only.

## Raw metadata field correspondence

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap12/resource-source-audit/verify_raw_fields.py
```

Checks three hash-pinned local inputs before/after. Uses the retained M1a reader for section/reference geometry, then direct raw reads at208/212/216 to compare every saved row. Writes raw-field-replay.json only in resource-source-audit/. This is fresh byte correspondence and map-local dense-index evidence; it does not rerun the full legacy object codec/roundtrip/mutation controls or decode library/code identity. Agent audit-receipt.json is the earlier saved-summary/source audit; the root raw-field receipt adds original cache reads.
