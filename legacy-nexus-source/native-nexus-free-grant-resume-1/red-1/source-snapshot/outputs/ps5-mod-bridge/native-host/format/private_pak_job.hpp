#ifndef MB_PRIVATE_PAK_JOB_HPP
#define MB_PRIVATE_PAK_JOB_HPP
#include <array>
#include <memory>
#include <sys/stat.h>
extern "C" {
#include "pak_archive_job.h"
#include "config_file_job.h"
}

namespace modbridge {
// Trusted captured data only. Its owner keeps the borrowed read-only FD and
// immutable backing alive until every job is freed. This is not game discovery.
struct CapturedPakBase {
    int fd=-1;
    uint64_t bytes=0;
    std::array<uint8_t,32> sha{};
    struct stat snapshot{};
};
bool admit_captured_pak_base(int fd,uint64_t bytes,const uint8_t sha[32],
    uint32_t timeout_ms,CapturedPakBase& admitted) noexcept;

class PrivatePakJob {
public:
    enum class State { Refused,AdditionalActionRequired,Prepared,Stopped,StorageFailed };
    State state=State::Refused;
    uint32_t unhandled=0;
    bool whole_bundle_handled=false,artifact_complete=false,readback_observed=false;
    uint64_t artifact_bytes=0;
    std::array<uint8_t,32> artifact_sha{};
    char stage[48]{},error[96]{};
    bool config_mode=false;
    pm_config_file_job_result_v2 config_result{};
    PrivatePakJob(int directory,const char id[33],const CapturedPakBase& base,uint32_t timeout_ms);
    ~PrivatePakJob() = default;
    void prepare(const mb_archive_source_v1& source,bool component_only) noexcept;
    // Strict single Engine.ini archive: all source headers must belong to the
    // explicit scalar SystemSettings subset. Target/mount/seed derive from base.
    void prepare_config(const mb_archive_source_v1& source) noexcept;
    // Unconditional identity-bound cleanup. Failed cleanup keeps this owner and
    // journal alive for retry. No unlink of substituted leaves, no game paths.
    bool cleanup() noexcept;
    bool storage_failed() const noexcept { return uncertain_close || state==State::StorageFailed; }
private:
    int root=-1,quarantine=-1,output=-1,selected_member=-1;
    bool q_owned=false,out_owned=false,uncertain_close=false;
    struct stat root_stat{},q_stat{},out_stat{};
    char q_name[48]{},out_name[52]{};
    CapturedPakBase base;
    uint32_t timeout=0;
    uint64_t started=0,last=0,deadline=0;
    bool terminal=false;
    uint64_t expected_output_bytes=0;
    std::unique_ptr<mb_pak_archive_job_workspace_v1> workspace;
    mb_pak_archive_job_result_v1 result{};
    static int gate(void *context) noexcept;
    static int acquire(void *context,int *fd) noexcept;
    static bool hash_file(int fd,uint64_t bytes,std::array<uint8_t,32>& digest,
                         mb_pak_prepare_gate_v1 gate,void *context) noexcept;
    static bool stable_input(int fd,const struct stat& before) noexcept;
    bool stopped() noexcept;
    bool directories_valid() const noexcept;
    bool close_owned(int& descriptor) noexcept;
    bool begin() noexcept;
    bool create_quarantine() noexcept;
    bool config_body(const mb_archive_source_v1& source);
    bool finalize() noexcept;
};
}
#endif
