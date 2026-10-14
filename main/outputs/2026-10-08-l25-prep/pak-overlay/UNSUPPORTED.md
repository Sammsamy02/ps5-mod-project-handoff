# UNSUPPORTED.md — honest capability matrix for overlay_builder.py

Agent GK (Claude round-6 item 3a), 2026-10-08. Classic-PAK intake +
shader-fallback substitutions sections by agent HI (Claude round-7 items
A+B), 2026-10-10. Everything below is decided against LOCAL evidence only
(the pinned CUE4Parse/retoc/repak schema copies, the retained IoStore
spike trees, and the observed container captures). Where a fact is not
locally pinned, the builder refuses with a machine-readable `Refused`
verdict — it never guesses. Pin keys (P1..P8) are defined in the header
docstring of `overlay_builder.py`; classic-PAK pins P9..P11 likewise.

## 1. TOC version span (EIoStoreTocVersion 0..10, P1)

| Version | Name | Verdict | Why (local evidence) |
|---|---|---|---|
| 0 | Invalid | REFUSED | not a container |
| 1 | Initial | UNSUPPORTED | pre-v2 has no directory index (P2 gate `Version >= DirectoryIndex`); no local observed file |
| 2 | DirectoryIndex | UNSUPPORTED | no perfect-hash arrays is handleable, but pre-v8 FIoChunkHash size is not retained (P7 unknown 3) so meta rows / EOF closure cannot be verified; no local observed file |
| 3 | PartitionSize | UNSUPPORTED for build; HEADER_PARSED verdict | LOCALLY OBSERVED (Stellar Blade chunk0/global utocs, 2026-09-30 audit): 144-byte header, no perfect-hash counts, Indexed, directory index present, partitionSize UINT64_MAX — header parse and verdicts work; but no local .ucas pairing for a v3 container and pre-v8 meta size unknown, so overlay composition is refused |
| 4 | PerfectHash | UNSUPPORTED | seeds-only hash map (P2), pre-v8 metas unverifiable; no local observed file |
| 5 | PerfectHashWithOverflow | UNSUPPORTED | seeds+overflow like v8, but pre-v8 FIoChunkHash metas unverifiable (P7 unknown 3); no local observed file |
| 6 | OnDemandMetaData | UNSUPPORTED | adds OnDemand meta regions (P2 gate `Version == 6 AND OnDemand flag`); no local observed file |
| 7 | RemovedOnDemandMetaData | UNSUPPORTED | pre-v8 metas unverifiable; no local observed file |
| **8** | **ReplaceIoChunkHashWithIoHash** | **SUPPORTED (parse + emit + overlay)** | the only version pinned end-to-end locally: Townfall pakchunk0-ps5.utoc + Leon PC mod trio (P8) through the one-game adapter trial (P5); metas are 24-byte rows with a 20-byte BLAKE3 IoHash (P3+P6) |
| 9 | AddedSourceHashes | UNSUPPORTED | the pinned CUE4Parse subset names the version and reads `TocSourceHashCount` only under a Game >= UE6_0 header gate (P1); the array-region gate for source hashes is not locally pinned and no local file carries version 9 |
| 10 | ContainerEncryptionMethod (Latest) | UNSUPPORTED | adds an explicit EncryptionMethod byte + AES_CTR IV array semantics (P2); no local observed file; version 10 containers are typically encrypted anyway (see section 2) |

Version-to-engine mapping honesty: which UE minor ships which TOC version is
NOT locally provable except v8 <-> the observed UE5.x captures (P8). The
builder therefore never maps a version byte to an engine version; it keys
everything on the version byte itself. retoc's own README (P4 tree) notes
pre-5.0 IoStore versions "were not well defined" and needs per-game
overrides — the same caution applies here.

## 2. Encrypted containers — REFUSED (policy)

Detection (P1/P2): `ContainerFlags & Encrypted` OR non-zero 16-byte
`EncryptionKeyGuid` OR (version >= 10 semantics) — any of these produce
`encrypted_container_refused` / `encryption_key_guid_refused` before any
payload is touched. Policy: AES keys and decryption are OUT OF SCOPE on the
Mac until Phase B. No local record decrypts any container (every captured
TOC in the workspace — Townfall v8, Stellar Blade v3, Leon PC trio v8 — has
encryption flags clear and zero key GUIDs), so nothing in the local evidence
contradicts this refusal. Special per-game hardcoded directory-index keys
(the pinned CUE4Parse copy contains literal keys for two specific games in
`FIoStoreTocResource.GetDirectoryIndexBuffer`) are intentionally NOT
implemented: they are per-title training knowledge, not local evidence, and
would violate the game-agnostic law.

## 3. Signed containers — REFUSED

`Signed` flag set -> `signed_container_refused`. The signature array layout
(hashSize + two byte arrays + SHA1 per block) is readable (P2/P7) but its
verification semantics are explicitly "unknown" in the retained header spec
(P7 unknown 7), and any derived index would invalidate the TOC signature we
cannot recompute. All local captures have the signed flag clear.

## 4. Compression — all-stored profile only

Admission requires `CompressionMethodNameCount == 0` (every block method-0
stored). Rationale: every locally observed IoStore container (P8) is
all-stored; block-level compression would still be byte-carriable in
principle (the TOC maps methods), but no local record exercises a
compressed IoStore container, so decode/verify paths would be untested
guesses. The `Compressed` container flag and any non-empty method-name table
refuse. Classic PAK (`.pak`) compression codecs are a different path (see
section 7).

## 5. Partition geometry

Supported inputs: `PartitionCount == 1` and `PartitionSize == UINT64_MAX`
(unbounded single partition) — the shape of every locally observed base
container and PC mod trio (P8; the CUE4Parse reader forces these values for
TOC version < 3 anyway, P2). A base container already split into multiple
partitions (PC >4GB splits) is refused (`input_must_be_unbounded_single_
partition` / `unsupported_partition_count`). Derived output is always the
proven two-partition shape: partition 0 = untouched base UCAS, partition 1 =
`<stem>_s1.ucas` sidecar, `PartitionSize = base UCAS size` (P5 emit).

## 6. Mod archive shapes refused

- more than one, or zero, ContainerHeader chunk (`EIoChunkType` id byte 6)
  in a mod TOC — the merge needs exactly one (`mod_requires_exactly_one_
  container_header`); its chunk id must equal the mod's own container id +
  `00000006`.
- mod ContainerHeader not the v5 `SoftPackageReferencesOffset` layout, or
  with nonempty optional/localization/redirect/soft-reference sections, or
  unclaimed nonzero package-store bytes (`unsupported_container_header_
  layout`, `nonempty_container_header_section`, ...) — same admission as
  the proven one-game merge (P5).
- memory-mapped chunks (meta flags byte 2, or chunk-type MemoryMappedBulk
  data id 4) in any mod, and memory-mapped base chunks being replaced —
  the sidecar is a plain file partition; mmap-backed rows cannot be
  faithfully redirected (`memory_mapped_*` refusals; P5 admission).
- mod UCAS trailing garbage: >15 bytes or nonzero bytes beyond the last
  block's aligned end (`mod_ucas_unaccounted_trailing_bytes`,
  `nonzero_mod_ucas_trailing_padding`) — the sidecar must be exactly the
  mod's block stream (byte-identity law).
- mount point / root-name mismatch between a mod and its base
  (`mount_point_mismatch`, `root_name_mismatch`).
- a mod whose directory index references its own ContainerHeader chunk
  (`selected_file_references_unknown_chunk`) — the header row never enters
  the derived index (P5 behavior).
- sidecar budget: total overlay sidecar must stay under 512 MiB AND under
  the base UCAS size (`sidecar_would_require_more_than_one_new_partition`)
  — one new partition is the proven shape.

## 7. Classic PAK mods (`.pak`) — read + verdict in-tool; conversion via the pinned retoc binary; content-conversion honesty

The builder now has a classic-PAK input path (agent HI): a pure-python
v11 reader (`parse_pak` / `inspect_pak`, layout pins P9-P11 in the module
docstring: repak footer/pak.rs/entry.rs schema, the boolean-width +
directory-schema research notes, and this project's own promoted v11
writer), a content-family classifier (`classify_pak_entries`), and
conversion ORCHESTRATION behind `--classic-pak [ID=]PATH.pak --retoc-tool
PATH` (`convert_classic_pak`). Verdict matrix:

| Input profile | Verdict | Refusal code / notes |
|---|---|---|
| v11, unencrypted, all-stored, only cooked split packages (.uasset/.umap + .uexp siblings, optional .ubulk/.uptnl/.m.ubulk, .ushaderbytecode, scriptobjects.bin) | CONVERTIBLE_VIA_PINNED_RETOC | converted by the pinned local retoc v0.1.5 binary (deterministic: identical runs → identical shas, tested on the real retained archive), output admitted through the ordinary ModSpec admission (TOC v8 via the fixed override pair), merged as a normal IoStore mod |
| PAK v1..v10 (footer located; v5/v7/v8a/v8b/v9 real fixtures retained) | UNSUPPORTED | `classic_pak_version_unsupported` — only v11 is pinned end-to-end locally |
| Encrypted index / non-zero key GUID (real v11 fixtures retained) | REFUSED | `classic_pak_index_encryption_refused` / `classic_pak_encryption_refused` — keys out of scope until Phase B |
| Encrypted entries | REFUSED | `classic_pak_encryption_refused` (per-entry encrypted bit) |
| Compressed entries (real Zlib fixtures retained) | REFUSED | `classic_pak_compression_refused` with the footer-declared codec name; decompression/recompression belongs to the conversion tool, and the sidecar profile carries stored blocks only |
| Non-encoded records / negative locations / tombstone-only | REFUSED | `classic_pak_nonencoded_entries_unsupported` / `classic_pak_empty` |
| Path-hash-only (no full directory) | REFUSED | `classic_pak_full_directory_required` — names would have to be guessed |
| Loose non-asset files (ini/txt/bin…), orphan bulk, uncooked packages | REFUSED for conversion | `classic_pak_content_refused` — no IoStore chunk model for loose files (use the mod manager's classic-pak staging path); uncooked packages would be silently dropped by the tool, so the builder refuses first |
| Per-entry payload corruption (real corruption fixture retained) | REFUSED | `classic_pak_entry_sha1_mismatch` — stored SHA1 verified against every retained producer family |
| `--classic-pak` without `--retoc-tool` and no reusable staged trio | REFUSED | `classic_pak_conversion_tool_unavailable` with the pointer — pure-python asset re-serialization would fabricate export bundles (never-fabricate law); the pinned binary is at `.superpowers/native-dependency-spike/retoc-v0.1.5-raw-roundtrip-1/tool/retoc_cli-aarch64-apple-darwin/retoc` (pinned repak build: `unreal-library-container-survey-1/mount-research/tools/build/aarch64-apple-darwin/release/repak`) |

HONEST LIMIT: no cooked legacy BP archive exists in any local tree, so a
full CONTENT conversion through retoc to-zen is UNPROVEN — proven locally:
the reader on real corpora (byte-oracle against the pinned repak binary),
the exact orchestrator invocation on the real retained v11 archive
(deterministic, produces a v8 TOC), failure/absence propagation, staging
reuse laws, and the admission gates. GN's LogicMods stage 1 still needs a
user-supplied real BP pak to exercise the content path end-to-end; engine
acceptance of any derived container remains the standing Phase 0 question
(same as section 8).

Provenance laws kept: the source .pak is read-only (sha-pinned in the
journal, drift = refusal); staged trios live under
`<state-dir>/classic-staging/<mod_id>/` (removed on uninstall-to-pristine);
re-derives reuse the staged trio when its recorded shas match, else
re-convert (deterministic); the overlay carries the converted trio's
bytes verbatim — only the index and merged ContainerHeader are derived.

## 7a. Substitutions (L1/L2 shader fallback) — derived entries only

`--substitutions FILE` consumes GM's classifier directives
(`shader-fallback/classify_materials.py` output shape, assembled by the
mod manager): rows `{mod_id, package_id, action}` where action is

- `hide` — suppress THIS mod's override for the package id: the mod's
  chunk rows, store entry and directory rows for that pid contribute
  nothing; the base export (with its own PS5 shader-map hashes) shows.
  Suppressed contributions never count as conflicts.
- `closest-match` — repoint the DERIVED merged store entry's import
  `from_package_id` → `to_package_id` (which must exist in the base
  container store). The mod's package bytes stay verbatim; only the
  derived ContainerHeader changes (store-entry surgery is the proven
  merge surface; FFilePackageStoreEntry imports are positional FPackageIds).

Refusals: `substitution_mod_unknown`, `substitution_target_not_in_mod`,
`substitute_not_in_base_store`, `substitution_import_not_found`,
`substitution_target_conflicts_across_mods` (closest-match on a
later-wins-contested pid), plus strict schema validation. The receipt and
journal carry a per-row ledger (see the report for the exact shape);
originals are never touched (law).

## 8. Honest gaps in the supported path

- The derived TOC's negative-seed explicit-overflow lookup emit is verified
  against the pinned CUE4Parse dispatch logic (P4/P5) and by local
  re-resolution of every row — but actual ENGINE acceptance of a derived
  multi-mod index is an unproved runtime question (same standing as the
  historical trial: staged bytes verified, mounting never observed — see
  PHASEA-PLANS #7 "engine acceptance unproved").
- The base ContainerHeader chunk is read by bounded ranged reads from the
  base `.ucas` when present, or from a pinned pre-extracted file
  (`--base-container-header`) when the dump lacks the huge file; both paths
  are tested (synthetic + Leon fixture respectively).
- Mod meta rows are carried verbatim (their BLAKE3 IoHashes stay the mod's
  own). The only recomputed hash is the merged ContainerHeader chunk's,
  via the pure-python BLAKE3 verified against the official test vectors and
  the pinned dylib (P6).
- Game updates: detected by sha mismatch against the journal (STALE
  verdicts on install/uninstall/verify), never auto-repaired. Restoring
  requires the journal's backup to still match its recorded sha.
