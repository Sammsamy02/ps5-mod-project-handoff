/*
 * test_prepare_bridge.c — host stub tests for ps5_prepare_bridge.
 * Origin: worker-installer-ps5io, 2026-10-04.
 *
 * The intake api is stubbed; the tests prove the bridge's sequencing,
 * arguments and failure rules (journal never erased on failure) without the
 * pinned libarchive closure, whose link stays controller-composed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ps5_io.h"
#include "ps5_prepare_bridge.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, name) do { \
    if (cond) { ++g_pass; } \
    else { ++g_fail; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)

/* ---- stub intake ---------------------------------------------------------- */

static char g_order[8];
static int g_calls;
static mb_archive_source_v1 g_src_cap;
static mb_archive_materialize_request_v1 g_mr_cap;
static uint64_t g_deadline_cap; static int (*g_cancel_cap)(void*);
static int g_scan_fail, g_mat_fail, g_inventory_unsupported;

static void stub_limits(mb_archive_limits_v1 *l) { memset(l, 0x5a, sizeof *l); }
static void stub_budget(mb_archive_budget_v1 *b) { memset(b, 0, sizeof *b); }

static int stub_scan(const mb_archive_source_v1 *src,
                     const mb_archive_limits_v1 *lim, mb_archive_budget_v1 *bud,
                     const mb_archive_buffers_v1 *buf, mb_archive_inventory_v1 *inv)
{
    int i;
    (void)lim; (void)buf;
    g_order[g_calls++] = 'S';
    g_src_cap = *src;
    g_deadline_cap = bud->deadline_ms; g_cancel_cap = bud->cancelled;
    if (g_scan_fail) return MB_ARCHIVE_IO_ERROR;
    memset(inv, 0, sizeof *inv);
    inv->struct_size = (uint32_t)sizeof *inv;
    inv->abi_version = MB_ARCHIVE_ABI_V1;
    inv->complete = 1;
    inv->installation_supported = !g_inventory_unsupported;
    inv->row_count = 3;
    for (i = 0; i < 32; ++i) inv->inventory_sha256[i] = (uint8_t)(i + 1);
    /* rows the caller must already own: regular, directory, regular */
    { mb_archive_row_v1 *r = (mb_archive_row_v1 *)buf->rows;
      memset(r, 0, 3 * sizeof *r);
      r[0].kind = MB_ARCHIVE_REGULAR;  r[0].path_offset = 0;  r[0].path_length = 6; r[0].size = 10;
      r[1].kind = MB_ARCHIVE_DIRECTORY; r[1].path_offset = 7;  r[1].path_length = 3;
      r[2].kind = MB_ARCHIVE_REGULAR;  r[2].path_offset = 11; r[2].path_length = 7; r[2].size = 20;
      memcpy((char *)buf->names, "meshes\0dir\0lua/mod\0", 19); }
    return 0;
}

static int stub_materialize(const mb_archive_materialize_request_v1 *req,
                            const mb_archive_limits_v1 *lim, mb_archive_budget_v1 *bud,
                            mb_archive_materialized_v1 *out)
{
    (void)lim; (void)bud;
    g_order[g_calls++] = 'M';
    g_mr_cap = *req;
    if (g_mat_fail) return MB_ARCHIVE_IO_ERROR;
    memset(out, 0, sizeof *out);
    out->struct_size = (uint32_t)sizeof *out;
    out->abi_version = MB_ARCHIVE_ABI_V1;
    out->complete = 1;
    out->selected_count = req->selected_count;
    out->decoded_bytes = 12345;
    strcpy(out->manifest_name, "mb-owned-000001");
    return 0;
}

static const ps5_intake_api STUB = { stub_limits, stub_budget, stub_scan,
                                     stub_materialize };

/* ---- harness --------------------------------------------------------------- */

static char qdir[256];

typedef struct { mb_archive_row_v1 rows[8]; char names[256]; uint32_t sort[8];
                 uint8_t chunk[MB_ARCHIVE_CHUNK_BYTES];
                 mb_archive_row_v1 rrows[8]; char rnames[256]; uint32_t rsort[8];
                 uint8_t rchunk[MB_ARCHIVE_CHUNK_BYTES];
                 uint32_t selected[8]; mb_archive_owned_file_v1 journal[8]; } scratch;

static void fill_req(ps5_prepare_request *req, scratch *sc, int archive_fd)
{
    memset(req, 0, sizeof *req);
    memset(sc, 0, sizeof *sc);
    req->archive_fd = archive_fd;
    req->archive_size = 4096;
    { int i; for (i = 0; i < 32; ++i) req->archive_sha256[i] = (uint8_t)(i * 3); }
    req->quarantine_dir = qdir;
    req->rows = sc->rows; req->row_capacity = 8;
    req->names = sc->names; req->name_capacity = 256;
    req->sort = sc->sort; req->sort_capacity = 8;
    req->chunk = sc->chunk; req->chunk_capacity = MB_ARCHIVE_CHUNK_BYTES;
    req->replay_rows = sc->rrows; req->replay_row_capacity = 8;
    req->replay_names = sc->rnames; req->replay_name_capacity = 256;
    req->replay_sort = sc->rsort; req->replay_sort_capacity = 8;
    req->replay_chunk = sc->rchunk; req->replay_chunk_capacity = MB_ARCHIVE_CHUNK_BYTES;
    req->selected = sc->selected; req->selected_capacity = 8;
    req->journal = sc->journal; req->journal_capacity = 8;
}

static int cancel_yes(void *ctx) { (void)ctx; return 1; }
static int cancel_no(void *ctx) { (void)ctx; return 0; }

static int big_only(void *ctx, const mb_archive_row_v1 *row, const char *name)
{ (void)ctx; (void)name; return row->size > 15 ? 1 : 0; }

int main(void)
{
    ps5_prepare_request req; ps5_prepare_result res; scratch sc;
    int afd;

    snprintf(qdir, sizeof qdir, "/tmp/ps5prep-%ld/q", (long)getpid());
    { char cmd[400]; snprintf(cmd, sizeof cmd, "rm -rf /tmp/ps5prep-%ld", (long)getpid());
      if (system(cmd)) {} }
    if (mkdir("/tmp", 0755)) {}
    snprintf(qdir, sizeof qdir, "/tmp/ps5prep-%ld", (long)getpid());
    if (mkdir(qdir, 0755)) {}
    snprintf(qdir + strlen(qdir), 8, "/q");
    if (mkdir(qdir, 0755)) {}

    afd = ps5io_open("/etc/hosts", PS5IO_RDONLY, 0); /* any readable fd */
    CHECK(afd >= 0, "archive fd");
    fill_req(&req, &sc, afd);

    /* happy path */
    memset(&res, 0, sizeof res);
    g_calls = 0; g_scan_fail = g_mat_fail = g_inventory_unsupported = 0;
    CHECK(ps5_prepare_archive(&STUB, &req, &res) == 0, "prepare ok");
    CHECK(res.status == 0 && res.complete == 1, "prepared status");
    CHECK(g_calls == 2 && g_order[0] == 'S' && g_order[1] == 'M', "scan then materialize");
    CHECK(g_src_cap.fd == afd && g_src_cap.size == 4096 &&
          g_src_cap.struct_size == sizeof(mb_archive_source_v1) &&
          g_src_cap.abi_version == MB_ARCHIVE_ABI_V1, "source passed verbatim");
    CHECK(g_mr_cap.selected_count == 2 && g_mr_cap.selected[0] == 0 &&
          g_mr_cap.selected[1] == 2, "regular rows selected, dir skipped");
    CHECK(g_mr_cap.journal_capacity >= g_mr_cap.selected_count + 3,
          "journal capacity rule");
    CHECK(g_mr_cap.quarantine_fd == res.quarantine_fd &&
          g_mr_cap.quarantine_fd >= 0, "quarantine fd handed to intake");
    CHECK(memcmp(g_mr_cap.target_token, g_src_cap.target_token, 32) == 0,
          "token consistent source<->request");
    CHECK(res.journal_retained == 1, "journal retained on success");
    CHECK(res.decoded_bytes == 12345 && res.selected_count == 2, "materialized facts");
    CHECK(strcmp(res.manifest_name, "mb-owned-000001") == 0, "manifest name");
    { ps5io_stat qs; CHECK(ps5io_stat_path(qdir, &qs) == 0 && qs.is_dir == 1,
                            "quarantine intact"); }
    CHECK(ps5io_close(res.quarantine_fd) == 0, "caller closes quarantine");

    /* determinism of the target token */
    { ps5_prepare_result r2; g_calls = 0;
      ps5_prepare_archive(&STUB, &req, &r2);
      CHECK(memcmp(r2.target_token, res.target_token, 32) == 0, "token deterministic");
      ps5io_close(r2.quarantine_fd); }

    /* deadline + cancel wiring */
    req.deadline_ms = 7777; req.cancelled = cancel_yes; req.cancel_ctx = 0;
    g_calls = 0;
    memset(&res, 0, sizeof res);
    CHECK(ps5_prepare_archive(&STUB, &req, &res) == -1 &&
          res.status == PS5_PREPARE_E_CANCELLED && g_calls == 0,
          "cancel before scan");
    req.cancelled = cancel_no; /* never fires; proves pointer forwarding */
    memset(&res, 0, sizeof res); g_calls = 0;
    ps5_prepare_archive(&STUB, &req, &res);
    CHECK(g_deadline_cap == 7777 && g_cancel_cap == cancel_no,
          "budget deadline/cancel forwarded");
    ps5io_close(res.quarantine_fd);
    req.deadline_ms = 0; req.cancelled = 0;

    /* scan failure: no materialize, fd reclaimed */
    g_scan_fail = 1; g_calls = 0; memset(&res, 0, sizeof res);
    CHECK(ps5_prepare_archive(&STUB, &req, &res) == -1 &&
          res.status == PS5_PREPARE_E_SCAN && g_calls == 1 &&
          res.quarantine_fd < 0 && res.journal_retained == 0,
          "scan failure stops before materialize");
    g_scan_fail = 0;

    /* inventory says unsupported */
    g_inventory_unsupported = 1; memset(&res, 0, sizeof res); g_calls = 0;
    CHECK(ps5_prepare_archive(&STUB, &req, &res) == -1 &&
          res.status == PS5_PREPARE_E_SCAN && g_calls == 1,
          "unsupported inventory refuses");
    g_inventory_unsupported = 0;

    /* materialize failure: journal retained */
    g_mat_fail = 1; memset(&res, 0, sizeof res); g_calls = 0;
    CHECK(ps5_prepare_archive(&STUB, &req, &res) == -1 &&
          res.status == PS5_PREPARE_E_MATERIALIZE && g_calls == 2 &&
          res.journal_retained == 1 && res.quarantine_fd < 0,
          "materialize failure retains journal");
    g_mat_fail = 0;

    /* bad arguments */
    memset(&res, 0, sizeof res);
    CHECK(ps5_prepare_archive(0, &req, &res) == -1 &&
          res.status == PS5_PREPARE_E_ARGUMENT, "null api");
    { ps5_prepare_request bad; memcpy(&bad, &req, sizeof bad);
      bad.chunk_capacity = 1024; /* below MB_ARCHIVE_CHUNK_BYTES */
      memset(&res, 0, sizeof res);
      CHECK(ps5_prepare_archive(&STUB, &bad, &res) == -1 &&
            res.status == PS5_PREPARE_E_ARGUMENT, "chunk too small");
      memcpy(&bad, &req, sizeof bad);
      bad.archive_fd = -1;
      memset(&res, 0, sizeof res);
      CHECK(ps5_prepare_archive(&STUB, &bad, &res) == -1, "bad fd"); }

    /* selection overruns journal capacity */
    { ps5_prepare_request bad; memcpy(&bad, &req, sizeof bad);
      bad.journal_capacity = 4; /* 2 selected + 3 = 5 > 4 */
      memset(&res, 0, sizeof res); g_calls = 0;
      CHECK(ps5_prepare_archive(&STUB, &bad, &res) == -1 &&
            res.status == PS5_PREPARE_E_SELECT && g_calls == 1,
            "journal overflow refuses before materialize"); }

    /* quarantine missing */
    { ps5_prepare_request bad; memcpy(&bad, &req, sizeof bad);
      bad.quarantine_dir = "/tmp/ps5prep-missing-dir";
      memset(&res, 0, sizeof res);
      CHECK(ps5_prepare_archive(&STUB, &bad, &res) == -1 &&
            res.status == PS5_PREPARE_E_QUARANTINE, "missing quarantine"); }

    /* quarantine is a file, not a dir */
    { ps5_prepare_request bad; memcpy(&bad, &req, sizeof bad);
      bad.quarantine_dir = "/etc/hosts";
      memset(&res, 0, sizeof res);
      CHECK(ps5_prepare_archive(&STUB, &bad, &res) == -1 &&
            res.status == PS5_PREPARE_E_QUARANTINE, "quarantine not a dir"); }

    /* custom selector honored: only rows larger than 15 bytes */
    { static mb_archive_row_v1 seen[8]; static int nseen;
      ps5_prepare_request sel; ps5_prepare_result r3;
      memcpy(&sel, &req, sizeof sel);
      sel.selector = big_only; sel.selector_ctx = seen;
      g_calls = 0; memset(&r3, 0, sizeof r3);
      CHECK(ps5_prepare_archive(&STUB, &sel, &r3) == 0, "selector prepare ok");
      CHECK(r3.selected_count == 1 && g_mr_cap.selected[0] == 2,
            "selector picked only the 20-byte row");
      ps5io_close(r3.quarantine_fd); (void)nseen; }

    ps5io_close(afd);
    { char cmd[400]; snprintf(cmd, sizeof cmd, "rm -rf /tmp/ps5prep-%ld", (long)getpid());
      if (system(cmd)) {} }

    printf("RESULT pass=%d fail=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
