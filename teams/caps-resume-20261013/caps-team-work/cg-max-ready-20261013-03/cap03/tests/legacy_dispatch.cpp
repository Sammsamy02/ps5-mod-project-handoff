#define main legacy_main
#include "legacy_host_test.cpp"
#undef main
int main(int argc,char**argv)
{
    if(argc!=2)return 3;
    fixture_setup();
    std::string c=argv[1];
#define CASE(name) if(c==#name) name(); else
    CASE(test_arg_validation)
    CASE(test_query_failure)
    CASE(test_mixed_protection)
    CASE(test_happy_patch_unpatch)
    CASE(test_cross_page_span)
    CASE(test_already_applied)
    CASE(test_open_mprotect_failure)
    CASE(test_verify_open_failure_qmp_stale)
    CASE(test_verify_open_failure_vq_disagrees)
    CASE(test_restore_retry_budget)
    CASE(test_restore_mprotect_terminal)
    CASE(test_raii_scope)
    CASE(test_backup_validation)
    CASE(test_module_slot_entries)
    return 3;
#undef CASE
    printf("LEGACY %s: checks=%d failures=%d\n",argv[1],g_checks,g_fails);
    return g_fails?1:0;
}
