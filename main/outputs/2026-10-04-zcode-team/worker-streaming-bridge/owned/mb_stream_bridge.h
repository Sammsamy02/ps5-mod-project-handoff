/*
 * mb_stream_bridge.h — streaming download-to-preparation bridge (prototype).
 *
 * Origin: worker-streaming-bridge, 2026-10-04 (owned).
 *
 * Problem: the download stage (modinst_download.c, worker-e2e-final) writes
 * received bytes into a private temp file <intake>/<id>.zip.tmp through the
 * exclusive curl-owner file-sink ABI, then publishes by rename. The
 * preparation stage (ps5-mod-bridge native-host archive/intake.c,
 * mb_archive_scan_v1 / mb_archive_materialize_v1) consumes a
 * mb_archive_source_v1 {fresh O_RDONLY fd, size, sha256, target_token}.
 * Today nothing connects the two: the archive sits unexamined in intake
 * until a later command re-opens it by path, and a failed prepare leaves a
 * published-but-unusable archive behind.
 *
 * This prototype is the SIMPLER bridge approach (see
 * design/STREAMING-BRIDGE-DESIGN.md for the full incremental-feed design):
 *
 *   1. The download keeps writing exactly one private temp file — no second
 *      copy of the archive bytes is ever made. The bridge is that file's
 *      owner: it exposes sink callbacks with the same shape the transport's
 *      borrowed file-sink params expect (sequential write_at + positional
 *      readback), so modinst_download can delegate its sink I/O to the
 *      bridge without changing the transport ABI.
 *   2. When the transport reports stream completion, the bridge performs a
 *      THRESHOLD-GATED HANDOFF: fsync the write descriptor, retire both
 *      download-side descriptors, open one fresh O_RDONLY descriptor on the
 *      same inode, verify size/EOF/identity (a full re-read re-hash through
 *      an injected hash engine must equal the sink-computed digest), and
 *      emit a ready mb_archive_source_v1. The threshold gate refuses the
 *      handoff until the stream is complete and at least
 *      params.threshold_bytes are on disk (the prototype requires
 *      completion; a prefix-scanning consumer is the full streaming design).
 *   3. Because the handoff is fd-based, PREPARATION CAN RUN BEFORE THE
 *      RENAME: the composition may scan/materialize the handed-off source
 *      first and only then publish (rename + dir fsync) — a failed prepare
 *      then leaves no archive in intake at all. mb_bridge_publish_v1
 *      implements that ordering; the current composed order (publish then
 *      prepare) keeps working unchanged by calling publish first.
 *
 * Ownership/lifecycle rules (house style, fail closed):
 *   - init opens the private temp file pair; abort closes everything and
 *     unlinks the unpublished prefix (a failed download stays private).
 *   - handoff may be called once; after a successful handoff the bridge no
 *     longer owns the source descriptor — the consumer closes it via
 *     mb_bridge_source_close_v1 after scan/materialize (intake contract:
 *     "Caller source and quarantine descriptors remain open").
 *   - The hash engine is injected (same shape as the transport's injected
 *     entropy hook): this TU performs no crypto itself and is host-testable
 *     with any re-initializable engine. The host test injects CommonCrypto;
 *     the console composition injects the pinned MBedTLS sha256 closure.
 *   - Host flavor is plain POSIX; the console flavor must swap the five
 *     marked mb_posix_* shims for ps5_io equivalents (same split as
 *     modinst_download.c). No console link is claimed by this prototype.
 *
 * There are no threads, no blocking waits, no retries and no network here.
 * Callers must inspect every return code; failed calls leave state machine
 * and caller storage untouched unless explicitly documented below.
 */
#ifndef MB_STREAM_BRIDGE_H
#define MB_STREAM_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MB_STREAM_BRIDGE_ABI_V1 1u

/* Stream progress thresholds. */
#define MB_STREAM_BRIDGE_THRESHOLD_MAX_BYTES (UINT64_C(8) << 30) /* intake raw cap */
#define MB_STREAM_BRIDGE_MAX_BYTES MB_STREAM_BRIDGE_THRESHOLD_MAX_BYTES

enum mb_stream_bridge_state_v1 {
    MB_STREAM_BRIDGE_CREATED = 0,        /* init ok, no bytes yet */
    MB_STREAM_BRIDGE_STREAMING = 1,      /* sink_write accepted bytes */
    MB_STREAM_BRIDGE_STREAM_COMPLETE = 2,/* sink_finish ok, handoff allowed */
    MB_STREAM_BRIDGE_HANDOFF_COMPLETE = 3,/* source emitted; bridge done */
    MB_STREAM_BRIDGE_FAILED = 4,         /* terminal; abort required */
    MB_STREAM_BRIDGE_ABORTED = 5         /* terminal; prefix unlinked */
};

enum mb_stream_bridge_status_v1 {
    MB_STREAM_BRIDGE_OK = 0,
    MB_STREAM_BRIDGE_BAD_ARGUMENT = 1,
    MB_STREAM_BRIDGE_BAD_STATE = 2,      /* call not allowed in this state */
    MB_STREAM_BRIDGE_IO_ERROR = 3,
    MB_STREAM_BRIDGE_IDENTITY_MISMATCH = 4, /* file bytes != sink digest */
    MB_STREAM_BRIDGE_LIMIT = 5,          /* cap/threshold not satisfiable */
    MB_STREAM_BRIDGE_NOT_READY = 6       /* threshold gate still closed */
};

/* Injected re-initializable hash engine (init/update/finish, 0 == ok).
 * finish_v1 must release nothing (the bridge re-inits for the handoff
 * re-read); heap ownership stays with the context owner. The console
 * composition passes the pinned MBedTLS sha256 closure here. */
typedef struct mb_stream_bridge_hash_v1 {
    void *context;
    int (*init_v1)(void *context);
    int (*update_v1)(void *context, const uint8_t *data, size_t len);
    int (*finish_v1)(void *context, uint8_t out[32]);
} mb_stream_bridge_hash_v1;

typedef struct mb_stream_bridge_params_v1 {
    uint32_t struct_size, abi_version;
    uint64_t threshold_bytes;  /* handoff requires received >= this; 0 means
                                * completion alone opens the gate */
    uint64_t max_bytes;        /* stream cap (sink limit); must be <= MAX */
} mb_stream_bridge_params_v1;

/* Exactly the fields mb_archive_source_v1 needs from the download side.
 * The consumer copies them into its own mb_archive_source_v1 (which adds
 * target_token and owns fd identity checks). */
typedef struct mb_stream_bridge_source_v1 {
    int fd;                    /* fresh O_RDONLY regular descriptor */
    uint64_t size;             /* == fstat st_size == received bytes */
    uint8_t sha256[32];        /* verified: re-read digest == sink digest */
} mb_stream_bridge_source_v1;

typedef struct mb_stream_bridge_v1 {
    uint32_t struct_size, abi_version;
    uint32_t state, reserved;
    uint64_t received_bytes;   /* bytes accepted via sink_write */
    uint64_t threshold_bytes;
    uint64_t max_bytes;
    /* Private: descriptors, temp path, sink digest, hash engine. */
    int fd_w, fd_r;
    char *tmp_path;
    uint8_t sink_sha256[32];
    uint64_t sink_total;
    mb_stream_bridge_hash_v1 hash;
} mb_stream_bridge_v1;

/* Open <tmp_path> O_RDWR|CREAT|TRUNC 0600 plus a read-only readback
 * descriptor (the exact two-descriptor scheme of modinst_download.c).
 * The hash engine is copied by value; it must outlive the bridge. */
int mb_stream_bridge_init_v1(mb_stream_bridge_v1 *bridge, const char *tmp_path,
                             const mb_stream_bridge_params_v1 *params,
                             const mb_stream_bridge_hash_v1 *hash);

/* Sequential sink write (the transport's write_at adapter target). Updates
 * the running hash and enforces max_bytes. *written receives len on OK. */
int mb_stream_bridge_sink_write_v1(mb_stream_bridge_v1 *bridge,
                                   const uint8_t *buf, uint32_t len,
                                   uint32_t *written);

/* Positional sink readback (the transport's read_at adapter target); never
 * disturbs the write position. */
int mb_stream_bridge_sink_read_v1(mb_stream_bridge_v1 *bridge, uint64_t off,
                                  uint8_t *buf, uint32_t len, uint32_t *read);

/* Record transport completion: total must equal received bytes; sha256 is
 * the transport-computed stream digest used as the handoff identity. */
int mb_stream_bridge_sink_finish_v1(mb_stream_bridge_v1 *bridge,
                                    uint64_t total_bytes,
                                    const uint8_t sha256[32]);

/* THRESHOLD GATE. Requires STREAM_COMPLETE and received >= threshold.
 * fsync -> retire download descriptors -> fresh O_RDONLY fstat identity
 * (regular, size, EOF probe) -> full re-read re-hash == sink digest ->
 * emit source. On failure the emitted fd (if any) is closed and the bridge
 * is FAILED; the temp prefix is NOT unlinked (abort decides that). */
int mb_stream_bridge_handoff_v1(mb_stream_bridge_v1 *bridge,
                                mb_stream_bridge_source_v1 *out);

/* Post-prepare publish: rename the (handed-off, prepared) temp inode to
 * final_path and fsync the intake directory. Requires HANDOFF_COMPLETE. */
int mb_stream_bridge_publish_v1(mb_stream_bridge_v1 *bridge,
                                const char *final_path);

/* Close the consumer-side source descriptor and zero the record. */
int mb_stream_bridge_source_close_v1(mb_stream_bridge_source_v1 *source);

/* Terminal cleanup on any path: close bridge-owned descriptors, unlink the
 * private temp prefix (best effort), state = ABORTED. Idempotent. */
int mb_stream_bridge_abort_v1(mb_stream_bridge_v1 *bridge);

#ifdef __cplusplus
}
#endif

#endif /* MB_STREAM_BRIDGE_H */
