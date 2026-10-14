# Durable original download store

`store.h` is a C ABI with an opaque context. The owner supplies an existing owner-only `0700` directory fd and keeps it open until the context is freed. Calls are serialized by the owner. Free releases memory only. An optional canonical absolute root must identify that exact directory; lookup checks it again before returning a native archive path.

Publish accepts a read-only regular source descriptor, game domain, positive mod/file IDs, expected length (at most 1 GiB), and lowercase SHA-256. It derives `dl-<SHA256(game + newline + mod + newline + file + newline + archive hash)>`, copies the source into a fresh exclusive owner-only `0600` staging file, hashes the copy in that single streaming pass, and exclusively links the finished archive, checks source stability and bytes/hash, fsyncs the archive and directory, and then publishes fsynced metadata by an exclusive link. It never links the source archive. The metadata contains identifiers, hash, byte count, version, and archive filesystem fingerprints; no account data, URLs, credentials, or caller paths.

The metadata link is the commit point. A complete archive left before commit is reusable only after its ownership, permissions, link count, length and hash pass. Incomplete or conflicting archives cause an integrity failure and are left untouched. Interrupted partial copies remain only under a unique staging name, so a retry uses a fresh name and can finish. If a crash leaves a fully published archive plus its staging hardlink, recovery requires full byte/hash verification and a matching generated staging pathname with the same device/inode before unlinking that one staging link. No unrelated final pathname is replaced. Temporary metadata uses an exclusive generated name; abandoned temporary metadata is ignored and never broadly cleaned. If a crash leaves the committed metadata and its temporary hardlink, its maximum link count of two is accepted. Archives always require one link. A failure can leave durable files for a later retry; free never removes them.

Public publish output is a single record with `download_id`, `state: "downloaded"`, `game`, `mod_id`, `file_id`, `sha256`, `bytes`, and `retained: true`. List emits `{ "items": [...] }` and scans at most 256 metadata records. Public output omits filesystem fingerprints. Lookup adds `archive_path` for a trusted native caller; this response must never be sent to an HTTP/browser client.

Polling list checks committed metadata and archive regular-file type, owner, mode, single link, device/inode, byte count and nanosecond mtime/ctime. It does not read archive contents. Missing, truncated, symlinked or replaced archives are omitted. Mutation with unchanged complete filesystem fingerprints can escape a list check; trusted lookup hashes the complete archive before returning a usable path. Publish also hashes complete bytes. Reopen reconstructs state from metadata without keeping a RAM-only catalog.

All JSON responses are bounded to 256 KiB and NUL terminated; error output is empty. Return codes are declared in the header. Output capacity is checked before archive publication. The directory is private to this app; protection against a malicious process with the same user and directory access is outside this API's contract.

`mb_download_store_publish_metadata(store, download_id, metadata, metadata_bytes)`
retains an authenticated native metadata capture for later service registration.
The caller supplies exact JSON object bytes, at most
`MB_DOWNLOAD_STORE_METADATA_LIMIT` (128 KiB). The store validates complete UTF-8
JSON, a root object, unique decoded keys, depth at most 64, and at most 200,000
parser events. Raw NUL bytes are rejected. Source/requirements semantics remain
the registration adapter's responsibility. This API is for the native metadata
owner, never direct browser submission.

The immutable `<download_id>.metadata.json` sidecar contains the original bytes,
their SHA-256, the download ID, and a SHA-256 binding to the existing archive's
complete 15-field manifest. Its wire envelope is bounded to 769 KiB. The capture
uses a fresh exclusive `0600` staging file, file fsync, exclusive atomic final
link, staging unlink, and directory fsync. Existing byte-identical captures are
a no-op; changed captures return `MB_DOWNLOAD_STORE_INTEGRITY_FAILED` without
overwriting the original. Matching retries still sync the directory, completing
any earlier failed post-link sync before reporting success. An interrupted committed sidecar with its staging
hardlink remains readable (one or two links are accepted). Abandoned staging
files are ignored; store close never deletes captures.

`mb_download_store_lookup_metadata(store, download_id, output, capacity)` returns
only the exact capture plus a NUL terminator. A missing capture returns
`MB_DOWNLOAD_STORE_NOT_FOUND` and empty output; it never creates an empty or
complete requirements object. A small output returns
`MB_DOWNLOAD_STORE_OUTPUT_TOO_SMALL` and empty output. Invalid input returns
`MB_DOWNLOAD_STORE_BAD_ARGUMENT`; malformed, tampered, or mismatched captures
return `MB_DOWNLOAD_STORE_INTEGRITY_FAILED`. I/O failures can return
`MB_DOWNLOAD_STORE_STORAGE_FAILED`. Both APIs validate the committed archive
manifest and current inode/size/timestamp fingerprint without rehashing archive
contents. Final trusted archive lookup at registration still performs the full
hash. Sidecar reads additionally check their own bounded bytes, hash, type,
owner, permissions, link count, and stable open/named identity. Sidecars remain
bound to the original archive identity even after close/reopen. Public lists
and archive manifests keep their existing fields and expose no capture bytes.

Host test (from repository root):

```sh
cc -c outputs/ps5-mod-bridge/native-host/archive/private/posix_compat.c -o /tmp/mb-store-posix.o
c++ -std=c++17 -I outputs/ps5-mod-bridge/third_party outputs/ps5-mod-bridge/native-host/download_store/store.cpp outputs/ps5-mod-bridge/native-host/download_store/tests/store_test.cpp /tmp/mb-store-posix.o -o /tmp/mb-store-test
/tmp/mb-store-test
```

Focused directory-fsync failure/retry control (uses only owned local fixtures):

```sh
c++ -std=c++17 -I outputs/ps5-mod-bridge/third_party -Dfsync=mb_download_store_test_fsync -c outputs/ps5-mod-bridge/native-host/download_store/store.cpp -o /tmp/mb-store-sync.o
c++ -std=c++17 -I outputs/ps5-mod-bridge/third_party -DMB_DOWNLOAD_STORE_SYNC_TEST outputs/ps5-mod-bridge/native-host/download_store/tests/store_test.cpp /tmp/mb-store-sync.o /tmp/mb-store-posix.o -o /tmp/mb-store-sync-test
/tmp/mb-store-sync-test
```

This fixture fails the first post-link directory sync and the next exact replay,
then requires a successful retry. It also creates a matching final sidecar
between the missing check and exclusive link, verifies the `EEXIST` branch
propagates a directory-sync failure, and checks the next retry succeeds. The
interposition is compiled into the fixture only; the production API has no
test callbacks.
