#ifndef MB_PRIVATE_IOSTORE_JOB_HPP
#define MB_PRIVATE_IOSTORE_JOB_HPP
#include <array>
#include <memory>
#include <sys/stat.h>
extern "C" {
#include "original_file_job.h"
#include "../archive/archive_selection.h"
}

namespace modbridge {
// Borrowed immutable captured metadata, owned until jobs_free succeeds. The
// observed large base partition is never opened by this format preparer.
struct CapturedIoStoreBase {
    int toc_fd=-1,header_fd=-1;
    uint64_t toc_bytes=0,header_bytes=0,observed_ucas_bytes=0;
    std::array<uint8_t,32> toc_sha{},header_sha{},token{};
    struct stat toc_snapshot{},header_snapshot{};
};
bool admit_captured_iostore_base(int toc_fd,uint64_t toc_bytes,const uint8_t toc_sha[32],
    int header_fd,uint64_t header_bytes,const uint8_t header_sha[32],uint64_t observed_ucas_bytes,
    uint32_t timeout_ms,CapturedIoStoreBase& admitted) noexcept;

class PrivateIoStoreJob {
public:
    enum class State { Refused,AdditionalActionRequired,Prepared,Stopped,StorageFailed,Conflict };
    State state=State::Refused;
    bool batch_mode=false;
    uint32_t source_count=0;
    uint32_t unhandled=0;
    // Totals count only content-verified omitted members; the source counters
    // count archives containing at least one such member, never inferred roles.
    uint32_t omitted_empty_paks=0,omitted_documents=0;
    uint32_t sources_with_empty_paks=0,sources_with_documents=0;
    bool whole_bundle_handled=false,artifact_complete=false,readback_observed=false;
    std::array<uint64_t,MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT> artifact_bytes{};
    std::array<std::array<uint8_t,32>,MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT> artifact_sha{};
    mb_iostore_file_job_result_v1 result{}; // Batch projection: batch_result.preparation.
    mb_iostore_batch_file_job_result_v1 batch_result{};
    mb_iostore_archive_selection_v1 selection{}; // Legacy single source only.
    std::array<mb_iostore_archive_selection_v1,MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES> source_selections{};
    char stage[48]{},error[96]{};
    PrivateIoStoreJob(int directory,const char id[33],const CapturedIoStoreBase&,uint32_t timeout_ms);
    ~PrivateIoStoreJob();
    void prepare(const mb_archive_source_v1&) noexcept;
    // Explicit immutable source archives, one private output generation. Each
    // archive must resolve to the existing supported IoStore scope. Strict core
    // conflicts refuse; source ordering never implies overwrite priority.
    // Both methods admit one operation per owner. Source descriptors remain
    // borrowed/immutable until the call returns. The owner retains its private
    // materializations and output until explicit successful cleanup; failed
    // cleanup requires retaining the owner and its journals for safe retry.
    void prepare_batch(const mb_archive_source_v1 *,uint32_t) noexcept;
    bool cleanup() noexcept;
    bool storage_failed() const noexcept {return uncertain_close||state==State::StorageFailed;}
private:
    struct Workspace;
    struct Source;
    std::unique_ptr<Workspace> workspace;
    std::array<std::unique_ptr<Source>,MB_IOSTORE_BATCH_FILE_JOB_MAX_SOURCES> sources;
    int root=-1,destination=-1,readback_fd=-1;
    bool out_owned=false,uncertain_close=false,attempted=false;
    char q_name[48]{},out_name[48]{};
    struct stat root_stat{},out_stat{};
    std::array<struct stat,MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT> artifacts{};
    std::array<bool,MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT> created{},closed{};
    std::array<int,MB_IOSTORE_FILE_JOB_ARTIFACT_COUNT> writer_fds{};
    CapturedIoStoreBase base;
    uint32_t timeout=0;
    uint64_t last=0,deadline=0;
    bool terminal=false;
    static int cancelled(void*) noexcept;
    static int continuing(void*) noexcept;
    static int artifact_event(void*,uint32_t,int,uint32_t) noexcept;
    static int read_at(void*,int,uint64_t,uint8_t*,uint32_t,uint32_t*) noexcept;
    static int write_at(void*,int,uint64_t,const uint8_t*,uint32_t,uint32_t*) noexcept;
    bool stopped() noexcept;
    bool directories_valid() const noexcept;
    bool close_owned(int&) noexcept;
    bool create_directory(const char*,int&,struct stat&,bool&) noexcept;
    void prepare_sources(const mb_archive_source_v1*,uint32_t,bool) noexcept;
    bool body();
    bool intake(uint32_t);
    bool sources_stable() const noexcept;
    bool charge_paths(uint64_t) noexcept;
    bool finalize() noexcept;
    void clear_completion() noexcept;
};
}
#endif
