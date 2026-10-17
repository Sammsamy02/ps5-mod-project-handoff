# Profile rules v2 — complete-header container dispatch (worker-container-headers, 2026-10-04)

Supersedes `worker-6-containers/PROFILE-RULES.md` R1 ("magic-driven dispatch") and the
two-magic claim in its `iostore` schema. v2 keys handler selection on the COMPLETE parsed
header/structure tuple. Worker-6's file is preserved unchanged as provenance.

## 1. Selection key (engine-generic, profile-parameterized)

A container is admitted only when its **full parsed tuple** matches the build profile's
admission set. Fields, in evaluation order (cheapest hard-fail first):

| # | Field (source: HEADER-SPEC.md) | Townfall admission set | Failure reason emitted |
|---|---|---|---|
| 1 | TocMagic == `-==-`×4 | exact (generic IoStore format magic, S1/S4) | `toc-magic:<hex>` |
| 2 | Version (u8 @16) | {8} (ReplaceIoChunkHashWithIoHash); enum bound 1..10 (S1) | `toc-version:<n>` (+ `outside enum` if n>10 or 0) |
| 3 | TocHeaderSize (u32 @20) | {144} (S1 SIZE; S5/S6 observed) | `toc-header-size:<n>` |
| 4 | TocCompressedBlockEntrySize (u32 @32) | {12} (S3) | `toc-block-entry-size:<n>` |
| 5 | ContainerFlags (u8 @80) | ⊆ {Indexed} (i.e. Compressed/Encrypted/Signed/OnDemand all rejected today) | `encrypted-container` / `signed-container` / `ondemand-metadata` |
| 6 | Compression methods referenced by blocks (census per S3) | ⊆ {None, Zlib} — None proved by receipts; Zlib admissible via pinned zlib 1.3.2 but no saved IoStore block references it (latent path) | `compression-referenced:<names>` |
| 7 | Structural EOF closure (computed end == file size) | required (proved on 8/8 corpus) | `structure-mismatch:<computed> vs <size>` |
| 8 | PartitionCount (u32 @52) | {1} (S6 + all parsed) | warning `partition-count:<n>`; admission requires 1 until multi-partition evidence |
| 9 | Sibling .ucas (same stem) | required for chunk-level ops | `missing-ucas-sibling` (header-only otherwise) |

Verdicts: `supported` (1–9 all pass, sibling present) · `supported-header-only` (1–8 pass, no
sibling) · `unsupported` (any hard-fail above; magic alone never produces a positive verdict).

## 2. Profile schema v2 (per-BUILD facts; no per-mod cases)

```json
{
  "schema_version": 2,
  "title_id": "PPSA33286",
  "evidence_provenance": [
    "outputs/2026-10-04-zcode-team/worker-container-headers/evidence/utoc-header-probe.json",
    ".superpowers/native-observations/townfall-manifest-capture-2026-10-01/container-header-receipt.json",
    ".superpowers/native-observations/townfall-original-zen-headers-1/profile.json",
    ".superpowers/native-dependency-spike/retoc-v0.1.5-raw-roundtrip-1/repack_runner.py"
  ],
  "iostore": {
    "toc_magic": "2d3d3d2d x4 (GENERIC IoStore magic; not a platform marker)",
    "admission_versions": [8],
    "admission_header_size": [144],
    "admission_block_entry_size": [12],
    "admission_container_flags_subset": ["Indexed"],
    "admission_referenced_methods": ["None", "Zlib"],
    "admission_partition_count": [1],
    "observed_per_container": [
      {"stem": "zzz_Leon_Townfall_2K_P", "version": 8, "entries": 162, "container_id": "0x9039560806956a15", "pair": "complete"},
      {"stem": "1_999_P", "version": 8, "entries": 27, "container_id": "0x75b4f107f45fab23", "pair": "complete"},
      {"stem": "global", "version": 8, "entries": 1, "container_id": "0xffffffffffffffff", "pair": "utoc-only"},
      {"stem": "pakchunk0-ps5", "version": 8, "entries": 59478, "container_id": "0x8f8bb2ccc07563e0", "pair": "utoc-only"},
      {"stem": "pakchunk1-ps5", "version": 8, "entries": 197, "container_id": "0xf4c0cfa40e8d6087", "pair": "utoc-only"},
      {"stem": "pakchunk10-ps5", "version": 8, "entries": 16, "container_id": "0x8d988b9bf60b165b", "pair": "utoc-only"},
      {"stem": "pakchunk11-ps5", "version": 8, "entries": 27, "container_id": "0x34b343b8c75a63f1", "pair": "utoc-only"},
      {"stem": "pakchunk12-ps5", "version": 8, "entries": 30, "container_id": "0x8099615cdecd56d6", "pair": "utoc-only"}
    ],
    "name_tables_observed_empty": true,
    "oodle_referenced_anywhere": false
  },
  "wrapper_pak_pattern": "same-stem 347-byte v11 pak carrying utoc/ucas sidecars (observed on both complete pairs)",
  "ucas": {"has_magic": false, "classification": "same-stem sibling of utoc only",
           "container_header_chunk_layout": "unknown (no retained source)"}
}
```

## 3. Rules (v2)

- **R1′ (replaces R1): complete-header dispatch.** Magic recognition only opens parsing; the
  verdict comes from fields 2–9 above, each failure reason citing the actual parsed value.
  There is exactly one IoStore TOC magic; "which magic" is never a handler selector.
- **R2 (unchanged, sharpened): compression admission from the profile**, now keyed on the
  block-level census (which methods blocks actually reference), not just the name table. An
  admissible-but-unreferenced name (e.g. a dormant Zlib slot) is a warning, not a failure.
- **R3 (unchanged): encryption/signing detection-only.** Encrypted flag, non-zero key guid, or
  Signed flag ⇒ unsupported with the flag value; no key guesses; signature verification has no
  implementation (S2 explicitly skips it).
- **R4 (unchanged): payload caps stay profile facts** (4 MiB selected / 1 MiB zlib block).
- **R5 (unchanged): cooked-payload platform tagging** — platform incompatibility is decided by
  package-level receipts (`platform_compatible: false`), never by TOC magic/version.
- **R6 (new): structural EOF closure is a mandatory admission check.** A header that parses but
  whose array region does not close at the file size is `structure-mismatch` — this is what
  actually proves "supported structure" and would catch a wrong-version mapping (e.g. UE6
  trailing-field reading) empirically.
- **R7 (new): ucas classification rides the utoc.** No ucas-side magic exists (verified on all
  three saved ucas files); a ucas without its utoc is `unclassifiable-orphan`, and pairing is
  same-stem only. Wrapper-pak rule from worker-6 retained (347 B v11 wrapper = sidecar carrier).
- **R8 (was R7): new builds** start from schema v2 with empty admission sets filled from that
  build's parsed evidence. Nothing Townfall-specific enters generic code.

## 4. What changed vs worker-6 PROFILE-RULES and why

1. `toc_magic_observed: "0x2d3d3d2d (Zen '-==-')"` + `desktop_magic_0xCAECA5A9_observed: false`
   → deleted. The magic is generic; `0xCAECA5A9` is unproven (no retained source defines it).
2. "Accept Zen `-==-` and desktop `0xCAECA5A9` magics" → single magic + version/structure
   admission (this file §1).
3. "Version fields after the Zen magic were NOT decoded" (worker-6 limit) → now decoded and
   proved: version 8 / 144 / 12 / {None} on 8/8 corpus files with exact EOF closure.
4. Entry counts, directory-index presence/sizes, container ids, partition facts are now recorded
   per container in the profile (they were absent).

## 5. Remaining gaps (unchanged honesty lines)

- In-game IoStore mount: no receipt anywhere (`in_game_mount_result: false`).
- Prepared-wrapper acceptance (`nativePreparedLeon`) remains reader-side only.
- utoc directory-index string-table encoding and ucas ContainerHeader chunk layout: unknown
  from retained sources (HEADER-SPEC §4) — needed before name-level pak↔utoc crosswalks.
- 4699-vs-4544 pak delta: still open; settle from already-saved pak index bytes
  (CLASSIFICATION.md §5).
