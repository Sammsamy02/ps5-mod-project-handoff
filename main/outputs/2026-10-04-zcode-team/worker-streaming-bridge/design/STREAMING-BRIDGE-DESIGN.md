# Streaming download-to-preparation bridge — design

Origin: worker-streaming-bridge, 2026-10-04. Prototype status: the SIMPLER
approach (section 3) is implemented and host-tested in this directory; the
FULL streaming approach (section 4) is a future design, not built here.

## 1. Current state (the gap)

* Download path (`worker-e2e-final/owned/modinst_download.c`): the exclusive
  curl owner streams received bytes into ONE private temp file
  `<intake>/<id>.zip.tmp` through the borrowed file-sink ABI (sequential
  `write_at` + positional readback `read_at`), computes the stream sha256,
  fsyncs, then publishes by rename to `<id>.zip` and writes the sidecar
  record. Failed prefixes are unlinked and stay private.
* Preparation path (`ps5-mod-bridge/native-host/archive/intake.c`):
  `mb_archive_scan_v1` / `mb_archive_materialize_v1` consume an
  `mb_archive_source_v1` — a fresh `O_RDONLY` regular descriptor plus
  declared `size`, `sha256`, `target_token`. The scanner re-verifies
  everything (start stamp, end stamp, full-file re-hash, EOF probe) and
  fails closed with `MB_ARCHIVE_IDENTITY_MISMATCH` / `SOURCE_CHANGED`.
* Gap: nothing connects the two ends. Today the archive is published
  first and only later re-opened by path for prepare. Consequences:
  (a) a failed prepare leaves a published-but-unusable archive in intake;
  (b) the download→prepare latency is fully serialized;
  (c) any naive "feed the scanner while downloading" idea breaks the
  intake contract, which requires a fixed size and an immutable, complete,
  verifiable file.

## 2. Non-negotiable contracts the bridge must respect

1. Intake source contract (from `source_start`/`source_end`): fresh O_RDONLY
   regular fd; `st_size == declared size` for the whole scan; stat stamp
   unchanged start→end; full-file re-read sha256 == declared sha256; EOF
   probe (exactly zero readable bytes at `size`).
2. Transport contract: `mb_curl_download_v1` borrows sink params (fd +
   `write_at`/`read_at`); the transport computes the authoritative stream
   sha256 and reports `bytes_written`/`received_bytes`; there is no
   reset/retry; the owner is exclusive and process-serialized.
3. Failure privacy: an unpublished failed prefix must never become visible
   (unlink, no sidecar, no record).

## 3. Approach A — temp file + threshold handoff (PROTOTYPE, tested)

Implemented as `owned/mb_stream_bridge.{h,c}`. One private temp file, no
second copy of the archive bytes, and a threshold-gated handoff that emits
exactly the `mb_archive_source_v1` inputs:

```
curl owner ──write_at──▶ bridge sink_write ──▶ private .tmp (only copy)
 (sha256 via            (hash engine updated    │
  injected engine)       on the same bytes)     │ stream complete
                                                ▼
                                    handoff gate: fsync → retire download
                                    fds → fresh O_RDONLY fstat identity →
                                    EOF probe → full re-read re-hash ==
                                    sink sha → emit {fd,size,sha256}
                                                │
                     scan/materialize ◀─────────┘   (source fd stays open)
                                                │ prepare OK
                                    publish: rename .tmp → <id>.zip + dir fsync
```

* Threshold semantics: `handoff` refuses (`NOT_READY`) until the transport
  reported completion AND `received_bytes >= threshold_bytes`. In the
  prototype the gate requires completion because the pinned scanner needs a
  fixed total size; the threshold parameter keeps the gate explicit and
  lets a future prefix-scanning consumer open earlier.
* Ordering improvement (tested): because the handoff is fd-based, prepare
  can run on the still-private inode and publish only AFTER prepare
  succeeds. A failed prepare then leaves intake clean; the sidecar record
  is written after the same commit point as today.
* Identity is front-loaded: the bridge performs the exact re-read re-hash
  that intake's `source_end` would perform, so the scanner never sees a
  file that can only fail late with `IDENTITY_MISMATCH`.
* Crypto is injected (same pattern as the transport's injected entropy
  hook). The bridge TU does no hashing itself; host test injects
  CommonCrypto, the console composition injects the pinned MBedTLS sha256
  closure.
* Cost vs today: zero extra copies, one extra full read of the file (the
  pre-handoff re-hash) and intake's own re-reads — identical I/O class to
  what the scanner already charges to the budget, moved earlier.

Integration into `modinst_download.c` (future patch, small): point
`dp.sink.read_at/write_at` at the bridge sink adapters, call
`sink_finish` with `dr.file.sha256`/`dr.file.bytes_written`, then handoff →
prepare → publish → sidecar. The transport ABI is untouched.

## 4. Approach B — full streaming (future design, NOT built)

Goal: begin preparation while bytes are still arriving, without a second
copy, without violating the intake fail-closed contract. The decisive
constraint: `mb_curl`'s owner is exclusive and process-serialized, so any
overlap must be pump-driven on the SAME thread — the transport calls back
into the bridge when bytes arrive; there are no worker threads and no
locks. libarchive must also tolerate a non-seekable, growing source.

### 4.1 ABI sketch: incremental source

Extend (v2, additive — never modify v1) with a streaming source:

```c
typedef struct mb_archive_stream_source_v2 {
    uint32_t struct_size, abi_version;
    /* forward-only feed supplied by the bridge; scanner never seeks */
    int (*pull_v2)(void *context, uint8_t *out, size_t want, size_t *got);
    /* called after each consumed chunk; returning non-zero cancels */
    int (*pump_v2)(void *context, uint64_t consumed);
} mb_archive_stream_source_v2;
```

* Scanner variant `mb_archive_scan_stream_v2`: registers only the
  streamable zip reader (`archive_read_support_format_zip_streamable`) —
  forward-only, no central directory. It emits incremental rows as entry
  headers arrive, charging the same budget counters, hashing each member as
  today (`name_row` + per-row sha256 unchanged).
* Identity: the scanner maintains a running sha256 over ALL consumed
  bytes; on EOF the bridge compares it with the transport's stream digest.
  `size` becomes `bytes consumed`, recorded in the result, not predeclared.
* Materialize stays v1: after the stream completes and identity verifies,
  the fully-written member files are already in the private quarantine
  (written through the same journal machinery); only the manifests remain,
  which `mb_archive_materialize_v1` can emit from the completed inventory.
  The scan-stream pass effectively merges with the materialize walk.
* Seekability loss is the price: streamable zip trusts local headers
  instead of the central directory. Mitigation: keep the v1 full scan as
  the ACCEPTANCE step after publish for console submissions that need
  central-directory verdicts, or accept the v2 verdict plus member-level
  sha256 (each member is fully hashed either way). This trade-off is a
  director decision, not silently chosen.
* Threshold policy: the bridge starts pumping into the scanner only after
  `stream_threshold_bytes` (e.g. 1 MiB) so pathological tiny files never
  pay scanner startup; backpressure is inherent — the transport is
  synchronous, so the scanner keeps pace with arrival by construction.
* Cancellation/budget: unchanged semantics (`cancelled`, deadline, io/ops
  counters); the pump callback is the natural cancellation point and keeps
  download cancel responsive during heavy prepare work.
* Failure: any scanner error or identity mismatch aborts the stream, and
  the bridge unlinks the private prefix — the download command reports
  transport failure exactly as today (the bytes were received but the
  payload is refused; that IS a download failure for this pipeline).

### 4.2 Why not interleaved scan-only-increment today

The pinned v1 scanner takes `size` up front (`source_start` compares
`st_size`) and re-stats the stamp at the end; both are incompatible with a
growing file. Any shortcut (fudge `size` to the eventual total, poll until
the file "looks done") would be a fake and is rejected. Approach A already
removes the second copy and the publish-order hazard; Approach B's only
additional win is overlapping parse/decompress with network arrival, which
needs the v2 source ABI above.

### 4.3 Console notes

* The bridge host flavor uses POSIX shims; console must route the five
  `mb_posix_*` calls through ps5_io. Known upstream asks remain: an RDWR
  open flag (the sink fd) and exported positional write (documented in
  worker-e2e-final's REPORT; unchanged by this work).
* No console link is claimed by this prototype; the host test proves the
  handoff against the REAL pinned intake + libarchive + mbedtls closure.

## 5. Evidence

`test/run_host_test.sh` builds the prototype plus the unmodified pinned
`intake.c` against the retained host libarchive/mbedtls closures and runs
46 checks (synthetic 10-entry stored ZIP, ~304 KB): full stream → handoff →
`mb_archive_scan_v1` (rows/bytes/identity) → `mb_archive_materialize_v1`
(journal states, manifest) → prepare-then-publish → refusal cases (gate
closed, wrong digest, total mismatch) → abort privacy. Result recorded in
`REPORT.md`.
