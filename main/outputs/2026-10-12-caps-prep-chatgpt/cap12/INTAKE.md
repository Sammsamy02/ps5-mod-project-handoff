# Existing-local shader intake (Mac-only)

Search result: local-payload-search.json records the scoped rg search, 47 matching local files and 14 paired TOCs. No paired TOC contains type8/9 entries; no .ushaderbytecode or type8/9 raw chunk filename was matched. Existing global cache is shared-code metadata, existing shader type-info is metadata, and inspected bounded-range capture is an external-package header. This does not prove absence of anonymously named/uncatalogued slices. No real shader-code payload was found with adequate provenance.

Implemented local_shader_intake.py. It only opens paths explicitly supplied in a JSON job. It verifies pinned parser and TOC source SHA256, uses v8 parser admission with strict_admission=False (reserved header bytes relaxed only) and explicitly validates every perfect-hash lookup row, restricts selection to explicit unique type8/9 chunk IDs, reconstructs bytes through stored block descriptors and checks BLAKE3-20 against TOC metadata. Geometry comes from parser semantics rather than equating logical offset with a physical offset. Source SHA256 is checked before and after reconstruction, with source identity/mtime guards. Physical EOF, unknown compression, zero rows, nonshader rows and hash mismatch refuse.

Caps: supplied local source <=64 MiB, individual reconstructed chunk <=4 MiB, total job <=8 MiB and <=16 selected IDs. A source may be a full .ucas with kind=full, or an already-local .ucas slice with kind=slice and explicit physical_base. The slice hash verifies that supplied file; the chunk IoHash ties reconstructed bytes to the pinned TOC row. It does NOT establish the full original UCAS hash or independently prove capture history. Anonymous .bin slices are not silently assumed to be UCAS.

Job shape: schema=1; parser, toc and ucas each contain actual path and sha256; ucas additionally states kind and physical_base when sliced; chunk_ids is a nonempty list of exact TOC keys. manifest-local-intake-requirements.json contains source-derived block coverage and expected chunk hash for the two chunk0 manifests, but deliberately leaves actual local source paths/hashes missing. It contains no acquisition action or fabricated payload.

Run after actual local inputs exist:

```sh
PYTHONDONTWRITEBYTECODE=1 python3 outputs/2026-10-12-caps-prep-chatgpt/cap12/local_shader_intake.py PROVIDED-JOB.json --out-dir outputs/2026-10-12-caps-prep-chatgpt/cap12/NEW-VERIFIED-INTAKE
```

Every selected payload must validate before any output is created. Destination must be a new directory inside cap12; files are exclusive writes. Output is raw source bytes plus source/TOC/parser/hash receipts, with no manifest semantics or instruction-boundary claims.

Self-check: 15 scoped cap12 tests pass. Eight intake cases cover exact discontiguous reconstruction, local-slice origin, wrong chunk/source hash, caps, EOF, nonshader refusal and a valid-geometry zero-seed TOC lookup refusal before output creation. Synthetic CLI end-to-end fixture also successfully reconstructed exactly six bytes (bcdefg), with IoHash verified. synthetic-intake-fixture and synthetic-intake-verified are explicitly artificial; they are not PS5 shader evidence. tdd-red-intake.txt and tdd-red-slice.txt preserve missing-feature failures, and tdd-green.txt contains the current scoped suite.

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s outputs/2026-10-12-caps-prep-chatgpt/cap12 -p 'test_*.py' -v
```

No UCAS payload acquisition, network, console, compiler/dependency work, game executable analysis or Epic/Sony source copying occurred. A manifest decoder remains pending actual hash-verified type8 bytes; no fabricated shader data substitutes for them.
