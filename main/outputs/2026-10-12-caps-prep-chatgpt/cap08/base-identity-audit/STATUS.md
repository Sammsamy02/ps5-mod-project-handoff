# Base identity audit (Mac-only)

The verified mod package-import edge from Deluxe material to M_Invisible resolves by exact package ID plus public hash a48b2ffd18348a31. Nine ScriptImport import-map slots resolve against the previously bound global table, including class/CDO/package names. This identifies metadata references; it does not identify a material-parent property or prove runtime resolution.

No provenance-sufficient base export payload is available for comparing the two Deluxe replacements. Two partial external-header candidates exist for mesh dependencies, but their own saved receipts say atomic_source_identity_proven=false and range_final_control_completion_observed=false. They remain quarantined and are not decoded or used as identity proof.

Existing chunk0 ContainerHeader now independently passes its TOC BLAKE3-20 hash. It confirms external package 953039a7b296b2ee imports 34e14272697a3b6a,9f5b09b4cf73717a,a472dfe5d5bd43b7. All three occur in that captured store and have empty imported-package arrays, closing that captured package-store subtree at three leaves. Their export/public-hash identities still need payload evidence. Overall import closure remains incomplete because chunk10 store entries and base export identities are missing.

Exact missing-input requirements are in base-input-audit.json: source TOC hash, container owner, chunk ID/row, logical range, expected IoHash and physical stored-block descriptors. The initial base export/store inputs are:

| Package | Captured owner data file | Logical bytes | Expected chunk IoHash |
|---|---|---:|---|
| 1fb37f42e0faef25 | pakchunk10-ps5.ucas | 122,393 | e138b6e53deb1dc665df2321107fe1bf70f44228 |
| 2c77facba1d32785 | pakchunk10-ps5.ucas | 1,508 | 597884ac2e57af5100097e846814e85c4edfd468 |
| 953039a7b296b2ee | pakchunk0-ps5.ucas | 500,891 | 4b45da629db4ad60864b4f19b16372b6d3e6c9e9 |
| 976ba4c659563aa7 | pakchunk10-ps5.ucas | 1,198,188 | b0640818cef6ee7ab498dcd121fb1f7864481b29 |
| owner ContainerHeader | pakchunk10-ps5.ucas | 416 | c5e339d2dab53bb538d4d554d98e2516bf30a5f3 |

M_Invisible has no captured base owner; this is not global absence or “new package” proof. Its mod-local public export identity already matches the requesting mod import. The receipt also gives exact metadata geometry for the three downstream base dependency exports, without initiating an acquisition.

The five original base TOCs were opened read-only: streaming SHA256 in <=1MiB buffers before/after, 718,890 aggregate random-read metadata bytes with every range offset/length/hash recorded. No whole-file TOC materialization or full TOC parsing. Existing parser hash helpers and targeted lookup checks bind each requested row. All TOC hashes and small inputs are unchanged after the audit. No base/mod asset UCAS read; only the already-bound 3.4MB ScriptObjects metadata was replayed.

Three focused tests pass: exact package/public-hash match; absent base target remains unknown; ambiguous public-hash identity refuses. tdd-red.txt records missing-feature failures; tdd-green.txt contains final focused tests. base_identity_resolved=false, compatible=false and native_shader_proven=false remain explicit.
