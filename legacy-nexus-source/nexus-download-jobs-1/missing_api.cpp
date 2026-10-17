#include "jobs.h"
int main() {
 mb_nexus_jobs_context_v1 *context=nullptr;
 mb_nexus_jobs_result_v1 result{};
 if(mb_nexus_jobs_create_v1(nullptr,&context,&result)!=MB_NEXUS_JOBS_BAD_ARGUMENT)return 1;
 if(mb_nexus_jobs_submit_v1(nullptr,nullptr,&result)!=MB_NEXUS_JOBS_BAD_ARGUMENT)return 2;
 if(mb_nexus_jobs_list_v1(nullptr,nullptr,&result)!=MB_NEXUS_JOBS_BAD_ARGUMENT)return 3;
 if(mb_nexus_jobs_get_v1(nullptr,nullptr,&result)!=MB_NEXUS_JOBS_BAD_ARGUMENT)return 4;
 if(mb_nexus_jobs_run_one_v1(nullptr,&result)!=MB_NEXUS_JOBS_BAD_ARGUMENT)return 5;
 return mb_nexus_jobs_free_v1(&context);
}
