#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "pak_archive_job.h"
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

#ifdef MB_GATE_LEGACY_BASELINE
mb_pak_archive_job_status_v1 mb_gate_legacy_archive(
    const mb_pak_archive_job_params_v1 *, mb_pak_archive_job_workspace_v1 *,
    mb_pak_archive_job_result_v1 *, mb_pak_prepare_gate_v1, void *);
#define archive_gated mb_gate_legacy_archive
mb_pak_stream_status mb_gate_legacy_stream(
    const mb_pak_stream_profile_v1 *, const mb_pak_stream_file_v1 *, uint32_t,
    const mb_pak_stream_limits_v1 *, const mb_pak_stream_io_v1 *,
    mb_pak_stream_workspace_v1 *, mb_pak_stream_result_v1 *,
    mb_pak_prepare_gate_v1, void *);
mb_pak_union_status_v1 mb_gate_legacy_union(const mb_pak_union_params_v1 *,
    mb_pak_union_result_v1 *, mb_pak_prepare_gate_v1, void *);
#define stream_gated mb_gate_legacy_stream
#define union_gated mb_gate_legacy_union
#else
#define archive_gated mb_pak_archive_job_prepare_gated_v1
#define stream_gated mb_pak_stream_write_v11_none_gated_v1
#define union_gated mb_pak_union_prepare_gated_v1
#endif

static unsigned checks, failures;
static unsigned forbidden_gate_calls;
static int forbidden_gate(void *context) {
    (void)context; ++forbidden_gate_calls; return 1;
}
static void check(int ok, const char *label) {
    ++checks;
    if (!ok) { ++failures; (void)printf("FAIL %s\n", label); }
}

static int hex32(const char *text, uint8_t out[32]) {
    if (!text || strlen(text) != 64u) return 0;
    for (unsigned i = 0; i < 32u; ++i) {
        unsigned value = 0;
        for (unsigned j = 0; j < 2u; ++j) {
            unsigned char c = (unsigned char)text[i * 2u + j];
            unsigned nibble;
            if (c >= '0' && c <= '9') nibble = c - '0';
            else if (c >= 'a' && c <= 'f') nibble = c - 'a' + 10u;
            else return 0;
            value = value * 16u + nibble;
        }
        out[i] = (uint8_t)value;
    }
    return 1;
}

static uint64_t bytes(int fd) {
    struct stat st;
    return fd >= 0 && fstat(fd, &st) == 0 && st.st_size >= 0 ?
           (uint64_t)st.st_size : UINT64_MAX;
}

static unsigned entries(int fd) {
    int copy = dup(fd);
    DIR *dir = copy < 0 ? NULL : fdopendir(copy);
    if (!dir) { if (copy >= 0) (void)close(copy); return UINT32_MAX; }
    rewinddir(dir);
    unsigned count = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = readdir(dir)) != NULL)
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) ++count;
    if (errno || closedir(dir)) return UINT32_MAX;
    return count;
}

typedef struct output_context {
    int root_fd, fd;
    unsigned calls;
} output_context;
typedef struct gate_context {
    unsigned calls, stopped, reinvoked;
    const char *mode;
    const mb_pak_archive_job_result_v1 *result;
    output_context *output;
    int quarantine_fd;
    uint64_t stopped_bytes;
    unsigned materialized_seen;
} gate_context;

static int acquire(void *opaque, int *fd) {
    output_context *out = opaque;
    ++out->calls;
    out->fd = openat(out->root_fd, "artifact.pak",
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    *fd = out->fd;
    return out->fd < 0;
}

static int operation_gate(void *opaque) {
    gate_context *gate = opaque;
    ++gate->calls;
    if (gate->stopped) { ++gate->reinvoked; return 0; }
    if (!strcmp(gate->mode, "materialization")) {
        if (!gate->result->materialization_complete) return 0;
        gate->materialized_seen = entries(gate->quarantine_fd);
    } else if (!strcmp(gate->mode, "partial")) {
        if (gate->output->fd < 0 || bytes(gate->output->fd) == 0u) return 0;
        gate->stopped_bytes = bytes(gate->output->fd);
    }
    gate->stopped = 1u;
    return 1;
}

static int zeros(const void *data, size_t length) {
    const unsigned char *p = data;
    for (size_t i = 0; i < length; ++i) if (p[i]) return 0;
    return 1;
}

static int digest_claims_empty(const mb_pak_stream_result_v1 *r) {
    return zeros(r->primary_sha1, sizeof(r->primary_sha1)) &&
           zeros(r->path_index_sha1, sizeof(r->path_index_sha1)) &&
           zeros(r->directory_sha1, sizeof(r->directory_sha1));
}

static uint64_t monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) || ts.tv_sec < 0 || ts.tv_nsec < 0)
        return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

typedef struct stream_context {
    int fd;
    unsigned reads, writes;
} stream_context;

static int owned_read(void *opaque, uint32_t file, uint64_t offset,
                      uint8_t *data, uint32_t count, uint32_t *actual) {
    stream_context *io = opaque;
    ++io->reads;
    if (offset > 1u || count > 1u - offset) return 1;
    if (count) data[0] = (uint8_t)file;
    *actual = count;
    return 0;
}

static int owned_write(void *opaque, uint64_t offset, const uint8_t *data,
                       uint32_t count, uint32_t *actual) {
    stream_context *io = opaque;
    ++io->writes;
    if (offset > INT64_MAX || count > INT64_MAX - offset) return 1;
    ssize_t n = pwrite(io->fd, data, count, (off_t)offset);
    if (n < 0) return 1;
    *actual = (uint32_t)n;
    return 0;
}

typedef struct stream_gate_context {
    const char *mode;
    mb_pak_stream_workspace_v1 *workspace;
    stream_context *io;
    unsigned calls, stopped, reinvoked, cpu_seen, cleanup_seen;
    uint64_t deadline, stopped_bytes, elapsed_ns, began;
} stream_gate_context;

static int scratch_empty(const mb_pak_stream_workspace_v1 *w) {
    return zeros(w->records, w->records_capacity * sizeof(*w->records)) &&
        zeros(w->directories, w->directories_capacity * sizeof(*w->directories)) &&
        zeros(w->order, w->order_capacity * sizeof(*w->order)) &&
        zeros(w->index_arena, w->index_capacity) && zeros(w->transfer, w->transfer_bytes);
}

static int stream_gate(void *opaque) {
    stream_gate_context *gate = opaque;
    ++gate->calls;
    if (gate->stopped) { ++gate->reinvoked; return 0; }
    if (!gate->began) {
        gate->began = monotonic_ns();
        gate->deadline = gate->began + UINT64_C(2000000);
        return 0;
    }
    if (!strcmp(gate->mode, "cpu")) {
        /* Real owned descriptor processing has begun. An intentional bounded
         * delay at the predicate makes its actual monotonic deadline expire;
         * callback counts are not interpreted as elapsed time. */
        if (gate->workspace->order[1] != 1u ||
            gate->workspace->records[1].input_index != 1u) return 0;
        gate->cpu_seen = 1u;
        const struct timespec delay = {0, 5000000};
        if (nanosleep(&delay, NULL)) return 1;
        uint64_t now = monotonic_ns();
        gate->elapsed_ns = now - gate->began;
        if (now < gate->deadline) return 0;
    } else {
        if (!gate->io->writes || !scratch_empty(gate->workspace)) return 0;
        gate->cleanup_seen = 1u;
    }
    gate->stopped_bytes = bytes(gate->io->fd);
    gate->stopped = 1u;
    return 1;
}

static int stream_control(const char *mode, const char *directory) {
    if (mkdir(directory, 0700)) return 2;
    int root = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int output = root < 0 ? -1 : openat(root, "artifact.pak",
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (output < 0) return 2;
    const uint32_t count = !strcmp(mode, "cpu") ? 512u : 2u;
    mb_pak_stream_file_v1 *files = calloc(count, sizeof(*files));
    char (*names)[64] = calloc(count, sizeof(*names));
    mb_pak_stream_workspace_v1 workspace = {0};
    workspace.records_capacity = workspace.order_capacity = count;
    workspace.directories_capacity = 16u;
    workspace.index_capacity = 131072u;
    workspace.transfer_bytes = MB_PAK_STREAM_TRANSFER_BYTES;
    workspace.records = malloc(count * sizeof(*workspace.records));
    workspace.order = malloc(count * sizeof(*workspace.order));
    workspace.directories = malloc(16u * sizeof(*workspace.directories));
    workspace.index_arena = malloc(workspace.index_capacity);
    workspace.transfer = malloc(workspace.transfer_bytes);
    if (!files || !names || !workspace.records || !workspace.order ||
        !workspace.directories || !workspace.index_arena || !workspace.transfer) return 2;
    memset(workspace.records, 0xa5, count * sizeof(*workspace.records));
    memset(workspace.order, 0xa5, count * sizeof(*workspace.order));
    memset(workspace.directories, 0xa5, 16u * sizeof(*workspace.directories));
    memset(workspace.index_arena, 0xa5, workspace.index_capacity);
    memset(workspace.transfer, 0xa5, workspace.transfer_bytes);
    for (uint32_t i = 0; i < count; ++i) {
        int n = snprintf(names[i], sizeof(*names), "Owned/Game/Asset%04u.bin", i);
        if (n < 0 || (unsigned)n >= sizeof(*names)) return 2;
        files[i].struct_size = sizeof(files[i]); files[i].abi_version = 1u;
        files[i].path = (const uint8_t *)names[i]; files[i].path_bytes = (uint32_t)n;
        files[i].payload_bytes = 1u;
    }
    static const uint8_t mount[] = "../../../Owned/";
    mb_pak_stream_profile_v1 profile = {sizeof(profile), 1u, mount,
        sizeof(mount) - 1u, 0u, UINT64_C(0x1234)};
    mb_pak_stream_limits_v1 limits;
    mb_pak_stream_limits_v1_default(&limits); limits.max_directories = 16u;
    stream_context io_context = {output, 0, 0};
    mb_pak_stream_io_v1 io = {sizeof(io), 1u, &io_context, owned_read, owned_write, 1u, 1u};
    mb_pak_stream_result_v1 result;
    memset(&result, 0x5a, sizeof(result));
    stream_gate_context gate = {mode, &workspace, &io_context, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    check(lseek(output, 11, SEEK_SET) == 11, "set owned output cursor");
    if (!strcmp(mode, "invalid")) {
        mb_pak_stream_result_v1 original = result;
        mb_pak_stream_status one = stream_gated(&profile, files, count, &limits,
            &io, &workspace, &result, NULL, &gate);
        check(one == MB_PAK_STREAM_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "stream NULL predicate with context refuses without result mutation");
        mb_pak_stream_status two = stream_gated(&profile, files, count, &limits,
            &io, &workspace, &result, forbidden_gate, &result);
        check(two == MB_PAK_STREAM_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "stream first-byte context alias refuses without result mutation");
        check(!io_context.reads && !io_context.writes && !forbidden_gate_calls &&
            bytes(output) == 0u && workspace.transfer[0] == 0xa5 &&
            workspace.transfer[workspace.transfer_bytes - 1u] == 0xa5,
            "invalid stream gates invoke no providers and preserve scratch");
        check(lseek(output, 0, SEEK_CUR) == 11 && close(output) == 0 && close(root) == 0,
            "invalid stream gates preserve caller descriptor ownership/cursor");
        free(files); free(names); free(workspace.records); free(workspace.order);
        free(workspace.directories); free(workspace.index_arena); free(workspace.transfer);
        return failures ? 1 : 0;
    }
    mb_pak_stream_status status = stream_gated(&profile, files, count, &limits,
        &io, &workspace, &result, stream_gate, &gate);
    check(status == MB_PAK_STREAM_STOPPED && result.status == status,
          "stream stop returns STOPPED");
    check(!result.archive_complete && !result.archive_bytes && digest_claims_empty(&result),
          "stream stop revokes artifact bytes and digests");
    check(gate.stopped && !gate.reinvoked, "stream stop latches one-shot predicate");
    check(scratch_empty(&workspace), "owned scratch is erased unconditionally");
    check(bytes(output) == gate.stopped_bytes, "no output after terminal gate");
    if (!strcmp(mode, "cpu")) {
        check(gate.cpu_seen && gate.elapsed_ns >= UINT64_C(2000000) &&
            !io_context.reads && !io_context.writes && bytes(output) == 0u,
            "actual monotonic expiry during CPU preflight precedes provider I/O");
    } else {
        check(gate.cleanup_seen && io_context.reads == count && io_context.writes &&
            bytes(output) > 0u, "late stop observes real full scratch cleanup after output");
    }
    check(fcntl(output, F_GETFD) >= 0 && lseek(output, 0, SEEK_CUR) == 11,
          "owned output descriptor and cursor survive");
    uint64_t size = bytes(output);
    check(close(output) == 0 && close(root) == 0, "close stream harness descriptors");
    free(files); free(names); free(workspace.records); free(workspace.order);
    free(workspace.directories); free(workspace.index_arena); free(workspace.transfer);
    (void)printf("{\"case\":\"%s\",\"checks\":%u,\"failures\":%u,"
        "\"return_status\":%u,\"result_status\":%u,\"complete\":%u,"
        "\"gate_calls\":%u,\"reinvoked\":%u,\"cpu_seen\":%u,"
        "\"cleanup_seen\":%u,\"elapsed_ns\":%" PRIu64 ",\"reads\":%u,"
        "\"writes\":%u,\"output_bytes\":%" PRIu64 "}\n", mode, checks, failures,
        (unsigned)status, (unsigned)result.status, result.archive_complete, gate.calls,
        gate.reinvoked, gate.cpu_seen, gate.cleanup_seen, gate.elapsed_ns,
        io_context.reads, io_context.writes, size);
    return failures ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc != 6 && argc != 7) {
        (void)fprintf(stderr, "usage: gate-driver BASE ORIGINAL_ZIP SHA256 TARGET_HEX FRESH_DIR [CASE]\n");
        return 2;
    }
    const char *mode = argc == 7 ? argv[6] : "admission";
    if (!strcmp(mode, "cpu") || !strcmp(mode, "cleanup")) return stream_control(mode, argv[5]);
    if (strcmp(mode, "admission") && strcmp(mode, "materialization") &&
        strcmp(mode, "partial") && strcmp(mode, "legacy") && strcmp(mode, "default") &&
        strcmp(mode, "invalid")) return 2;
    if (mkdir(argv[5], 0700) != 0) return 2;
    int root = open(argv[5], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int base = open(argv[1], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    int source = open(argv[2], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (root < 0 || base < 0 || source < 0 || mkdirat(root, "quarantine", 0700)) return 2;
    int quarantine = openat(root, "quarantine", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (quarantine < 0) return 2;
    mb_pak_archive_job_workspace_v1 *workspace = calloc(1, sizeof(*workspace));
    if (!workspace) return 2;
    workspace->struct_size = sizeof(*workspace);
    workspace->abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1;
    mb_pak_archive_job_params_v1 params;
    mb_pak_archive_job_result_v1 result;
    memset(&params, 0, sizeof(params)); memset(&result, 0, sizeof(result));
    params.struct_size = sizeof(params); params.abi_version = MB_PAK_ARCHIVE_JOB_ABI_V1;
    params.source.struct_size = sizeof(params.source); params.source.abi_version = MB_ARCHIVE_ABI_V1;
    params.source.fd = source; params.source.size = bytes(source);
    if (!hex32(argv[3], params.source.sha256) || !hex32(argv[4], params.source.target_token)) return 2;
    params.base_fd = base; params.base_bytes = bytes(base); params.quarantine_fd = quarantine;
    params.output_fd = -1;
    params.immutable_inputs = params.private_quarantine = params.unpublished_output = 1u;
    params.policy = MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY;
    output_context output = {root, -1, 0};
    gate_context gate = {0, 0, 0, mode, &result, &output, quarantine, 0, 0};
    params.acquire_output = acquire; params.output_context = &output;
    check(lseek(base, 7, SEEK_SET) == 7 && lseek(source, 3, SEEK_SET) == 3,
          "set borrowed input cursors");
    if (!strcmp(mode, "invalid")) {
        memset(&result, 0x5a, sizeof(result));
        mb_pak_archive_job_result_v1 original = result;
        mb_pak_archive_job_workspace_v1 *original_work = malloc(sizeof(*workspace));
        if (!original_work) return 2;
        memcpy(original_work, workspace, sizeof(*workspace));
        mb_pak_archive_job_status_v1 one = archive_gated(&params, workspace, &result, NULL, &gate);
        check(one == MB_PAK_ARCHIVE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)) &&
            !memcmp(workspace, original_work, sizeof(*workspace)),
            "archive NULL predicate with context refuses without mutation");
        mb_pak_archive_job_status_v1 two = archive_gated(&params, workspace, &result,
            forbidden_gate, &result);
        check(two == MB_PAK_ARCHIVE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)) &&
            !memcmp(workspace, original_work, sizeof(*workspace)),
            "archive first-byte context alias refuses without mutation");
        int direct_output = openat(root, "union.pak",
            O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
        if (direct_output < 0) return 2;
        mb_pak_union_params_v1 union_params = {sizeof(union_params), 1u, base, source,
            direct_output, 1u, 1u, 0u, bytes(base), bytes(source)};
        mb_pak_union_result_v1 union_result, union_original;
        memset(&union_result, 0x5a, sizeof(union_result)); union_original = union_result;
        mb_pak_union_status_v1 three = union_gated(&union_params, &union_result, NULL, &gate);
        check(three == MB_PAK_UNION_BAD_ARGUMENT &&
            !memcmp(&union_result, &union_original, sizeof(union_result)),
            "union NULL predicate with context refuses without mutation");
        mb_pak_union_status_v1 four = union_gated(&union_params, &union_result,
            forbidden_gate, &union_result);
        check(four == MB_PAK_UNION_BAD_ARGUMENT &&
            !memcmp(&union_result, &union_original, sizeof(union_result)),
            "union first-byte context alias refuses without mutation");
        check(!forbidden_gate_calls && !output.calls && !entries(quarantine) &&
            bytes(direct_output) == 0u, "invalid archive/union gates have no callback or file side effects");
        check(lseek(base, 0, SEEK_CUR) == 7 && lseek(source, 0, SEEK_CUR) == 3 &&
            close(direct_output) == 0 && close(quarantine) == 0 && close(source) == 0 &&
            close(base) == 0 && close(root) == 0, "invalid gates preserve borrowed descriptor ownership/cursors");
        free(original_work); free(workspace);
        size_t length = strlen(argv[5]) + 8u;
        char *stream_directory = malloc(length);
        if (!stream_directory) return 2;
        (void)snprintf(stream_directory, length, "%s-stream", argv[5]);
        int stream_status = stream_control(mode, stream_directory);
        free(stream_directory);
        (void)printf("{\"case\":\"invalid\",\"checks\":%u,\"failures\":%u,"
            "\"callback_calls\":%u,\"console_executed\":false}\n", checks, failures, forbidden_gate_calls);
        return stream_status;
    }
    if (!strcmp(mode, "default")) params.policy = MB_PAK_ARCHIVE_POLICY_DEFAULT;
    const int legacy = !strcmp(mode, "legacy") || !strcmp(mode, "default");
    mb_pak_archive_job_status_v1 status = legacy ?
        mb_pak_archive_job_prepare_v1(&params, workspace, &result) :
        archive_gated(&params, workspace, &result, operation_gate, &gate);
    if (legacy) {
        if (!strcmp(mode, "legacy")) {
            check(status == MB_PAK_ARCHIVE_JOB_OK && result.status == status &&
                result.preparation_complete && result.union_result.archive_complete &&
                result.union_result.writer.archive_complete, "old API normal preparation preserved");
            check(output.calls == 1u && bytes(output.fd) == UINT64_C(297439679) &&
                result.materialization_complete && entries(quarantine) == 5u,
                "old API completes the retained real archive/base fixture");
        } else {
            check(status == MB_PAK_ARCHIVE_JOB_ADDITIONAL_ACTION_REQUIRED && result.status == status &&
                !result.preparation_complete && !output.calls && !entries(quarantine),
                "old API default mixed-bundle refusal precedes output");
        }
        check(!gate.calls, "old API has no operation callback");
    } else {
    check(status == MB_PAK_ARCHIVE_JOB_STOPPED && result.status == status,
          "operation stop returns STOPPED");
    check(!result.preparation_complete && !result.union_result.archive_complete &&
          !result.union_result.output_size_valid && !result.union_result.writer.archive_complete &&
          digest_claims_empty(&result.union_result.writer),
          "operation stop publishes no artifact complete flags or digests");
    check(gate.calls >= 1u && gate.stopped == 1u && !gate.reinvoked,
          "one-shot stop is latched without reinvocation");
    if (!strcmp(mode, "admission")) {
    check(gate.calls == 1u, "admission is sampled once before work");
    check(output.calls == 0u && output.fd < 0 && entries(quarantine) == 0u,
          "admission stop creates no output or materialization");
    } else if (!strcmp(mode, "materialization")) {
        check(gate.materialized_seen == 5u && result.materialization_complete &&
            result.materialized.complete && result.materialized.selected_count == 3u &&
            output.calls == 0u && output.fd < 0,
            "all real members materialize before stop and deferred acquisition remains unused");
    } else {
        check(output.calls == 1u && output.fd >= 0 && result.materialization_complete &&
            gate.stopped_bytes > 0u && gate.stopped_bytes < UINT64_C(297439679) &&
            bytes(output.fd) == gate.stopped_bytes,
            "real partial output stops without any later write");
        check(fcntl(output.fd, F_GETFD) >= 0 && lseek(output.fd, 0, SEEK_CUR) == 0,
            "acquired output remains caller-owned with cursor preserved");
    }
    check(!result.installation_supported && !result.runtime_supported,
          "private preparation never claims installation or runtime");
    }
    check(fcntl(base, F_GETFD) >= 0 && fcntl(source, F_GETFD) >= 0 &&
          fcntl(quarantine, F_GETFD) >= 0 && lseek(base, 0, SEEK_CUR) == 7 &&
          lseek(source, 0, SEEK_CUR) == 3,
          "borrowed descriptors and file cursors survive");
    const uint64_t output_bytes = output.fd < 0 ? UINT64_C(0) : bytes(output.fd);
    const unsigned quarantine_entries = entries(quarantine);
    if (output.fd >= 0) check(close(output.fd) == 0, "close owned output");
    check(close(quarantine) == 0 && close(source) == 0 && close(base) == 0 &&
          close(root) == 0, "close borrowed harness owners");
    free(workspace);
    (void)printf("{\"case\":\"%s\",\"checks\":%u,\"failures\":%u,"
        "\"return_status\":%u,\"result_status\":%u,\"complete\":%u,"
        "\"gate_calls\":%u,\"reinvoked\":%u,\"acquire_calls\":%u,\"output_bytes\":%" PRIu64 ","
        "\"quarantine_entries\":%u,\"console_executed\":false}\n", mode,
        checks, failures, (unsigned)status, (unsigned)result.status,
        result.preparation_complete, gate.calls, gate.reinvoked, output.calls,
        output_bytes, quarantine_entries);
    return failures ? 1 : 0;
}
