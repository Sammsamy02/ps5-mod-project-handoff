#include "private_iostore_job.hpp"
extern "C" {
#include "../archive/private/posix_compat.h"
#include "toc_reader.h"
#include "container_header_merge.h"
}
#include <picosha2/picosha2.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <vector>

namespace modbridge {
namespace {
constexpr uint32_t rows_cap=512,names_cap=262144;
constexpr uint32_t member_cap=MB_IOSTORE_ARCHIVE_MAX_PROBES+2;
constexpr uint64_t archive_cap=UINT64_C(512)<<20;
bool clock_ms(uint64_t& value) noexcept {
    struct timespec ts{};
    if(clock_gettime(CLOCK_MONOTONIC,&ts)||ts.tv_sec<0||ts.tv_nsec<0||ts.tv_nsec>=1000000000||
       static_cast<uint64_t>(ts.tv_sec)>(UINT64_MAX-999)/1000)return false;
    value=static_cast<uint64_t>(ts.tv_sec)*1000+static_cast<uint64_t>(ts.tv_nsec)/1000000;return true;
}
bool identity(const struct stat& a,const struct stat& b) noexcept {return a.st_dev==b.st_dev&&a.st_ino==b.st_ino;}
bool stable(const struct stat& a,const struct stat& b) noexcept {
    return identity(a,b)&&a.st_mode==b.st_mode&&a.st_size==b.st_size&&a.st_nlink==b.st_nlink&&
#if defined(__APPLE__)
        a.st_mtimespec.tv_sec==b.st_mtimespec.tv_sec&&a.st_mtimespec.tv_nsec==b.st_mtimespec.tv_nsec&&
        a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec&&a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec;
#else
        a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&
        a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
#endif
}
bool private_directory(const struct stat& s) noexcept {return S_ISDIR(s.st_mode)&&s.st_uid==geteuid()&&(s.st_mode&07777)==0700;}
bool private_file(const struct stat& s) noexcept {return S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&(s.st_mode&07777)==0600&&s.st_nlink==1&&s.st_size>=0;}
bool readonly(int fd,uint64_t bytes,struct stat& s) noexcept {
    int flags=fd>=0?fcntl(fd,F_GETFL):-1;
    return flags>=0&&(flags&O_ACCMODE)==O_RDONLY&&!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_size>=0&&static_cast<uint64_t>(s.st_size)==bytes;
}
bool stable_fd(int fd,const struct stat& before) noexcept {struct stat after{};return !fstat(fd,&after)&&stable(before,after);}
bool charge(mb_archive_budget_v1 *budget,uint64_t bytes,uint64_t operations,bool *exhausted=nullptr) noexcept {
    if(!budget)return true;
    if(budget->io_used>budget->io_limit||bytes>budget->io_limit-budget->io_used||
       budget->operations_used>budget->operation_limit||operations>budget->operation_limit-budget->operations_used) {
        if(exhausted)*exhausted=true;return false;
    }
    budget->io_used+=bytes;budget->operations_used+=operations;return true;
}
bool read_bytes(int fd,uint8_t *out,uint64_t bytes,mb_archive_cancel_v1 gate,void *context,
    mb_archive_budget_v1 *budget=nullptr,bool *exhausted=nullptr) noexcept {
    uint64_t at=0;uint32_t calls=0;
    while(at<bytes) {
        if((gate&&gate(context))||++calls>32768)return false;
        size_t n=static_cast<size_t>(std::min<uint64_t>(65536,bytes-at));
        if(!charge(budget,n,1,exhausted))return false;
        auto actual=pread(fd,out+at,n,static_cast<off_t>(at));
        if(gate&&gate(context))return false;
        if(actual<0&&errno==EINTR)continue;
        if(actual<=0||static_cast<size_t>(actual)>n)return false;
        at+=static_cast<uint64_t>(actual);
    }
    return !(gate&&gate(context));
}
bool hash_fd(int fd,uint64_t bytes,std::array<uint8_t,32>& digest,mb_archive_cancel_v1 gate,void *context,
    mb_archive_budget_v1 *budget=nullptr,bool *exhausted=nullptr) {
    std::array<uint8_t,65536> chunk{};picosha2::hash256_one_by_one hash;hash.init();
    uint64_t at=0;uint32_t calls=0;
    while(at<bytes) {
        if((gate&&gate(context))||++calls>32768)return false;
        size_t n=static_cast<size_t>(std::min<uint64_t>(chunk.size(),bytes-at));
        if(!charge(budget,n,1,exhausted))return false;
        auto actual=pread(fd,chunk.data(),n,static_cast<off_t>(at));
        if(gate&&gate(context))return false;
        if(actual<0&&errno==EINTR)continue;
        if(actual<=0||static_cast<size_t>(actual)>n)return false;
        hash.process(chunk.begin(),chunk.begin()+actual);at+=static_cast<uint64_t>(actual);
    }
    hash.finish();hash.get_hash_bytes(digest.begin(),digest.end());return !(gate&&gate(context));
}
struct Admission {
    uint64_t deadline=0,last=0;bool terminal=false;
    static int check(void *context) noexcept {
        auto& g=*static_cast<Admission*>(context);uint64_t now=0;
        if(g.terminal||!clock_ms(now)||now<g.last||now>=g.deadline)g.terminal=true;
        g.last=now;return g.terminal;
    }
};
}
struct PrivateIoStoreJob::Workspace {
    mb_archive_row_v1 rows[rows_cap]{},replay_rows[rows_cap]{};
    char names[names_cap]{},replay_names[names_cap]{},group_keys[names_cap]{};
    uint32_t sort[rows_cap]{},replay_sort[rows_cap]{},group_sort[rows_cap]{},selected[rows_cap]{};
    uint8_t transfer[65536]{},replay_transfer[65536]{},probe_scratch[65536]{},primary_scratch[65536]{};
    mb_archive_group_v1 groups[rows_cap]{};
    mb_archive_group_member_v1 group_members[rows_cap]{};
    mb_iostore_archive_member_v1 members[rows_cap]{};
    mb_archive_budget_v1 budget{};
    bool budget_exhausted=false;
    uint64_t written[MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT]{};
    picosha2::hash256_one_by_one stream[MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT];
    Workspace(){for(auto& h:stream)h.init();}
};
struct PrivateIoStoreJob::Source {
    mb_archive_source_v1 archive{};
    struct stat snapshot{},directory_stat{};
    int directory=-1,mod_toc=-1,mod_ucas=-1;
    bool owned=false;
    char name[48]{};
    mb_archive_owned_file_v1 journal[member_cap+3]{};
    int member_fds[member_cap];uint32_t opened=0;
    struct stat member_stats[member_cap]{};
    Source(){for(auto& fd:member_fds)fd=-1;}
};
bool admit_captured_iostore_base(int toc_fd,uint64_t toc_bytes,const uint8_t toc_sha[32],int header_fd,
    uint64_t header_bytes,const uint8_t header_sha[32],uint64_t observed_ucas_bytes,uint32_t timeout_ms,CapturedIoStoreBase& admitted) noexcept {
    if(!toc_sha||!header_sha||!toc_bytes||toc_bytes>MB_IOSTORE_MAX_TOC_BYTES||!header_bytes||header_bytes>CCH_HEADER_BYTES_CAP||
       !observed_ucas_bytes||observed_ucas_bytes>INT64_MAX||!timeout_ms||timeout_ms>300000)return false;
    struct stat toc{},header{};
    if(!readonly(toc_fd,toc_bytes,toc)||!readonly(header_fd,header_bytes,header)||identity(toc,header))return false;
    uint64_t now=0;if(!clock_ms(now)||now>UINT64_MAX-timeout_ms)return false;
    Admission gate{now+timeout_ms,now,false};CapturedIoStoreBase candidate;
    try {
        if(!hash_fd(toc_fd,toc_bytes,candidate.toc_sha,Admission::check,&gate)||
           !hash_fd(header_fd,header_bytes,candidate.header_sha,Admission::check,&gate)||
           std::memcmp(candidate.toc_sha.data(),toc_sha,32)||std::memcmp(candidate.header_sha.data(),header_sha,32)||
           !stable_fd(toc_fd,toc)||!stable_fd(header_fd,header)||Admission::check(&gate))return false;
        candidate.toc_fd=toc_fd;candidate.header_fd=header_fd;candidate.toc_bytes=toc_bytes;candidate.header_bytes=header_bytes;
        candidate.observed_ucas_bytes=observed_ucas_bytes;candidate.toc_snapshot=toc;candidate.header_snapshot=header;
        std::array<uint8_t,72> binding{};
        std::copy(candidate.toc_sha.begin(),candidate.toc_sha.end(),binding.begin());
        std::copy(candidate.header_sha.begin(),candidate.header_sha.end(),binding.begin()+32);
        for(unsigned i=0;i<8;++i)binding[64+i]=static_cast<uint8_t>(observed_ucas_bytes>>(8*i));
        picosha2::hash256(binding.begin(),binding.end(),candidate.token.begin(),candidate.token.end());
        admitted=candidate;return true;
    }catch(...){return false;}
}
PrivateIoStoreJob::PrivateIoStoreJob(int directory,const char id[33],const CapturedIoStoreBase& captured,uint32_t timeout_ms)
    :root(directory),base(captured),timeout(timeout_ms) {
    std::snprintf(q_name,sizeof(q_name),"mb-io-%s-q",id);std::snprintf(out_name,sizeof(out_name),"mb-io-%s-out",id);
    writer_fds.fill(-1);
    if(fstat(root,&root_stat)||!private_directory(root_stat))state=State::StorageFailed;
}
PrivateIoStoreJob::~PrivateIoStoreJob()=default;
int PrivateIoStoreJob::cancelled(void *context) noexcept {return static_cast<PrivateIoStoreJob*>(context)->stopped();}
int PrivateIoStoreJob::continuing(void *context) noexcept {return !static_cast<PrivateIoStoreJob*>(context)->stopped();}
bool PrivateIoStoreJob::stopped() noexcept {
    uint64_t now=0;if(terminal||!clock_ms(now)||now<last||now>=deadline)terminal=true;last=now;return terminal||(workspace&&workspace->budget_exhausted);
}
bool PrivateIoStoreJob::close_owned(int& fd) noexcept {
    if(fd<0)return true;int previous=fd;fd=-1;
    if(!close(previous))return true;uncertain_close=true;state=State::StorageFailed;return false;
}
bool PrivateIoStoreJob::directories_valid() const noexcept {
    struct stat observed{},named{};
    if(fstat(root,&observed)||!private_directory(observed)||!identity(root_stat,observed))return false;
    for(const auto& source:sources)if(source&&source->owned&&
       (source->directory<0||fstat(source->directory,&observed)||!private_directory(observed)||!identity(source->directory_stat,observed)||
        mb_archive_posix_fstatat(root,source->name,&named,AT_SYMLINK_NOFOLLOW)||!private_directory(named)||!identity(source->directory_stat,named)))return false;
    if(out_owned&&(destination<0||fstat(destination,&observed)||!private_directory(observed)||!identity(out_stat,observed)||
       mb_archive_posix_fstatat(root,out_name,&named,AT_SYMLINK_NOFOLLOW)||!private_directory(named)||!identity(out_stat,named)))return false;
    return true;
}
bool PrivateIoStoreJob::create_directory(const char *name,int& fd,struct stat& bound,bool& owned) noexcept {
    if(stopped()||!directories_valid())return false;
    if(mb_archive_posix_mkdirat(root,name,0700)){state=State::StorageFailed;return false;}owned=true;
    if(mb_archive_posix_fstatat(root,name,&bound,AT_SYMLINK_NOFOLLOW)||!private_directory(bound)){state=State::StorageFailed;return false;}
    fd=mb_archive_posix_openat(root,name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
    if(fd<0||!directories_valid()){state=State::StorageFailed;return false;}
    return !stopped();
}
int PrivateIoStoreJob::artifact_event(void *context,uint32_t index,int fd,uint32_t event) noexcept {
    auto& job=*static_cast<PrivateIoStoreJob*>(context);
    if(index>=MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT)return 0;
    if(event==MB_IOSTORE_FILE_JOB_ARTIFACT_CREATED) {
        job.created[index]=true;job.writer_fds[index]=fd;
        if(fstat(fd,&job.artifacts[index])||!private_file(job.artifacts[index])||job.artifacts[index].st_size) {
            job.state=State::StorageFailed;return 0;
        }
        return 1;
    }
    if(event==MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSE_FAILED||job.writer_fds[index]!=fd) {
        job.uncertain_close=true;job.state=State::StorageFailed;return 0;
    }
    if(event==MB_IOSTORE_FILE_JOB_ARTIFACT_CLOSED){job.closed[index]=true;job.writer_fds[index]=-1;return 1;}
    return 0;
}
int PrivateIoStoreJob::write_at(void *context,int fd,uint64_t offset,const uint8_t *data,uint32_t bytes,uint32_t *actual) noexcept {
    *actual=0;auto& job=*static_cast<PrivateIoStoreJob*>(context);
    if(!job.workspace||offset>INT64_MAX||bytes>INT64_MAX-offset)return 1;
    uint32_t index=0;for(;index<MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT;++index)if(job.writer_fds[index]==fd)break;
    if(index==MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT||offset!=job.workspace->written[index])return 1;
    ssize_t n=-1;unsigned attempts=0;
    do {
        if(job.stopped()||++attempts>32||!charge(&job.workspace->budget,bytes,1,&job.workspace->budget_exhausted))return 1;
        n=pwrite(fd,data,bytes,static_cast<off_t>(offset));
    }while(n<0&&errno==EINTR);
    if(n<=0||static_cast<uint64_t>(n)>bytes)return 1;
    try {job.workspace->stream[index].process(data,data+n);}catch(...){return 1;}
    job.workspace->written[index]+=static_cast<uint64_t>(n);*actual=static_cast<uint32_t>(n);return 0;
}
int PrivateIoStoreJob::read_at(void *context,int fd,uint64_t offset,uint8_t *data,uint32_t bytes,uint32_t *actual) noexcept {
    *actual=0;auto& job=*static_cast<PrivateIoStoreJob*>(context);
    if(!job.workspace||offset>INT64_MAX||bytes>INT64_MAX-offset)return 1;
    uint32_t at=0,attempts=0;
    while(at<bytes) {
        if(job.stopped()||++attempts>32||!charge(&job.workspace->budget,bytes-at,1,&job.workspace->budget_exhausted))return 1;
        auto n=pread(fd,data+at,bytes-at,static_cast<off_t>(offset+at));
        if(job.stopped())return 1;
        if(n<0&&errno==EINTR)continue;
        if(n<=0||static_cast<uint64_t>(n)>bytes-at)return 1;
        at+=static_cast<uint32_t>(n);
    }
    *actual=at;return 0;
}
void PrivateIoStoreJob::clear_completion() noexcept {
    artifact_complete=false;readback_observed=false;whole_bundle_handled=false;artifact_bytes.fill(0);
    for(auto& sha:artifact_sha)sha.fill(0);
}
void PrivateIoStoreJob::prepare(const mb_archive_source_v1& source) noexcept {
    prepare_sources(&source,1,false);
}
void PrivateIoStoreJob::prepare_batch(const mb_archive_source_v1 *original,uint32_t count) noexcept {
    prepare_sources(original,count,true);
}
bool PrivateIoStoreJob::sources_stable() const noexcept {
    for(uint32_t i=0;i<source_count;++i)
        if(!sources[i]||!stable_fd(sources[i]->archive.fd,sources[i]->snapshot))return false;
    return true;
}
bool PrivateIoStoreJob::charge_paths(uint64_t work) noexcept {
    if(!workspace)return false;auto& budget=workspace->budget;
    if(budget.path_work_used>budget.path_work_limit||work>budget.path_work_limit-budget.path_work_used) {
        workspace->budget_exhausted=true;return false;
    }
    budget.path_work_used+=work;return !stopped();
}
void PrivateIoStoreJob::prepare_sources(const mb_archive_source_v1 *original,uint32_t count,bool batch) noexcept {
    // Known source/owner aliases refuse without changing either backing object.
    const auto first=reinterpret_cast<uintptr_t>(original),owner=reinterpret_cast<uintptr_t>(this);
    const size_t bytes=count&&count<=MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES?count*sizeof(*original):sizeof(*original);
    if(original&&bytes&&(first>UINTPTR_MAX-bytes||owner>UINTPTR_MAX-sizeof(*this)||
       (first<owner+sizeof(*this)&&owner<first+bytes)))return;
    if(attempted||state==State::StorageFailed)return;
    attempted=true;batch_mode=batch;
    if(!original||!count||count>MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES||!timeout||timeout>300000) {
        std::snprintf(error,sizeof(error),"source_count_or_arguments_refused");return;
    }
    source_count=count;bool prepared=false;
    try {
        uint64_t now=0;
        if(!clock_ms(now)||now>UINT64_MAX-timeout){state=State::Stopped;return;}
        last=now;deadline=now+timeout;
        workspace=std::make_unique<Workspace>();
        mb_archive_budget_default_v1(&workspace->budget);
        workspace->budget.io_limit=(batch?UINT64_C(16):UINT64_C(8))<<30;
        workspace->budget.operation_limit=1000000;
        workspace->budget.path_work_limit=(batch?UINT64_C(128):UINT64_C(16))<<20;
        workspace->budget.cancelled=cancelled;workspace->budget.context=this;
        workspace->budget.deadline_ms=deadline;
        bool admitted=true;
        for(uint32_t i=0;i<count;++i) {
            sources[i]=std::make_unique<Source>();auto& source=*sources[i];
            source.archive=original[i];std::copy(base.token.begin(),base.token.end(),source.archive.target_token);
            int name_bytes=batch?std::snprintf(source.name,sizeof(source.name),"%s%u",q_name,i):
                                 std::snprintf(source.name,sizeof(source.name),"%s",q_name);
            if(name_bytes<=0||static_cast<size_t>(name_bytes)>=sizeof(source.name)||stopped()||
               original[i].struct_size!=sizeof(original[i])||original[i].abi_version!=MB_ARCHIVE_ABI_V1||original[i].reserved||
               !original[i].size||original[i].size>archive_cap||!readonly(original[i].fd,original[i].size,source.snapshot)||
               identity(source.snapshot,base.toc_snapshot)||identity(source.snapshot,base.header_snapshot)) {
                admitted=false;std::snprintf(stage,sizeof(stage),"input_open");
                std::snprintf(error,sizeof(error),"readonly_source_or_captured_base_refused");break;
            }
            for(uint32_t j=0;j<i;++j)if(identity(source.snapshot,sources[j]->snapshot)) {
                state=State::Conflict;admitted=false;
                batch_result.struct_size=sizeof(batch_result);batch_result.abi_version=MB_IOSTORE_FILE_JOB_ABI_V1;
                batch_result.source_count=count;batch_result.conflict_kind=MB_IOSTORE_BATCH_CONFLICT_SOURCE_IDENTITY;
                batch_result.conflict_source_a=j;batch_result.conflict_source_b=i;
                batch_result.preparation.struct_size=sizeof(result);batch_result.preparation.abi_version=MB_IOSTORE_FILE_JOB_ABI_V1;
                batch_result.preparation.status=MB_IOSTORE_FILE_JOB_CONFLICT;result=batch_result.preparation;
                std::snprintf(stage,sizeof(stage),"input_open");std::snprintf(error,sizeof(error),"repeated_source_identity");break;
            }
            if(!admitted)break;
        }
        if(admitted&&body())prepared=true;
    }catch(...){state=State::Refused;std::snprintf(error,sizeof(error),"owner_exception");}
    for(auto& source:sources)if(source)
        for(uint32_t i=0;i<source->opened;++i)if(!close_owned(source->member_fds[i]))prepared=false;
    if(prepared&&finalize()){state=State::Prepared;artifact_complete=true;whole_bundle_handled=true;std::snprintf(stage,sizeof(stage),"complete");}
    else if(state!=State::StorageFailed&&state!=State::AdditionalActionRequired&&state!=State::Conflict)
        state=terminal?State::Stopped:State::Refused;
    if(state!=State::Prepared) {
        clear_completion();result.prepared=0;batch_result.preparation.prepared=0;
        if(workspace&&workspace->budget_exhausted)std::snprintf(error,sizeof(error),"resource_budget_exhausted");
    }
}
bool PrivateIoStoreJob::intake(uint32_t source_index) {
    auto refuse=[&](const char *at,const char *why)->bool {
        std::snprintf(stage,sizeof(stage),"%s",at);std::snprintf(error,sizeof(error),"%s",why);return false;
    };
    auto& input=*sources[source_index];const auto& source=input.archive;auto& w=*workspace;
    if(stopped()||!stable_fd(source.fd,input.snapshot)||!stable_fd(base.toc_fd,base.toc_snapshot)||
       !stable_fd(base.header_fd,base.header_snapshot)||!directories_valid())
        return refuse("input_open","readonly_source_or_captured_base_refused");
    mb_archive_limits_v1 limits{};mb_archive_limits_default_v1(&limits);
    limits.raw_bytes=limits.member_bytes=archive_cap;limits.expanded_bytes=UINT64_C(1)<<30;
    limits.entries=rows_cap;limits.name_bytes=names_cap;auto& budget=w.budget;
    mb_archive_buffers_v1 scan{w.rows,rows_cap,w.names,names_cap,w.sort,rows_cap,w.transfer,sizeof(w.transfer)};
    mb_archive_inventory_v1 inventory{};
    std::snprintf(stage,sizeof(stage),"archive_scan");
    if(stopped()||mb_archive_scan_v1(&source,&limits,&budget,&scan,&inventory)!=MB_ARCHIVE_OK||inventory.complete!=1||
       inventory.installation_supported||stopped()||!stable_fd(source.fd,input.snapshot))return refuse("archive_scan","scan_incomplete_or_source_changed");
    mb_archive_prepared_view_v1 view{&inventory,w.rows,rows_cap,w.names,names_cap};
    mb_iostore_archive_workspace_v1 scope{sizeof(scope),MB_IOSTORE_ARCHIVE_SELECTION_ABI_V1,
        {w.group_sort,rows_cap,w.groups,rows_cap,w.group_members,rows_cap,w.group_keys,names_cap},
        w.probe_scratch,sizeof(w.probe_scratch),w.primary_scratch,sizeof(w.primary_scratch)};
    mb_iostore_archive_selection_v1 staged_selection{};
    auto selected=mb_iostore_archive_select_v1(&view,nullptr,0,&scope,w.selected,rows_cap,w.members,rows_cap,&staged_selection);
    source_selections[source_index]=staged_selection;
    if(!batch_mode)selection=staged_selection;
    if(!charge_paths(staged_selection.path_work_used))return refuse("archive_selection","stopped_or_path_budget");
    if(selected!=MB_IOSTORE_ARCHIVE_OK&&selected!=MB_IOSTORE_ARCHIVE_PROBES_REQUIRED) {
        state=State::AdditionalActionRequired;
        for(uint32_t i=0;i<inventory.row_count;++i)if(w.rows[i].kind==MB_ARCHIVE_REGULAR)++unhandled;
        return refuse("archive_selection","one_supported_iostore_scope_required");
    }
    const uint32_t count=staged_selection.member_count;
    if(staged_selection.selection_complete!=1||!count||count>member_cap||std::memcmp(staged_selection.source_sha256,source.sha256,32)||
       std::memcmp(staged_selection.inventory_sha256,inventory.inventory_sha256,32)||std::memcmp(staged_selection.target_token,source.target_token,32))
        return refuse("archive_selection","selection_identity_refused");
    if(!create_directory(input.name,input.directory,input.directory_stat,input.owned))return refuse("quarantine_create","private_owner_refused");
    mb_archive_materialize_request_v1 request{};
    request.source=&source;request.inventory=&inventory;request.rows=w.rows;request.row_capacity=rows_cap;
    request.names=w.names;request.name_capacity=names_cap;request.selected=w.selected;request.selected_count=count;
    std::copy(base.token.begin(),base.token.end(),request.target_token);
    request.quarantine_fd=input.directory;request.quarantine_device=static_cast<uint64_t>(input.directory_stat.st_dev);request.quarantine_inode=static_cast<uint64_t>(input.directory_stat.st_ino);
    request.journal=input.journal;request.journal_capacity=member_cap+3;
    request.replay={w.replay_rows,rows_cap,w.replay_names,names_cap,w.replay_sort,rows_cap,w.replay_transfer,sizeof(w.replay_transfer)};
    mb_archive_materialized_v1 materialized{};
    std::snprintf(stage,sizeof(stage),"archive_materialize");
    if(stopped()||mb_archive_materialize_v1(&request,&limits,&budget,&materialized)!=MB_ARCHIVE_OK||stopped()||materialized.complete!=1||
       materialized.selected_count!=count||materialized.journal_count>member_cap+3||std::memcmp(materialized.inventory_sha256,inventory.inventory_sha256,32)||
       std::memcmp(materialized.target_token,source.target_token,32)||!directories_valid())return refuse("archive_materialize","materialization_incomplete");
    std::array<mb_iostore_archive_probe_v1,MB_IOSTORE_ARCHIVE_MAX_PROBES> probes{};
    std::array<std::vector<uint8_t>,MB_IOSTORE_ARCHIVE_MAX_PROBES> probe_bytes;
    uint32_t probe_count=0;int mod_toc=-1,mod_ucas=-1;
    for(uint32_t i=0;i<count;++i) {
        const auto& m=w.members[i];const mb_archive_owned_file_v1 *owned=nullptr;
        for(uint32_t j=0;j<materialized.journal_count;++j)if(input.journal[j].row_index==m.row_index&&input.journal[j].state==MB_ARCHIVE_CREATED) {
            if(owned)return refuse("member_open","ambiguous_owned_member");owned=&input.journal[j];
        }
        if(stopped()||!owned||!std::memchr(owned->name,0,sizeof(owned->name))||!owned->name[0]||std::strchr(owned->name,'/')||
           owned->expected_size!=m.bytes||owned->written_bytes!=m.bytes||std::memcmp(owned->sha256,m.sha256,32))return refuse("member_open","materialized_binding_refused");
        int fd=mb_archive_posix_openat(input.directory,owned->name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
        if(fd<0)return refuse("member_open","bound_member_open_failed");
        uint32_t index=input.opened++;input.member_fds[index]=fd;
        auto& snapshot=input.member_stats[index];std::array<uint8_t,32> hash{};
        if(!readonly(fd,m.bytes,snapshot)||!private_file(snapshot)||static_cast<uint64_t>(snapshot.st_dev)!=owned->device||
           static_cast<uint64_t>(snapshot.st_ino)!=owned->inode||!hash_fd(fd,m.bytes,hash,cancelled,this,&budget,&w.budget_exhausted)||
           std::memcmp(hash.data(),m.sha256,32)||!stable_fd(fd,snapshot))return refuse("member_open","member_identity_or_sha_refused");
        if(m.row_index==staged_selection.toc_row)mod_toc=fd;
        if(m.row_index==staged_selection.ucas_row)mod_ucas=fd;
        if(m.probe_required) {
            uint64_t cap=m.role==MB_IOSTORE_ARCHIVE_ROLE_PAK_COMPANION?MB_IOSTORE_ARCHIVE_MAX_EMPTY_PAK_BYTES:MB_IOSTORE_ARCHIVE_MAX_DOCUMENT_BYTES;
            if(probe_count==MB_IOSTORE_ARCHIVE_MAX_PROBES||m.bytes>cap)return refuse("member_probe","probe_limit");
            auto& buffer=probe_bytes[probe_count];buffer.resize(static_cast<size_t>(m.bytes));
            if(!read_bytes(fd,buffer.data(),m.bytes,cancelled,this,&budget,&w.budget_exhausted)||!stable_fd(fd,snapshot))return refuse("member_probe","probe_read_failed");
            probes[probe_count++]={m.row_index,0,buffer.data(),buffer.size()};
        }
    }
    selected=mb_iostore_archive_select_v1(&view,probes.data(),probe_count,&scope,w.selected,rows_cap,w.members,rows_cap,&staged_selection);
    source_selections[source_index]=staged_selection;
    if(!batch_mode)selection=staged_selection;
    if(stopped()||selected!=MB_IOSTORE_ARCHIVE_OK||staged_selection.preparation_ready!=1||staged_selection.selection_complete!=1||staged_selection.member_count!=count||
       mod_toc<0||mod_ucas<0||std::memcmp(staged_selection.inventory_sha256,inventory.inventory_sha256,32))return refuse("member_probe","companion_content_scope_refused");
    if(!charge_paths(staged_selection.path_work_used))return refuse("member_probe","stopped_or_path_budget");
    input.mod_toc=mod_toc;input.mod_ucas=mod_ucas;
    omitted_empty_paks+=staged_selection.omitted_empty_paks;omitted_documents+=staged_selection.omitted_documents;
    if(staged_selection.omitted_empty_paks)++sources_with_empty_paks;
    if(staged_selection.omitted_documents)++sources_with_documents;
    return true;
}
bool PrivateIoStoreJob::body() {
    auto refuse=[&](const char *at,const char *why)->bool {
        std::snprintf(stage,sizeof(stage),"%s",at);std::snprintf(error,sizeof(error),"%s",why);return false;
    };
    for(uint32_t i=0;i<source_count;++i)if(!intake(i))return false;
    if(stopped()||!sources_stable()||!create_directory(out_name,destination,out_stat,out_owned))
        return refuse("output_create","private_owner_refused");
    mb_iostore_file_job_control_v1 control{sizeof(control),MB_IOSTORE_FILE_JOB_CONTROL_ABI_V1,continuing,this,artifact_event,this};
    mb_iostore_file_job_status_v1 code=MB_IOSTORE_FILE_JOB_BAD_ARGUMENT;
    std::snprintf(stage,sizeof(stage),"container_prepare");
    if(batch_mode) {
        std::array<mb_iostore_batch_source_v1,MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES> inputs{};
        for(uint32_t i=0;i<source_count;++i)inputs[i]={sources[i]->mod_toc,sources[i]->mod_ucas};
        mb_iostore_batch_file_job_params_v1 params{};
        params.struct_size=sizeof(params);params.abi_version=MB_IOSTORE_FILE_JOB_ABI_V1;
        params.sources=inputs.data();params.source_count=source_count;
        params.base_toc_fd=base.toc_fd;params.base_header_fd=base.header_fd;params.destination_dir_fd=destination;
        params.immutable_inputs=params.private_unpublished_directory=1;params.observed_base_ucas_bytes=base.observed_ucas_bytes;
        params.io_context=this;params.read_at=read_at;params.write_at=write_at;
        mb_iostore_batch_file_job_result_v1 staged_result{};
        code=mb_iostore_batch_file_job_gated_v1(&params,&staged_result,&control);
        batch_result=staged_result;result=staged_result.preparation;
    } else {
        mb_iostore_file_job_params_v1 params{};params.struct_size=sizeof(params);params.abi_version=MB_IOSTORE_FILE_JOB_ABI_V1;
        params.mod_toc_fd=sources[0]->mod_toc;params.mod_ucas_fd=sources[0]->mod_ucas;
        params.base_toc_fd=base.toc_fd;params.base_header_fd=base.header_fd;params.destination_dir_fd=destination;
        params.immutable_inputs=params.private_unpublished_directory=1;params.observed_base_ucas_bytes=base.observed_ucas_bytes;
        params.io_context=this;params.read_at=read_at;params.write_at=write_at;
        mb_iostore_file_job_result_v1 staged_result{};
        code=mb_iostore_original_file_job_gated_v1(&params,&staged_result,&control);result=staged_result;
    }
    if(code==MB_IOSTORE_FILE_JOB_CONFLICT&&state!=State::StorageFailed) {
        state=State::Conflict;return refuse("container_prepare","strict_source_conflict");
    }
    if(code!=MB_IOSTORE_FILE_JOB_OK||!result.prepared||result.installation_authority||result.allocation_live_after_cleanup||
       result.allocations!=result.releases||stopped()||!sources_stable()||
       !stable_fd(base.toc_fd,base.toc_snapshot)||!stable_fd(base.header_fd,base.header_snapshot))
        return refuse("container_prepare","format_job_incomplete");
    for(const auto& input:sources)if(input)
        for(uint32_t i=0;i<input->opened;++i)if(!stable_fd(input->member_fds[i],input->member_stats[i]))
            return refuse("container_prepare","source_member_changed");
    return true;
}
bool PrivateIoStoreJob::finalize() noexcept {
    if(!workspace||uncertain_close||stopped()||!directories_valid())return false;
    try {
        for(uint32_t i=0;i<MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT;++i) {
            std::snprintf(stage,sizeof(stage),"private_readback");
            struct stat named{},opened{},after{};const auto *name=mb_iostore_file_job_artifact_name_v1(i);
            if(!created[i]||!closed[i]||!name||!result.artifact_bytes[i]||workspace->written[i]!=result.artifact_bytes[i]||stopped()||
               mb_archive_posix_fstatat(destination,name,&named,AT_SYMLINK_NOFOLLOW)||!private_file(named)||!identity(artifacts[i],named)||
               static_cast<uint64_t>(named.st_size)!=result.artifact_bytes[i])return false;
            artifacts[i]=named;
            readback_fd=mb_archive_posix_openat(destination,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
            if(readback_fd<0||!readonly(readback_fd,result.artifact_bytes[i],opened)||!stable(named,opened))return false;
            std::array<uint8_t,32> hash{},expected{};
            workspace->stream[i].finish();workspace->stream[i].get_hash_bytes(expected.begin(),expected.end());
            if(!hash_fd(readback_fd,result.artifact_bytes[i],hash,cancelled,this,&workspace->budget,&workspace->budget_exhausted)||hash!=expected||fstat(readback_fd,&after)||!stable(named,after)||
               !close_owned(readback_fd)||mb_archive_posix_fstatat(destination,name,&after,AT_SYMLINK_NOFOLLOW)||!stable(named,after))return false;
            artifact_sha[i]=hash;artifact_bytes[i]=result.artifact_bytes[i];
        }
        if(!directories_valid()||!sources_stable()||!stable_fd(base.toc_fd,base.toc_snapshot)||!stable_fd(base.header_fd,base.header_snapshot)||stopped())return false;
        readback_observed=true;return true;
    }catch(...){return false;}
}
bool PrivateIoStoreJob::cleanup() noexcept {
    clear_completion();bool okay=!uncertain_close;struct stat observed{};
    if(!close_owned(readback_fd))okay=false;
    for(auto& source:sources)if(source)
        for(uint32_t i=0;i<source->opened;++i)if(!close_owned(source->member_fds[i]))okay=false;
    if(!okay||!directories_valid())return false;
    if(out_owned) {
        for(uint32_t i=0;i<MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT;++i)if(created[i]) {
            const auto *name=mb_iostore_file_job_artifact_name_v1(i);
            if(!closed[i]||!artifacts[i].st_ino){okay=false;continue;}
            if(mb_archive_posix_fstatat(destination,name,&observed,AT_SYMLINK_NOFOLLOW)) {
                if(errno==ENOENT)created[i]=false;else okay=false;
            }else if(!private_file(observed)||!identity(artifacts[i],observed))okay=false;
            else if(mb_archive_posix_unlinkat(destination,name,0)&&errno!=ENOENT)okay=false;
            else created[i]=false;
        }
        if(okay&&directories_valid()) {
            if(mb_archive_posix_unlinkat(root,out_name,AT_REMOVEDIR)&&errno!=ENOENT)okay=false;else out_owned=false;
        }
    }
    if(!out_owned&&!close_owned(destination))okay=false;
    for(auto& source:sources)if(source) {
        if(source->owned) {
            for(size_t i=member_cap+3;i>0;--i) {
                auto& item=source->journal[i-1];
                if(item.state!=MB_ARCHIVE_CREATED&&item.state!=MB_ARCHIVE_RETAINED)continue;
                if(i==1&&!okay)continue;
                if(mb_archive_posix_fstatat(source->directory,item.name,&observed,AT_SYMLINK_NOFOLLOW)) {
                    if(errno==ENOENT)item.state=MB_ARCHIVE_REMOVED;else okay=false;
                }else if(!item.inode||!S_ISREG(observed.st_mode)||static_cast<uint64_t>(observed.st_dev)!=item.device||static_cast<uint64_t>(observed.st_ino)!=item.inode)okay=false;
                else if(mb_archive_posix_unlinkat(source->directory,item.name,0)&&errno!=ENOENT)okay=false;
                else item.state=MB_ARCHIVE_REMOVED;
            }
            if(okay&&directories_valid()) {
                if(mb_archive_posix_unlinkat(root,source->name,AT_REMOVEDIR)&&errno!=ENOENT)okay=false;else source->owned=false;
            }
        }
        if(!source->owned&&!close_owned(source->directory))okay=false;
    }
    if(!okay)state=State::StorageFailed;
    return okay&&!uncertain_close;
}
}
