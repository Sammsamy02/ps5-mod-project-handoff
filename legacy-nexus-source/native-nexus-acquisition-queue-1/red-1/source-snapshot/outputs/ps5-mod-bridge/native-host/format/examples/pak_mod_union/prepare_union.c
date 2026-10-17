#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif
#include "pak_primary_capture.h"
#include "pak_secondary_integrity.h"
#include "pak_directory_parser.h"
#include "pak_reference_graph.h"
#include "pak_physical_header.h"
#include "pak_candidate_reconcile.h"
#include "pak_v3_none_reader.h"
#include "pak_stream_writer.h"
#include "prepare_union.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Ordinary host preparation only. Archive names never become file targets. */
#define JOB_MAX_IO_BYTES UINT64_C(2147483648)
#define JOB_MAX_IO_CALLS 131072u
#define JOB_MAX_ALLOCATIONS 32u
#define JOB_MAX_ALLOCATED_BYTES UINT64_C(67108864)
#define JOB_MAX_PATH_COMPARE_BYTES UINT64_C(268435456)
#define INPUT_ROWS (MB_PAK_STREAM_MAX_FILES * 2u)

typedef struct job_io job_io;
typedef struct source_file {
    job_io *job;
    int fd;
    struct stat before;
    uint64_t bytes;
    uint32_t changed;
} source_file;
typedef struct payload_source {
    uint64_t data_offset, bytes;
    uint32_t source_ordinal;
} payload_source;
struct job_io {
    source_file sources[2];
    int output_fd;
    struct stat output_created;
    uint64_t requested_bytes;
    uint32_t calls;
    payload_source *rows;
    uint32_t row_count;
    mb_pak_prepare_gate_v1 gate;
    void *gate_context;
    uint32_t stopped, gate_work;
};
typedef struct allocations {
    void *blocks[JOB_MAX_ALLOCATIONS];
    size_t sizes[JOB_MAX_ALLOCATIONS];
    uint32_t count, released;
    uint64_t bytes, peak;
} allocations;
typedef struct path_ref {
    const uint8_t *path;
    uint32_t bytes, source_ordinal, source_row;
} path_ref;

static int gate_stopped(void *context) {
    job_io *io = context;
    if (io->stopped || (io->gate && io->gate(io->gate_context))) {
        io->stopped = 1u; return 1;
    }
    return 0;
}
static int check_gate(job_io *io) { return !gate_stopped(io); }
static int charge_gate_work(job_io *io, uint32_t n) {
    io->gate_work += n;
    if (io->gate_work >= 4096u) { io->gate_work = 0u; return check_gate(io); }
    return !io->stopped;
}

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
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static int source_stable(source_file *s) {
    struct stat now;
    if (fstat(s->fd, &now) || !S_ISREG(now.st_mode) ||
        !same_snapshot(&s->before, &now)) { s->changed = 1u; return 0; }
    return !s->changed;
}
static int all_sources_stable(job_io *io) {
    int stable = 1;
    for (unsigned i = 0u; i < 2u; ++i)
        if (io->sources[i].fd >= 0 && io->sources[i].bytes &&
            !source_stable(&io->sources[i])) stable = 0;
    return stable;
}
static int charge_io(job_io *io, uint32_t n) {
    if (!n || n > MB_PAK_STREAM_TRANSFER_BYTES || io->calls >= JOB_MAX_IO_CALLS ||
        io->requested_bytes > JOB_MAX_IO_BYTES || n > JOB_MAX_IO_BYTES - io->requested_bytes) return 0;
    ++io->calls;
    io->requested_bytes += n;
    return 1;
}
static pak_provider_status source_read(void *context, uint64_t offset,
                                     uint8_t *dst, uint32_t n, uint32_t *actual) {
    source_file *s = context;
    *actual = 0u;
    if (!dst || !n || n > MB_PAK_STREAM_TRANSFER_BYTES || offset > s->bytes ||
        n > s->bytes - offset || offset > INT64_MAX) return PAK_PROVIDER_CONTRACT_ERROR;
    if (!check_gate(s->job)) return PAK_PROVIDER_CANCELLED;
    if (!source_stable(s)) return PAK_PROVIDER_CHANGED;
    if (!check_gate(s->job)) return PAK_PROVIDER_CANCELLED;
    uint32_t done = 0u;
    while (done < n) {
        if (!check_gate(s->job)) { *actual = done; return PAK_PROVIDER_CANCELLED; }
        if (!charge_io(s->job, n - done)) { *actual = done; return PAK_PROVIDER_CONTRACT_ERROR; }
        ssize_t got = pread(s->fd, dst + done, n - done, (off_t)(offset + done));
        int saved_errno = errno;
        if (got > 0) done += (uint32_t)got;
        if (!check_gate(s->job)) { *actual = done; return PAK_PROVIDER_CANCELLED; }
        if (got < 0) {
            if (saved_errno == EINTR) continue;
            *actual = done; return PAK_PROVIDER_IO_ERROR;
        }
        if (!got) break;
    }
    *actual = done;
    int stable = source_stable(s);
    if (!check_gate(s->job)) return PAK_PROVIDER_CANCELLED;
    return stable ? PAK_PROVIDER_OK : PAK_PROVIDER_CHANGED;
}
static int read_all(source_file *s, uint64_t offset, uint8_t *dst, size_t bytes) {
    size_t done = 0u;
    while (done < bytes) {
        uint32_t n = (uint32_t)(bytes - done > MB_PAK_STREAM_TRANSFER_BYTES ?
                               MB_PAK_STREAM_TRANSFER_BYTES : bytes - done), actual = 0u;
        if (source_read(s, offset + done, dst + done, n, &actual) != PAK_PROVIDER_OK || actual != n) return 0;
        done += n;
    }
    return 1;
}
static int payload_read(void *context, uint32_t row, uint64_t offset,
                        uint8_t *dst, uint32_t n, uint32_t *actual) {
    job_io *io = context;
    *actual = 0u;
    if (row >= io->row_count) return 1;
    const payload_source *p = &io->rows[row];
    if (p->source_ordinal >= 2u || offset > p->bytes || n > p->bytes - offset) return 1;
    source_file *s = &io->sources[p->source_ordinal];
    if (p->data_offset > s->bytes || offset > s->bytes - p->data_offset) return 1;
    return source_read(s, p->data_offset + offset, dst, n, actual) != PAK_PROVIDER_OK;
}
static int output_write(void *context, uint64_t offset, const uint8_t *src,
                        uint32_t n, uint32_t *actual) {
    job_io *io = context;
    *actual = 0u;
    if (!src || !n || n > MB_PAK_STREAM_TRANSFER_BYTES || offset > MB_PAK_STREAM_MAX_ARCHIVE_BYTES ||
        n > MB_PAK_STREAM_MAX_ARCHIVE_BYTES - offset || offset > INT64_MAX) return 1;
    if (!check_gate(io) || !all_sources_stable(io) || !check_gate(io)) return 1;
    uint32_t done = 0u;
    while (done < n) {
        if (!check_gate(io)) { *actual = done; return 1; }
        if (!charge_io(io, n - done)) { *actual = done; return 1; }
        ssize_t put = pwrite(io->output_fd, src + done, n - done, (off_t)(offset + done));
        int saved_errno = errno;
        if (put > 0) done += (uint32_t)put;
        if (!check_gate(io)) { *actual = done; return 1; }
        if (put < 0) { if (saved_errno == EINTR) continue; *actual = done; return 1; }
        if (!put) break;
    }
    *actual = done;
    return 0;
}
static void *allocate(allocations *a, job_io *io, size_t count, size_t width) {
    if (!check_gate(io)) return NULL;
    if (!width || count > SIZE_MAX / width || a->count >= JOB_MAX_ALLOCATIONS) return NULL;
    size_t bytes = count * width;
    if (!bytes) bytes = 1u;
    if (a->bytes > JOB_MAX_ALLOCATED_BYTES || bytes > JOB_MAX_ALLOCATED_BYTES - a->bytes) return NULL;
    void *block = calloc(1u, bytes);
    if (!block) return NULL;
    a->blocks[a->count] = block;
    a->sizes[a->count++] = bytes;
    a->bytes += bytes;
    if (a->bytes > a->peak) a->peak = a->bytes;
    /* Register the obtained block before the post-allocation gate so an
     * expired/cancelled operation still releases it unconditionally. */
    if (!check_gate(io)) return NULL;
    return block;
}
static void release_all(allocations *a) {
    while (a->released < a->count) {
        uint32_t i = a->released++;
        free(a->blocks[i]); a->blocks[i] = NULL; a->bytes -= a->sizes[i];
    }
}

static uint8_t lower_ascii(uint8_t c) {
    return c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a' - 'A')) : c;
}
static int ordinary_component(const uint8_t *p, uint32_t n) {
    if (!n || n > 255u || p[0] == ' ' || p[n - 1u] == ' ' ||
        (n == 1u && p[0] == '.') || (n == 2u && p[0] == '.' && p[1] == '.')) return 0;
    for (uint32_t i = 0u; i < n; ++i) {
        uint8_t c = p[i];
        if (c < 32u || c >= 127u || c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') return 0;
    }
    return 1;
}
static int relative_path(const uint8_t *p, uint32_t n, int directory) {
    if (!n || n > MB_PAK_STREAM_MAX_PATH_BYTES || p[0] == '/' ||
        (directory ? p[n - 1u] != '/' : p[n - 1u] == '/')) return 0;
    uint32_t start = 0u;
    for (uint32_t i = 0u; i < n; ++i) {
        if (p[i] != '/' && i != n - 1u) continue;
        uint32_t end = p[i] == '/' ? i : i + 1u;
        if (!ordinary_component(p + start, end - start)) return 0;
        start = i + 1u;
    }
    return 1;
}
/* A mount is leading ../ components followed by zero or more ordinary
 * relative directory components. Absolute/root mounts are not admitted. */
static int mount_parts(const uint8_t *p, uint32_t n, uint32_t *depth, uint32_t *ordinary) {
    if (!p || !n || n > MB_PAK_STREAM_MAX_PATH_BYTES || p[n - 1u] != '/') return 0;
    uint32_t at = 0u, parents = 0u;
    while (n - at >= 3u && p[at] == '.' && p[at + 1u] == '.' && p[at + 2u] == '/') {
        at += 3u; ++parents;
    }
    if (at < n && !relative_path(p + at, n - at, 1)) return 0;
    *depth = parents; *ordinary = at;
    return 1;
}
static int mount_rebase(const uint8_t *base, uint32_t base_n,
                        const uint8_t *mod, uint32_t mod_n,
                        const uint8_t **suffix, uint32_t *suffix_n) {
    uint32_t bd, bo, md, mo;
    if (!mount_parts(base, base_n, &bd, &bo) || !mount_parts(mod, mod_n, &md, &mo) || bd != md) return 0;
    uint32_t ordinary_base_n = base_n - bo;
    if (ordinary_base_n > mod_n - mo || memcmp(base + bo, mod + mo, ordinary_base_n)) return 0;
    *suffix = mod + mo + ordinary_base_n;
    *suffix_n = mod_n - mo - ordinary_base_n;
    return 1;
}

/* The existing candidate reconciler deliberately caps None/Zlib candidates
 * at 4 MiB. This separate caller gate admits only streamed None, with an
 * explicit <=512 MiB size cap; the existing candidate/Zlib API is unchanged. */
static pak_candidate_reconcile_status reconcile_none(
    const pak_entry_result_v1 *e, const pak_physical_result_v1 *p,
    uint64_t archive_bytes, uint64_t max_payload_bytes) {
    if (!e || !p || !archive_bytes || !max_payload_bytes ||
        max_payload_bytes > MB_PAK_STREAM_MAX_PAYLOAD_BYTES ||
        e->struct_size != sizeof(*e) || e->abi_version != 1u ||
        p->struct_size != sizeof(*p) || p->abi_version != 1u ||
        e->status != PAK_ENTRY_STATUS_OK || e->record_complete != 1u ||
        p->status != PAK_PHYSICAL_STATUS_OK || p->record_complete != 1u) return PAK_RECONCILE_BAD_ARGUMENT;
    if (e->method_index != 0u || p->method_index != 0u || e->method_name_bytes != 4u ||
        p->method_name_bytes != 4u || memcmp(e->method_name, "None", 4u) ||
        memcmp(p->method_name, "None", 4u) || e->block_count || p->block_count ||
        p->flags || p->physical_block_size) return PAK_RECONCILE_UNSUPPORTED;
    if (e->header_bytes != 53u || p->header_bytes != 53u || p->bytes_consumed != 53u ||
        e->compressed_size != e->uncompressed_size || e->compressed_size != p->compressed_size ||
        e->uncompressed_size != p->uncompressed_size ||
        (p->stored_offset != 0u && p->stored_offset != e->archive_offset)) return PAK_RECONCILE_HEADER_CONFLICT;
    if (e->compressed_size > max_payload_bytes || e->uncompressed_size > max_payload_bytes)
        return PAK_RECONCILE_UNSUPPORTED;
    if (e->archive_offset > archive_bytes || 53u > archive_bytes - e->archive_offset ||
        e->data_offset != e->archive_offset + 53u ||
        e->compressed_size > archive_bytes - e->data_offset) return PAK_RECONCILE_RANGE_CONFLICT;
    return PAK_RECONCILE_OK;
}

static int compare_folded(const path_ref *a, const path_ref *b, uint64_t *spent, int *ok, job_io *io) {
    uint32_t n = a->bytes < b->bytes ? a->bytes : b->bytes;
    for (uint32_t i = 0u; i < n; ++i) {
        if (*spent > JOB_MAX_PATH_COMPARE_BYTES - 2u) { *ok = 0; return 0; }
        *spent += 2u;
        if (!charge_gate_work(io, 2u)) { *ok = 0; return 0; }
        uint8_t ac = lower_ascii(a->path[i]), bc = lower_ascii(b->path[i]);
        if (ac != bc) return ac < bc ? -1 : 1;
    }
    return a->bytes < b->bytes ? -1 : a->bytes > b->bytes ? 1 : 0;
}
/* Stable bounded O(n log n) sort; no unbounded comparator-global state. */
static int sort_paths(path_ref *rows, path_ref *scratch, uint32_t count, uint64_t *spent, job_io *io) {
    int ok = 1;
    for (uint32_t width = 1u; width < count; width *= 2u) {
        for (uint32_t lo = 0u; lo < count; lo += 2u * width) {
            if (!check_gate(io)) return 0;
            uint32_t mid = lo + width < count ? lo + width : count;
            uint32_t hi = mid + width < count ? mid + width : count;
            uint32_t a = lo, b = mid, out = lo;
            while (a < mid && b < hi) {
                int c = compare_folded(&rows[a], &rows[b], spent, &ok, io);
                if (!ok) return 0;
                scratch[out++] = c <= 0 ? rows[a++] : rows[b++];
            }
            while (a < mid) { if (!charge_gate_work(io, 64u)) return 0; scratch[out++] = rows[a++]; }
            while (b < hi) { if (!charge_gate_work(io, 64u)) return 0; scratch[out++] = rows[b++]; }
        }
        memcpy(rows, scratch, (size_t)count * sizeof(*rows));
        if (!check_gate(io)) return 0;
    }
    return 1;
}
#define NEED_ALLOC(dest, count, width) do { \
    (dest) = allocate(&owned, &io, (count), (width)); \
    if (!(dest)) { error = "bounded_allocation_failed"; goto cleanup; } \
} while (0)
#define REFUSE(message) do { error = (message); goto cleanup; } while (0)
#define CHECK_GATE() do { \
    if (!check_gate(&io)) { status = MB_PAK_UNION_STOPPED; \
        error = "operation_gate_stopped"; goto cleanup; } \
} while (0)


static int spans_overlap(const void *a, size_t an, const void *b, size_t bn) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if (an > UINTPTR_MAX - x || bn > UINTPTR_MAX - y) return 1;
    return x < y + bn && y < x + an;
}
static int valid_arguments(const mb_pak_union_params_v1 *p, mb_pak_union_result_v1 *r) {
    if (!p || !r || (uintptr_t)p % _Alignof(mb_pak_union_params_v1) ||
        (uintptr_t)r % _Alignof(mb_pak_union_result_v1) ||
        spans_overlap(p, sizeof(*p), r, sizeof(*r))) return 0;
    return p->struct_size == sizeof(*p) && p->abi_version == MB_PAK_UNION_ABI_V1 &&
        p->base_fd >= 0 && p->mod_fd >= 0 && p->output_fd >= 0 &&
        p->immutable_inputs == 1u && p->unpublished_output == 1u && p->reserved == 0u &&
        p->base_bytes && p->base_bytes <= MB_PAK_STREAM_MAX_ARCHIVE_BYTES &&
        p->mod_bytes && p->mod_bytes <= MB_PAK_STREAM_MAX_ARCHIVE_BYTES;
}
static void copy_text(char *dst, size_t cap, const char *text) {
    size_t n = strlen(text);
    if (n >= cap) n = cap - 1u;
    memcpy(dst, text, n); dst[n] = 0;
}

mb_pak_union_status_v1 mb_pak_union_prepare_v1(
    const mb_pak_union_params_v1 *params, mb_pak_union_result_v1 *result) {
    return mb_pak_union_prepare_gated_v1(params, result, NULL, NULL);
}
mb_pak_union_status_v1 mb_pak_union_prepare_gated_v1(
    const mb_pak_union_params_v1 *params, mb_pak_union_result_v1 *result,
    mb_pak_prepare_gate_v1 gate, void *gate_context) {
    if ((!gate && gate_context) || !valid_arguments(params, result) ||
        (gate_context && (spans_overlap(gate_context, 1u, params, sizeof(*params)) ||
                         spans_overlap(gate_context, 1u, result, sizeof(*result)))))
        return MB_PAK_UNION_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result); result->abi_version = MB_PAK_UNION_ABI_V1;
    job_io io = {.sources = {{.fd = params->base_fd}, {.fd = params->mod_fd}},
        .output_fd = params->output_fd, .gate = gate, .gate_context = gate_context};
    allocations owned = {0};
    pak_primary_capture_result_v1 captured = {0};
    pak_directory_result_v1 parsed = {0};
    pak_graph_result_v1 graph = {0};
    pak_secondary_result_v1 secondary = {0};
    mb_pak_v3_buffers_v1 mod_buffers = {0};
    mb_pak_v3_result_v1 mod = {0};
    mb_pak_stream_workspace_v1 writer_work = {0};
    mb_pak_stream_result_v1 written = {0};
    mb_pak_stream_file_v1 *files = NULL;
    uint8_t *path_arena = NULL;
    uint32_t base_count = 0u, input_count = 0u, final_count = 0u;
    uint32_t replaced = 0u, added = 0u, path_used = 0u;
    uint32_t component_status = 0u, component_detail = 0u, failure_row = UINT32_MAX;
    uint64_t payload_bytes = 0u, path_compare_bytes = 0u, observed_output_bytes = 0u;
    const char *stage = "fd_preflight", *error = "none";
    int success = 0, snapshot_stable = 0, output_size_valid = 0;
    mb_pak_union_status_v1 status = MB_PAK_UNION_FD_REFUSED;
    CHECK_GATE();
    uint64_t declared[2] = {params->base_bytes, params->mod_bytes};
    for (unsigned i = 0u; i < 2u; ++i) {
        CHECK_GATE();
        source_file *source = &io.sources[i]; source->job = &io;
        int flags = fcntl(source->fd, F_GETFL);
        if (flags < 0 || (flags & O_ACCMODE) != O_RDONLY ||
            fstat(source->fd, &source->before) || !S_ISREG(source->before.st_mode) ||
            source->before.st_size <= 0 || (uint64_t)source->before.st_size != declared[i])
            REFUSE("readonly_source_identity_or_size_refused");
        source->bytes = declared[i];
        CHECK_GATE();
    }
    if (same_inode(&io.sources[0].before, &io.sources[1].before)) REFUSE("duplicate_source_inode_refused");
    int output_flags = fcntl(io.output_fd, F_GETFL);
    if (output_flags < 0 || ((output_flags & O_ACCMODE) != O_WRONLY && (output_flags & O_ACCMODE) != O_RDWR) ||
        (output_flags & O_APPEND) || fstat(io.output_fd, &io.output_created) ||
        !S_ISREG(io.output_created.st_mode) || io.output_created.st_size != 0 ||
        same_inode(&io.output_created, &io.sources[0].before) || same_inode(&io.output_created, &io.sources[1].before))
        REFUSE("empty_writable_distinct_output_refused");
    status = MB_PAK_UNION_PREPARATION_REFUSED;
    CHECK_GATE();
    pak_source_v1 base_source = {sizeof(base_source), 1u, io.sources[0].bytes, 1u,
        PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE, 0u, &io.sources[0], source_read};
    pak_source_v1 mod_source = {sizeof(mod_source), 1u, io.sources[1].bytes, 2u,
        PAK_SNAPSHOT_SCOPE_CALLER_ASSERTS_IMMUTABLE, 0u, &io.sources[1], source_read};
    stage = "base_primary";
    uint8_t *reader_scratch, *primary, *secondary_scratch;
    NEED_ALLOC(reader_scratch, PAK_READER_WORKSPACE_BYTES, 1u);
    NEED_ALLOC(primary, PAK_READER_MAX_PRIMARY_BYTES, 1u);
    NEED_ALLOC(secondary_scratch, PAK_SECONDARY_WORKSPACE_BYTES, 1u);
    pak_limits_v1 reader_limits;
    pak_limits_v1_default(&reader_limits);
    reader_limits.max_declared_files = MB_PAK_STREAM_MAX_FILES;
    CHECK_GATE();
    component_status = (uint32_t)pak_capture_v11_primary(&base_source, &reader_limits,
        reader_scratch, PAK_READER_WORKSPACE_BYTES, primary, PAK_READER_MAX_PRIMARY_BYTES, &captured);
    component_detail = (uint32_t)captured.reason;
    CHECK_GATE();
    if (component_status != PAK_PRIMARY_CAPTURE_OK || !captured.primary_capture_complete ||
        !captured.encoded_span_located) REFUSE("base_primary_refused");
    const pak_primary_result_v1 *meta = &captured.primary;
    if (meta->mount_encoding != PAK_MOUNT_ENCODING_ANSI_BYTES || !meta->mount_bytes ||
        !meta->declared_file_count || meta->declared_file_count > MB_PAK_STREAM_MAX_FILES ||
        captured.encoded_span_offset > captured.primary_capture_bytes ||
        captured.encoded_span_bytes > captured.primary_capture_bytes - captured.encoded_span_offset)
        REFUSE("base_primary_profile_refused");
    base_count = meta->declared_file_count;
    stage = "base_secondary";
    pak_secondary_limits_v1 secondary_limits;
    pak_secondary_limits_v1_default(&secondary_limits);
    pak_read_budget_v1 secondary_budget = {sizeof(secondary_budget), 1u,
        PAK_READER_MAX_TOTAL_READ_BYTES, 2048u, 0u};
    const pak_secondary_index *spans[2] = {&meta->path_hash_index, &meta->directory_index};
    for (unsigned i = 0u; i < 2u; ++i) {
        CHECK_GATE();
        component_status = (uint32_t)pak_verify_v11_secondary_raw_sha1(&base_source, spans[i],
            &secondary_limits, &secondary_budget, secondary_scratch, PAK_SECONDARY_WORKSPACE_BYTES, &secondary);
        component_detail = (uint32_t)secondary.reason;
        CHECK_GATE();
        if (component_status != PAK_STATUS_OK || !secondary.digest_valid) REFUSE("base_secondary_sha1_refused");
    }
    if (!meta->directory_index.size || meta->directory_index.size > PAK_DIRECTORY_MAX_INDEX_BYTES)
        REFUSE("base_directory_size_refused");
    stage = "base_directory";
    uint8_t *directory_bytes, *names;
    pak_directory_record_v1 *directories;
    pak_directory_entry_v1 *entries;
    NEED_ALLOC(directory_bytes, (size_t)meta->directory_index.size, 1u);
    NEED_ALLOC(directories, MB_PAK_STREAM_MAX_DIRECTORIES, sizeof(*directories));
    NEED_ALLOC(entries, base_count, sizeof(*entries));
    NEED_ALLOC(names, PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES, 1u);
    if (!read_all(&io.sources[0], meta->directory_index.offset, directory_bytes,
                  (size_t)meta->directory_index.size)) REFUSE("base_directory_read_failed");
    pak_directory_limits_v1 directory_limits;
    pak_directory_limits_v1_default(&directory_limits);
    directory_limits.max_directories = MB_PAK_STREAM_MAX_DIRECTORIES;
    directory_limits.max_entries = MB_PAK_STREAM_MAX_FILES;
    CHECK_GATE();
    component_status = (uint32_t)pak_directory_read_classic_v11(directory_bytes,
        (size_t)meta->directory_index.size, captured.encoded_span_bytes, &directory_limits,
        directories, MB_PAK_STREAM_MAX_DIRECTORIES, entries, base_count,
        names, PAK_DIRECTORY_MAX_TOTAL_NAME_BYTES, &parsed);
    component_detail = (uint32_t)parsed.reason;
    CHECK_GATE();
    if (component_status != PAK_DIRECTORY_STATUS_OK || !parsed.metadata_complete ||
        parsed.entry_count != base_count || parsed.tombstone_count) REFUSE("base_directory_refused");

    stage = "base_reference_graph";
    pak_entry_limits_v1 entry_limits = {sizeof(entry_limits), 1u,
        PAK_ENTRY_DECODER_MAX_BLOB_BYTES, 128u, 0u, UINT32_MAX,
        MB_PAK_STREAM_MAX_PAYLOAD_BYTES, MB_PAK_STREAM_MAX_PAYLOAD_BYTES, MB_PAK_STREAM_MAX_PAYLOAD_BYTES};
    pak_graph_limits_v1 graph_limits;
    pak_graph_limits_v1_default(&graph_limits);
    graph_limits.max_records = MB_PAK_STREAM_MAX_FILES;
    graph_limits.max_directories = MB_PAK_STREAM_MAX_DIRECTORIES;
    pak_graph_workspace_v1 graph_work = {.struct_size = sizeof(graph_work), .abi_version = 1u};
    graph_work.starts_capacity = graph_work.extents_capacity = graph_work.sort_capacity = base_count;
    graph_work.seen_bytes = (base_count + 7u) / 8u;
    NEED_ALLOC(graph_work.starts, base_count, sizeof(*graph_work.starts));
    NEED_ALLOC(graph_work.extents, base_count, sizeof(*graph_work.extents));
    NEED_ALLOC(graph_work.sort_indices, base_count, sizeof(*graph_work.sort_indices));
    NEED_ALLOC(graph_work.seen, graph_work.seen_bytes, 1u);
    pak_graph_input_v1 graph_input = {sizeof(graph_input), 1u, base_count,
        parsed.directory_count, parsed.entry_count, parsed.string_bytes, io.sources[0].bytes,
        primary + captured.encoded_span_offset, captured.encoded_span_bytes,
        &captured.method_table, &entry_limits, directories, entries, names, 0u, 0u, {0u, 0u}};
    CHECK_GATE();
    component_status = (uint32_t)pak_validate_v11_reference_graph(&graph_input, &graph_limits,
        &graph_work, gate ? gate_stopped : NULL, gate ? &io : NULL, &graph);
    component_detail = (uint32_t)graph.reason;
    CHECK_GATE();
    if (component_status != PAK_GRAPH_OK || !graph.metadata_complete || graph.record_count != base_count)
        REFUSE("base_reference_graph_refused");

    stage = "base_physical_extents";
    /* All indexed physical extents must precede the first metadata span.
     * The graph's sorted extents permit one adjacent overlap pass. */
    if (io.sources[0].bytes < PAK_V11_FOOTER_BYTES) REFUSE("base_footer_bounds_refused");
    uint64_t first_metadata = io.sources[0].bytes - PAK_V11_FOOTER_BYTES;
    if (meta->primary_index_offset < first_metadata) first_metadata = meta->primary_index_offset;
    if (meta->path_hash_index.offset < first_metadata) first_metadata = meta->path_hash_index.offset;
    if (meta->directory_index.offset < first_metadata) first_metadata = meta->directory_index.offset;
    uint64_t prior_end = 0u;
    for (uint32_t i = 0u; i < base_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        uint32_t at = graph_work.sort_indices[i];
        if (at >= base_count) REFUSE("base_extent_order_refused");
        const pak_graph_extent_v1 *e = &graph_work.extents[at];
        failure_row = e->file_index;
        if (e->archive_end <= e->archive_start || e->archive_end > first_metadata)
            REFUSE("base_entry_extent_not_before_metadata");
        if (i && prior_end > e->archive_start) REFUSE("base_entry_extents_overlap");
        prior_end = e->archive_end;
    }
    NEED_ALLOC(files, INPUT_ROWS, sizeof(*files));
    NEED_ALLOC(io.rows, INPUT_ROWS, sizeof(*io.rows));
    /* Input paths can use two complete 4 MiB inventories. Writer separately
     * bounds the resulting selected name set to 4 MiB before any copy. */
    NEED_ALLOC(path_arena, 2u * MB_PAK_STREAM_MAX_NAME_BYTES, 1u);
    pak_physical_limits_v1 physical_limits = {sizeof(physical_limits), 1u,
        53u, 0u, UINT32_MAX, 0u, MB_PAK_STREAM_MAX_PAYLOAD_BYTES, MB_PAK_STREAM_MAX_PAYLOAD_BYTES};
    stage = "base_entries";
    for (uint32_t i = 0u; i < base_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        const pak_graph_extent_v1 *extent = &graph_work.extents[i];
        if (extent->file_index >= base_count || extent->directory_index >= parsed.directory_count)
            REFUSE("base_graph_binding_refused");
        uint32_t row = extent->file_index;
        failure_row = row;
        const pak_directory_record_v1 *dir = &directories[extent->directory_index];
        const pak_directory_entry_v1 *file = &entries[row];
        const uint8_t *prefix = names + dir->name_offset, *filename = names + file->name_offset;
        uint32_t prefix_bytes = dir->name_length;
        if (prefix_bytes == 1u && prefix[0] == '/') prefix_bytes = 0u;
        if ((prefix_bytes && !relative_path(prefix, prefix_bytes, 1)) ||
            !ordinary_component(filename, file->name_length)) REFUSE("base_path_profile_refused");
        uint32_t n = prefix_bytes + file->name_length;
        if (!n || n > MB_PAK_STREAM_MAX_PATH_BYTES || path_used > 2u * MB_PAK_STREAM_MAX_NAME_BYTES ||
            n + 1u > 2u * MB_PAK_STREAM_MAX_NAME_BYTES - path_used) REFUSE("base_path_bounds_refused");
        uint8_t *path = path_arena + path_used;
        memcpy(path, prefix, prefix_bytes); memcpy(path + prefix_bytes, filename, file->name_length);
        path[n] = 0u; path_used += n + 1u;
        pak_entry_result_v1 decoded;
        component_status = (uint32_t)pak_entry_decode_v11(primary + captured.encoded_span_offset,
            captured.encoded_span_bytes, extent->encoded_offset, io.sources[0].bytes,
            &captured.method_table, &entry_limits, NULL, 0u, &decoded);
        component_detail = (uint32_t)decoded.reason;
        if (component_status != PAK_ENTRY_STATUS_OK || !decoded.record_complete ||
            decoded.bytes_consumed != extent->encoded_bytes || decoded.archive_offset != extent->archive_start ||
            decoded.data_offset + decoded.compressed_size != extent->archive_end) REFUSE("base_entry_binding_refused");
        uint8_t physical_bytes[53]; uint32_t actual = 0u;
        if (source_read(&io.sources[0], decoded.archive_offset, physical_bytes, sizeof(physical_bytes), &actual) != PAK_PROVIDER_OK ||
            actual != sizeof(physical_bytes)) REFUSE("base_physical_header_read_failed");
        pak_physical_result_v1 physical;
        component_status = (uint32_t)pak_physical_header_parse_v11(physical_bytes, sizeof(physical_bytes),
            &captured.method_table, &physical_limits, NULL, 0u, &physical);
        component_detail = (uint32_t)physical.reason;
        if (component_status != PAK_PHYSICAL_STATUS_OK) REFUSE("base_physical_header_refused");
        component_status = (uint32_t)reconcile_none(&decoded, &physical,
            io.sources[0].bytes, MB_PAK_STREAM_MAX_PAYLOAD_BYTES);
        component_detail = 0u;
        if (component_status != PAK_RECONCILE_OK) REFUSE("base_none_reconciliation_refused");
        files[row] = (mb_pak_stream_file_v1){.struct_size = sizeof(*files), .abi_version = 1u,
            .path = path, .path_bytes = n, .verify_sha1 = 1u, .payload_bytes = decoded.uncompressed_size};
        memcpy(files[row].expected_sha1, physical.sha1, 20u);
        io.rows[row] = (payload_source){decoded.data_offset, decoded.uncompressed_size, 0u};
    }
    failure_row = UINT32_MAX;
    stage = "mod_metadata";
    mod_buffers.struct_size = sizeof(mod_buffers); mod_buffers.abi_version = 1u;
    mod_buffers.index_capacity = MB_PAK_V3_MAX_INDEX_BYTES;
    mod_buffers.entries_capacity = MB_PAK_V3_MAX_FILES;
    mod_buffers.names_capacity = MB_PAK_V3_MAX_NAME_BYTES;
    mod_buffers.sort_capacity = MB_PAK_V3_MAX_FILES;
    mod_buffers.transfer_bytes = MB_PAK_V3_TRANSFER_BYTES;
    NEED_ALLOC(mod_buffers.index, mod_buffers.index_capacity, 1u);
    NEED_ALLOC(mod_buffers.entries, mod_buffers.entries_capacity, sizeof(*mod_buffers.entries));
    NEED_ALLOC(mod_buffers.names, mod_buffers.names_capacity, 1u);
    NEED_ALLOC(mod_buffers.sort_indices, mod_buffers.sort_capacity, sizeof(*mod_buffers.sort_indices));
    NEED_ALLOC(mod_buffers.transfer, mod_buffers.transfer_bytes, 1u);
    mb_pak_v3_limits_v1 mod_limits;
    mb_pak_v3_limits_v1_default(&mod_limits);
    CHECK_GATE();
    component_status = (uint32_t)mb_pak_v3_read_none_v1(&mod_source, &mod_limits, 0u, 0u, &mod_buffers, &mod);
    component_detail = (uint32_t)mod.stage;
    CHECK_GATE();
    if (component_status != MB_PAK_V3_OK || !mod.metadata_complete || mod.selected_complete ||
        !mod.file_count || mod.file_count > MB_PAK_STREAM_MAX_FILES) REFUSE("mod_v3_metadata_refused");

    stage = "mount_rebase";
    const uint8_t *mod_prefix = NULL;
    uint32_t mod_prefix_bytes = 0u;
    CHECK_GATE();
    if (!mount_rebase(meta->mount_raw, meta->mount_bytes, mod.mount, mod.mount_bytes,
                      &mod_prefix, &mod_prefix_bytes)) REFUSE("mount_depth_or_subtree_mismatch");
    CHECK_GATE();
    stage = "mod_rebased_paths";
    for (uint32_t i = 0u; i < mod.file_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        failure_row = i;
        const mb_pak_v3_entry_v1 *e = &mod_buffers.entries[i];
        if (e->path_offset > mod.name_bytes || e->path_bytes >= mod.name_bytes - e->path_offset ||
            mod_buffers.names[e->path_offset + e->path_bytes] != 0u ||
            !relative_path(mod_buffers.names + e->path_offset, e->path_bytes, 0)) REFUSE("mod_path_profile_refused");
        uint32_t n = mod_prefix_bytes + e->path_bytes;
        if (!n || n > MB_PAK_STREAM_MAX_PATH_BYTES || path_used > 2u * MB_PAK_STREAM_MAX_NAME_BYTES ||
            n + 1u > 2u * MB_PAK_STREAM_MAX_NAME_BYTES - path_used) REFUSE("mod_rebased_path_bounds_refused");
        uint8_t *path = path_arena + path_used;
        memcpy(path, mod_prefix, mod_prefix_bytes);
        memcpy(path + mod_prefix_bytes, mod_buffers.names + e->path_offset, e->path_bytes);
        path[n] = 0u; path_used += n + 1u;
        uint32_t row = base_count + i;
        files[row] = (mb_pak_stream_file_v1){.struct_size = sizeof(*files), .abi_version = 1u,
            .path = path, .path_bytes = n, .verify_sha1 = 1u, .payload_bytes = e->payload_bytes};
        memcpy(files[row].expected_sha1, e->sha1, 20u);
        io.rows[row] = (payload_source){e->data_offset, e->payload_bytes, 1u};
    }
    failure_row = UINT32_MAX;


    stage = "path_union";
    input_count = base_count + mod.file_count;
    path_ref *paths, *sort_scratch;
    uint32_t *mod_to_base;
    NEED_ALLOC(paths, input_count, sizeof(*paths));
    NEED_ALLOC(sort_scratch, input_count, sizeof(*sort_scratch));
    NEED_ALLOC(mod_to_base, mod.file_count, sizeof(*mod_to_base));
    for (uint32_t i = 0u; i < input_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        if (!files[i].path || !files[i].path_bytes) REFUSE("input_path_binding_refused");
        paths[i] = (path_ref){files[i].path, files[i].path_bytes,
            i < base_count ? 0u : 1u, i < base_count ? i : i - base_count};
    }
    for (uint32_t i = 0u; i < mod.file_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        mod_to_base[i] = UINT32_MAX;
    }
    if (!sort_paths(paths, sort_scratch, input_count, &path_compare_bytes, &io)) REFUSE("path_comparison_budget_refused");
    for (uint32_t i = 1u; i < input_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        int ok = 1;
        const path_ref *a = &paths[i - 1u], *b = &paths[i];
        int c = compare_folded(a, b, &path_compare_bytes, &ok, &io);
        if (!ok) REFUSE("path_comparison_budget_refused");
        if (c) continue;
        if (a->bytes != b->bytes || memcmp(a->path, b->path, a->bytes)) REFUSE("case_only_file_alias_refused");
        if (a->source_ordinal == b->source_ordinal) REFUSE("duplicate_input_path_refused");
        const path_ref *base = a->source_ordinal == 0u ? a : b;
        const path_ref *incoming = a->source_ordinal == 1u ? a : b;
        if (base->source_row >= base_count || incoming->source_row >= mod.file_count ||
            mod_to_base[incoming->source_row] != UINT32_MAX) REFUSE("replacement_binding_refused");
        mod_to_base[incoming->source_row] = base->source_row;
    }
    final_count = base_count;
    for (uint32_t i = 0u; i < mod.file_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        uint32_t source_row = base_count + i, target = mod_to_base[i];
        if (target == UINT32_MAX) {
            if (final_count >= MB_PAK_STREAM_MAX_FILES) REFUSE("union_file_count_refused");
            target = final_count++; ++added;
        } else ++replaced;
        /* Targets never exceed the current incoming source row: compaction
         * cannot overwrite descriptors for a future mod row. */
        if (target > source_row) REFUSE("union_compaction_binding_refused");
        files[target] = files[source_row]; io.rows[target] = io.rows[source_row];
    }
    for (uint32_t i = 0u; i < final_count; ++i) {
        if ((i & 63u) == 0u) CHECK_GATE();
        if (files[i].payload_bytes > MB_PAK_STREAM_MAX_PAYLOAD_BYTES - payload_bytes)
            REFUSE("union_payload_cap_refused");
        payload_bytes += files[i].payload_bytes;
    }
    io.row_count = final_count;
    stage = "allocate_writer";
    writer_work.records_capacity = final_count;
    writer_work.order_capacity = final_count;
    writer_work.directories_capacity = MB_PAK_STREAM_MAX_DIRECTORIES;
    writer_work.index_capacity = MB_PAK_STREAM_MAX_INDEX_BYTES;
    writer_work.transfer_bytes = MB_PAK_STREAM_TRANSFER_BYTES;
    NEED_ALLOC(writer_work.records, writer_work.records_capacity, sizeof(*writer_work.records));
    NEED_ALLOC(writer_work.order, writer_work.order_capacity, sizeof(*writer_work.order));
    NEED_ALLOC(writer_work.directories, writer_work.directories_capacity, sizeof(*writer_work.directories));
    NEED_ALLOC(writer_work.index_arena, writer_work.index_capacity, 1u);
    NEED_ALLOC(writer_work.transfer, writer_work.transfer_bytes, 1u);

    mb_pak_stream_profile_v1 profile = {sizeof(profile), 1u, meta->mount_raw,
        meta->mount_bytes, 0u, meta->path_hash_seed};
    mb_pak_stream_limits_v1 write_limits;
    mb_pak_stream_limits_v1_default(&write_limits);
    mb_pak_stream_io_v1 writer_io = {sizeof(writer_io), 1u, &io, payload_read, output_write, 1u, 1u};
    stage = "stream_v11_union";
    /* Writer preflights the entire final set, including case-only DIRECTORY
     * aliases, file ancestors, path hashes and all geometry before first I/O. */
    CHECK_GATE();
    component_status = (uint32_t)mb_pak_stream_write_v11_none_gated_v1(&profile, files, final_count,
        &write_limits, &writer_io, &writer_work, &written,
        gate ? gate_stopped : NULL, gate ? &io : NULL);
    component_detail = 0u;
    CHECK_GATE();
    if (component_status != MB_PAK_STREAM_OK || !written.archive_complete) REFUSE("v11_union_stream_refused");


    stage = "finalize_preparation";
    struct stat output_now;
    snapshot_stable = all_sources_stable(&io);
    if (!snapshot_stable) { status = MB_PAK_UNION_SOURCE_CHANGED; REFUSE("source_snapshot_changed"); }
    if (fstat(io.output_fd, &output_now) || !S_ISREG(output_now.st_mode) ||
        !same_inode(&output_now, &io.output_created) || output_now.st_size < 0 ||
        (uint64_t)output_now.st_size != written.archive_bytes) {
        status = MB_PAK_UNION_OUTPUT_SIZE_MISMATCH; REFUSE("completed_output_identity_or_size_refused");
    }
    observed_output_bytes = (uint64_t)output_now.st_size;
    output_size_valid = 1; success = 1; status = MB_PAK_UNION_OK; stage = "complete";

cleanup:
    if (io.output_fd >= 0) {
        struct stat now;
        if (!fstat(io.output_fd, &now) && now.st_size >= 0) observed_output_bytes = (uint64_t)now.st_size;
    }
    snapshot_stable = io.sources[0].bytes && io.sources[1].bytes && all_sources_stable(&io);
    if (!snapshot_stable && success) {
        success = 0; status = MB_PAK_UNION_SOURCE_CHANGED;
        if (!strcmp(error, "none")) { error = "source_changed_at_final_cleanup_check"; stage = "final_source_snapshot"; }
    } else if ((io.sources[0].changed || io.sources[1].changed) && status != MB_PAK_UNION_FD_REFUSED) {
        status = MB_PAK_UNION_SOURCE_CHANGED;
    }
    /* Never suppress owned cleanup because of a terminal predicate. Publish
     * completion only after free/snapshot work and the final shared sample. */
    release_all(&owned);
    if (gate_stopped(&io)) {
        if (success) stage = "final_cleanup";
        success = 0; output_size_valid = 0; status = MB_PAK_UNION_STOPPED;
        error = "operation_gate_stopped";
        written.archive_complete = 0u;
        written.archive_bytes = 0u; written.file_count = 0u; written.directory_count = 0u;
        memset(written.primary_sha1, 0, sizeof(written.primary_sha1));
        memset(written.path_index_sha1, 0, sizeof(written.path_index_sha1));
        memset(written.directory_sha1, 0, sizeof(written.directory_sha1));
    }
    result->status = status;
    copy_text(result->stage, sizeof(result->stage), stage);
    copy_text(result->error, sizeof(result->error), error);
    result->component_status = component_status; result->component_detail = component_detail; result->failure_row = failure_row;
    result->archive_complete = success ? 1u : 0u; result->source_snapshot_stable = snapshot_stable ? 1u : 0u;
    result->output_size_valid = output_size_valid ? 1u : 0u;
    result->base_source_bytes = io.sources[0].bytes; result->mod_source_bytes = io.sources[1].bytes;
    result->path_hash_seed = captured.primary.path_hash_seed;
    result->mount_bytes = captured.primary.mount_bytes; result->mod_mount_bytes = mod.mount_bytes;
    memcpy(result->mount, captured.primary.mount_raw, result->mount_bytes);
    memcpy(result->mod_mount, mod.mount, result->mod_mount_bytes);
    result->base_file_count = base_count; result->mod_file_count = mod.file_count;
    result->input_file_count = input_count; result->final_file_count = final_count; result->replaced = replaced; result->added = added;
    result->base_metadata_complete = captured.primary_capture_complete; result->base_reference_graph_complete = graph.metadata_complete;
    result->mod_metadata_complete = mod.metadata_complete;
    result->chosen_payloads_sha1_checked = written.archive_complete ? final_count : 0u;
    result->chosen_base_payloads_sha1_checked = written.archive_complete ? base_count - replaced : 0u;
    result->chosen_mod_payloads_sha1_checked = written.archive_complete ? mod.file_count : 0u;
    result->payload_bytes = payload_bytes; result->archive_bytes = written.archive_bytes; result->observed_output_bytes = observed_output_bytes;
    result->base_graph_steps = graph.steps_used; result->base_graph_comparison_bytes = graph.name_comparison_bytes;
    result->path_comparison_bytes = path_compare_bytes; result->requested_job_io_bytes = io.requested_bytes; result->job_io_calls = io.calls;
    result->writer = written;
    result->allocation_peak_bytes = owned.peak; result->allocation_calls = owned.count; result->release_calls = owned.released;
    result->allocation_live_after_cleanup = owned.bytes;
    return status;
}
