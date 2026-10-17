#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "pak_archive_job.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct job_span_v1 { uintptr_t begin, end; } job_span_v1;
typedef struct job_gate_v1 {
    mb_pak_prepare_gate_v1 predicate;
    void *context;
    uint32_t stopped;
} job_gate_v1;

static int gate_stopped(void *context) {
    job_gate_v1 *gate = context;
    if (gate->stopped || (gate->predicate && gate->predicate(gate->context))) {
        gate->stopped = 1u; return 1;
    }
    return 0;
}

static int span(const void *p, size_t size, size_t alignment, job_span_v1 *out) {
    uintptr_t address = (uintptr_t)p;
    if (!p || address % alignment || size > UINTPTR_MAX - address) return 0;
    out->begin = address;
    out->end = address + size;
    return 1;
}

static int disjoint(const job_span_v1 *a, const job_span_v1 *b) {
    return a->end <= b->begin || b->end <= a->begin;
}

static int arguments_valid(const mb_pak_archive_job_params_v1 *p,
                           const mb_pak_archive_job_workspace_v1 *w,
                           const mb_pak_archive_job_result_v1 *r,
                           mb_pak_prepare_gate_v1 gate, void *gate_context) {
    job_span_v1 s[3];
    if (!gate && gate_context) return 0;
    if (!span(p, sizeof(*p), _Alignof(mb_pak_archive_job_params_v1), s) ||
        !span(w, sizeof(*w), _Alignof(mb_pak_archive_job_workspace_v1), s + 1) ||
        !span(r, sizeof(*r), _Alignof(mb_pak_archive_job_result_v1), s + 2) ||
        !disjoint(s, s + 1) || !disjoint(s, s + 2) || !disjoint(s + 1, s + 2))
        return 0;
    if (gate_context) {
        job_span_v1 context;
        if (!span(gate_context, 1u, 1u, &context)) return 0;
        for (unsigned i = 0u; i < 3u; ++i)
            if (!disjoint(&context, s + i)) return 0;
    }
    if (p->struct_size != sizeof(*p) || p->abi_version != MB_PAK_ARCHIVE_JOB_ABI_V1 ||
        w->struct_size != sizeof(*w) || w->abi_version != MB_PAK_ARCHIVE_JOB_ABI_V1 ||
        p->source.struct_size != sizeof(p->source) || p->source.abi_version != MB_ARCHIVE_ABI_V1 ||
        p->source.reserved || p->reserved || p->immutable_inputs != 1u ||
        p->private_quarantine != 1u || p->unpublished_output != 1u ||
        p->source.fd < 0 || p->base_fd < 0 || p->quarantine_fd < 0 ||
        p->source.fd == p->base_fd || p->source.fd == p->quarantine_fd ||
        p->base_fd == p->quarantine_fd || !p->source.size || !p->base_bytes ||
        p->source.size > MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES ||
        p->base_bytes > MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES ||
        (p->policy != MB_PAK_ARCHIVE_POLICY_DEFAULT &&
         p->policy != MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY)) return 0;
    if (p->output_fd >= 0) {
        if (p->acquire_output || p->output_context || p->output_fd == p->source.fd ||
            p->output_fd == p->base_fd || p->output_fd == p->quarantine_fd) return 0;
    } else if (p->output_fd != -1 || !p->acquire_output) return 0;
    const unsigned char *journal_bytes = (const unsigned char *)w->journal;
    for (size_t i = 0; i < sizeof(w->journal); ++i)
        if (journal_bytes[i]) return 0;
    return 1;
}

static int same_inode(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static int same_snapshot(const struct stat *a, const struct stat *b) {
    if (!same_inode(a, b) || a->st_mode != b->st_mode || a->st_size != b->st_size) return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}

static int file_stable(int fd, const struct stat *before) {
    struct stat after;
    return fd >= 0 && fstat(fd, &after) == 0 && S_ISREG(after.st_mode) &&
           same_snapshot(before, &after);
}

static int read_only_regular(int fd, struct stat *st, uint64_t expected) {
    int flags;
    if (fd < 0 || fstat(fd, st) != 0 || !S_ISREG(st->st_mode) || st->st_size <= 0 ||
        (uint64_t)st->st_size != expected || expected > MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES)
        return 0;
    flags = fcntl(fd, F_GETFL);
    return flags >= 0 && (flags & O_ACCMODE) == O_RDONLY;
}

static int private_directory(const struct stat *st) {
    return S_ISDIR(st->st_mode) && st->st_uid == geteuid() &&
           (st->st_mode & 0700) == 0700 && (st->st_mode & 0077) == 0;
}

static int directory_empty(int fd) {
    int copy = dup(fd);
    if (copy < 0) return 0;
    DIR *dir = fdopendir(copy);
    if (!dir) { (void)close(copy); return 0; }
    /* dup shares the directory cursor. Start at the beginning even when the
     * borrowed descriptor was previously iterated; its final cursor is unspecified. */
    rewinddir(dir);
    int empty = 1;
    unsigned seen = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (++seen > 8u || (strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))) {
            empty = 0;
            break;
        }
    }
    if (errno) empty = 0;
    if (closedir(dir) != 0) empty = 0;
    return empty;
}

static int output_empty(int fd, struct stat *st,
                        const struct stat *source, const struct stat *base) {
    int flags = fd >= 0 ? fcntl(fd, F_GETFL) : -1;
    return flags >= 0 && ((flags & O_ACCMODE) == O_WRONLY || (flags & O_ACCMODE) == O_RDWR) &&
           !(flags & O_APPEND) && fstat(fd, st) == 0 && S_ISREG(st->st_mode) &&
           st->st_uid == geteuid() && (st->st_mode & 0077) == 0 &&
           st->st_size == 0 && st->st_nlink == 1 &&
           !same_inode(st, source) && !same_inode(st, base);
}

static const mb_archive_owned_file_v1 *find_owned(
    const mb_pak_archive_job_workspace_v1 *w, uint32_t row, uint32_t count) {
    const mb_archive_owned_file_v1 *found = NULL;
    if (count > MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY) return NULL;
    for (uint32_t i = 0; i < count; ++i) {
        const mb_archive_owned_file_v1 *item = w->journal + i;
        if (item->row_index != row ||
            (item->state != MB_ARCHIVE_CREATED && item->state != MB_ARCHIVE_RETAINED)) continue;
        if (found) return NULL;
        found = item;
    }
    return found;
}

static int member_bound(const mb_pak_archive_member_v1 *member,
                        const mb_archive_owned_file_v1 *owned) {
    char expected[40];
    int n = snprintf(expected, sizeof(expected), "member-%08u.bin", member->row_index);
    return owned && owned->state == MB_ARCHIVE_CREATED &&
           owned->row_index == member->row_index && n > 0 && (size_t)n < sizeof(expected) &&
           memchr(owned->name, '\0', sizeof(owned->name)) && !strcmp(owned->name, expected) &&
           owned->expected_size == member->bytes && owned->written_bytes == member->bytes &&
           !memcmp(owned->sha256, member->sha256, sizeof(member->sha256));
}

static void failure(mb_pak_archive_job_result_v1 *r,
                     mb_pak_archive_job_status_v1 status,
                     mb_pak_archive_job_stage_v1 stage, const char *error) {
    r->status = status;
    r->stage = stage;
    (void)snprintf(r->error, sizeof(r->error), "%s", error);
    r->preparation_complete = 0u;
}

const char *mb_pak_archive_job_stage_name_v1(mb_pak_archive_job_stage_v1 stage) {
    switch (stage) {
        case MB_PAK_ARCHIVE_JOB_STAGE_ARGUMENTS: return "arguments";
        case MB_PAK_ARCHIVE_JOB_STAGE_INPUTS: return "input_open";
        case MB_PAK_ARCHIVE_JOB_STAGE_SCAN: return "archive_scan";
        case MB_PAK_ARCHIVE_JOB_STAGE_SELECTION: return "archive_selection";
        case MB_PAK_ARCHIVE_JOB_STAGE_QUARANTINE: return "private_directories";
        case MB_PAK_ARCHIVE_JOB_STAGE_MATERIALIZATION: return "archive_materialization";
        case MB_PAK_ARCHIVE_JOB_STAGE_MEMBER: return "pak_member_open";
        case MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT: return "output_create";
        case MB_PAK_ARCHIVE_JOB_STAGE_UNION: return "pak_union";
        case MB_PAK_ARCHIVE_JOB_STAGE_SNAPSHOT: return "source_snapshot";
        case MB_PAK_ARCHIVE_JOB_STAGE_CLOSE: return "descriptor_close";
        case MB_PAK_ARCHIVE_JOB_STAGE_COMPLETE: return "complete";
        default: return "invalid_stage";
    }
}

mb_pak_archive_job_status_v1 mb_pak_archive_job_prepare_v1(
    const mb_pak_archive_job_params_v1 *p,
    mb_pak_archive_job_workspace_v1 *w,
    mb_pak_archive_job_result_v1 *r) {
    return mb_pak_archive_job_prepare_gated_v1(p, w, r, NULL, NULL);
}
mb_pak_archive_job_status_v1 mb_pak_archive_job_prepare_gated_v1(
    const mb_pak_archive_job_params_v1 *p,
    mb_pak_archive_job_workspace_v1 *w,
    mb_pak_archive_job_result_v1 *r,
    mb_pak_prepare_gate_v1 predicate, void *gate_context) {
    if (!arguments_valid(p, w, r, predicate, gate_context)) return MB_PAK_ARCHIVE_JOB_BAD_ARGUMENT;

    memset(r, 0, sizeof(*r));
    r->struct_size = sizeof(*r);
    r->abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1;
    r->status = MB_PAK_ARCHIVE_JOB_FD_REFUSED;
    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_INPUTS;
    r->scan_status = r->selection_status = r->materialize_status = r->union_status = UINT32_MAX;
    r->output_fd = p->output_fd;
    mb_archive_limits_v1 limits;
    mb_archive_limits_default_v1(&limits);
    limits.raw_bytes = MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES;
    limits.member_bytes = MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES;
    limits.expanded_bytes = UINT64_C(1) << 30;
    limits.entries = MB_PAK_ARCHIVE_JOB_ROWS;
    limits.name_bytes = MB_PAK_ARCHIVE_JOB_NAME_BYTES;
    mb_archive_budget_default_v1(&r->budget);
    r->budget.io_limit = UINT64_C(8) << 30;
    r->budget.operation_limit = UINT64_C(1000000);
    r->budget.path_work_limit = UINT64_C(16) << 20;
    job_gate_v1 gate = {predicate, gate_context, 0u};
    r->budget.cancelled = predicate ? gate_stopped : NULL;
    r->budget.context = predicate ? &gate : NULL;

    struct stat source_before, base_before, quarantine_before, selected_before, output_before;
    int snapshots_ready = 0, selected_ready = 0, selected_fd = -1;
    int output_ready = 0;
    const mb_pak_archive_member_v1 *pak_member = NULL;
    const mb_archive_owned_file_v1 *pak_owned = NULL;
#define REFUSE(code, at, why) do { failure(r, (code), (at), (why)); goto done; } while (0)
#define CHECK_GATE() do { if (gate_stopped(&gate)) \
    REFUSE(MB_PAK_ARCHIVE_JOB_STOPPED, r->stage, "operation_gate_stopped"); } while (0)

    CHECK_GATE();
    if (!read_only_regular(p->source.fd, &source_before, p->source.size))
        REFUSE(MB_PAK_ARCHIVE_JOB_FD_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_INPUTS,
               "readonly_archive_identity_or_size_refused");
    if (!read_only_regular(p->base_fd, &base_before, p->base_bytes) ||
        same_inode(&source_before, &base_before))
        REFUSE(MB_PAK_ARCHIVE_JOB_FD_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_INPUTS,
               "readonly_distinct_base_identity_or_size_refused");
    snapshots_ready = 1;
    CHECK_GATE();
    if (p->output_fd >= 0) {
        if (!output_empty(p->output_fd, &output_before, &source_before, &base_before))
            REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
                   "empty_private_writable_distinct_output_refused");
        output_ready = 1;
        r->output_acquired = 1u;
    }

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_SCAN;
    CHECK_GATE();
    mb_archive_buffers_v1 scan = {w->rows, MB_PAK_ARCHIVE_JOB_ROWS,
        w->names, MB_PAK_ARCHIVE_JOB_NAME_BYTES, w->scan_sort, MB_PAK_ARCHIVE_JOB_ROWS,
        w->transfer, sizeof(w->transfer)};
    r->scan_status = (uint32_t)mb_archive_scan_v1(&p->source, &limits, &r->budget,
                                                 &scan, &r->inventory);
    CHECK_GATE();
    if (r->scan_status != MB_ARCHIVE_OK || r->inventory.complete != 1u ||
        r->inventory.installation_supported)
        REFUSE(MB_PAK_ARCHIVE_JOB_SCAN_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_SCAN, "scan_incomplete");
    if (!file_stable(p->source.fd, &source_before) ||
        r->inventory.source_size != p->source.size ||
        memcmp(r->inventory.source_sha256, p->source.sha256, 32u))
        REFUSE(MB_PAK_ARCHIVE_JOB_SOURCE_CHANGED, MB_PAK_ARCHIVE_JOB_STAGE_SCAN,
               "source_identity_changed");

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_SELECTION;
    CHECK_GATE();
    mb_archive_prepared_view_v1 view = {&r->inventory, w->rows, MB_PAK_ARCHIVE_JOB_ROWS,
        w->names, MB_PAK_ARCHIVE_JOB_NAME_BYTES};
    mb_archive_prepared_workspace_v1 grouping = {w->group_sort, MB_PAK_ARCHIVE_JOB_ROWS,
        w->groups, MB_PAK_ARCHIVE_JOB_ROWS, w->group_members, MB_PAK_ARCHIVE_JOB_ROWS,
        w->group_keys, MB_PAK_ARCHIVE_JOB_NAME_BYTES};
    r->selection_called = 1u;
    r->selection_status = (uint32_t)mb_pak_archive_select_v1(&view,
        (mb_pak_archive_policy_v1)p->policy, &grouping,
        w->selected, MB_PAK_ARCHIVE_JOB_ROWS, w->members, MB_PAK_ARCHIVE_JOB_ROWS,
        &r->selection);
    CHECK_GATE();
    if (r->selection_status == MB_PAK_ARCHIVE_ADDITIONAL_ACTION_REQUIRED)
        REFUSE(MB_PAK_ARCHIVE_JOB_ADDITIONAL_ACTION_REQUIRED, MB_PAK_ARCHIVE_JOB_STAGE_SELECTION,
               "additional_regular_members_need_explicit_policy");
    if (r->selection_status != MB_PAK_ARCHIVE_OK || !r->selection.selection_complete ||
        !r->selection.preparation_ready || !r->selection.member_list_complete ||
        !r->selection.member_count || r->selection.member_count > MB_PAK_ARCHIVE_JOB_ROWS ||
        r->selection.pak_row >= r->inventory.row_count ||
        memcmp(r->selection.source_sha256, p->source.sha256, 32u) ||
        memcmp(r->selection.inventory_sha256, r->inventory.inventory_sha256, 32u) ||
        memcmp(r->selection.target_token, p->source.target_token, 32u) ||
        r->selection.installation_supported || r->selection.runtime_supported ||
        r->selection.platform_compatible || r->selection.format_validated)
        REFUSE(MB_PAK_ARCHIVE_JOB_SELECTION_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_SELECTION,
               "selection_refused_or_identity_mismatch");

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_QUARANTINE;
    CHECK_GATE();
    if (fstat(p->quarantine_fd, &quarantine_before) || !private_directory(&quarantine_before) ||
        !directory_empty(p->quarantine_fd))
        REFUSE(MB_PAK_ARCHIVE_JOB_FD_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_QUARANTINE,
               "private_empty_quarantine_refused");

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_MATERIALIZATION;
    CHECK_GATE();
    mb_archive_materialize_request_v1 materialize = {0};
    materialize.source = &p->source;
    materialize.inventory = &r->inventory;
    materialize.rows = w->rows;
    materialize.row_capacity = MB_PAK_ARCHIVE_JOB_ROWS;
    materialize.names = w->names;
    materialize.name_capacity = MB_PAK_ARCHIVE_JOB_NAME_BYTES;
    materialize.selected = w->selected;
    materialize.selected_count = r->selection.member_count;
    memcpy(materialize.target_token, p->source.target_token, 32u);
    materialize.quarantine_fd = p->quarantine_fd;
    materialize.quarantine_device = (uint64_t)quarantine_before.st_dev;
    materialize.quarantine_inode = (uint64_t)quarantine_before.st_ino;
    materialize.journal = w->journal;
    materialize.journal_capacity = MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY;
    materialize.replay = (mb_archive_buffers_v1){w->replay_rows, MB_PAK_ARCHIVE_JOB_ROWS,
        w->replay_names, MB_PAK_ARCHIVE_JOB_NAME_BYTES, w->replay_sort, MB_PAK_ARCHIVE_JOB_ROWS,
        w->replay_transfer, sizeof(w->replay_transfer)};
    r->materialize_status = (uint32_t)mb_archive_materialize_v1(&materialize, &limits,
                                                               &r->budget, &r->materialized);
    CHECK_GATE();
    if (r->materialize_status != MB_ARCHIVE_OK || r->materialized.complete != 1u ||
        r->materialized.installation_supported ||
        r->materialized.selected_count != r->selection.member_count ||
        r->materialized.journal_count > MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY ||
        memcmp(r->materialized.inventory_sha256, r->inventory.inventory_sha256, 32u) ||
        memcmp(r->materialized.target_token, p->source.target_token, 32u))
        REFUSE(MB_PAK_ARCHIVE_JOB_MATERIALIZATION_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_MATERIALIZATION,
               "materialization_incomplete");
    r->materialization_complete = 1u;
    for (uint32_t i = 0; i < r->selection.member_count; ++i) {
        CHECK_GATE();
        const mb_archive_owned_file_v1 *owned = find_owned(w, w->members[i].row_index,
                                                          r->materialized.journal_count);
        if (!member_bound(w->members + i, owned))
            REFUSE(MB_PAK_ARCHIVE_JOB_MEMBER_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_MATERIALIZATION,
                   "materialized_journal_binding_failed");
        if (w->members[i].row_index == r->selection.pak_row) {
            if (pak_member)
                REFUSE(MB_PAK_ARCHIVE_JOB_MEMBER_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_MEMBER,
                       "selected_pak_not_uniquely_bound");
            pak_member = w->members + i;
            pak_owned = owned;
        }
    }

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_MEMBER;
    CHECK_GATE();
    if (!pak_member || !member_bound(pak_member, pak_owned))
        REFUSE(MB_PAK_ARCHIVE_JOB_MEMBER_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_MEMBER,
               "selected_pak_not_uniquely_bound");
    r->selected_pak_bytes = pak_member->bytes;
    selected_fd = openat(p->quarantine_fd, pak_owned->name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (!read_only_regular(selected_fd, &selected_before, pak_member->bytes) ||
        (uint64_t)selected_before.st_dev != pak_owned->device ||
        (uint64_t)selected_before.st_ino != pak_owned->inode ||
        same_inode(&selected_before, &source_before) || same_inode(&selected_before, &base_before))
        REFUSE(MB_PAK_ARCHIVE_JOB_MEMBER_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_MEMBER,
               "selected_pak_identity_refused");
    selected_ready = 1;
    CHECK_GATE();

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT;
    CHECK_GATE();
    if (p->output_fd == -1) {
        int acquired = -1;
        int callback_status = p->acquire_output(p->output_context, &acquired);
        r->output_fd = acquired;
        r->output_acquired = acquired >= 0 ? 1u : 0u;
        CHECK_GATE();
        if (callback_status || acquired < 0)
            REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
                   "output_acquisition_failed");
        if (!output_empty(acquired, &output_before, &source_before, &base_before))
            REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
                   "empty_private_writable_distinct_output_refused");
        output_ready = 1;
    } else {
        struct stat now;
        if (!output_empty(r->output_fd, &now, &source_before, &base_before) ||
            !same_snapshot(&output_before, &now))
            REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
                   "provided_output_snapshot_changed");
    }
    if (!output_ready || same_inode(&output_before, &selected_before))
        REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
               "output_aliases_selected_member");
    /* No materialized member or recovery-control inode can become output,
     * including a zero-byte retained extra supplied by a trusted callback. */
    for (uint32_t i = 0; i < r->materialized.journal_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        if ((uint64_t)output_before.st_dev == w->journal[i].device &&
            (uint64_t)output_before.st_ino == w->journal[i].inode)
            REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_OUTPUT,
                   "output_aliases_materialized_journal");
    }
    if (!file_stable(p->source.fd, &source_before) || !file_stable(p->base_fd, &base_before) ||
        !file_stable(selected_fd, &selected_before))
        REFUSE(MB_PAK_ARCHIVE_JOB_SOURCE_CHANGED, MB_PAK_ARCHIVE_JOB_STAGE_SNAPSHOT,
               "input_snapshot_changed_before_union");

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_UNION;
    CHECK_GATE();
    mb_pak_union_params_v1 merge = {0};
    merge.struct_size = sizeof(merge);
    merge.abi_version = MB_PAK_UNION_ABI_V1;
    merge.base_fd = p->base_fd;
    merge.mod_fd = selected_fd;
    merge.output_fd = r->output_fd;
    merge.immutable_inputs = 1u;
    merge.unpublished_output = 1u;
    merge.base_bytes = p->base_bytes;
    merge.mod_bytes = pak_member->bytes;
    r->union_status = (uint32_t)mb_pak_union_prepare_gated_v1(&merge, &r->union_result,
        predicate ? gate_stopped : NULL, predicate ? &gate : NULL);
    CHECK_GATE();
    if (r->union_status != MB_PAK_UNION_OK || r->union_result.status != MB_PAK_UNION_OK ||
        r->union_result.archive_complete != 1u || r->union_result.source_snapshot_stable != 1u ||
        r->union_result.output_size_valid != 1u ||
        r->union_result.observed_output_bytes != r->union_result.archive_bytes ||
        !r->union_result.archive_bytes)
        REFUSE(MB_PAK_ARCHIVE_JOB_UNION_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_UNION,
               "pak_union_incomplete");

    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_SNAPSHOT;
    CHECK_GATE();
    struct stat output_after, quarantine_after;
    if (fstat(r->output_fd, &output_after) || !S_ISREG(output_after.st_mode) ||
        output_after.st_uid != geteuid() || (output_after.st_mode & 0077) ||
        output_after.st_nlink != 1 || output_after.st_size < 0 ||
        (uint64_t)output_after.st_size != r->union_result.archive_bytes ||
        !same_inode(&output_before, &output_after) ||
        fstat(p->quarantine_fd, &quarantine_after) || !private_directory(&quarantine_after) ||
        !same_inode(&quarantine_before, &quarantine_after))
        REFUSE(MB_PAK_ARCHIVE_JOB_OUTPUT_REFUSED, MB_PAK_ARCHIVE_JOB_STAGE_SNAPSHOT,
               "output_or_quarantine_snapshot_check_failed");
    r->status = MB_PAK_ARCHIVE_JOB_OK;
    r->stage = MB_PAK_ARCHIVE_JOB_STAGE_COMPLETE;

done:
    if (snapshots_ready) {
        r->source_stable = file_stable(p->source.fd, &source_before) &&
            file_stable(p->base_fd, &base_before) &&
            (!selected_ready || file_stable(selected_fd, &selected_before));
        if (!r->source_stable && r->status == MB_PAK_ARCHIVE_JOB_OK)
            failure(r, MB_PAK_ARCHIVE_JOB_SOURCE_CHANGED, MB_PAK_ARCHIVE_JOB_STAGE_SNAPSHOT,
                    "input_snapshot_changed_after_union");
    }
    if (selected_fd >= 0 && close(selected_fd) != 0 && r->status == MB_PAK_ARCHIVE_JOB_OK)
        failure(r, MB_PAK_ARCHIVE_JOB_CLOSE_FAILED, MB_PAK_ARCHIVE_JOB_STAGE_CLOSE,
                "selected_member_close_failed");
    /* The archive budget borrowed our stack latch only during execution.
     * Returned budget diagnostics must not retain its callback/context. */
    r->budget.cancelled = NULL;
    r->budget.context = NULL;
    if (gate_stopped(&gate)) {
        failure(r, MB_PAK_ARCHIVE_JOB_STOPPED, r->stage, "operation_gate_stopped");
        r->union_result.archive_complete = 0u;
        r->union_result.output_size_valid = 0u;
        r->union_result.archive_bytes = 0u;
        r->union_result.chosen_payloads_sha1_checked = 0u;
        r->union_result.chosen_base_payloads_sha1_checked = 0u;
        r->union_result.chosen_mod_payloads_sha1_checked = 0u;
        r->union_result.writer.archive_complete = 0u;
        r->union_result.writer.archive_bytes = 0u;
        r->union_result.writer.file_count = 0u;
        r->union_result.writer.directory_count = 0u;
        memset(r->union_result.writer.primary_sha1, 0, sizeof(r->union_result.writer.primary_sha1));
        memset(r->union_result.writer.path_index_sha1, 0, sizeof(r->union_result.writer.path_index_sha1));
        memset(r->union_result.writer.directory_sha1, 0, sizeof(r->union_result.writer.directory_sha1));
    }
    r->preparation_complete = r->status == MB_PAK_ARCHIVE_JOB_OK ? 1u : 0u;
#undef CHECK_GATE
#undef REFUSE
    return r->status;
}
