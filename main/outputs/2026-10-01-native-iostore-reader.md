# Native IoStore reader checkpoint

The shared C reader now inspects unencrypted, unsigned IoStore v8 headers, selected package descriptors and selected compression-block ranges. Its 9,788 focused checks pass against synthetic cases and actual captured mod/game metadata. The same source compiles and links for PS5; that target artifact has not been executed.

The reader correctly accounts for perfect-hash seed and overflow arrays before the block table, reads the format's mixed byte ordering, and checks selected stored ranges against supplied UCAS partition lengths. It reads immutable caller-owned spans and returns owning scalar results, with no allocation, file access, game branches or retained pointers. Large package descriptors and large asset containers do not inherit a small payload-buffer limit.

| Captured metadata | Verified inspection |
| --- | --- |
| Leon replacement | All 162 chunk and 2,948 block descriptors; largest chunk about 8.7 MB. |
| Townfall global container | Its chunk and 53 block descriptors, including seed-table displacement. |
| Townfall main container | Header geometry, sampled descriptors and a selected 12.3 MB package descriptor; 29,739 hash seeds correctly locate the block table. Paired asset length is about 128 GB. |

These are metadata results. Directory contents, full logical coverage, payload decoding, platform cooking, engine mounting and mod effects remain unverified. Declared compression methods have no decoder implementation here; encrypted, signed and unsupported layouts are refused. No universal compatibility or installation capability has been enabled.

The byte schema is pinned to retained primary CUE4Parse sources; retained retoc writers establish construction rules. One strict native compiler warning was corrected, and the same focused host and target checks passed afterward. Raw game/mod captures and target binaries remain in private development evidence.

Source and reusable verification instructions: [IoStore reader](ps5-mod-bridge/native-host/iostore/README.md). The actual unchanged-mod trial is recorded separately in [Townfall mod trial](2026-10-01-townfall-mod-trial.md).
