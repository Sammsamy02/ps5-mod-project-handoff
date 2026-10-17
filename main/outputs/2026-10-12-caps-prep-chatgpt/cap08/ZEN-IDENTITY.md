# Source-bound Zen export identity (Mac-only)

Implemented zen_identity.py and replay_zen_identity.py. The reader supports exactly UE5 unversioned Zen packages with an independently verified container header version 5 (SoftPackageReferencesOffset). It decodes bounded summary regions, v5 cell offsets, name batches, mapped names, 72-byte export entries, public export hashes and typed object references. Unknown versioning, invalid map stride/offset/name/reference, truncation and unsupported filter/padding fail closed. Unparsed header metadata is preserved by byte count and SHA256, not interpreted.

Real replay: leon-zen-identities.json records the exact closure, original mod TOC/UCAS, existing parser and public format source hashes. The original 185,446,608-byte mod UCAS is streamed for SHA256, read through read-only mmap for bounded selected chunks, and hashed again after replay. Existing parser validates every lookup and stored block geometry. All four bounded chunk reads (v5 ContainerHeader and three exports) reproduce their TOC BLAKE3-20 IoHashes. No payload or game source was copied into outputs.

| Export name | Public export hash | Raw class reference | Class reference kind |
|---|---|---|---|
| MI_Simon_BodywarmerBeanieTag_Deluxe | a12c0cc7d8398944 | 7f7253ab5570e48f | ScriptImport |
| M_Invisible | a48b2ffd18348a31 | 7d87c25136227324 | ScriptImport |
| SK_Simon_HeadCap_Deluxe_optimized | 23d513e1780a2d17 | 42d93e6acfce1861 | ScriptImport |

These are parsed values, not name heuristics or arbitrary external extractor assertions. The referenced script class NAMES are still unresolved. Package-import references expose package/hash indices and bind package IDs to the same verified store's import array where applicable, but that does not establish their classes or compatibility. No base export payload is supplied, so class equivalence, native shader usability and import compatibility remain false/unproven. This output is evidence for review and cannot by itself satisfy the directive gate or create a fallback action.

Public format provenance: retained retoc zen.rs, name_map.rs and container_header.rs (MIT; copyright 2025 Truman Kilen and Archengius); retained CUE4Parse FPackageObjectIndex.cs and FExportMapEntry.cs (Apache-2.0). Full source and license file hashes/paths are recorded in format_sources. This is an independent Python implementation of format facts; no third-party source text or Epic/Sony source is copied/distributed. Source refs are local retained public parser trees.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap08 -p 'test_*.py' -v
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap08/replay_zen_identity.py --closure outputs/2026-10-12-caps-prep-chatgpt/cap07/evidence/leon-secondary-mod-import-closure.json --parser outputs/2026-10-08-l25-prep/pak-overlay/overlay_builder.py --toc .superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc --ucas .superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.ucas --retoc-source .superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/retoc --cue4-source .superpowers/native-dependency-spike/unreal-dependency-compatibility-1/source/CUE4Parse --out outputs/2026-10-12-caps-prep-chatgpt/cap08/leon-zen-identities.json
```

21 scoped cap08 tests pass. Seven Zen cases cover parsed identity with unresolved script class, export stride, name index, truncation, and unsupported version/versioned summary, exact 60-byte input, and header end before name-count field. tdd-red-zen.txt records missing-feature failures; tdd-green.txt is the full current scoped suite. No console, network, install, compiler or game-executable analysis occurred.
