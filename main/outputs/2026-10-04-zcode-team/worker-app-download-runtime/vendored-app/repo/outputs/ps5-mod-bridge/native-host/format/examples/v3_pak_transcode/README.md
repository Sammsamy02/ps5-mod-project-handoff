# Stream a classic v3 PAK into v11

`transcode.c` is an ordinary host caller for the shared v3 reader and v11
streaming writer. It derives the mount, ordered paths, extents and expected
SHA1s from the source archive. No game or mod names are built into it.

```sh
transcode original.pak fresh-output.pak path-hash-seed [max-output-bytes]
```

The seed is an explicit decimal or hexadecimal integer because v3 has no
v11 path-hash seed. The default output limit is 512 MiB. Input must use the
reader's unencrypted, uncompressed v3 profile. Output is unsigned,
unencrypted v11 with uncompressed entries; the writer admits printable
ASCII names. Other layouts are refused without silently dropping entries.

The caller reads source metadata before exclusively creating the output.
The writer streams every payload and verifies its source SHA1, using 64 KiB
transfers. Source paths remain archive metadata and are never used as file
destinations. Existing output files are preserved. Failed output remains a
private, incomplete artifact; `artifact_complete` is false. Finalization
checks source stability, output identity and size, flushes the output and
closes both files. The caller does not merge a base archive, install a mod,
recook assets, or establish game compatibility.

Other bounds are 8192 files, 4 MiB reader index/name storage, 8 MiB writer
index storage, 2 GiB of attempted I/O and 131072 system-call attempts.
Source snapshot checks are optimistic filesystem checks, not a lock against
other writers. Use a private source/output directory. Directory durability
and later installation remain the embedding application's responsibility.

From `outputs/ps5-mod-bridge`, provide the already acquired Mbed TLS source
identified by `native-host/pak/dependency-lock.json`:

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -D_DARWIN_C_SOURCE '-DMBEDTLS_CONFIG_FILE="pak_mbedtls_config.h"' \
  -I native-host/pak -I native-host/format \
  -I "$PAK_MBEDTLS_SOURCE/include" -I "$PAK_MBEDTLS_SOURCE/library" \
  native-host/format/examples/v3_pak_transcode/transcode.c \
  native-host/pak/pak_v3_none_reader.c \
  native-host/format/pak_stream_writer.c \
  "$PAK_MBEDTLS_SOURCE/library/sha1.c" \
  "$PAK_MBEDTLS_SOURCE/library/platform_util.c" -o /tmp/pak-v3-transcode
```

The Darwin feature flag exposes `O_NOFOLLOW`; the Apple snapshot branch
uses `st_mtimespec` and `st_ctimespec`. The ordinary host build was tested
on macOS. No PS5 build or execution is claimed.

A separately acquired 5849-byte v3 mod PAK produced a 6119-byte v11 PAK.
Pinned independent repak reads found the same mount, the same single path
and byte-identical 5643-byte contents. A second invocation refused the
existing output without changing it. The private receipt and oracle are
in `pak-v3-original-transcode-1/trial-1` beneath the retained dependency-spike
directory. Mod content is not included in this example.
