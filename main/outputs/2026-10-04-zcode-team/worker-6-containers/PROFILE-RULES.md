# Profile-driven container rules (worker-6-containers, 2026-10-04)

Principle (from NEXUS_INSTALLER_SPEC PATHS AND PROFILES): per-BUILD facts live in profiles;
generic parsers stay game-agnostic. One profile per build's facts. No per-mod special cases.
This file defines the container-fact schema and the Townfall baseline filled from evidence;
other builds start from the same schema with their own observed facts.

## Profile schema (container facts)

```json
{
  "schema_version": 1,
  "title_id": "...",
  "build_id": "...",                     // e.g. disc/patch build fingerprint + content sha256 roots
  "evidence_provenance": ["receipt path", ...],
  "container_facts": {
    "pak": {"footer_version_observed": 11, "frozen_index_observed": false,
            "footer_compression_slots": 5, "mount_point_observed": "../../../",
            "entry_count_observed": 4699, "encrypted_index_observed": false},
    "iostore": {"toc_magic_observed": "0x2d3d3d2d (Zen '-==-')",
                "desktop_magic_0xCAECA5A9_observed": false,
                "wrapper_pak_pattern": "347-byte v11 pak carrying utoc/ucas sidecars",
                "chunk_codecs_available": ["None", "zlib"],
                "chunk_codecs_missing": ["Oodle"]},
    "archive_intake": {"zip_methods_observed": [0, 8], "encrypted_zip_observed": false},
    "asset_platform": {"windows_cooked_payloads_platform_compatible": false,
                       "open_questions": ["re-cook requirement", "package summary flags",
                                          "texture/VT tile layout"]}
  }
}
```

## Townfall baseline profile (facts with receipts)

- `title_id`: PPSA33286; `sdk_version`: 0x11000000; `required_firmware`: 13.60 (FORMATS.md,
  console-verified SDK fix).
- PAK: v11, unfrozen, empty method table, entry count 4699 in the saved pakchunk0 primary index
  (checkpoint survey counted 4544 classic names — the 155-name delta is recorded as an open
  question: duplicates/deleted entries in primary index vs unique full-directory-index names;
  do not "fix" the number without decoding the full directory index).
- IoStore: Zen `-==-` TOC magic; complete saved pairs only for Leon trio and 1_999_P corpus;
  main-game utoc captures lack ucas.
- Asset platform: `platform_compatible: false` for Windows-cooked packages (three receipts).

## Rules (engine-generic, profile-parameterized)

R1. **Magic-driven dispatch, never extension-driven alone.** `.pak` with a same-stem `.utoc`
    sibling is a wrapper (profile fact `wrapper_pak_pattern`), not a loose-file pak.
R2. **Compression admission set comes from the profile** (`chunk_codecs_available`). A method
    outside the set fails intake with `unsupported-compression:<name>`. Today that means Oodle
    chunks fail; when a codec is added with proof, update the profile, not the mod.
R3. **Encryption handling is detection-only.** Encrypted ZIP members, possibly-encrypted PAK
    index, unknown TOC signing ⇒ `unsupported-encrypted` reason. No key guesses.
R4. **Payload caps are profile facts**, not code constants where possible: 4 MiB selected-payload
    / 1 MiB zlib block caps are the host-proved bounds for the current reader generation; a
    different build's larger entries need new evidence before the caps move.
R5. **Windows-vs-PS5 cooked variants differ per build**: intake must tag every cooked payload
    with the source platform (from archive provenance + TOC/package headers actually parsed) and
    consult the profile's `asset_platform` facts. `platform_compatible: false` ⇒ class
    `unsupported-needs-conversion`, visible to the user; a conversion capability, if ever built,
    is separate engine-adapter work with its own proof.
R6. **Path-hash generation** (pak mount keys) stays a frozen offline-verified capability until
    native semantics are verified on a real mount; profile records
    `path_hash_native_verified: false`.
R7. **New builds (other UE titles, other engines)**: same schema; facts start empty and are
    filled from that build's saved originals/receipts. Nothing Townfall-specific may appear in
    generic intake code (mirrors "Townfall must not be embedded into generic code").

## Open questions for the controller (engine semantics unknown — not gaps we can close by guessing)

1. Zen TOC header fields beyond the magic (version/entry-count semantics on PS5) — decode against
   the retoc corpus with an exact fixture before trusting any native parser's field mapping.
2. Whether UE 5.6 PS5 accepts foreign-platform cooked packages at mount time at all.
3. Whether the 4699 vs 4544 entry-count delta is duplicates or deleted entries.
4. Whether prepared IoStore wrapper candidates (`nativePreparedLeon`, platform_compatible=false)
   can ever be mounted in-game; the readback proof is reader-side only.
5. Whether Oodle is referenced by any chunk in the complete saved pairs (Leon trio, 1_999_P) —
   a bounded TOC-method census would answer it; not done this pass to stay within bounds.
