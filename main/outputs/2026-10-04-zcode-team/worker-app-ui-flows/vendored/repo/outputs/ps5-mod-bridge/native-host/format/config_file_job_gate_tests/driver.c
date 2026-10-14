#define _DARWIN_C_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "config_file_job.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifdef MB_CONFIG_GATE_LEGACY_BASELINE
pm_config_file_job_status pm_gate_legacy_v1(const pm_config_file_job_params_v1 *,
    pm_config_file_job_result_v1 *, mb_pak_prepare_gate_v1, void *);
pm_config_file_job_status pm_gate_legacy_v2(const pm_config_file_job_params_v2 *,
    pm_config_file_job_result_v2 *, mb_pak_prepare_gate_v1, void *);
#define gated_v1 pm_gate_legacy_v1
#define gated_v2 pm_gate_legacy_v2
#else
#define gated_v1 pm_config_file_job_gated_v1
#define gated_v2 pm_config_file_job_gated_v2
#endif

static unsigned checks, failures;
static void check(int ok, const char *label) {
    ++checks;
    if (!ok) { ++failures; (void)printf("FAIL %s\n", label); }
}
static uint64_t file_bytes(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 && st.st_size >= 0 ? (uint64_t)st.st_size : UINT64_MAX;
}
static int empty(const void *pointer, size_t bytes) {
    const uint8_t *p = pointer;
    for (size_t i = 0; i < bytes; ++i) if (p[i]) return 0;
    return 1;
}
static int no_digest(const pm_config_file_job_result_v1 *r) {
    return empty(r->merged_sha256, sizeof(r->merged_sha256)) &&
        empty(r->writer.primary_sha1, sizeof(r->writer.primary_sha1)) &&
        empty(r->writer.path_index_sha1, sizeof(r->writer.path_index_sha1)) &&
        empty(r->writer.directory_sha1, sizeof(r->writer.directory_sha1));
}
typedef struct allocation { void *pointer; size_t bytes; } allocation;
typedef struct context {
    int output;
    const char *mode;
    const pm_config_file_job_result_v1 *result;
    unsigned reads, writes, allocated, released, provider_errors;
    uint64_t live, peak;
    allocation blocks[32];
    unsigned gate_calls, stopped, reinvoked, premature_publication, final_release_seen;
    unsigned stopped_reads, stopped_writes;
    uint64_t stopped_size, began, deadline, elapsed;
} context;
static uint64_t monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) || ts.tv_sec < 0 || ts.tv_nsec < 0) return 0;
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}
static void *owned_allocate(void *opaque, size_t bytes) {
    context *c = opaque;
    if (c->allocated >= 32u || !bytes) { ++c->provider_errors; return NULL; }
    void *pointer = malloc(bytes);
    if (!pointer) return NULL;
    c->blocks[c->allocated++] = (allocation){pointer, bytes};
    c->live += bytes;
    if (c->live > c->peak) c->peak = c->live;
    return pointer;
}
static void owned_release(void *opaque, void *pointer) {
    context *c = opaque;
    for (unsigned i = 0; i < c->allocated; ++i) {
        if (c->blocks[i].pointer != pointer) continue;
        c->live -= c->blocks[i].bytes; c->blocks[i].pointer = NULL;
        ++c->released; free(pointer);
        if (c->result->archive_complete || c->result->writer.archive_complete || !no_digest(c->result))
            ++c->premature_publication;
        if (!strcmp(c->mode, "late") && !c->live && c->writes) {
            /* The trusted owned allocator actually frees its final block, then
             * crosses the caller's original monotonic deadline. No clock is
             * changed, reset or inferred from a callback count. */
            c->final_release_seen = 1u;
            for (;;) {
                uint64_t now = monotonic_ns();
                if (!now) { ++c->provider_errors; break; }
                if (now >= c->deadline + UINT64_C(5000000)) {
                    c->elapsed = now - c->began; break;
                }
                uint64_t remaining = c->deadline + UINT64_C(5000000) - now;
                struct timespec delay = {(time_t)(remaining / UINT64_C(1000000000)),
                    (long)(remaining % UINT64_C(1000000000))};
                if (nanosleep(&delay, NULL) && errno != EINTR) { ++c->provider_errors; break; }
            }
        }
        return;
    }
    ++c->provider_errors;
}
static int owned_read(void *opaque, int fd, uint64_t offset, uint8_t *data,
                      uint32_t requested, uint32_t *actual) {
    context *c = opaque; ++c->reads; *actual = 0;
    if (offset > INT64_MAX || requested > INT64_MAX - offset) return 1;
    ssize_t n = pread(fd, data, requested, (off_t)offset);
    if (n < 0) return 1;
    *actual = (uint32_t)n; return 0;
}
static int owned_write(void *opaque, int fd, uint64_t offset, const uint8_t *data,
                       uint32_t requested, uint32_t *actual) {
    context *c = opaque; ++c->writes; *actual = 0;
    if (offset > INT64_MAX || requested > INT64_MAX - offset) return 1;
    ssize_t n = pwrite(fd, data, requested, (off_t)offset);
    if (n < 0) return 1;
    *actual = (uint32_t)n; return 0;
}
static int operation_gate(void *opaque) {
    context *c = opaque; ++c->gate_calls;
    if (c->stopped) { ++c->reinvoked; return 0; }
    if (!c->began) { c->began = monotonic_ns(); c->deadline = c->began + UINT64_C(5000000000); }
    if (!strcmp(c->mode, "postread") && !c->reads) return 0;
    if (!strcmp(c->mode, "partial") && !file_bytes(c->output)) return 0;
    if (!strcmp(c->mode, "late")) {
        uint64_t now = monotonic_ns();
        if (now && now < c->deadline) return 0;
        c->elapsed = now >= c->began ? now - c->began : 0u;
    }
    c->stopped = 1u; c->stopped_size = file_bytes(c->output);
    c->stopped_reads = c->reads; c->stopped_writes = c->writes;
    return 1;
}
static unsigned invalid_gate_calls;
static int invalid_gate(void *context_pointer) {
    (void)context_pointer; ++invalid_gate_calls; return 1;
}

int main(int argc, char **argv) {
    if (argc != 5) return 2;
    const char *mode = argv[4];
    if (strcmp(mode, "admission") && strcmp(mode, "admission-v1") && strcmp(mode, "postread") &&
        strcmp(mode, "partial") && strcmp(mode, "late") && strcmp(mode, "invalid") &&
        strcmp(mode, "legacy-v1") && strcmp(mode, "legacy-v2")) return 2;
    if (mkdir(argv[3], 0700)) return 2;
    int root = open(argv[3], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    int base = open(argv[1], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    int member = open(argv[2], O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    int output = root < 0 ? -1 : openat(root, "artifact.pak",
        O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (base < 0 || member < 0 || output < 0) return 2;
    context c = {0}; c.output = output; c.mode = mode;
    static const uint8_t platform[] = "PS5", section[] = "SystemSettings";
    const pm_ini_bytes sections[] = {{section, sizeof(section) - 1u}};
    pm_config_file_job_params_v2 p = {0};
    p.struct_size = sizeof(p); p.abi_version = 2u;
    p.base.struct_size = sizeof(p.base); p.base.abi_version = 1u;
    p.base.original_fd = base; p.base.member_fd = member; p.base.output_fd = output;
    p.base.immutable_inputs = p.base.unpublished_output = 1u;
    p.base.original_bytes = file_bytes(base); p.base.member_bytes = file_bytes(member);
    p.base.sections = sections; p.base.section_count = 1u;
    p.base.io_context = p.base.allocator_context = &c;
    p.base.read_at = owned_read; p.base.write_at = owned_write;
    p.base.allocate = owned_allocate; p.base.release = owned_release;
    p.platform = (pm_ini_bytes){platform, sizeof(platform) - 1u};
    pm_config_file_job_result_v2 result;
    memset(&result, 0xa5, sizeof(result));
    c.result = &result.preparation;
    check(lseek(base, 7, SEEK_SET) == 7 && lseek(member, 3, SEEK_SET) == 3 &&
        lseek(output, 11, SEEK_SET) == 11, "set caller-owned file cursors");
    const int v1 = !strcmp(mode, "admission-v1") || !strcmp(mode, "legacy-v1");
    if (!strcmp(mode, "late")) {
        c.began = monotonic_ns(); c.deadline = c.began + UINT64_C(5000000000);
        if (!c.began) return 2;
    }
    if (v1 || !strcmp(mode, "invalid")) {
        static const uint8_t target[] = "Townfall/Platforms/PS5/Config/PS5Engine.ini", mount[] = "../../../";
        p.base.target_entry = (pm_ini_bytes){target, sizeof(target) - 1u};
        p.base.mount = (pm_ini_bytes){mount, sizeof(mount) - 1u}; p.base.path_hash_seed = UINT64_C(636187772);
    }
    pm_config_file_job_status status;
    const int legacy = !strcmp(mode, "legacy-v1") || !strcmp(mode, "legacy-v2");
    if (!strcmp(mode, "invalid")) {
        pm_config_file_job_result_v2 original = result;
        pm_config_file_job_params_v2 original_params = p;
        pm_config_file_job_status a = gated_v1(&p.base, &result.preparation, NULL, &c);
        check(a == PM_CONFIG_FILE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "v1 NULL predicate plus context leaves whole owning storage unchanged");
        pm_config_file_job_status b = gated_v1(&p.base, &result.preparation, invalid_gate, &result.preparation);
        check(b == PM_CONFIG_FILE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "v1 first-byte context alias refuses before callback or result mutation");
        p.base.target_entry = (pm_ini_bytes){NULL, 0}; p.base.mount = (pm_ini_bytes){NULL, 0}; p.base.path_hash_seed = 0;
        pm_config_file_job_status d = gated_v2(&p, &result, NULL, &c);
        check(d == PM_CONFIG_FILE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "v2 NULL predicate plus context leaves whole owning storage unchanged");
        pm_config_file_job_status e = gated_v2(&p, &result, invalid_gate, &result);
        check(e == PM_CONFIG_FILE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "v2 first-byte context alias refuses before callback or result mutation");
        pm_config_file_job_status f = gated_v2(&p, &result, invalid_gate, (void *)p.platform.data);
        check(f == PM_CONFIG_FILE_JOB_BAD_ARGUMENT && !memcmp(&result, &original, sizeof(result)),
            "v2 first-byte immutable platform alias is refused without mutation");
        original_params.base.target_entry = (pm_ini_bytes){NULL, 0};
        original_params.base.mount = (pm_ini_bytes){NULL, 0}; original_params.base.path_hash_seed = 0;
        check(!memcmp(&p, &original_params, sizeof(p)) && !invalid_gate_calls && !c.reads &&
            !c.writes && !c.allocated && file_bytes(output) == 0u,
            "invalid gates preserve inputs and invoke no IO/allocation/predicate");
        check(lseek(base, 0, SEEK_CUR) == 7 && lseek(member, 0, SEEK_CUR) == 3 &&
            lseek(output, 0, SEEK_CUR) == 11 && close(output) == 0 && close(member) == 0 &&
            close(base) == 0 && close(root) == 0, "invalid gates preserve caller FD ownership/cursors");
        (void)printf("{\"case\":\"invalid\",\"checks\":%u,\"failures\":%u,"
            "\"predicate_calls\":%u,\"reads\":%u,\"writes\":%u,\"allocations\":%u}\n",
            checks, failures, invalid_gate_calls, c.reads, c.writes, c.allocated);
        return failures ? 1 : 0;
    }
    if (legacy) status = v1 ? pm_config_file_job_v1(&p.base, &result.preparation) : pm_config_file_job_v2(&p, &result);
    else status = v1 ? gated_v1(&p.base, &result.preparation, operation_gate, &c) :
        gated_v2(&p, &result, operation_gate, &c);
    const pm_config_file_job_result_v1 *r = &result.preparation;
    if (legacy) {
        check(status == PM_CONFIG_FILE_JOB_OK && r->status == status && r->archive_complete &&
            r->writer.archive_complete && file_bytes(output) == UINT64_C(297483249),
            "legacy API real conversion returns complete expected archive");
        check(r->merged_bytes == 3626u && r->scalar_winners == 30u && r->replacements == 2u &&
            r->additions == 28u && !no_digest(r), "legacy API unchanged published scalar merge preserved");
        check(!c.gate_calls && !c.premature_publication,
            "legacy wrapper has no predicate and completion stays staged through cleanup");
        if (!v1) {
            static const char expected_target[] = "Townfall/Platforms/PS5/Config/PS5Engine.ini";
            check(!p.base.target_entry.data && !p.base.target_entry.size && !p.base.mount.data &&
                !p.base.mount.size && !p.base.path_hash_seed && result.selector_status == 0u &&
                result.selector_root_count == 1u && result.selector_target_count == 1u &&
                result.resolved_path_bytes == sizeof(expected_target) - 1u &&
                !memcmp(result.resolved_path, expected_target, sizeof(expected_target) - 1u) &&
                result.resolved_mount_bytes == 9u && !memcmp(result.resolved_mount, "../../../", 9u) &&
                result.resolved_path_hash_seed == UINT64_C(636187772),
                "automatic v2 derives exact identity from actual metadata without supplied target/mount/seed");
        }
    } else {
    check(status == PM_CONFIG_FILE_JOB_STOPPED && r->status == status,
        "public gate return agrees with STOPPED owning result");
    check(!r->archive_complete && !r->writer.archive_complete && no_digest(r),
        "stop has no parent/writer completion or digest claim");
    check(c.stopped && c.gate_calls >= 1u && !c.reinvoked,
        "first stop latches and predicate is never reinvoked");
    check(c.reads == c.stopped_reads && c.writes == c.stopped_writes && file_bytes(output) == c.stopped_size,
        "terminal stop causes no later read/write callback or output bytes");
    if (!strcmp(mode, "admission") || !strcmp(mode, "admission-v1")) {
        check(c.gate_calls == 1u && !c.reads && !c.writes && !c.allocated && file_bytes(output) == 0u,
            "preadmission cancellation invokes no IO or allocator callback");
    } else if (!strcmp(mode, "postread")) {
        check(c.reads == 1u && !c.writes && c.allocated && file_bytes(output) == 0u,
            "stop immediately after actual first read releases admitted owners before output");
    } else if (!strcmp(mode, "partial")) {
        check(c.writes == 1u && c.stopped_size > 0u && c.stopped_size < UINT64_C(297483249) &&
            c.allocated && r->writer.status == MB_PAK_STREAM_STOPPED,
            "actual partial streamed output propagates writer STOPPED with no later write");
    } else {
        check(c.final_release_seen && c.elapsed >= UINT64_C(5000000000) && !c.premature_publication &&
            file_bytes(output) == UINT64_C(297483249),
            "final physical release crosses actual monotonic deadline before completion publication");
    }
    }
    check(!c.live && c.allocated == c.released && !c.provider_errors &&
        r->allocation_live_after_cleanup == 0u && r->allocation_calls == r->release_calls,
        "all admitted allocation owners are balanced");
    check(fcntl(base, F_GETFD) >= 0 && fcntl(member, F_GETFD) >= 0 && fcntl(output, F_GETFD) >= 0 &&
        lseek(base, 0, SEEK_CUR) == 7 && lseek(member, 0, SEEK_CUR) == 3 &&
        lseek(output, 0, SEEK_CUR) == 11, "borrowed file descriptors and cursors survive");
    uint64_t size = file_bytes(output);
    check(close(output) == 0 && close(member) == 0 && close(base) == 0 && close(root) == 0,
        "harness retains descriptor ownership through close");
    (void)printf("{\"case\":\"%s\",\"checks\":%u,\"failures\":%u,"
        "\"return_status\":%u,\"result_status\":%u,\"complete\":%u,"
        "\"gate_calls\":%u,\"reinvoked\":%u,\"reads\":%u,\"writes\":%u,"
        "\"allocations\":%u,\"releases\":%u,\"live_bytes\":%" PRIu64 ","
        "\"output_bytes\":%" PRIu64 ",\"premature_publication\":%u,"
        "\"final_release_seen\":%u,\"elapsed_ns\":%" PRIu64 ",\"automatic_v2\":%s,"
        "\"target_mount_seed_supplied\":%s,\"console_executed\":false}\n", mode,
        checks, failures, (unsigned)status, (unsigned)r->status, r->archive_complete,
        c.gate_calls, c.reinvoked, c.reads, c.writes, c.allocated, c.released, c.live, size,
        c.premature_publication, c.final_release_seen, c.elapsed, v1 ? "false" : "true", v1 ? "true" : "false");
    return failures ? 1 : 0;
}
