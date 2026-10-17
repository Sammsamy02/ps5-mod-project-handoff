#include "pak_stream_writer.h"
#include <mbedtls/sha1.h>
#include <string.h>

/* Validation/order/FNV/index geometry derived from unchanged production
 * pak_writer.c; its source hash/provenance is recorded in the host receipt. */
typedef struct span { uintptr_t begin,end; } span;
typedef struct work {
 const mb_pak_stream_file_v1 *files;
 const mb_pak_stream_limits_v1 *limits;
 mb_pak_stream_workspace_v1 *scratch;
 uint64_t used;
 uint32_t count,directories;
 mb_pak_stream_status status;
 mb_pak_prepare_gate_v1 gate;
 void *gate_context;
 uint64_t gate_work;
 uint32_t stopped;
} work;
typedef struct cursor { uint8_t *bytes;size_t position,bound;int failed; } cursor;
static void erase(void *p,size_t n) { volatile uint8_t *q=p;while(n--)*q++=0; }
static int range(const void *p,size_t n,size_t alignment,span *s) {
 uintptr_t a=(uintptr_t)p;
 if ((n&&!p)||a%alignment||n>UINTPTR_MAX-a) return 0;
 s->begin=a;s->end=a+n;return 1;
}
static int overlaps(span a,span b) { return a.begin!=a.end&&b.begin!=b.end&&a.begin<b.end&&b.begin<a.end; }
static int disjoint_input(const void *p,size_t n,size_t alignment,const span m[6]) {
 span s;if(!range(p,n,alignment,&s))return 0;
 for(unsigned i=0;i<6;++i)if(overlaps(s,m[i]))return 0;
 return 1;
}
static int gate_input_distinct(const void *p,size_t n,span gate) {
 span input;return range(p,n,1,&input)&&!overlaps(input,gate);
}
static int preflight(const mb_pak_stream_profile_v1 *p,const mb_pak_stream_file_v1 *f,
 uint32_t n,const mb_pak_stream_limits_v1 *l,const mb_pak_stream_io_v1 *io,
 mb_pak_stream_workspace_v1 *w,mb_pak_stream_result_v1 *r,
 mb_pak_prepare_gate_v1 gate,void *gate_context) {
 span m[6];
 if((!gate&&gate_context)||!p||!l||!io||!w||!r||n>MB_PAK_STREAM_MAX_FILES||
 w->records_capacity<n||w->records_capacity>MB_PAK_STREAM_MAX_FILES||
 w->order_capacity<n||w->order_capacity>MB_PAK_STREAM_MAX_FILES||
 w->directories_capacity<l->max_directories||w->directories_capacity>MB_PAK_STREAM_MAX_DIRECTORIES||
 w->index_capacity==0||w->index_capacity>MB_PAK_STREAM_MAX_INDEX_BYTES||
 w->transfer_bytes!=MB_PAK_STREAM_TRANSFER_BYTES||
 !range(w->records,(size_t)w->records_capacity*sizeof(*w->records),_Alignof(mb_pak_stream_record_scratch),&m[0])||
 !range(w->directories,(size_t)w->directories_capacity*sizeof(*w->directories),_Alignof(mb_pak_stream_directory_scratch),&m[1])||
 !range(w->order,(size_t)w->order_capacity*sizeof(*w->order),_Alignof(uint32_t),&m[2])||
 !range(w->index_arena,w->index_capacity,1,&m[3])||!range(w->transfer,w->transfer_bytes,1,&m[4])||
 !range(r,sizeof(*r),_Alignof(mb_pak_stream_result_v1),&m[5]))return 0;
 for(unsigned i=0;i<6;++i)for(unsigned j=0;j<i;++j)if(overlaps(m[i],m[j]))return 0;
 if(!disjoint_input(p,sizeof(*p),_Alignof(mb_pak_stream_profile_v1),m)||
 !disjoint_input(l,sizeof(*l),_Alignof(mb_pak_stream_limits_v1),m)||
 !disjoint_input(io,sizeof(*io),_Alignof(mb_pak_stream_io_v1),m)||
 !disjoint_input(w,sizeof(*w),_Alignof(mb_pak_stream_workspace_v1),m)||
 !disjoint_input(f,(size_t)n*sizeof(*f),_Alignof(mb_pak_stream_file_v1),m))return 0;
 if(p->struct_size!=sizeof(*p)||p->abi_version!=1||p->reserved||
 l->struct_size!=sizeof(*l)||l->abi_version!=1||
 l->max_files>MB_PAK_STREAM_MAX_FILES||l->max_directories>MB_PAK_STREAM_MAX_DIRECTORIES||
 l->max_path_bytes>MB_PAK_STREAM_MAX_PATH_BYTES||l->max_name_bytes>MB_PAK_STREAM_MAX_NAME_BYTES||
 l->max_payload_bytes>MB_PAK_STREAM_MAX_PAYLOAD_BYTES||l->max_archive_bytes>MB_PAK_STREAM_MAX_ARCHIVE_BYTES||
 l->max_work>MB_PAK_STREAM_MAX_WORK||
 io->struct_size!=sizeof(*io)||io->abi_version!=1||!io->read_payload||!io->write_at||
 io->immutable_source!=1||io->unpublished_destination!=1||
 !disjoint_input(p->mount,p->mount_bytes,1,m))return 0;
 for(uint32_t i=0;i<n;++i)if(f[i].struct_size!=sizeof(f[i])||f[i].abi_version!=1||f[i].reserved||
 f[i].verify_sha1>1||!disjoint_input(f[i].path,f[i].path_bytes,1,m))return 0;
 if(gate_context){
  span g;if(!range(gate_context,1,1,&g))return 0;
  for(unsigned i=0;i<6;++i)if(overlaps(g,m[i]))return 0;
  if(!gate_input_distinct(p,sizeof(*p),g)||!gate_input_distinct(l,sizeof(*l),g)||
   !gate_input_distinct(io,sizeof(*io),g)||!gate_input_distinct(w,sizeof(*w),g)||
   !gate_input_distinct(f,(size_t)n*sizeof(*f),g)||!gate_input_distinct(p->mount,p->mount_bytes,g))return 0;
  for(uint32_t i=0;i<n;++i)if(!gate_input_distinct(f[i].path,f[i].path_bytes,g))return 0;
 }
 return 1;
}
void mb_pak_stream_limits_v1_default(mb_pak_stream_limits_v1 *l) {
 if(!l)return;memset(l,0,sizeof(*l));l->struct_size=sizeof(*l);l->abi_version=1;
 l->max_files=MB_PAK_STREAM_MAX_FILES;l->max_directories=MB_PAK_STREAM_MAX_DIRECTORIES;
 l->max_path_bytes=MB_PAK_STREAM_MAX_PATH_BYTES;l->max_name_bytes=MB_PAK_STREAM_MAX_NAME_BYTES;
 l->max_payload_bytes=MB_PAK_STREAM_MAX_PAYLOAD_BYTES;l->max_archive_bytes=MB_PAK_STREAM_MAX_ARCHIVE_BYTES;l->max_work=MB_PAK_STREAM_MAX_WORK;
}
static int check_gate(work *w) {
    if (w->stopped || (w->gate && w->gate(w->gate_context))) {
        w->stopped = 1u; w->status = MB_PAK_STREAM_STOPPED; return 0;
    }
    return 1;
}
static int charge(work *w, uint64_t n) {
    if (w->status != MB_PAK_STREAM_OK) return 0;
    if (w->used > w->limits->max_work || n > w->limits->max_work - w->used) {
        w->status = MB_PAK_STREAM_RESOURCE_LIMIT; return 0;
    }
    w->used += n;
    /* A large work reservation polls once. Actual metadata loops also poll;
     * reservations do not pretend to execute batches of CPU work. */
    w->gate_work += n;
    if (w->gate_work >= 4096u) { w->gate_work = 0u; return check_gate(w); }
    return 1;
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
        if (w->status != MB_PAK_STREAM_OK || path_order(w, o[start], o[child], fold) >= 0) break;
        if (w->status != MB_PAK_STREAM_OK) break;
        const uint32_t tmp = o[start]; o[start] = o[child]; o[child] = tmp; start = child;
    }
}

static void sort_paths(work *w, int fold) {
    for (uint32_t i = w->count / 2; i && w->status == MB_PAK_STREAM_OK; ) path_sift(w, --i, w->count, fold);
    for (uint32_t end = w->count; end > 1 && w->status == MB_PAK_STREAM_OK; ) {
        --end; const uint32_t tmp = w->scratch->order[0];
        w->scratch->order[0] = w->scratch->order[end]; w->scratch->order[end] = tmp;
        path_sift(w, 0, end, fold);
    }
}

static mb_pak_stream_status validate_path(work *w, const uint8_t *p, uint32_t n, int mount) {
    if (!n) return MB_PAK_STREAM_INVALID_PATH;
    if (n > w->limits->max_path_bytes) return MB_PAK_STREAM_RESOURCE_LIMIT;
    if (mount && n == 1 && p[0] == '/') return charge(w, 1) ? MB_PAK_STREAM_OK : w->status;
    if (p[0] == '/' || (mount && p[n-1] != '/') || (!mount && p[n-1] == '/'))
        return MB_PAK_STREAM_INVALID_PATH;
    uint32_t start = 0; int ordinary_seen = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (!charge(w, 1)) return w->status;
        const uint8_t c = p[i];
        if (c >= 128) return MB_PAK_STREAM_UNSUPPORTED_NAME;
        if (c < 32 || c == 127 || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') return MB_PAK_STREAM_INVALID_PATH;
        if (c != '/' && i != n-1) continue;
        const uint32_t end = c == '/' ? i : i+1, bytes = end-start;
        if (!bytes || bytes > 255 || p[start] == ' ' || p[end-1] == ' ' ||
            (bytes == 1 && p[start] == '.')) return MB_PAK_STREAM_INVALID_PATH;
        if (bytes == 2 && p[start] == '.' && p[start+1] == '.') {
            if (!mount || ordinary_seen) return MB_PAK_STREAM_INVALID_PATH;
        } else ordinary_seen = 1;
        start = i+1;
    }
    return MB_PAK_STREAM_OK;
}

static int has_ancestor(work *w, const uint8_t *path, uint32_t bytes) {
    uint32_t lo = 0, hi = w->count;
    while (lo < hi && w->status == MB_PAK_STREAM_OK) {
        const uint32_t mid = lo + (hi-lo)/2;
        const mb_pak_stream_file_v1 *f = &w->files[w->scratch->order[mid]];
        const int c = compare(w, f->path, f->path_bytes, path, bytes, 1);
        if (c < 0) lo = mid+1; else hi = mid;
    }
    if (lo == w->count || w->status != MB_PAK_STREAM_OK) return 0;
    const mb_pak_stream_file_v1 *f = &w->files[w->scratch->order[lo]];
    return compare(w, f->path, f->path_bytes, path, bytes, 1) == 0 && w->status == MB_PAK_STREAM_OK;
}

static const uint8_t *dir_name(work *w, const mb_pak_stream_directory_scratch *d) {
    static const uint8_t root[] = "/";
    return d->input_index == UINT32_MAX ? root : w->files[d->input_index].path;
}

static int dir_compare(work *w, const mb_pak_stream_directory_scratch *a,
    const mb_pak_stream_directory_scratch *b, int fold) {
    return compare(w, dir_name(w,a), a->prefix_bytes, dir_name(w,b), b->prefix_bytes, fold);
}

static int add_directory(work *w, uint32_t input, uint32_t bytes) {
    const mb_pak_stream_directory_scratch d = {input, bytes, 0};
    mb_pak_stream_directory_scratch *dirs = w->scratch->directories;
    uint32_t lo = 0, hi = w->directories;
    while (lo < hi && w->status == MB_PAK_STREAM_OK) {
        const uint32_t mid = lo + (hi-lo)/2;
        if (dir_compare(w, &dirs[mid], &d, 1) < 0) lo = mid+1; else hi = mid;
    }
    if (w->status != MB_PAK_STREAM_OK) return 0;
    if (lo < w->directories && dir_compare(w, &dirs[lo], &d, 1) == 0) {
        if (w->status != MB_PAK_STREAM_OK) return 0;
        if (dir_compare(w, &dirs[lo], &d, 0) != 0 && w->status == MB_PAK_STREAM_OK)
            w->status = MB_PAK_STREAM_PATH_CONFLICT;
        return w->status == MB_PAK_STREAM_OK;
    }
    if (w->status != MB_PAK_STREAM_OK) return 0;
    if (w->directories == w->limits->max_directories) {
        w->status = MB_PAK_STREAM_RESOURCE_LIMIT; return 0;
    }
    if (!charge(w, 1 + (uint64_t)(w->directories-lo) * sizeof(*dirs))) return 0;
    memmove(dirs+lo+1, dirs+lo, (size_t)(w->directories-lo) * sizeof(*dirs));
    dirs[lo] = d; ++w->directories; return 1;
}

static void dir_sift(work *w, uint32_t start, uint32_t end) {
    mb_pak_stream_directory_scratch *d = w->scratch->directories;
    while (start < end / 2) {
        uint32_t child = start*2+1;
        if (child+1 < end && dir_compare(w,&d[child],&d[child+1],0) < 0) ++child;
        if (w->status != MB_PAK_STREAM_OK || dir_compare(w,&d[start],&d[child],0) >= 0) break;
        if (w->status != MB_PAK_STREAM_OK) break;
        const mb_pak_stream_directory_scratch tmp = d[start]; d[start] = d[child]; d[child] = tmp;
        start = child;
    }
}

static void sort_directories(work *w) {
    for (uint32_t i = w->directories/2; i && w->status == MB_PAK_STREAM_OK; ) dir_sift(w,--i,w->directories);
    for (uint32_t end = w->directories; end > 1 && w->status == MB_PAK_STREAM_OK; ) {
        --end; const mb_pak_stream_directory_scratch tmp = w->scratch->directories[0];
        w->scratch->directories[0] = w->scratch->directories[end]; w->scratch->directories[end] = tmp;
        dir_sift(w,0,end);
    }
}

static uint32_t find_directory(work *w, uint32_t input, uint32_t bytes) {
    const mb_pak_stream_directory_scratch target = {input,bytes,0};
    uint32_t lo = 0, hi = w->directories;
    while (lo < hi && w->status == MB_PAK_STREAM_OK) {
        const uint32_t mid = lo+(hi-lo)/2;
        if (dir_compare(w,&w->scratch->directories[mid],&target,0) < 0) lo = mid+1; else hi = mid;
    }
    if (w->status == MB_PAK_STREAM_OK && (lo == w->directories ||
        dir_compare(w,&w->scratch->directories[lo],&target,0) != 0)) w->status = MB_PAK_STREAM_INTERNAL_ERROR;
    return lo;
}

static int prepare(work *w, const mb_pak_stream_profile_v1 *p, uint64_t *payload_bytes,
                   uint64_t *directory_bytes) {
    w->status = validate_path(w,p->mount,p->mount_bytes,1);
    if (w->status != MB_PAK_STREAM_OK) return 0;
    if (w->count > w->limits->max_files) { w->status = MB_PAK_STREAM_RESOURCE_LIMIT; return 0; }
    uint64_t names = 0;
    for (uint32_t i = 0; i < w->count; ++i) {
        const mb_pak_stream_file_v1 *f = &w->files[i];
        w->status = validate_path(w,f->path,f->path_bytes,0);
        if (w->status != MB_PAK_STREAM_OK) return 0;
        names += f->path_bytes;
        if (names > w->limits->max_name_bytes || f->payload_bytes > w->limits->max_payload_bytes) {
            w->status = MB_PAK_STREAM_RESOURCE_LIMIT; return 0;
        }
        *payload_bytes += f->payload_bytes; w->scratch->order[i] = i;
    }
    sort_paths(w,1);
    for (uint32_t i = 1; i < w->count && w->status == MB_PAK_STREAM_OK; ++i)
        if (path_order(w,w->scratch->order[i-1],w->scratch->order[i],1) == 0 &&
            w->status == MB_PAK_STREAM_OK) w->status = MB_PAK_STREAM_PATH_CONFLICT;
    for (uint32_t i = 0; i < w->count && w->status == MB_PAK_STREAM_OK; ++i) {
        const mb_pak_stream_file_v1 *f = &w->files[i];
        for (uint32_t b = 0; b < f->path_bytes && w->status == MB_PAK_STREAM_OK; ++b) {
            if (!charge(w,1)) break;
            if (f->path[b] == '/' && has_ancestor(w,f->path,b)) w->status = MB_PAK_STREAM_PATH_CONFLICT;
        }
    }
    if (w->status != MB_PAK_STREAM_OK) return 0;
    if (w->count && !add_directory(w,UINT32_MAX,1)) return 0;
    for (uint32_t i = 0; i < w->count; ++i) {
        const mb_pak_stream_file_v1 *f = &w->files[i];
        for (uint32_t b = 0; b < f->path_bytes; ++b) {
            if (!charge(w,1)) return 0;
            if (f->path[b] == '/' && !add_directory(w,i,b+1)) return 0;
        }
    }
    sort_paths(w,0); sort_directories(w);
    if (w->status != MB_PAK_STREAM_OK) return 0;
    uint64_t physical = 0;
    for (uint32_t i = 0; i < w->count; ++i) {
        mb_pak_stream_record_scratch *r = &w->scratch->records[i];
        r->input_index = w->scratch->order[i];
        const mb_pak_stream_file_v1 *f = &w->files[r->input_index];
        if (physical > w->limits->max_archive_bytes || 53 + f->payload_bytes > w->limits->max_archive_bytes-physical) {
            w->status = MB_PAK_STREAM_RESOURCE_LIMIT; return 0;
        }
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
            if (hash == w->scratch->records[j].path_hash) { w->status = MB_PAK_STREAM_PATH_CONFLICT; return 0; }
        }
        r->directory_id = find_directory(w,r->filename_offset ? r->input_index : UINT32_MAX,
                                         r->filename_offset ? r->filename_offset : 1);
        if (w->status != MB_PAK_STREAM_OK) return 0;
        ++w->scratch->directories[r->directory_id].file_count;
    }
    *directory_bytes = 4;
    for (uint32_t d = 0; d < w->directories; ++d) {
        if (!charge(w,1)) return 0;
        *directory_bytes += 9 + w->scratch->directories[d].prefix_bytes;
    }
    for (uint32_t i = 0; i < w->count; ++i) {
        if (!charge(w,1)) return 0;
        const mb_pak_stream_record_scratch *r = &w->scratch->records[i];
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
        if ((d & 63u) == 0u && !check_gate(w)) return;
        const mb_pak_stream_directory_scratch *dir = &w->scratch->directories[d];
        string(c,dir_name(w,dir),dir->prefix_bytes); u32(c,dir->file_count);
        for (uint32_t i = 0; i < w->count; ++i) {
            if ((i & 63u) == 0u && !check_gate(w)) return;
            const mb_pak_stream_record_scratch *r = &w->scratch->records[i];
            if (r->directory_id != d) continue;
            const mb_pak_stream_file_v1 *f = &w->files[r->input_index];
            string(c,f->path+r->filename_offset,f->path_bytes-r->filename_offset); u32(c,12*i);
        }
    }
}


static int io_reserve(mb_pak_stream_result_v1 *r,uint32_t n,int reading) {
 uint64_t total=r->requested_read_bytes+r->requested_write_bytes;
 if(total>MB_PAK_STREAM_MAX_IO_BYTES||n>MB_PAK_STREAM_MAX_IO_BYTES-total||
 r->read_calls+r->write_calls>=MB_PAK_STREAM_MAX_IO_CALLS)return 0;
 if(reading){r->requested_read_bytes+=n;++r->read_calls;}
 else{r->requested_write_bytes+=n;++r->write_calls;}
 return 1;
}
static mb_pak_stream_status put_exact(work *w,const mb_pak_stream_io_v1 *io,
 mb_pak_stream_result_v1 *r,uint64_t offset,const uint8_t *p,uint32_t n) {
 uint32_t actual=0;
 if(!check_gate(w))return MB_PAK_STREAM_STOPPED;
 if(offset>r->required_bytes||n>r->required_bytes-offset||n>MB_PAK_STREAM_TRANSFER_BYTES||
 !io_reserve(r,n,0))return MB_PAK_STREAM_RESOURCE_LIMIT;
 int provider_status=io->write_at(io->context,offset,p,n,&actual);
 if(!check_gate(w))return MB_PAK_STREAM_STOPPED;
 if(provider_status!=0)return MB_PAK_STREAM_PROVIDER_ERROR;
 return actual==n?MB_PAK_STREAM_OK:MB_PAK_STREAM_SHORT_IO;
}
static mb_pak_stream_status read_exact(work *w,const mb_pak_stream_io_v1 *io,
 mb_pak_stream_result_v1 *r,uint32_t file,uint64_t offset,uint64_t bound,
 uint8_t *p,uint32_t n) {
 uint32_t actual=0;
 if(!check_gate(w))return MB_PAK_STREAM_STOPPED;
 if(offset>bound||n>bound-offset||n>MB_PAK_STREAM_TRANSFER_BYTES||!io_reserve(r,n,1))
 return MB_PAK_STREAM_RESOURCE_LIMIT;
 int provider_status=io->read_payload(io->context,file,offset,p,n,&actual);
 if(!check_gate(w))return MB_PAK_STREAM_STOPPED;
 if(provider_status!=0)return MB_PAK_STREAM_PROVIDER_ERROR;
 return actual==n?MB_PAK_STREAM_OK:MB_PAK_STREAM_SHORT_IO;
}
static void clear_workspace(mb_pak_stream_workspace_v1 *s) {
 erase(s->records,(size_t)s->records_capacity*sizeof(*s->records));
 erase(s->directories,(size_t)s->directories_capacity*sizeof(*s->directories));
 erase(s->order,(size_t)s->order_capacity*sizeof(*s->order));
 erase(s->index_arena,s->index_capacity);erase(s->transfer,s->transfer_bytes);
}
mb_pak_stream_status mb_pak_stream_write_v11_none(
 const mb_pak_stream_profile_v1 *p,const mb_pak_stream_file_v1 *f,uint32_t count,
 const mb_pak_stream_limits_v1 *l,const mb_pak_stream_io_v1 *io,
 mb_pak_stream_workspace_v1 *s,mb_pak_stream_result_v1 *result) {
 return mb_pak_stream_write_v11_none_gated_v1(p,f,count,l,io,s,result,NULL,NULL);
}
mb_pak_stream_status mb_pak_stream_write_v11_none_gated_v1(
 const mb_pak_stream_profile_v1 *p,const mb_pak_stream_file_v1 *f,uint32_t count,
 const mb_pak_stream_limits_v1 *l,const mb_pak_stream_io_v1 *io,
 mb_pak_stream_workspace_v1 *s,mb_pak_stream_result_v1 *result,
 mb_pak_prepare_gate_v1 gate,void *gate_context) {
 if(!preflight(p,f,count,l,io,s,result,gate,gate_context))return MB_PAK_STREAM_BAD_ARGUMENT;
 memset(result,0,sizeof(*result));result->struct_size=sizeof(*result);result->abi_version=1;
 work w={.files=f,.limits=l,.scratch=s,.count=count,.status=MB_PAK_STREAM_OK,
  .gate=gate,.gate_context=gate_context};
 uint64_t payload_bytes=0,directory_bytes=0,required=0;
 uint8_t digests[3][20]={{0}};
 int complete=0;
 if(!check_gate(&w))goto done;
 clear_workspace(s);
 if(!check_gate(&w))goto done;
 if(!prepare(&w,p,&payload_bytes,&directory_bytes))goto done;
 if(!check_gate(&w))goto done;
 const uint64_t primary_offset=payload_bytes+(uint64_t)count*53;
 const uint64_t primary_bytes=105+p->mount_bytes+(uint64_t)count*12;
 const uint64_t path_offset=primary_offset+primary_bytes;
 const uint64_t path_bytes=4+(uint64_t)count*12+4;
 const uint64_t directory_offset=path_offset+path_bytes;
 const uint64_t footer_offset=directory_offset+directory_bytes;
 required=footer_offset+221;
 const uint64_t index_bytes=required-primary_offset;
 if(required>l->max_archive_bytes||required>UINT32_MAX||index_bytes>s->index_capacity){
 w.status=index_bytes>s->index_capacity?MB_PAK_STREAM_BUFFER_TOO_SMALL:MB_PAK_STREAM_RESOURCE_LIMIT;goto done;}
 /* Exact worst-case call/read/write budget before invoking provider. Headers
  * are written once after payload SHA1 finishes, rather than written twice. */
 uint64_t payload_calls=0;
 for(uint32_t i=0;i<count;++i){
 if((i&63u)==0u&&!check_gate(&w))goto done;
 payload_calls+=(f[i].payload_bytes+MB_PAK_STREAM_TRANSFER_BYTES-1)/MB_PAK_STREAM_TRANSFER_BYTES;}
 const uint64_t callback_calls=payload_calls*2+count+(index_bytes+MB_PAK_STREAM_TRANSFER_BYTES-1)/MB_PAK_STREAM_TRANSFER_BYTES;
 if(callback_calls>MB_PAK_STREAM_MAX_IO_CALLS||payload_bytes+required>MB_PAK_STREAM_MAX_IO_BYTES){
 w.status=MB_PAK_STREAM_RESOURCE_LIMIT;goto done;}
 if(!charge(&w,required*3+payload_bytes+(uint64_t)w.directories*count*2+(uint64_t)count*128))goto done;
 result->required_bytes=required;result->index_bytes=index_bytes;
 /* Build/hash all metadata before touching output. */
 cursor c={s->index_arena,0,(size_t)index_bytes,0};
 string(&c,p->mount,p->mount_bytes);u32(&c,count);u64(&c,p->path_hash_seed);
 u32(&c,1);u64(&c,path_offset);u64(&c,path_bytes);
 const size_t path_digest_at=c.position;zeros(&c,20);
 u32(&c,1);u64(&c,directory_offset);u64(&c,directory_bytes);
 const size_t directory_digest_at=c.position;zeros(&c,20);
 u32(&c,count*12);
 for(uint32_t i=0;i<count;++i){
 if((i&63u)==0u&&!check_gate(&w))goto done;
 u32(&c,UINT32_C(0xe0000000));u32(&c,s->records[i].physical_offset);
 u32(&c,(uint32_t)f[s->records[i].input_index].payload_bytes);}
 u32(&c,0);if(c.position!=path_offset-primary_offset)c.failed=1;
 u32(&c,count);
 for(uint32_t i=0;i<count;++i){
 if((i&63u)==0u&&!check_gate(&w))goto done;
 u64(&c,s->records[i].path_hash);u32(&c,12*i);}
 u32(&c,0); /* Empty v11 pruned-map count; the full directory map follows separately. */
 if(c.position!=directory_offset-primary_offset)c.failed=1;
 directory_map(&c,&w);if(!check_gate(&w))goto done;
 if(c.position!=footer_offset-primary_offset)c.failed=1;
 zeros(&c,17);u32(&c,UINT32_C(0x5a6f12e1));u32(&c,11);
 u64(&c,primary_offset);u64(&c,primary_bytes);
 const size_t primary_digest_at=c.position;zeros(&c,20);zeros(&c,160);
 if(c.failed||c.position!=index_bytes){w.status=MB_PAK_STREAM_INTERNAL_ERROR;goto done;}
 if(!check_gate(&w))goto done;
 if(mbedtls_sha1(s->index_arena+(size_t)(path_offset-primary_offset),(size_t)path_bytes,digests[1])){
 w.status=MB_PAK_STREAM_SHA1_ERROR;goto done;}
 if(!check_gate(&w))goto done;
 if(mbedtls_sha1(s->index_arena+(size_t)(directory_offset-primary_offset),(size_t)directory_bytes,digests[2])){
 w.status=MB_PAK_STREAM_SHA1_ERROR;goto done;}
 if(!check_gate(&w))goto done;
 memcpy(s->index_arena+path_digest_at,digests[1],20);memcpy(s->index_arena+directory_digest_at,digests[2],20);
 if(mbedtls_sha1(s->index_arena,(size_t)primary_bytes,digests[0])){w.status=MB_PAK_STREAM_SHA1_ERROR;goto done;}
 if(!check_gate(&w))goto done;
 memcpy(s->index_arena+primary_digest_at,digests[0],20);
 for(uint32_t i=0;i<count;++i){
 if(!check_gate(&w))goto done;
 mb_pak_stream_record_scratch *r=&s->records[i];const mb_pak_stream_file_v1 *file=&f[r->input_index];
 mbedtls_sha1_context sha;mbedtls_sha1_init(&sha);
 if(mbedtls_sha1_starts(&sha)){w.status=MB_PAK_STREAM_SHA1_ERROR;mbedtls_sha1_free(&sha);goto done;}
 uint64_t offset=0;
 while(offset<file->payload_bytes){
 uint64_t remain=file->payload_bytes-offset;
 uint32_t n=(uint32_t)(remain>MB_PAK_STREAM_TRANSFER_BYTES?MB_PAK_STREAM_TRANSFER_BYTES:remain);
 w.status=read_exact(&w,io,result,r->input_index,offset,file->payload_bytes,s->transfer,n);
 if(w.status!=MB_PAK_STREAM_OK)break;
 if(mbedtls_sha1_update(&sha,s->transfer,n)){w.status=MB_PAK_STREAM_SHA1_ERROR;break;}
 w.status=put_exact(&w,io,result,(uint64_t)r->physical_offset+53+offset,s->transfer,n);
 if(w.status!=MB_PAK_STREAM_OK)break;offset+=n;
 }
 if(w.status==MB_PAK_STREAM_OK&&mbedtls_sha1_finish(&sha,r->sha1))w.status=MB_PAK_STREAM_SHA1_ERROR;
 mbedtls_sha1_free(&sha);if(w.status!=MB_PAK_STREAM_OK)goto done;
 if(file->verify_sha1&&memcmp(file->expected_sha1,r->sha1,20)){
 w.status=MB_PAK_STREAM_PAYLOAD_SHA1_MISMATCH;goto done;}
 uint8_t header[53];cursor hc={header,0,sizeof(header),0};
 u64(&hc,0);u64(&hc,file->payload_bytes);u64(&hc,file->payload_bytes);u32(&hc,0);
 emit(&hc,r->sha1,20);zeros(&hc,1);u32(&hc,0);
 if(hc.failed||hc.position!=53){w.status=MB_PAK_STREAM_INTERNAL_ERROR;goto done;}
 w.status=put_exact(&w,io,result,r->physical_offset,header,sizeof(header));erase(header,sizeof(header));
 if(w.status!=MB_PAK_STREAM_OK)goto done;
 }
 /* Metadata/footer last; caller-owned staging remains unpublished throughout. */
 for(uint64_t offset=0;offset<index_bytes;){
 uint64_t remain=index_bytes-offset;uint32_t n=(uint32_t)(remain>MB_PAK_STREAM_TRANSFER_BYTES?MB_PAK_STREAM_TRANSFER_BYTES:remain);
 w.status=put_exact(&w,io,result,primary_offset+offset,s->index_arena+(size_t)offset,n);
 if(w.status!=MB_PAK_STREAM_OK)goto done;offset+=n;
 }
 complete=1;
done:
 clear_workspace(s);
 /* Digests are provisional while archive_complete is zero. Erase the owned
  * digest scratch before the final sample; failure removes the provisional
  * output copies, and success needs only bounded scalar publication. */
 if(complete){memcpy(result->primary_sha1,digests[0],20);memcpy(result->path_index_sha1,digests[1],20);memcpy(result->directory_sha1,digests[2],20);}
 erase(digests,sizeof(digests));
 if(!check_gate(&w))complete=0;
 if(complete&&w.status==MB_PAK_STREAM_OK){
 result->archive_complete=1;result->archive_bytes=required;result->file_count=count;result->directory_count=w.directories;
 }else{erase(result->primary_sha1,20);erase(result->path_index_sha1,20);erase(result->directory_sha1,20);}
 result->status=w.status;result->work_used=w.used;
 return w.status;
}
