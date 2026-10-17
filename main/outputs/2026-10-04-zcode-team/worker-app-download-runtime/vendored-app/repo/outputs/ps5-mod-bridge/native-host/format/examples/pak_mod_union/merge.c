#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#include "prepare_union.h"
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* CLI owner only. The preparation core receives borrowed immutable FDs. */
static int same_inode(const struct stat *a, const struct stat *b) {
    return a->st_dev == b->st_dev && a->st_ino == b->st_ino;
}
static int same_snapshot(const struct stat *a, const struct stat *b) {
    if (!same_inode(a, b) || a->st_size != b->st_size) return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static int sources_stable(const int fds[2], const struct stat before[2]) {
    for (unsigned i = 0u; i < 2u; ++i) {
        struct stat now;
        if (fds[i] < 0 || fstat(fds[i], &now) || !S_ISREG(now.st_mode) || !same_snapshot(&before[i], &now)) return 0;
    }
    return 1;
}
static void json_string(const uint8_t *data, size_t bytes) {
    putchar('"');
    for (size_t i = 0u; i < bytes; ++i) {
        uint8_t c = data[i];
        if (c == '"' || c == '\\') { putchar('\\'); putchar(c); }
        else if (c < 32u || c >= 128u) printf("\\u%04x", (unsigned)c);
        else putchar(c);
    }
    putchar('"');
}


int main(int argc, char **argv) {
    int fds[2] = {-1, -1}, output_fd = -1;
    struct stat before[2] = {{0}, {0}}, output_created = {0};
    uint64_t source_bytes[2] = {0u, 0u}, observed_output_bytes = 0u;
    mb_pak_union_result_v1 prepared = {.failure_row = UINT32_MAX};
    const char *stage = "arguments", *error = "none";
    int success = 0, output_owned = 0, snapshot_stable = 0;
    int output_size_valid = 0, output_fsynced = 0, output_closed = 0;
    if (argc != 4) { error = "usage: merge BASE_V11_PAK MOD_V3_PAK FRESH_OUTPUT"; goto cleanup; }
    stage = "open_sources";
    for (unsigned i = 0u; i < 2u; ++i) {
        fds[i] = open(argv[i + 1u], O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        if (fds[i] < 0) { error = "readonly_source_open_failed"; goto cleanup; }
        if (fstat(fds[i], &before[i]) || !S_ISREG(before[i].st_mode) || before[i].st_size <= 0 ||
            (uint64_t)before[i].st_size > MB_PAK_STREAM_MAX_ARCHIVE_BYTES) {
            error = "source_type_or_size_refused"; goto cleanup;
        }
        source_bytes[i] = (uint64_t)before[i].st_size;
    }
    if (same_inode(&before[0], &before[1])) { error = "duplicate_source_inode_refused"; goto cleanup; }
    stage = "create_private_output";
    if (!sources_stable(fds, before)) { error = "source_snapshot_changed_before_output"; goto cleanup; }
    output_fd = open(argv[3], O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (output_fd < 0) { error = "fresh_output_creation_refused"; goto cleanup; }
    output_owned = 1;
    if (fstat(output_fd, &output_created) || !S_ISREG(output_created.st_mode) || output_created.st_size != 0 ||
        same_inode(&output_created, &before[0]) || same_inode(&output_created, &before[1])) {
        error = "output_identity_refused"; goto cleanup;
    }
    mb_pak_union_params_v1 params = {sizeof(params), MB_PAK_UNION_ABI_V1, fds[0], fds[1], output_fd,
        1u, 1u, 0u, source_bytes[0], source_bytes[1]};
    mb_pak_union_status_v1 status = mb_pak_union_prepare_v1(&params, &prepared);
    if (prepared.struct_size == sizeof(prepared) && prepared.abi_version == MB_PAK_UNION_ABI_V1) {
        stage = prepared.stage; error = prepared.error;
    } else {
        stage = "prepare_preflight"; error = "prepare_argument_refused";
    }
    if (status != MB_PAK_UNION_OK || !prepared.archive_complete) goto cleanup;
    stage = "finalize_private_artifact";
    struct stat output_now, path_now;
    snapshot_stable = sources_stable(fds, before);
    if (!snapshot_stable) { error = "source_snapshot_changed"; goto cleanup; }
    if (fstat(output_fd, &output_now) || !S_ISREG(output_now.st_mode) || !same_inode(&output_now, &output_created) ||
        output_now.st_size < 0 || (uint64_t)output_now.st_size != prepared.archive_bytes || lstat(argv[3], &path_now) ||
        !S_ISREG(path_now.st_mode) || !same_inode(&output_now, &path_now)) {
        error = "completed_output_identity_or_size_refused"; goto cleanup;
    }
    observed_output_bytes = (uint64_t)output_now.st_size; output_size_valid = 1;
    if (fsync(output_fd)) { error = "output_fsync_failed"; goto cleanup; }
    output_fsynced = 1;
    if (close(output_fd)) { output_fd = -1; error = "output_close_outcome_uncertain"; goto cleanup; }
    output_fd = -1; output_closed = 1;
    if (!sources_stable(fds, before)) { snapshot_stable = 0; error = "source_changed_after_finalize"; goto cleanup; }
    success = 1; stage = "complete";

cleanup:
    if (output_fd >= 0) {
        struct stat now;
        if (!fstat(output_fd, &now) && now.st_size >= 0) observed_output_bytes = (uint64_t)now.st_size;
        if (close(output_fd)) {
            success = 0;
            if (!strcmp(error, "none")) { error = "output_close_outcome_uncertain"; stage = "final_output_close"; }
        }
        output_fd = -1;
    }
    snapshot_stable = source_bytes[0] && source_bytes[1] && sources_stable(fds, before);
    if (!snapshot_stable && success) {
        success = 0;
        if (!strcmp(error, "none")) { error = "source_changed_at_final_cleanup_check"; stage = "final_source_snapshot"; }
    }
    for (unsigned i = 0u; i < 2u; ++i) {
        if (fds[i] >= 0 && close(fds[i])) {
            success = 0;
            if (!strcmp(error, "none")) { error = "source_close_outcome_uncertain"; stage = "final_source_close"; }
        }
    }
    printf("{\"phase\":\"pak_base_mod_union_preparation\",\"stage\":\"%s\",\"status\":\"%s\","
           "\"error\":\"%s\",\"component_status\":%u,\"component_detail\":%u,\"failure_row\":%u,"
           "\"base_source_bytes\":%" PRIu64 ",\"mod_source_bytes\":%" PRIu64 ","
           "\"base_version\":11,\"mod_version\":3,\"output_version\":11,\"mount\":",
           stage, success ? "pass" : "fail", error, prepared.component_status, prepared.component_detail, prepared.failure_row,
           source_bytes[0], source_bytes[1]);
    json_string(prepared.mount, prepared.mount_bytes);
    printf(",\"path_hash_seed\":%" PRIu64 ",\"mod_mount\":", prepared.path_hash_seed);
    json_string(prepared.mod_mount, prepared.mod_mount_bytes);
    printf(",\"base_file_count\":%u,\"mod_file_count\":%u,\"input_file_count\":%u,"
           "\"final_file_count\":%u,\"replaced\":%u,\"added\":%u,\"payload_bytes\":%" PRIu64 ","
           "\"base_metadata_complete\":%s,\"base_reference_graph_complete\":%s,\"mod_metadata_complete\":%s,"
           "\"base_graph_steps\":%" PRIu64 ",\"base_graph_comparison_bytes\":%" PRIu64 ","
           "\"path_comparison_bytes\":%" PRIu64 ",\"writer_status\":%u,\"writer_complete\":%s,"
           "\"chosen_payloads_sha1_checked\":%u,\"chosen_base_payloads_sha1_checked\":%u,"
           "\"chosen_mod_payloads_sha1_checked\":%u,\"overridden_base_payloads_read\":false,"
           "\"artifact_complete\":%s,\"archive_bytes\":%" PRIu64 ",\"observed_output_bytes\":%" PRIu64 ","
           "\"source_snapshot_stable\":%s,\"output_created_exclusively\":%s,\"output_size_valid\":%s,"
           "\"output_fsynced\":%s,\"output_closed\":%s,\"unpublished\":true,\"unpublished_partial_retained\":%s,"
           "\"requested_job_io_bytes\":%" PRIu64 ",\"job_io_calls\":%u,\"allocation_peak_bytes\":%" PRIu64 ","
           "\"allocations\":%u,\"releases\":%u,\"live_allocations\":%" PRIu64 ","
           "\"independent_readback_performed\":false,\"source_paths_used_as_destinations\":false,"
           "\"ini_scalar_merge_performed\":false,\"install_supported\":false,\"runtime_success_claimed\":false}\n",
           prepared.base_file_count, prepared.mod_file_count, prepared.input_file_count, prepared.final_file_count, prepared.replaced, prepared.added, prepared.payload_bytes,
           prepared.base_metadata_complete ? "true" : "false", prepared.base_reference_graph_complete ? "true" : "false",
           prepared.mod_metadata_complete ? "true" : "false", prepared.base_graph_steps, prepared.base_graph_comparison_bytes,
           prepared.path_comparison_bytes, (unsigned)prepared.writer.status, prepared.writer.archive_complete ? "true" : "false",
           prepared.writer.archive_complete ? prepared.final_file_count : 0u, prepared.writer.archive_complete ? prepared.base_file_count - prepared.replaced : 0u,
           prepared.writer.archive_complete ? prepared.mod_file_count : 0u, success ? "true" : "false",
           prepared.archive_bytes, observed_output_bytes, snapshot_stable ? "true" : "false",
           output_owned ? "true" : "false", output_size_valid ? "true" : "false",
           output_fsynced ? "true" : "false", output_closed ? "true" : "false",
           output_owned && !success ? "true" : "false", prepared.requested_job_io_bytes, prepared.job_io_calls,
           prepared.allocation_peak_bytes, prepared.allocation_calls, prepared.release_calls, prepared.allocation_live_after_cleanup);
    return success ? 0 : 1;
}
