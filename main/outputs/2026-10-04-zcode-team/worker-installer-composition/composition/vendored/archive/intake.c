#define _DARWIN_C_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "intake.h"
#include "private/posix_compat.h"
#include <archive.h>
#include <archive_entry.h>
#include <mbedtls/sha256.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
/* Opt in only with the matching built-in liblzma/libarchive closure. */
#ifndef MB_ARCHIVE_BUILTIN_XZ
#define MB_ARCHIVE_BUILTIN_XZ 0
#endif
typedef struct span { const void *p; size_t n; } span;
typedef struct run {
 const mb_archive_source_v1 *s; const mb_archive_limits_v1 *l;
 mb_archive_budget_v1 *b; const mb_archive_buffers_v1 *v;
 const mb_archive_materialize_request_v1 *q; mb_archive_materialized_v1 *out;
 uint64_t position,expanded; uint32_t count,names;
 int failure; struct stat stamp; mbedtls_sha256_context *emit_hash;
} run;
_Static_assert(sizeof(off_t)>=8,"archive intake requires 64-bit file offsets");
void mb_archive_limits_default_v1(mb_archive_limits_v1 *p) {
 if(p)*p=(mb_archive_limits_v1){MB_ARCHIVE_MAX_RAW_BYTES,MB_ARCHIVE_MAX_MEMBER_BYTES,
 MB_ARCHIVE_MAX_EXPANDED_BYTES,MB_ARCHIVE_MAX_ENTRIES,MB_ARCHIVE_MAX_NAME_BYTES};
}
void mb_archive_budget_default_v1(mb_archive_budget_v1 *p) {
 if(p)*p=(mb_archive_budget_v1){MB_ARCHIVE_MAX_IO_BYTES,MB_ARCHIVE_MAX_OPERATIONS,
 MB_ARCHIVE_MAX_PATH_WORK,0,0,0,0,0,0};
}
static int overlap(span a,span b) {
 uintptr_t x=(uintptr_t)a.p,y=(uintptr_t)b.p;
 return a.n&&b.n&&x<y+b.n&&y<x+a.n;
}
static int spans_ok(const span *v,size_t n) {
 size_t i,j;
 for(i=0;i<n;i++){
  if(v[i].n&&(!v[i].p||(uintptr_t)v[i].p>UINTPTR_MAX-v[i].n))return 0;
  for(j=0;j<i;j++)if(overlap(v[i],v[j]))return 0;
 }return 1;
}
static int aligned(const void *p,size_t a){return (uintptr_t)p%a==0;}
static int empty_bytes(const void *p,size_t n){const uint8_t *s=p;size_t i;for(i=0;i<n;i++)if(s[i])return 0;return 1;}
static int common(const mb_archive_source_v1 *s,const mb_archive_limits_v1 *l,
 mb_archive_budget_v1 *b,const mb_archive_buffers_v1 *v) {
 if(!s||!l||!b||!v||s->struct_size!=sizeof(*s)||s->abi_version!=1||s->reserved||s->fd<0)
  return MB_ARCHIVE_BAD_ARGUMENT;
 if(l->raw_bytes>MB_ARCHIVE_MAX_RAW_BYTES||l->member_bytes>MB_ARCHIVE_MAX_MEMBER_BYTES||
 l->expanded_bytes>MB_ARCHIVE_MAX_EXPANDED_BYTES||l->entries>MB_ARCHIVE_MAX_ENTRIES||
 l->name_bytes>MB_ARCHIVE_MAX_NAME_BYTES||b->io_limit>MB_ARCHIVE_MAX_IO_BYTES||
 b->operation_limit>MB_ARCHIVE_MAX_OPERATIONS||b->path_work_limit>MB_ARCHIVE_MAX_PATH_WORK||
 b->io_used>b->io_limit||b->operations_used>b->operation_limit||b->path_work_used>b->path_work_limit||
 v->row_capacity>MB_ARCHIVE_MAX_ENTRIES||v->name_capacity>MB_ARCHIVE_MAX_NAME_BYTES||
 v->sort_capacity>MB_ARCHIVE_MAX_ENTRIES||v->chunk_capacity!=MB_ARCHIVE_CHUNK_BYTES)
  return MB_ARCHIVE_LIMIT;
 if(!v->rows||!v->names||!v->sort_indices||!v->chunk||
 !aligned(v->rows,_Alignof(mb_archive_row_v1))||!aligned(v->sort_indices,_Alignof(uint32_t)))
  return MB_ARCHIVE_BAD_ARGUMENT;
 return 0;
}
static int check_cancel(run *r) {
 struct timespec t;
 if(r->b->cancelled&&r->b->cancelled(r->b->context))return MB_ARCHIVE_CANCELLED;
 if(r->b->deadline_ms){
  if(clock_gettime(CLOCK_MONOTONIC,&t)||t.tv_sec<0||(uint64_t)t.tv_sec>UINT64_MAX/1000)
   return MB_ARCHIVE_IO_ERROR;
  if((uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000>=r->b->deadline_ms)return MB_ARCHIVE_CANCELLED;
 }return 0;
}
static int charge(run *r,uint64_t bytes,uint64_t ops) {
 int e;
 if(bytes>r->b->io_limit-r->b->io_used||ops>r->b->operation_limit-r->b->operations_used)
  return MB_ARCHIVE_LIMIT;
 r->b->io_used+=bytes;r->b->operations_used+=ops;e=check_cancel(r);
 if(e)r->failure=e;return e;
}
static int work(run *r,uint64_t n) {
 if(n>r->b->path_work_limit-r->b->path_work_used)return MB_ARCHIVE_LIMIT;
 r->b->path_work_used+=n;return check_cancel(r);
}
static int stamp_same(const struct stat *a,const struct stat *b) {
#ifdef __APPLE__
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&
 a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
 a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
 return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_size==b->st_size&&
 a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
 a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int source_start(run *r) {
 int e,flags;
 if(r->s->size>r->l->raw_bytes)return MB_ARCHIVE_LIMIT;
 if((e=charge(r,0,2)))return e;
 flags=fcntl(r->s->fd,F_GETFL);
 if(flags<0||fstat(r->s->fd,&r->stamp))return MB_ARCHIVE_IO_ERROR;
 if(!S_ISREG(r->stamp.st_mode)||(flags&O_ACCMODE)!=O_RDONLY||r->stamp.st_size<0)
  return MB_ARCHIVE_BAD_ARGUMENT;
 if((uint64_t)r->stamp.st_size!=r->s->size)return MB_ARCHIVE_SOURCE_CHANGED;
 return 0;
}
static int hash_fd(run *r,int fd,uint64_t size,uint8_t digest[32]) {
 mbedtls_sha256_context h;uint64_t off=0;ssize_t got;size_t want;int e=0;
 mbedtls_sha256_init(&h);if(mbedtls_sha256_starts(&h,0)){e=MB_ARCHIVE_IO_ERROR;goto done;}
 while(off<size){
  want=(size_t)(size-off>MB_ARCHIVE_CHUNK_BYTES?MB_ARCHIVE_CHUNK_BYTES:size-off);
  if((e=charge(r,want,1)))goto done;
  got=pread(fd,r->v->chunk,want,(off_t)off);
  if(got<0){e=MB_ARCHIVE_IO_ERROR;goto done;}
  if((size_t)got!=want){e=MB_ARCHIVE_SOURCE_CHANGED;goto done;}
  if(mbedtls_sha256_update(&h,r->v->chunk,want)){e=MB_ARCHIVE_IO_ERROR;goto done;}
  off+=want;
 }
 if((e=charge(r,1,1)))goto done;
 got=pread(fd,r->v->chunk,1,(off_t)size);
 if(got!=0){e=got<0?MB_ARCHIVE_IO_ERROR:MB_ARCHIVE_SOURCE_CHANGED;goto done;}
 if(mbedtls_sha256_finish(&h,digest))e=MB_ARCHIVE_IO_ERROR;
done:mbedtls_sha256_free(&h);return e;
}
static int source_end(run *r) {
 struct stat st;uint8_t h[32];int e;
 if((e=hash_fd(r,r->s->fd,r->s->size,h)))return e;
 if(memcmp(h,r->s->sha256,32))return MB_ARCHIVE_IDENTITY_MISMATCH;
 if((e=charge(r,0,1)))return e;
 if(fstat(r->s->fd,&st))return MB_ARCHIVE_IO_ERROR;
 return stamp_same(&r->stamp,&st)?0:MB_ARCHIVE_SOURCE_CHANGED;
}
static la_ssize_t read_archive(struct archive *a,void *context,const void **out) {
 run *r=context;size_t want;ssize_t got;int e;(void)a;*out=r->v->chunk;
 want=(size_t)(r->s->size-r->position>MB_ARCHIVE_CHUNK_BYTES?MB_ARCHIVE_CHUNK_BYTES:r->s->size-r->position);
 if(!want)return 0;
 if((e=charge(r,want,1))){r->failure=e;return -1;}
 got=pread(r->s->fd,r->v->chunk,want,(off_t)r->position);
 if(got<0||(size_t)got!=want){r->failure=got<0?MB_ARCHIVE_IO_ERROR:MB_ARCHIVE_SOURCE_CHANGED;return -1;}
 r->position+=want;return got;
}
static la_int64_t seek_archive(struct archive *a,void *context,la_int64_t off,int origin) {
 run *r=context;uint64_t base,next;int e;(void)a;
 if((e=charge(r,0,1))){r->failure=e;return -1;}
 if(origin==SEEK_SET)base=0;else if(origin==SEEK_CUR)base=r->position;
 else if(origin==SEEK_END)base=r->s->size;else {r->failure=MB_ARCHIVE_READER_REJECTED;return -1;}
 if(off<0){uint64_t mag=(uint64_t)(-(off+1))+1;if(mag>base)return -1;next=base-mag;}
 else {if((uint64_t)off>r->s->size-base)return -1;next=base+(uint64_t)off;}
 if(next>r->s->size)return -1;r->position=next;return (la_int64_t)next;
}
static int close_archive(struct archive *a,void *context) {
 run *r=context;(void)a;return charge(r,0,1)?ARCHIVE_FATAL:ARCHIVE_OK;
}
static int reader_new(run *r,struct archive **out) {
 struct archive *a;int e;
 if((e=charge(r,0,1)))return e;
 a=archive_read_new();if(!a)return MB_ARCHIVE_IO_ERROR;*out=a;
#define REGISTER(fn) do {if((e=charge(r,0,1)))return e;if(fn(a)!=ARCHIVE_OK)return MB_ARCHIVE_READER_REJECTED;} while(0)
 REGISTER(archive_read_support_format_zip);REGISTER(archive_read_support_format_tar);
 REGISTER(archive_read_support_format_7zip);REGISTER(archive_read_support_format_rar);
 REGISTER(archive_read_support_format_rar5);REGISTER(archive_read_support_filter_none);
 REGISTER(archive_read_support_filter_gzip);
#if MB_ARCHIVE_BUILTIN_XZ
 REGISTER(archive_read_support_filter_xz);
#endif
#undef REGISTER
 if(archive_read_set_callback_data(a,r)!=ARCHIVE_OK||archive_read_set_read_callback(a,read_archive)!=ARCHIVE_OK||
 archive_read_set_seek_callback(a,seek_archive)!=ARCHIVE_OK||archive_read_set_close_callback(a,close_archive)!=ARCHIVE_OK)
  return MB_ARCHIVE_READER_REJECTED;
 if((e=charge(r,0,1)))return e;
 if(archive_read_open1(a)!=ARCHIVE_OK)return r->failure?r->failure:MB_ARCHIVE_READER_REJECTED;
 return 0;
}
static int utf8(const unsigned char *s,size_t n) {
 size_t i=0;while(i<n){unsigned c=s[i++],x,min,k,j;
  if(c<128){if(c<32||c==127)return 0;continue;}
  if(c>=0xc2&&c<=0xdf){x=c&31;k=1;min=128;}
  else if(c>=0xe0&&c<=0xef){x=c&15;k=2;min=2048;}
  else if(c>=0xf0&&c<=0xf4){x=c&7;k=3;min=65536;}
  else return 0;
  if(k>n-i)return 0;
  for(j=0;j<k;j++){c=s[i++];if((c&0xc0)!=0x80)return 0;x=(x<<6)|(c&63);}
  if(x<min||x>0x10ffff||(x>=0xd800&&x<=0xdfff))return 0;
 }return 1;
}
static int name_row(run *r,const char *name,mb_archive_row_v1 *row) {
 size_t n,from=0,end,used=0;char *path;int e;
 if(!name)return MB_ARCHIVE_UNSAFE_PATH;
 n=strnlen(name,MB_ARCHIVE_MAX_PATH_BYTES+1);
 if(n>MB_ARCHIVE_MAX_PATH_BYTES)return MB_ARCHIVE_LIMIT;
 if(!n||name[0]=='/'||name[0]=='\\'||!utf8((const unsigned char *)name,n))return MB_ARCHIVE_UNSAFE_PATH;
 if((e=work(r,n*4)))return e;
 if(2*(n+1)>r->l->name_bytes-r->names||2*(n+1)>r->v->name_capacity-r->names)return MB_ARCHIVE_LIMIT;
 row->original_offset=r->names;row->original_length=(uint32_t)n;
 memcpy(r->v->names+r->names,name,n+1);r->names+=(uint32_t)n+1;
 row->path_offset=r->names;path=r->v->names+r->names;
 while(from<n){
  end=from;while(end<n&&name[end]!='/'&&name[end]!='\\'){if(name[end]==':')return MB_ARCHIVE_UNSAFE_PATH;++end;}
  if(end-from==2&&name[from]=='.'&&name[from+1]=='.')return MB_ARCHIVE_UNSAFE_PATH;
  if(end>from&&!(end-from==1&&name[from]=='.')){
   if(name[end-1]=='.'||name[end-1]==' ')return MB_ARCHIVE_UNSAFE_PATH;
   if(used)path[used++]='/';memcpy(path+used,name+from,end-from);used+=end-from;
  }from=end+1;
 }
 if(!used)return MB_ARCHIVE_UNSAFE_PATH;path[used]=0;
 row->path_length=(uint32_t)used;r->names+=(uint32_t)used+1;return 0;
}
static unsigned folded(unsigned c){return c>='A'&&c<='Z'?c+('a'-'A'):c;}
static int compare(run *r,uint32_t x,uint32_t y,int *value) {
 const mb_archive_row_v1 *a=r->v->rows+x,*b=r->v->rows+y;
 size_t n=a->path_length<b->path_length?a->path_length:b->path_length,i;int e;
 if((e=work(r,n+1)))return e;
 for(i=0;i<n;i++){unsigned aa=folded((unsigned char)r->v->names[a->path_offset+i]);
  unsigned bb=folded((unsigned char)r->v->names[b->path_offset+i]);if(aa!=bb){*value=aa<bb?-1:1;return 0;}}
 *value=a->path_length==b->path_length?0:a->path_length<b->path_length?-1:1;return 0;
}
static int sift(run *r,uint32_t root,uint32_t n) {
 uint32_t child,tmp;int cmp,e;
 while(root<n/2){child=root*2+1;
  if(child+1<n){if((e=compare(r,r->v->sort_indices[child],r->v->sort_indices[child+1],&cmp)))return e;if(cmp<0)++child;}
  if((e=compare(r,r->v->sort_indices[root],r->v->sort_indices[child],&cmp)))return e;
  if(cmp>=0)break;tmp=r->v->sort_indices[root];r->v->sort_indices[root]=r->v->sort_indices[child];r->v->sort_indices[child]=tmp;root=child;
 }return 0;
}
static int validate_paths(run *r) {
 uint32_t i,j,end,tmp,lo,hi,mid;int cmp,e;
 for(i=0;i<r->count;i++)r->v->sort_indices[i]=i;
 for(i=r->count/2;i>0;i--)if((e=sift(r,i-1,r->count)))return e;
 for(end=r->count;end>1;){--end;tmp=r->v->sort_indices[0];r->v->sort_indices[0]=r->v->sort_indices[end];r->v->sort_indices[end]=tmp;if((e=sift(r,0,end)))return e;}
 for(i=1;i<r->count;i++){if((e=compare(r,r->v->sort_indices[i-1],r->v->sort_indices[i],&cmp)))return e;if(!cmp)return MB_ARCHIVE_UNSAFE_PATH;}
 /* Every slash prefix is searched; event-extra cannot hide ancestor event. */
 for(i=0;i<r->count;i++){const mb_archive_row_v1 *child=r->v->rows+i;const char *p=r->v->names+child->path_offset;
  for(j=0;j<child->path_length;j++)if(p[j]=='/'){
   lo=0;hi=r->count;
   while(lo<hi){const mb_archive_row_v1 *a;const char *s;uint32_t k,n;
    mid=lo+(hi-lo)/2;a=r->v->rows+r->v->sort_indices[mid];s=r->v->names+a->path_offset;n=a->path_length<j?a->path_length:j;
    if((e=work(r,n+1)))return e;cmp=0;
    for(k=0;k<n;k++){unsigned x=folded((unsigned char)s[k]),y=folded((unsigned char)p[k]);if(x!=y){cmp=x<y?-1:1;break;}}
    if(!cmp)cmp=a->path_length==j?0:a->path_length<j?-1:1;
    if(!cmp){if(a->kind==MB_ARCHIVE_REGULAR)return MB_ARCHIVE_UNSAFE_PATH;break;}
    if(cmp<0)lo=mid+1;else hi=mid;
   }
  }
 }return 0;
}
static void le64(uint8_t p[8],uint64_t x){unsigned i;for(i=0;i<8;i++){p[i]=(uint8_t)x;x>>=8;}}
static int hu64(mbedtls_sha256_context *h,uint64_t x){uint8_t p[8];le64(p,x);return mbedtls_sha256_update(h,p,8);}
static int inventory_digest(const mb_archive_source_v1 *s,const mb_archive_row_v1 *rows,
 const char *names,uint32_t count,uint32_t bytes,uint8_t out[32]) {
 mbedtls_sha256_context h;uint32_t i;int e=0;mbedtls_sha256_init(&h);
 if(mbedtls_sha256_starts(&h,0)||mbedtls_sha256_update(&h,(const unsigned char *)"MBARCHIVE1",10)||
 hu64(&h,s->size)||mbedtls_sha256_update(&h,s->sha256,32)||mbedtls_sha256_update(&h,s->target_token,32)||
 hu64(&h,count)||hu64(&h,bytes)){e=MB_ARCHIVE_IO_ERROR;goto done;}
 for(i=0;i<count;i++){const mb_archive_row_v1 *p=rows+i;
  if(hu64(&h,p->kind)||hu64(&h,p->size)||hu64(&h,p->original_length)||
  mbedtls_sha256_update(&h,(const unsigned char *)names+p->original_offset,p->original_length)||
  hu64(&h,p->path_length)||mbedtls_sha256_update(&h,(const unsigned char *)names+p->path_offset,p->path_length)||
  mbedtls_sha256_update(&h,p->sha256,32)){e=MB_ARCHIVE_IO_ERROR;goto done;}
 }if(mbedtls_sha256_finish(&h,out))e=MB_ARCHIVE_IO_ERROR;
done:mbedtls_sha256_free(&h);return e;
}
static int write_all(run *r,int fd,const void *p,size_t n,mb_archive_owned_file_v1 *file) {
 const uint8_t *bytes=p;int e;ssize_t got;
 while(n){if((e=charge(r,n,1)))return e;got=write(fd,bytes,n);
  if(got<=0)return MB_ARCHIVE_IO_ERROR;
  if(r->emit_hash&&mbedtls_sha256_update(r->emit_hash,bytes,(size_t)got))return MB_ARCHIVE_IO_ERROR;
  bytes+=got;n-=(size_t)got;file->written_bytes+=(uint64_t)got;
 }return 0;
}
static int create_owned(run *r,mb_archive_owned_file_v1 *p,int *fd) {
 struct stat st;int e;
 if((e=charge(r,0,1)))return e;
 *fd=mb_archive_posix_openat(r->q->quarantine_fd,p->name,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
 if(*fd<0)return MB_ARCHIVE_IO_ERROR;p->state=MB_ARCHIVE_RETAINED;
 if((e=charge(r,0,1)))return e;if(fstat(*fd,&st))return MB_ARCHIVE_IO_ERROR;
 p->device=(uint64_t)st.st_dev;p->inode=(uint64_t)st.st_ino;p->state=MB_ARCHIVE_CREATED;return 0;
}
static int finish_owned(run *r,int *fd) {
 int e=charge(r,0,1),bad=0;
 if(!e&&fsync(*fd))e=MB_ARCHIVE_IO_ERROR;
 if(r->b->operations_used<r->b->operation_limit)++r->b->operations_used;else bad=1;
 if(close(*fd))bad=1;*fd=-1;return e?e:bad?MB_ARCHIVE_IO_ERROR:0;
}
static int selected_slot(run *r,uint32_t row) {
 uint32_t i;if(!r->q)return -1;
 for(i=0;i<r->q->selected_count;i++)if(r->q->selected[i]==row)return (int)i+1;return -1;
}
static int walk(run *r,mb_archive_inventory_v1 *result) {
 struct archive *a=NULL;struct archive_entry *entry;mbedtls_sha256_context h;
 int e=0,code,slot,fd=-1,hash_live=0,close_code,free_code;uint32_t kind,i;
 uint64_t declared,actual;la_ssize_t got;size_t want;uint8_t decoded[MB_ARCHIVE_CHUNK_BYTES];
 if((e=reader_new(r,&a)))goto done;
 for(;;){
  if((e=charge(r,0,1)))goto done;code=archive_read_next_header(a,&entry);
  if(code==ARCHIVE_EOF)break;
  if(code!=ARCHIVE_OK){e=r->failure?r->failure:MB_ARCHIVE_READER_REJECTED;goto done;}
  if(r->count>=r->l->entries||r->count>=r->v->row_capacity||r->count>=r->v->sort_capacity){e=MB_ARCHIVE_LIMIT;goto done;}
  if(archive_entry_is_encrypted(entry)||archive_entry_symlink(entry)||archive_entry_hardlink(entry)||archive_entry_sparse_count(entry)){e=MB_ARCHIVE_UNSUPPORTED_ENTRY;goto done;}
  kind=archive_entry_filetype(entry)==AE_IFREG?MB_ARCHIVE_REGULAR:archive_entry_filetype(entry)==AE_IFDIR?MB_ARCHIVE_DIRECTORY:0;
  if(!kind){e=MB_ARCHIVE_UNSUPPORTED_ENTRY;goto done;}
  if(!archive_entry_size_is_set(entry)||archive_entry_size(entry)<0){e=MB_ARCHIVE_READER_REJECTED;goto done;}
  declared=(uint64_t)archive_entry_size(entry);
  if((kind==MB_ARCHIVE_DIRECTORY&&declared)||declared>r->l->member_bytes||declared>r->l->expanded_bytes-r->expanded){e=MB_ARCHIVE_LIMIT;goto done;}
  {mb_archive_row_v1 *row=r->v->rows+r->count;row->kind=kind;row->size=declared;
   if((e=name_row(r,archive_entry_pathname_utf8(entry),row)))goto done;
   if(r->q){const mb_archive_row_v1 *expected;
    if(r->count>=r->q->inventory->row_count){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto done;}
    expected=r->q->rows+r->count;
    if(expected->kind!=kind||expected->size!=declared||expected->original_length!=row->original_length||expected->path_length!=row->path_length||
    memcmp(r->q->names+expected->original_offset,r->v->names+row->original_offset,row->original_length)||
    memcmp(r->q->names+expected->path_offset,r->v->names+row->path_offset,row->path_length)){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto done;}
   }
  }
  if(r->q&&(e=work(r,r->q->selected_count)))goto done;
  slot=selected_slot(r,r->count);if(slot>=0&&(e=create_owned(r,r->q->journal+slot,&fd)))goto done;
  mbedtls_sha256_init(&h);hash_live=1;if(mbedtls_sha256_starts(&h,0)){e=MB_ARCHIVE_IO_ERROR;goto done;}
  actual=0;
  for(;;){
   want=(size_t)(declared-actual>=MB_ARCHIVE_CHUNK_BYTES?MB_ARCHIVE_CHUNK_BYTES:declared-actual+1);
   if((e=charge(r,want,1)))goto done;
   /* Distinct bounded decoded storage: libarchive may retain the callback
    * chunk, so using it as the archive_read_data destination would alias. */
   got=archive_read_data(a,decoded,want);
   if(got<0){e=r->failure?r->failure:MB_ARCHIVE_READER_REJECTED;goto done;}
   if(!got)break;
   if((uint64_t)got>declared-actual){e=MB_ARCHIVE_READER_REJECTED;goto done;}
   actual+=(uint64_t)got;r->expanded+=(uint64_t)got;
   if(mbedtls_sha256_update(&h,decoded,(size_t)got)){e=MB_ARCHIVE_IO_ERROR;goto done;}
   if(fd>=0&&(e=write_all(r,fd,decoded,(size_t)got,r->q->journal+slot)))goto done;
  }
  if(actual!=declared){e=MB_ARCHIVE_READER_REJECTED;goto done;}
  if(mbedtls_sha256_finish(&h,r->v->rows[r->count].sha256)){e=MB_ARCHIVE_IO_ERROR;goto done;}
  mbedtls_sha256_free(&h);hash_live=0;
  if(r->q&&memcmp(r->v->rows[r->count].sha256,r->q->rows[r->count].sha256,32)){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto done;}
  if(fd>=0&&(e=finish_owned(r,&fd)))goto done;++r->count;
 }
 for(i=0;i<(uint32_t)archive_filter_count(a);i++){
  int filter=archive_filter_code(a,(int)i);
  if(filter!=ARCHIVE_FILTER_NONE&&filter!=ARCHIVE_FILTER_GZIP&&!(MB_ARCHIVE_BUILTIN_XZ&&filter==ARCHIVE_FILTER_XZ)){e=MB_ARCHIVE_UNSUPPORTED_ENTRY;goto done;}
 }
 if((archive_read_format_capabilities(a)&(ARCHIVE_READ_FORMAT_CAPS_ENCRYPT_DATA|ARCHIVE_READ_FORMAT_CAPS_ENCRYPT_METADATA))&&archive_read_has_encrypted_entries(a)!=0){e=MB_ARCHIVE_UNSUPPORTED_ENTRY;goto done;}
 result->format_code=(uint32_t)archive_format(a);result->filter_count=(uint32_t)archive_filter_count(a);
 if((e=validate_paths(r)))goto done;
done:
 if(hash_live)mbedtls_sha256_free(&h);
 memset(decoded,0,sizeof(decoded));
 if(fd>=0){if(r->b->operations_used<r->b->operation_limit)++r->b->operations_used;close(fd);}
 if(a){close_code=archive_read_close(a);free_code=archive_read_free(a);
  if(!e&&(close_code!=ARCHIVE_OK||free_code!=ARCHIVE_OK))e=r->failure?r->failure:MB_ARCHIVE_READER_REJECTED;
 }return e;
}
static void clear_buffers(const mb_archive_buffers_v1 *v) {
 memset(v->rows,0,(size_t)v->row_capacity*sizeof(*v->rows));memset(v->names,0,v->name_capacity);
 memset(v->sort_indices,0,(size_t)v->sort_capacity*sizeof(*v->sort_indices));memset(v->chunk,0,v->chunk_capacity);
}
static void inventory_failure(mb_archive_inventory_v1 *r,int e) {
 memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=1;r->status=(uint32_t)e;
}
static int scan_spans(const mb_archive_source_v1 *s,const mb_archive_limits_v1 *l,
 mb_archive_budget_v1 *b,const mb_archive_buffers_v1 *v,mb_archive_inventory_v1 *out) {
 span p[]={{s,sizeof(*s)},{l,sizeof(*l)},{b,sizeof(*b)},{v,sizeof(*v)},{out,sizeof(*out)},
 {v->rows,(size_t)v->row_capacity*sizeof(*v->rows)},{v->names,v->name_capacity},
 {v->sort_indices,(size_t)v->sort_capacity*sizeof(*v->sort_indices)},{v->chunk,v->chunk_capacity}};
 return spans_ok(p,sizeof(p)/sizeof(*p));
}
int mb_archive_scan_v1(const mb_archive_source_v1 *s,const mb_archive_limits_v1 *l,
 mb_archive_budget_v1 *b,const mb_archive_buffers_v1 *v,mb_archive_inventory_v1 *out) {
 run r;int e;
 if(!out)return MB_ARCHIVE_BAD_ARGUMENT;if((e=common(s,l,b,v)))return e;
 if(!scan_spans(s,l,b,v,out))return MB_ARCHIVE_BAD_ARGUMENT;
 memset(&r,0,sizeof(r));r.s=s;r.l=l;r.b=b;r.v=v;clear_buffers(v);inventory_failure(out,0);
 if((e=source_start(&r))||(e=walk(&r,out))||(e=source_end(&r))||
 (e=inventory_digest(s,v->rows,v->names,r.count,r.names,out->inventory_sha256)))goto failure;
 out->row_count=r.count;out->name_bytes=r.names;out->expanded_bytes=r.expanded;
 out->source_size=s->size;out->source_device=(uint64_t)r.stamp.st_dev;out->source_inode=(uint64_t)r.stamp.st_ino;
 memcpy(out->source_sha256,s->sha256,32);memcpy(out->target_token,s->target_token,32);out->complete=1;
 memset(v->chunk,0,v->chunk_capacity);return 0;
failure:clear_buffers(v);inventory_failure(out,e);return e;
}
static int view_valid(const mb_archive_materialize_request_v1 *q) {
 uint32_t i;const mb_archive_inventory_v1 *v=q->inventory;
 if(!v||v->struct_size!=sizeof(*v)||v->abi_version!=1||v->status||v->complete!=1||v->installation_supported||v->reserved||
 v->row_count>q->row_capacity||q->row_capacity>MB_ARCHIVE_MAX_ENTRIES||v->name_bytes>q->name_capacity||
 q->name_capacity>MB_ARCHIVE_MAX_NAME_BYTES||!q->rows||!q->names||!aligned(q->rows,_Alignof(mb_archive_row_v1)))return 0;
 for(i=0;i<v->row_count;i++){const mb_archive_row_v1 *p=q->rows+i;
  if(p->reserved||(p->kind!=MB_ARCHIVE_REGULAR&&p->kind!=MB_ARCHIVE_DIRECTORY)||
  p->original_offset>=v->name_bytes||p->original_length>=v->name_bytes-p->original_offset||
  p->path_offset>=v->name_bytes||p->path_length>=v->name_bytes-p->path_offset||
  p->original_length>MB_ARCHIVE_MAX_PATH_BYTES||p->path_length>MB_ARCHIVE_MAX_PATH_BYTES||
  q->names[p->original_offset+p->original_length]||q->names[p->path_offset+p->path_length])return 0;
 }return 1;
}
static int materialize_spans(const mb_archive_materialize_request_v1 *q,
 const mb_archive_limits_v1 *l,mb_archive_budget_v1 *b,mb_archive_materialized_v1 *out) {
 const mb_archive_buffers_v1 *v=&q->replay;
 span p[]={{q,sizeof(*q)},{q->source,sizeof(*q->source)},{q->inventory,sizeof(*q->inventory)},
 {l,sizeof(*l)},{b,sizeof(*b)},{out,sizeof(*out)},
 {q->rows,(size_t)q->row_capacity*sizeof(*q->rows)},{q->names,q->name_capacity},
 {q->selected,(size_t)q->selected_count*sizeof(*q->selected)},{q->journal,(size_t)q->journal_capacity*sizeof(*q->journal)},
 {v->rows,(size_t)v->row_capacity*sizeof(*v->rows)},{v->names,v->name_capacity},
 {v->sort_indices,(size_t)v->sort_capacity*sizeof(*v->sort_indices)},{v->chunk,v->chunk_capacity}};
 return spans_ok(p,sizeof(p)/sizeof(*p));
}
static int private_empty(run *r) {
 struct stat st;DIR *d;struct dirent *item;int fd,e=0;
 if((e=charge(r,0,2)))return e;
 if(fstat(r->q->quarantine_fd,&st)||!S_ISDIR(st.st_mode)||
 (uint64_t)st.st_dev!=r->q->quarantine_device||(uint64_t)st.st_ino!=r->q->quarantine_inode||
 st.st_uid!=geteuid()||(st.st_mode&077))return MB_ARCHIVE_BAD_ARGUMENT;
 /* dup shares the directory position and could miss existing names after a
  * caller enumeration. Open a new description rooted at the trusted FD. */
 fd=mb_archive_posix_openat(r->q->quarantine_fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
 if(fd<0)return MB_ARCHIVE_IO_ERROR;
 d=fdopendir(fd);if(!d){close(fd);return MB_ARCHIVE_IO_ERROR;}
 for(;;){if((e=charge(r,0,1)))break;errno=0;item=readdir(d);
  if(!item){if(errno)e=MB_ARCHIVE_IO_ERROR;break;}
  if(strcmp(item->d_name,".")&&strcmp(item->d_name,"..")){e=MB_ARCHIVE_BAD_ARGUMENT;break;}
 }if(closedir(d)&&!e)e=MB_ARCHIVE_IO_ERROR;return e;
}
static int emit_u64(run *r,int fd,uint64_t x,mb_archive_owned_file_v1 *f) {
 uint8_t p[8];le64(p,x);return write_all(r,fd,p,8,f);
}
static int emit_manifest(run *r,int fd,mb_archive_owned_file_v1 *f,int intent) {
 uint32_t i,j;int e;mbedtls_sha256_context h;const mb_archive_materialize_request_v1 *q=r->q;
 const char *magic=intent?"MBARINT1":"MBARMAN1";
 if((e=work(r,(uint64_t)q->inventory->row_count*q->selected_count)))return e;
 mbedtls_sha256_init(&h);r->emit_hash=&h;
 if(mbedtls_sha256_starts(&h,0)){e=MB_ARCHIVE_IO_ERROR;goto done;}
 if((e=write_all(r,fd,magic,8,f))||(e=write_all(r,fd,q->source->sha256,32,f))||
 (e=write_all(r,fd,q->inventory->inventory_sha256,32,f))||(e=write_all(r,fd,q->target_token,32,f))||
 (e=emit_u64(r,fd,q->selected_count,f)))goto done;
 /* Canonical row-index order independent of selected[] ordering. */
 for(i=0;i<q->inventory->row_count;i++)for(j=0;j<q->selected_count;j++)if(q->selected[j]==i){
  const mb_archive_row_v1 *row=q->rows+i;const mb_archive_owned_file_v1 *member=q->journal+j+1;
  if((e=emit_u64(r,fd,i,f))||(e=emit_u64(r,fd,row->size,f))||(e=write_all(r,fd,row->sha256,32,f))||
  (e=emit_u64(r,fd,strlen(member->name),f))||(e=write_all(r,fd,member->name,strlen(member->name),f))||
  (e=emit_u64(r,fd,row->original_length,f))||(e=write_all(r,fd,q->names+row->original_offset,row->original_length,f))||
  (e=emit_u64(r,fd,row->path_length,f))||(e=write_all(r,fd,q->names+row->path_offset,row->path_length,f)))goto done;
 }
 f->expected_size=f->written_bytes;
 e=mbedtls_sha256_finish(&h,f->sha256)?MB_ARCHIVE_IO_ERROR:0;
done:r->emit_hash=NULL;mbedtls_sha256_free(&h);return e;
}
static int readback(run *r,mb_archive_owned_file_v1 *p) {
 int fd,e,bad=0;struct stat st;uint8_t hash[32];
 if((e=charge(r,0,2)))return e;
 fd=mb_archive_posix_openat(r->q->quarantine_fd,p->name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);if(fd<0)return MB_ARCHIVE_IO_ERROR;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||(uint64_t)st.st_dev!=p->device||
 (uint64_t)st.st_ino!=p->inode||st.st_size<0||(uint64_t)st.st_size!=p->expected_size)e=MB_ARCHIVE_IDENTITY_MISMATCH;
 else {e=hash_fd(r,fd,p->expected_size,hash);if(!e&&memcmp(hash,p->sha256,32))e=MB_ARCHIVE_IDENTITY_MISMATCH;}
 if(r->b->operations_used<r->b->operation_limit)++r->b->operations_used;else bad=1;
 if(close(fd))bad=1;return e?e:bad?MB_ARCHIVE_IO_ERROR:0;
}
static int cleanup(run *r) {
 uint32_t i;int good=1;struct stat st;
 /* Cleanup allowance was reserved before creation; cancellation cannot
  * suppress local cleanup attempts. Keep ownership states on all failures. */
 for(i=r->out->journal_count;i>0;i--){mb_archive_owned_file_v1 *p=r->q->journal+i-1;
  if(p->state==MB_ARCHIVE_PLANNED||p->state==MB_ARCHIVE_REMOVED)continue;
  /* The durable intent remains available whenever any member/control path
   * could not be safely removed, including an identity replacement. */
  if(i==1&&!good){p->state=MB_ARCHIVE_RETAINED;continue;}
  if(mb_archive_posix_fstatat(r->q->quarantine_fd,p->name,&st,AT_SYMLINK_NOFOLLOW)){
   if(errno==ENOENT)p->state=MB_ARCHIVE_REMOVED;else {p->state=MB_ARCHIVE_RETAINED;good=0;}continue;
  }
  if(!p->inode||!S_ISREG(st.st_mode)||(uint64_t)st.st_dev!=p->device||(uint64_t)st.st_ino!=p->inode||
  mb_archive_posix_unlinkat(r->q->quarantine_fd,p->name,0)){p->state=MB_ARCHIVE_RETAINED;good=0;}
  else p->state=MB_ARCHIVE_REMOVED;
 }if(fsync(r->q->quarantine_fd))good=0;return good;
}
int mb_archive_materialize_v1(const mb_archive_materialize_request_v1 *q,
 const mb_archive_limits_v1 *l,mb_archive_budget_v1 *b,mb_archive_materialized_v1 *out) {
 run r;int e,fd=-1;uint32_t i,j;uint8_t hash[32];mb_archive_inventory_v1 replay;
 mb_archive_owned_file_v1 *intent,*temporary,*final;uint64_t cleanup_ops;
 if(!q||!out)return MB_ARCHIVE_BAD_ARGUMENT;if((e=common(q->source,l,b,&q->replay)))return e;
 if(q->reserved||q->quarantine_fd<0||!q->selected||!q->journal||!q->selected_count||
 q->selected_count>MB_ARCHIVE_MAX_ENTRIES||q->journal_capacity>MB_ARCHIVE_MAX_ENTRIES+3||
 q->journal_capacity<q->selected_count+3||!aligned(q->selected,_Alignof(uint32_t))||
 !aligned(q->journal,_Alignof(mb_archive_owned_file_v1))||!view_valid(q)||!materialize_spans(q,l,b,out))
  return MB_ARCHIVE_BAD_ARGUMENT;
 /* A failed or completed earlier operation still owns its journal. Never
  * overwrite those identities merely because a caller reuses the buffer. */
 if(!empty_bytes(q->journal,(size_t)q->journal_capacity*sizeof(*q->journal))){
  memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;
  out->status=MB_ARCHIVE_BAD_ARGUMENT;return MB_ARCHIVE_BAD_ARGUMENT;
 }
 memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;out->cleanup_complete=1;
 memset(q->journal,0,(size_t)q->journal_capacity*sizeof(*q->journal));clear_buffers(&q->replay);
 memset(&r,0,sizeof(r));r.s=q->source;r.l=l;r.b=b;r.v=&q->replay;r.q=q;r.out=out;
 if(memcmp(q->target_token,q->source->target_token,32)||memcmp(q->target_token,q->inventory->target_token,32)||
 q->inventory->source_size!=q->source->size||memcmp(q->inventory->source_sha256,q->source->sha256,32)){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto fail;}
 if((e=inventory_digest(q->source,q->rows,q->names,q->inventory->row_count,q->inventory->name_bytes,hash)))goto fail;
 if(memcmp(hash,q->inventory->inventory_sha256,32)){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto fail;}
 for(i=0;i<q->selected_count;i++){
  if(q->selected[i]>=q->inventory->row_count||q->rows[q->selected[i]].kind!=MB_ARCHIVE_REGULAR){e=MB_ARCHIVE_BAD_ARGUMENT;goto fail;}
  for(j=0;j<i;j++){if((e=work(&r,1)))goto fail;if(q->selected[j]==q->selected[i]){e=MB_ARCHIVE_BAD_ARGUMENT;goto fail;}}
 }
 if((e=source_start(&r)))goto fail;
 if((uint64_t)r.stamp.st_dev!=q->inventory->source_device||(uint64_t)r.stamp.st_ino!=q->inventory->source_inode){e=MB_ARCHIVE_SOURCE_CHANGED;goto fail;}
 if((e=source_end(&r))||(e=private_empty(&r)))goto fail;
 cleanup_ops=(uint64_t)(q->selected_count+3)*2+1;
 if(cleanup_ops>b->operation_limit-b->operations_used){e=MB_ARCHIVE_LIMIT;goto fail;}
 b->operations_used+=cleanup_ops;out->journal_count=q->selected_count+3;
 intent=q->journal;temporary=q->journal+q->selected_count+1;final=temporary+1;
 strcpy(intent->name,"intent-v1.bin");strcpy(temporary->name,"manifest-v1.part");strcpy(final->name,"manifest-v1.bin");
 for(i=0;i<out->journal_count;i++){q->journal[i].state=MB_ARCHIVE_PLANNED;q->journal[i].row_index=UINT32_MAX;}
 for(i=0;i<q->selected_count;i++){mb_archive_owned_file_v1 *p=q->journal+i+1;const mb_archive_row_v1 *row=q->rows+q->selected[i];
  (void)snprintf(p->name,sizeof(p->name),"member-%08"PRIu32".bin",q->selected[i]);
  p->row_index=q->selected[i];p->expected_size=row->size;memcpy(p->sha256,row->sha256,32);
 }
 if((e=create_owned(&r,intent,&fd))||(e=emit_manifest(&r,fd,intent,1))||
 (e=finish_owned(&r,&fd))||(e=readback(&r,intent)))goto fail;
 if((e=charge(&r,0,1)))goto fail;if(fsync(q->quarantine_fd)){e=MB_ARCHIVE_IO_ERROR;goto fail;}
 inventory_failure(&replay,0);
 if((e=walk(&r,&replay))||(e=source_end(&r)))goto fail;
 if(r.count!=q->inventory->row_count||r.names!=q->inventory->name_bytes||r.expanded!=q->inventory->expanded_bytes){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto fail;}
 if((e=inventory_digest(q->source,r.v->rows,r.v->names,r.count,r.names,hash)))goto fail;
 if(memcmp(hash,q->inventory->inventory_sha256,32)){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto fail;}
 for(i=0;i<q->selected_count;i++)if((e=readback(&r,q->journal+i+1)))goto fail;
 if((e=create_owned(&r,temporary,&fd))||(e=emit_manifest(&r,fd,temporary,0))||
 (e=finish_owned(&r,&fd))||(e=readback(&r,temporary)))goto fail;
 temporary->expected_size=temporary->written_bytes;
 if((e=charge(&r,0,1)))goto fail;
 if(mb_archive_posix_linkat(q->quarantine_fd,temporary->name,q->quarantine_fd,final->name,0)){
  if(errno!=EOPNOTSUPP&&errno!=ENOTSUP){e=MB_ARCHIVE_IO_ERROR;goto fail;}
  /* Some native filesystems cannot hard-link. Exclusive re-emission is
   * visible before completion; only the verified owning result publishes
   * success. Keep the final file's independently observed inode. */
  if((e=create_owned(&r,final,&fd))||(e=emit_manifest(&r,fd,final,0))||
  (e=finish_owned(&r,&fd))||(e=readback(&r,final)))goto fail;
  if(final->expected_size!=temporary->expected_size||final->written_bytes!=temporary->written_bytes||
  memcmp(final->sha256,temporary->sha256,32)){e=MB_ARCHIVE_IDENTITY_MISMATCH;goto fail;}
 } else {
  final->device=temporary->device;final->inode=temporary->inode;final->state=MB_ARCHIVE_CREATED;
  final->expected_size=temporary->expected_size;final->written_bytes=temporary->written_bytes;
  memcpy(final->sha256,temporary->sha256,32);
 }
 if((e=charge(&r,0,1)))goto fail;if(mb_archive_posix_unlinkat(q->quarantine_fd,temporary->name,0)){e=MB_ARCHIVE_IO_ERROR;goto fail;}
 temporary->state=MB_ARCHIVE_REMOVED;
 if((e=charge(&r,0,1)))goto fail;if(fsync(q->quarantine_fd)){e=MB_ARCHIVE_IO_ERROR;goto fail;}
 if((e=check_cancel(&r)))goto fail;
 out->selected_count=q->selected_count;
 for(i=0;i<q->selected_count;i++)out->decoded_bytes+=q->rows[q->selected[i]].size;
 memcpy(out->inventory_sha256,q->inventory->inventory_sha256,32);memcpy(out->target_token,q->target_token,32);
 strcpy(out->manifest_name,final->name);out->complete=1;clear_buffers(&q->replay);return 0;
fail:
 if(fd>=0){if(b->operations_used<b->operation_limit)++b->operations_used;close(fd);}
 out->cleanup_complete=out->journal_count?(uint32_t)cleanup(&r):1;
 if(!out->cleanup_complete)e=MB_ARCHIVE_CLEANUP_INCOMPLETE;
 out->status=(uint32_t)e;out->complete=0;out->selected_count=0;out->decoded_bytes=0;
 memset(out->inventory_sha256,0,32);memset(out->target_token,0,32);out->manifest_name[0]=0;
 clear_buffers(&q->replay);return e;
}
