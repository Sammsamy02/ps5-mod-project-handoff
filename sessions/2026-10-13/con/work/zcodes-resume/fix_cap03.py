from pathlib import Path
r=Path('/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ready-20261013-02/cap03/src')
p=r/'CodePatch.hpp';s=p.read_text().replace('kNotPatchedState,        /* unpatch: target bytes != recorded patch  */','kNotPatchedState,        /* unpatch: target bytes != recorded patch  */\n    kDirtyState,             /* failed recovery: no new patches allowed */\n    kUnreadableTarget,       /* original bytes cannot safely be read    */')
s=s.replace('const char* patch_result_str(PatchResult r);','const char* patch_result_str(PatchResult r);\n/* Sticky for this module/process lifetime. No reset API: recovery is external. */\nbool patch_state_dirty();')
s=s.replace('PROPOSAL ONLY. Written 2026-10-13, Mac-only. Never built, never run on a','STAGED REVISION cg-ready-02. Mac host-tested; never built for or run on a')
p.write_text(s)
p=r/'CodePatch.cpp';s=p.read_text()
s=s.replace('static bool        g_last_backup_seen;','static bool        g_last_backup_seen;\nstatic bool        g_dirty_state;\n\nbool patch_state_dirty() { return g_dirty_state; }')
s=s.replace('case PatchResult::kBadFlags:', 'case PatchResult::kDirtyState:           return "DIRTY_STATE";\n    case PatchResult::kUnreadableTarget:     return "UNREADABLE_TARGET";\n    case PatchResult::kBadFlags:')
# Insert permission-only restore path before revert_only. No bytes may be accessed here.
start=s.index('/* REVERT-ONLY path:')
s=s[:start]+'''/* Permission-only recovery is also safe when OPEN could not be verified.
 * Never infer access from a successful mprotect return alone. */
static PatchResult restore_permissions(const Span* span)
{
    PatchResult result = PatchResult::kOk;
    for(int i = 0; i < span->page_count; ++i)
    {
        void* page = (void*)((uintptr_t)span->base + (size_t)i * PAGE_SIZE);
        errno = 0;
        int rc = mprotect(page, PAGE_SIZE, span->orig_prot[i]);
        emitf("[CAP3][REVERT] STEP=RESTORE-PROT page=%d rc=%d errno=%d",
              i, rc, errno);
        if(rc != 0) result = PatchResult::kRestoreMprotectFailed;
    }
    for(int i = 0; i < span->page_count; ++i)
    {
        const void* page = (const void*)((uintptr_t)span->base + (size_t)i * PAGE_SIZE);
        SpanProtection sp;
        if(query_span_protection(page, &sp) != PatchResult::kOk)
        {
            if(result == PatchResult::kOk) result = PatchResult::kRestoreQueryFailed;
        }
        else if(sp.prot != span->orig_prot[i] && result == PatchResult::kOk)
            result = PatchResult::kRestoreVerifyFailed;
    }
    if(result != PatchResult::kOk)
    {
        g_dirty_state = true;
        emitf("[CAP3][REVERT] STATE DIRTY rc=%d; new patches refused", (int)result);
    }
    return result;
}

''' + s[start:]
# Replace old protection loop block only in revert_only
start=s.index('    /* R2: original protection')
end=s.index('\n}\n',start)
s=s[:start]+'''    if(rw != PatchResult::kOk) g_dirty_state = true;
    PatchResult rp = restore_permissions(span);
    return rw != PatchResult::kOk ? rw : rp;''' + s[end:]
# argument strict flags everywhere
s=s.replace('if((flags & kFlagRequireLiveness) && (flags & kFlagSkipLiveness))','if((flags & ~(kFlagRequireLiveness | kFlagSkipLiveness)) ||\n       ((flags & kFlagRequireLiveness) && (flags & kFlagSkipLiveness)))')
# guard target wrap and dirty before span
old='''    Span span;
    span_of(target, length, &span);'''
new='''    if((uintptr_t)target > UINTPTR_MAX - (length - 1))
        return PatchResult::kBadArgs;
    if(g_dirty_state) return PatchResult::kDirtyState;
    Span span;
    span_of(target, length, &span);'''
assert s.count(old)==1;s=s.replace(old,new)
s=s.replace('''    log_hex_bytes("original", target, length);''','''    for(int i = 0; i < span.page_count; ++i)
        if((span.orig_prot[i] & PROT_READ) == 0)
            return PatchResult::kUnreadableTarget;
    log_hex_bytes("original", target, length);''')
# verification failure has no byte write
s=s.replace('''        revert_only(target, local_backup.original_bytes, length, &span,
                    "verify-open");''','''        (void)restore_permissions(&span);''')
s=s.replace('''        revert_only(target, backup->replacement_bytes, length, &span,
                    "unpatch-verify-open");''','''        (void)restore_permissions(&span);''')
# validate backup before reading arrays, calculating spans or accessing target.
needle='''    void* target = backup->target;
    size_t length = backup->length;'''
replacement='''    if(!backup->target || !backup->page_base || backup->length == 0 ||
       backup->length > kMaxPatchBytes || backup->page_count == 0 ||
       backup->page_count > (uint32_t)kMaxPatchPages ||
       (uintptr_t)backup->target > UINTPTR_MAX - (backup->length - 1))
        return PatchResult::kBackupInvalid;
    Span expected;
    span_of(backup->target, backup->length, &expected);
    if(backup->page_base != expected.base || backup->span_len != expected.len ||
       backup->page_count != (uint32_t)expected.page_count)
        return PatchResult::kBackupInvalid;
    for(uint32_t i = 0; i < backup->page_count; ++i)
        if(backup->orig_prot[i] < 0 ||
           (backup->orig_prot[i] & ~(PROT_READ | PROT_WRITE | PROT_EXEC)))
            return PatchResult::kBackupInvalid;
    void* target = backup->target;
    size_t length = backup->length;'''
assert s.count(needle)==1;s=s.replace(needle,replacement)
# unpatch preflight access verification
s=s.replace('''    if(memcmp(target, backup->replacement_bytes, length) != 0)''','''    for(int i = 0; i < span.page_count; ++i)
        if((cur_prot[i] & PROT_READ) == 0)
            return PatchResult::kUnreadableTarget;
    if(memcmp(target, backup->replacement_bytes, length) != 0)''')
s=s.replace('''    emitf("[CAP3][UNPATCH] RESTORE-FAILED''','''    g_dirty_state = true;
    emitf("[CAP3][UNPATCH] RESTORE-FAILED''')
s=s.replace('further patches this boot','further new patches in this process')
s=s.replace('PROPOSAL ONLY (2026-10-13, Mac-only; never built, never run on console).','STAGED REVISION cg-ready-02 (Mac host-tested; never built/run on console).')
p.write_text(s)
