#define _POSIX_C_SOURCE 200809L
#include "pak_v3_none_reader.h"
#include "pak_stream_writer.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Ordinary host caller only: archive paths are metadata, never file targets. */
#define JOB_MAX_IO_BYTES UINT64_C(2147483648)
#define JOB_MAX_IO_CALLS 131072u
#define JOB_MAX_BLOCKS 12u
_Static_assert(MB_PAK_V3_TRANSFER_BYTES == MB_PAK_STREAM_TRANSFER_BYTES,
               "one 64 KiB positional I/O profile required");

typedef struct job_io {
    int source_fd, output_fd;
    struct stat source_before, output_created;
    uint64_t source_bytes, output_cap, requested_bytes;
    uint32_t calls, source_changed;
    const mb_pak_v3_entry_v1 *entries;
    uint32_t entry_count;
} job_io;

typedef struct allocations {
    void *blocks[JOB_MAX_BLOCKS];
    size_t sizes[JOB_MAX_BLOCKS];
    uint32_t count, released;
    uint64_t bytes, peak;
} allocations;

static int same_snapshot(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino ||
        a->st_size != b->st_size) return 0;
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

static int source_stable(job_io *io) {
    struct stat now;
    if (fstat(io->source_fd, &now) || !S_ISREG(now.st_mode) ||
        !same_snapshot(&io->source_before, &now)) {
        io->source_changed = 1u;
        return 0;
    }
    return 1;
}

static int charge_io(job_io *io, uint32_t n) {
    if (!n || n > MB_PAK_STREAM_TRANSFER_BYTES || io->calls >= JOB_MAX_IO_CALLS ||
        io->requested_bytes > JOB_MAX_IO_BYTES ||
        n > JOB_MAX_IO_BYTES - io->requested_bytes) return 0;
    ++io->calls;
    io->requested_bytes += n;
    return 1;
}

static pak_provider_status source_read(void *context, uint64_t offset,
                                     uint8_t *bytes, uint32_t n, uint32_t *actual) {
    job_io *io = context;
    *actual = 0u;
    if (!bytes || !n || n > MB_PAK_STREAM_TRANSFER_BYTES || offset > io->source_bytes ||
        n > io->source_bytes - offset || offset > INT64_MAX) return PAK_PROVIDER_CONTRACT_ERROR;
    if (!source_stable(io)) return PAK_PROVIDER_CHANGED;
    /* EINTR retries are bounded and charged, as are successful short retries. */
    uint32_t done = 0u;
    while (done < n) {
        if (!charge_io(io, n - done)) { *actual = done; return PAK_PROVIDER_CONTRACT_ERROR; }
        ssize_t count = pread(io->source_fd, bytes + done, n - done, (off_t)(offset + done));
        if (count < 0) {
            if (errno == EINTR) continue;
            *actual = done;
            return PAK_PROVIDER_IO_ERROR;
        }
        if (!count) break;
        done += (uint32_t)count;
    }
    *actual = done;
    return source_stable(io) ? PAK_PROVIDER_OK : PAK_PROVIDER_CHANGED;
}

static int payload_read(void *context, uint32_t row, uint64_t offset,
                        uint8_t *bytes, uint32_t n, uint32_t *actual) {
    job_io *io = context;
    *actual = 0u;
    if (row >= io->entry_count) return 1;
    const mb_pak_v3_entry_v1 *entry = &io->entries[row];
    if (offset > entry->payload_bytes || n > entry->payload_bytes - offset ||
        entry->data_offset > io->source_bytes || offset > io->source_bytes - entry->data_offset)
        return 1;
    return source_read(context, entry->data_offset + offset, bytes, n, actual) != PAK_PROVIDER_OK;
}

static int output_write(void *context, uint64_t offset, const uint8_t *bytes,
                        uint32_t n, uint32_t *actual) {
    job_io *io = context;
    *actual = 0u;
    if (!bytes || !n || n > MB_PAK_STREAM_TRANSFER_BYTES || offset > io->output_cap ||
        n > io->output_cap - offset || offset > INT64_MAX || !source_stable(io)) return 1;
    uint32_t done = 0u;
    while (done < n) {
        if (!charge_io(io, n - done)) { *actual = done; return 1; }
        ssize_t count = pwrite(io->output_fd, bytes + done, n - done, (off_t)(offset + done));
        if (count < 0) {
            if (errno == EINTR) continue;
            *actual = done;
            return 1;
        }
        if (!count) break;
        done += (uint32_t)count;
    }
    *actual = done;
    return 0;
}

static void *allocate(allocations *a, size_t count, size_t width) {
    if (!width || count > SIZE_MAX / width || a->count >= JOB_MAX_BLOCKS) return NULL;
    size_t size = count * width;
    if (!size) size = 1u;
    void *block = calloc(1u, size);
    if (!block) return NULL;
    a->blocks[a->count] = block;
    a->sizes[a->count++] = size;
    a->bytes += size;
    if (a->bytes > a->peak) a->peak = a->bytes;
    return block;
}

static void release_all(allocations *a) {
    while (a->released < a->count) {
        uint32_t i = a->released++;
        free(a->blocks[i]);
        a->blocks[i] = NULL;
        a->bytes -= a->sizes[i];
    }
}

static int parse_u64(const char *text, uint64_t *out) {
    if (!text || text[0] < '0' || text[0] > '9') return 0;
    int base = text[0] == '0' && (text[1] == 'x' || text[1] == 'X') ? 16 : 10;
    if (base == 16 && !text[2]) return 0;
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &end, base);
    if (errno || !end || end == text || *end) return 0;
    *out = (uint64_t)value;
    return 1;
}

static void hex(const uint8_t *data, size_t bytes) {
    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0u; i < bytes; ++i) {
        putchar(alphabet[data[i] >> 4]);
        putchar(alphabet[data[i] & 15u]);
    }
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
    job_io io = {.source_fd = -1, .output_fd = -1};
    allocations a = {0};
    mb_pak_v3_result_v1 metadata = {0};
    mb_pak_stream_result_v1 written = {0};
    mb_pak_v3_buffers_v1 reader = {0};
    mb_pak_stream_workspace_v1 work = {0};
    uint64_t seed = 0u, payload_bytes = 0u;
    const char *error = "none", *stage = "arguments";
    int success = 0, output_owned = 0, snapshot_stable = 0;
    int output_size_valid = 0, output_fsynced = 0, output_closed = 0;
    uint64_t observed_output_bytes = 0u;
    io.output_cap = MB_PAK_STREAM_MAX_ARCHIVE_BYTES;
    if ((argc != 4 && argc != 5) || !parse_u64(argv[3], &seed) ||
        (argc == 5 && (!parse_u64(argv[4], &io.output_cap) ||
                       !io.output_cap || io.output_cap > MB_PAK_STREAM_MAX_ARCHIVE_BYTES))) {
        error = "usage: transcode original.pak fresh-private-output.pak seed [max-output-bytes]";
        goto cleanup;
    }
    stage = "open_source";
    io.source_fd = open(argv[1], O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (io.source_fd < 0) { error = "source_open_failed"; goto cleanup; }
    if (fstat(io.source_fd, &io.source_before) || !S_ISREG(io.source_before.st_mode) ||
        io.source_before.st_size <= 0 ||
        (uint64_t)io.source_before.st_size > MB_PAK_V3_MAX_ARCHIVE_BYTES) {
        error = "source_type_or_size_refused"; goto cleanup;
    }
    io.source_bytes = (uint64_t)io.source_before.st_size;

    stage = "allocate_metadata";
    reader.struct_size = sizeof(reader); reader.abi_version = 1u;
    reader.index_capacity = MB_PAK_V3_MAX_INDEX_BYTES;
    reader.index = allocate(&a, reader.index_capacity, 1u);
    reader.entries_capacity = MB_PAK_V3_MAX_FILES;
    reader.entries = allocate(&a, reader.entries_capacity, sizeof(*reader.entries));
    reader.names_capacity = MB_PAK_V3_MAX_NAME_BYTES;
    reader.names = allocate(&a, reader.names_capacity, 1u);
    reader.sort_capacity = MB_PAK_V3_MAX_FILES;
    reader.sort_indices = allocate(&a, reader.sort_capacity, sizeof(*reader.sort_indices));
    reader.transfer_bytes = MB_PAK_V3_TRANSFER_BYTES;
    reader.transfer = allocate(&a, reader.transfer_bytes, 1u);
    /* Metadata-only mode: no selected payload buffer or whole-asset allocation. */
    if (!reader.index || !reader.entries || !reader.names || !reader.sort_indices || !reader.transfer) {
        error = "metadata_allocation_failed"; goto cleanup;
    }
    pak_source_v1 source = {sizeof(source), 1u, io.source_bytes, 1u,
        PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE, 0u, &io, source_read};
    mb_pak_v3_limits_v1 read_limits;
    mb_pak_v3_limits_v1_default(&read_limits);
    stage = "read_metadata";
    if (mb_pak_v3_read_none_v1(&source, &read_limits, 0u, 0u, &reader, &metadata) != MB_PAK_V3_OK ||
        !metadata.metadata_complete || metadata.selected_complete ||
        metadata.file_count > MB_PAK_STREAM_MAX_FILES || !source_stable(&io)) {
        error = "v3_metadata_refused"; goto cleanup;
    }
    io.entries = reader.entries;
    io.entry_count = metadata.file_count;

    stage = "derive_descriptors";
    mb_pak_stream_file_v1 *files = allocate(&a, MB_PAK_STREAM_MAX_FILES, sizeof(*files));
    if (!files) { error = "descriptor_allocation_failed"; goto cleanup; }
    for (uint32_t i = 0u; i < metadata.file_count; ++i) {
        const mb_pak_v3_entry_v1 *entry = &reader.entries[i];
        if (entry->path_offset > metadata.name_bytes ||
            entry->path_bytes >= metadata.name_bytes - entry->path_offset ||
            reader.names[entry->path_offset + entry->path_bytes] != 0u ||
            entry->payload_bytes > MB_PAK_STREAM_MAX_PAYLOAD_BYTES - payload_bytes) {
            error = "source_descriptor_bounds_refused"; goto cleanup;
        }
        payload_bytes += entry->payload_bytes;
        files[i].struct_size = sizeof(files[i]); files[i].abi_version = 1u;
        files[i].path = reader.names + entry->path_offset;
        files[i].path_bytes = entry->path_bytes;
        files[i].payload_bytes = entry->payload_bytes;
        files[i].verify_sha1 = 1u;
        memcpy(files[i].expected_sha1, entry->sha1, sizeof(entry->sha1));
    }
    work.records_capacity = MB_PAK_STREAM_MAX_FILES;
    work.records = allocate(&a, work.records_capacity, sizeof(*work.records));
    work.directories_capacity = MB_PAK_STREAM_MAX_DIRECTORIES;
    work.directories = allocate(&a, work.directories_capacity, sizeof(*work.directories));
    work.order_capacity = MB_PAK_STREAM_MAX_FILES;
    work.order = allocate(&a, work.order_capacity, sizeof(*work.order));
    work.index_capacity = MB_PAK_STREAM_MAX_INDEX_BYTES;
    work.index_arena = allocate(&a, work.index_capacity, 1u);
    work.transfer_bytes = MB_PAK_STREAM_TRANSFER_BYTES;
    work.transfer = allocate(&a, work.transfer_bytes, 1u);
    if (!work.records || !work.directories || !work.order || !work.index_arena || !work.transfer) {
        error = "writer_allocation_failed"; goto cleanup;
    }

    stage = "create_private_output";
    io.output_fd = open(argv[2], O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (io.output_fd < 0) { error = "fresh_output_creation_refused"; goto cleanup; }
    output_owned = 1;
    if (fstat(io.output_fd, &io.output_created) || !S_ISREG(io.output_created.st_mode) ||
        io.output_created.st_size != 0 ||
        (io.output_created.st_dev == io.source_before.st_dev &&
         io.output_created.st_ino == io.source_before.st_ino)) {
        error = "output_identity_refused"; goto cleanup;
    }
    mb_pak_stream_profile_v1 profile = {sizeof(profile), 1u, metadata.mount, metadata.mount_bytes, 0u, seed};
    mb_pak_stream_limits_v1 write_limits;
    mb_pak_stream_limits_v1_default(&write_limits);
    write_limits.max_archive_bytes = io.output_cap;
    mb_pak_stream_io_v1 writer_io = {sizeof(writer_io), 1u, &io, payload_read, output_write, 1u, 1u};
    stage = "stream_v11";
    if (mb_pak_stream_write_v11_none(&profile, files, metadata.file_count,
                                    &write_limits, &writer_io, &work, &written) != MB_PAK_STREAM_OK ||
        !written.archive_complete) {
        error = "v11_stream_refused"; goto cleanup;
    }

    stage = "finalize_private_artifact";
    struct stat output_now, path_now;
    snapshot_stable = source_stable(&io);
    if (!snapshot_stable) { error = "source_snapshot_changed"; goto cleanup; }
    if (fstat(io.output_fd, &output_now) || !S_ISREG(output_now.st_mode) ||
        output_now.st_dev != io.output_created.st_dev || output_now.st_ino != io.output_created.st_ino ||
        output_now.st_size < 0 || (uint64_t)output_now.st_size != written.archive_bytes ||
        lstat(argv[2], &path_now) || !S_ISREG(path_now.st_mode) ||
        path_now.st_dev != output_now.st_dev || path_now.st_ino != output_now.st_ino) {
        error = "completed_output_identity_or_size_refused"; goto cleanup;
    }
    observed_output_bytes = (uint64_t)output_now.st_size;
    output_size_valid = 1;
    if (fsync(io.output_fd)) { error = "output_fsync_failed"; goto cleanup; }
    output_fsynced = 1;
    if (close(io.output_fd)) {
        io.output_fd = -1; error = "output_close_outcome_uncertain"; goto cleanup;
    }
    io.output_fd = -1;
    output_closed = 1;
    if (!source_stable(&io)) { snapshot_stable = 0; error = "source_changed_after_finalize"; goto cleanup; }
    success = 1;
    stage = "complete";

cleanup:
    if (io.output_fd >= 0) {
        struct stat now;
        if (!fstat(io.output_fd, &now) && now.st_size >= 0) observed_output_bytes = (uint64_t)now.st_size;
        if (close(io.output_fd) && strcmp(error, "none") == 0) error = "output_close_outcome_uncertain";
    }
    if (io.source_fd >= 0) {
        if (!io.source_changed && io.source_bytes) {
            snapshot_stable = source_stable(&io);
            if (!snapshot_stable) {
                success = 0;
                if (strcmp(error, "none") == 0) {
                    error = "source_changed_at_final_cleanup_check";
                    stage = "final_source_snapshot";
                }
            }
        }
        if (close(io.source_fd)) { success = 0; error = "source_close_outcome_uncertain"; }
    }
    release_all(&a);
    printf("{\"phase\":\"pak_v3_to_v11_preparation\",\"stage\":\"%s\",\"status\":\"%s\","
           "\"error\":\"%s\",\"source_bytes\":%" PRIu64 ",\"source_version\":3,"
           "\"output_version\":11,\"path_hash_seed\":%" PRIu64 ",\"mount\":",
           stage, success ? "pass" : "fail", error, io.source_bytes, seed);
    json_string(metadata.mount, metadata.mount_bytes);
    printf(",\"mount_bytes\":%u,\"file_count\":%u,\"payload_bytes\":%" PRIu64 ","
           "\"metadata_complete\":%s,\"reader_status\":%u,\"reader_stage\":%u,"
           "\"source_index_sha1\":\"", metadata.mount_bytes, metadata.file_count, payload_bytes,
           metadata.metadata_complete ? "true" : "false", (unsigned)metadata.status, (unsigned)metadata.stage);
    hex(metadata.index_sha1, sizeof(metadata.index_sha1));
    printf("\",\"reader_requested_bytes\":%" PRIu64 ",\"reader_calls\":%u,"
           "\"writer_status\":%u,\"writer_complete\":%s,\"artifact_complete\":%s,"
           "\"archive_bytes\":%" PRIu64 ",\"observed_output_bytes\":%" PRIu64 ","
           "\"output_cap\":%" PRIu64 ",\"source_snapshot_stable\":%s,"
           "\"output_created_exclusively\":%s,\"output_size_valid\":%s,"
           "\"output_fsynced\":%s,\"output_closed\":%s,\"unpublished\":true,"
           "\"unpublished_partial_retained\":%s,\"requested_job_io_bytes\":%" PRIu64 ","
           "\"job_io_calls\":%u,\"allocation_peak_bytes\":%" PRIu64 ","
           "\"allocations\":%u,\"releases\":%u,\"live_allocations\":%" PRIu64 ","
           "\"source_paths_used_as_destinations\":false,\"install_supported\":false,"
           "\"base_merge_claimed\":false,\"runtime_success_claimed\":false}\n",
           metadata.requested_bytes, metadata.read_calls, (unsigned)written.status,
           written.archive_complete ? "true" : "false", success ? "true" : "false",
           written.archive_bytes, observed_output_bytes, io.output_cap,
           snapshot_stable ? "true" : "false", output_owned ? "true" : "false",
           output_size_valid ? "true" : "false", output_fsynced ? "true" : "false",
           output_closed ? "true" : "false", output_owned && !success ? "true" : "false",
           io.requested_bytes, io.calls, a.peak, a.count, a.released, a.bytes);
    return success ? 0 : 1;
}
