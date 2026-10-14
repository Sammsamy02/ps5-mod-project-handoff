#include "private_pak_job.hpp"
extern "C" {
#include "../archive/candidate_selector.h"
#include "../archive/private/posix_compat.h"
#include "examples/archive_config_job/config_archive_scope.h"
}
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <vector>

namespace modbridge {
namespace {
bool same_inode(const struct stat& a,const struct stat& b) noexcept {
    return a.st_dev==b.st_dev&&a.st_ino==b.st_ino;
}
bool readonly_file(int fd,uint64_t bytes,struct stat& observed) noexcept {
    int flags=fd>=0?fcntl(fd,F_GETFL):-1;
    return flags>=0&&(flags&O_ACCMODE)==O_RDONLY&&!fstat(fd,&observed)&&S_ISREG(observed.st_mode)&&
        observed.st_size>=0&&static_cast<uint64_t>(observed.st_size)==bytes;
}
int read_at(void*,int fd,uint64_t offset,uint8_t *out,uint32_t bytes,uint32_t *actual) {
    *actual=0;if(offset>INT64_MAX||bytes>INT64_MAX-offset)return 1;
    auto received=pread(fd,out,bytes,static_cast<off_t>(offset));
    if(received<0||static_cast<uint64_t>(received)>bytes)return 1;
    *actual=static_cast<uint32_t>(received);return 0;
}
int write_at(void*,int fd,uint64_t offset,const uint8_t *data,uint32_t bytes,uint32_t *actual) {
    *actual=0;if(offset>INT64_MAX||bytes>INT64_MAX-offset)return 1;
    auto written=pwrite(fd,data,bytes,static_cast<off_t>(offset));
    if(written<0||static_cast<uint64_t>(written)>bytes)return 1;
    *actual=static_cast<uint32_t>(written);return 0;
}
void *allocate(void*,size_t bytes) {return std::malloc(bytes);}
void release(void*,void *memory) {std::free(memory);}
bool white(uint8_t c) noexcept {return c==' '||c=='\t';}
uint8_t fold(uint8_t c) noexcept {return c>='A'&&c<='Z'?static_cast<uint8_t>(c+32):c;}
// Scope check only. The shared scalar parser still validates syntax/UTF-8,
// arrays and limits. Reject every unselected header so it cannot drop a section.
bool selected_headers_only(const std::vector<uint8_t>& source,mb_pak_prepare_gate_v1 predicate,void *context) {
    static constexpr char selected[]="SystemSettings";
    size_t pos=source.size()>=3&&source[0]==0xef&&source[1]==0xbb&&source[2]==0xbf?3:0;
    while(pos<source.size()) {
        if(predicate(context))return false;
        size_t end=pos;
        while(end<source.size()&&source[end]!='\n'&&source[end]!='\r')++end;
        size_t begin=pos,trim=end;
        while(begin<trim&&white(source[begin]))++begin;
        while(trim>begin&&white(source[trim-1]))--trim;
        if(begin<trim&&source[begin]=='[') {
            if(trim-begin!=sizeof(selected)+1||source[trim-1]!=']')return false;
            for(size_t i=0;i<sizeof(selected)-1;++i)
                if(fold(source[begin+1+i])!=fold(static_cast<uint8_t>(selected[i])))return false;
        }
        pos=end;
        if(pos<source.size()&&source[pos]=='\r')++pos;
        if(pos<source.size()&&source[pos]=='\n')++pos;
    }
    return !predicate(context);
}
}

void PrivatePakJob::prepare_config(const mb_archive_source_v1& source) noexcept {
    if(state==State::StorageFailed)return;
    config_mode=true;bool prepared=false;
    try {if(begin())prepared=config_body(source);}catch(...){state=State::Refused;}
    // Member FD belongs to this owner. Closing and later cleanup ignore stop.
    if(!close_owned(selected_member))prepared=false;
    if(prepared) {
        std::snprintf(stage,sizeof(stage),"private_readback");
        if(finalize()){state=State::Prepared;artifact_complete=true;std::snprintf(stage,sizeof(stage),"complete");}
        else state=uncertain_close?State::StorageFailed:terminal?State::Stopped:State::Refused;
    }
    if(terminal&&state!=State::StorageFailed)state=State::Stopped;
    if(state!=State::Prepared){artifact_complete=false;readback_observed=false;artifact_bytes=0;artifact_sha.fill(0);whole_bundle_handled=false;}
}

bool PrivatePakJob::config_body(const mb_archive_source_v1& original_source) {
    auto refuse=[&](const char *at,const char *why)->bool {
        std::snprintf(stage,sizeof(stage),"%s",at);std::snprintf(error,sizeof(error),"%s",why);
        if(state!=State::StorageFailed&&state!=State::AdditionalActionRequired)state=terminal?State::Stopped:State::Refused;
        return false;
    };
    if(original_source.struct_size!=sizeof(original_source)||original_source.abi_version!=MB_ARCHIVE_ABI_V1||
       original_source.reserved||!original_source.size||original_source.size>MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES)
        return refuse("input_open","archive_contract_refused");
    mb_archive_source_v1 source=original_source;
    std::copy(base.sha.begin(),base.sha.end(),source.target_token);
    struct stat source_before{},member_before{};
    if(stopped()||!readonly_file(source.fd,source.size,source_before)||same_inode(source_before,base.snapshot)||!directories_valid())
        return refuse("input_open","readonly_source_or_private_owner_refused");
    workspace=std::make_unique<mb_pak_archive_job_workspace_v1>();
    workspace->struct_size=sizeof(*workspace);workspace->abi_version=MB_PAK_ARCHIVE_JOB_ABI_V1;
    auto& w=*workspace;
    mb_archive_limits_v1 limits{};mb_archive_limits_default_v1(&limits);
    limits.raw_bytes=limits.member_bytes=MB_PAK_ARCHIVE_JOB_MAX_INPUT_BYTES;
    limits.expanded_bytes=UINT64_C(1)<<30;limits.entries=MB_PAK_ARCHIVE_JOB_ROWS;limits.name_bytes=MB_PAK_ARCHIVE_JOB_NAME_BYTES;
    mb_archive_budget_v1 budget{};mb_archive_budget_default_v1(&budget);
    budget.io_limit=UINT64_C(8)<<30;budget.operation_limit=1000000;budget.path_work_limit=UINT64_C(16)<<20;
    budget.cancelled=gate;budget.context=this;
    mb_archive_inventory_v1 inventory{};
    mb_archive_buffers_v1 scan{w.rows,MB_PAK_ARCHIVE_JOB_ROWS,w.names,MB_PAK_ARCHIVE_JOB_NAME_BYTES,
        w.scan_sort,MB_PAK_ARCHIVE_JOB_ROWS,w.transfer,sizeof(w.transfer)};
    std::snprintf(stage,sizeof(stage),"archive_scan");
    int scan_code=stopped()?MB_ARCHIVE_CANCELLED:mb_archive_scan_v1(&source,&limits,&budget,&scan,&inventory);
    result.inventory=inventory;
    if(stopped()||scan_code!=MB_ARCHIVE_OK||inventory.complete!=1||inventory.installation_supported||
       !stable_input(source.fd,source_before))return refuse("archive_scan","scan_incomplete_or_source_changed");
    mb_archive_config_selector_profile_v1 selection_profile{};
    selection_profile.struct_size=sizeof(selection_profile);selection_profile.abi_version=MB_ARCHIVE_CONFIG_SELECTOR_ABI_V1;
    selection_profile.basename="Engine.ini";selection_profile.basename_bytes=10;
    selection_profile.max_source_bytes=MB_ARCHIVE_CONFIG_SELECTOR_MAX_SOURCE_BYTES;
    std::copy(base.sha.begin(),base.sha.end(),selection_profile.target_token);
    mb_archive_config_candidate_v1 selected{};
    int selection=mb_archive_config_select_v1(&inventory,w.rows,MB_PAK_ARCHIVE_JOB_ROWS,w.names,MB_PAK_ARCHIVE_JOB_NAME_BYTES,&selection_profile,&selected);
    if(stopped()||selection!=MB_ARCHIVE_CONFIG_SELECTOR_OK||!selected.complete||selected.candidate_count!=1||
       selected.row_index>=inventory.row_count||selected.format_validated||selected.platform_compatible||
       selected.installation_supported||selected.runtime_supported)
        return refuse("archive_selection","unique_engine_ini_refused");
    for(uint32_t i=0;i<inventory.row_count;++i) {
        if(stopped())return refuse("archive_selection","operation_gate_stopped");
        if(w.rows[i].kind==MB_ARCHIVE_REGULAR&&i!=selected.row_index)++unhandled;
    }
    int scope=pm_config_archive_scope(&inventory,w.rows,MB_PAK_ARCHIVE_JOB_ROWS,&selected);
    if(scope==PM_CONFIG_SCOPE_UNHANDLED_REGULAR) {
        state=State::AdditionalActionRequired;
        return refuse("archive_selection","additional_regular_members_need_handler");
    }
    if(scope!=PM_CONFIG_SCOPE_OK)return refuse("archive_selection","inventory_scope_refused");
    if(!create_quarantine())return refuse("private_directories","quarantine_acquisition_refused");
    w.selected[0]=selected.row_index;
    mb_archive_materialize_request_v1 request{};
    request.source=&source;request.inventory=&inventory;request.rows=w.rows;request.row_capacity=MB_PAK_ARCHIVE_JOB_ROWS;
    request.names=w.names;request.name_capacity=MB_PAK_ARCHIVE_JOB_NAME_BYTES;request.selected=w.selected;request.selected_count=1;
    std::copy(base.sha.begin(),base.sha.end(),request.target_token);
    request.quarantine_fd=quarantine;request.quarantine_device=q_stat.st_dev;request.quarantine_inode=q_stat.st_ino;
    request.journal=w.journal;request.journal_capacity=MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY;
    request.replay={w.replay_rows,MB_PAK_ARCHIVE_JOB_ROWS,w.replay_names,MB_PAK_ARCHIVE_JOB_NAME_BYTES,
        w.replay_sort,MB_PAK_ARCHIVE_JOB_ROWS,w.replay_transfer,sizeof(w.replay_transfer)};
    mb_archive_materialized_v1 materialized{};
    std::snprintf(stage,sizeof(stage),"archive_materialization");
    int materialize=stopped()?MB_ARCHIVE_CANCELLED:mb_archive_materialize_v1(&request,&limits,&budget,&materialized);
    result.materialized=materialized;result.budget=budget;result.budget.cancelled=nullptr;result.budget.context=nullptr;
    if(stopped()||materialize!=MB_ARCHIVE_OK||materialized.complete!=1||materialized.selected_count!=1||
       materialized.installation_supported||materialized.journal_count>MB_PAK_ARCHIVE_JOB_JOURNAL_CAPACITY)
        return refuse("archive_materialization","materialization_incomplete");
    const mb_archive_owned_file_v1 *owned=nullptr;
    for(uint32_t i=0;i<materialized.journal_count;++i) {
        const auto& entry=w.journal[i];
        if(entry.row_index==selected.row_index&&entry.state==MB_ARCHIVE_CREATED) {
            if(owned)return refuse("ini_member_open","journal_binding_ambiguous");
            owned=&entry;
        }
    }
    char name[40]{};std::snprintf(name,sizeof(name),"member-%08u.bin",selected.row_index);
    if(!owned||std::memcmp(owned->name,name,std::strlen(name)+1)||owned->expected_size!=selected.member_bytes||
       owned->written_bytes!=selected.member_bytes||std::memcmp(owned->sha256,selected.member_sha256,32))
        return refuse("ini_member_open","journal_binding_refused");
    selected_member=mb_archive_posix_openat(quarantine,name,O_RDONLY|O_NOFOLLOW|O_CLOEXEC,0);
    if(stopped()||!readonly_file(selected_member,selected.member_bytes,member_before)||
       static_cast<uint64_t>(member_before.st_dev)!=owned->device||static_cast<uint64_t>(member_before.st_ino)!=owned->inode||
       same_inode(member_before,source_before)||same_inode(member_before,base.snapshot))
        return refuse("ini_member_open","journal_member_identity_refused");
    std::array<uint8_t,32> member_hash{};
    if(!hash_file(selected_member,selected.member_bytes,member_hash,gate,this)||
       std::memcmp(member_hash.data(),selected.member_sha256,32))return refuse("ini_member_open","member_readback_hash_refused");
    std::vector<uint8_t> member(static_cast<size_t>(selected.member_bytes));
    uint32_t read=0;
    if(stopped()||read_at(nullptr,selected_member,0,member.data(),static_cast<uint32_t>(member.size()),&read)||
       read!=member.size()||stopped())return refuse("ini_sections","source_read_refused");
    if(!selected_headers_only(member,gate,this)) {
        if(!terminal){state=State::AdditionalActionRequired;unhandled=1;}
        return refuse("ini_sections","source_sections_need_explicit_handler");
    }
    if(!stable_input(source.fd,source_before)||!stable_input(base.fd,base.snapshot)||!stable_input(selected_member,member_before))
        return refuse("source_snapshot","input_changed_before_config");
    int acquired=-1;
    if(acquire(this,&acquired)||acquired<0||stopped())return refuse("output_create","private_output_acquisition_refused");
    pm_config_file_job_params_v2 params{};params.struct_size=sizeof(params);params.abi_version=PM_CONFIG_FILE_JOB_ABI_V2;
    auto& p=params.base;p.struct_size=sizeof(p);p.abi_version=PM_CONFIG_FILE_JOB_ABI_V1;
    p.original_fd=base.fd;p.member_fd=selected_member;p.output_fd=acquired;p.immutable_inputs=p.unpublished_output=1;
    p.original_bytes=base.bytes;p.member_bytes=selected.member_bytes;
    static constexpr uint8_t section[]="SystemSettings",platform[]="PS5";
    pm_ini_bytes sections[]={{section,sizeof(section)-1}};
    p.sections=sections;p.section_count=1;p.read_at=read_at;p.write_at=write_at;p.allocate=allocate;p.release=release;
    params.platform={platform,sizeof(platform)-1};
    pm_config_file_job_result_v2 staged{};
    std::snprintf(stage,sizeof(stage),"ini_to_pak");
    auto status=pm_config_file_job_gated_v2(&params,&staged,gate,this);
    config_result=staged;
    if(stopped()||status!=PM_CONFIG_FILE_JOB_OK||!staged.preparation.archive_complete||
       staged.preparation.allocation_live_after_cleanup||!staged.resolved_path_bytes||!staged.resolved_mount_bytes)
        return refuse("ini_to_pak","shared_config_preparation_refused");
    if(!stable_input(source.fd,source_before)||!stable_input(base.fd,base.snapshot)||!stable_input(selected_member,member_before))
        return refuse("source_snapshot","input_changed_after_config");
    expected_output_bytes=staged.preparation.writer.archive_bytes;
    whole_bundle_handled=true;std::snprintf(stage,sizeof(stage),"complete");return true;
}
}
