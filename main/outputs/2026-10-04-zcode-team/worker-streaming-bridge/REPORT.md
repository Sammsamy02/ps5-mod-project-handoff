# REPORT.md — worker-streaming-bridge (2026-10-04, P3)

Worker: worker-streaming-bridge (ZCode team, PS5 mod-manager).
Scope: design + prototype the streaming download-to-preparation bridge
between the archive download path and the archive intake path, without
loading entire archives into memory. CLAIM.json was written first; this
file is last. Everything new lives under this directory only; all inputs
elsewhere were treated as read-only.

## 1. What was read (provenance)

* `outputs/2026-10-04-zcode-team/worker-e2e-final/owned/modinst_download.{c,h}`
  — stage-1 download: exclusive curl owner (`mb_curl_runtime_start_v1` /
  `mb_curl_download_v1` / `mb_curl_runtime_close_v1`), borrowed file-sink
  params (sequential `write_at` + positional `read_at` on a private
  `<intake>/<id>.zip.tmp` pair of descriptors), transport-computed sha256,
  fsync → rename publish → sidecar record; failed prefixes stay private.
* `outputs/ps5-mod-bridge/native-host/archive/intake.{c}` — pinned intake:
  `mb_archive_scan_v1` / `mb_archive_materialize_v1` over
  `mb_archive_source_v1` (fresh O_RDONLY regular fd, size, sha256,
  target_token), with start/end stat-stamp checks, full-file re-read re-hash
  (`source_end`), EOF probe, budgeted I/O, fail-closed identity.
* `outputs/ps5-mod-bridge/native-host/download/runtime.h`,
  `.../archive/private/posix_compat.{c,h}` (linked, unmodified),
  `worker-e2e-final/owned/modinst_archive_prepare.h` (stage-2a context).

## 2. What was designed

`design/STREAMING-BRIDGE-DESIGN.md`:

* **Approach A (prototype, tested)** — temp file + threshold handoff: the
  download keeps its single private temp copy (no second copy ever);
  a threshold-gated handoff retires the download descriptors, reopens the
  same inode fresh O_RDONLY, verifies size/EOF/identity (full re-read
  re-hash through an injected hash engine), and emits exactly the
  `mb_archive_source_v1` inputs. Because the handoff is fd-based, prepare
  can run BEFORE the rename publish — a failed prepare then leaves intake
  clean (the current composed order keeps working unchanged too).
* **Approach B (future, not built)** — full streaming: an additive v2
  stream-source ABI (`pull`/`pump`, forward-only streamable zip, running
  sha256 identity, single-threaded pump-driven overlap with the exclusive
  curl owner, threshold before scanner start). The doc explains why this
  cannot be faked on the pinned v1 scanner (fixed size + end-stamp
  contract) and names the central-directory trade-off as an explicit
  director decision.

## 3. What was built

* `owned/mb_stream_bridge.h` / `owned/mb_stream_bridge.c` — the bridge
  state machine (CREATED → STREAMING → STREAM_COMPLETE → HANDOFF_COMPLETE,
  FAILED/ABORTED terminal), sink adapters matching the file-sink callback
  shapes, threshold gate, handoff identity verification, prepare-then-
  publish, source close, abort privacy. No crypto inside the TU (injected
  engine, same pattern as the transport's injected entropy hook); host
  flavor is POSIX with five `mb_posix_*` shims marked for the ps5_io
  console swap. No console link is claimed.
* `test/make_fixture.py` — deterministic synthetic stored-ZIP (10 entries:
  3 dirs + 7 files, 303,652 regular bytes; no intake-rejected names).
* `test/bridge_host_test.c` + `test/run_host_test.sh` — host test linking
  the bridge AND the UNMODIFIED pinned `intake.c` against the retained
  read-only host closures (libarchive from
  `.superpowers/.../native-zip-experiment-1/host/libarchive-build`,
  mbedtls from `.../native-https-host-experiment-2/host-libraries`), plus
  `-lz` for the libarchive closure's zlib symbols.

## 4. Host test evidence (actual run, 2026-10-04)

`test/run_host_test.sh` on this Mac (arm64, clang/Xcode SDK):
fixture 10 rows / 303,652 bytes / archive 304,752 bytes streamed in odd
chunks (1, 2, 65537, 98311 boundaries).

**46 checks, 0 failed**, including:

* A: stream → threshold gate → handoff → REAL `mb_archive_scan_v1`
  (`A.scan`, rows/expanded/identity/row-size-sum all match) → REAL
  `mb_archive_materialize_v1` on the still-private inode (`A.materialize`,
  manifest + journal states CREATED/REMOVED only) → `publish` AFTER prepare
  (`A.publish*`) → abort leaves nothing.
* B: stream complete but `received < threshold` → handoff `NOT_READY`
  (gate stays closed); abort privacy.
* C: wrong transport sha256 → handoff `IDENTITY_MISMATCH` — the scanner
  never sees a file it could only reject late; abort privacy.
* D: finish total mismatch → `LIMIT`.
* E: abort mid-stream unlinks the private prefix.
* Budget note: `A.budget` proves the cumulative scan+materialize intake
  I/O accounting held (used ≤ limit) through the bridge handoff.

## 5. Honest limits (no overclaim)

* Prototype quality: the bridge is new, host-tested code. It is NOT linked
  into the composed installer and NOT linked for console; `mi_cmd_download`
  integration (pointing `dp.sink.*` at the bridge, calling
  `sink_finish`/`handoff`/`publish`) is a small future patch, described in
  the design doc, not performed here.
* The console flavor needs the documented ps5_io shims (RDWR open flag,
  positional readback) — upstream asks unchanged from worker-e2e-final.
* Approach B (true overlap while downloading) is a design only; it needs
  the v2 stream-source ABI and a streamable-zip policy decision. This work
  claims overlap only in the prepare-vs-publish ordering sense that
  Approach A enables and the host test demonstrates.
* Host test uses injected CommonCrypto sha256; the pinned MBedTLS closure
  is exercised (real `intake.c` hashing) but the bridge's injected engine
  in the console composition is future wiring.
* Read-only inputs used for linking (never modified): the spike-built
  libarchive/mbedtls archives and vendored headers listed in CLAIM.json.

## 6. Files (this directory)

```
CLAIM.json
REPORT.md                          (this file, last)
owned/mb_stream_bridge.h           (prototype API)
owned/mb_stream_bridge.c           (prototype impl)
design/STREAMING-BRIDGE-DESIGN.md  (Approach A semantics + Approach B future design)
test/make_fixture.py               (synthetic archive generator)
test/bridge_host_test.c            (host test, real intake linked)
test/run_host_test.sh              (build + run)
build/                             (build artifacts + fixtures from the run)
```
