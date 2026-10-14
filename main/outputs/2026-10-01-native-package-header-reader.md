# Automatic Unreal package-header inspection

The shared native Zen reader now parses unversioned UE5.3+ package summaries and batched name maps from bounded, already decoded header spans. It preserves raw summary offsets, flags, mapped-name references, name hashes and UTF-8 or UTF-16LE bytes in caller-owned output. It resolves local Package names; Container and Global references remain unresolved. It performs no allocation or I/O and has no game, mod or asset-name branches.

The format rules are grounded in [CUE4Parse at the retained commit](https://github.com/FabianFG/CUE4Parse/tree/28f385e622468c4ab84997d596b46bb1c3adfcfa), particularly `FZenPackageSummary`, `FMappedName` and batched `FNameEntrySerialized`. The caller supplies the summary layout and cell-offset presence. These unversioned headers omit the engine version needed for authoritative branch selection; this reader does not discover that version or infer a target platform.

Sixty-eight focused host checks pass, including rejected layouts, malformed spans, output limits and alias handling. Both retained real header slices match the source-backed field receipt. The same C reader also compiles and links for PS5 with only `memcpy` and `memset` imports; that ELF has not been executed on the console.

A local integration experiment composed the committed native IoStore reader with this reader. It automatically enumerated every `ExportBundleData` chunk in the unchanged downloaded Leon bundle, assembled bounded header spans from `None` blocks and tried the two implemented cell-offset layout hypotheses. The walking code selected no individual game, mod or asset paths.

| Observed result | Count / scope |
| --- | --- |
| Asset package chunks accounted for | 127 of 127 |
| Unique structurally coherent layout candidates | 127 |
| Ambiguous or rejected headers | 0 in this bundle |
| Header bytes supplied to the reader | 627,303; largest header 29,659 bytes |
| Container/package data read by the walking process | 635,075 bytes |
| Independently captured reference headers matched | Both: 688-byte texture and 15,249-byte mesh headers |
| Source container bytes changed | None; complete before/after hashes agree |

All observed candidates include the cell-offset pair. This is a structural observation under the tested schemas, not a trusted engine-version detection result. The walking process streams small spans rather than loading the 185 MB UCAS into memory. This particular integration ran on the development computer and does not establish execution of the combined walk on PS5.

This advances automatic asset inspection. It does not decode serialized exports, mesh render data, texture bulk data or shaders, recook assets, provide UE4SS's Unreal services, install a mod or prove in-game acceptance. Other summary layouts, versioned packages, compressed-block assembly and automatic trusted cook-profile discovery remain outside this slice. The reusable parser is in [native-host/unreal-package](ps5-mod-bridge/native-host/unreal-package/README.md); private experiment receipts retain the exact input identities and diagnostics.
