# Native archive intake and grouping

These C11 modules prepare downloaded archives without executing their contents or writing to a game. They are reusable preparation components; they are not a finished Nexus downloader, asset converter or game installer.

`intake.h` exposes a full scan of a caller-owned read-only regular file and selected-member materialization into an empty private directory. Scan publishes owning rows, member hashes and logical names only after complete parsing, exact lengths, raw-source SHA-256/EOF checks and source identity checks. Materialization replays the entire archive, binds the inventory and caller context token, writes flat ordinal filenames with exclusive/no-follow creation, synchronizes and reads back every output, then publishes an owning binary manifest. Archive paths are metadata and never filesystem destinations. The context token does not establish game identity or compatibility.

Manifest publication normally uses a no-replace hard link. If the filesystem returns `EOPNOTSUPP`/`ENOTSUP`, the library exclusively creates the final file and emits, syncs and reads back the same canonical manifest before removing its temporary file. That fallback preserves no-overwrite and independently observed ownership, but the final filename can be visible with incomplete contents. Consumers must use the completed owning result as the success signal; filename existence alone is insufficient.

`prepared.h` derives all complete container selections and collision-free bundle names from owning inventory/group output and profile conventions, then binds exact materialization results to owning file records. Both same-inode hard-link and independently created final manifests are admitted only with complete matching device, length and hash facts. These remain preparation candidates without platform or runtime authority.

The caller retains the source and directory descriptors, independent replay storage, one cumulative I/O/operation ledger and a fresh ownership journal. Failed writes clean up only matching owned file identities. Cleanup uncertainty retains the recovery intent and journal; it must not be reported as success. The embedding caller owns successful quarantine cleanup. Caller limits may tighten the declared ceilings. Libarchive allocation, LZMA dictionary memory and internal CPU work are not hard bounded by those ceilings; cancellation is cooperative.

`groups.h` groups complete inventory rows by normalized parent and exact filename stem, with ASCII folding of the `.pak`, `.utoc` and `.ucas` extensions. It preserves original row indices and copies group keys into caller storage. Missing companions, case ambiguity and partition-shaped UCAS sets remain unresolved; duplicate roles fail the operation. These are metadata candidates, with format validation, installation and runtime flags false.

The explicit reader registry admits ZIP, tar, 7zip, RAR/RAR5 and selected built-in filters. Registration is not a compatibility claim. Reader warnings fail closed, including unavailable pathname conversion. Unicode names are reader-reported UTF-8; the pinned Darwin reader normalizes names to NFD. Native locale and individual codec behavior require target evidence.

## Verification

Use a fresh output directory:

```sh
python3 outputs/ps5-mod-bridge/native-host/archive/verify.py --phase green --output <fresh-directory>
python3 outputs/ps5-mod-bridge/native-host/archive/groups_verify.py --phase host --output-dir <fresh-directory>
```

The retained private corpus consists of five unchanged Nexus ZIP downloads. Host materialization matches an independent streaming ZIP oracle and serialized manifests for all five, plus plain tar and a Unicode fixture. The focused failure suite passed 312 checks across 31 cases. Grouping passed 252 checks across 14 cases and matched the existing host analyzer for all five archives.

The root composition experiment uses native grouping to select complete container members, or regular files when there are no container groups. Independent oracle selections are assertions only. All five archives passed, including a streamed 185,446,608-byte UCAS. The automatically selected IoStore roles fed the shared container/package readers and yielded 127 structurally readable package summaries. Original downloads remained unchanged. Scripts and Windows DLLs were treated as bytes and never executed.

The native verifier uses pinned SDK, codec and crypto inputs. A caller-local SDK linker-script repair prevents allocated orphan sections from enlarging `PT_DYNAMIC`; validation compares its complete geometry with `SHT_DYNAMIC`. Compiler/linker success or this structural check does not establish execution, resolved target imports, target filesystem behavior or gameplay compatibility. Do not dispatch older malformed experiment ELFs.

An unchanged 1,435-byte stamina ZIP now completes scan and extraction on the PS5 in private quarantine. Independent FTP readback matches both original members and its manifest; no Lua or game file was executed. The direct console hard-link diagnostic returned unsupported operation, and the corrected fallback completed with an independently observed final inode. Focused fallback tests passed 61 checks across three cases; prepared binding passed 163 checks across 21 cases and the real unchanged Leon bundle on the host.

See [the current archive checkpoint](../../../2026-10-01-native-archive-pipeline.md) for the measured boundary and target-test result.

## Request-driven standalone caller

`request.h`/`request.c` provide a bounded, canonical request/context decoder;
`request_caller.c` is a separate C `main` that invokes the archive APIs above.
It is not part of the status host, HTTP server, default `build.py` profiles, or
the Makefile. It processes one request and exits. Its only compiled input path
is `/data/ModbridgeInbox`; archive identity and naming-profile values arrive
in request files, not per-mod compile-time definitions.

`request.v1` is ASCII, LF-only, ordered, and at most 512 bytes. Its derived
claim, quarantine, and receipt names must match the request ID exactly:

```
MBARREQ1
id=0123456789abcdef0123456789abcdef
source=archive.zip
context=profile.ctx
context_sha256=1a70c4c31db986a7d02967614f4860cdabf4fe85ebe9cae36d4b98d19c6d06e9
quarantine=q-0123456789abcdef0123456789abcdef
receipt=r-0123456789abcdef0123456789abcdef.json
end
```

The exact bytes of `profile.ctx` are SHA-256-bound by that request field:

```
MBARCTX1
source_bytes=1
source_sha256=2d711642b726b04401627ca9fbac32f5c8530fb1903cc4db02258717921a4881
target_token=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
prefix=modbridge
suffix=_P
end
```

This pair illustrates the canonical wire shape; the one-byte source is not a
valid archive. For a real request, write the source archive first, then put its
exact byte count and SHA-256 in the context, hash the complete LF-terminated
context, and place that context hash in the request. The caller checks bounded
regular-file reads, source identity, and exclusive derived names. The inbox
must be owned by the caller's effective user. If an ordinary `MKD` leaves it
at `0755`, the caller restricts the opened directory to `0700`, syncs it, and
rechecks ownership and mode before reading requests.

A standalone native request caller uses the same pinned archive, zlib, and
crypto archives as the archive verifier; it does not change the default host
build. Starting from a fresh output directory, generate the reviewed
SDK-v0.43 caller linker script and link these source files as one separate
artifact:

```sh
ROOT=/path/to/translation-core/what
OUT="$ROOT/.superpowers/native-dependency-spike/native-request-caller-build"
SDK="$ROOT/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk"
LLVM_BIN="$ROOT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin"
RESOURCE="$ROOT/.superpowers/native-dependency-spike/tools/resource-root/LLVM-21.1.8-macOS-ARM64/lib/clang/21"
NATIVE_CRYPTO="$ROOT/outputs/ps5-mod-bridge/native-host/build/crypto/task1-fix1-evidence/artifacts/native-038ce86825317d4d74716405"
MBEDTLS="$NATIVE_CRYPTO/sources/mbedtls/mbedtls-068ff080b369adfac81509f9b57b2afabaf82dc5"
CRYPTO_CONFIG="$NATIVE_CRYPTO/config"
LIBARCHIVE_SOURCE="$ROOT/.superpowers/native-dependency-spike/libarchive-release/libarchive"
LIBARCHIVE_NATIVE="$ROOT/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/native/libarchive-build/.libs/libarchive.a"
LIBLZMA_NATIVE="$ROOT/.superpowers/native-dependency-spike/native-libarchive-lzma-experiment-1/native/xz-build/src/liblzma/.libs/liblzma.a"
ZLIB_NATIVE="$ROOT/.superpowers/native-dependency-spike/native-zip-experiment-1/native/zlib-build/libz.a"
MBEDCRYPTO_NATIVE="$NATIVE_CRYPTO/libmbedcrypto.a"
mkdir -p "$OUT"

python3 -c 'import sys; from pathlib import Path; sys.path.insert(0, sys.argv[1]); from link_layout import write_linker_script; write_linker_script(Path(sys.argv[2]), Path(sys.argv[3]))' \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive" "$SDK" "$OUT"

env -u CPATH -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH -u SDKROOT \
  LLVM_CONFIG="$LLVM_BIN/llvm-config" \
  "$SDK/bin/prospero-clang" -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -DMB_ARCHIVE_BUILTIN_XZ=1 '-DMBEDTLS_USER_CONFIG_FILE="crypto_config.h"' \
  -resource-dir "$RESOURCE" \
  -I"$ROOT/outputs/ps5-mod-bridge/native-host/archive" \
  -I"$MBEDTLS/include" -I"$CRYPTO_CONFIG" \
  -I"$LIBARCHIVE_SOURCE" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive/request_caller.c" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive/request.c" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive/intake.c" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive/private/posix_compat.c" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive/groups.c" \
  "$ROOT/outputs/ps5-mod-bridge/native-host/archive/prepared.c" \
  "$LIBARCHIVE_NATIVE" "$LIBLZMA_NATIVE" "$ZLIB_NATIVE" "$MBEDCRYPTO_NATIVE" \
  -Wl,--no-undefined,-z,now -Wl,-T,"$OUT/elf_x86_64.dynamic-only.x" \
  -o "$OUT/request-caller.elf"
```

Resolve the library, SDK, resource, and LLVM variables from the retained
verified build inputs; do not substitute host headers or libraries. The
request-caller ELF was target-tested with two different requests: the stamina
archive returned `no_container_candidate` after scanning four rows, and the
Leon archive prepared one group with three members. That establishes request
reuse and bounded quarantine preparation for those inputs only. It does not
download from Nexus, alter game paths, execute mod contents, or prove game
mounting, platform compatibility, installation, or gameplay.
