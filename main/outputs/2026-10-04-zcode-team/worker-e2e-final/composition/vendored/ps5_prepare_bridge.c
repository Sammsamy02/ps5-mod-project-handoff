/*
 * ps5_prepare_bridge.c — implementation of ps5_prepare_bridge.h.
 *
 * Origin: worker-installer-ps5io, 2026-10-04. No libc allocation; every
 * buffer is caller-supplied per intake.h; the target token is derived with
 * the vendored mfb_sha256 (no new crypto owner, no new dependency). The only
 * undefined symbols are ps5io_* plus compiler-emitted memset (nm-verified
 * against the pinned libc; see receipts/console-build.log).
 */
#include "ps5_prepare_bridge.h"

#include "ps5_io.h"

#include <errno.h>
#include <string.h>

static void copy_bytes(void *dst, const void *src, size_t n)
{
    size_t i;
    char *d = (char *)dst;
    const char *s = (const char *)src;
    for (i = 0; i < n; ++i) d[i] = s[i];
}

static void zero_bytes(void *dst, size_t n)
{
    size_t i;
    char *d = (char *)dst;
    for (i = 0; i < n; ++i) d[i] = 0;
}

int ps5_prepare_select_regular(void *ctx, const mb_archive_row_v1 *row,
                               const char *name)
{
    (void)ctx; (void)name;
    return (row && row->kind == MB_ARCHIVE_REGULAR) ? 1 : 0;
}

/* target token = SHA256("ps5io-prepare-target-v1" || archive sha256):
 * binds the quarantine manifest to this exact downloaded artifact. */
static void derive_target_token(const uint8_t archive_sha[32], uint8_t out[32])
{
    static const char label[] = "ps5io-prepare-target-v1";
    mfb_sha256 sha;
    unsigned char digest[32];
    int i;
    mfb_sha256_init(&sha);
    mfb_sha256_update(&sha, label, sizeof label - 1);
    mfb_sha256_update(&sha, archive_sha, 32);
    mfb_sha256_final(&sha, digest);
    for (i = 0; i < 32; ++i) out[i] = digest[i];
}

static int cancel_fired(const ps5_prepare_request *req)
{
    return req->cancelled && req->cancelled(req->cancel_ctx);
}

static int validate_request(const ps5_intake_api *api,
                            const ps5_prepare_request *req,
                            const ps5_prepare_result *out)
{
    if (!api || !req || !out) return 0;
    if (!api->limits_default || !api->budget_default || !api->scan ||
        !api->materialize)
        return 0;
    if (req->archive_fd < 0) return 0;
    if (req->archive_size == 0 || req->archive_size > MB_ARCHIVE_MAX_RAW_BYTES) return 0;
    if (!req->quarantine_dir || !req->quarantine_dir[0]) return 0;
    if (!req->rows || !req->names || !req->sort || !req->chunk) return 0;
    if (req->row_capacity == 0 || req->row_capacity > MB_ARCHIVE_MAX_ENTRIES) return 0;
    if (req->name_capacity == 0) return 0;
    if (req->sort_capacity < req->row_capacity) return 0;
    if (req->chunk_capacity < MB_ARCHIVE_CHUNK_BYTES) return 0;
    if (!req->replay_rows || !req->replay_names || !req->replay_sort ||
        !req->replay_chunk)
        return 0;
    if (req->replay_row_capacity == 0 || req->replay_name_capacity == 0) return 0;
    if (req->replay_sort_capacity < req->replay_row_capacity) return 0;
    if (req->replay_chunk_capacity < MB_ARCHIVE_CHUNK_BYTES) return 0;
    if (!req->journal || req->journal_capacity < 4) return 0;
    if (!req->selected || req->selected_capacity == 0) return 0;
    return 1;
}

int ps5_prepare_archive(const ps5_intake_api *api,
                        const ps5_prepare_request *req,
                        ps5_prepare_result *out)
{
    mb_archive_source_v1 source;
    mb_archive_inventory_v1 inventory;
    mb_archive_materialized_v1 materialized;
    mb_archive_materialize_request_v1 mr;
    mb_archive_limits_v1 limits;
    mb_archive_budget_v1 budget;
    mb_archive_buffers_v1 scan_buffers, replay_buffers;
    ps5io_stat qstat;
    uint32_t selected_count = 0, i;
    int qfd, rc;

    zero_bytes(out, sizeof *out);
    out->quarantine_fd = -1;

    if (!validate_request(api, req, out)) {
        out->status = PS5_PREPARE_E_ARGUMENT;
        return -1;
    }

    /* Quarantine directory: must exist (the owner created it at download
     * acceptance); hand intake its descriptor + device/inode identity. */
    rc = ps5io_stat_path(req->quarantine_dir, &qstat);
    if (rc != 0 || qstat.is_dir != 1) {
        out->status = PS5_PREPARE_E_QUARANTINE;
        return -1;
    }
    qfd = ps5io_open(req->quarantine_dir, PS5IO_RDONLY | PS5IO_DIRECTORY, 0);
    if (qfd < 0) {
        out->status = PS5_PREPARE_E_QUARANTINE;
        return -1;
    }
    out->quarantine_fd = qfd;
    out->quarantine_device = qstat.device;
    out->quarantine_inode = qstat.inode;

    zero_bytes(&source, sizeof source);
    source.struct_size = (uint32_t)sizeof source;
    source.abi_version = MB_ARCHIVE_ABI_V1;
    source.fd = req->archive_fd;
    source.size = req->archive_size;
    copy_bytes(source.sha256, req->archive_sha256, 32);
    derive_target_token(req->archive_sha256, source.target_token);
    copy_bytes(out->target_token, source.target_token, 32);

    api->limits_default(&limits);
    api->budget_default(&budget);
    budget.deadline_ms = req->deadline_ms;
    if (req->cancelled) {
        budget.cancelled = req->cancelled;   /* same int(*)(void*) shape */
        budget.context = req->cancel_ctx;
    }

    if (cancel_fired(req)) {
        out->status = PS5_PREPARE_E_CANCELLED;
        ps5io_close(qfd);
        out->quarantine_fd = -1;
        return -1;
    }

    zero_bytes(&scan_buffers, sizeof scan_buffers);
    scan_buffers.rows = req->rows;
    scan_buffers.row_capacity = req->row_capacity;
    scan_buffers.names = req->names;
    scan_buffers.name_capacity = req->name_capacity;
    scan_buffers.sort_indices = req->sort;
    scan_buffers.sort_capacity = req->sort_capacity;
    scan_buffers.chunk = req->chunk;
    scan_buffers.chunk_capacity = req->chunk_capacity;

    zero_bytes(&inventory, sizeof inventory);
    inventory.struct_size = (uint32_t)sizeof inventory;
    inventory.abi_version = MB_ARCHIVE_ABI_V1;

    rc = api->scan(&source, &limits, &budget, &scan_buffers, &inventory);
    if (rc != 0 || !inventory.complete) {
        out->status = rc != 0 ? PS5_PREPARE_E_SCAN : PS5_PREPARE_E_SELECT;
        out->complete = inventory.complete;
        copy_bytes(out->inventory_sha256, inventory.inventory_sha256, 32);
        ps5io_close(qfd);
        out->quarantine_fd = -1;
        return -1;
    }
    copy_bytes(out->inventory_sha256, inventory.inventory_sha256, 32);
    /* composition correction (recorded patch, worker-installer-composition):
     * intake ABI v1 keeps inventory.installation_supported reserved-zero —
     * archive/groups.c rejects inventories where it is nonzero and
     * archive/tests/corpus_test.c asserts ==0 — so this vendored gate was
     * inverted against the real 3.8.7 closure and always refused. The
     * installation decision belongs to the caller's row selection (and the
     * groups stage), not this reserved field. */

    /* Selection: default all regular rows; never exceed journal capacity
     * (intake demands journal_capacity >= selected_count + 3). */
    for (i = 0; i < inventory.row_count && i < req->row_capacity; ++i) {
        const mb_archive_row_v1 *row = &req->rows[i];
        const char *name = req->names + row->path_offset;
        int take = req->selector ? req->selector(req->selector_ctx, row, name)
                                 : ps5_prepare_select_regular(0, row, name);
        if (!take) continue;
        /* post-admission count must satisfy selected_count + 3 <= capacity */
        if (selected_count >= req->selected_capacity ||
            selected_count + 4 > req->journal_capacity) {
            out->status = PS5_PREPARE_E_SELECT;
            out->journal_retained = 0; /* nothing admitted yet */
            ps5io_close(qfd);
            out->quarantine_fd = -1;
            return -1;
        }
        req->selected[selected_count++] = i;
    }
    if (selected_count == 0) {
        out->status = PS5_PREPARE_E_SELECT;
        ps5io_close(qfd);
        out->quarantine_fd = -1;
        return -1;
    }

    zero_bytes(&replay_buffers, sizeof replay_buffers);
    replay_buffers.rows = req->replay_rows;
    replay_buffers.row_capacity = req->replay_row_capacity;
    replay_buffers.names = req->replay_names;
    replay_buffers.name_capacity = req->replay_name_capacity;
    replay_buffers.sort_indices = req->replay_sort;
    replay_buffers.sort_capacity = req->replay_sort_capacity;
    replay_buffers.chunk = req->replay_chunk;
    replay_buffers.chunk_capacity = req->replay_chunk_capacity;

    zero_bytes(&mr, sizeof mr);
    mr.source = &source;
    mr.inventory = &inventory;
    mr.rows = req->rows;
    mr.row_capacity = req->row_capacity;
    mr.names = req->names;
    mr.name_capacity = req->name_capacity;
    mr.selected = req->selected;
    mr.selected_count = selected_count;
    copy_bytes(mr.target_token, source.target_token, 32);
    mr.quarantine_fd = qfd;
    mr.quarantine_device = qstat.device;
    mr.quarantine_inode = qstat.inode;
    mr.journal = req->journal;
    mr.journal_capacity = req->journal_capacity;
    mr.replay = replay_buffers;

    zero_bytes(req->journal,
               (size_t)req->journal_capacity * sizeof *req->journal);
    zero_bytes(&materialized, sizeof materialized);
    materialized.struct_size = (uint32_t)sizeof materialized;
    materialized.abi_version = MB_ARCHIVE_ABI_V1;

    if (cancel_fired(req)) {
        out->status = PS5_PREPARE_E_CANCELLED;
        ps5io_close(qfd);
        out->quarantine_fd = -1;
        return -1;
    }

    rc = api->materialize(&mr, &limits, &budget, &materialized);
    if (rc != 0 || !materialized.complete) {
        out->status = PS5_PREPARE_E_MATERIALIZE;
        out->complete = materialized.complete;
        out->decoded_bytes = materialized.decoded_bytes;
        out->journal_retained = 1; /* a journal may have been admitted */
        ps5io_close(qfd);
        out->quarantine_fd = -1;
        return -1;
    }

    out->status = 0;
    out->complete = 1;
    out->selected_count = selected_count;
    out->decoded_bytes = materialized.decoded_bytes;
    copy_bytes(out->inventory_sha256, materialized.inventory_sha256, 32);
    copy_bytes(out->target_token, materialized.target_token, 32);
    {
        size_t j, mn = sizeof out->manifest_name;
        for (j = 0; j < mn; ++j) out->manifest_name[j] = materialized.manifest_name[j];
    }
    out->journal_retained = 1; /* journal lives on; caller exports/retains */
    /* quarantine fd stays open on success (intake leaves it to the caller) */
    return 0;
}
