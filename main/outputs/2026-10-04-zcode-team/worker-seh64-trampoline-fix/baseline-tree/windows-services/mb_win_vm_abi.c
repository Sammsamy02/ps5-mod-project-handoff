#include "mb_win_vm_abi.h"
#if !defined(__x86_64__)
#error "Virtual-memory Windows ABI requires AMD64"
#endif
#define WIN64 __attribute__((ms_abi))

static mb_win_vm *installed;
static mb_win_vm_abi_session *sessions;
static mb_win_vm_abi_session **find(mb_win_vm_abi_session *s)
{ mb_win_vm_abi_session **p = &sessions; while (*p && *p != s) p = &(*p)->next; return p; }
static int live(void) { return installed && installed->active && sessions; }

mb_win_vm_abi_status mb_win_vm_abi_begin_session(mb_win_vm_abi_session *s,
                                                 mb_win_vm *owner, mb_pe_module *m)
{
    if (!s || !owner || !m) return MB_WIN_VM_ABI_INVALID_ARGUMENT;
    if (s->active || s->module || s->next || m->state != MB_PE_MODULE_EMPTY)
        return MB_WIN_VM_ABI_BAD_STATE;
    for (mb_win_vm_abi_session *p = sessions; p; p = p->next)
        if (p == s || p->module == m) return MB_WIN_VM_ABI_BAD_STATE;
    if (installed && installed != owner) return MB_WIN_VM_ABI_BUSY;
    s->module = m; s->next = sessions; s->active = 1; sessions = s;
    if (!installed) installed = owner;
    return MB_WIN_VM_ABI_OK;
}

mb_win_vm_abi_status mb_win_vm_abi_end_session(mb_win_vm_abi_session *s)
{
    if (!s) return MB_WIN_VM_ABI_INVALID_ARGUMENT;
    mb_win_vm_abi_session **p = find(s);
    if (!*p || !s->active) return MB_WIN_VM_ABI_BAD_STATE;
    if (s->module->state != MB_PE_MODULE_EMPTY) return MB_WIN_VM_ABI_BUSY;
    *p = s->next; *s = (mb_win_vm_abi_session){0};
    if (!sessions) installed = (mb_win_vm *)0;
    return MB_WIN_VM_ABI_OK;
}

/* Exactly five Win64 entry points with the documented Microsoft x64
 * signatures; each forwards to the native-CC freestanding owner. Windows
 * DWORD size/length arguments are 32-bit and widen to the owner's size_t;
 * SIZE_T returns narrow to 64 bits. A dead family reports failure/0 without
 * touching the owner (the combined payload only calls through live binds). */
static WIN64 void *svc_virtual_alloc(void *address, uint32_t size,
                                     uint32_t allocation_type, uint32_t protect)
{
    if (!live()) return (void *)0;
    return mb_win_vm_alloc(installed, address, (size_t)size, allocation_type, protect);
}
static WIN64 int32_t svc_virtual_free(void *address, uint32_t size, uint32_t free_type)
{
    if (!live()) return 0;
    return mb_win_vm_free(installed, address, (size_t)size, free_type) == MB_WIN_VM_OK;
}
static WIN64 int32_t svc_virtual_protect(void *address, uint32_t size,
                                         uint32_t new_protect, uint32_t *old_protect)
{
    if (!live()) return 0;
    return mb_win_vm_protect(installed, address, (size_t)size, new_protect,
                             old_protect) == MB_WIN_VM_OK;
}
static WIN64 uint64_t svc_virtual_query(const void *address,
                                        mb_win_vm_basic_info *info, uint32_t length)
{
    if (!live()) return 0;
    return (uint64_t)mb_win_vm_query(installed, address, info, (size_t)length);
}
static WIN64 uint64_t svc_virtual_query_ex(void *process, const void *address,
                                           mb_win_vm_basic_info *info, uint32_t length)
{
    if (!live()) return 0;
    return (uint64_t)mb_win_vm_query_ex(installed, process, address, info, (size_t)length);
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
#define BIND(literal, fn) \
    if (MATCH(import->name, literal, 0)) { \
        *value = (uint64_t)(uintptr_t)&fn; return MB_PE_BIND_BOUND; }

mb_pe_bind_result mb_win_vm_abi_bind(void *context, const mb_pe_import *import,
                                     uint64_t *value)
{
    mb_win_vm_abi_session *session = context;
    if (!session || !*find(session) || !session->active || !import || !value ||
        (!import->library.data && import->library.size) ||
        (!import->name.data && import->name.size) ||
        (session->module->state != MB_PE_MODULE_LOADING &&
         session->module->state != MB_PE_MODULE_ACTIVE))
        return MB_PE_BIND_ERROR;
    if (import->by_ordinal || !MATCH(import->library, "kernel32.dll", 1))
        return MB_PE_BIND_UNRESOLVED;
    BIND("VirtualAlloc", svc_virtual_alloc) BIND("VirtualFree", svc_virtual_free)
    BIND("VirtualProtect", svc_virtual_protect) BIND("VirtualQuery", svc_virtual_query)
    BIND("VirtualQueryEx", svc_virtual_query_ex)
    return MB_PE_BIND_UNRESOLVED;
}
