/* Explicit missing-feature baseline: real old core, intentionally absent gate. */
#include "config_file_job.h"
pm_config_file_job_status pm_gate_legacy_v1(const pm_config_file_job_params_v1 *p,
    pm_config_file_job_result_v1 *r, mb_pak_prepare_gate_v1 gate, void *context) {
    (void)gate; (void)context;
    return pm_config_file_job_v1(p, r);
}
pm_config_file_job_status pm_gate_legacy_v2(const pm_config_file_job_params_v2 *p,
    pm_config_file_job_result_v2 *r, mb_pak_prepare_gate_v1 gate, void *context) {
    (void)gate; (void)context;
    return pm_config_file_job_v2(p, r);
}
