#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L

#include "../../../archive/intake.h"
#include "../../../archive/pak_selection.h"
#include "../../pak_archive_job.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Ordinary-host archive intake and PAK preparation. Archive names remain
 * metadata; only intake-generated journal names are opened as quarantine files. */
#define ROWS 512u
#define NAMES (256u * 1024u)
#define JOURNAL_CAPACITY (ROWS + 3u)
#define TRANSFER_BYTES MB_ARCHIVE_CHUNK_BYTES
#define MAX_INPUT_BYTES (UINT64_C(512) << 20)

static mb_pak_archive_job_workspace_v1 workspace = {
    .struct_size = sizeof(workspace), .abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1
};
static mb_pak_archive_job_result_v1 job_result = {
    .scan_status = UINT32_MAX, .selection_status = UINT32_MAX,
    .materialize_status = UINT32_MAX, .union_status = UINT32_MAX,
    .output_fd = -1
};
#define rows workspace.rows
#define names workspace.names
#define pak_members workspace.members
#define journal workspace.journal
#define inventory job_result.inventory
#define materialized job_result.materialized
#define selection job_result.selection
#define union_result job_result.union_result
#define scan_status job_result.scan_status
#define selection_status job_result.selection_status
#define materialize_status job_result.materialize_status
#define union_status job_result.union_status
#define selection_called job_result.selection_called
#define materialization_complete job_result.materialization_complete
static const char *phase = "arguments", *stage = "not_started", *status_text = "refused";
static uint32_t selection_ready, output_created, artifact_complete;
static mb_archive_source_v1 source;
static uint64_t archive_bytes_out;
static int archive_fd = -1, base_fd = -1, quarantine_fd = -1, output_fd = -1;
static struct stat archive_before, base_before, quarantine_before, output_before;
static int close_failure;

static int parse_decimal(const char *text, uint64_t *value) {
    uint64_t n = 0;
    const unsigned char *p = (const unsigned char *)text;
    if (!p || !*p) return 0;
    for (; *p; ++p) {
        unsigned digit;
        if (*p < '0' || *p > '9') return 0;
        digit = (unsigned)(*p - '0');
        if (n > (UINT64_MAX - digit) / 10u) return 0;
        n = n * 10u + digit;
    }
    *value = n;
    return 1;
}

static int hex_value(unsigned char c) {
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (int)(c - 'A' + 10);
    return -1;
}

static int parse_hex32(const char *text, uint8_t out[32]) {
    if (!text || strlen(text) != 64u) return 0;
    for (unsigned i = 0; i < 32u; ++i) {
        int hi = hex_value((unsigned char)text[2u * i]);
        int lo = hex_value((unsigned char)text[2u * i + 1u]);
        if (hi < 0 || lo < 0) return 0;
        out[i] = (uint8_t)((unsigned)hi * 16u + (unsigned)lo);
    }
    return 1;
}

static void hex32(const uint8_t value[32], char out[65]) {
    static const char digits[] = "0123456789abcdef";
    for (unsigned i = 0; i < 32u; ++i) {
        out[2u * i] = digits[value[i] >> 4];
        out[2u * i + 1u] = digits[value[i] & 15u];
    }
    out[64] = '\0';
}

static int same_inode(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}

static int same_file_snapshot(const struct stat *a, const struct stat *b) {
    if (!same_inode(a, b) || a->st_mode != b->st_mode || a->st_size != b->st_size)
        return 0;
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

static int stat_file_stable(int fd, const struct stat *before) {
    struct stat after;
    return fd >= 0 && fstat(fd, &after) == 0 && S_ISREG(after.st_mode) &&
           same_file_snapshot(before, &after);
}

static int is_private_directory(const struct stat *st) {
    return S_ISDIR(st->st_mode) && st->st_uid == geteuid() &&
           (st->st_mode & 0700) == 0700 && (st->st_mode & 0077) == 0;
}

static int directory_path_still_same(const char *path, const struct stat *before) {
    struct stat now;
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int ok = fd >= 0 && fstat(fd, &now) == 0 && is_private_directory(&now) &&
             same_inode(before, &now);
    if (fd >= 0 && close(fd) != 0) ok = 0;
    return ok;
}

static const mb_archive_owned_file_v1 *find_owned(uint32_t row,
                                                  uint32_t journal_count) {
    const mb_archive_owned_file_v1 *found = NULL;
    if (journal_count > JOURNAL_CAPACITY) return NULL;
    for (uint32_t i = 0; i < journal_count; ++i) {
        const mb_archive_owned_file_v1 *item = journal + i;
        if (item->row_index != row ||
            (item->state != MB_ARCHIVE_CREATED && item->state != MB_ARCHIVE_RETAINED))
            continue;
        if (found) return NULL;
        found = item;
    }
    return found;
}

static int emit_json_string(const char *bytes, size_t length) {
    static const char hex[] = "0123456789abcdef";
    if (putchar('"') == EOF) return 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)bytes[i];
        if (c == '"' || c == '\\') {
            if (putchar('\\') == EOF || putchar((int)c) == EOF) return 0;
        } else if (c < 0x20u) {
            char escaped[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15u]};
            if (fwrite(escaped, 1u, sizeof(escaped), stdout) != sizeof(escaped)) return 0;
        } else if (putchar((int)c) == EOF) return 0;
    }
    return putchar('"') != EOF;
}

static int emit_report(void) {
    char source_hash[65], inventory_hash[65], target_hash[65];
    uint32_t member_count = selection.member_list_complete ? selection.member_count : 0u;
    hex32(source.sha256, source_hash);
    hex32(inventory.inventory_sha256, inventory_hash);
    hex32(source.target_token, target_hash);
    if (printf("{\"phase\":\"%s\",\"stage\":\"%s\",\"status\":\"%s\","
               "\"selection_status\":%" PRIu32 ",\"preparation_ready\":%s,"
               "\"whole_bundle_handled\":%s,\"member_count\":%" PRIu32 ","
               "\"retained_extra_count\":%" PRIu32 ",\"pak_row\":",
               phase, stage, status_text, selection_status,
               selection.preparation_ready ? "true" : "false",
               selection.whole_bundle_handled ? "true" : "false",
               member_count, selection.retained_extra_count) < 0) return 0;
    if (selection.selection_complete) {
        if (printf("%" PRIu32, selection.pak_row) < 0) return 0;
    } else if (fputs("null", stdout) == EOF) return 0;
    if (printf(",\"artifact_complete\":%s,\"archive_bytes\":%" PRIu64 ","
               "\"archive_complete\":%s,\"materialization_complete\":%s,"
               "\"scan_status\":%" PRIu32 ",\"materialize_status\":%" PRIu32 ","
               "\"union_status\":%" PRIu32 ","
               "\"source_bytes\":%" PRIu64 ",\"base_pak_bytes\":%" PRIu64 ","
               "\"mod_pak_bytes\":%" PRIu64 ",\"source_sha256\":\"%s\","
               "\"inventory_sha256\":\"%s\",\"target_sha256\":\"%s\","
               "\"merged_counts\":{\"base_files\":%" PRIu32 ",\"mod_files\":%" PRIu32 ","
               "\"input_files\":%" PRIu32 ",\"final_files\":%" PRIu32 ","
               "\"replaced\":%" PRIu32 ",\"added\":%" PRIu32 "},"
               "\"installation_supported\":false,\"runtime_supported\":false,\"members\":[",
               artifact_complete ? "true" : "false", archive_bytes_out,
               union_result.archive_complete ? "true" : "false",
               materialization_complete ? "true" : "false",
               scan_status, materialize_status, union_status,
               source.size, (uint64_t)(base_before.st_size > 0 ? base_before.st_size : 0),
               (uint64_t)(selection_ready && selection.pak_row < inventory.row_count ?
                    rows[selection.pak_row].size : 0u),
               source_hash, inventory_hash, target_hash,
               union_result.base_file_count, union_result.mod_file_count,
               union_result.input_file_count, union_result.final_file_count,
               union_result.replaced, union_result.added) < 0) return 0;
    for (uint32_t i = 0; i < member_count; ++i) {
        const mb_pak_archive_member_v1 *member = pak_members + i;
        const mb_archive_row_v1 *row;
        const char *logical_name;
        const mb_archive_owned_file_v1 *owned = NULL;
        char hash[65];
        if (member->row_index >= inventory.row_count) return 0;
        row = rows + member->row_index;
        if (row->path_offset >= inventory.name_bytes ||
            row->path_length >= inventory.name_bytes - row->path_offset) return 0;
        logical_name = names + row->path_offset;
        hex32(member->sha256, hash);
        if (i && putchar(',') == EOF) return 0;
        if (printf("{\"row_index\":%" PRIu32 ",\"role\":\"%s\",\"bytes\":%" PRIu64
                   ",\"sha256\":\"%s\",\"logical_name\":",
                   member->row_index,
                   member->role == MB_PAK_ARCHIVE_ROLE_PAK ? "pak" : "retained_extra",
                   member->bytes, hash) < 0 ||
            !emit_json_string(logical_name, row->path_length)) return 0;
        if (materialized.journal_count <= JOURNAL_CAPACITY && materialized.journal_count)
            owned = find_owned(member->row_index, materialized.journal_count);
        if (owned && (owned->state == MB_ARCHIVE_CREATED || owned->state == MB_ARCHIVE_RETAINED)) {
            if (fputs(",\"retained_name\":", stdout) == EOF ||
                !emit_json_string(owned->name, strnlen(owned->name, sizeof(owned->name)))) return 0;
        }
        if (putchar('}') == EOF) return 0;
    }
    if (printf("],\"journal_count\":%" PRIu32 ",\"journal\":[",
               materialized.journal_count <= JOURNAL_CAPACITY ? materialized.journal_count : 0u) < 0) return 0;
    uint32_t journal_count = materialized.journal_count <= JOURNAL_CAPACITY ?
                             materialized.journal_count : 0u;
    for (uint32_t i = 0; i < journal_count; ++i) {
        const mb_archive_owned_file_v1 *owned = journal + i;
        char hash[65];
        size_t name_bytes = strnlen(owned->name, sizeof(owned->name));
        if (name_bytes == sizeof(owned->name)) return 0;
        hex32(owned->sha256, hash);
        if (i && putchar(',') == EOF) return 0;
        if (printf("{\"state\":%" PRIu32 ",\"row_index\":%" PRIu32
                   ",\"name\":", owned->state, owned->row_index) < 0 ||
            !emit_json_string(owned->name, name_bytes) ||
            printf(",\"device\":%" PRIu64 ",\"inode\":%" PRIu64
                   ",\"expected_size\":%" PRIu64 ",\"written_bytes\":%" PRIu64
                   ",\"sha256\":\"%s\"}", owned->device, owned->inode,
                   owned->expected_size, owned->written_bytes, hash) < 0) return 0;
    }
    return fputs("]}\n", stdout) != EOF && fflush(stdout) == 0;
}

static int close_one(int *fd) {
    int result;
    if (*fd < 0) return 1;
    result = close(*fd);
    *fd = -1;
    return result == 0;
}

static void set_failure(const char *at, const char *why) {
    phase = at;
    stage = why;
    status_text = "refused";
}

static int acquire_output(void *context, int *fd) {
    const char *path = (const char *)context;
    struct stat existing;
    *fd = -1;
    if (lstat(path, &existing) == 0 || errno != ENOENT) return 1;
    output_fd = open(path, O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    *fd = output_fd;
    if (output_fd < 0) return 1;
    output_created = 1u;
    return fstat(output_fd, &output_before) != 0;
}

int main(int argc, char **argv) {
    mb_pak_archive_job_params_v1 params;
    uint64_t declared_archive_bytes;
    int rc = 1;
    const char *quarantine_path, *output_path;
    memset(&params, 0, sizeof(params));
    if ((argc != 8 && argc != 9) ||
        (argc == 9 && strcmp(argv[8], "--pak-component-only") != 0)) {
        fprintf(stderr, "usage: archive_merge BASE_PAK ORIGINAL_ARCHIVE ARCHIVE_BYTES ARCHIVE_SHA256 TARGET_TOKEN_HEX FRESH_0700_QUARANTINE FRESH_OUTPUT [--pak-component-only]\n");
        return 2;
    }
    source.struct_size = sizeof(source); source.abi_version = MB_ARCHIVE_ABI_V1;
    if (!parse_decimal(argv[3], &declared_archive_bytes) || !declared_archive_bytes ||
        declared_archive_bytes > MAX_INPUT_BYTES || !parse_hex32(argv[4], source.sha256) ||
        !parse_hex32(argv[5], source.target_token)) {
        fprintf(stderr, "invalid bounded input metadata\n"); return 2;
    }
    source.size = declared_archive_bytes;
    quarantine_path = argv[6]; output_path = argv[7];
    phase = "input_open";
    archive_fd = open(argv[2], O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    base_fd = open(argv[1], O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    quarantine_fd = open(quarantine_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    source.fd = archive_fd;
    if (archive_fd < 0 || base_fd < 0 || quarantine_fd < 0 ||
        fstat(archive_fd, &archive_before) || fstat(base_fd, &base_before) ||
        fstat(quarantine_fd, &quarantine_before) || base_before.st_size <= 0) {
        set_failure(phase, "input_open_or_stat_failed"); goto cleanup;
    }
    params.struct_size = sizeof(params); params.abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1;
    params.source = source; params.base_fd = base_fd; params.quarantine_fd = quarantine_fd;
    params.base_bytes = (uint64_t)base_before.st_size;
    params.output_fd = -1; params.acquire_output = acquire_output;
    params.output_context = (void *)output_path;
    params.immutable_inputs = params.private_quarantine = params.unpublished_output = 1u;
    params.policy = argc == 9 ? MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY : MB_PAK_ARCHIVE_POLICY_DEFAULT;
    mb_pak_archive_job_status_v1 job_status = mb_pak_archive_job_prepare_v1(&params, &workspace, &job_result);
    phase = "archive_job"; stage = mb_pak_archive_job_stage_name_v1(job_result.stage);
    selection_ready = selection.selection_complete && selection.preparation_ready;
    archive_bytes_out = union_result.archive_bytes;
    if (job_status == MB_PAK_ARCHIVE_JOB_ADDITIONAL_ACTION_REQUIRED) {
        status_text = "additional_action_required"; rc = 3; goto cleanup;
    }
    if (job_status != MB_PAK_ARCHIVE_JOB_OK || !job_result.preparation_complete ||
        !union_result.archive_complete || output_fd < 0) {
        stage = job_result.error[0] ? job_result.error : "job_preflight_or_preparation_refused";
        goto cleanup;
    }
    phase = "artifact_finalize"; stage = "fsync_and_bind_unpublished_output";
    struct stat now, path_now;
    if (fstat(output_fd, &now) || !S_ISREG(now.st_mode) || now.st_size < 0 ||
        (uint64_t)now.st_size != archive_bytes_out || !same_inode(&now, &output_before) ||
        lstat(output_path, &path_now) || !S_ISREG(path_now.st_mode) || !same_inode(&now, &path_now) ||
        fsync(output_fd) || !stat_file_stable(archive_fd, &archive_before) ||
        !stat_file_stable(base_fd, &base_before) ||
        !directory_path_still_same(quarantine_path, &quarantine_before)) {
        set_failure(phase, "output_or_source_snapshot_check_failed"); goto cleanup;
    }
    status_text = "ok"; stage = "prepared_unpublished_artifact"; rc = 0;
cleanup:
    if (!close_one(&output_fd)) close_failure = 1;
    if (rc == 0 && !close_failure) {
        struct stat path_now;
        if (lstat(output_path, &path_now) || !S_ISREG(path_now.st_mode) || path_now.st_size < 0 ||
            (uint64_t)path_now.st_size != archive_bytes_out || !same_inode(&path_now, &output_before) ||
            !stat_file_stable(archive_fd, &archive_before) || !stat_file_stable(base_fd, &base_before)) {
            set_failure("artifact_finalize", "path_or_source_changed_after_close"); rc = 1;
        }
    }
    if (!close_one(&base_fd)) close_failure = 1;
    if (!close_one(&archive_fd)) close_failure = 1;
    if (!close_one(&quarantine_fd)) close_failure = 1;
    if (close_failure && rc == 0) {
        set_failure("descriptor_close", "close_failed_after_preparation"); rc = 1;
    }
    artifact_complete = rc == 0 && job_result.preparation_complete && output_created && !close_failure;
    if (!selection_called) selection_status = UINT32_MAX;
    if (!emit_report()) return 1;
    return rc;
}
