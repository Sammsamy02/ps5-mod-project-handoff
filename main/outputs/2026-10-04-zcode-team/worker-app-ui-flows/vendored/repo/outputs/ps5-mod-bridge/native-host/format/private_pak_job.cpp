#include "private_pak_job.hpp"
extern "C" {
#include "../archive/private/posix_compat.h"
}
#include <picosha2/picosha2.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

namespace modbridge {
namespace {
bool monotonic(uint64_t& value) noexcept {
    struct timespec ts{};
    if(clock_gettime(CLOCK_MONOTONIC,&ts)||ts.tv_sec<0||ts.tv_nsec<0||ts.tv_nsec>=1000000000||
       static_cast<uint64_t>(ts.tv_sec)>(UINT64_MAX-999)/1000)return false;
    value=static_cast<uint64_t>(ts.tv_sec)*1000+static_cast<uint64_t>(ts.tv_nsec)/1000000;return true;
}
bool identity(const struct stat& a,const struct stat& b) noexcept {
    return a.st_dev==b.st_dev&&a.st_ino==b.st_ino;
}
bool snapshot(const struct stat& a,const struct stat& b) noexcept {
    return identity(a,b)&&a.st_size==b.st_size&&a.st_mode==b.st_mode&&a.st_nlink==b.st_nlink&&
#if defined(__APPLE__)
        a.st_mtimespec.tv_sec==b.st_mtimespec.tv_sec&&a.st_mtimespec.tv_nsec==b.st_mtimespec.tv_nsec&&
        a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec&&a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec;
#else
        a.st_mtim.tv_sec==b.st_mtim.tv_sec&&a.st_mtim.tv_nsec==b.st_mtim.tv_nsec&&
        a.st_ctim.tv_sec==b.st_ctim.tv_sec&&a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
#endif
}
bool private_file(const struct stat& s) noexcept {
    return S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&(s.st_mode&07777)==0600&&s.st_nlink==1&&s.st_size>=0;
}
bool private_directory(const struct stat& s) noexcept {
    return S_ISDIR(s.st_mode)&&s.st_uid==geteuid()&&(s.st_mode&07777)==0700;
}
bool hash_fd(int fd,uint64_t bytes,std::array<uint8_t,32>& digest,
             mb_pak_prepare_gate_v1 gate,void *context) {
    std::array<uint8_t,65536> chunk{};picosha2::hash256_one_by_one hash;hash.init();
    uint64_t offset=0;uint32_t attempts=0;
    while(offset<bytes) {
        if((gate&&gate(context))||++attempts>32768)return false;
        size_t request=static_cast<size_t>(std::min<uint64_t>(chunk.size(),bytes-offset));
        auto actual=pread(fd,chunk.data(),request,static_cast<off_t>(offset));
        if(gate&&gate(context))return false;
        if(actual<0&&errno==EINTR)continue;
        if(actual<=0||static_cast<size_t>(actual)>request)return false;
        hash.process(chunk.begin(),chunk.begin()+actual);offset+=static_cast<uint64_t>(actual);
    }
    hash.finish();hash.get_hash_bytes(digest.begin(),digest.end());return !(gate&&gate(context));
}
struct AdmissionGate {
    uint64_t deadline=0,last=0;bool terminal=false;
    static int check(void *context) noexcept {
        auto& g=*static_cast<AdmissionGate*>(context);uint64_t now=0;
        if(g.terminal||!monotonic(now)||now<g.last||now>=g.deadline)g.terminal=true;
        g.last=now;return g.terminal;
    }
};
}
bool admit_captured_pak_base(int fd,uint64_t bytes,const uint8_t sha[32],uint32_t timeout_ms,CapturedPakBase& admitted) noexcept {
    if(fd<0||!sha||!bytes||bytes>MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES||!timeout_ms||timeout_ms>300000)return false;
    struct stat before{},after{};int flags=fcntl(fd,F_GETFL);
    if(flags<0||(flags&O_ACCMODE)!=O_RDONLY||fstat(fd,&before)||!S_ISREG(before.st_mode)||
       before.st_size<0||static_cast<uint64_t>(before.st_size)!=bytes)return false;
    uint64_t now=0;if(!monotonic(now)||now>UINT64_MAX-timeout_ms)return false;
    AdmissionGate gate{now+timeout_ms,now,false};std::array<uint8_t,32> observed{};
    try {
        if(!hash_fd(fd,bytes,observed,AdmissionGate::check,&gate)||
           std::memcmp(observed.data(),sha,32)||fstat(fd,&after)||!snapshot(before,after)||AdmissionGate::check(&gate))return false;
        CapturedPakBase candidate;candidate.fd=fd;candidate.bytes=bytes;candidate.sha=observed;candidate.snapshot=before;
        admitted=candidate;return true;
    }catch(...){return false;}
}
PrivatePakJob::PrivatePakJob(int directory,const char id[33],const CapturedPakBase& captured,uint32_t timeout_ms)
    :root(directory),base(captured),timeout(timeout_ms) {
    std::snprintf(q_name,sizeof(q_name),"mb-pak-%s",id);
    std::snprintf(out_name,sizeof(out_name),"mb-pak-%s.pak",id);
    if(fstat(root,&root_stat)||!private_directory(root_stat))state=State::StorageFailed;
}
int PrivatePakJob::gate(void *context) noexcept {return static_cast<PrivatePakJob*>(context)->stopped();}
bool PrivatePakJob::hash_file(int fd,uint64_t bytes,std::array<uint8_t,32>& digest,
                             mb_pak_prepare_gate_v1 predicate,void *context) noexcept {
    try {return hash_fd(fd,bytes,digest,predicate,context);}catch(...){return false;}
}
bool PrivatePakJob::stable_input(int fd,const struct stat& before) noexcept {
    struct stat after{};return fd>=0&&!fstat(fd,&after)&&S_ISREG(after.st_mode)&&snapshot(before,after);
}
bool PrivatePakJob::stopped() noexcept {
    uint64_t now=0;
    if(terminal||!monotonic(now)||now<last||now>=deadline)terminal=true;
    last=now;return terminal;
}
bool PrivatePakJob::directories_valid() const noexcept {
    struct stat observed{},named{};
    if(fstat(root,&observed)||!private_directory(observed)||!identity(root_stat,observed))return false;
    if(!q_owned)return true;
    return quarantine>=0&&!fstat(quarantine,&observed)&&private_directory(observed)&&identity(q_stat,observed)&&
        !mb_archive_posix_fstatat(root,q_name,&named,AT_SYMLINK_NOFOLLOW)&&private_directory(named)&&identity(q_stat,named);
}
bool PrivatePakJob::close_owned(int& fd) noexcept {
    if(fd<0)return true;
    int previous=fd;fd=-1;
    if(!close(previous))return true;
    uncertain_close=true;state=State::StorageFailed;return false;
}
int PrivatePakJob::acquire(void *context,int *fd) noexcept {
    auto& job=*static_cast<PrivatePakJob*>(context);
    if(!fd||job.stopped()||!job.directories_valid())return -1;
    job.output=mb_archive_posix_openat(job.root,job.out_name,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(job.output<0)return -1;
    job.out_owned=true;*fd=job.output;
    if(fstat(job.output,&job.out_stat)||!private_file(job.out_stat)||job.out_stat.st_size||job.stopped())return -1;
    return 0;
}
bool PrivatePakJob::begin() noexcept {
    if(!monotonic(started)||started>UINT64_MAX-timeout){state=State::Stopped;return false;}
    last=started;deadline=started+timeout;
    struct stat observed{};
    if(stopped()||fstat(base.fd,&observed)||!snapshot(base.snapshot,observed)) {
        state=terminal?State::Stopped:State::Refused;return false;
    }
    return true;
}
bool PrivatePakJob::create_quarantine() noexcept {
    if(stopped()){state=State::Stopped;return false;}
    if(!directories_valid()||mb_archive_posix_mkdirat(root,q_name,0700)){state=State::StorageFailed;return false;}
    q_owned=true;
    if(mb_archive_posix_fstatat(root,q_name,&q_stat,AT_SYMLINK_NOFOLLOW)||!private_directory(q_stat)){state=State::StorageFailed;return false;}
    quarantine=mb_archive_posix_openat(root,q_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,0);
    if(!directories_valid()){state=State::StorageFailed;return false;}
    if(stopped()){state=State::Stopped;return false;}
    return true;
}
void PrivatePakJob::prepare(const mb_archive_source_v1& source,bool component_only) noexcept {
    if(state==State::StorageFailed)return;
    try {
        if(!begin())return;
        workspace=std::make_unique<mb_pak_archive_job_workspace_v1>();
        workspace->struct_size=sizeof(*workspace);workspace->abi_version=MB_PAK_ARCHIVE_JOB_ABI_V1;
        if(!create_quarantine())return;
        mb_pak_archive_job_params_v1 params{};params.struct_size=sizeof(params);params.abi_version=MB_PAK_ARCHIVE_JOB_ABI_V1;
        params.source=source;std::copy(base.sha.begin(),base.sha.end(),params.source.target_token);
        params.base_fd=base.fd;params.base_bytes=base.bytes;params.quarantine_fd=quarantine;params.output_fd=-1;
        params.immutable_inputs=params.private_quarantine=params.unpublished_output=1;
        params.policy=component_only?MB_PAK_ARCHIVE_POLICY_PAK_COMPONENT_ONLY:MB_PAK_ARCHIVE_POLICY_DEFAULT;
        params.acquire_output=acquire;params.output_context=this;
        // Callback backing is this owner. Keep the mutable core result outside
        // that backing for the synchronous call, then retain its journal report.
        mb_pak_archive_job_result_v1 staged_result{};
        auto code=mb_pak_archive_job_prepare_gated_v1(&params,workspace.get(),&staged_result,gate,this);
        result=staged_result;
        expected_output_bytes=result.union_result.archive_bytes;
        unhandled=result.selection.retained_extra_count;whole_bundle_handled=result.selection.whole_bundle_handled!=0;
        std::snprintf(stage,sizeof(stage),"%s",mb_pak_archive_job_stage_name_v1(result.stage));
        std::snprintf(error,sizeof(error),"%s",result.error);
        if(code==MB_PAK_ARCHIVE_JOB_ADDITIONAL_ACTION_REQUIRED)state=State::AdditionalActionRequired;
        else if(code==MB_PAK_ARCHIVE_JOB_STOPPED||terminal)state=State::Stopped;
        else if(code!=MB_PAK_ARCHIVE_JOB_OK||!result.preparation_complete||result.installation_supported||result.runtime_supported)state=State::Refused;
        else if(finalize()){state=State::Prepared;artifact_complete=true;}
        else state=uncertain_close?State::StorageFailed:terminal?State::Stopped:State::Refused;
        if(state!=State::Prepared){artifact_complete=false;readback_observed=false;artifact_bytes=0;artifact_sha.fill(0);}
    }catch(...){state=State::Refused;artifact_complete=false;readback_observed=false;artifact_bytes=0;artifact_sha.fill(0);}
}
bool PrivatePakJob::finalize() noexcept {
    struct stat observed{},named{},base_now{};
    if(stopped()||!directories_valid()||output<0||fstat(output,&observed)||!private_file(observed)||!identity(out_stat,observed)||
       !expected_output_bytes||static_cast<uint64_t>(observed.st_size)!=expected_output_bytes||fsync(output)||stopped())return false;
    out_stat=observed;
    if(!close_owned(output))return false;
    output=mb_archive_posix_openat(root,out_name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
    if(output<0||fstat(output,&observed)||!private_file(observed)||!snapshot(out_stat,observed)||stopped())return false;
    std::array<uint8_t,32> hash{};
    try {if(!hash_fd(output,static_cast<uint64_t>(observed.st_size),hash,gate,this))return false;}catch(...){return false;}
    if(fstat(output,&observed)||!snapshot(out_stat,observed)||!close_owned(output)||!directories_valid()||
       mb_archive_posix_fstatat(root,out_name,&named,AT_SYMLINK_NOFOLLOW)||!private_file(named)||!snapshot(out_stat,named)||
       fstat(base.fd,&base_now)||!snapshot(base.snapshot,base_now)||stopped())return false;
    artifact_sha=hash;artifact_bytes=static_cast<uint64_t>(named.st_size);readback_observed=true;return true;
}
bool PrivatePakJob::cleanup() noexcept {
    artifact_complete=false;readback_observed=false;artifact_sha.fill(0);artifact_bytes=0;
    if(uncertain_close||!directories_valid())return false;
    bool okay=true;struct stat observed{};
    if(out_owned) {
        if(mb_archive_posix_fstatat(root,out_name,&observed,AT_SYMLINK_NOFOLLOW)) {
            if(errno!=ENOENT)okay=false;else out_owned=false;
        }else if(!out_stat.st_ino||!private_file(observed)||!identity(out_stat,observed))okay=false;
        else if(mb_archive_posix_unlinkat(root,out_name,0)&&errno!=ENOENT)okay=false;
        else out_owned=false;
    }
    if(!close_owned(output))okay=false;
    if(!close_owned(selected_member))okay=false;
    if(q_owned&&workspace) {
        for(size_t i=MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY;i>0;--i) {
            auto& entry=workspace->journal[i-1];
            if(entry.state!=MB_ARCHIVE_CREATED&&entry.state!=MB_ARCHIVE_RETAINED)continue;
            if(i==1&&!okay)continue;
            if(mb_archive_posix_fstatat(quarantine,entry.name,&observed,AT_SYMLINK_NOFOLLOW)) {
                if(errno==ENOENT)entry.state=MB_ARCHIVE_REMOVED;else okay=false;
            }else if(!entry.inode||!S_ISREG(observed.st_mode)||static_cast<uint64_t>(observed.st_dev)!=entry.device||
                     static_cast<uint64_t>(observed.st_ino)!=entry.inode)okay=false;
            else if(mb_archive_posix_unlinkat(quarantine,entry.name,0)&&errno!=ENOENT)okay=false;
            else entry.state=MB_ARCHIVE_REMOVED;
        }
        if(okay&&directories_valid()) {
            if(mb_archive_posix_unlinkat(root,q_name,AT_REMOVEDIR)&&errno!=ENOENT)okay=false;
            else q_owned=false;
        }
    }
    if(!q_owned&&!close_owned(quarantine))okay=false;
    if(!okay)state=State::StorageFailed;
    return okay&&!uncertain_close;
}
}
