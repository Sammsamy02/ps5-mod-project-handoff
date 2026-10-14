/*
 * modinst_archive_prepare.c — see modinst_archive_prepare.h.
 * Origin: worker-installer-composition, 2026-10-04 (owned).
 *
 * Scratch is fixed static (bounded small build): 2048 rows / 128 KiB names /
 * 64 KiB chunks (the intake chunk size) and an identically sized independent
 * replay set. Archives whose inventory exceeds these capacities are refused
 * honestly (intake limit status / bridge PS5_PREPARE_E_BUFFERS), never
 * truncated silently.
 */
#include "modinst_archive_prepare.h"

#include <stdio.h>
#include <string.h>

#include "intake.h"              /* vendored archive intake ABI v1 */
#include "modforge_bundle.h"     /* vendored mfb_sha256 */
#include "ps5_io.h"
#include "ps5_prepare_bridge.h"  /* vendored, unmodified */
#include "e2e_status.h"

#define MI_ROWS 2048u
#define MI_NAMES (128u * 1024u)
#define MI_CHUNK (64u * 1024u)

/* exit codes matching the composed core's contract */
#define MI_EX_OK 0
#define MI_EX_IO 1
#define MI_EX_BAD 2

static void hex(char *out, const uint8_t *b, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) snprintf(out + 2 * i, 3, "%02x", b[i]);
}

static int sha256_file_ps5io(const char *path, uint8_t out[32], uint64_t *size_out) {
    static unsigned char buf[MI_CHUNK];
    ps5io_stat st;
    mfb_sha256 sha;
    uint64_t off = 0;
    int fd = ps5io_open(path, PS5IO_RDONLY, 0);
    if (fd < 0) return -1;
    if (ps5io_stat_path(path, &st)) { ps5io_close(fd); return -1; }
    mfb_sha256_init(&sha);
    while (off < st.size) {
        size_t want = st.size - off > MI_CHUNK ? MI_CHUNK : (size_t)(st.size - off);
        if (ps5io_pread_exact(fd, off, buf, want)) { ps5io_close(fd); return -1; }
        mfb_sha256_update(&sha, buf, want);
        off += want;
    }
    ps5io_close(fd);
    mfb_sha256_final(&sha, out);
    if (size_out) *size_out = st.size;
    return 0;
}

static void derive_token(uint8_t token[32], const uint8_t archive_sha[32],
                         const char *quarantine) {
    static const char label[] = "ps5-prepare-quarantine-v1";
    mfb_sha256 sha;
    mfb_sha256_init(&sha);
    mfb_sha256_update(&sha, label, sizeof label - 1);
    mfb_sha256_update(&sha, archive_sha, 32);
    mfb_sha256_update(&sha, quarantine, strlen(quarantine));
    mfb_sha256_final(&sha, token);
}

int mi_cmd_prepare_archive(int argc, char **argv) {
    const char *archive = 0, *quarantine = 0, *title = 0;
    int i;
    int afd, qfd, rc;
    uint64_t asize = 0;
    uint8_t asha[32], token[32];
    ps5io_stat qst;
    ps5_prepare_request req;
    ps5_prepare_result res;
    /* the real intake, statically present in both flavors of the composition */
    static const ps5_intake_api api = {
        mb_archive_limits_default_v1,
        mb_archive_budget_default_v1,
        mb_archive_scan_v1,
        mb_archive_materialize_v1
    };
    /* caller scratch per intake.h rules; replay buffers independent */
    static mb_archive_row_v1 rows[MI_ROWS], replay_rows[MI_ROWS];
    static char names[MI_NAMES], replay_names[MI_NAMES];
    static uint32_t sort_idx[MI_ROWS], replay_sort[MI_ROWS], selected[MI_ROWS];
    static unsigned char chunk[MI_CHUNK], replay_chunk[MI_CHUNK];
    static mb_archive_owned_file_v1 journal[MI_ROWS + 3];

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--archive") && i + 1 < argc) archive = argv[++i];
        else if (!strcmp(argv[i], "--quarantine") && i + 1 < argc) quarantine = argv[++i];
        else if (!strcmp(argv[i], "--title") && i + 1 < argc) title = argv[++i];
        /* unknown flags belong to the composed core's loop; ignored here */
    }
    if (!archive || !quarantine) {
        printf("error bad_request prepare-archive needs --archive FILE --quarantine DIR\n");
        e2e_status_event("stage-2-prepare", "bad_request", "missing --archive/--quarantine");
        return MI_EX_BAD;
    }

    if (sha256_file_ps5io(archive, asha, &asize)) {
        printf("error archive_unreadable cannot open/hash %s\n", archive);
        e2e_status_event("stage-2-prepare", "fail", "archive unreadable");
        return MI_EX_BAD;
    }
    derive_token(token, asha, quarantine);

    if (ps5io_mkdir_p(quarantine, 0700)) { /* intake demands mode & 077 == 0 */
        printf("error io_error cannot create quarantine %s\n", quarantine);
        e2e_status_event("stage-2-prepare", "fail", "quarantine mkdir");
        return MI_EX_IO;
    }
    qfd = ps5io_open(quarantine, PS5IO_RDONLY | PS5IO_DIRECTORY, 0);
    if (qfd < 0 || ps5io_stat_path(quarantine, &qst)) {
        printf("error io_error quarantine unusable %s\n", quarantine);
        e2e_status_event("stage-2-prepare", "fail", "quarantine open");
        return MI_EX_IO;
    }
    afd = ps5io_open(archive, PS5IO_RDONLY, 0);
    if (afd < 0) {
        printf("error archive_unreadable cannot reopen %s\n", archive);
        e2e_status_event("stage-2-prepare", "fail", "archive reopen");
        return MI_EX_BAD;
    }

    memset(&req, 0, sizeof req);
    req.archive_fd = afd;
    req.archive_size = asize;
    memcpy(req.archive_sha256, asha, 32);
    req.quarantine_dir = quarantine;
    req.selector = 0;                 /* bridge default: all MB_ARCHIVE_REGULAR */
    req.deadline_ms = 0;
    req.cancelled = 0;
    req.rows = rows;            req.row_capacity = MI_ROWS;
    req.names = names;          req.name_capacity = MI_NAMES;
    req.sort = sort_idx;        req.sort_capacity = MI_ROWS;
    req.chunk = chunk;          req.chunk_capacity = MI_CHUNK;
    req.replay_rows = replay_rows;   req.replay_row_capacity = MI_ROWS;
    req.replay_names = replay_names; req.replay_name_capacity = MI_NAMES;
    req.replay_sort = replay_sort;   req.replay_sort_capacity = MI_ROWS;
    req.replay_chunk = replay_chunk; req.replay_chunk_capacity = MI_CHUNK;
    req.selected = selected;    req.selected_capacity = MI_ROWS;
    req.journal = journal;      req.journal_capacity = MI_ROWS + 3;

    memset(&res, 0, sizeof res);
    rc = ps5_prepare_archive(&api, &req, &res);
    ps5io_close(afd);

    if (rc == 0 && res.status == 0 && res.complete) {
        char sha_hex[65], tok_hex[65];
        hex(sha_hex, res.inventory_sha256, 32); sha_hex[64] = 0;
        hex(tok_hex, res.target_token, 32); tok_hex[64] = 0;
        printf("prepared title=%s quarantine=%s manifest=%s files=%u bytes=%llu "
               "inventory=%s token=%s journal_retained=%u\n",
               title ? title : "-", quarantine, res.manifest_name,
               res.selected_count, (unsigned long long)res.decoded_bytes,
               sha_hex, tok_hex, res.journal_retained);
        e2e_status_event("stage-2-prepare", "ok", res.manifest_name);
        if (res.quarantine_fd >= 0) ps5io_close(res.quarantine_fd);
        else ps5io_close(qfd);
        return MI_EX_OK;
    }
    printf("error requirement-unavailable prepare-archive status=%u complete=%u (bridge rc=%d)\n",
           res.status, res.complete, rc);
    e2e_status_event("stage-2-prepare", "fail", "intake refused");
    /* the intake rule: a surviving journal is never erased by this path */
    if (res.quarantine_fd >= 0) ps5io_close(res.quarantine_fd);
    else ps5io_close(qfd);
    return MI_EX_IO;
}
