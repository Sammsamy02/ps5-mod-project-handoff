#if defined(__APPLE__)
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "catalog.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef MB_TITLE_CATALOG_HOST_TESTING
#include "catalog_test.h"
#endif

#if !defined(O_NOFOLLOW) || !defined(O_DIRECTORY) || !defined(O_CLOEXEC) || !defined(AT_SYMLINK_NOFOLLOW)
#error "The catalog requires ordinary no-follow directory APIs from selected platform headers"
#endif

typedef struct catalog_span { uintptr_t begin,end; } catalog_span;
typedef struct catalog_ops {
    void *context;
    int (*lstat_path)(void *,const char *,struct stat *);
    int (*open_path)(void *,const char *,int);
    int (*stat_relative)(void *,int,const char *,struct stat *,int);
    int (*open_relative)(void *,int,const char *,int);
    int (*stat_fd)(void *,int,struct stat *);
    int (*close_fd)(void *,int);
    DIR *(*fdopendir_stream)(void *,int);
    struct dirent *(*readdir_stream)(void *,DIR *);
    int (*closedir_stream)(void *,DIR *);
} catalog_ops;
typedef struct catalog_directory {
    int fd;
    const char *name;
    struct stat identity;
} catalog_directory;
typedef struct catalog_state {
    mb_title_catalog_ledger_v1 *ledger;
    mb_title_catalog_result_v1 *result;
    const catalog_ops *ops;
    catalog_directory directories[18];
    size_t directory_count;
    DIR *stream;
    uint64_t initial_reserved_operations,initial_reserved_read_bytes;
    uint64_t attempted,actual_read_bytes;
    int fatal;
    int fatal_status;
    uint32_t fatal_reason;
} catalog_state;

static int os_lstat(void *v,const char *p,struct stat *s){(void)v;return lstat(p,s);}
static int os_open(void *v,const char *p,int f){(void)v;return open(p,f);}
static int os_atstat(void *v,int d,const char *p,struct stat *s,int f){(void)v;return fstatat(d,p,s,f);}
static int os_atopen(void *v,int d,const char *p,int f){(void)v;return openat(d,p,f);}
static int os_fstat(void *v,int d,struct stat *s){(void)v;return fstat(d,s);}
static int os_close(void *v,int d){(void)v;return close(d);}
static DIR *os_fdopendir(void *v,int d){(void)v;return fdopendir(d);}
static struct dirent *os_readdir(void *v,DIR *d){(void)v;return readdir(d);}
static int os_closedir(void *v,DIR *d){(void)v;return closedir(d);}
static const catalog_ops ordinary_catalog_ops={NULL,os_lstat,os_open,os_atstat,os_atopen,os_fstat,os_close,
    os_fdopendir,os_readdir,os_closedir};

static int span(const void *p,size_t n,size_t alignment,catalog_span *out){
    const uintptr_t address=(uintptr_t)p;
    if(!p||!n||!alignment||address%alignment||n>UINTPTR_MAX-address)return 0;
    out->begin=address;out->end=address+n;return 1;
}
static int disjoint(const catalog_span *spans,size_t count){
    size_t i,j;for(i=0;i<count;++i)for(j=0;j<i;++j)
        if(spans[i].begin<spans[j].end&&spans[j].begin<spans[i].end)return 0;
    return 1;
}
static int zero_words(const uint32_t *v,size_t n){size_t i;for(i=0;i<n;++i)if(v[i])return 0;return 1;}
static int valid_ledger(const mb_title_catalog_ledger_v1 *l){
    uint64_t ops_total,bytes_total;
    if(l->struct_size!=sizeof(*l)||l->abi_version!=MB_TITLE_CATALOG_ABI_VERSION||
       !zero_words(l->reserved,2)||l->entries_seen||l->candidates_seen||l->entry_name_bytes)return 0;
    if(l->source_io.struct_size!=sizeof(l->source_io)||l->source_io.abi_version!=MB_TITLE_SOURCE_ABI_VERSION||
       !zero_words(l->source_io.reserved,2))return 0;
    if(l->source_io.remaining_operations>UINT64_MAX-l->source_io.reserved_operations)return 0;
    ops_total=l->source_io.remaining_operations+l->source_io.reserved_operations;
    if(ops_total!=MB_TITLE_CATALOG_OPERATION_LIMIT)return 0;
    if(l->source_io.remaining_read_bytes>UINT64_MAX-l->source_io.reserved_read_bytes)return 0;
    bytes_total=l->source_io.remaining_read_bytes+l->source_io.reserved_read_bytes;
    return bytes_total==MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT;
}
static int valid_ops(const catalog_ops *ops){
    return ops&&ops->lstat_path&&ops->open_path&&ops->stat_relative&&ops->open_relative&&
        ops->stat_fd&&ops->close_fd&&ops->fdopendir_stream&&ops->readdir_stream&&ops->closedir_stream;
}
static int valid_path(const char *path,size_t bytes,char copied[513],size_t *components){
    size_t i,start,count=0;
    if(!bytes||bytes>512||path[0]!='/'||path[bytes-1]=='/')return 0;
    memcpy(copied,path,bytes);copied[bytes]=0;start=1;
    for(i=1;i<=bytes;++i){
        if(i<bytes&&copied[i]=='\0')return 0;
        if(i==bytes||copied[i]=='/'){
            const size_t n=i-start;
            if(!n||n>255||count>=16||(n==1&&copied[start]=='.')||
               (n==2&&copied[start]=='.'&&copied[start+1]=='.'))return 0;
            ++count;copied[i]=0;start=i+1;
        }
    }
    *components=count;return count>0;
}
static int valid_catalog_request(const mb_title_catalog_request_v1 *request){
    return request->struct_size==sizeof(*request)&&request->abi_version==MB_TITLE_CATALOG_ABI_VERSION&&
        (request->profile==MB_TITLE_CACHE_SYSTEM_PRIVATE||request->profile==MB_TITLE_CACHE_USER)&&
        request->scan_assertion==MB_TITLE_CATALOG_ASSERT_IMMUTABLE_SCAN&&zero_words(request->reserved,2);
}
static int preflight(const mb_title_catalog_request_v1 *request,mb_title_catalog_ledger_v1 *ledger,
    char *name_scratch,size_t name_bytes,char *candidate,size_t candidate_bytes,
    uint8_t *workspace,size_t workspace_bytes,mb_title_metadata_result_v1 *records,uint32_t record_capacity,
    uint32_t *issues,uint32_t issue_capacity,mb_title_catalog_result_v1 *result,
    const char *root,size_t root_bytes,const catalog_ops *ops,const void *source_ops,size_t source_ops_bytes,
    size_t source_ops_alignment,const void *test_catalog_ops,size_t test_catalog_ops_bytes,
    size_t test_catalog_ops_alignment,int fixture){
    catalog_span spans[13];size_t n=0;char path_copy[513];size_t component_count;
    if(name_bytes!=MB_TITLE_CATALOG_ENTRY_NAME_BYTES||candidate_bytes!=MB_TITLE_CATALOG_CANDIDATE_BYTES||
       workspace_bytes!=MB_TITLE_CATALOG_SOURCE_WORKSPACE_BYTES||record_capacity!=MB_TITLE_CATALOG_MAX_RECORDS||
       issue_capacity!=MB_TITLE_CATALOG_MAX_ISSUES)return 0;
    if(!span(request,sizeof(*request),_Alignof(mb_title_catalog_request_v1),&spans[n++])||
       !span(ledger,sizeof(*ledger),_Alignof(mb_title_catalog_ledger_v1),&spans[n++])||
       !span(name_scratch,name_bytes,1,&spans[n++])||!span(candidate,candidate_bytes,1,&spans[n++])||
       !span(workspace,workspace_bytes,1,&spans[n++])||
       !span(records,sizeof(*records)*record_capacity,_Alignof(mb_title_metadata_result_v1),&spans[n++])||
       !span(issues,sizeof(*issues)*issue_capacity,_Alignof(uint32_t),&spans[n++])||
       !span(result,sizeof(*result),_Alignof(mb_title_catalog_result_v1),&spans[n++]))return 0;
    if(!valid_catalog_request(request)||!valid_ledger(ledger))return 0;
    if(fixture){
        if(!span(root,root_bytes,1,&spans[n++])||!valid_path(root,root_bytes,path_copy,&component_count))return 0;
#ifdef MB_TITLE_CATALOG_HOST_TESTING
        if(source_ops&&(!source_ops_bytes||!span(source_ops,source_ops_bytes,source_ops_alignment,&spans[n++])))return 0;
        if(test_catalog_ops&&(!test_catalog_ops_bytes||
           !span(test_catalog_ops,test_catalog_ops_bytes,test_catalog_ops_alignment,&spans[n++])))return 0;
#else
        (void)source_ops;(void)source_ops_bytes;(void)source_ops_alignment;
        return 0;
#endif
    }
    if(ops&&!valid_ops(ops))return 0;
    if(!disjoint(spans,n))return 0;
    return 1;
}
static void clear_bytes(void *v,size_t n){volatile unsigned char *p=(volatile unsigned char *)v;while(n--)*p++=0;}
static void result_init(mb_title_catalog_result_v1 *r,const mb_title_catalog_request_v1 *request){
    memset(r,0,sizeof(*r));r->struct_size=sizeof(*r);r->abi_version=MB_TITLE_CATALOG_ABI_VERSION;
    r->status=MB_TITLE_CATALOG_STATUS_ERROR;r->reason=MB_TITLE_CATALOG_REASON_PROVIDER_ERROR;r->profile=request->profile;
    r->source_scope=request->profile==MB_TITLE_CACHE_SYSTEM_PRIVATE?
        MB_TITLE_CATALOG_SOURCE_SCOPE_SYSTEM_PRIVATE_METADATA_CACHE:MB_TITLE_CATALOG_SOURCE_SCOPE_USER_METADATA_CACHE;
    r->cleanup_complete=1;
}
static int reserve_ops(catalog_state *s,uint32_t amount){
    mb_title_source_budget_v1 *b=&s->ledger->source_io;
    if(amount>MB_TITLE_CATALOG_OPERATION_LIMIT||b->remaining_operations<amount||
       b->reserved_operations>MB_TITLE_CATALOG_OPERATION_LIMIT-amount){
        s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;
        s->fatal_reason=MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT;return 0;
    }
    b->remaining_operations-=amount;b->reserved_operations+=amount;return 1;
}
static int reserve_name_bytes(catalog_state *s,size_t amount){
    mb_title_source_budget_v1 *b=&s->ledger->source_io;
    if(amount>MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT||b->remaining_read_bytes<amount||
       s->ledger->entry_name_bytes>MB_TITLE_CATALOG_CHARGED_BYTE_LIMIT-amount){
        s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;
        s->fatal_reason=MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT;return 0;
    }
    b->remaining_read_bytes-=amount;s->ledger->entry_name_bytes+=amount;return 1;
}
static int issue_add(catalog_state *s,uint32_t issue,uint32_t *issues){
    uint32_t i;for(i=0;i<s->result->issue_count;++i)if(issues[i]==issue)return 1;
    if(s->result->issue_count>=MB_TITLE_CATALOG_MAX_ISSUES){
        s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;
        s->fatal_reason=MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT;return 0;
    }
    issues[s->result->issue_count++]=issue;return 1;
}
static void fatal(catalog_state *s,uint32_t reason,uint32_t issue,uint32_t *issues){
    if(!s->fatal){s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;s->fatal_reason=reason;}
    (void)issue_add(s,issue,issues);
}
static void unavailable(catalog_state *s,uint32_t *issues){
    if(!s->fatal){s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_UNAVAILABLE;
        s->fatal_reason=MB_TITLE_CATALOG_REASON_PLATFORM_PROVIDER_UNAVAILABLE;}
    (void)issue_add(s,MB_TITLE_CATALOG_ISSUE_PLATFORM_PROVIDER_UNAVAILABLE,issues);
}
static void partial(catalog_state *s,uint32_t reason,uint32_t issue,uint32_t *issues){
    if(s->fatal)return;
    s->result->status=MB_TITLE_CATALOG_STATUS_PARTIAL;s->result->complete=0;
    if(s->result->reason==MB_TITLE_CATALOG_REASON_NONE||reason==MB_TITLE_CATALOG_REASON_SOURCE_CHANGED)
        s->result->reason=reason;
    (void)issue_add(s,issue,issues);
}
static uint32_t issue_for_reason(uint32_t reason){
    switch(reason){
        case MB_TITLE_CATALOG_REASON_PLATFORM_PROVIDER_UNAVAILABLE:return MB_TITLE_CATALOG_ISSUE_PLATFORM_PROVIDER_UNAVAILABLE;
        case MB_TITLE_CATALOG_REASON_INVALID_PROVIDER_CAPTURE:return MB_TITLE_CATALOG_ISSUE_INVALID_PROVIDER_CAPTURE;
        case MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT:return MB_TITLE_CATALOG_ISSUE_INVENTORY_RESOURCE_LIMIT;
        case MB_TITLE_CATALOG_REASON_SOURCE_CHANGED:return MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED;
        case MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE:return MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE;
        default:return MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR;
    }
}
static int inaccessible(int error){return error==ENOENT||error==EACCES||error==EPERM||error==ENOTDIR||error==ELOOP;}
static int op_lstat(catalog_state *s,const char *path,struct stat *st){
    if(!reserve_ops(s,1))return -1;++s->attempted;errno=0;
    if(s->ops->lstat_path(s->ops->context,path,st))return errno?errno:EIO;
    return 0;
}
static int op_stat_relative(catalog_state *s,int fd,const char *name,struct stat *st){
    if(!reserve_ops(s,1))return -1;++s->attempted;errno=0;
    if(s->ops->stat_relative(s->ops->context,fd,name,st,AT_SYMLINK_NOFOLLOW))return errno?errno:EIO;
    return 0;
}
static int op_fstat(catalog_state *s,int fd,struct stat *st){
    if(!reserve_ops(s,1))return -1;++s->attempted;errno=0;
    if(s->ops->stat_fd(s->ops->context,fd,st))return errno?errno:EIO;
    return 0;
}
static int op_open_path(catalog_state *s,const char *path){
    int fd;if(!reserve_ops(s,2))return -2;++s->attempted;errno=0;
    fd=s->ops->open_path(s->ops->context,path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    return fd<0?-(errno?errno:EIO)-2:fd;
}
static int op_open_relative(catalog_state *s,int parent,const char *name){
    int fd;if(!reserve_ops(s,2))return -2;++s->attempted;errno=0;
    fd=s->ops->open_relative(s->ops->context,parent,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    return fd<0?-(errno?errno:EIO)-2:fd;
}
static int same_node(const struct stat *a,const struct stat *b){
    return a->st_dev==b->st_dev&&a->st_ino==b->st_ino&&a->st_mode==b->st_mode;
}
static int same_directory(const struct stat *a,const struct stat *b){
#if defined(__APPLE__)
    const struct timespec am=a->st_mtimespec,bm=b->st_mtimespec,ac=a->st_ctimespec,bc=b->st_ctimespec;
#else
    const struct timespec am=a->st_mtim,bm=b->st_mtim,ac=a->st_ctim,bc=b->st_ctim;
#endif
    return same_node(a,b)&&a->st_size==b->st_size&&a->st_nlink==b->st_nlink&&
        am.tv_sec==bm.tv_sec&&am.tv_nsec==bm.tv_nsec&&ac.tv_sec==bc.tv_sec&&ac.tv_nsec==bc.tv_nsec;
}
static void close_node(catalog_state *s,size_t index){
    catalog_directory *d=&s->directories[index];
    if(d->fd<0)return;
    ++s->attempted;errno=0;
    if(s->ops->close_fd(s->ops->context,d->fd)){
        s->result->cleanup_complete=0;
    s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;
    s->fatal_reason=MB_TITLE_CATALOG_REASON_PROVIDER_ERROR;
    }
    d->fd=-1;
}
static void cleanup(catalog_state *s){
    size_t i;
    if(s->stream){
        ++s->attempted;errno=0;
        if(s->ops->closedir_stream(s->ops->context,s->stream)){
            s->result->cleanup_complete=0;
            s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;
            s->fatal_reason=MB_TITLE_CATALOG_REASON_PROVIDER_ERROR;
        }
        s->stream=NULL;
        if(s->directory_count)s->directories[s->directory_count-1].fd=-1;
    }
    i=s->directory_count;while(i)close_node(s,--i);
}
static int parse_components(char path[513],const char **parts,size_t *count){
    size_t i,start=1,n=0,len=strlen(path);
    for(i=1;i<=len;++i)if(i==len||path[i]=='/'){
        if(n>=16)return 0;parts[n++]=path+start;path[i]=0;start=i+1;
    }
    *count=n;return n>0;
}
static int root_open(catalog_state *s,char path[513],const char **parts,size_t count,uint32_t *issues){
    struct stat path_st,fd_st;int error,fd;size_t i;
    s->directories[0].fd=-1;s->directory_count=0;
    error=op_lstat(s,"/",&path_st);
    if(error){if(error<0)return 0;if(inaccessible(error))unavailable(s,issues);
        else fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return 0;}
    fd=op_open_path(s,"/");
    if(fd==-2)return 0;
    if(fd< -2){error=-fd-2;if(inaccessible(error))unavailable(s,issues);
        else fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return 0;}
    s->directories[0].fd=fd;s->directories[0].name="/";s->directory_count=1;
    error=op_fstat(s,fd,&fd_st);
    if(error){if(error<0)return 0;unavailable(s,issues);return 0;}
    if(!S_ISDIR(fd_st.st_mode)||!same_node(&path_st,&fd_st)){
        partial(s,MB_TITLE_CATALOG_REASON_SOURCE_CHANGED,MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED,issues);return 0;}
    s->directories[0].identity=fd_st;
    for(i=0;i<count;++i){
        struct stat before,opened;const int parent=s->directories[s->directory_count-1].fd;
        error=op_stat_relative(s,parent,parts[i],&before);
        if(error){if(error<0)return 0;if(inaccessible(error))unavailable(s,issues);
            else fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return 0;}
        if(!S_ISDIR(before.st_mode)||S_ISLNK(before.st_mode)){unavailable(s,issues);return 0;}
        fd=op_open_relative(s,parent,parts[i]);
        if(fd==-2)return 0;
        if(fd< -2){error=-fd-2;if(inaccessible(error))unavailable(s,issues);
            else fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return 0;}
        s->directories[s->directory_count].fd=fd;s->directories[s->directory_count].name=parts[i];
        ++s->directory_count;
        error=op_fstat(s,fd,&opened);
        if(error){if(error<0)return 0;unavailable(s,issues);return 0;}
        if(!S_ISDIR(opened.st_mode)||!same_node(&before,&opened)){
            partial(s,MB_TITLE_CATALOG_REASON_SOURCE_CHANGED,MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED,issues);return 0;}
        s->directories[s->directory_count-1].identity=opened;
    }
    if(!reserve_ops(s,1))return 0;
    ++s->attempted;errno=0;
    s->stream=s->ops->fdopendir_stream(s->ops->context,s->directories[s->directory_count-1].fd);
    if(!s->stream){error=errno?errno:ENOMEM;if(inaccessible(error))unavailable(s,issues);
        else fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return 0;}
    /* Keep the descriptor number for identity checks; closedir consumes its close escrow. */
    (void)path;return 1;
}

static int valid_ascii_id(const char *s,size_t *length){
    size_t i,n=strnlen(s,65);
    if(!n||n>64)return 0;
    for(i=0;i<n;++i){const unsigned char c=(unsigned char)s[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return 0;
    }
    if(!((s[0]>='A'&&s[0]<='Z')||(s[0]>='a'&&s[0]<='z')||(s[0]>='0'&&s[0]<='9')))return 0;
    *length=n;return 1;
}
static int valid_zero_tail(const char *s,size_t cap,int available,int required){
    size_t n=strnlen(s,cap),i;
    if(n==cap)return 0;
    if(required&&!available)return 0;
    if(!available&&n)return 0;
    for(i=n+1;i<cap;++i)if(s[i])return 0;
    return 1;
}
static int valid_metadata(const mb_title_metadata_result_v1 *m,const char *candidate,size_t candidate_bytes){
    size_t id_bytes,digest_bytes,i;
    if(m->struct_size!=sizeof(*m)||m->abi_version!=MB_TITLE_METADATA_ABI_VERSION||m->status!=MB_TITLE_METADATA_OK||
       m->reason!=MB_TITLE_METADATA_REASON_NONE||!m->metadata_complete||!m->title_id_available||
       m->captured_bytes==0||m->captured_bytes>MB_TITLE_METADATA_INPUT_LIMIT||
       m->source_scope!=MB_TITLE_SOURCE_METADATA_CAPTURE||m->metadata_identity_scope!=MB_TITLE_IDENTITY_CAPTURED_BYTES_SHA256||
       m->name_available>1||m->version_available>1||m->locale_available>1||m->metadata_sha256_available!=1||
       m->installed_observed||m->running_observed||m->build_fingerprint_available||m->engine_acceptance_verified||
       m->runtime_supported||m->installation_supported||m->authenticity_verified||m->game_acceptance_verified||
       m->reserved[0]||m->reserved[1])return 0;
    if(!valid_ascii_id(m->title_id,&id_bytes)||id_bytes!=candidate_bytes||memcmp(m->title_id,candidate,candidate_bytes))return 0;
    if(!valid_zero_tail(m->title_id,sizeof(m->title_id),1,1)||
       !valid_zero_tail(m->name,sizeof(m->name),m->name_available,0)||
       !valid_zero_tail(m->version,sizeof(m->version),m->version_available,0)||
       !valid_zero_tail(m->locale,sizeof(m->locale),m->locale_available,0)||
       !valid_zero_tail(m->metadata_sha256,sizeof(m->metadata_sha256),1,1))return 0;
    digest_bytes=strnlen(m->metadata_sha256,sizeof(m->metadata_sha256));
    if(digest_bytes!=64)return 0;
    for(i=0;i<digest_bytes;++i)if(!((m->metadata_sha256[i]>='0'&&m->metadata_sha256[i]<='9')||
        (m->metadata_sha256[i]>='a'&&m->metadata_sha256[i]<='f')))return 0;
    return 1;
}
static unsigned char fold_ascii(unsigned char c){return c>='A'&&c<='Z'?(unsigned char)(c-'A'+'a'):c;}
static int ids_equal_folded(const char *a,const char *b){
    size_t i,na=strnlen(a,65),nb=strnlen(b,65);if(na!=nb||na==65)return 0;
    for(i=0;i<na;++i)if(fold_ascii((unsigned char)a[i])!=fold_ascii((unsigned char)b[i]))return 0;
    return 1;
}
static int candidate_name(const char *name,size_t n){
    size_t i;if(!n||n>64)return 0;
    for(i=0;i<n;++i){const unsigned char c=(unsigned char)name[i];
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||
             (i>0&&(c=='.'||c=='_'||c=='-'))))return 0;
    }
    return 1;
}
static int is_dot_name(const char *name,size_t n){return (n==1&&name[0]=='.')||(n==2&&name[0]=='.'&&name[1]=='.');}
static void source_read_account(catalog_state *s,const mb_title_source_result_v1 *child){
    if(child->operations_attempted>UINT64_MAX-s->attempted||child->actual_read_bytes>UINT64_MAX-s->actual_read_bytes){
        s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;s->fatal_reason=MB_TITLE_CATALOG_REASON_PROVIDER_ERROR;return;
    }
    s->attempted+=child->operations_attempted;s->actual_read_bytes+=child->actual_read_bytes;
}
static int child_capture(catalog_state *s,const mb_title_catalog_request_v1 *request,const char *candidate,
    size_t candidate_bytes,const char *fixture_root,size_t fixture_root_bytes,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    const mb_title_source_test_ops_v1 *source_ops,
#endif
    uint8_t *workspace,mb_title_source_result_v1 *child){
    mb_title_source_request_v1 child_request;memset(&child_request,0,sizeof(child_request));
    child_request.struct_size=sizeof(child_request);child_request.abi_version=MB_TITLE_SOURCE_ABI_VERSION;
    child_request.profile=request->profile;child_request.candidate_bytes=(uint32_t)candidate_bytes;child_request.candidate=candidate;
    memset(child,0,sizeof(*child));
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    if(fixture_root)return mb_title_source_capture_fixture_v1(&child_request,&s->ledger->source_io,workspace,
        MB_TITLE_SOURCE_WORKSPACE_BYTES,child,fixture_root,fixture_root_bytes,source_ops);
#else
    (void)fixture_root;(void)fixture_root_bytes;
#endif
    return mb_title_source_capture_v1(&child_request,&s->ledger->source_io,workspace,MB_TITLE_SOURCE_WORKSPACE_BYTES,child);
}
static int count_entry(catalog_state *s){
    if(s->ledger->entries_seen>=MB_TITLE_CATALOG_MAX_ENTRIES){
        s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;s->fatal_reason=MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT;return 0;
    }
    ++s->ledger->entries_seen;return 1;
}
static void classify_child(catalog_state *s,const mb_title_catalog_request_v1 *request,const char *candidate,
    size_t candidate_bytes,const char *fixture_root,size_t fixture_root_bytes,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    const mb_title_source_test_ops_v1 *source_ops,
#endif
    uint8_t *workspace,mb_title_metadata_result_v1 *records,uint32_t *issues){
    mb_title_source_result_v1 child;mb_title_source_status child_status;uint32_t i;
    if(s->ledger->candidates_seen>=MB_TITLE_CATALOG_MAX_CANDIDATES||
       s->result->record_count>=MB_TITLE_CATALOG_MAX_RECORDS){
        s->fatal=1;s->fatal_status=MB_TITLE_CATALOG_STATUS_ERROR;
        s->fatal_reason=MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT;return;
    }
    ++s->ledger->candidates_seen;s->result->candidates_seen=s->ledger->candidates_seen;
    child_status=(mb_title_source_status)child_capture(s,request,candidate,candidate_bytes,fixture_root,fixture_root_bytes,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
        source_ops,
#endif
        workspace,&child);
    if(child.struct_size==sizeof(child)&&child.abi_version==MB_TITLE_SOURCE_ABI_VERSION)source_read_account(s,&child);
    else {fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return;}
    if(s->fatal)return;
    if(child_status==MB_TITLE_SOURCE_RESOURCE_LIMIT){
        fatal(s,MB_TITLE_CATALOG_REASON_INVENTORY_RESOURCE_LIMIT,MB_TITLE_CATALOG_ISSUE_INVENTORY_RESOURCE_LIMIT,issues);return;
    }
    if(child_status==MB_TITLE_SOURCE_INTERNAL_ERROR||child_status==MB_TITLE_SOURCE_CLEANUP_FAILED||
       child_status==MB_TITLE_SOURCE_BAD_ARGUMENT){
        fatal(s,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);return;
    }
    if(child_status!=MB_TITLE_SOURCE_OK){
        if(child_status==MB_TITLE_SOURCE_CHANGED)
            partial(s,MB_TITLE_CATALOG_REASON_SOURCE_CHANGED,MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED,issues);
        else partial(s,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
            MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);
        return;
    }
    if(child.requested_profile!=request->profile||!child.source_complete||!child.identity_checked||
       !child.candidate_matches||!child.cleanup_complete||child.metadata_status!=MB_TITLE_METADATA_OK||
       child.metadata_reason!=MB_TITLE_METADATA_REASON_NONE||
       !valid_metadata(&child.metadata,candidate,candidate_bytes)){
        fatal(s,MB_TITLE_CATALOG_REASON_INVALID_PROVIDER_CAPTURE,MB_TITLE_CATALOG_ISSUE_INVALID_PROVIDER_CAPTURE,issues);return;
    }
    for(i=0;i<s->result->record_count;++i){
        if(ids_equal_folded(records[i].title_id,child.metadata.title_id)){
            fatal(s,MB_TITLE_CATALOG_REASON_INVALID_PROVIDER_CAPTURE,MB_TITLE_CATALOG_ISSUE_INVALID_PROVIDER_CAPTURE,issues);return;
        }
    }
    records[s->result->record_count++]=child.metadata;
}

static mb_title_catalog_status scan_impl(const mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,char *name_scratch,char *candidate_scratch,uint8_t *workspace,
    mb_title_metadata_result_v1 *records,uint32_t *issues,mb_title_catalog_result_v1 *result,
    const char *fixture_root,size_t fixture_root_bytes,const catalog_ops *ops,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    const mb_title_source_test_ops_v1 *source_ops,
#endif
    int fixture){
    catalog_state state;const char *parts[16];char root_copy[513];size_t part_count=0;int root_ready=0;
    if(fixture){memcpy(root_copy,fixture_root,fixture_root_bytes);root_copy[fixture_root_bytes]=0;
        if(!parse_components(root_copy,parts,&part_count))return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;}
    else{
        const char *root=request->profile==MB_TITLE_CACHE_SYSTEM_PRIVATE?"/system_data/priv/appmeta":"/user/appmeta";
        size_t bytes=strlen(root);memcpy(root_copy,root,bytes+1);if(!parse_components(root_copy,parts,&part_count))return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;
    }
    memset(&state,0,sizeof(state));state.ledger=ledger;state.result=result;state.ops=ops?ops:&ordinary_catalog_ops;
    state.initial_reserved_operations=ledger->source_io.reserved_operations;
    state.initial_reserved_read_bytes=ledger->source_io.reserved_read_bytes;
    result_init(result,request);memset(records,0,sizeof(*records)*MB_TITLE_CATALOG_MAX_RECORDS);
    memset(issues,0,sizeof(*issues)*MB_TITLE_CATALOG_MAX_ISSUES);
    memset(name_scratch,0,MB_TITLE_CATALOG_ENTRY_NAME_BYTES);memset(candidate_scratch,0,MB_TITLE_CATALOG_CANDIDATE_BYTES);
    memset(workspace,0,MB_TITLE_SOURCE_WORKSPACE_BYTES);
    root_ready=root_open(&state,root_copy,parts,part_count,issues);
    if(root_ready){
        for(;;){
            struct dirent *entry;size_t name_bytes;struct stat entry_stat;int error;
            if(!reserve_ops(&state,1))break;
            ++state.attempted;errno=0;entry=state.ops->readdir_stream(state.ops->context,state.stream);
            if(!entry){
                if(errno){fatal(&state,MB_TITLE_CATALOG_REASON_PROVIDER_ERROR,MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR,issues);}
                else result->enumeration_eof=1;
                break;
            }
            name_bytes=strnlen(entry->d_name,MB_TITLE_CATALOG_ENTRY_NAME_BYTES);
            if(!reserve_name_bytes(&state,name_bytes))break;
            if(name_bytes>=MB_TITLE_CATALOG_ENTRY_NAME_BYTES){
                if(!count_entry(&state))break;
                partial(&state,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
                    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);continue;
            }
            if(name_bytes==0){partial(&state,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
                    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);continue;}
            memcpy(name_scratch,entry->d_name,name_bytes);name_scratch[name_bytes]=0;
            if(is_dot_name(name_scratch,name_bytes))continue;
            if(!count_entry(&state))break;
            if(memchr(name_scratch,'/',name_bytes)){
                partial(&state,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
                    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);continue;
            }
            error=op_stat_relative(&state,state.directories[state.directory_count-1].fd,name_scratch,&entry_stat);
            if(error<0)break;
            if(error){
                if(error==ENOENT)partial(&state,MB_TITLE_CATALOG_REASON_SOURCE_CHANGED,MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED,issues);
                else partial(&state,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
                    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);
                continue;
            }
            if(S_ISREG(entry_stat.st_mode))continue;
            if(S_ISLNK(entry_stat.st_mode)||!S_ISDIR(entry_stat.st_mode)){
                partial(&state,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
                    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);continue;
            }
            if(!candidate_name(name_scratch,name_bytes)){
                partial(&state,MB_TITLE_CATALOG_REASON_PROVIDER_OBSERVATION_INCOMPLETE,
                    MB_TITLE_CATALOG_ISSUE_PROVIDER_OBSERVATION_INCOMPLETE,issues);continue;
            }
            memcpy(candidate_scratch,name_scratch,name_bytes+1);
            classify_child(&state,request,candidate_scratch,name_bytes,fixture_root,fixture_root_bytes,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
                source_ops,
#endif
                workspace,records,issues);
            memset(candidate_scratch,0,MB_TITLE_CATALOG_CANDIDATE_BYTES);
            if(state.fatal)break;
            memset(name_scratch,0,MB_TITLE_CATALOG_ENTRY_NAME_BYTES);
        }
        if(result->enumeration_eof&&!state.fatal){
            /* Recheck the held root chain and its path bindings before claiming a complete scan. */
            struct stat current;size_t i;int checked=1;
            if(!state.stream)checked=0;
            if(checked){
                const int rootfd=state.directories[state.directory_count-1].fd;
                int error=op_fstat(&state,rootfd,&current);
                if(error||!same_directory(&state.directories[state.directory_count-1].identity,&current))checked=0;
            }
            if(checked){
                for(i=state.directory_count-1;i>0;--i){
                    struct stat named,opened;const int parent=state.directories[i-1].fd;int error;
                    error=op_stat_relative(&state,parent,state.directories[i].name,&named);
                    if(error||!same_directory(&state.directories[i].identity,&named)){checked=0;break;}
                    if(i+1<state.directory_count){
                        error=op_fstat(&state,state.directories[i].fd,&opened);
                        if(error||!same_directory(&state.directories[i].identity,&opened)){checked=0;break;}
                    }
                }
            }
            if(checked){
                struct stat root_path;int error=op_lstat(&state,"/",&root_path);
                if(error||!same_node(&state.directories[0].identity,&root_path))checked=0;
                else{struct stat root_fd;error=op_fstat(&state,state.directories[0].fd,&root_fd);
                    if(error||!same_node(&state.directories[0].identity,&root_fd))checked=0;}
            }
            if(checked)result->root_identity_checked=1;
            else if(!state.fatal)partial(&state,MB_TITLE_CATALOG_REASON_SOURCE_CHANGED,
                MB_TITLE_CATALOG_ISSUE_SOURCE_CHANGED,issues);
        }
    }
    cleanup(&state);
    result->entries_seen=ledger->entries_seen;result->candidates_seen=ledger->candidates_seen;
    result->entry_name_bytes=ledger->entry_name_bytes;
    result->operations_reserved=ledger->source_io.reserved_operations-state.initial_reserved_operations;
    result->requested_read_bytes=ledger->source_io.reserved_read_bytes-state.initial_reserved_read_bytes;
    result->operations_attempted=state.attempted;result->actual_read_bytes=state.actual_read_bytes;
    if(state.fatal){result->status=(uint32_t)state.fatal_status;result->reason=state.fatal_reason;result->complete=0;
        if(result->status==MB_TITLE_CATALOG_STATUS_UNAVAILABLE)result->issue_count=1;
        else (void)issue_add(&state,issue_for_reason(result->reason),issues);
        memset(records,0,sizeof(*records)*MB_TITLE_CATALOG_MAX_RECORDS);
        result->record_count=0;
    }else if(result->status==MB_TITLE_CATALOG_STATUS_PARTIAL){result->complete=0;}
    else if(result->enumeration_eof&&result->root_identity_checked&&result->cleanup_complete&&
            result->issue_count==0&&result->record_count==ledger->candidates_seen){
        result->status=MB_TITLE_CATALOG_STATUS_OBSERVED;result->reason=MB_TITLE_CATALOG_REASON_NONE;result->complete=1;
    }else{
        result->status=MB_TITLE_CATALOG_STATUS_ERROR;result->reason=MB_TITLE_CATALOG_REASON_PROVIDER_ERROR;result->complete=0;
        memset(records,0,sizeof(*records)*MB_TITLE_CATALOG_MAX_RECORDS);result->record_count=0;
        issues[0]=MB_TITLE_CATALOG_ISSUE_PROVIDER_ERROR;result->issue_count=1;
    }
    clear_bytes(name_scratch,MB_TITLE_CATALOG_ENTRY_NAME_BYTES);
    clear_bytes(candidate_scratch,MB_TITLE_CATALOG_CANDIDATE_BYTES);
    clear_bytes(workspace,MB_TITLE_SOURCE_WORKSPACE_BYTES);
    if(result->status==MB_TITLE_CATALOG_STATUS_ERROR||result->status==MB_TITLE_CATALOG_STATUS_UNAVAILABLE){
        if(result->status==MB_TITLE_CATALOG_STATUS_UNAVAILABLE){
            memset(records,0,sizeof(*records)*MB_TITLE_CATALOG_MAX_RECORDS);
            if(result->issue_count==0){issues[0]=MB_TITLE_CATALOG_ISSUE_PLATFORM_PROVIDER_UNAVAILABLE;result->issue_count=1;}
        }
    }
    return (mb_title_catalog_status)result->status;
}

static int init_source_test_ops(mb_title_source_test_ops_v1 *out){
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    memset(out,0,sizeof(*out));out->struct_size=sizeof(*out);out->abi_version=1;
    out->lstat_path=os_lstat;out->open_path=os_open;out->stat_relative=os_atstat;out->open_relative=os_atopen;
    out->stat_fd=os_fstat;out->read_fd=NULL;out->close_fd=os_close;
    /* The source fixture needs a read callback; plain POSIX read is kept local. */
    return 1;
#else
    (void)out;return 0;
#endif
}
#ifdef MB_TITLE_CATALOG_HOST_TESTING
static ssize_t os_read(void *v,int d,void *p,size_t n){(void)v;return read(d,p,n);}
#endif
static mb_title_catalog_status scan_common(const mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,char *name_scratch,size_t name_bytes,char *candidate,size_t candidate_bytes,
    uint8_t *workspace,size_t workspace_bytes,mb_title_metadata_result_v1 *records,uint32_t record_capacity,
    uint32_t *issues,uint32_t issue_capacity,mb_title_catalog_result_v1 *result,
    const char *fixture_root,size_t fixture_root_bytes,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    const mb_title_source_test_ops_v1 *source_ops_in,const mb_title_catalog_test_ops_v1 *catalog_ops_in,
#endif
    int fixture){
    catalog_ops selected_ops=ordinary_catalog_ops;const catalog_ops *ops=&selected_ops;
#ifdef MB_TITLE_CATALOG_HOST_TESTING
    mb_title_source_test_ops_v1 default_source_ops;const mb_title_source_test_ops_v1 *source_ops=source_ops_in;
    if(!preflight(request,ledger,name_scratch,name_bytes,candidate,candidate_bytes,workspace,workspace_bytes,
       records,record_capacity,issues,issue_capacity,result,fixture_root,fixture_root_bytes,ops,
       source_ops_in,sizeof(mb_title_source_test_ops_v1),_Alignof(mb_title_source_test_ops_v1),
       catalog_ops_in,sizeof(mb_title_catalog_test_ops_v1),_Alignof(mb_title_catalog_test_ops_v1),fixture))
        return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;
    if(catalog_ops_in){
        if(catalog_ops_in->struct_size!=sizeof(*catalog_ops_in)||catalog_ops_in->abi_version!=1||
           !catalog_ops_in->lstat_path||!catalog_ops_in->open_path||!catalog_ops_in->stat_relative||
           !catalog_ops_in->open_relative||!catalog_ops_in->stat_fd||!catalog_ops_in->close_fd||
           !catalog_ops_in->fdopendir_stream||!catalog_ops_in->readdir_stream||!catalog_ops_in->closedir_stream)
            return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;
        selected_ops.context=catalog_ops_in->context;selected_ops.lstat_path=catalog_ops_in->lstat_path;
        selected_ops.open_path=catalog_ops_in->open_path;selected_ops.stat_relative=catalog_ops_in->stat_relative;
        selected_ops.open_relative=catalog_ops_in->open_relative;selected_ops.stat_fd=catalog_ops_in->stat_fd;
        selected_ops.close_fd=catalog_ops_in->close_fd;selected_ops.fdopendir_stream=catalog_ops_in->fdopendir_stream;
        selected_ops.readdir_stream=catalog_ops_in->readdir_stream;selected_ops.closedir_stream=catalog_ops_in->closedir_stream;
        ops=&selected_ops;
    }
    if(fixture&&!source_ops){init_source_test_ops(&default_source_ops);default_source_ops.read_fd=os_read;source_ops=&default_source_ops;}
    if(fixture&&source_ops&&(!source_ops->read_fd||!source_ops->lstat_path||!source_ops->open_path||
       !source_ops->stat_relative||!source_ops->open_relative||!source_ops->stat_fd||!source_ops->close_fd))
        return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;
    if(!valid_ops(ops))return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;
    return scan_impl(request,ledger,name_scratch,candidate,workspace,records,issues,result,
        fixture_root,fixture_root_bytes,ops,source_ops,fixture);
#else
    if(!preflight(request,ledger,name_scratch,name_bytes,candidate,candidate_bytes,workspace,workspace_bytes,
       records,record_capacity,issues,issue_capacity,result,NULL,0,ops,NULL,0,0,NULL,0,0,0))return MB_TITLE_CATALOG_STATUS_BAD_ARGUMENT;
    return scan_impl(request,ledger,name_scratch,candidate,workspace,records,issues,result,NULL,0,ops,0);
#endif
}

mb_title_catalog_status mb_title_catalog_scan_v1(const mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,char *entry_name_scratch,size_t entry_name_scratch_bytes,
    char *candidate_scratch,size_t candidate_scratch_bytes,uint8_t *source_workspace,size_t source_workspace_bytes,
    mb_title_metadata_result_v1 *records,uint32_t record_capacity,uint32_t *issue_codes,uint32_t issue_capacity,
    mb_title_catalog_result_v1 *result){
    return scan_common(request,ledger,entry_name_scratch,entry_name_scratch_bytes,candidate_scratch,candidate_scratch_bytes,
        source_workspace,source_workspace_bytes,records,record_capacity,issue_codes,issue_capacity,result,NULL,0,
#ifdef MB_TITLE_CATALOG_HOST_TESTING
        NULL,NULL,
#endif
        0);
}
#ifdef MB_TITLE_CATALOG_HOST_TESTING
mb_title_catalog_status mb_title_catalog_scan_fixture_v1(const mb_title_catalog_request_v1 *request,
    mb_title_catalog_ledger_v1 *ledger,char *entry_name_scratch,size_t entry_name_scratch_bytes,
    char *candidate_scratch,size_t candidate_scratch_bytes,uint8_t *source_workspace,size_t source_workspace_bytes,
    mb_title_metadata_result_v1 *records,uint32_t record_capacity,uint32_t *issue_codes,uint32_t issue_capacity,
    mb_title_catalog_result_v1 *result,const char *fixture_root,size_t fixture_root_bytes,
    const mb_title_source_test_ops_v1 *source_ops,const mb_title_catalog_test_ops_v1 *catalog_ops){
    return scan_common(request,ledger,entry_name_scratch,entry_name_scratch_bytes,candidate_scratch,candidate_scratch_bytes,
        source_workspace,source_workspace_bytes,records,record_capacity,issue_codes,issue_capacity,result,
        fixture_root,fixture_root_bytes,source_ops,catalog_ops,1);
}
#endif
