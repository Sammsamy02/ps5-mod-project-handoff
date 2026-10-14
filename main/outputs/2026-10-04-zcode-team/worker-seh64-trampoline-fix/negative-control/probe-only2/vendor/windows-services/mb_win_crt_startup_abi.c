/* VENDORED ADAPTED COPY (origin: pattern of worker-4-win-services/sources/
 * mb_win_init_once_abi.c; that file is unchanged). */
#include "mb_win_crt_startup_abi.h"
#if !defined(__x86_64__)
#error "CRT startup Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))
/* Exactly eight Win64 entry points with the documented Microsoft x64
 * signatures; each forwards to the native-CC freestanding owner in
 * mb_win_crt_startup.c. unsigned long (Win32) is the 32-bit uint32_t here. */
static WIN64 void svc_initterm(mb_win_crt_init_fn *b, mb_win_crt_init_fn *e)
{ mb_win_crt_initterm(b, e); }
static WIN64 int32_t svc_initterm_e(mb_win_crt_init_fn *b, mb_win_crt_init_fn *e)
{ return mb_win_crt_initterm_e(b, e); }
static WIN64 int32_t svc_seh_filter_dll(uint32_t code, void *info)
{ return mb_win_crt_seh_filter_dll(code, info); }
static WIN64 int32_t svc_crt_atexit(mb_win_crt_exit_fn fn)
{ return mb_win_crt_atexit(fn); }
static WIN64 void svc_cexit(void) { mb_win_crt_cexit(); }
static WIN64 int32_t svc_initialize_onexit_table(mb_win_crt_onexit_table *t)
{ return mb_win_crt_initialize_onexit_table(t); }
static WIN64 void *svc_register_onexit_function(mb_win_crt_onexit_table *t,
                                                mb_win_crt_exit_fn fn)
{ return (void *)(uintptr_t)mb_win_crt_register_onexit_function(t, fn); }
static WIN64 int32_t svc_execute_onexit_table(mb_win_crt_onexit_table *t)
{ return mb_win_crt_execute_onexit_table(t); }

static mb_win_crt_startup_session *sessions;
static mb_win_crt_startup_session **find(mb_win_crt_startup_session *s)
{ mb_win_crt_startup_session **p = &sessions; while (*p && *p != s) p = &(*p)->next; return p; }

mb_win_crt_startup_status mb_win_crt_startup_begin_session(mb_win_crt_startup_session *s,
                                                           mb_pe_module *m)
{
    if (!s || !m) return MB_WIN_CRT_STARTUP_INVALID_ARGUMENT;
    if (s->active || s->module || s->next || m->state != MB_PE_MODULE_EMPTY)
        return MB_WIN_CRT_STARTUP_BAD_STATE;
    for (mb_win_crt_startup_session *p = sessions; p; p = p->next)
        if (p == s || p->module == m) return MB_WIN_CRT_STARTUP_BAD_STATE;
    s->module = m; s->next = sessions; s->active = 1; sessions = s;
    return MB_WIN_CRT_STARTUP_OK;
}

mb_win_crt_startup_status mb_win_crt_startup_end_session(mb_win_crt_startup_session *s)
{
    if (!s) return MB_WIN_CRT_STARTUP_INVALID_ARGUMENT;
    mb_win_crt_startup_session **p = find(s);
    if (!*p || !s->active) return MB_WIN_CRT_STARTUP_BAD_STATE;
    if (s->module->state != MB_PE_MODULE_EMPTY) return MB_WIN_CRT_STARTUP_BUSY;
    *p = s->next; *s = (mb_win_crt_startup_session){0};
    return MB_WIN_CRT_STARTUP_OK;
}

static int match(mb_pe_string v, const char *s, size_t n, int fold)
{
    if (v.size != n) return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)v.data[i];
        if (fold && c >= 'A' && c <= 'Z') c = (unsigned char)(c + 'a' - 'A');
        if (c != (unsigned char)s[i]) return 0;
    }
    return 1;
}
#define MATCH(v, s, f) match((v), (s), sizeof(s) - 1, (f))

mb_pe_bind_result mb_win_crt_startup_bind(void *context, const mb_pe_import *i, uint64_t *out)
{
    mb_win_crt_startup_session *s = context;
    if (!s || !*find(s) || !s->active || !i || !out ||
        (!i->library.data && i->library.size) || (!i->name.data && i->name.size) ||
        (s->module->state != MB_PE_MODULE_LOADING && s->module->state != MB_PE_MODULE_ACTIVE))
        return MB_PE_BIND_ERROR;
    if (i->by_ordinal) return MB_PE_BIND_UNRESOLVED;
    /* Census-observed runtime library first (every startup_imports row of
     * both retained DLLs), then the documented canonical/adjacent homes. */
    int runtime = MATCH(i->library, "api-ms-win-crt-runtime-l1-1-0.dll", 1) ||
                  MATCH(i->library, "ucrtbase.dll", 1);
    int vcrun = MATCH(i->library, "vcruntime140.dll", 1);
    if (!runtime && !vcrun) return MB_PE_BIND_UNRESOLVED;
    if (MATCH(i->name, "_initterm", 0))
        *out = (uint64_t)(uintptr_t)&svc_initterm;
    else if (MATCH(i->name, "_initterm_e", 0))
        *out = (uint64_t)(uintptr_t)&svc_initterm_e;
    else if (vcrun && MATCH(i->name, "_seh_filter_dll", 0))
        *out = (uint64_t)(uintptr_t)&svc_seh_filter_dll;
    else if (runtime && MATCH(i->name, "_crt_atexit", 0))
        *out = (uint64_t)(uintptr_t)&svc_crt_atexit;
    else if (runtime && MATCH(i->name, "_cexit", 0))
        *out = (uint64_t)(uintptr_t)&svc_cexit;
    else if (runtime && MATCH(i->name, "_initialize_onexit_table", 0))
        *out = (uint64_t)(uintptr_t)&svc_initialize_onexit_table;
    else if (runtime && MATCH(i->name, "_register_onexit_function", 0))
        *out = (uint64_t)(uintptr_t)&svc_register_onexit_function;
    else if (runtime && MATCH(i->name, "_execute_onexit_table", 0))
        *out = (uint64_t)(uintptr_t)&svc_execute_onexit_table;
    else
        return MB_PE_BIND_UNRESOLVED; /* e.g. _initialize_narrow_environment,
                                         _errno, __stdio_common_*: other families */
    return MB_PE_BIND_BOUND;
}
