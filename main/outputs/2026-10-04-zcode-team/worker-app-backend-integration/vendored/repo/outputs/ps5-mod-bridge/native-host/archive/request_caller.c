#define _DARWIN_C_SOURCE 1
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "request.h"
#include "prepared.h"
#include "private/posix_compat.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <mbedtls/sha256.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define INBOX_ROOT "/data/ModbridgeInbox"
#define ROWS 256u
#define NAMES (256u*1024u)
#define REPORT_BYTES (64u*1024u)
#define CALLER_OPERATIONS 256u
#define CALLER_IO_BYTES (2u*REPORT_BYTES + 2u*MB_ARCHIVE_REQUEST_MAX_BYTES + 512u + 8u)

static mb_archive_row_v1 rows[ROWS], replay_rows[ROWS];
static uint32_t sorted[ROWS], replay_sorted[ROWS], selected[ROWS];
static char names[NAMES], replay_names[NAMES];
static uint8_t chunk[MB_ARCHIVE_CHUNK_BYTES], replay_chunk[MB_ARCHIVE_CHUNK_BYTES];
static mb_archive_owned_file_v1 journal[ROWS+3];
static mb_archive_source_v1 source;
static mb_archive_inventory_v1 inventory;
static mb_archive_materialized_v1 materialized;
static mb_archive_materialize_request_v1 materialize_request;
static mb_archive_limits_v1 limits;
static mb_archive_budget_v1 budget;
static mb_archive_group_v1 groups[ROWS];
static mb_archive_group_member_v1 members[ROWS];
static uint32_t group_sort[ROWS];
static char group_keys[NAMES];
static mb_archive_prepared_file_v1 candidate_files[ROWS], bound_files[ROWS];
static mb_archive_prepared_result_v1 candidate, bound;
static mb_archive_prepared_profile_v1 profile;
static mb_archive_request_v1 decoded;
static char report[REPORT_BYTES];
static size_t report_len;
static unsigned caller_ops;
static uint8_t request_sha256[32], context_sha256[32];
static int request_status=-1, scan_status=-1, prepare_status=-1;
static int materialize_status=-1, bind_status=-1, claim_created;
static int report_save_failure;
static const char *outcome="not_started";

static int step(void) { if(caller_ops==CALLER_OPERATIONS)return 0;++caller_ops;return 1; }
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
static int hash_bytes(const uint8_t *bytes,size_t size,uint8_t digest[32]) {
    mbedtls_sha256_context h;int rc=0;
    mbedtls_sha256_init(&h);
    if(mbedtls_sha256_starts(&h,0)||mbedtls_sha256_update(&h,bytes,size)||mbedtls_sha256_finish(&h,digest))rc=0;
    else rc=1;
    mbedtls_sha256_free(&h);return rc;
}
static int read_small(int root,const char *name,uint8_t *buffer,size_t capacity,size_t *length,uint8_t digest[32]) {
    struct stat before,after;int fd=-1,flags,good=0;uint64_t size,offset=0;ssize_t got;uint8_t extra;
    if(!step())return 0;
    fd=mb_archive_posix_openat(root,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
    if(fd<0)return 0;
    if(!step()||fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_size<0||(uint64_t)before.st_size>capacity)goto done;
    if(!step())goto done;flags=fcntl(fd,F_GETFL);if(flags<0||(flags&O_ACCMODE)!=O_RDONLY)goto done;
    size=(uint64_t)before.st_size;
    while(offset<size) {
        size_t want=(size_t)(size-offset>MB_ARCHIVE_CHUNK_BYTES?MB_ARCHIVE_CHUNK_BYTES:size-offset);
        if(!step())goto done;got=pread(fd,buffer+(size_t)offset,want,(off_t)offset);
        if(got<=0)goto done;offset+=(uint64_t)got;
    }
    if(!step()||pread(fd,&extra,1,(off_t)size)!=0)goto done;
    if(!step()||fstat(fd,&after)||!stamp_same(&before,&after))goto done;
    *length=(size_t)size;
    if(!hash_bytes(buffer,*length,digest))goto done;
    good=1;
done:
    if(fd>=0){if(!step())good=0;if(close(fd))good=0;}
    return good;
}
static int write_exact(int fd,const uint8_t *bytes,size_t size) {
    size_t offset=0;ssize_t wrote;
    while(offset<size) {
        if(!step())return 0;
        wrote=write(fd,bytes+offset,size-offset);
        if(wrote<=0)return 0;offset+=(size_t)wrote;
    }
    return 1;
}
static int absent(int root,const char *name) {
    struct stat st;if(!step())return 0;
    if(mb_archive_posix_fstatat(root,name,&st,AT_SYMLINK_NOFOLLOW)==0)return 0;
    return errno==ENOENT;
}
static void add(const char *fmt,...) {
    va_list ap;int n;if(report_len>=sizeof(report))return;
    va_start(ap,fmt);n=vsnprintf(report+report_len,sizeof(report)-report_len,fmt,ap);va_end(ap);
    if(n<0||(size_t)n>=sizeof(report)-report_len){report_len=sizeof(report);return;}
    report_len+=(size_t)n;
}
static void hex(const uint8_t *bytes,char out[65]) {
    static const char d[]="0123456789abcdef";unsigned i;
    for(i=0;i<32;i++){out[2*i]=d[bytes[i]>>4];out[2*i+1]=d[bytes[i]&15];}out[64]=0;
}
static void append_files(const char *label,const mb_archive_prepared_file_v1 *files,uint32_t count) {
    uint32_t i;char hash[65];add("\"%s\":[",label);
    for(i=0;i<count;i++) {
        const mb_archive_prepared_file_v1 *f=files+i;hex(f->sha256,hash);
        add("%s{\"row_index\":%u,\"group_index\":%u,\"role\":%u,\"bytes\":%"PRIu64
            ",\"sha256\":\"%s\",\"source_name\":\"%s\",\"destination_name\":\"%s\"}",
            i?",":"",f->row_index,f->group_index,f->role,f->bytes,hash,f->source_name,f->destination_name);
    }
    add("]");
}
static void make_report(int rc,const char *phase,int cleanup_complete) {
    char rqh[65],sourceh[65],ctxh[65],invh[65],token[65];
    hex(request_sha256,rqh);hex(decoded.source_sha256,sourceh);hex(context_sha256,ctxh);
    hex(inventory.inventory_sha256,invh);hex(decoded.target_token,token);
    add("{\"schema_version\":1,\"scope\":\"bounded_archive_request_preparation\",\"outcome\":\"%s\",\"phase\":\"%s\",",
        outcome,phase);
    add("\"complete\":%s,\"request_complete\":%s,\"scan_complete\":%s,\"candidate_complete\":%s,"
        "\"materialize_attempted\":%s,\"materialize_complete\":%s,\"bind_complete\":%s,\"cleanup_complete\":%s,",
        (rc==0&&cleanup_complete&&bound.complete)?"true":"false",decoded.complete?"true":"false",inventory.complete?"true":"false",
        candidate.complete?"true":"false",materialize_status>=0?"true":"false",
        materialized.complete?"true":"false",bound.complete?"true":"false",cleanup_complete?"true":"false");
    add("\"operation_status\":%d,\"request_status\":%d,\"scan_status\":%d,\"prepare_status\":%d,"
        "\"materialize_status\":%d,\"bind_status\":%d,",rc,request_status,scan_status,prepare_status,materialize_status,bind_status);
    add("\"request_id\":\"%s\",\"source_ref\":\"%s\",\"context_ref\":\"%s\","
        "\"quarantine_ref\":\"%s\",\"receipt_ref\":\"%s\",\"claim_ref\":\"%s\",",
        decoded.request_id,decoded.source_ref,decoded.context_ref,decoded.quarantine_ref,decoded.receipt_ref,decoded.claim_ref);
    add("\"request_sha256\":\"%s\",\"context_sha256\":\"%s\",\"target_token\":\"%s\","
        "\"source_sha256\":\"%s\",\"source_bytes\":%"PRIu64",\"inventory_sha256\":\"%s\",",
        rqh,ctxh,token,sourceh,decoded.source_bytes,invh);
    add("\"row_count\":%u,\"group_count\":%u,\"candidate_file_count\":%u,\"bound_file_count\":%u,"
        "\"decoded_bytes\":%"PRIu64",\"io_charged\":%"PRIu64",\"operations_charged\":%"PRIu64","
        "\"path_work_charged\":%"PRIu64",\"caller_io_reserved\":%u,\"caller_operations_reserved\":%u,",
        inventory.row_count,candidate.group_count,candidate.file_count,bound.file_count,materialized.decoded_bytes,
        budget.io_used,budget.operations_used,budget.path_work_used,CALLER_IO_BYTES,CALLER_OPERATIONS);
    add("\"installation_supported\":false,\"platform_compatible\":false,\"runtime_supported\":false,\"format_validated\":false,");
    append_files("candidate_files",candidate_files,candidate.file_count);add(",");
    append_files("bound_files",bound_files,bound.file_count);add(",\"journal_count\":%u,\"claim_created\":%s}\n",
        materialized.journal_count,claim_created?"true":"false");
}
static int create_claim(int root) {
    char body[512],rq[65],cx[65],src[65];int fd,ok=1,n;struct stat st;
    hex(request_sha256,rq);hex(context_sha256,cx);hex(decoded.source_sha256,src);
    n=snprintf(body,sizeof(body),"MBARCLAIM1\nid=%s\nrequest_sha256=%s\ncontext_sha256=%s\nsource_sha256=%s\nend\n",
        decoded.request_id,rq,cx,src);
    if(n<0||(size_t)n>=sizeof(body)||!step())return 0;
    fd=mb_archive_posix_openat(root,decoded.claim_ref,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0)return 0;claim_created=1;
    if(!step()||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||st.st_size!=0)ok=0;
    if(ok&&!write_exact(fd,(const uint8_t *)body,(size_t)n))ok=0;
    if(ok&&(!step()||fsync(fd)))ok=0;
    if(!step()||close(fd))ok=0;
    if(ok&&(!step()||fsync(root)))ok=0;
    return ok;
}
static int save_report(int root) {
    int fd,ok=1;struct stat st;size_t off=0;uint8_t check[1024];ssize_t got;
    if(report_len>=sizeof(report)||!step()){report_save_failure=1;return 0;}
    fd=mb_archive_posix_openat(root,decoded.receipt_ref,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(fd<0){report_save_failure=2;return 0;}
    if(!write_exact(fd,(const uint8_t *)report,report_len)){report_save_failure=3;ok=0;}
    if(ok&&(!step()||fsync(fd))){report_save_failure=4;ok=0;}
    if(ok&&(!step()||fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_size!=(off_t)report_len)){report_save_failure=5;ok=0;}
    while(ok&&off<report_len) {
        size_t want=report_len-off>sizeof(check)?sizeof(check):report_len-off;
        if(!step()){report_save_failure=6;ok=0;break;}
        got=pread(fd,check,want,(off_t)off);if(got!=(ssize_t)want||memcmp(check,report+off,want)){report_save_failure=7;ok=0;break;}off+=(size_t)got;
    }
    if(ok&&(!step()||pread(fd,check,1,(off_t)report_len)!=0)){report_save_failure=8;ok=0;}
    if(!step()||close(fd)){report_save_failure=9;ok=0;}
    if(ok&&(!step()||fsync(root))){report_save_failure=10;ok=0;}
    if(ok)report_save_failure=0;
    return ok;
}
static int close_fd(int *fd) {
    if(*fd<0)return 1;if(!step()){(void)close(*fd);*fd=-1;return 0;}
    if(close(*fd)){*fd=-1;return 0;}*fd=-1;return 1;
}
static int process_root(int root) {
    struct stat root_stat,source_stat,qstat;int rc=1,source_fd=-1,qfd=-1,root_ok=0;
    size_t request_len=0,context_len=0;const char *phase="root";
    mb_archive_buffers_v1 first={rows,ROWS,names,NAMES,sorted,ROWS,chunk,sizeof(chunk)};
    mb_archive_buffers_v1 replay={replay_rows,ROWS,replay_names,NAMES,replay_sorted,ROWS,replay_chunk,sizeof(replay_chunk)};
    mb_archive_prepared_view_v1 view;
    mb_archive_prepared_workspace_v1 workspace={group_sort,ROWS,groups,ROWS,members,ROWS,group_keys,NAMES};
    mb_archive_prepared_materialization_v1 binding;
    mb_archive_request_v1 request_local;mb_archive_request_ref_v1 request_ref;
    uint8_t request_buffer[MB_ARCHIVE_REQUEST_MAX_BYTES],context_buffer[MB_ARCHIVE_REQUEST_MAX_BYTES];
    uint64_t raw_cap=UINT64_C(128)<<20, expanded_cap=UINT64_C(256)<<20;
    int report_ok=0,cleanup_ok=1;
    memset(&inventory,0,sizeof(inventory));memset(&candidate,0,sizeof(candidate));
    memset(&materialized,0,sizeof(materialized));memset(&bound,0,sizeof(bound));
    memset(&decoded,0,sizeof(decoded));memset(&profile,0,sizeof(profile));
    request_status=scan_status=prepare_status=materialize_status=bind_status=-1;
    claim_created=0;outcome="not_started";report_len=0;caller_ops=0;source.fd=-1;
    if(!step()||fstat(root,&root_stat)||!S_ISDIR(root_stat.st_mode)||root_stat.st_uid!=geteuid())return 10;
    if((root_stat.st_mode&0777)!=0700) {
        if(!step()||fchmod(root,0700)||!step()||fsync(root)||!step()||fstat(root,&root_stat)||
           !S_ISDIR(root_stat.st_mode)||root_stat.st_uid!=geteuid()||(root_stat.st_mode&0777)!=0700)return 10;
    }
    root_ok=1;
    phase="request";
    if(!read_small(root,"request.v1",request_buffer,sizeof(request_buffer),&request_len,request_sha256))return 11;
    memset(&request_ref,0,sizeof(request_ref));
    request_status=rc=mb_archive_request_ref_decode_v1(request_buffer,request_len,&request_ref);
    if(rc)return 12;
    if(!read_small(root,request_ref.context_ref,context_buffer,sizeof(context_buffer),&context_len,context_sha256))return 13;
    request_status=rc=mb_archive_request_decode_v1(request_buffer,request_len,context_buffer,context_len,context_sha256,&request_local);
    if(rc)return 14;
    decoded=request_local;
    if(decoded.source_bytes>raw_cap)return 15;
    phase="source_open";
    if(!step())return 16;
    source_fd=mb_archive_posix_openat(root,decoded.source_ref,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
    if(source_fd<0)return 17;
    if(!step()||fstat(source_fd,&source_stat)||!S_ISREG(source_stat.st_mode)||source_stat.st_size<0){(void)close_fd(&source_fd);return 18;}
    if(!absent(root,decoded.claim_ref)||!absent(root,decoded.quarantine_ref)||!absent(root,decoded.receipt_ref)){(void)close_fd(&source_fd);return 19;}
    if(!create_claim(root)){(void)close_fd(&source_fd);return errno==EEXIST?20:21;}
    phase="scan";
    source.struct_size=sizeof(source);source.abi_version=MB_ARCHIVE_ABI_V1;source.fd=source_fd;
    source.size=decoded.source_bytes;memcpy(source.sha256,decoded.source_sha256,32);memcpy(source.target_token,decoded.target_token,32);
    mb_archive_limits_default_v1(&limits);limits.raw_bytes=raw_cap;limits.member_bytes=expanded_cap;
    limits.expanded_bytes=expanded_cap;limits.entries=ROWS;limits.name_bytes=NAMES;
    mb_archive_budget_default_v1(&budget);budget.io_limit=UINT64_C(2)<<30;budget.operation_limit=65536;
    budget.path_work_limit=UINT64_C(16)<<20;budget.io_used=CALLER_IO_BYTES;budget.operations_used=CALLER_OPERATIONS;
    scan_status=rc=mb_archive_scan_v1(&source,&limits,&budget,&first,&inventory);
    profile.struct_size=sizeof(profile);profile.abi_version=MB_ARCHIVE_PREPARED_ABI_V1;
    memcpy(profile.target_token,decoded.target_token,32);memcpy(profile.nonce,decoded.nonce,sizeof(profile.nonce));
    memcpy(profile.prefix,decoded.prefix,sizeof(profile.prefix));memcpy(profile.suffix,decoded.suffix,sizeof(profile.suffix));
    if(!rc) {
        view.inventory=&inventory;view.rows=rows;view.row_capacity=ROWS;view.names=names;view.name_capacity=NAMES;
        phase="prepare";prepare_status=rc=mb_archive_prepare_groups_v1(&view,&profile,&workspace,selected,ROWS,candidate_files,ROWS,&candidate);
        if(!rc&&!candidate.file_count){outcome="no_container_candidate";phase="no_candidate";rc=0;goto done;}
        if(!rc) {
            phase="quarantine";
            if(!step()||mb_archive_posix_mkdirat(root,decoded.quarantine_ref,0700)){rc=30;goto done;}
            if(!step()){rc=30;goto done;}qfd=mb_archive_posix_openat(root,decoded.quarantine_ref,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
            if(qfd<0||!step()||fstat(qfd,&qstat)||!S_ISDIR(qstat.st_mode)||qstat.st_uid!=geteuid()||(qstat.st_mode&077)){rc=31;goto done;}
            materialize_request.source=&source;materialize_request.inventory=&inventory;materialize_request.rows=rows;
            materialize_request.row_capacity=ROWS;materialize_request.names=names;materialize_request.name_capacity=NAMES;
            materialize_request.selected=selected;materialize_request.selected_count=candidate.file_count;
            memcpy(materialize_request.target_token,decoded.target_token,32);materialize_request.quarantine_fd=qfd;
            materialize_request.quarantine_device=(uint64_t)qstat.st_dev;materialize_request.quarantine_inode=(uint64_t)qstat.st_ino;
            materialize_request.journal=journal;materialize_request.journal_capacity=ROWS+3;materialize_request.replay=replay;
            phase="materialize";materialize_status=rc=mb_archive_materialize_v1(&materialize_request,&limits,&budget,&materialized);
            if(!rc) {
                binding.result=&materialized;binding.journal=journal;binding.journal_capacity=ROWS+3;
                binding.quarantine_device=(uint64_t)qstat.st_dev;binding.quarantine_inode=(uint64_t)qstat.st_ino;
                phase="bind";bind_status=rc=mb_archive_bind_prepared_v1(&view,&profile,&workspace,&candidate,
                    candidate_files,ROWS,&binding,bound_files,ROWS,&bound);
                if(!rc){outcome="prepared_quarantine";phase="complete";}
            }
        }
    }
done:
    if(!rc&&strcmp(outcome,"no_container_candidate")&&strcmp(outcome,"prepared_quarantine"))outcome="prepared_quarantine";
    if(rc&&strcmp(outcome,"no_container_candidate"))outcome="failed";
    if(!close_fd(&source_fd))cleanup_ok=0;
    if(!close_fd(&qfd))cleanup_ok=0;
    if(!cleanup_ok&&!rc){rc=32;phase="cleanup";outcome="failed";}
    make_report(rc,phase,cleanup_ok);
    if(root_ok)report_ok=save_report(root);
    if(!report_ok)return 40+report_save_failure;
    return (rc==0&&cleanup_ok&&report_ok&&(bound.complete||strcmp(outcome,"no_container_candidate")==0))?0:1;
}
int main(int argc,char **argv) {
    int root=-1,rc;const char *root_path=INBOX_ROOT;
#ifdef MB_ARCHIVE_REQUEST_HOST_ROOT_ARG
    if(argc!=2)return 2;root_path=argv[1];
#else
    (void)argv;
    if(argc!=1)return 2;
#endif
    root=mb_archive_posix_openat(AT_FDCWD,root_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
    if(root<0)return 3;
    rc=process_root(root);(void)close(root);return rc;
}
