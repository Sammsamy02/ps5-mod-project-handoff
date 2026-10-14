# Container capability matrix — evidence-grounded (worker-6-containers, 2026-10-04)

Status vocabulary:
- **proved-on-target** — executed on the actual PS5 with receipts (console ELF ran, game files untouched unless stated).
- **host-proved** — ran on the Darwin host against real original bytes with receipts; PS5 compile/link may also pass, but no console execution.
- **declared-only** — source/docs claim it; no executed receipt found in the checkpoint or locally.
- **gap** — no implementation or explicitly unknown engine semantics.

Ground rules: "a console reader can consume the bundle" (intake/decode/verify) is tracked separately
from "the contained mod executes" (game mount/in-game acceptance). Only checkpoint fields and local
receipts count; comments in source do not.

## 1. ZIP (Nexus download container)

| Operation | Status | Evidence |
|---|---|---|
| Detect + bounded list (methods, flags, unsafe paths) | host-proved (this pass) | `evidence/zip-roundtrip.json`: 8 real Nexus originals, store+deflate only, no encryption, no data descriptors except ue4ss/leon, no unsafe names |
| CRC verify | host-proved (this pass) | `testzip()` clean on all 8 |
| Extract | proved-on-target | `nativeArchivePreparation`: scan+extraction on unchanged stamina ZIP executed on PS5 (`nativeMaterialization`: 2 members, 2811 decoded bytes, independent readback); plus host 7+5 cases |
| Repack/rewrite | proved-on-target (same-family) | `native_zip`/`native_zip_combined_intake` lineage; failure corpus 312 checks/31 cases |
| Hostile-ZIP rejection | host-proved | `native_zip_method_preflight` fixtures (traversal, zip64 mismatch, bad central offset, data-descriptor, unknown flags...) |
| Encrypted ZIP | gap | No AES-ZIP fixture or receipt; detection only (flag bit observed absent in all 8 originals) |

## 2. UE PAK (classic loose-file)

| Operation | Status | Evidence |
|---|---|---|
| Footer/index detection (v11) | host-proved (this pass) | `evidence/pak-probe.json`: all 6 saved paks parse; version 11, frozen=0, empty footer compression table (uncompressed), mount `../../../` |
| Read entries + payload verify | host-proved + PS5 compile/link | `unreal_container_survey`: 4544 declared files, 3 original payload SHA-1 all match; 10232 green checks; PS5 link-only ELF cfcd4392... |
| Read on console | proved-on-target (reader ran) | `native_pak_*` lineage + `nativeArchivePreparation.nativeMaterialization` on PS5; **in-game mount: not proved** (`in_game_mount_result: false`, FTP550 route unavailable) |
| Write/repack (prepared candidates) | host-proved | `native_pak_writer`: 270 host behavior checks, 3 actual originals exact, PS5 compile+link; **`ps5_executed: false`, `game_acceptance_proved: false`** |
| Large None-compression payloads | host-proved | 4 MiB selected-payload cap lifted to 4194304 with 1 MiB zlib block cap; 48 real records 1–4 MiB motivated it; real >1 MiB payloads NOT read/validated (`real_large_payloads_validated: false`) |
| Path-hash (mount key) generation | host-proved (frozen offline match) | 4544 captured vs 3 generated pairs, 0 mismatches; **`native_path_semantics_verified: false`** — deferred before any in-game result |
| Encrypted index/entries | gap | No encrypted-PAK fixture or receipt; detection only via index read failure |

## 3. UE IoStore (utoc/ucas) — PS5 Zen variant observed

| Operation | Status | Evidence |
|---|---|---|
| Detect Zen TOC magic `-==-` (0x2d3d3d2d), not 0xCAECA5A9 | host-proved (this pass) | `evidence/utoc-retoc-verify.json` + hexdumps; matches checkpoint `townfall-original-zen-headers-1` |
| Verify TOC+UCAS pair (third-party pinned tool) | host-proved (this pass) | retoc v0.1.5 `verify` exit 0 on Leon trio utoc and 1_999_P.utoc |
| Full byte roundtrip unpack→pack | host-proved (existing receipt, re-checked) | `container_repacking`: 27 chunks/46349508 bytes identical, container id + toc version preserved, source unchanged; corpus hashes today still match receipt source_before/after |
| Native C IoStore reader | host-proved + PS5 link | `native_iostore_reader`: 9788 host checks, PS5 link passed; **`ps5_executed: false`, payload_verified: false, runtime_supported: false, installation_supported: false`** |
| Zen chunk reader / v5 header merge / BLAKE3 | host-proved (host_only) | `portable_iostore_work`: none-chunk reader, v5 header merger, blake3 portable — **`native_installer_integrated: false`** |
| In-game IoStore mount | gap | No receipt; `in_game_mount_result: false` |
| IoStore-in-PAK wrapper generation for mods | host-proved (candidates) | `nativeArchivePreparation.preparedCandidate`: 21 host cases, Leon exact bytes 185497319, bound on console via `nativePreparedLeon` (below); `platform_compatible: false` |
| Prepared IoStore bundle consumed on console | proved-on-target (reader side only) | `nativePreparedLeon`: target_executed, 3 files, group 1, bound_bytes 185497319, independent readback, game files untouched — **installation/runtime NOT supported** |

## 4. Bethesda archives (bsa/ba2), other engines' containers

| Operation | Status | Evidence |
|---|---|---|
| All | gap | No fixture, receipt, or decoder in checkpoint. FORMATS.md declares role mapping only (`.bsa/.ba2` under `pak` role: "Bethesda archives hold PC assets and are converted") — declared-only, no converter exists |

## 5. RAR / 7z / other Nexus archive formats

| Operation | Status | Evidence |
|---|---|---|
| RAR intake | host-proved (extracted once) | `townfall-62-rar-intake-1` corpus exists (1_999_P trio extracted from a RAR); no general RAR decode receipt in checkpoint; treat as bounded one-off |
| 7z | gap | No receipt |

## 6. Nexus mod-package composition (Claude backend, C + Python twins)

| Operation | Status | Evidence |
|---|---|---|
| Automatic archive scan/grouping | host-proved | `automatic_container_grouping`: 30 focused cases, production CLI identical on actual archive, Leon trio member hashes verified |
| Mod bundle (.mfb) write/read, composer, loadlist | host-proved (declared host+C parity, tests in backend tree) | FORMATS.md + `modforge_bundle.h`/`modforge_compose.h`; not re-executed this pass; no console receipt |
| PRX/SELF SDK retarget (4-byte stamp) | proved-on-target | FORMATS.md: "exactly the one verified on the console"; SDK0x11000000 ESDKVERSION fix |

## 7. Compression/codec dependencies (pinned)

| Component | Status | Evidence |
|---|---|---|
| zlib 1.3.2 | host-proved + PS5 compile/link | `native_zlib_codec` lineage; closure-3 descriptor libz.a 4b727b9f... |
| xz/liblzma 5.8.4 | host controls + PS5 compile/link | `native_lzma_dependency`: 5 host controls; **`decoder_heap_bounded: false`, `production_intake_proved: false`, ps5_executed: false** |
| libarchive 3.8.7 | pinned sources + host build | libarchive-3.8.7.tar.xz + host libarchive.a 65b5b6b8...; console ZIP evidence under `nativeArchivePreparation` |
| MbedTLS 3.6.7 (commit 068ff080...) | pinned | TLS-side (https lineage), not archive-codec; retained pin, unchanged |
| Oodle (IoStore chunk codec) | gap | No decoder. Zen TOCs on PS5 may reference Oodle for compressed chunks; nothing in the workspace decodes Oodle. All observed loose-file paks had empty compression tables |

## Cross-cutting honesty lines

- Nothing in this matrix proves any mod **executed**. Closest runtime evidence elsewhere in the
  project is the UE4SS/DLL loader lineage, not these containers.
- Console-executed container work is **reader-side only** (scan, extract, verify, prepared-candidate
  readback) with `game_files_modified: false` everywhere.
- The only complete saved utoc+ucas pairs are the Leon trio and the retoc corpus 1_999_P; the five
  main-game utoc captures lack ucas siblings, so chunk-level work against main-game IoStore payloads
  is not currently reproducible from saved bytes alone.
