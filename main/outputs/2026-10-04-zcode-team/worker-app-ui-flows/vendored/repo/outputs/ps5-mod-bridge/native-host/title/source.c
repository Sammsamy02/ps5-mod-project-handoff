#if defined(__APPLE__)
/* Darwin's POSIX-only visibility hides O_NOFOLLOW and changes time field names.
 * This host-only feature selection never changes SDK constants or headers. */
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "source.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef MB_TITLE_SOURCE_HOST_TESTING
#include "source_test.h"
#endif

/* OS constants/prototypes come only from the selected platform headers. */
#if !defined(O_NOFOLLOW) || !defined(O_DIRECTORY) || !defined(O_CLOEXEC) || !defined(AT_SYMLINK_NOFOLLOW)
#error "This source profile needs ordinary no-follow directory/relative APIs"
#endif

#define SOURCE_ROOT_COMPONENTS 16u
#define SOURCE_DIRECTORIES (SOURCE_ROOT_COMPONENTS+2u)
typedef struct source_ops {
    void *context;
    int (*lstat_path)(void *,const char *,struct stat *);
    int (*open_path)(void *,const char *,int);
    int (*stat_relative)(void *,int,const char *,struct stat *,int);
    int (*open_relative)(void *,int,const char *,int);
    int (*stat_fd)(void *,int,struct stat *);
    ssize_t (*read_fd)(void *,int,void *,size_t);
    int (*close_fd)(void *,int);
} source_ops;
typedef struct source_span {uintptr_t begin,end;} source_span;
typedef struct source_directory {int fd; const char *name; struct stat identity;} source_directory;
typedef struct source_capture {
    mb_title_source_budget_v1 *budget;
    mb_title_source_result_v1 result;
    const source_ops *ops;
    source_directory directories[SOURCE_DIRECTORIES];
    size_t directory_count;
    int file_fd;
} source_capture;

static int os_lstat(void *v,const char *p,struct stat *s) {(void)v; return lstat(p,s);}
static int os_open(void *v,const char *p,int f) {(void)v; return open(p,f);}
static int os_atstat(void *v,int d,const char *p,struct stat *s,int f) {(void)v; return fstatat(d,p,s,f);}
static int os_atopen(void *v,int d,const char *p,int f) {(void)v; return openat(d,p,f);}
static int os_fstat(void *v,int d,struct stat *s) {(void)v; return fstat(d,s);}
static ssize_t os_read(void *v,int d,void *p,size_t n) {(void)v; return read(d,p,n);}
static int os_close(void *v,int d) {(void)v; return close(d);}
static const source_ops ordinary_ops={NULL,os_lstat,os_open,os_atstat,os_atopen,os_fstat,os_read,os_close};

static int span(const void *p,size_t size,size_t alignment,source_span *out) {
    const uintptr_t address=(uintptr_t)p;
    if(!p || !size || address%alignment || size>UINTPTR_MAX-address) return 0;
    out->begin=address; out->end=address+size; return 1;
}
static int disjoint(const source_span *spans,size_t count) {
    size_t i,j;
    for(i=0;i<count;++i) for(j=0;j<i;++j)
        if(spans[i].begin<spans[j].end && spans[j].begin<spans[i].end) return 0;
    return 1;
}
static int ascii_alnum(unsigned char c) {
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9');
}
static int candidate_valid(const char *p,size_t n) {
    size_t i;
    if(!n || n>64 || !ascii_alnum((unsigned char)p[0])) return 0;
    for(i=1;i<n;++i) {
        const unsigned char c=(unsigned char)p[i];
        if(!ascii_alnum(c) && c!='.' && c!='_' && c!='-') return 0;
    }
    return 1;
}
static int preflight(const mb_title_source_request_v1 *request,
    const mb_title_source_budget_v1 *budget,uint8_t *workspace,size_t capacity,
    mb_title_source_result_v1 *result,const char *fixture_root,size_t fixture_bytes,
    const void *fixture_ops,size_t fixture_ops_size,size_t fixture_ops_alignment) {
    source_span ranges[8]; size_t count=5;
    if(capacity!=MB_TITLE_SOURCE_WORKSPACE_BYTES ||
        !span(request,sizeof(*request),_Alignof(mb_title_source_request_v1),&ranges[0]) ||
        !span(budget,sizeof(*budget),_Alignof(mb_title_source_budget_v1),&ranges[1]) ||
        !span(workspace,capacity,1,&ranges[2]) ||
        !span(result,sizeof(*result),_Alignof(mb_title_source_result_v1),&ranges[3])) return 0;
    if(request->struct_size!=sizeof(*request) || request->abi_version!=MB_TITLE_SOURCE_ABI_VERSION ||
        (request->profile!=MB_TITLE_CACHE_SYSTEM_PRIVATE && request->profile!=MB_TITLE_CACHE_USER) ||
        request->reserved[0] || request->reserved[1] || !request->candidate_bytes || request->candidate_bytes>64 ||
        !span(request->candidate,request->candidate_bytes,1,&ranges[4])) return 0;
    if(budget->struct_size!=sizeof(*budget) || budget->abi_version!=MB_TITLE_SOURCE_ABI_VERSION ||
        budget->reserved[0] || budget->reserved[1] ||
        budget->reserved_operations>UINT64_MAX-budget->remaining_operations ||
        budget->reserved_read_bytes>UINT64_MAX-budget->remaining_read_bytes) return 0;
    if(fixture_root) {
        if(!fixture_bytes || fixture_bytes>512 ||
            !span(fixture_root,fixture_bytes,1,&ranges[count++]) ||
            !span(fixture_ops,fixture_ops_size,fixture_ops_alignment,&ranges[count++])) return 0;
    }
    return disjoint(ranges,count) && candidate_valid(request->candidate,request->candidate_bytes);
}
static void clear_workspace(uint8_t *workspace,size_t n) {
    volatile uint8_t *p=workspace;
    while(n--) *p++=0;
}
static int fail(source_capture *s,mb_title_source_status status,mb_title_source_reason reason,int error) {
    s->result.status=(uint32_t)status; s->result.reason=(uint32_t)reason;
    s->result.last_errno=error>0?(uint32_t)error:0; return 0;
}
static int reserve(source_capture *s,uint32_t operations,size_t read_bytes) {
    if(operations>MB_TITLE_SOURCE_OPERATION_LIMIT-s->result.operations_reserved ||
        operations>s->budget->remaining_operations)
        return fail(s,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_OPERATION_BUDGET,0);
    if(read_bytes>MB_TITLE_SOURCE_REQUESTED_BYTE_LIMIT-s->result.requested_read_bytes ||
        read_bytes>s->budget->remaining_read_bytes)
        return fail(s,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_READ_BUDGET,0);
    s->budget->remaining_operations-=operations; s->budget->reserved_operations+=operations;
    s->budget->remaining_read_bytes-=read_bytes; s->budget->reserved_read_bytes+=read_bytes;
    s->result.operations_reserved+=operations; s->result.requested_read_bytes+=read_bytes;
    return 1;
}
static int stat_path(source_capture *s,struct stat *st) {
    if(!reserve(s,1,0))return 0;
    ++s->result.operations_attempted; errno=0;
    if(s->ops->lstat_path(s->ops->context,"/",st))
        return fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_STAT_FAILED,errno);
    return 1;
}
static int stat_relative(source_capture *s,int parent,const char *name,struct stat *st) {
    if(!reserve(s,1,0))return 0;
    ++s->result.operations_attempted; errno=0;
    if(s->ops->stat_relative(s->ops->context,parent,name,st,AT_SYMLINK_NOFOLLOW))
        return fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_STAT_FAILED,errno);
    return 1;
}
static int stat_fd(source_capture *s,int fd,struct stat *st) {
    if(!reserve(s,1,0))return 0;
    ++s->result.operations_attempted; errno=0;
    if(s->ops->stat_fd(s->ops->context,fd,st))
        return fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_STAT_FAILED,errno);
    return 1;
}
static int open_path(source_capture *s) {
    int fd;
    if(!reserve(s,2,0))return -1; /* one open + one escrowed close */
    ++s->result.operations_attempted; errno=0;
    fd=s->ops->open_path(s->ops->context,"/",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(fd<0)fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_OPEN_FAILED,errno);
    return fd;
}
static int open_relative(source_capture *s,int parent,const char *name,int directory) {
    int fd;
    if(!reserve(s,2,0))return -1;
    ++s->result.operations_attempted; errno=0;
    fd=s->ops->open_relative(s->ops->context,parent,name,
        O_RDONLY|O_NOFOLLOW|O_CLOEXEC|(directory?O_DIRECTORY:O_NONBLOCK));
    if(fd<0)fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_OPEN_FAILED,errno);
    return fd;
}
static int directory_type(source_capture *s,const struct stat *st) {
    if(S_ISLNK(st->st_mode)) return fail(s,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_SYMLINK,0);
    if(!S_ISDIR(st->st_mode))return fail(s,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_NOT_DIRECTORY,0);
    return 1;
}
static int regular_type(source_capture *s,const struct stat *st) {
    if(S_ISLNK(st->st_mode)) return fail(s,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_SYMLINK,0);
    if(!S_ISREG(st->st_mode))return fail(s,MB_TITLE_SOURCE_UNSAFE_TYPE,MB_TITLE_SOURCE_NOT_REGULAR,0);
    if(st->st_size<0 || (uint64_t)st->st_size>MB_TITLE_SOURCE_WORKSPACE_BYTES)
        return fail(s,MB_TITLE_SOURCE_RESOURCE_LIMIT,MB_TITLE_SOURCE_FILE_TOO_LARGE,0);
    return 1;
}
static int same_node(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_mode==b->st_mode;
}
static int same_file(const struct stat *a,const struct stat *b) {
#if defined(__APPLE__)
    const struct timespec am=a->st_mtimespec,bm=b->st_mtimespec,ac=a->st_ctimespec,bc=b->st_ctimespec;
#else
    const struct timespec am=a->st_mtim,bm=b->st_mtim,ac=a->st_ctim,bc=b->st_ctim;
#endif
    return same_node(a,b) && a->st_size==b->st_size && a->st_nlink==b->st_nlink &&
        a->st_uid==b->st_uid && a->st_gid==b->st_gid &&
        am.tv_sec==bm.tv_sec && am.tv_nsec==bm.tv_nsec && ac.tv_sec==bc.tv_sec && ac.tv_nsec==bc.tv_nsec;
}
static int unchanged(source_capture *s,int same) {
    return same?1:fail(s,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_IDENTITY_CHANGED,0);
}
static int read_exact(source_capture *s,uint8_t *workspace,size_t total) {
    size_t filled=0;
    while(filled<total) {
        const size_t request=total-filled>MB_TITLE_SOURCE_READ_REQUEST_LIMIT?MB_TITLE_SOURCE_READ_REQUEST_LIMIT:total-filled;
        ssize_t actual;
        if(!reserve(s,1,request))return 0;
        ++s->result.operations_attempted; errno=0;
        actual=s->ops->read_fd(s->ops->context,s->file_fd,workspace+filled,request);
        if(actual<0)return fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_READ_FAILED,errno);
        if((uint64_t)actual>request)return fail(s,MB_TITLE_SOURCE_INTERNAL_ERROR,MB_TITLE_SOURCE_INTERNAL_FAILURE,0);
        s->result.actual_read_bytes+=(uint64_t)actual;
        if((size_t)actual!=request)return fail(s,MB_TITLE_SOURCE_SHORT_READ,MB_TITLE_SOURCE_INCOMPLETE_READ,0);
        filled+=request;
    }
    {
        uint8_t extra=0; ssize_t actual;
        if(!reserve(s,1,1))return 0;
        ++s->result.operations_attempted; errno=0;
        actual=s->ops->read_fd(s->ops->context,s->file_fd,&extra,1);
        if(actual<0)return fail(s,MB_TITLE_SOURCE_UNAVAILABLE,MB_TITLE_SOURCE_READ_FAILED,errno);
        if(actual>1)return fail(s,MB_TITLE_SOURCE_INTERNAL_ERROR,MB_TITLE_SOURCE_INTERNAL_FAILURE,0);
        s->result.actual_read_bytes+=(uint64_t)actual;
        if(actual)return fail(s,MB_TITLE_SOURCE_CHANGED,MB_TITLE_SOURCE_GROWTH_OBSERVED,0);
    }
    return 1;
}
static void close_owned(source_capture *s,int fd) {
    /* Its one operation token was escrowed by the corresponding open. */
    ++s->result.operations_attempted; errno=0;
    if(s->ops->close_fd(s->ops->context,fd)) {
        const int error=errno;
        s->result.cleanup_complete=0;
        if(s->result.status==MB_TITLE_SOURCE_OK)
            fail(s,MB_TITLE_SOURCE_CLEANUP_FAILED,MB_TITLE_SOURCE_CLOSE_FAILED,error);
        else if(!s->result.last_errno) s->result.last_errno=error>0?(uint32_t)error:0;
    }
}
static int no_authority(const mb_title_metadata_result_v1 *m) {
    return !m->installed_observed && !m->running_observed && !m->build_fingerprint_available &&
        !m->engine_acceptance_verified && !m->runtime_supported && !m->installation_supported &&
        !m->authenticity_verified && !m->game_acceptance_verified && !m->reserved[0] && !m->reserved[1];
}
static mb_title_source_status capture(const mb_title_source_request_v1 *request,
    mb_title_source_budget_v1 *budget,uint8_t *workspace,mb_title_source_result_v1 *out,
    const char *const *components,size_t component_count,const source_ops *ops,uint32_t scope) {
    source_capture s; struct stat before,opened,after; size_t i,total;
    char candidate[65]; mb_title_metadata_input_v1 input; mb_title_metadata_result_v1 metadata;
    int fd;
    memset(&s,0,sizeof(s)); s.file_fd=-1; s.budget=budget; s.ops=ops;
    s.result.struct_size=sizeof(s.result); s.result.abi_version=MB_TITLE_SOURCE_ABI_VERSION;
    s.result.requested_profile=request->profile; s.result.source_scope=scope; s.result.cleanup_complete=1;
    memcpy(candidate,request->candidate,request->candidate_bytes); candidate[request->candidate_bytes]=0;

    if(!stat_path(&s,&before) || !directory_type(&s,&before))goto done;
    fd=open_path(&s); if(fd<0)goto done;
    s.directories[0].fd=fd; s.directories[0].name="/"; s.directory_count=1;
    if(!stat_fd(&s,fd,&opened) || !directory_type(&s,&opened) || !unchanged(&s,same_node(&before,&opened)))goto done;
    s.directories[0].identity=opened;
    for(i=0;i<=component_count;++i) {
        const char *name=i==component_count?candidate:components[i];
        const int parent=s.directories[s.directory_count-1].fd;
        if(!stat_relative(&s,parent,name,&before) || !directory_type(&s,&before))goto done;
        fd=open_relative(&s,parent,name,1); if(fd<0)goto done;
        s.directories[s.directory_count].fd=fd; s.directories[s.directory_count].name=name;
        ++s.directory_count;
        if(!stat_fd(&s,fd,&opened) || !directory_type(&s,&opened) || !unchanged(&s,same_node(&before,&opened)))goto done;
        s.directories[s.directory_count-1].identity=opened;
    }
    fd=s.directories[s.directory_count-1].fd;
    if(!stat_relative(&s,fd,"param.json",&before) || !regular_type(&s,&before))goto done;
    s.file_fd=open_relative(&s,fd,"param.json",0); if(s.file_fd<0)goto done;
    if(!stat_fd(&s,s.file_fd,&opened) || !regular_type(&s,&opened) || !unchanged(&s,same_file(&before,&opened)))goto done;
    total=(size_t)opened.st_size;
    if(!read_exact(&s,workspace,total))goto done;
    if(!stat_fd(&s,s.file_fd,&after) || !unchanged(&s,same_file(&opened,&after)))goto done;
    if(!stat_relative(&s,fd,"param.json",&after) || !unchanged(&s,same_file(&opened,&after)))goto done;
    /* Verify every name still binds the observed node under its pinned parent.
     * Directory mtime/size do not identify this one file and are not compared. */
    for(i=s.directory_count-1;i>0;--i) {
        if(!stat_relative(&s,s.directories[i-1].fd,s.directories[i].name,&after) ||
            !unchanged(&s,same_node(&s.directories[i].identity,&after)))goto done;
    }
    if(!stat_path(&s,&after) || !unchanged(&s,same_node(&s.directories[0].identity,&after)))goto done;
    memset(&input,0,sizeof(input)); input.struct_size=sizeof(input); input.abi_version=MB_TITLE_METADATA_ABI_VERSION;
    input.bytes=workspace; input.available=total; input.profile=MB_TITLE_PARAM_JSON_ROOT_V1;
    memset(&metadata,0,sizeof(metadata));
    s.result.metadata_status=mb_title_metadata_decode_v1(&input,&metadata); s.result.metadata_reason=metadata.reason;
    if(s.result.metadata_status!=MB_TITLE_METADATA_OK) {fail(&s,MB_TITLE_SOURCE_METADATA_REJECTED,MB_TITLE_SOURCE_DECODE_FAILED,0); goto done;}
    if(!no_authority(&metadata)) {fail(&s,MB_TITLE_SOURCE_INTERNAL_ERROR,MB_TITLE_SOURCE_INTERNAL_FAILURE,0); goto done;}
    if(strlen(metadata.title_id)!=request->candidate_bytes || memcmp(candidate,metadata.title_id,request->candidate_bytes)) {
        fail(&s,MB_TITLE_SOURCE_ID_MISMATCH,MB_TITLE_SOURCE_CANDIDATE_MISMATCH,0); goto done;
    }
    s.result.metadata=metadata; s.result.source_complete=1; s.result.identity_checked=1; s.result.candidate_matches=1;
done:
    if(s.file_fd>=0)close_owned(&s,s.file_fd);
    while(s.directory_count)close_owned(&s,s.directories[--s.directory_count].fd);
    clear_workspace(workspace,MB_TITLE_SOURCE_WORKSPACE_BYTES);
    if(s.result.status!=MB_TITLE_SOURCE_OK) {
        memset(&s.result.metadata,0,sizeof(s.result.metadata));
        s.result.source_complete=0; s.result.identity_checked=0; s.result.candidate_matches=0;
    }
    *out=s.result;
    return (mb_title_source_status)s.result.status;
}

mb_title_source_status mb_title_source_capture_v1(const mb_title_source_request_v1 *request,
    mb_title_source_budget_v1 *budget,uint8_t *workspace,size_t workspace_bytes,mb_title_source_result_v1 *out) {
    static const char *const system_components[]={"system_data","priv","appmeta"};
    static const char *const user_components[]={"user","appmeta"};
    if(!preflight(request,budget,workspace,workspace_bytes,out,NULL,0,NULL,0,1))return MB_TITLE_SOURCE_BAD_ARGUMENT;
    return capture(request,budget,workspace,out,
        request->profile==MB_TITLE_CACHE_SYSTEM_PRIVATE?system_components:user_components,
        request->profile==MB_TITLE_CACHE_SYSTEM_PRIVATE?3:2,&ordinary_ops,MB_TITLE_SOURCE_METADATA_CACHE_SCOPE);
}

#ifdef MB_TITLE_SOURCE_HOST_TESTING
mb_title_source_status mb_title_source_capture_fixture_v1(const mb_title_source_request_v1 *request,
    mb_title_source_budget_v1 *budget,uint8_t *workspace,size_t workspace_bytes,mb_title_source_result_v1 *out,
    const char *root,size_t root_bytes,const mb_title_source_test_ops_v1 *test_ops) {
    char copied[513]; const char *components[SOURCE_ROOT_COMPONENTS]; size_t i,start,count=0;
    source_ops ops;
    if(!root || !preflight(request,budget,workspace,workspace_bytes,out,root,root_bytes,
        test_ops,sizeof(*test_ops),_Alignof(mb_title_source_test_ops_v1)))return MB_TITLE_SOURCE_BAD_ARGUMENT;
    if(test_ops->struct_size!=sizeof(*test_ops) || test_ops->abi_version!=1 || !test_ops->lstat_path ||
        !test_ops->open_path || !test_ops->stat_relative || !test_ops->open_relative ||
        !test_ops->stat_fd || !test_ops->read_fd || !test_ops->close_fd || root[0]!='/')return MB_TITLE_SOURCE_BAD_ARGUMENT;
    memcpy(copied,root,root_bytes); copied[root_bytes]=0; start=1;
    for(i=1;i<=root_bytes;++i) {
        if(i<root_bytes && !copied[i])return MB_TITLE_SOURCE_BAD_ARGUMENT;
        if(i==root_bytes || copied[i]=='/') {
            const size_t length=i-start;
            if(!length || length>255 || count==SOURCE_ROOT_COMPONENTS ||
                (length==1 && copied[start]=='.') ||
                (length==2 && copied[start]=='.' && copied[start+1]=='.'))return MB_TITLE_SOURCE_BAD_ARGUMENT;
            components[count++]=copied+start; copied[i]=0; start=i+1;
        }
    }
    ops.context=test_ops->context; ops.lstat_path=test_ops->lstat_path; ops.open_path=test_ops->open_path;
    ops.stat_relative=test_ops->stat_relative; ops.open_relative=test_ops->open_relative;
    ops.stat_fd=test_ops->stat_fd; ops.read_fd=test_ops->read_fd; ops.close_fd=test_ops->close_fd;
    return capture(request,budget,workspace,out,components,count,&ops,MB_TITLE_SOURCE_HOST_FIXTURE_SCOPE);
}
#endif
