# Bounded host PAK union preparation

`merge BASE_V11_PAK MOD_V3_PAK FRESH_OUTPUT`

This standalone ordinary host caller prepares a fresh unsigned, unencrypted
v11 None PAK from one v11 None base and one classic v3 None mod PAK. It does
not install files, access a console, interpret configuration settings, or
claim game/runtime compatibility. The three filesystem names are caller
inputs. Archive mount and entry names are metadata and never filesystem
destinations. No member hash, target entry, game name, mount, or hash seed is
supplied as a preparation recipe.

## Union policy

The original base supplies the output mount and path-hash seed. Mounts must
have identical leading `../` depth. After those components, the mod's ordinary
directory mount must be within the base's ordinary directory mount, with an
exact byte prefix ending at a directory boundary. The remainder of the mod
mount is joined with each mod entry to obtain a base-relative path. Absolute
mounts, dot components after the leading parents, and unsafe/non-ASCII names
are refused. A mod may use the same mount as the base.

An exact path replaces the whole base payload; a missing path is added. This
is ordinary PAK replacement semantics, with no scalar INI merge or platform
path substitution. Exact duplicate paths within either input and case-only
file aliases are refused by the caller's bounded sorted inventory. The
stream writer refuses case-only directory aliases, file/ancestor conflicts,
path-hash collisions, and invalid output geometry before its first callback.
Base row order is retained; replacements occupy the corresponding base row,
and additions follow the mod's source row order. New indexes are rebuilt by
the existing writer; unused empty directory records are not preserved.

## Source admission and copying

Inputs are opened once with `O_RDONLY | O_NOFOLLOW | O_CLOEXEC`, required to
be distinct regular-file inodes, and retained until cleanup. Source callbacks
check size, device/inode, and nanosecond modification/change times before and
after reads; both snapshots are also checked around output/finalization.
These observations detect ordinary concurrent changes. As with the existing
reader contract, the caller must ensure the files are immutable during the
job; a read-only FD alone does not seal other processes out of the file.

Base primary capture validates the v11 footer and primary SHA1. Both secondary
spans have their declared SHA1 checked. The classic directory parser plus the
complete reference graph validates true compact-record starts, one-to-one
references, and counts. Every indexed physical extent must lie before the
first metadata span; one adjacent pass over the graph's sorted extents rejects
overlap. Each 53-byte None physical header is parsed and reconciled with its
graph-certified compact record. The v3 reader independently validates its
flat index, physical headers and extent geometry in metadata-only mode.

The existing candidate reconciler has a deliberate 4 MiB None/Zlib candidate
cap. It is unchanged. A separate caller-local None-only reconciliation gate
checks matching method, header, sizes, stored offset (zero or the real entry
offset), flags, block count, zero physical block size, and bounds under the
explicit 512 MiB streamed payload profile. No compressed codec is admitted.

The writer reads the chosen payloads directly through a row's source ordinal
and archive data offset. It checks each chosen payload's physical-header SHA1
during its single streaming copy. An overridden base payload is not copied
or payload-hashed; its metadata/header was still admitted. Diagnostics count
completed chosen payload checks only after the writer completes. The caller
does not read whole assets into memory or launch an external packer.

## Limits and output ownership

- Each source PAK: at most 512 MiB; each source inventory: 8,192 files.
- Final inventory: at most 8,192 files, 16,384 directories, 1,024-byte paths;
  final payload/output: at most 512 MiB, enforced by the writer.
- Primary, directory, and v3 index/name buffers: at most 4 MiB each; combined
  rebased input paths: 8 MiB; final selected names: at most 4 MiB; writer index:
  8 MiB; payload transfer buffer: exactly 64 KiB.
- At most 32 owned allocations and 64 MiB live requested allocation bytes;
  every owned block is freed on every exit.
- Combined positional read/write requests: 2 GiB and 131,072 syscall attempts;
  each attempt is at most 64 KiB. EINTR/short-read retries are charged. Child
  reader/writer and reference-graph limits remain in force as well.
- Folded path comparisons: at most 256 MiB of compared bytes, using a stable
  bounded O(n log n) sort of at most 16,384 input paths. Exact adjacent byte
  comparisons additionally span at most the bounded input names.

Output is created with `O_EXCL | O_NOFOLLOW`, mode 0600, and checked to be a
distinct empty regular-file inode. This caller never overwrites, unlinks,
renames, or truncates input/output paths. Successful finalization requires
writer completion, exact output FD/path identity and size, fsync, close, and
stable final input snapshots. A failed creation does not alter an existing
path; a failed operation after creation leaves the partial output unpublished
for the caller to inspect/discard. `artifact_complete` is true only on final
success. Independent archive readback/comparison belongs to the root's test
receipt and is not claimed by this program.

## Build and verification

From `outputs/ps5-mod-bridge`, provide the already acquired Mbed TLS source
identified by `native-host/pak/dependency-lock.json`:

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  '-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"' \
  -I native-host/pak -I native-host/format \
  -I "$PAK_MBEDTLS_SOURCE/include" -I "$PAK_MBEDTLS_SOURCE/library" \
  native-host/format/examples/pak_mod_union/merge.c \
  native-host/format/examples/pak_mod_union/prepare_union.c \
  native-host/pak/pak_reader.c native-host/pak/pak_primary_capture.c \
  native-host/pak/pak_secondary_integrity.c native-host/pak/pak_directory_parser.c \
  native-host/pak/pak_reference_graph.c native-host/pak/pak_entry_decoder.c \
  native-host/pak/pak_physical_header.c native-host/pak/pak_v3_none_reader.c \
  native-host/format/pak_stream_writer.c \
  "$PAK_MBEDTLS_SOURCE/library/sha1.c" \
  "$PAK_MBEDTLS_SOURCE/library/platform_util.c" -o /tmp/pak-mod-union
```

On macOS the caller enables `_DARWIN_C_SOURCE` and uses `st_mtimespec` and
`st_ctimespec`. The host build passes strict C11 warnings. No target build
or console execution is implied.

The focused authored-fixture driver needs the separately built
[v3 transcoder](../v3_pak_transcode/README.md), pinned repak and a fresh local
directory. It generates its own tiny source PAKs; no game assets are included:

```sh
python3 native-host/format/examples/pak_mod_union/controls.py \
  /tmp/pak-mod-union /tmp/pak-v3-transcode /absolute/path/to/repak \
  /absolute/path/to/fresh-controls-directory
```

Checks cover exact replacement plus addition, preserved hash seed, rejection
of different mount depths, case-only file and directory aliases, file/ancestor
conflicts, corrupt payloads, existing-output preservation and unchanged inputs.
All passed on the tested sources.

A saved 297,572,462-byte game PAK and a separately downloaded 5849-byte v3 PAK
produced a fresh 297,439,679-byte archive: one mod file replaced its matching
base path and all 4698 other base files were preserved. The same caller found
the mount, paths and seed from source metadata without game/mod-specific code.
An independent repak `hash-list` read and hashed decoded contents from all
three archives. The full path-and-content union matched with zero differences.
The run took about 0.8 seconds on the development Mac, requesting 37,037,502
bytes of peak job allocation and releasing all 27 owned blocks. This is a
host preparation measurement, not a PS5 benchmark or gameplay result.

For an independently decoded comparison of another admitted pair:

```sh
python3 native-host/format/examples/pak_mod_union/oracle.py \
  /absolute/path/to/repak base.pak mod.pak combined.pak fresh-receipt.json
```

The oracle preserves full virtual mount/path strings, including leading
`../` components. They are engine metadata, never extraction destinations.
It requires declared entry counts to match unique decoded paths, then compares
the base map overridden/extended by the mod with the output map. Receipt
files are created exclusively. Private retained evidence is in
`pak-base-mod-union-1/trial-1` and `controls-run-1` beneath the dependency-spike
directory. The direct PAK example is also used by the raw archive caller
below. Application UI, console publication and a game runtime remain separate.

## Borrowed-descriptor entry point

`prepare_union.h` exposes `mb_pak_union_prepare_v1` for a caller that already
owns the input and output descriptors. It uses the same preparation code as
the CLI. Parameters declare the two source sizes and the caller's immutable
input/private output obligations. Sources must be distinct regular read-only
files; the output must be a distinct empty writable nonappend file.

The function preserves all descriptors and their file positions, releases
its temporary allocations, and returns owning diagnostics. It does not open
paths, close descriptors, synchronize storage, print, install, or publish.
`archive_complete` means preparation succeeded; the caller still owns output
finalization. Failure may leave private partial output. The CLI retains the
previous arguments and JSON keys, including `artifact_complete`, which also
requires its finalization checks. An invalid source may now leave an empty
exclusively created output, because the core requires an output FD at entry.

Build `fd_controls.c` in place of `merge.c` with the same remaining source
list to exercise descriptor ownership, cursor preservation and refusal of
writable sources, mismatched lengths, append or nonempty output. Supply tiny
authored base/mod fixtures from `controls.py` and a fresh output directory.

## Original archive intake

```text
archive_merge BASE_PAK ORIGINAL_ARCHIVE ARCHIVE_BYTES ARCHIVE_SHA256 TARGET_TOKEN_HEX FRESH_0700_QUARANTINE FRESH_OUTPUT [--pak-component-only]
```

[`pak_archive_job.h`](../../pak_archive_job.h) exposes
`mb_pak_archive_job_prepare_v1`, implemented in `pak_archive_job.c`, to compose
intake, the shared [`pak_selection.h`](../../../archive/pak_selection.h)
selector, materialization and the borrowed-FD union in the caller's process.
`archive_merge.c` supplies CLI arguments, descriptor acquisition, JSON reporting
and final output durability/path checks. The caller supplies the original
download's identity and a target token, but no archive member filename,
ordinal, size, hash, mount, game name or translation recipe.

The job borrows read-only source/base descriptors and a caller-exclusive,
initially empty owned mode-0700 quarantine directory. It accepts either a
borrowed empty private output descriptor or `output_fd=-1` with a trusted
synchronous `acquire_output(context, &fd)` callback. The callback runs at most
once, only after selection is ready, every regular member is materialized,
and the selected PAK is opened and pinned to its journal inode. Default-policy
extra-member refusal invokes no callback. Any acquired output descriptor
remains caller-owned even if acquisition or preparation subsequently fails.
The CLI uses this callback to create its output path exclusively at that point.

The selector validates the complete intake inventory and requires exactly
one resolved standalone PAK. Multiple PAK choices and unresolved container
groups require resolution; a PAK accompanying an incomplete IoStore set is
never silently treated as standalone. Every regular member is enumerated.
Any extra regular file, including a README, requires additional action by
default, without creating output or quarantine files. No optionality is
inferred from filenames or prose.

The explicit generic `--pak-component-only` policy prepares the one PAK
component and retains all other regular members as unhandled. It never
reports the whole bundle as handled when extras exist. All regular members
are materialized under intake-generated ordinal names, with size/SHA256 and
source/inventory/target identities retained. The chosen PAK is pinned to its
journal inode and passed directly to the core. Archive names are JSON
metadata, not filesystem destinations. The report includes recovery journal
entries on failure; partial output and surviving owned files stay local and
unpublished. Installation and runtime flags remain false even on success.

The job has no global mutable job state: parameters, workspace and result are
separate caller-owned spans, supporting reentrant calls with independent
storage, descriptors and callback contexts. The CLI owns one static workspace;
the library owns no static inventory buffers. A fresh all-zero journal is
required. Available inventory/member diagnostics and any recovery journal
remain in caller storage after an admitted refusal; the caller owns
recovery before reuse. The job closes only its selected-member and temporary
directory-iteration descriptors. Borrowed file positions are preserved;
the quarantine directory cursor is unspecified. It performs no stdout, exit,
subprocess, output fsync or publication. Materialization retains its existing
private-journal synchronization. `preparation_complete` means preparation only;
the CLI's separate `artifact_complete` also requires finalization.

The job bounds raw and individual members to 512 MiB, expanded intake to 1 GiB,
512 inventory rows, 256 KiB names, 8 GiB intake I/O, one million intake operations and
16 MiB intake path work. The union's separate limits above still apply.
Libarchive's internal allocation/CPU is not covered by the union allocation
accounting. The job is not wired to the application's download queue or a
native target; installation, runtime and platform compatibility remain unproved.

Build with the core's PAK sources, substituting `archive_merge.c` for
`merge.c`, and add `format/pak_archive_job.c`, `archive/pak_selection.c`,
`archive/intake.c`, `archive/groups.c` and `archive/private/posix_compat.c`. Include the archive
and pinned libarchive headers. Use the existing unified host Mbed TLS build
with SHA1 and SHA256: replace the SHA1-only config and standalone crypto
sources in the earlier recipe with its include/config paths,
`-DMBEDTLS_USER_CONFIG_FILE='"crypto_config.h"'`, and `libmbedcrypto.a`.
Use `-D_DARWIN_C_SOURCE=1 -DMB_ARCHIVE_BUILTIN_XZ=1`; link the retained
`libarchive.a`, `liblzma.a`, `libz.a`, then `libmbedcrypto.a`. The exact cached
host command and dependency identities are in private
`pak-archive-job-1/build.json` beneath the dependency-spike directory.

```sh
python3 native-host/format/examples/pak_mod_union/archive_controls.py \
  /absolute/path/to/archive-merge /path/to/authored-base-v11.pak \
  /path/to/authored-mod-v3.pak /path/to/fresh-controls-directory
```

Five authored archive controls passed after CLI delegation to the job:
automatic discovery under arbitrary wrapper paths, extra-member refusal by
default, explicit component-only
preparation with exact preservation of every member, multiple-PAK ambiguity,
and incomplete IoStore refusal. The retained receipt is
`pak-archive-job-1/archive-controls-1/receipt.json`. Earlier borrowed-FD controls
passed 64 checks, and replacement/addition controls passed after extraction of
the underlying union core.

Build `archive_job_controls.c` in place of `archive_merge.c` with the same
remaining sources and libraries to exercise the job directly. Supply the
authored base and `mixed.zip` generated by `archive_controls.py`, that ZIP's
SHA256, the same target token, and a nonexistent harness directory:

```text
archive_job_controls BASE MIXEDZIP ARCHIVE_SHA256 TARGET_HEX FRESH_DIR
```

The focused job controls passed 10 cases and 270 checks, covering default
extra-member refusal, repeated same-process preparation, borrowed file
descriptor and cursor preservation, output/source refusals and fresh-journal preflight.
Private receipts are `pak-archive-job-1/job-controls-run-2.json` and
`pak-archive-job-1/job-controls-2/receipt.json`.

The original 5,810-byte Nexus ZIP was then fed directly into the caller.
Default policy exposed all three regular members and required additional
action without writes. Component-only mode discovered its 5,849-byte PAK,
retained that PAK plus the 78-byte INI and 1,731-byte README exactly, and
prepared a 297,439,679-byte output in 0.6179 seconds on the Mac. Its full
SHA256 matched the previously independently decoded union with one replacement
and 4,698 preserved files. Original ZIP/base bytes were unchanged. Private
evidence is `pak-archive-job-1/real-trial-1/receipt.json`; this proves this
bounded automatic host preparation path, not the mod's effects or platform
compatibility. No console file was changed.
