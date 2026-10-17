/*
 * bridge_host_test.c — host test for the streaming download-to-preparation
 * bridge (worker-streaming-bridge, 2026-10-04).
 *
 * Links the REAL pinned intake (archive/intake.c) against a host libarchive
 * + mbedtls closure and drives a synthetic stored-ZIP through the bridge as
 * if the exclusive curl owner were streaming it:
 *
 *   A. full stream -> threshold gate -> handoff -> REAL mb_archive_scan_v1
 *      -> REAL mb_archive_materialize_v1 -> publish (prepare-before-publish
 *      ordering) -> abort leaves nothing behind.
 *   B. pure threshold gate: completion with received < threshold stays
 *      NOT_READY.
 *   C. wrong sink digest -> handoff IDENTITY_MISMATCH (the scanner never
 *      sees a file it could only reject late).
 *   D. finish total mismatch -> LIMIT.
 *   E. abort mid-stream unlinks the private prefix.
 *
 * argv: bridge_host_test FIXTURE.ZIP ROWS REGULAR_BYTES WORKDIR
 */
#define _DARWIN_C_SOURCE 1
#include "mb_stream_bridge.h"
#include "intake.h"

#include <CommonCrypto/CommonDigest.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_checks = 0, g_failed = 0;
#define CHECK(cond, name) do { ++g_checks; if (cond) printf("PASS %s\n", name); \
    else { ++g_failed; printf("FAIL %s\n", name); } } while (0)

/* ------------------- injected hash engine: CommonCrypto -------------------- */
typedef struct { CC_SHA256_CTX ctx; } cc_engine;
static int cc_init(void *c) { CC_SHA256_Init((CC_SHA256_CTX *)c); return 0; }
static int cc_update(void *c, const uint8_t *d, size_t n) {
    return CC_SHA256_Update((CC_SHA256_CTX *)c, d, (CC_LONG)n) == 1 ? 0 : -1;
}
static int cc_finish(void *c, uint8_t out[32]) {
    return CC_SHA256_Final(out, (CC_SHA256_CTX *)c) == 1 ? 0 : -1;
}
/* The bridge copies the engine by value; the cc_engine must outlive it. */
static mb_stream_bridge_hash_v1 mk_hash(cc_engine *e) {
    mb_stream_bridge_hash_v1 h;
    h.context = &e->ctx;
    h.init_v1 = cc_init; h.update_v1 = cc_update; h.finish_v1 = cc_finish;
    return h;
}

/* ------------------------------ test helpers ------------------------------ */
static uint8_t *slurp(const char *path, uint64_t *size) {
    FILE *f = fopen(path, "rb");
    uint8_t *buf;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    *size = (uint64_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (uint8_t *)malloc(*size ? (size_t)*size : 1);
    if (!buf || fread(buf, 1, (size_t)*size, f) != *size) { free(buf); fclose(f); return 0; }
    fclose(f);
    return buf;
}

static void scan_buffers_init(mb_archive_buffers_v1 *v, uint32_t rows, uint32_t names) {
    v->rows = (mb_archive_row_v1 *)calloc(rows, sizeof(mb_archive_row_v1));
    v->names = (char *)calloc(1, names);
    v->sort_indices = (uint32_t *)calloc(rows, sizeof(uint32_t));
    v->chunk = (uint8_t *)calloc(1, MB_ARCHIVE_CHUNK_BYTES);
    v->row_capacity = rows; v->name_capacity = names;
    v->sort_capacity = rows; v->chunk_capacity = MB_ARCHIVE_CHUNK_BYTES;
}
static void scan_buffers_free(mb_archive_buffers_v1 *v) {
    free(v->rows); free(v->names); free(v->sort_indices); free(v->chunk);
    memset(v, 0, sizeof(*v));
}

static void params_fill(mb_stream_bridge_params_v1 *p, uint64_t threshold, uint64_t maxb) {
    memset(p, 0, sizeof *p);
    p->struct_size = (uint32_t)sizeof *p;
    p->abi_version = MB_STREAM_BRIDGE_ABI_V1;
    p->threshold_bytes = threshold;
    p->max_bytes = maxb;
}

static void stream_chunks(mb_stream_bridge_v1 *b, const mb_stream_bridge_hash_v1 *hash,
                          const uint8_t *data, uint64_t size,
                          const uint64_t *cuts, size_t ncuts) {
    /* cuts are byte offsets where a sink_write boundary lands (odd sizes). */
    uint64_t off = 0; size_t ci = 0;
    while (off < size) {
        uint64_t next = (ci < ncuts && cuts[ci] > off && cuts[ci] <= size) ? cuts[ci] : size;
        uint32_t len = (uint32_t)(next - off);
        uint32_t written = 0;
        if (mb_stream_bridge_sink_write_v1(b, data + off, len, &written) != MB_STREAM_BRIDGE_OK ||
            written != len) { printf("FAIL stream write at %llu\n", (unsigned long long)off); ++g_failed; return; }
        off = next; ++ci;
    }
    (void)hash;
}

int main(int argc, char **argv) {
    const char *fixture, *workdir;
    uint64_t rows_expect, reg_expect, fsize = 0;
    uint8_t *zip;
    uint64_t cuts1[4];
    mb_stream_bridge_v1 br;
    mb_stream_bridge_params_v1 p;
    mb_stream_bridge_source_v1 src;
    mb_archive_source_v1 source;
    mb_archive_limits_v1 limits;
    mb_archive_budget_v1 budget;
    mb_archive_buffers_v1 bufs;
    mb_archive_inventory_v1 inv;
    cc_engine engine_a;
    mb_stream_bridge_hash_v1 hash_a;
    char tmp[4096], finalp[4096], qdir[4096];
    uint32_t i, selected[2], selected_count = 0;
    uint64_t reg_sum = 0;
    uint8_t sha_a[32];

    if (argc != 5) { printf("usage: %s FIXTURE.ZIP ROWS REGULAR_BYTES WORKDIR\n", argv[0]); return 2; }
    fixture = argv[1]; rows_expect = strtoull(argv[2], 0, 10);
    reg_expect = strtoull(argv[3], 0, 10); workdir = argv[4];
    zip = slurp(fixture, &fsize);
    if (!zip) { printf("FAIL fixture read\n"); return 2; }
    snprintf(tmp, sizeof tmp, "%s/bridge-a.zip.tmp", workdir);
    snprintf(finalp, sizeof finalp, "%s/bridge-a.zip", workdir);

    /* ---------------- A: full stream, handoff, REAL intake ---------------- */
    params_fill(&p, fsize, MB_STREAM_BRIDGE_MAX_BYTES); /* gate opens at completion */
    hash_a = mk_hash(&engine_a);
    CHECK(mb_stream_bridge_init_v1(&br, tmp, &p, &hash_a) == MB_STREAM_BRIDGE_OK, "A.init");
    cuts1[0] = 1; cuts1[1] = 2; cuts1[2] = 65537; cuts1[3] = 98311; /* odd boundaries */
    stream_chunks(&br, &hash_a, zip, fsize, cuts1, 4);
    CHECK(br.received_bytes == fsize && br.state == MB_STREAM_BRIDGE_STREAMING, "A.streamed");
    CHECK(mb_stream_bridge_sink_finish_v1(&br, fsize, 0) == MB_STREAM_BRIDGE_BAD_ARGUMENT,
          "A.finish-needs-sha");
    { uint8_t sha[32];
      CHECK(hash_a.finish_v1(hash_a.context, sha) == 0, "A.transport-sha");
      CHECK(mb_stream_bridge_sink_finish_v1(&br, fsize, sha) == MB_STREAM_BRIDGE_OK, "A.finish");
      /* readback channel spot check (transport read_at semantics). */
      { uint8_t rb[16]; uint32_t got = 0;
        CHECK(mb_stream_bridge_sink_read_v1(&br, 0, rb, sizeof rb, &got) == MB_STREAM_BRIDGE_OK &&
              got == sizeof rb && !memcmp(rb, zip, sizeof rb), "A.readback"); }
      CHECK(mb_stream_bridge_handoff_v1(&br, &src) == MB_STREAM_BRIDGE_OK, "A.handoff");
      CHECK(src.fd >= 0 && src.size == fsize && !memcmp(src.sha256, sha, 32), "A.source");
      memcpy(sha_a, sha, 32); }

    /* Feed the REAL pinned intake scanner. */
    memset(&source, 0, sizeof source);
    source.struct_size = (uint32_t)sizeof source; source.abi_version = MB_ARCHIVE_ABI_V1;
    source.fd = src.fd; source.size = src.size;
    memcpy(source.sha256, src.sha256, 32);
    for (i = 0; i < 32; ++i) source.target_token[i] = (uint8_t)(0x5A ^ i);
    mb_archive_limits_default_v1(&limits);
    mb_archive_budget_default_v1(&budget);
    scan_buffers_init(&bufs, 128, 1u << 20);
    CHECK(mb_archive_scan_v1(&source, &limits, &budget, &bufs, &inv) == MB_ARCHIVE_OK, "A.scan");
    CHECK(inv.complete == 1 && inv.status == MB_ARCHIVE_OK && inv.row_count == rows_expect,
          "A.scan-rows");
    CHECK(inv.expanded_bytes == reg_expect, "A.scan-expanded");
    CHECK(inv.source_size == fsize && !memcmp(inv.source_sha256, sha_a, 32) &&
          !memcmp(inv.target_token, source.target_token, 32), "A.scan-identity");
    for (i = 0; i < inv.row_count; ++i)
        if (bufs.rows[i].kind == MB_ARCHIVE_REGULAR) reg_sum += bufs.rows[i].size;
    CHECK(reg_sum == reg_expect, "A.row-size-sum");

    /* REAL materialize on the handed-off (still private) inode: the
     * prepare-before-publish ordering this bridge enables. */
    snprintf(qdir, sizeof qdir, "%s/quarantine.XXXXXX", workdir);
    if (!mkdtemp(qdir)) { printf("FAIL mkdtemp\n"); return 2; }
    {
        mb_archive_materialize_request_v1 q;
        mb_archive_materialized_v1 mat;
        mb_archive_buffers_v1 replay;
        mb_archive_owned_file_v1 journal[5];
        uint32_t j, ok = 1;
        for (i = 0; i < inv.row_count && selected_count < 2; ++i)
            if (bufs.rows[i].kind == MB_ARCHIVE_REGULAR) selected[selected_count++] = i;
        CHECK(selected_count == 2, "A.selected");
        memset(&q, 0, sizeof q);
        q.source = &source; q.inventory = &inv;
        q.rows = bufs.rows; q.row_capacity = bufs.row_capacity;
        q.names = bufs.names; q.name_capacity = bufs.name_capacity;
        q.selected = selected; q.selected_count = selected_count;
        memset(journal, 0, sizeof journal);
        q.journal = journal; q.journal_capacity = (uint32_t)(selected_count + 3);
        memcpy(q.target_token, source.target_token, 32);
        { int qfd = open(qdir, O_RDONLY | O_DIRECTORY); struct stat qs;
          q.quarantine_fd = qfd;
          if (qfd >= 0 && !fstat(qfd, &qs)) { q.quarantine_device = (uint64_t)qs.st_dev;
            q.quarantine_inode = (uint64_t)qs.st_ino; } }
        scan_buffers_init(&replay, 128, 1u << 20);
        q.replay = replay;
        memset(&mat, 0, sizeof mat);
        { int rc = mb_archive_materialize_v1(&q, &limits, &budget, &mat);
          CHECK(rc == MB_ARCHIVE_OK && mat.complete == 1 && mat.status == MB_ARCHIVE_OK,
                "A.materialize");
          CHECK(mat.selected_count == 2 && mat.manifest_name[0], "A.materialize-manifest");
          for (j = 0; j < q.journal_capacity; ++j)
              if (journal[j].state != MB_ARCHIVE_CREATED && journal[j].state != MB_ARCHIVE_REMOVED)
                  ok = 0;
          CHECK(ok, "A.journal-states"); }
        scan_buffers_free(&replay);
        /* budget accounting is cumulative across scan+materialize: prove the
         * intake contract held without tripping any limit. */
        CHECK(budget.io_used > 0 && budget.io_used <= budget.io_limit, "A.budget");
    }
    CHECK(mb_stream_bridge_source_close_v1(&src) == MB_STREAM_BRIDGE_OK && src.fd == -1,
          "A.source-close");
    /* Publish AFTER prepare succeeded (the reordered flow), then nothing left. */
    CHECK(mb_stream_bridge_publish_v1(&br, finalp) == MB_STREAM_BRIDGE_OK, "A.publish");
    { struct stat st; CHECK(stat(finalp, &st) == 0 && S_ISREG(st.st_mode), "A.published"); }
    CHECK(mb_stream_bridge_abort_v1(&br) == MB_STREAM_BRIDGE_OK, "A.abort-after-publish");
    CHECK(access(tmp, F_OK) != 0, "A.no-tmp-after-publish");

    /* ---------------- B: pure threshold gate stays closed ---------------- */
    { mb_stream_bridge_v1 b2; mb_stream_bridge_params_v1 p2; mb_stream_bridge_source_v1 s2;
      mb_stream_bridge_hash_v1 hash_b; cc_engine engine_b;
      uint32_t written = 0; char t2[4096]; uint8_t sha[32];
      snprintf(t2, sizeof t2, "%s/bridge-b.zip.tmp", workdir);
      params_fill(&p2, fsize + 1, fsize + 1); /* unreachable gate */
      hash_b = mk_hash(&engine_b);
      CHECK(mb_stream_bridge_init_v1(&b2, t2, &p2, &hash_b) == MB_STREAM_BRIDGE_OK, "B.init");
      CHECK(mb_stream_bridge_sink_write_v1(&b2, zip, (uint32_t)fsize, &written) == MB_STREAM_BRIDGE_OK &&
            written == fsize, "B.streamed");
      CHECK(hash_b.finish_v1(hash_b.context, sha) == 0, "B.sha");
      CHECK(mb_stream_bridge_sink_finish_v1(&b2, fsize, sha) == MB_STREAM_BRIDGE_OK, "B.finish");
      CHECK(mb_stream_bridge_handoff_v1(&b2, &s2) == MB_STREAM_BRIDGE_NOT_READY, "B.gate-closed");
      CHECK(mb_stream_bridge_abort_v1(&b2) == MB_STREAM_BRIDGE_OK, "B.abort");
      CHECK(access(t2, F_OK) != 0, "B.no-tmp"); }

    /* ---------------- C: wrong sink digest refused at handoff ------------- */
    { mb_stream_bridge_v1 b3; mb_stream_bridge_params_v1 p3; mb_stream_bridge_source_v1 s3;
      mb_stream_bridge_hash_v1 hash_c; cc_engine engine_c;
      uint32_t written = 0; char t3[4096]; uint8_t badsha[32];
      snprintf(t3, sizeof t3, "%s/bridge-c.zip.tmp", workdir);
      params_fill(&p3, 0, MB_STREAM_BRIDGE_MAX_BYTES);
      hash_c = mk_hash(&engine_c);
      CHECK(mb_stream_bridge_init_v1(&b3, t3, &p3, &hash_c) == MB_STREAM_BRIDGE_OK, "C.init");
      CHECK(mb_stream_bridge_sink_write_v1(&b3, zip, (uint32_t)fsize, &written) == MB_STREAM_BRIDGE_OK,
            "C.streamed");
      memset(badsha, 0xAB, sizeof badsha);
      CHECK(mb_stream_bridge_sink_finish_v1(&b3, fsize, badsha) == MB_STREAM_BRIDGE_OK, "C.finish");
      CHECK(mb_stream_bridge_handoff_v1(&b3, &s3) == MB_STREAM_BRIDGE_IDENTITY_MISMATCH, "C.refused");
      CHECK(mb_stream_bridge_abort_v1(&b3) == MB_STREAM_BRIDGE_OK, "C.abort");
      CHECK(access(t3, F_OK) != 0, "C.no-tmp"); }

    /* ---------------- D: finish total mismatch refused -------------------- */
    { mb_stream_bridge_v1 b4; mb_stream_bridge_params_v1 p4; mb_stream_bridge_hash_v1 hash_d;
      cc_engine engine_d; char t4[4096]; uint32_t written = 0; uint8_t sha[32];
      snprintf(t4, sizeof t4, "%s/bridge-d.zip.tmp", workdir);
      params_fill(&p4, 0, MB_STREAM_BRIDGE_MAX_BYTES);
      hash_d = mk_hash(&engine_d);
      CHECK(mb_stream_bridge_init_v1(&b4, t4, &p4, &hash_d) == MB_STREAM_BRIDGE_OK, "D.init");
      CHECK(mb_stream_bridge_sink_write_v1(&b4, zip, 1024, &written) == MB_STREAM_BRIDGE_OK, "D.streamed");
      CHECK(hash_d.finish_v1(hash_d.context, sha) == 0, "D.sha");
      CHECK(mb_stream_bridge_sink_finish_v1(&b4, 1025, sha) == MB_STREAM_BRIDGE_LIMIT, "D.total-refused");
      CHECK(mb_stream_bridge_abort_v1(&b4) == MB_STREAM_BRIDGE_OK, "D.abort"); }

    /* ---------------- E: abort mid-stream unlinks prefix ------------------ */
    { mb_stream_bridge_v1 b5; mb_stream_bridge_params_v1 p5; mb_stream_bridge_hash_v1 hash_e;
      cc_engine engine_e; char t5[4096]; uint32_t written = 0;
      snprintf(t5, sizeof t5, "%s/bridge-e.zip.tmp", workdir);
      params_fill(&p5, 0, MB_STREAM_BRIDGE_MAX_BYTES);
      hash_e = mk_hash(&engine_e);
      CHECK(mb_stream_bridge_init_v1(&b5, t5, &p5, &hash_e) == MB_STREAM_BRIDGE_OK, "E.init");
      CHECK(mb_stream_bridge_sink_write_v1(&b5, zip, 512, &written) == MB_STREAM_BRIDGE_OK, "E.streamed");
      CHECK(access(t5, F_OK) == 0, "E.tmp-exists-midstream");
      CHECK(mb_stream_bridge_abort_v1(&b5) == MB_STREAM_BRIDGE_OK, "E.abort");
      CHECK(access(t5, F_OK) != 0, "E.no-tmp"); }

    free(zip);
    scan_buffers_free(&bufs);
    { char cmd[8192]; snprintf(cmd, sizeof cmd, "rm -rf '%s'", qdir); if (system(cmd)) {} }
    printf("CHECKS=%d FAILED=%d\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
