#include "pak_writer.h"
#include <mbedtls/sha1.h>
#include <string.h>

/* Classic geometry follows pinned repak355b5f62 entry.rs/pak.rs/footer.rs.
 * ASCII is deliberate: no guessed Unicode Unreal lowercase equivalence. */
typedef struct span { uintptr_t begin, end; } span;
typedef struct work {
    const mb_pak_writer_file_v1 *files;
    const mb_pak_writer_limits_v1 *limits;
    mb_pak_writer_workspace_v1 *scratch;
    uint64_t used;
    uint32_t count, directories;
    mb_pak_writer_status status;
} work;
typedef struct cursor {
    uint8_t *bytes;
    size_t position, bound;
    int failed;
} cursor;

static void erase(void *pointer, size_t bytes) {
    volatile uint8_t *p = pointer;
    while (bytes--) *p++ = 0;
}

static int range(const void *p, size_t n, size_t alignment, span *s) {
    const uintptr_t a = (uintptr_t)p;
    if ((n && !p) || a % alignment || n > UINTPTR_MAX - a) return 0;
    s->begin = a; s->end = a + n;
    return 1;
}

static int overlaps(span a, span b) {
    return a.begin != a.end && b.begin != b.end && a.begin < b.end && b.begin < a.end;
}

static int disjoint_input(const void *p, size_t n, size_t alignment, const span mutable[3]) {
    span s;
    if (!range(p, n, alignment, &s)) return 0;
    for (unsigned i = 0; i < 3; ++i) if (overlaps(s, mutable[i])) return 0;
    return 1;
}

static int preflight(const mb_pak_writer_profile_v1 *p, const mb_pak_writer_file_v1 *f,
    uint32_t count, const mb_pak_writer_limits_v1 *l, mb_pak_writer_workspace_v1 *w,
    uint8_t *output, size_t capacity, mb_pak_writer_result_v1 *r) {
    span mutable[3];
    if (!p || !l || !w || !r || count > MB_PAK_WRITER_MAX_FILES ||
        capacity > MB_PAK_WRITER_MAX_ARCHIVE_BYTES ||
        !range(w, sizeof(*w), _Alignof(mb_pak_writer_workspace_v1), &mutable[0]) ||
        !range(r, sizeof(*r), _Alignof(mb_pak_writer_result_v1), &mutable[1]) ||
        !range(output, capacity, 1, &mutable[2])) return 0;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < i; ++j) if (overlaps(mutable[i], mutable[j])) return 0;
    if (!disjoint_input(p, sizeof(*p), _Alignof(mb_pak_writer_profile_v1), mutable) ||
        !disjoint_input(l, sizeof(*l), _Alignof(mb_pak_writer_limits_v1), mutable) ||
        !disjoint_input(f, (size_t)count * sizeof(*f), _Alignof(mb_pak_writer_file_v1), mutable))
        return 0;
    if (p->struct_size != sizeof(*p) || p->abi_version != 1 || p->reserved ||
        l->struct_size != sizeof(*l) || l->abi_version != 1 ||
        l->max_files > MB_PAK_WRITER_MAX_FILES ||
        l->max_directories > MB_PAK_WRITER_MAX_DIRECTORIES ||
        l->max_path_bytes > MB_PAK_WRITER_MAX_PATH_BYTES ||
        l->max_name_bytes > MB_PAK_WRITER_MAX_NAME_BYTES ||
        l->max_payload_bytes > MB_PAK_WRITER_MAX_PAYLOAD_BYTES ||
        l->max_archive_bytes > MB_PAK_WRITER_MAX_ARCHIVE_BYTES ||
        l->max_work > MB_PAK_WRITER_MAX_WORK ||
        !disjoint_input(p->mount, p->mount_bytes, 1, mutable)) return 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (f[i].struct_size != sizeof(f[i]) || f[i].abi_version != 1 || f[i].reserved ||
            f[i].payload_bytes > SIZE_MAX ||
            !disjoint_input(f[i].path, f[i].path_bytes, 1, mutable) ||
            !disjoint_input(f[i].payload, (size_t)f[i].payload_bytes, 1, mutable)) return 0;
    }
    return 1;
}

void mb_pak_writer_limits_v1_default(mb_pak_writer_limits_v1 *l) {
    if (!l) return;
    memset(l, 0, sizeof(*l)); l->struct_size = sizeof(*l); l->abi_version = 1;
    l->max_files = MB_PAK_WRITER_MAX_FILES; l->max_directories = MB_PAK_WRITER_MAX_DIRECTORIES;
    l->max_path_bytes = MB_PAK_WRITER_MAX_PATH_BYTES; l->max_name_bytes = MB_PAK_WRITER_MAX_NAME_BYTES;
    l->max_payload_bytes = MB_PAK_WRITER_MAX_PAYLOAD_BYTES;
    l->max_archive_bytes = MB_PAK_WRITER_MAX_ARCHIVE_BYTES; l->max_work = MB_PAK_WRITER_MAX_WORK;
}

static int charge(work *w, uint64_t n) {
    if (w->used > w->limits->max_work || n > w->limits->max_work - w->used) {
        w->status = MB_PAK_WRITE_RESOURCE_LIMIT; return 0;
    }
    w->used += n; return 1;
}

static uint8_t lower(uint8_t c) { return c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a'-'A')) : c; }

static int compare(work *w, const uint8_t *a, uint32_t an, const uint8_t *b, uint32_t bn,
                   int fold) {
    if (!charge(w, 1)) return 0;
    const uint32_t n = an < bn ? an : bn;
    for (uint32_t i = 0; i < n; ++i) {
        if (!charge(w, 1)) return 0;
        const uint8_t ac = fold ? lower(a[i]) : a[i], bc = fold ? lower(b[i]) : b[i];
        if (ac != bc) return ac < bc ? -1 : 1;
    }
    return an < bn ? -1 : an > bn ? 1 : 0;
}

static int path_order(work *w, uint32_t a, uint32_t b, int fold) {
    return compare(w, w->files[a].path, w->files[a].path_bytes,
                   w->files[b].path, w->files[b].path_bytes, fold);
}

static void path_sift(work *w, uint32_t start, uint32_t end, int fold) {
    uint32_t *o = w->scratch->order;
    while (start < end / 2) {
        uint32_t child = start * 2 + 1;
        if (child + 1 < end && path_order(w, o[child], o[child+1], fold) < 0) ++child;
        if (w->status != MB_PAK_WRITE_OK || path_order(w, o[start], o[child], fold) >= 0) break;
        if (w->status != MB_PAK_WRITE_OK) break;
        const uint32_t tmp = o[start]; o[start] = o[child]; o[child] = tmp; start = child;
    }
}

static void sort_paths(work *w, int fold) {
    for (uint32_t i = w->count / 2; i && w->status == MB_PAK_WRITE_OK; ) path_sift(w, --i, w->count, fold);
    for (uint32_t end = w->count; end > 1 && w->status == MB_PAK_WRITE_OK; ) {
        --end; const uint32_t tmp = w->scratch->order[0];
        w->scratch->order[0] = w->scratch->order[end]; w->scratch->order[end] = tmp;
        path_sift(w, 0, end, fold);
    }
}

static mb_pak_writer_status validate_path(work *w, const uint8_t *p, uint32_t n, int mount) {
    if (!n) return MB_PAK_WRITE_INVALID_PATH;
    if (n > w->limits->max_path_bytes) return MB_PAK_WRITE_RESOURCE_LIMIT;
    if (mount && n == 1 && p[0] == '/') return charge(w, 1) ? MB_PAK_WRITE_OK : w->status;
    if (p[0] == '/' || (mount && p[n-1] != '/') || (!mount && p[n-1] == '/'))
        return MB_PAK_WRITE_INVALID_PATH;
    uint32_t start = 0; int ordinary_seen = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (!charge(w, 1)) return w->status;
        const uint8_t c = p[i];
        if (c >= 128) return MB_PAK_WRITE_UNSUPPORTED_NAME;
        if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') return MB_PAK_WRITE_INVALID_PATH;
        if (c != '/' && i != n-1) continue;
        const uint32_t end = c == '/' ? i : i+1, bytes = end-start;
        if (!bytes || bytes > 255 || p[start] == ' ' || p[end-1] == ' ' ||
            (bytes == 1 && p[start] == '.')) return MB_PAK_WRITE_INVALID_PATH;
        if (bytes == 2 && p[start] == '.' && p[start+1] == '.') {
            if (!mount || ordinary_seen) return MB_PAK_WRITE_INVALID_PATH;
        } else ordinary_seen = 1;
        start = i+1;
    }
    return MB_PAK_WRITE_OK;
}

static int has_ancestor(work *w, const uint8_t *path, uint32_t bytes) {
    uint32_t lo = 0, hi = w->count;
    while (lo < hi && w->status == MB_PAK_WRITE_OK) {
        const uint32_t mid = lo + (hi-lo)/2;
        const mb_pak_writer_file_v1 *f = &w->files[w->scratch->order[mid]];
        const int c = compare(w, f->path, f->path_bytes, path, bytes, 1);
        if (c < 0) lo = mid+1; else hi = mid;
    }
    if (lo == w->count || w->status != MB_PAK_WRITE_OK) return 0;
    const mb_pak_writer_file_v1 *f = &w->files[w->scratch->order[lo]];
    return compare(w, f->path, f->path_bytes, path, bytes, 1) == 0 && w->status == MB_PAK_WRITE_OK;
}

static const uint8_t *dir_name(work *w, const mb_pak_writer_directory_scratch *d) {
    static const uint8_t root[] = "/";
    return d->input_index == UINT32_MAX ? root : w->files[d->input_index].path;
}

static int dir_compare(work *w, const mb_pak_writer_directory_scratch *a,
    const mb_pak_writer_directory_scratch *b, int fold) {
    return compare(w, dir_name(w,a), a->prefix_bytes, dir_name(w,b), b->prefix_bytes, fold);
}

static int add_directory(work *w, uint32_t input, uint32_t bytes) {
    const mb_pak_writer_directory_scratch d = {input, bytes, 0};
    mb_pak_writer_directory_scratch *dirs = w->scratch->directories;
    uint32_t lo = 0, hi = w->directories;
    while (lo < hi && w->status == MB_PAK_WRITE_OK) {
        const uint32_t mid = lo + (hi-lo)/2;
        if (dir_compare(w, &dirs[mid], &d, 1) < 0) lo = mid+1; else hi = mid;
    }
    if (w->status != MB_PAK_WRITE_OK) return 0;
    if (lo < w->directories && dir_compare(w, &dirs[lo], &d, 1) == 0) {
        if (w->status != MB_PAK_WRITE_OK) return 0;
        if (dir_compare(w, &dirs[lo], &d, 0) != 0 && w->status == MB_PAK_WRITE_OK)
            w->status = MB_PAK_WRITE_PATH_CONFLICT;
        return w->status == MB_PAK_WRITE_OK;
    }
    if (w->status != MB_PAK_WRITE_OK) return 0;
    if (w->directories == w->limits->max_directories) {
        w->status = MB_PAK_WRITE_RESOURCE_LIMIT; return 0;
    }
    if (!charge(w, 1 + (uint64_t)(w->directories-lo) * sizeof(*dirs))) return 0;
    memmove(dirs+lo+1, dirs+lo, (size_t)(w->directories-lo) * sizeof(*dirs));
    dirs[lo] = d; ++w->directories; return 1;
}

static void dir_sift(work *w, uint32_t start, uint32_t end) {
    mb_pak_writer_directory_scratch *d = w->scratch->directories;
    while (start < end / 2) {
        uint32_t child = start*2+1;
        if (child+1 < end && dir_compare(w,&d[child],&d[child+1],0) < 0) ++child;
        if (w->status != MB_PAK_WRITE_OK || dir_compare(w,&d[start],&d[child],0) >= 0) break;
        if (w->status != MB_PAK_WRITE_OK) break;
        const mb_pak_writer_directory_scratch tmp = d[start]; d[start] = d[child]; d[child] = tmp;
        start = child;
    }
}

static void sort_directories(work *w) {
    for (uint32_t i = w->directories/2; i && w->status == MB_PAK_WRITE_OK; ) dir_sift(w,--i,w->directories);
    for (uint32_t end = w->directories; end > 1 && w->status == MB_PAK_WRITE_OK; ) {
        --end; const mb_pak_writer_directory_scratch tmp = w->scratch->directories[0];
        w->scratch->directories[0] = w->scratch->directories[end]; w->scratch->directories[end] = tmp;
        dir_sift(w,0,end);
    }
}

static uint32_t find_directory(work *w, uint32_t input, uint32_t bytes) {
    const mb_pak_writer_directory_scratch target = {input,bytes,0};
    uint32_t lo = 0, hi = w->directories;
    while (lo < hi && w->status == MB_PAK_WRITE_OK) {
        const uint32_t mid = lo+(hi-lo)/2;
        if (dir_compare(w,&w->scratch->directories[mid],&target,0) < 0) lo = mid+1; else hi = mid;
    }
    if (w->status == MB_PAK_WRITE_OK && (lo == w->directories ||
        dir_compare(w,&w->scratch->directories[lo],&target,0) != 0)) w->status = MB_PAK_WRITE_INTERNAL_ERROR;
    return lo;
}

static int prepare(work *w, const mb_pak_writer_profile_v1 *p, uint64_t *payload_bytes,
                   uint64_t *directory_bytes) {
    w->status = validate_path(w,p->mount,p->mount_bytes,1);
    if (w->status != MB_PAK_WRITE_OK) return 0;
    if (w->count > w->limits->max_files) { w->status = MB_PAK_WRITE_RESOURCE_LIMIT; return 0; }
    uint64_t names = 0;
    for (uint32_t i = 0; i < w->count; ++i) {
        const mb_pak_writer_file_v1 *f = &w->files[i];
        w->status = validate_path(w,f->path,f->path_bytes,0);
        if (w->status != MB_PAK_WRITE_OK) return 0;
        names += f->path_bytes;
        if (names > w->limits->max_name_bytes || f->payload_bytes > w->limits->max_payload_bytes) {
            w->status = MB_PAK_WRITE_RESOURCE_LIMIT; return 0;
        }
        *payload_bytes += f->payload_bytes; w->scratch->order[i] = i;
    }
    sort_paths(w,1);
    for (uint32_t i = 1; i < w->count && w->status == MB_PAK_WRITE_OK; ++i)
        if (path_order(w,w->scratch->order[i-1],w->scratch->order[i],1) == 0 &&
            w->status == MB_PAK_WRITE_OK) w->status = MB_PAK_WRITE_PATH_CONFLICT;
    for (uint32_t i = 0; i < w->count && w->status == MB_PAK_WRITE_OK; ++i) {
        const mb_pak_writer_file_v1 *f = &w->files[i];
        for (uint32_t b = 0; b < f->path_bytes && w->status == MB_PAK_WRITE_OK; ++b) {
            if (!charge(w,1)) break;
            if (f->path[b] == '/' && has_ancestor(w,f->path,b)) w->status = MB_PAK_WRITE_PATH_CONFLICT;
        }
    }
    if (w->status != MB_PAK_WRITE_OK) return 0;
    if (w->count && !add_directory(w,UINT32_MAX,1)) return 0;
    for (uint32_t i = 0; i < w->count; ++i) {
        const mb_pak_writer_file_v1 *f = &w->files[i];
        for (uint32_t b = 0; b < f->path_bytes; ++b) {
            if (!charge(w,1)) return 0;
            if (f->path[b] == '/' && !add_directory(w,i,b+1)) return 0;
        }
    }
    sort_paths(w,0); sort_directories(w);
    if (w->status != MB_PAK_WRITE_OK) return 0;
    uint64_t physical = 0;
    for (uint32_t i = 0; i < w->count; ++i) {
        mb_pak_writer_record_scratch *r = &w->scratch->records[i];
        r->input_index = w->scratch->order[i];
        const mb_pak_writer_file_v1 *f = &w->files[r->input_index];
        r->physical_offset = (uint32_t)physical; physical += 53 + f->payload_bytes;
        uint64_t hash = UINT64_C(0xcbf29ce484222325) + p->path_hash_seed;
        for (uint32_t b = 0; b < f->path_bytes; ++b) {
            if (!charge(w,3)) return 0;
            if (f->path[b] == '/') r->filename_offset = b+1;
            hash = (hash ^ lower(f->path[b])) * UINT64_C(0x100000001b3);
            hash *= UINT64_C(0x100000001b3); /* UTF-16LE ASCII high byte is zero. */
        }
        r->path_hash = hash;
        for (uint32_t j = 0; j < i; ++j) {
            if (!charge(w,1)) return 0;
            if (hash == w->scratch->records[j].path_hash) { w->status = MB_PAK_WRITE_PATH_CONFLICT; return 0; }
        }
        r->directory_id = find_directory(w,r->filename_offset ? r->input_index : UINT32_MAX,
                                         r->filename_offset ? r->filename_offset : 1);
        if (w->status != MB_PAK_WRITE_OK) return 0;
        ++w->scratch->directories[r->directory_id].file_count;
    }
    *directory_bytes = 4;
    for (uint32_t d = 0; d < w->directories; ++d) {
        if (!charge(w,1)) return 0;
        *directory_bytes += 9 + w->scratch->directories[d].prefix_bytes;
    }
    for (uint32_t i = 0; i < w->count; ++i) {
        if (!charge(w,1)) return 0;
        const mb_pak_writer_record_scratch *r = &w->scratch->records[i];
        *directory_bytes += 9 + w->files[r->input_index].path_bytes - r->filename_offset;
    }
    return 1;
}

static void emit(cursor *c, const void *bytes, size_t n) {
    if (c->failed || c->position > c->bound || n > c->bound-c->position) { c->failed = 1; return; }
    if (n) memcpy(c->bytes+c->position,bytes,n);
    c->position += n;
}
static void zeros(cursor *c, size_t n) {
    if (c->failed || c->position > c->bound || n > c->bound-c->position) { c->failed = 1; return; }
    memset(c->bytes+c->position,0,n); c->position += n;
}
static void u32(cursor *c, uint32_t v) {
    uint8_t b[4]; for (unsigned i = 0; i < 4; ++i) b[i] = (uint8_t)(v >> (8*i)); emit(c,b,4);
}
static void u64(cursor *c, uint64_t v) {
    uint8_t b[8]; for (unsigned i = 0; i < 8; ++i) b[i] = (uint8_t)(v >> (8*i)); emit(c,b,8);
}
static void string(cursor *c, const uint8_t *p, uint32_t n) { u32(c,n+1); emit(c,p,n); zeros(c,1); }

static void directory_map(cursor *c, work *w) {
    u32(c,w->directories);
    for (uint32_t d = 0; d < w->directories; ++d) {
        const mb_pak_writer_directory_scratch *dir = &w->scratch->directories[d];
        string(c,dir_name(w,dir),dir->prefix_bytes); u32(c,dir->file_count);
        for (uint32_t i = 0; i < w->count; ++i) {
            const mb_pak_writer_record_scratch *r = &w->scratch->records[i];
            if (r->directory_id != d) continue;
            const mb_pak_writer_file_v1 *f = &w->files[r->input_index];
            string(c,f->path+r->filename_offset,f->path_bytes-r->filename_offset); u32(c,12*i);
        }
    }
}

mb_pak_writer_status mb_pak_write_v11_none(const mb_pak_writer_profile_v1 *p,
    const mb_pak_writer_file_v1 *f, uint32_t count, const mb_pak_writer_limits_v1 *l,
    mb_pak_writer_workspace_v1 *scratch, uint8_t *output, size_t capacity,
    mb_pak_writer_result_v1 *result) {
    if (!preflight(p,f,count,l,scratch,output,capacity,result)) return MB_PAK_WRITE_BAD_ARGUMENT;
    memset(result,0,sizeof(*result)); result->struct_size = sizeof(*result); result->abi_version = 1;
    memset(scratch,0,sizeof(*scratch));
    work w = {f,l,scratch,0,count,0,MB_PAK_WRITE_OK};
    uint64_t payload_bytes = 0, directory_bytes = 0, required = 0;
    uint8_t digests[3][20] = {{0}};
    int writing = 0;
    if (!prepare(&w,p,&payload_bytes,&directory_bytes)) goto done;
    /* Checked ceilings above imply these sums are far below UINT32_MAX/SIZE_MAX:
     * <=256*4MiB payload, <=2048*1024 directory names, <=64KiB file paths. */
    const uint64_t primary_offset = payload_bytes + (uint64_t)count*53;
    const uint64_t primary_bytes = 105 + p->mount_bytes + (uint64_t)count*12;
    const uint64_t path_offset = primary_offset + primary_bytes;
    const uint64_t path_bytes = 4 + (uint64_t)count*12 + directory_bytes;
    const uint64_t directory_offset = path_offset + path_bytes;
    const uint64_t footer_offset = directory_offset + directory_bytes;
    required = footer_offset + 221;
    if (required > l->max_archive_bytes || required > SIZE_MAX) { w.status = MB_PAK_WRITE_RESOURCE_LIMIT; goto done; }
    result->required_bytes = required;
    if (capacity < required) { w.status = MB_PAK_WRITE_BUFFER_TOO_SMALL; goto done; }
    /* Reserve all publication/copy/hash work before touching output. This
     * conservative byte weight includes payload hashes, metadata hashes,
     * serialization, and both bounded directory-row scans. */
    if (!charge(&w,required*3 + payload_bytes + (uint64_t)w.directories*count*2 + (uint64_t)count*128)) {
        result->required_bytes = 0; goto done;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const mb_pak_writer_file_v1 *file = &f[scratch->records[i].input_index];
        static const uint8_t empty = 0;
        if (mbedtls_sha1(file->payload_bytes ? file->payload : &empty,
            (size_t)file->payload_bytes,scratch->records[i].sha1) != 0) {
            w.status = MB_PAK_WRITE_SHA1_ERROR; result->required_bytes = 0; goto done;
        }
    }
    writing = 1; cursor c = {output,0,(size_t)required,0};
    for (uint32_t i = 0; i < count; ++i) {
        const mb_pak_writer_record_scratch *r = &scratch->records[i];
        const mb_pak_writer_file_v1 *file = &f[r->input_index];
        u64(&c,0); u64(&c,file->payload_bytes); u64(&c,file->payload_bytes); u32(&c,0);
        emit(&c,r->sha1,20); zeros(&c,1); u32(&c,0); emit(&c,file->payload,(size_t)file->payload_bytes);
    }
    if (c.position != primary_offset) c.failed = 1;
    string(&c,p->mount,p->mount_bytes); u32(&c,count); u64(&c,p->path_hash_seed);
    u32(&c,1); u64(&c,path_offset); u64(&c,path_bytes);
    const size_t path_digest_at = c.position; zeros(&c,20);
    u32(&c,1); u64(&c,directory_offset); u64(&c,directory_bytes);
    const size_t directory_digest_at = c.position; zeros(&c,20);
    u32(&c,count*12);
    for (uint32_t i = 0; i < count; ++i) {
        u32(&c,UINT32_C(0xe0000000)); u32(&c,scratch->records[i].physical_offset);
        u32(&c,(uint32_t)f[scratch->records[i].input_index].payload_bytes);
    }
    u32(&c,0);
    if (c.position != path_offset) c.failed = 1;
    u32(&c,count);
    for (uint32_t i = 0; i < count; ++i) { u64(&c,scratch->records[i].path_hash); u32(&c,12*i); }
    directory_map(&c,&w);
    if (c.position != directory_offset) c.failed = 1;
    directory_map(&c,&w);
    if (c.position != footer_offset) c.failed = 1;
    zeros(&c,17); u32(&c,UINT32_C(0x5a6f12e1)); u32(&c,11);
    u64(&c,primary_offset); u64(&c,primary_bytes);
    const size_t primary_digest_at = c.position; zeros(&c,20); zeros(&c,160);
    if (c.failed || c.position != required) { w.status = MB_PAK_WRITE_INTERNAL_ERROR; goto done; }
    if (mbedtls_sha1(output+(size_t)path_offset,(size_t)path_bytes,digests[1]) ||
        mbedtls_sha1(output+(size_t)directory_offset,(size_t)directory_bytes,digests[2])) {
        w.status = MB_PAK_WRITE_SHA1_ERROR; goto done;
    }
    memcpy(output+path_digest_at,digests[1],20); memcpy(output+directory_digest_at,digests[2],20);
    if (mbedtls_sha1(output+(size_t)primary_offset,(size_t)primary_bytes,digests[0])) {
        w.status = MB_PAK_WRITE_SHA1_ERROR; goto done;
    }
    memcpy(output+primary_digest_at,digests[0],20);
    result->archive_complete = 1; result->archive_bytes = required;
    result->file_count = count; result->directory_count = w.directories;
    memcpy(result->primary_sha1,digests[0],20); memcpy(result->path_index_sha1,digests[1],20);
    memcpy(result->directory_sha1,digests[2],20);
done:
    if (writing && w.status != MB_PAK_WRITE_OK) { erase(output,(size_t)required); result->required_bytes = 0; }
    result->work_used = w.used; result->status = w.status;
    erase(digests,sizeof(digests)); erase(scratch,sizeof(*scratch));
    return w.status;
}
