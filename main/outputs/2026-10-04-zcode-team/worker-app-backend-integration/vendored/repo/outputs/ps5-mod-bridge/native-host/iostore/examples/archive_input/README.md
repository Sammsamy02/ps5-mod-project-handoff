# Original archive to IoStore preparation

`archive_job.c` joins archive intake, automatic container selection, ordinal-file extraction and the reusable original-file job in one ordinary host process. The unchanged download is its input. No extracted mod files, member-name recipe, raw chunks, premerged header, expected output or AI service participates in preparation.

```text
archive_job ARCHIVE SHA256 BYTES CONTEXT_TOKEN BASE_TOC BASE_HEADER \
  OBSERVED_BASE_UCAS_BYTES PRIVATE_QUARANTINE PRIVATE_OUTPUT_DIRECTORY
```

SHA256 and CONTEXT_TOKEN are 64 lowercase hexadecimal characters. BYTES and OBSERVED_BASE_UCAS_BYTES are decimal. The archive size/digest come from the download receipt; the context token binds the request's observations, without proving compatibility. BASE_TOC and BASE_HEADER are captured original target metadata. The program binds the header to the base TOC's source-derived chunk ID, length and digest. It never opens the large base UCAS. Acquiring current target metadata remains an embedding application's responsibility.

Create two different, empty, owner-private directories with mode `0700`. Inputs are opened read-only. Archive logical paths never become filesystem destinations. Intake scans every member, derives group roles, replays into generated ordinal filenames and returns an owning journal. The caller binds those file identities, probes small companion files and hands the selected TOC/UCAS descriptors to `mb_iostore_original_file_job_v1`. It emits successful JSON only after the job and descriptor cleanup complete. Output stays local and unpublished.

The selector admits exactly one complete IoStore group, including its optional verified empty v11 PAK and up to 16 verified plaintext README files. Multiple groups require selection. A nonempty PAK, unknown runtime file, missing companion or unsupported layout stops preparation; these files are never silently dropped. An empty PAK is omitted only from this existing-container overlay operation. This is not a rule for every mounting method. See [archive selection](../../../archive/README.md#iostore-archive-selection).

The job creates five fixed artifacts: `generated-sidecar.ucas`, `generated-small.utoc`, `merged-container-header.bin`, `generated-directory.bin` and `generated-final.utoc`. Existing files are refused. Failed partial artifacts remain unpublished; the caller owns their recovery. This example limits intake to 512 rows, 256 KiB names, 512 MiB raw/member data and 1 GiB expanded data. Its allocation limit and admitted IoStore/header formats follow [portable preparation](../../portable_preparation.md). Libarchive's internal allocations are outside the job's allocation accounting.

## Ordinary host build

Use the retained host libarchive, LZMA, zlib and Mbed TLS libraries. The Mbed TLS build must include SHA-1 and SHA-256. This example does not require or invoke a console SDK. Set `NATIVE` to `outputs/ps5-mod-bridge/native-host`, `ARCHIVE_INCLUDE` to the libarchive header directory, `MBEDTLS_INCLUDE` to the Mbed TLS header directory, `CRYPTO_CONFIG` to the directory containing the matching `crypto_config.h`, and the four library variables to their host static archives.

```sh
clang -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Wno-unused-function \
  -D_DARWIN_C_SOURCE=1 -DMB_ARCHIVE_BUILTIN_XZ=1 \
  '-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"' \
  -DBLAKE3_NO_SSE2 -DBLAKE3_NO_SSE41 -DBLAKE3_NO_AVX2 \
  -DBLAKE3_NO_AVX512 -DBLAKE3_USE_NEON=0 \
  -I"$NATIVE/archive" -I"$NATIVE/pak" -I"$NATIVE/iostore" \
  -I"$NATIVE/iostore/vendor/blake3" \
  -I"$ARCHIVE_INCLUDE" -I"$MBEDTLS_INCLUDE" -I"$CRYPTO_CONFIG" \
  "$NATIVE/iostore/examples/archive_input/archive_job.c" \
  "$NATIVE/archive/archive_selection.c" "$NATIVE/archive/intake.c" \
  "$NATIVE/archive/groups.c" "$NATIVE/archive/private/posix_compat.c" \
  "$NATIVE/pak/pak_reader.c" "$NATIVE/pak/pak_primary_capture.c" \
  "$NATIVE/pak/pak_secondary_integrity.c" \
  "$NATIVE/iostore/original_file_job.c" "$NATIVE/iostore/none_range_reader.c" \
  "$NATIVE/iostore/container_header_merge.c" "$NATIVE/iostore/none_packer.c" \
  "$NATIVE/iostore/directory_merge.c" "$NATIVE/iostore/toc_overlay.c" \
  "$NATIVE/iostore/toc_reader.c" \
  "$NATIVE/iostore/vendor/blake3/blake3.c" \
  "$NATIVE/iostore/vendor/blake3/blake3_dispatch.c" \
  "$NATIVE/iostore/vendor/blake3/blake3_portable.c" \
  "$LIBARCHIVE" "$LIBLZMA" "$ZLIB" "$MBEDCRYPTO" -o archive_job
```

## Retained result

The unchanged 105,647,244-byte Leon ZIP completed this route in **3.949 seconds on the Mac**. The classifier found four regular members: TOC, UCAS, an empty 347-byte PAK and a 4,927-byte README. Both companion probes passed. The job verified 161 source asset digests, emitted 162 chunks, and produced a 187,713,168-byte sidecar and 29,870,724-byte final TOC. All five artifacts matched the earlier source-order original-input build in length and SHA-256, compared only after preparation. The earlier build separately passed independent logical-content comparison.

The first integrated run stopped because the harness created quarantine with mode `0755`. No extracted or prepared artifacts were created. Changing new directories to mode `0700` and checking that requirement in the caller resolved this integration mistake. The failed receipt is preserved; no format check was relaxed.

Private evidence is retained in `.superpowers/native-dependency-spike/iostore-archive-composition-1/trial-2/receipt.json`; the earlier failure is in `trial-1`. Reusable-job lifetime controls passed 55 assertions; selector controls passed 17. Test metadata in `archive/selection_tests/actual_inventory_fixture.h` describes a retained input only and is not linked into the selector or preparation caller. No captured game/mod assets are committed.

This establishes automatic local file preparation for the admitted format class. It does not establish PS5 execution, game publication, shader recooking, UE4SS, Windows DLL translation or compatibility across games. The user's earlier camera/Leon observations remain separate in-game evidence. The INI trial's intended visual effect remains unconfirmed.

## Multiple selected downloads

`batch_archive_job.c` performs the same intake and preparation for one through eight original archives. Each archive must contain exactly one admitted IoStore group. It retains each source's bound TOC/UCAS descriptors, then invokes `mb_iostore_batch_file_job_v1` once to produce a shared sidecar and final index. Overlapping incoming chunk IDs, package IDs or exact virtual paths are reported as conflicts; the caller never silently chooses a winner.

```text
batch_archive_job BASE_TOC BASE_HEADER BASE_UCAS_BYTES CONTEXT_TOKEN PRIVATE_OUTPUT \
  ARCHIVE SHA256 BYTES PRIVATE_QUARANTINE [ARCHIVE SHA256 BYTES PRIVATE_QUARANTINE ...]
```

Create a distinct empty `0700` quarantine directory for each archive and another for output. The caller reuses inventory workspace after copying each archive's selection, materialization journal and identity observations into retained state. Noncontainer probe descriptors close before moving to the next archive. It retains the original archive and base metadata snapshots through completion. Per-archive limits match the single caller; cumulative intake limits are 16 GiB I/O, one million operations and 128 MiB path work. The shared job's format and allocation limits still apply. Libarchive allocations are outside the job's accounting.

Build with the command above, replacing `archive_job.c` with `batch_archive_job.c` and the executable name with `batch_archive_job`. This ordinary host example does not invoke a console SDK, upload files or publish output.

The unchanged Leon and movement-speed ZIPs completed together in **3.685 seconds on the Mac**. Automatic selection retained four regular members from each archive, including the movement archive's nested directories. Preparation verified 162 asset digests and emitted 163 chunks, a 187,714,978-byte sidecar and a 29,870,736-byte final TOC. No per-mod member names, extracted files, translated chunks or expected outputs were supplied to this caller. The private composition receipt is `iostore-batch-archive-composition-1/trial-1/receipt.json` beneath the retained dependency-spike directory. Preparation is distinct from the combined bundle's pending in-game test.

An independent comparison verified all 162 incoming assets (185,442,370 logical bytes), the merged header's 38,500 package entries, 56,993 virtual paths, 59,573 final chunk IDs and 59,410 unaffected base rows. It computed the prepared sidecar's full SHA-256 during the same streaming pass and checked base block preservation. The original 128 GB UCAS was never opened. The exact archive output is bound by `iostore-batch-oracle-1/archive-oracle-result.json`; expected data was used only after preparation. This verifies byte preservation and metadata composition, not cross-platform material or script execution.
