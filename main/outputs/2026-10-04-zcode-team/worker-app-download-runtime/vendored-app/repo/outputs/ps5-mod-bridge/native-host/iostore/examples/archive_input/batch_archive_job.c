#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "archive_selection.h"
#include "original_file_job.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ROWS 512u
#define NAMES 262144u
#define SOURCE_CAP MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES
#define MEMBER_CAP (MB_IOSTORE_ARCHIVE_MAX_PROBES + 2u)
#define JOURNAL_CAP (MEMBER_CAP + 3u)

typedef struct retained_source {
    const char *path;
    uint64_t expected_bytes;
    uint8_t expected_sha256[32];
    int archive_fd, quarantine_fd, toc_fd, ucas_fd;
    struct stat archive_snapshot, quarantine_snapshot;
    mb_archive_inventory_v1 inventory;
    mb_iostore_archive_selection_v1 selection;
    mb_archive_materialized_v1 materialized;
    uint32_t selected_count, selected_rows[MEMBER_CAP];
    mb_iostore_archive_member_v1 members[MEMBER_CAP];
    uint32_t journal_count;
    mb_archive_owned_file_v1 journal[JOURNAL_CAP];
} retained_source;

static mb_archive_row_v1 rows[ROWS], replay_rows[ROWS];
static char names[NAMES], replay_names[NAMES], group_keys[NAMES];
static uint32_t sort_rows[ROWS], replay_sort[ROWS], group_sort[ROWS], selected[ROWS];
static uint32_t selected_before[MEMBER_CAP];
static uint8_t transfer[65536], replay_transfer[65536], probe_scratch[65536], primary_scratch[65536];
static mb_archive_group_v1 groups[ROWS];
static mb_archive_group_member_v1 group_members[ROWS];
static mb_iostore_archive_member_v1 members[ROWS];
static mb_archive_owned_file_v1 journal[JOURNAL_CAP];
static retained_source retained[SOURCE_CAP];
static int current_fds[MEMBER_CAP];
static uint32_t current_rows[MEMBER_CAP], current_fd_count;

static int decimal(const char *s, uint64_t *out) {
    if (!s || !*s) return 0;
    uint64_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        if (*p < '0' || *p > '9' || n > (UINT64_MAX - (*p - '0')) / 10) return 0;
        n = n * 10 + (*p - '0');
    }
    *out = n;
    return 1;
}

static int unhex(const char *s, uint8_t *out) {
    if (!s || strlen(s) != 64) return 0;
    for (unsigned i = 0; i < 32; ++i) {
        unsigned hi, lo;
        unsigned char a = (unsigned char)s[i * 2], b = (unsigned char)s[i * 2 + 1];
        hi = a >= '0' && a <= '9' ? a - '0' : a >= 'a' && a <= 'f' ? a - 'a' + 10 : 99;
        lo = b >= '0' && b <= '9' ? b - '0' : b >= 'a' && b <= 'f' ? b - 'a' + 10 : 99;
        if (hi > 15 || lo > 15) return 0;
        out[i] = (uint8_t)(hi * 16 + lo);
    }
    return 1;
}

static void hex(const uint8_t *p, char out[65]) {
    static const char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32; ++i) {
        out[i * 2] = digits[p[i] >> 4];
        out[i * 2 + 1] = digits[p[i] & 15];
    }
    out[64] = '\0';
}

static int same_inode(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static int stable_file(const struct stat *a, const struct stat *b) {
    if (!same_inode(a, b) || a->st_mode != b->st_mode || a->st_size != b->st_size) return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}

static int directory_private_empty(int fd, struct stat *snapshot) {
    if (fstat(fd, snapshot) || !S_ISDIR(snapshot->st_mode) || snapshot->st_uid != geteuid() ||
        (snapshot->st_mode & 07777) != 0700) return 0;
    int copy = openat(fd, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (copy < 0) return 0;
    DIR *dir = fdopendir(copy);
    if (!dir) {
        (void)close(copy);
        return 0;
    }
    int empty = 1;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            empty = 0;
            break;
        }
    }
    int read_error = errno;
    int close_error = closedir(dir);
    return empty && !read_error && !close_error;
}

static int read_all(int fd, uint8_t *out, size_t bytes) {
    size_t at = 0;
    unsigned calls = 0;
    while (at < bytes) {
        if (++calls > 65536) return 0;
        size_t wanted = bytes - at;
        if (wanted > 65536) wanted = 65536;
        ssize_t n = pread(fd, out + at, wanted, (off_t)at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0 || (size_t)n > wanted) return 0;
        at += (size_t)n;
    }
    return 1;
}

static const mb_archive_owned_file_v1 *owned_member(uint32_t row, uint32_t count) {
    const mb_archive_owned_file_v1 *found = NULL;
    for (uint32_t i = 0; i < count; ++i) {
        if (journal[i].row_index == row && journal[i].state == MB_ARCHIVE_CREATED) {
            if (found) return NULL;
            found = journal + i;
        }
    }
    return found;
}

static int row_fd(uint32_t row) {
    for (uint32_t i = 0; i < current_fd_count; ++i)
        if (current_rows[i] == row) return current_fds[i];
    return -1;
}

static void reset_scan_storage(void) {
    memset(rows, 0, sizeof(rows));
    memset(replay_rows, 0, sizeof(replay_rows));
    memset(names, 0, sizeof(names));
    memset(replay_names, 0, sizeof(replay_names));
    memset(group_keys, 0, sizeof(group_keys));
    memset(sort_rows, 0, sizeof(sort_rows));
    memset(replay_sort, 0, sizeof(replay_sort));
    memset(group_sort, 0, sizeof(group_sort));
    memset(selected, 0, sizeof(selected));
    memset(selected_before, 0, sizeof(selected_before));
    memset(transfer, 0, sizeof(transfer));
    memset(replay_transfer, 0, sizeof(replay_transfer));
    memset(probe_scratch, 0, sizeof(probe_scratch));
    memset(primary_scratch, 0, sizeof(primary_scratch));
    memset(groups, 0, sizeof(groups));
    memset(group_members, 0, sizeof(group_members));
    memset(members, 0, sizeof(members));
    memset(journal, 0, sizeof(journal));
}

static void close_current_fds(int *close_status) {
    for (uint32_t i = 0; i < current_fd_count; ++i) {
        if (current_fds[i] >= 0 && close(current_fds[i]) && !*close_status) *close_status = 1;
        current_fds[i] = -1;
    }
    current_fd_count = 0;
}

#define REQUIRE(condition, label) do { \
    if (!(condition)) { fprintf(stderr, "%s; errno=%d\n", label, errno); goto done; } \
} while (0)

int main(int argc, char **argv) {
    if (argc < 10 || ((argc - 6) % 4) != 0) {
        fprintf(stderr, "usage: batch_archive_job BASE_TOC BASE_HEADER BASE_UCAS_BYTES CONTEXT_TOKEN PRIVATE_OUTPUT ARCHIVE SHA256 BYTES PRIVATE_QUARANTINE [... up to 8 sources]\n");
        return 2;
    }
    const uint32_t source_count = (uint32_t)((argc - 6) / 4);
    if (!source_count || source_count > SOURCE_CAP) {
        fprintf(stderr, "source count must be 1..%u\n", SOURCE_CAP);
        return 2;
    }

    int base_toc_fd = -1, base_header_fd = -1, output_fd = -1;
    int rc = 1, close_status = 0, job_ready = 0;
    uint64_t base_ucas_bytes = 0;
    uint8_t context_token[32];
    mb_iostore_batch_source_v1 batch_sources[SOURCE_CAP];
    mb_iostore_batch_file_job_result_v1 batch_result;
    struct stat base_toc_stat, base_header_stat, output_stat;
    struct stat base_toc_after, base_header_after;
    mb_archive_source_v1 scan_source = { .struct_size = sizeof(scan_source), .abi_version = 1 };
    mb_archive_limits_v1 limits;
    mb_archive_budget_v1 budget;
    mb_archive_buffers_v1 scan;
    mb_archive_inventory_v1 inventory;
    mb_iostore_archive_workspace_v1 workspace;
    mb_iostore_archive_selection_v1 selection;
    mb_archive_materialized_v1 materialized;
    mb_iostore_archive_probe_v1 probes[MB_IOSTORE_ARCHIVE_MAX_PROBES];
    uint32_t probe_count = 0;

    for (uint32_t i = 0; i < SOURCE_CAP; ++i) {
        retained[i].archive_fd = retained[i].quarantine_fd = retained[i].toc_fd = retained[i].ucas_fd = -1;
        batch_sources[i] = (mb_iostore_batch_source_v1){-1, -1};
    }
    for (uint32_t i = 0; i < MEMBER_CAP; ++i) current_fds[i] = -1;
    memset(probes, 0, sizeof(probes));
    memset(&batch_result, 0, sizeof(batch_result));

    REQUIRE(decimal(argv[3], &base_ucas_bytes) && base_ucas_bytes &&
            unhex(argv[4], context_token), "invalid base size or context token");
    base_toc_fd = open(argv[1], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    base_header_fd = open(argv[2], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    output_fd = open(argv[5], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    REQUIRE(base_toc_fd >= 0 && base_header_fd >= 0 && output_fd >= 0 &&
            !fstat(base_toc_fd, &base_toc_stat) && S_ISREG(base_toc_stat.st_mode) && base_toc_stat.st_size > 0 &&
            !fstat(base_header_fd, &base_header_stat) && S_ISREG(base_header_stat.st_mode) && base_header_stat.st_size > 0 &&
            !same_inode(&base_toc_stat, &base_header_stat) && directory_private_empty(output_fd, &output_stat),
            "base inputs or empty private output directory refused");

    for (uint32_t i = 0; i < source_count; ++i) {
        const int arg = 6 + (int)i * 4;
        retained[i].path = argv[arg];
        if (!unhex(argv[arg + 1], retained[i].expected_sha256) ||
            !decimal(argv[arg + 2], &retained[i].expected_bytes) || !retained[i].expected_bytes ||
            retained[i].expected_bytes > (UINT64_C(512) << 20)) {
            fprintf(stderr, "invalid archive observation at source %u\n", i);
            goto done;
        }
        retained[i].archive_fd = open(retained[i].path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        retained[i].quarantine_fd = open(argv[arg + 3], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        REQUIRE(retained[i].archive_fd >= 0 && retained[i].quarantine_fd >= 0 &&
                !fstat(retained[i].archive_fd, &retained[i].archive_snapshot) &&
                S_ISREG(retained[i].archive_snapshot.st_mode) && retained[i].archive_snapshot.st_size > 0 &&
                (uint64_t)retained[i].archive_snapshot.st_size == retained[i].expected_bytes &&
                directory_private_empty(retained[i].quarantine_fd, &retained[i].quarantine_snapshot),
                "archive or empty private quarantine refused");
        for (uint32_t j = 0; j < i; ++j) {
            struct stat other_archive;
            REQUIRE(!fstat(retained[j].archive_fd, &other_archive) &&
                    !same_inode(&retained[i].archive_snapshot, &other_archive) &&
                    !same_inode(&retained[i].quarantine_snapshot, &retained[j].quarantine_snapshot),
                    "duplicate archive or quarantine identity");
        }
        REQUIRE(!same_inode(&retained[i].quarantine_snapshot, &output_stat), "quarantine aliases output directory");
        REQUIRE(!same_inode(&retained[i].archive_snapshot, &base_toc_stat) &&
                !same_inode(&retained[i].archive_snapshot, &base_header_stat), "archive aliases base metadata");
    }

    mb_archive_limits_default_v1(&limits);
    limits.raw_bytes = UINT64_C(512) << 20;
    limits.member_bytes = UINT64_C(512) << 20;
    limits.expanded_bytes = UINT64_C(1024) << 20;
    limits.entries = ROWS;
    limits.name_bytes = NAMES;
    mb_archive_budget_default_v1(&budget);
    budget.io_limit = UINT64_C(16) << 30;
    budget.operation_limit = UINT64_C(1000000);
    budget.path_work_limit = UINT64_C(128) << 20;
    scan = (mb_archive_buffers_v1){rows, ROWS, names, NAMES, sort_rows, ROWS, transfer, sizeof(transfer)};
    workspace = (mb_iostore_archive_workspace_v1){sizeof(workspace), 1,
        {group_sort, ROWS, groups, ROWS, group_members, ROWS, group_keys, NAMES},
        probe_scratch, sizeof(probe_scratch), primary_scratch, sizeof(primary_scratch)};

    struct timespec started, ended;
    if (clock_gettime(CLOCK_MONOTONIC, &started)) {
        fprintf(stderr, "monotonic clock unavailable; errno=%d\n", errno);
        goto done;
    }
    for (uint32_t source_index = 0; source_index < source_count; ++source_index) {
        retained_source *saved = retained + source_index;
        memset(&scan_source, 0, sizeof(scan_source));
        scan_source.struct_size = sizeof(scan_source);
        scan_source.abi_version = 1;
        scan_source.fd = saved->archive_fd;
        scan_source.size = saved->expected_bytes;
        memcpy(scan_source.sha256, saved->expected_sha256, 32);
        memcpy(scan_source.target_token, context_token, 32);
        memset(&inventory, 0, sizeof(inventory));
        int intake_status = mb_archive_scan_v1(&scan_source, &limits, &budget, &scan, &inventory);
        REQUIRE(intake_status == MB_ARCHIVE_OK && inventory.complete == 1 &&
                inventory.source_size == saved->expected_bytes &&
                !memcmp(inventory.source_sha256, saved->expected_sha256, 32) &&
                !memcmp(inventory.target_token, context_token, 32) &&
                inventory.source_device == (uint64_t)saved->archive_snapshot.st_dev &&
                inventory.source_inode == (uint64_t)saved->archive_snapshot.st_ino,
                "archive scan incomplete or source binding mismatch");

        mb_archive_prepared_view_v1 view = {&inventory, rows, ROWS, names, NAMES};
        memset(&selection, 0, sizeof(selection));
        mb_iostore_archive_status_v1 selection_status = mb_iostore_archive_select_v1(
            &view, NULL, 0, &workspace, selected, ROWS, members, ROWS, &selection);
        REQUIRE(selection_status == MB_IOSTORE_ARCHIVE_OK ||
                selection_status == MB_IOSTORE_ARCHIVE_PROBES_REQUIRED,
                "archive group selection refused");
        REQUIRE(selection.selection_complete == 1 && selection.member_count &&
                selection.member_count <= MEMBER_CAP &&
                !memcmp(selection.source_sha256, saved->expected_sha256, 32) &&
                !memcmp(selection.inventory_sha256, inventory.inventory_sha256, 32) &&
                !memcmp(selection.target_token, context_token, 32), "archive selection identity mismatch");
        const uint32_t selected_count = selection.member_count;
        memcpy(selected_before, selected, selected_count * sizeof(*selected));

        struct stat qstat;
        REQUIRE(!fstat(saved->quarantine_fd, &qstat) && same_inode(&qstat, &saved->quarantine_snapshot),
                "quarantine directory changed");
        mb_archive_materialize_request_v1 request = {0};
        request.source = &scan_source;
        request.inventory = &inventory;
        request.rows = rows;
        request.row_capacity = ROWS;
        request.names = names;
        request.name_capacity = NAMES;
        request.selected = selected;
        request.selected_count = selected_count;
        memcpy(request.target_token, context_token, 32);
        request.quarantine_fd = saved->quarantine_fd;
        request.quarantine_device = (uint64_t)qstat.st_dev;
        request.quarantine_inode = (uint64_t)qstat.st_ino;
        request.journal = journal;
        request.journal_capacity = JOURNAL_CAP;
        request.replay = (mb_archive_buffers_v1){replay_rows, ROWS, replay_names, NAMES,
                                                 replay_sort, ROWS, replay_transfer, sizeof(replay_transfer)};
        memset(journal, 0, sizeof(journal));
        memset(&materialized, 0, sizeof(materialized));
        intake_status = mb_archive_materialize_v1(&request, &limits, &budget, &materialized);
        REQUIRE(intake_status == MB_ARCHIVE_OK && materialized.complete == 1 &&
                materialized.cleanup_complete == 1 && materialized.selected_count == selected_count &&
                materialized.journal_count <= JOURNAL_CAP &&
                !memcmp(materialized.inventory_sha256, inventory.inventory_sha256, 32) &&
                !memcmp(materialized.target_token, context_token, 32), "materialization incomplete");

        current_fd_count = probe_count = 0;
        for (uint32_t i = 0; i < MEMBER_CAP; ++i) current_fds[i] = -1;
        for (uint32_t i = 0; i < MB_IOSTORE_ARCHIVE_MAX_PROBES; ++i) probes[i] = (mb_iostore_archive_probe_v1){0};
        for (uint32_t i = 0; i < selected_count; ++i) {
            const mb_iostore_archive_member_v1 *member = members + i;
            const mb_archive_owned_file_v1 *owned = owned_member(member->row_index, materialized.journal_count);
            REQUIRE(owned && memchr(owned->name, '\0', sizeof(owned->name)) && owned->name[0] &&
                    !strchr(owned->name, '/') && owned->expected_size == member->bytes &&
                    owned->written_bytes == member->bytes && !memcmp(owned->sha256, member->sha256, 32),
                    "materialized row/journal binding failed");
            int fd = openat(saved->quarantine_fd, owned->name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
            REQUIRE(fd >= 0, "materialized member open failed");
            current_fds[current_fd_count] = fd;
            current_rows[current_fd_count++] = member->row_index;
            struct stat member_stat;
            REQUIRE(!fstat(fd, &member_stat) && S_ISREG(member_stat.st_mode) && member_stat.st_size >= 0 &&
                    (uint64_t)member_stat.st_size == member->bytes &&
                    (uint64_t)member_stat.st_dev == owned->device &&
                    (uint64_t)member_stat.st_ino == owned->inode, "materialized member identity changed");
            if (member->probe_required) {
                uint64_t cap = member->role == MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION ?
                    MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES : MB_IOSTORE_ARCHIVE_MAX_DOCUMENT_BYTES;
                REQUIRE(probe_count < MB_IOSTORE_ARCHIVE_MAX_PROBES && member->bytes <= cap &&
                        member->bytes <= SIZE_MAX, "bounded content probe refused");
                uint8_t *bytes = malloc(member->bytes ? (size_t)member->bytes : 1);
                REQUIRE(bytes, "probe allocation failed");
                probes[probe_count++] = (mb_iostore_archive_probe_v1){member->row_index, 0, bytes, (size_t)member->bytes};
                REQUIRE(read_all(fd, bytes, (size_t)member->bytes), "probe read failed");
            }
        }
        selection_status = mb_iostore_archive_select_v1(&view, probes, probe_count, &workspace,
                                                        selected, ROWS, members, ROWS, &selection);
        if (selection_status != MB_IOSTORE_ARCHIVE_OK) {
            fprintf(stderr, "content probes refused source=%u status=%u row=%u pak_status=%u reason=%u\n",
                    source_index, selection_status, selection.problem_row,
                    selection.pak_inner_status, selection.pak_inner_reason);
            goto done;
        }
        REQUIRE(selection.selection_complete == 1 && selection.preparation_ready == 1 &&
                selection.member_count == selected_count &&
                !memcmp(selection.source_sha256, saved->expected_sha256, 32) &&
                !memcmp(selection.inventory_sha256, inventory.inventory_sha256, 32) &&
                !memcmp(selection.target_token, context_token, 32) &&
                !memcmp(selected, selected_before, selected_count * sizeof(*selected)),
                "content-probe selection identity changed");
        uint32_t toc_matches = 0, ucas_matches = 0;
        for (uint32_t i = 0; i < selected_count; ++i) {
            if (members[i].row_index == selection.toc_row && members[i].role == MB_IOSTORE_ARCHIVE_ROLE_UTOC) ++toc_matches;
            if (members[i].row_index == selection.ucas_row && members[i].role == MB_IOSTORE_ARCHIVE_ROLE_UCAS) ++ucas_matches;
        }
        REQUIRE(toc_matches == 1 && ucas_matches == 1 && selection.toc_row != selection.ucas_row,
                "selected UTOC/UCAS row roles unresolved");
        const int selected_toc_fd = row_fd(selection.toc_row);
        const int selected_ucas_fd = row_fd(selection.ucas_row);
        REQUIRE(selected_toc_fd >= 0 && selected_ucas_fd >= 0 && selected_toc_fd != selected_ucas_fd,
                "selected container descriptors absent");
        saved->toc_fd = selected_toc_fd;
        saved->ucas_fd = selected_ucas_fd;

        saved->inventory = inventory;
        saved->selection = selection;
        saved->materialized = materialized;
        saved->selected_count = selected_count;
        memcpy(saved->selected_rows, selected, selected_count * sizeof(*selected));
        memcpy(saved->members, members, selected_count * sizeof(*members));
        saved->journal_count = materialized.journal_count;
        memcpy(saved->journal, journal, saved->journal_count * sizeof(*journal));
        for (uint32_t i = 0; i < probe_count; ++i) {
            free((void *)probes[i].data);
            probes[i].data = NULL;
        }
        probe_count = 0;

        /* The scan/journal workspace is reusable only after each created row is
         * copied into caller-owned retained state and the two selected source
         * descriptors are pinned in retained[]. Noncontainer probe FDs close. */
        for (uint32_t i = 0; i < current_fd_count; ++i) {
            if (current_fds[i] == saved->toc_fd || current_fds[i] == saved->ucas_fd) {
                current_fds[i] = -1;
            }
        }
        close_current_fds(&close_status);
        REQUIRE(!close_status, "probe member close failed");
        struct stat after_scan;
        REQUIRE(!fstat(saved->archive_fd, &after_scan) && stable_file(&saved->archive_snapshot, &after_scan),
                "archive source changed during intake");
        batch_sources[source_index] = (mb_iostore_batch_source_v1){saved->toc_fd, saved->ucas_fd};
        reset_scan_storage();
    }

    REQUIRE(directory_private_empty(output_fd, &output_stat), "output directory changed before batch job");
    mb_iostore_batch_file_job_params_v1 job = {
        .struct_size = sizeof(job), .abi_version = 1,
        .sources = batch_sources, .source_count = source_count,
        .base_toc_fd = base_toc_fd, .base_header_fd = base_header_fd,
        .destination_dir_fd = output_fd, .immutable_inputs = 1,
        .private_unpublished_directory = 1,
        .observed_base_ucas_bytes = base_ucas_bytes
    };
    batch_result.struct_size = sizeof(batch_result);
    batch_result.abi_version = 1;
    mb_iostore_file_job_status_v1 job_status = mb_iostore_batch_file_job_v1(&job, &batch_result);
    REQUIRE(!fstat(base_toc_fd, &base_toc_after) && stable_file(&base_toc_stat, &base_toc_after) &&
            !fstat(base_header_fd, &base_header_after) && stable_file(&base_header_stat, &base_header_after),
            "base metadata changed during batch preparation");
    if (job_status != MB_IOSTORE_FILE_JOB_OK || !batch_result.preparation.prepared ||
        batch_result.source_count != source_count || batch_result.conflict_kind != MB_IOSTORE_BATCH_CONFLICT_NONE) {
        fprintf(stderr, "batch preparation refused status=%u stage=%u conflict=%u sources=%u,%u id=",
                job_status, batch_result.preparation.stage, batch_result.conflict_kind,
                batch_result.conflict_source_a, batch_result.conflict_source_b);
        for (unsigned j = 0; j < sizeof(batch_result.conflict_chunk_id); ++j) fprintf(stderr, "%02x", batch_result.conflict_chunk_id[j]);
        fprintf(stderr, " package=%" PRIu64 " system=%d owned_mask=%u\n",
                batch_result.conflict_package_id, batch_result.preparation.system_error,
                batch_result.preparation.created_artifact_mask);
        goto done;
    }
    REQUIRE(!batch_result.preparation.allocation_live_after_cleanup &&
            batch_result.preparation.allocations == batch_result.preparation.releases &&
            !batch_result.preparation.installation_authority &&
            batch_result.preparation.source_snapshots_stable, "batch completion contract failed");
    for (uint32_t i = 0; i < source_count; ++i) {
        struct stat after;
        REQUIRE(!fstat(retained[i].archive_fd, &after) && stable_file(&retained[i].archive_snapshot, &after),
                "archive source changed during batch preparation");
        struct stat qafter;
        REQUIRE(!fstat(retained[i].quarantine_fd, &qafter) &&
                same_inode(&retained[i].quarantine_snapshot, &qafter) &&
                qafter.st_uid == geteuid() && (qafter.st_mode & 07777) == 0700,
                "quarantine identity changed");
    }
    struct stat output_after;
    REQUIRE(!fstat(output_fd, &output_after) && same_inode(&output_stat, &output_after) &&
            output_after.st_uid == geteuid() && (output_after.st_mode & 07777) == 0700,
            "output directory identity changed");
    if (clock_gettime(CLOCK_MONOTONIC, &ended)) {
        fprintf(stderr, "monotonic clock read failed; errno=%d\n", errno);
        goto done;
    }
    job_ready = 1;
    rc = 0;

done:
    for (uint32_t i = 0; i < probe_count; ++i) free((void *)probes[i].data);
    close_current_fds(&close_status);
    if (close_status) rc = 1;
    for (uint32_t i = 0; i < SOURCE_CAP; ++i) {
        if (retained[i].toc_fd >= 0 && close(retained[i].toc_fd) && !rc) rc = 1;
        if (retained[i].ucas_fd >= 0 && close(retained[i].ucas_fd) && !rc) rc = 1;
        if (retained[i].quarantine_fd >= 0 && close(retained[i].quarantine_fd) && !rc) rc = 1;
        if (retained[i].archive_fd >= 0 && close(retained[i].archive_fd) && !rc) rc = 1;
    }
    if (base_toc_fd >= 0 && close(base_toc_fd) && !rc) rc = 1;
    if (base_header_fd >= 0 && close(base_header_fd) && !rc) rc = 1;
    if (output_fd >= 0 && close(output_fd) && !rc) rc = 1;
    if (!rc && job_ready) {
        double seconds = (double)(ended.tv_sec - started.tv_sec) +
                         (double)(ended.tv_nsec - started.tv_nsec) / 1e9;
        printf("{\"phase\":\"batch_archive_prepared\",\"seconds\":%.6f,\"source_count\":%u,\"strict_conflicts\":true,"
               "\"inventory_io_used\":%" PRIu64 ",\"inventory_operations_used\":%" PRIu64 ","
               "\"emitted_chunks\":%u,\"sidecar_bytes\":%" PRIu64 ",\"final_toc_bytes\":%" PRIu64 ","
               "\"source_asset_hashes_verified\":%u,\"allocations\":%u,\"releases\":%u,"
               "\"installation_authority\":false,\"sources\":[",
               seconds, source_count, budget.io_used, budget.operations_used,
               batch_result.preparation.emitted_chunks, batch_result.preparation.artifact_bytes[0],
               batch_result.preparation.artifact_bytes[4], batch_result.preparation.incoming_asset_hashes_verified,
               batch_result.preparation.allocations, batch_result.preparation.releases);
        for (uint32_t i = 0; i < source_count; ++i) {
            char archive_hash[65], inventory_hash[65];
            hex(retained[i].expected_sha256, archive_hash);
            hex(retained[i].inventory.inventory_sha256, inventory_hash);
            printf("%s{\"index\":%u,\"archive_sha256\":\"%s\",\"inventory_sha256\":\"%s\","
                   "\"archive_bytes\":%" PRIu64 ",\"toc_row\":%u,\"ucas_row\":%u,"
                   "\"selected_members\":%u,\"journal_count\":%u,\"materialized_decoded_bytes\":%" PRIu64 "}",
                   i ? "," : "", i, archive_hash, inventory_hash, retained[i].expected_bytes,
                   retained[i].selection.toc_row, retained[i].selection.ucas_row,
                   retained[i].selected_count, retained[i].journal_count, retained[i].materialized.decoded_bytes);
        }
        printf("]}\n");
        if (fflush(stdout) == EOF || ferror(stdout)) rc = 1;
    }
    /* Quarantine intents/manifests and exclusive partial output artifacts stay
     * caller-owned on every outcome; no paths from the archives are destinations. */
    return rc;
}
