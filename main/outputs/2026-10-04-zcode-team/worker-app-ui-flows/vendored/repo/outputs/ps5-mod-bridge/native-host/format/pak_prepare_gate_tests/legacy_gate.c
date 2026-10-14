/* Behavioral absent-gate baseline. The production legacy functions do all
 * parsing/materialization/stream writes; only the unavailable gate is ignored. */
#include "pak_archive_job.h"
#include "examples/pak_mod_union/prepare_union.h"

mb_pak_archive_job_status_v1 mb_gate_legacy_archive(
    const mb_pak_archive_job_params_v1 *p, mb_pak_archive_job_workspace_v1 *w,
    mb_pak_archive_job_result_v1 *r, mb_pak_prepare_gate_v1 gate, void *context) {
    (void)gate; (void)context;
    return mb_pak_archive_job_prepare_v1(p, w, r);
}

mb_pak_union_status_v1 mb_gate_legacy_union(
    const mb_pak_union_params_v1 *p, mb_pak_union_result_v1 *r,
    mb_pak_prepare_gate_v1 gate, void *context) {
    (void)gate; (void)context;
    return mb_pak_union_prepare_v1(p, r);
}

mb_pak_stream_status mb_gate_legacy_stream(
    const mb_pak_stream_profile_v1 *p, const mb_pak_stream_file_v1 *f,
    uint32_t count, const mb_pak_stream_limits_v1 *limits,
    const mb_pak_stream_io_v1 *io, mb_pak_stream_workspace_v1 *workspace,
    mb_pak_stream_result_v1 *r, mb_pak_prepare_gate_v1 gate, void *context) {
    (void)gate; (void)context;
    return mb_pak_stream_write_v11_none(p, f, count, limits, io, workspace, r);
}
