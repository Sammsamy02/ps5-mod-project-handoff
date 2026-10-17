// Real production protocol with OS-boundary doubles from the existing host test.
#define main legacy_main
#include "legacy_host_test.cpp"
#undef main

static void unreadable_target()
{
    g_pages[(uintptr_t)g_span] = PROT_EXEC;
    // Actual inaccessible page catches the pre-OPEN log/memcpy read.
    ::mprotect(g_span, PAGE_SIZE, PROT_NONE);
    PatchBackup b{};
    const auto r = cap03::patch_code_ex(g_span, kReplacement, kPatchLen, &b, 0);
    CHECK(r != PatchResult::kOk, "unreadable preflight refuses safely");
    CHECK(g_mprotect_calls == 0, "unreadable refusal changes no permissions");
}
static void failed_open_never_writes()
{
    // mprotect double updates metadata; stale query denies verified W.
    g_qmp_use_frozen = true;
    PatchBackup b{};
    const auto r = cap03::patch_code_ex(g_span, kReplacement, kPatchLen, &b, 0);
    CHECK(r == PatchResult::kVerifyOpenFailed, "unverified OPEN refused");
    CHECK(!log_contains("STEP=WRITE+READBACK"), "failed OPEN verification never writes even rollback bytes");
    CHECK(page_prot_is(g_span,kOrigProt), "permission-only recovery restores original protection");
}
static void dirty_restore_blocks_new_patch()
{
    PatchBackup b{};
    CHECK(cap03::patch_code_ex(g_span,kReplacement,kPatchLen,&b,0)==PatchResult::kOk,"setup patch");
    g_mprotect_allow = g_mprotect_calls + 1;
    CHECK(cap03::unpatch_code_ex(&b,0)==PatchResult::kRestoreMprotectFailed,"restoration fails");
    fixture_reset_injections();
    const long before=g_mprotect_calls;
    CHECK(cap03::patch_code_ex(g_span+32,kReplacement,kPatchLen,nullptr,0)!=PatchResult::kOk,"dirty state refuses new patch");
    CHECK(g_mprotect_calls==before,"dirty admission performs no mprotect");
}
static void dirty_recovery_blocks_new_patch()
{
    g_qmp_use_frozen = true;
    g_mprotect_allow = 1; // OPEN succeeds; permission-only recovery fails.
    PatchBackup b{};
    CHECK(cap03::patch_code_ex(g_span,kReplacement,kPatchLen,&b,0)!=PatchResult::kOk,"failed OPEN and failed recovery refuse");
    fixture_reset_injections();
    const long before=g_mprotect_calls;
    CHECK(cap03::patch_code_ex(g_span+32,kReplacement,kPatchLen,nullptr,0)!=PatchResult::kOk,"failed recovery latches dirty");
    CHECK(g_mprotect_calls==before,"dirty recovery admission performs no mprotect");
}
static void unknown_flags()
{
    PatchBackup b{};
    CHECK(cap03::patch_code_ex(g_span,kReplacement,kPatchLen,&b,0x80000000u)==PatchResult::kBadFlags,"unknown flag is refused");
}
static void malformed_backup()
{
    PatchBackup b{};
    b.magic=cap03::kBackupMagic;b.valid=1;b.page_count=5;
    b.target=g_span;b.page_base=g_span;b.span_len=5*PAGE_SIZE;b.length=1;
    CHECK(cap03::unpatch_code_ex(&b,0)==PatchResult::kBackupInvalid,"oversized backup page count refused");
    CHECK(g_qmp_calls==0 && g_mprotect_calls==0,"malformed backup rejected before OS access");
}
static void invalid_query_bounds()
{
    g_qmp_bad_bounds=true;
    PatchBackup b{};
    CHECK(cap03::patch_code_ex(g_span,kReplacement,kPatchLen,&b,0)==PatchResult::kQueryFailed,"query with empty bracket refuses before access");
    CHECK(g_mprotect_calls==0,"invalid query bracket never opens");
}
static void scope_failed_revert_latches_dirty()
{
    {
        cap03::PatchScope scope(g_span, kReplacement, kPatchLen, 0);
        CHECK(scope.ok(), "scope setup applies patch");
        g_span[0] ^= 1; // Refuse before WRITE: target is no longer the recorded replacement.
        CHECK(scope.revert() == PatchResult::kNotPatchedState, "scope refuses changed target");
        CHECK(cap03::patch_state_dirty(), "failed armed revert latches dirty");
        const long before = g_mprotect_calls;
        CHECK(cap03::patch_code_ex(g_span+32,kReplacement,kPatchLen,nullptr,0)==PatchResult::kDirtyState,
              "dirty scope state refuses new patch");
        CHECK(g_mprotect_calls==before, "dirty admission does not open pages");
        CHECK(scope.revert() == PatchResult::kNotPatchedState, "repeated revert returns recorded failure");
        CHECK(g_mprotect_calls==before, "repeated revert performs no restoration cycle");
    }
}
int main(int argc,char**argv)
{
    if(argc!=2)return 3;
    fixture_setup();
    std::string c=argv[1];
    if(c=="unreadable")unreadable_target();
    else if(c=="verify-open")failed_open_never_writes();
    else if(c=="dirty-restore")dirty_restore_blocks_new_patch();
    else if(c=="dirty-recovery")dirty_recovery_blocks_new_patch();
    else if(c=="flags")unknown_flags();
    else if(c=="backup")malformed_backup();
    else if(c=="query-bounds")invalid_query_bounds();
    else if(c=="scope-refusal")scope_failed_revert_latches_dirty();
    else return 3;
    printf("REGRESSION %s: checks=%d failures=%d\n",argv[1],g_checks,g_fails);
    return g_fails ? 1 : 0;
}
