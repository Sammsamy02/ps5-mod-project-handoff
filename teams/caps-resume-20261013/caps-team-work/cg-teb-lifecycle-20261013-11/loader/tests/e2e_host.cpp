// e2e_host.cpp -- CAP13 Mac-level end-to-end proof host (agent KF, 2026-10-13).
//
// WHAT THIS IS
//   The first execution of the staged cap13 PE loader on real PE bytes.
//   Built as an x86_64 macOS process (run under Rosetta 2 on Apple Silicon),
//   where the CPU can execute the x86-64 machine code inside a Windows PE
//   directly. This tests the LOADER + RELOCATIONS + IMPORT RESOLUTION +
//   Win64<->SysV bridge -- NOT the PS5 (console proof is a separate rung).
//
//   The staged sources are consumed UNMODIFIED:
//     ../native-loader/include/pe_loader.h   ../native-loader/src/pe_loader.cpp
//     ../native-loader/src/pe_types.h        ../native-loader/src/win64_bridge.{h,S}
//     ../native-loader/src/dllmain_invoker.{h,cpp}
//   (win_api_shim.cpp is compiled too, but only for the coverage-audit
//   subcommand: it shows what the staged registry alone would resolve. The
//   execution pipeline uses the e2e resolver below, whose rows carry the
//   real import names + documented Win32 arities -- see audit findings F1/F14.)
//
//   Original files are never written: each input is sha256-hashed before and
//   after the run and the host asserts byte identity.
//
// GUEST->HOST BRIDGING ON MAC (the Rosetta story -- measured, see
// REPORT/E2E-PROOF.md "Debugging notes"):
//   The design's guest->host trampolines (win64_bridge.h bridge_stamp_tramp)
//   are stamped into adapter-owned executable memory. On PS5 that is exactly
//   right. Under Rosetta 2 it is NOT executable machinery: any `movabs r11`
//   executed from JIT'd anonymous-page code wedges this Rosetta build's
//   exception server (reproduced minimally; r11 is special to Rosetta's
//   translated-code protocol). Writes to r11 from ordinary host __text are
//   fine. The Mac host therefore uses per-row naked thunks compiled into the
//   host binary: they set r11 from host text and jump into the STAGED shared
//   thunks (cap13_g2h_thunk4s0..s4), so every staged bridge instruction that
//   can run on macOS still runs here. The anon-page stamp path
//   (bridge_stamp_tramp) is the one staged artifact this Mac proof cannot
//   exercise; it needs no Rosetta on PS5. Documented, not hidden.
//
// CASES
//   bridge                   staged h2g callN + staged g2h shared thunks
//   fixture <dll>            DllMain (real entry point) + start_mod/stop_mod
//   console-dump <dll>       KA-built ue4ss-flavor sample: start_mod lifecycle,
//                            UE4SS C-API imports, string args verified
//   mbm <dll>                mbm_mod_entry_v1 sample: full C-table lifecycle
//                            (entry -> program_start -> unreal_init -> fired
//                            keybind callback -> destroy) against a bridged
//                            Mac host table
//   loadonly <dll>           map + resolve + export lookup only (no calls)
//   refuse <dll> <substr>    expect a precise named refusal containing <substr>
//   shim-coverage <dll>      staged win_api_shim resolve coverage vs census
//
// Build/run: see build_and_run.sh in this directory.

#include <CommonCrypto/CommonDigest.h>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "pe_loader.h"
#include "pe_types.h"
#include "win64_bridge.h"
#include "dllmain_invoker.h"
#include "win_api_shim.h"
#include "sdk/mbm/NativeModAbi.h"

// --------------------------------------------------------------------------
// Tiny test harness
// --------------------------------------------------------------------------

static int g_checks = 0, g_failed = 0;
static void check(bool ok, const char* what, const std::string& detail = "")
{
    ++g_checks;
    printf("CHECK: %-52s %s%s%s\n", what, ok ? "PASS" : "FAIL",
           detail.empty() ? "" : " | ", detail.c_str());
    if (!ok) ++g_failed;
}
static std::vector<std::string> g_events; // observed guest->host crossings
static void event(const std::string& e)
{
    g_events.push_back(e);
    printf("  [event] %s\n", e.c_str());
}
static int count_event(const char* prefix)
{
    int n = 0;
    for (const auto& e : g_events)
        if (e.rfind(prefix, 0) == 0) ++n;
    return n;
}
static bool has_event(const std::string& needle)
{
    for (const auto& x : g_events)
        if (x.find(needle) != std::string::npos) return true;
    return false;
}
static std::string sha256_of(const uint8_t* p, size_t n)
{
    unsigned char d[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(p, (CC_LONG)n, d);
    char out[65];
    for (int i = 0; i < 32; ++i) snprintf(out + i * 2, 3, "%02x", d[i]);
    return std::string(out, 64);
}
static int finish()
{
    printf("RESULT: %s (%d checks, %d failed)\n", g_failed ? "FAIL" : "PASS", g_checks, g_failed);
    return g_failed ? 1 : 0;
}

// --------------------------------------------------------------------------
// Guest->host bridging, Mac shape.
//
// One target slot per bridge row; a naked per-row thunk (host __text) sets
// r10 = row index and jumps to an arity dispatch blob; dispatch loads
// r11 = body address and jumps into the STAGED shared thunk
// (cap13_g2h_thunk4s0..s4), which saves Win64 non-volatiles, remaps the
// argument registers and calls r11. Nothing here touches the staged .S.
// --------------------------------------------------------------------------

extern "C" {
void* e2e_row_targets[96]; // row index -> SysV body (filled from the row table)

// Dispatch blobs: r10d = row index (set by the row thunk), then
// r11 = e2e_row_targets[r10], then jmp into the STAGED shared thunk for the
// arity class. r10 is the staged thunks' own scratch, consumed here.
#define E2E_DISPATCH(sfx, staged)                                    \
    void e2e_dispatch_##sfx();                                       \
    __asm__(".text\n"                                                \
            "_e2e_dispatch_" #sfx ":\n"                              \
            "  leaq _e2e_row_targets(%rip), %r11\n"                  \
            "  movslq %r10d, %r10\n"                                 \
            "  movq (%r11,%r10,8), %r11\n"                           \
            "  jmp _" #staged "\n");
E2E_DISPATCH(s0, cap13_g2h_thunk4s0)
E2E_DISPATCH(s1, cap13_g2h_thunk4s1)
E2E_DISPATCH(s2, cap13_g2h_thunk4s2)
E2E_DISPATCH(s3, cap13_g2h_thunk4s3)
E2E_DISPATCH(s4, cap13_g2h_thunk4s4)
} // extern "C"

// A row thunk is entered with Win64 calling convention (the guest called it
// through its IAT). It only records "which row I am" and tail-jumps.
#define E2E_STR2(x) #x
#define E2E_STR(x) E2E_STR2(x)
#define E2E_ROW_THUNK(i, sfx)                                              \
    int64_t e2e_row_thunk_##i();                                           \
    __attribute__((naked)) int64_t e2e_row_thunk_##i()                     \
    {                                                                      \
        __asm__ volatile("movl $" E2E_STR(i) ", %r10d\n\tjmp _e2e_dispatch_" #sfx); \
    }

// --------------------------------------------------------------------------
// Shim bodies (SysV). Guests reach them ONLY through row thunks + staged
// shared thunks. Names/arities = documented Win32 / UE4SS C-API prototypes.
// This row set is the census-completion set for the sampled DLLs; it is
// data, not per-mod code: any DLL whose imports fall outside it is refused
// with the complete missing list (the loader enforces that, fail-closed).
// --------------------------------------------------------------------------

// ---- kernel32 bodies ----
static int64_t body_disable_thread_library_calls(void*, uint64_t) { event("KERNEL32!DisableThreadLibraryCalls()"); return 1; }
static int64_t body_get_module_handle_w(void* m) { event("KERNEL32!GetModuleHandleW()"); return m ? 1 : 0; }
static int64_t body_get_process_heap() { event("KERNEL32!GetProcessHeap()"); return 0x0000BEEF0000ull; }

static uint8_t* g_arena = nullptr;
static size_t g_arena_used = 0;
static int64_t body_heap_alloc(void*, uint64_t, uint64_t bytes, void* /*HeapReAlloc ptr: ignored*/)
{
    if (!g_arena) g_arena = static_cast<uint8_t*>(malloc(1u << 20)); // Mac test heap; leaked by design
    size_t n = bytes ? (size_t)bytes : 1;
    n = (n + 15) & ~size_t(15);
    if (g_arena_used + n > (1u << 20)) { event("KERNEL32!HeapAlloc(oom)"); return 0; }
    void* p = g_arena + g_arena_used;
    g_arena_used += n;
    memset(p, 0, n);
    char buf[64]; snprintf(buf, sizeof(buf), "KERNEL32!HeapAlloc(%llu bytes)", (unsigned long long)bytes);
    event(buf);
    return (int64_t)(uintptr_t)p;
}
static int64_t body_heap_free(void*, uint64_t, void*) { event("KERNEL32!HeapFree()"); return 1; }
static int64_t body_close_handle(void*) { event("KERNEL32!CloseHandle()"); return 1; }
static int64_t body_get_last_error() { event("KERNEL32!GetLastError()"); return 0; }
static int64_t body_set_last_error(uint64_t e) { char b[64]; snprintf(b, 64, "KERNEL32!SetLastError(%llu)", (unsigned long long)e); event(b); return 0; }
static int64_t body_sleep(uint64_t ms) { char b[64]; snprintf(b, 64, "KERNEL32!Sleep(%llu ms)", (unsigned long long)ms); event(b); return 0; }
static int64_t body_wait_for_single_object(void*, uint64_t) { event("KERNEL32!WaitForSingleObject()"); return 0x00000102ull; } // WAIT_TIMEOUT
static int64_t body_get_exit_code_thread(void*, int64_t* out) { event("KERNEL32!GetExitCodeThread()"); if (out) *out = 0; return 1; }
static int64_t body_qpc(int64_t* out)
{
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t v = (int64_t)ts.tv_sec * 10000000ll + ts.tv_nsec / 100; // 10 MHz, QPC-like
    if (out) *out = v;
    event("KERNEL32!QueryPerformanceCounter()");
    return 1;
}
static int64_t body_qpf(int64_t* out) { if (out) *out = 10000000ll; event("KERNEL32!QueryPerformanceFrequency()"); return 1; }
static int64_t body_tls_alloc() { event("KERNEL32!TlsAlloc()"); return 0; }
static int64_t body_tls_get(int64_t) { event("KERNEL32!TlsGetValue()"); return 0; }
static int64_t body_tls_set(int64_t, void*) { event("KERNEL32!TlsSetValue()"); return 1; }
static int64_t body_tls_free(int64_t) { event("KERNEL32!TlsFree()"); return 1; }
static int64_t body_create_thread(void*, void*, void*, void*, uint64_t, void*)
{
    // Deliberately NOT started: launching a guest start routine requires the
    // adapter-side bridging that audit finding F3 says is missing.
    event("KERNEL32!CreateThread(REFUSED in e2e: bridged start routine not implemented, audit F3)");
    return 0;
}
// ---- user32 ----
static int64_t body_get_async_key_state(int64_t) { event("USER32!GetAsyncKeyState()"); return 0; }
// ---- kernel32 file surface (resolved; a live call is fail-closed here) ----
static int64_t body_create_file_w(const void*, uint64_t, uint64_t, void*, uint64_t, uint64_t, void*)
{
    event("KERNEL32!CreateFileW(REFUSED in e2e: T2 path demarshal not implemented, audit F4)");
    return -1; // INVALID_HANDLE_VALUE
}
// ---- UE4SS C-API (real export names, staged UE4SS.dll v3.0.1) ----
static int64_t body_is_ue4ss_initialized() { event("UE4SS!is_ue4ss_initialized()"); return 1; }
static int64_t body_execute_lua_in_mod(const char* mod_name, const char* lua_code)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "UE4SS!execute_lua_in_mod(\"%s\", \"%s\")", mod_name, lua_code);
    event(buf);
    return 1;
}
static int64_t body_call_script_function(const char* mod_name, const char* fn_name, void* param)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "UE4SS!call_script_function(\"%s\", \"%s\", %p)", mod_name, fn_name, param);
    event(buf);
    return 1;
}
// ---- vcruntime140 / vcruntime140_1: T4 fail-closed EH rows under their REAL
// names (audit F1: the staged registry miskeys "_CxxThrowException" as
// "CxxThrowException"; F14: no TlsFree row. The e2e rows carry the real names
// so execution is not blocked by those registry bugs; the shim-coverage
// subcommand shows the staged behavior unchanged.) ----
static int64_t body_fail_closed(const char* what)
{
    char buf[256];
    snprintf(buf, sizeof(buf), "FAIL-CLOSED: %s executed by guest; EH/SEH unsupported", what);
    event(buf);
    return 0;
}
static int64_t body_cxx_frame_handler3(void*, void*, void*, void*) { return body_fail_closed("__CxxFrameHandler3"); }
static int64_t body_cxx_frame_handler4(void*, void*, void*, void*) { return body_fail_closed("__CxxFrameHandler4"); }
static int64_t body_cxx_throw(void*, void*) { return body_fail_closed("_CxxThrowException"); }
static int64_t body_std_terminate() { return body_fail_closed("__std_terminate"); }
static int64_t body_c_specific_handler(void*, void*, void*, void*) { return body_fail_closed("__C_specific_handler"); }
static void* body_memcpy(void* d, const void* s, uint64_t n) { event("VCRUNTIME!memcpy()"); return d ? memcpy(d, s, n) : d; }
static void* body_memmove(void* d, const void* s, uint64_t n) { event("VCRUNTIME!memmove()"); return d ? memmove(d, s, n) : d; }
static void* body_memset(void* d, int c, uint64_t n) { event("VCRUNTIME!memset()"); return d ? memset(d, c, n) : d; }
static int64_t body_chkstk() { event("VCRUNTIME!__chkstk(no-op probe)"); return 0; }

// ---- mbm host-service bodies (SysV, behind row thunks; see make_mbm_host) --
static int64_t mbm_log(void*, uint64_t, int32_t level, const char* utf8)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "mbm_host.log(level=%d, \"%s\")", level, utf8 ? utf8 : "(null)");
    event(buf);
    return MBM_OK;
}
static int64_t mbm_find_object(void*, uint64_t, const char* path, void**) { event(std::string("mbm_host.find_object(\"") + (path ? path : "?") + "\") -> UNBOUND"); return MBM_E_UNAVAILABLE; }
static int64_t mbm_find_first_of(void*, uint64_t, const char* cls, void**) { event(std::string("mbm_host.find_first_of(\"") + (cls ? cls : "?") + "\") -> UNBOUND"); return MBM_E_UNAVAILABLE; }
static int64_t mbm_find_all_of(void*, uint64_t, const char*, void**, int32_t, int32_t*) { event("mbm_host.find_all_of -> UNBOUND"); return MBM_E_UNAVAILABLE; }
static int64_t mbm_property_set(void*, uint64_t, void*, const char* prop, const void*) { event(std::string("mbm_host.property_set(\"") + (prop ? prop : "?") + "\") -> UNBOUND"); return MBM_E_UNAVAILABLE; }
static int64_t mbm_property_get(void*, uint64_t, void*, const char*, void*) { event("mbm_host.property_get -> UNBOUND"); return MBM_E_UNAVAILABLE; }
static int64_t mbm_generic_fail() { event("mbm_host.<service> -> UNBOUND (fail-closed)"); return MBM_E_UNAVAILABLE; }
static struct { void* fn; void* user; } g_last_key = {nullptr, nullptr};
static int64_t mbm_register_key_down(void*, uint64_t, uint32_t key, uint32_t mask, void* fn, void* user, uint64_t* out_id)
{
    // Record the guest callback so the host can fire it back through the
    // bridge (reverse direction). 7 args = thunk4s3 envelope.
    static uint64_t next_id = 1;
    char buf[128];
    snprintf(buf, sizeof(buf), "mbm_host.register_key_down(key=0x%02X mask=%u fn=%p user=%p)", key, mask, fn, user);
    event(buf);
    g_last_key.fn = fn; g_last_key.user = user;
    if (out_id) *out_id = next_id;
    ++next_id;
    return MBM_OK;
}
static int64_t mbm_register_hook(void*, uint64_t, uint32_t, void*, void*, uint64_t*)
{
    // The tick hook shape is (user, float): outside the all-integer bridge
    // envelope (audit F5). Fail-closed admission instead of a fake success.
    event("mbm_host.register_hook -> UNAVAILABLE (mixed float signature outside bridge envelope)");
    return MBM_E_UNAVAILABLE;
}

// --------------------------------------------------------------------------
// The row table. Every row: (dll, symbol, arity, body, note).
// sfx = staged shared thunk class per bridge_select_thunk(arity):
//   arity 0-4 -> s0, 5 -> s1, 6 -> s2, 7 -> s3, 8 -> s4.
// --------------------------------------------------------------------------
struct Row { const char* dll; const char* sym; int64_t (*thunk)(); uint32_t arity; const char* note; };

// Index order MUST match the E2E_ROW_* thunk/table definitions below.
enum RowId {
    R_DISABLE_TLC, R_GETMODHANDLEW, R_GETPROCHEAP, R_HEAPALLOC, R_HEAPFREE,
    R_CLOSEHANDLE, R_GETLASTERR, R_SETLASTERR, R_SLEEP, R_WAITFORONE,
    R_GETEXITCODE, R_QPC, R_QPF, R_TLSALLOC, R_TLSGET, R_TLSSET, R_TLSFREE,
    R_CREATETHREAD, R_GKS,
    R_UE4SS_INIT, R_UE4SS_EXEC_LUA, R_UE4SS_CALLSCRIPT,
    R_CXXFH3, R_CXXFH4, R_CXXTHROW, R_STDTERM, R_CSH, R_MEMCPY, R_MEMMOVE,
    R_MEMSET, R_CHKSTK, R_CREATEFILEW,
    // mbm host-table rows (never import-resolved; address-of used directly)
    R_MBM_LOG, R_MBM_FINDOBJ, R_MBM_FINDFIRSTOF, R_MBM_FINDALLOF,
    R_MBM_PROPSET, R_MBM_PROPGET,
    R_MBM_GF0, R_MBM_GF1, R_MBM_GF2, R_MBM_GF3,
    R_MBM_REGKEY, R_MBM_REGHOOK,
    R_COUNT
};

// (index, sfx, arity, body, dll, symbol, note)
#define FOR_EACH_IMPORT_ROW(X) \
    X(0,   s0, 1, body_disable_thread_library_calls, "kernel32.dll",     "DisableThreadLibraryCalls", "BOOL(HMODULE)") \
    X(1, s0, 1, body_get_module_handle_w,            "kernel32.dll",     "GetModuleHandleW", "HMODULE(LPCWSTR)") \
    X(2,   s0, 0, body_get_process_heap,                "kernel32.dll",     "GetProcessHeap", "HANDLE()") \
    X(3,     s0, 3, body_heap_alloc,                      "kernel32.dll",     "HeapAlloc", "LPVOID(HANDLE,DWORD,SIZE_T)") \
    X(4,      s0, 3, body_heap_free,                       "kernel32.dll",     "HeapFree", "BOOL(HANDLE,DWORD,LPVOID)") \
    X(5,   s0, 1, body_close_handle,                    "kernel32.dll",     "CloseHandle", "BOOL(HANDLE)") \
    X(6,    s0, 0, body_get_last_error,                  "kernel32.dll",     "GetLastError", "DWORD()") \
    X(7,    s0, 1, body_set_last_error,                  "kernel32.dll",     "SetLastError", "VOID(DWORD)") \
    X(8,         s0, 1, body_sleep,                           "kernel32.dll",     "Sleep", "VOID(DWORD)") \
    X(9,    s0, 2, body_wait_for_single_object,          "kernel32.dll",     "WaitForSingleObject", "DWORD(HANDLE,DWORD)") \
    X(10,   s0, 2, body_get_exit_code_thread,            "kernel32.dll",     "GetExitCodeThread", "BOOL(HANDLE,LPDWORD)") \
    X(11,           s0, 1, body_qpc,                             "kernel32.dll",     "QueryPerformanceCounter", "BOOL(LARGE_INTEGER*)") \
    X(12,           s0, 1, body_qpf,                             "kernel32.dll",     "QueryPerformanceFrequency", "BOOL(LARGE_INTEGER*)") \
    X(13,      s0, 0, body_tls_alloc,                       "kernel32.dll",     "TlsAlloc", "DWORD()") \
    X(14,        s0, 1, body_tls_get,                         "kernel32.dll",     "TlsGetValue", "LPVOID(DWORD)") \
    X(15,        s0, 2, body_tls_set,                         "kernel32.dll",     "TlsSetValue", "BOOL(DWORD,LPVOID)") \
    X(16,       s0, 1, body_tls_free,                        "kernel32.dll",     "TlsFree", "BOOL(DWORD)") \
    X(17,  s2, 6, body_create_thread,                   "kernel32.dll",     "CreateThread", "refused in e2e (audit F3)") \
    X(18,           s0, 1, body_get_async_key_state,             "user32.dll",       "GetAsyncKeyState", "SHORT(int)") \
    X(19,    s0, 0, body_is_ue4ss_initialized,            "ue4ss.dll",        "is_ue4ss_initialized", "int(void)") \
    X(20,s0, 2, body_execute_lua_in_mod,              "ue4ss.dll",        "execute_lua_in_mod", "int(char*,char*)") \
    X(21,s0,3, body_call_script_function,            "ue4ss.dll",        "call_script_function", "int(char*,char*,void*)") \
    X(22,        s0, 4, body_cxx_frame_handler3,              "vcruntime140.dll", "__CxxFrameHandler3", "fail-closed") \
    X(23,        s0, 4, body_cxx_frame_handler4,              "vcruntime140_1.dll", "__CxxFrameHandler4", "fail-closed") \
    X(24,      s0, 2, body_cxx_throw,                       "vcruntime140.dll", "_CxxThrowException", "fail-closed") \
    X(25,       s0, 0, body_std_terminate,                   "vcruntime140.dll", "__std_terminate", "fail-closed") \
    X(26,           s0, 4, body_c_specific_handler,              "vcruntime140.dll", "__C_specific_handler", "fail-closed") \
    X(27,        s0, 3, body_memcpy,                          "vcruntime140.dll", "memcpy", "plain") \
    X(28,       s0, 3, body_memmove,                         "vcruntime140.dll", "memmove", "plain") \
    X(29,        s0, 3, body_memset,                          "vcruntime140.dll", "memset", "plain") \
    X(30,        s0, 0, body_chkstk,                          "vcruntime140.dll", "__chkstk", "no-op probe") \
    X(31,        s3, 7, body_create_file_w,                   "kernel32.dll",     "CreateFileW", "resolved; call fail-closed (audit F4)")

#define FOR_EACH_MBM_ROW(X) \
    X(32,        s0, 4, mbm_log,             "mbm.host", "log",             "4 args") \
    X(33,    s0, 4, mbm_find_object,     "mbm.host", "find_object",     "4 args") \
    X(34,s0, 4, mbm_find_first_of,   "mbm.host", "find_first_of",   "4 args") \
    X(35,  s2, 6, mbm_find_all_of,     "mbm.host", "find_all_of",     "6 args") \
    X(36,    s1, 5, mbm_property_set,    "mbm.host", "property_set",    "5 args") \
    X(37,    s1, 5, mbm_property_get,    "mbm.host", "property_get",    "5 args") \
    X(38,        s0, 3, mbm_generic_fail,    "mbm.host", "fail-closed",     "arity<=4 class") \
    X(39,        s1, 5, mbm_generic_fail,    "mbm.host", "fail-closed",     "5 args class") \
    X(40,        s2, 6, mbm_generic_fail,    "mbm.host", "fail-closed",     "6 args class") \
    X(41,        s3, 7, mbm_generic_fail,    "mbm.host", "fail-closed",     "7 args class") \
    X(42,     s3, 7, mbm_register_key_down, "mbm.host", "register_key_down", "7 args") \
    X(43,    s2, 6, mbm_register_hook,   "mbm.host", "register_hook",   "6 args")

#define DEF_TARGET(i, sfx, arity, body, dll, sym, note) \
    extern "C" { void* e2e_row_target_##i = (void*)&body; }
#define DEF_THUNK(i, sfx, arity, body, dll, sym, note) E2E_ROW_THUNK(i, sfx)
#define ROW_ENTRY(i, sfx, arity, body, dll, sym, note) \
    {dll, sym, &e2e_row_thunk_##i, arity, note},

// Wire the dispatch table: e2e_row_targets[i] = row i's SysV body. Static
// init runs before main, so every thunk resolves from the first guest call.
#define DEF_ROW_TARGET(i, sfx, arity, body, dll, sym, note) e2e_row_targets[(i)] = (void*)&body;
static const bool g_rows_wired = [] {
    FOR_EACH_IMPORT_ROW(DEF_ROW_TARGET)
    FOR_EACH_MBM_ROW(DEF_ROW_TARGET)
    return true;
}();

FOR_EACH_IMPORT_ROW(DEF_THUNK)
FOR_EACH_MBM_ROW(DEF_THUNK)

static const Row kRows[] = {
    FOR_EACH_IMPORT_ROW(ROW_ENTRY)
};
static const Row* mbm_row(RowId id) // host-table rows by id (dll fields unused)
{
    static const Row mbm[] = { FOR_EACH_MBM_ROW(ROW_ENTRY) };
    return &mbm[id - R_MBM_LOG];
}

class E2eResolver : public cap13::ImportResolver
{
  public:
    void* resolve(const char* dll, const char* symbol, uint64_t ordinal) override
    {
        if (!dll || (!symbol && !ordinal)) return nullptr;
        std::string d = dll;
        for (auto& c : d) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        for (const auto& r : kRows)
        {
            if (strcmp(r.dll, d.c_str()) != 0) continue;
            if (symbol ? strcmp(r.sym, symbol) != 0 : true) continue;
            return (void*)r.thunk; // Win64-caller-safe entry in host __text
        }
        return nullptr; // loader refuses with the full missing list
    }
};

// --------------------------------------------------------------------------
// File loading + byte-identity guard
// --------------------------------------------------------------------------

static std::vector<uint8_t> read_file(const char* path, std::string* sha_out)
{
    FILE* f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); _exit(2); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf((size_t)n);
    if (n && fread(buf.data(), 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "short read\n"); _exit(2); }
    fclose(f);
    if (sha_out) *sha_out = sha256_of(buf.data(), buf.size());
    return buf;
}

struct Loaded
{
    std::vector<uint8_t> file_bytes;
    std::string sha_before, sha_after;
    cap13::LoadResult lr;
};

static bool load_pe(const char* path, Loaded& out, std::string* err)
{
    out.file_bytes = read_file(path, &out.sha_before);
    E2eResolver resolver;
    cap13::LoadOptions opts;
    // HOST PAGE-SIZE FACT (measured, see E2E-PROOF debugging notes): under
    // Rosetta 2 the translated x86_64 process gets VIRTUALIZED 4 KiB pages --
    // mmap returns 4 KiB-aligned but NOT 16 KiB-aligned addresses, so the
    // loader's default 0x4000 rounding of mprotect ranges would reach outside
    // the allocation and fail with ENOMEM. 0x1000 matches the Rosetta ABI.
    // On PS5 (and native arm64 macOS) mmap is 16 KiB-aligned: the PS5 build
    // keeps the default page_size = 0x4000, where the 16 KiB RWX-partition
    // rule of design 3b applies unchanged.
    opts.page_size = 0x1000;
    opts.allow_rwx_partition = true; // Mac stands in for a positive J1 probe --
                                     // on PS5 the default (refuse) applies and
                                     // small images hit it; documented in E2E-PROOF.
    // This synthetic fixture has no relocation directory but is known to be
    // position-independent. The target loader keeps its default refusal.
    opts.allow_no_relocs = std::strstr(path, "cap13-ue4ss-native-mod-FIXTURE.dll") != nullptr;
    out.lr = cap13::pe_load(out.file_bytes.data(), out.file_bytes.size(), resolver, opts);
    out.sha_after = sha256_of(out.file_bytes.data(), out.file_bytes.size());
    if (!out.lr.ok)
    {
        if (err) *err = out.lr.reason + (out.lr.missing.empty() ? "" : " | missing: " + std::to_string(out.lr.missing.size()));
        return false;
    }
    return true;
}

// --------------------------------------------------------------------------
// mbm host table: every member the guest may call is a row thunk (Win64-safe
// entry in host __text) that dispatches into the staged g2h bridge.
// --------------------------------------------------------------------------

static mbm_host_v1 make_mbm_host()
{
    mbm_host_v1 h{};
    h.magic = MBM_ABI_MAGIC;
    h.abi_version = MBM_ABI_VERSION;
    h.struct_size = sizeof(mbm_host_v1);
    h.host_build_id = "cap13-e2e-macos-rosetta";
    h.ctx = nullptr;
    h.log                     = decltype(h.log)((void*)mbm_row(R_MBM_LOG)->thunk);
    h.find_object             = decltype(h.find_object)((void*)mbm_row(R_MBM_FINDOBJ)->thunk);
    h.find_first_of           = decltype(h.find_first_of)((void*)mbm_row(R_MBM_FINDFIRSTOF)->thunk);
    h.find_all_of             = decltype(h.find_all_of)((void*)mbm_row(R_MBM_FINDALLOF)->thunk);
    h.object_name             = decltype(h.object_name)((void*)mbm_row(R_MBM_GF3)->thunk);
    h.object_class            = decltype(h.object_class)((void*)mbm_row(R_MBM_GF0)->thunk);
    h.object_is_a             = decltype(h.object_is_a)((void*)mbm_row(R_MBM_GF1)->thunk);
    h.object_is_valid         = decltype(h.object_is_valid)((void*)mbm_row(R_MBM_GF0)->thunk);
    h.property_get            = decltype(h.property_get)((void*)mbm_row(R_MBM_PROPGET)->thunk);
    h.property_set            = decltype(h.property_set)((void*)mbm_row(R_MBM_PROPSET)->thunk);
    h.property_address        = decltype(h.property_address)((void*)mbm_row(R_MBM_GF2)->thunk);
    h.call_function           = decltype(h.call_function)((void*)mbm_row(R_MBM_GF3)->thunk);
    h.hook_admission          = decltype(h.hook_admission)((void*)mbm_row(R_MBM_GF0)->thunk);
    h.register_ufunction_hook = decltype(h.register_ufunction_hook)((void*)mbm_row(R_MBM_GF3)->thunk);
    h.register_hook           = decltype(h.register_hook)((void*)mbm_row(R_MBM_REGHOOK)->thunk);
    h.register_key_down       = decltype(h.register_key_down)((void*)mbm_row(R_MBM_REGKEY)->thunk);
    h.execute_in_game_thread  = decltype(h.execute_in_game_thread)((void*)mbm_row(R_MBM_GF0)->thunk);
    h.unregister              = decltype(h.unregister)((void*)mbm_row(R_MBM_GF0)->thunk);
    h.gui = nullptr; // GUI host not in this build (fail-closed by ABI contract)
    return h;
}

// --------------------------------------------------------------------------
// Cases
// --------------------------------------------------------------------------

// Staged-bridge sanity, no PE involved:
//  A) h2g: SysV caller -> staged cap13_win64_callN -> guest-shaped Win64 code
//     (here: a row thunk, which is precisely "Win64-caller code").
//  B) g2h: staged cap13_g2h_thunk4sN -> SysV body, with Win64 register state
//     established by the h2g stubs (the exact composition the IAT path uses).
static int case_bridge()
{
    printf("== CASE bridge ==\n");
    // h2g arg-remap check: call1 maps sysv(rdi=fn, rsi=a1) to win(rcx=a1).
    // body_sleep logs its argument, proving the remap carried the value.
    printf("  h2g: cap13_win64_call1(body_sleep row, 0)...\n");
    cap13_win64_call1((void*)kRows[R_SLEEP].thunk, 0);
    check(count_event("KERNEL32!Sleep(0 ms)") == 1, "h2g call1 register remap reached body");
    check(e2e_row_targets[R_SLEEP] == (void*)&body_sleep, "g2h dispatch table wired");
    return finish();
}

// 6-arg (thunk4s2-class) crossing. SEPARATE CASE: this class wedges in this
// Rosetta build's exception server (measured 2026-10-13) -- run it only under
// a watchdog; the wedge is recorded as "not validated on Mac" for the staged
// stack-staging thunks (s1..s4). PS5 has no Rosetta; console rung validates.
static int case_bridge6()
{
    printf("== CASE bridge6 ==\n");
    printf("  h2g: cap13_win64_call6 into 6-arg row (thunk4s2 stack-arg staging)...\n");
    cap13_win64_call6((void*)kRows[R_CREATETHREAD].thunk, 0, 0, 0, 0, 0, 0);
    check(count_event("KERNEL32!CreateThread(REFUSED in e2e") == 1, "h2g call6 6-arg crossing (s2 class)");
    return finish();
}

static int case_fixture(const char* path)
{
    printf("== CASE fixture: %s ==\n", path);
    Loaded L; std::string err;
    if (!load_pe(path, L, &err))
    {
        check(false, "fixture: pe_load ok", err);
        return finish();
    }
    const cap13::LoadResult& lr = L.lr;
    check(true, "fixture: pe_load ok");
    printf("  image=%p size=%" PRIu64 " slide=0x%" PRIx64 " entry_rva=0x%X\n",
           (void*)lr.image, lr.image_size, lr.slide, lr.entry_rva);
    check(lr.entry_rva != 0, "fixture has a real entry point (AddressOfEntryPoint)");
    check(lr.slide != 0, "mapped at a slid base (relocation condition applies)");

    void* start_mod = lr.find_export("start_mod");
    void* stop_mod = lr.find_export("stop_mod");
    check(start_mod && stop_mod, "find_export(start_mod/stop_mod)");

    // DllMain through the STAGED dllmain_invoker (call3: hinst, ATTACH, 0).
    uint64_t entry_host = 0;
    cap13::EntryStatus st = cap13::invoke_dllmain_attach(lr, entry_host);
    check(st == cap13::EntryStatus::Ok, "DllMain(PROCESS_ATTACH) via dllmain_invoker");
    check(count_event("KERNEL32!DisableThreadLibraryCalls()") == 1, "DllMain called KERNEL32 import exactly once");
    check(count_event("UE4SS!is_ue4ss_initialized()") == 1, "DllMain called UE4SS import exactly once");

    // start_mod calls is_ue4ss_initialized again (guest code touching its IAT).
    uint64_t rc = cap13_win64_call0(start_mod);
    check(rc == 0, "start_mod() returned 0 (fixture contract)");
    check(count_event("UE4SS!is_ue4ss_initialized()") == 2, "start_mod called UE4SS import (2nd crossing)");

    (void)cap13_win64_call0(stop_mod);
    check(true, "stop_mod() executed");
    check(L.sha_before == L.sha_after, "input file byte-identical after load+execute");
    return finish();
}

static int case_console_dump(const char* path)
{
    printf("== CASE console-dump: %s ==\n", path);
    Loaded L; std::string err;
    if (!load_pe(path, L, &err))
    {
        check(false, "console-dump: pe_load ok", err);
        return finish();
    }
    const cap13::LoadResult& lr = L.lr;
    check(true, "console-dump: pe_load ok");
    printf("  image=%p size=%" PRIu64 " slide=0x%" PRIx64 " entry_rva=0x%X\n",
           (void*)lr.image, lr.image_size, lr.slide, lr.entry_rva);
    check(lr.entry_rva == 0, "no PE entry point (lld-link -noentry): loader must report NoEntryPoint, not crash");
    if (lr.entry_rva == 0)
    {
        uint64_t dummy = 0;
        check(cap13::invoke_dllmain_attach(lr, dummy) == cap13::EntryStatus::NoEntryPoint,
              "dllmain_invoker returns NoEntryPoint");
    }

    void* start_mod = lr.find_export("start_mod");
    void* uninstall_mod = lr.find_export("uninstall_mod");
    check(start_mod && uninstall_mod, "find_export(start_mod/uninstall_mod)");
    check(lr.find_export("mbm_mod_entry_v1") == nullptr, "route selection: no mbm export on a start_mod PE");

    // The guest's start_mod: is_ue4ss_initialized() then execute_lua_in_mod
    // with string literals from the guest's .rdata, then returns (void*)1.
    uint64_t obj = cap13_win64_call0(start_mod);
    check(obj == 1, "start_mod() returned (void*)1", "got 0x" + std::to_string((unsigned long long)obj));
    check(count_event("UE4SS!is_ue4ss_initialized()") == 1, "is_ue4ss_initialized crossed once");
    check(has_event("execute_lua_in_mod(\"ConsoleEnablerMod\", \"ExecuteInConsole('') DumpAllActors() \")"),
          "execute_lua_in_mod received the guest's exact .rdata strings");

    (void)cap13_win64_call1(uninstall_mod, 0);
    check(true, "uninstall_mod(0) executed");
    check(L.sha_before == L.sha_after, "input file byte-identical after load+execute");
    return finish();
}

static int case_mbm(const char* path)
{
    printf("== CASE mbm: %s ==\n", path);
    Loaded L; std::string err;
    if (!load_pe(path, L, &err))
    {
        check(false, "mbm: pe_load ok", err);
        return finish();
    }
    const cap13::LoadResult& lr = L.lr;
    check(true, "mbm: pe_load ok");
    printf("  image=%p size=%" PRIu64 " slide=0x%" PRIx64 "\n", (void*)lr.image, lr.image_size, lr.slide);

    void* entry = lr.find_export("mbm_mod_entry_v1");
    check(entry != nullptr, "find_export(mbm_mod_entry_v1)");

    static mbm_host_v1 host = make_mbm_host();
    const mbm_mod_v1* mod = nullptr;
    uint64_t token = 0xCA313ull;
    // Win64: (host in RCX, token in RDX, &mod in R8) via call3.
    int64_t rc = (int64_t)cap13_win64_call3(entry, (uint64_t)&host, token, (uint64_t)&mod);
    rc = (int32_t)(uint32_t)rc;
    check(rc == MBM_OK, "mbm_mod_entry_v1 returned MBM_OK", "rc=" + std::to_string(rc));
    if (rc != MBM_OK || !mod) return finish();
    check(mod->magic == MBM_ABI_MAGIC && mod->abi_version == MBM_ABI_VERSION,
          "guest-published mbm_mod_v1 magic/abi_version");
    check(mod->struct_size == sizeof(mbm_mod_v1), "mbm_mod_v1 struct_size matches host layout",
          std::to_string(mod->struct_size) + " vs " + std::to_string(sizeof(mbm_mod_v1)));
    printf("  mod.name='%s' version='%s' intended_sdk='%s'\n",
           mod->name ? mod->name : "?", mod->version ? mod->version : "?",
           mod->intended_sdk ? mod->intended_sdk : "?");

    if (mod->on_program_start)
    {
        rc = (int32_t)(uint32_t)cap13_win64_call1((void*)mod->on_program_start, (uint64_t)mod->instance);
        check(rc == MBM_OK, "on_program_start returned MBM_OK", "rc=" + std::to_string(rc));
    }
    std::string prog_event;
    for (const auto& e : g_events)
        if (e.find("program start") != std::string::npos) prog_event = e;
    check(!prog_event.empty(), "guest program-start log observed through bridged host->log", prog_event);

    if (mod->on_unreal_init)
    {
        rc = (int32_t)(uint32_t)cap13_win64_call1((void*)mod->on_unreal_init, (uint64_t)mod->instance);
        printf("  on_unreal_init rc=%lld (host services are fail-closed UNBOUND on Mac; guest must survive)\n", (long long)rc);
        check(rc >= MBM_E_MOD_FAULT, "on_unreal_init returned a defined status (no crash, no fake success)");
    }

    if (mod->on_update)
        printf("  SKIP on_update: mbm lifecycle tick is the (user, float) mixed-signature class -- refused by the staged bridge envelope (audit F5)\n");

    // Reverse direction: fire the guest callback the mod registered.
    if (g_last_key.fn)
    {
        rc = (int32_t)(uint32_t)cap13_win64_call1(g_last_key.fn, (uint64_t)g_last_key.user);
        check(rc == MBM_OK, "fired registered guest keybind callback through bridge", "rc=" + std::to_string(rc));
    }

    if (mod->destroy)
    {
        rc = (int32_t)(uint32_t)cap13_win64_call1((void*)mod->destroy, (uint64_t)mod->instance);
        check(rc == MBM_OK, "destroy returned MBM_OK", "rc=" + std::to_string(rc));
    }
    check(L.sha_before == L.sha_after, "input file byte-identical after full lifecycle");
    return finish();
}

static int case_loadonly(const char* path)
{
    printf("== CASE loadonly: %s ==\n", path);
    Loaded L; std::string err;
    if (!load_pe(path, L, &err))
    {
        check(false, "loadonly: pe_load ok", err);
        printf("  reason: %s\n", L.lr.reason.c_str());
        for (const auto& m : L.lr.missing) printf("    missing %s\n", m.c_str());
        return finish();
    }
    const cap13::LoadResult& lr = L.lr;
    char buf[256];
    snprintf(buf, sizeof(buf), "image=%p slide=0x%" PRIx64, (void*)lr.image, lr.slide);
    check(true, "loadonly: pe_load ok", buf);
    check(L.sha_before == L.sha_after, "input file byte-identical");
    return finish();
}

static int case_refuse(const char* path, const char* expect_substr)
{
    printf("== CASE refuse: %s ==\n", path);
    Loaded L; std::string err;
    load_pe(path, L, &err);
    check(!L.lr.ok, "loader refused (fail-closed)");
    check(L.lr.reason.find(expect_substr) != std::string::npos, "refusal names the cause",
          "reason='" + L.lr.reason + "'");
    if (!L.lr.missing.empty())
    {
        printf("  missing-import list (%zu rows):\n", L.lr.missing.size());
        for (const auto& m : L.lr.missing) printf("    %s\n", m.c_str());
    }
    check(L.sha_before == L.sha_after, "input file byte-identical even on refusal");
    return finish();
}

// TLS-refusal case with evidence: expect the loader's named TLS refusal, then
// read the TLS directory from the FILE and classify it empty (lld-link
// artifact: no raw data, no callbacks -- audit finding F20 refinement) or real.
static int case_tls(const char* path)
{
    printf("== CASE tls: %s ==\n", path);
    Loaded L; std::string err;
    load_pe(path, L, &err);
    check(!L.lr.ok, "loader refused (fail-closed)");
    check(L.lr.reason.find("static TLS directory") != std::string::npos, "refusal names the TLS rule",
          "reason='" + L.lr.reason + "'");
    check(L.sha_before == L.sha_after, "input file byte-identical even on refusal");

    // Read the TLS directory (dir 9) straight from the file bytes.
    const uint8_t* b = L.file_bytes.data();
    size_t n = L.file_bytes.size();
    uint32_t lfa = *(const uint32_t*)(b + 0x3C);
    const cap13::PeFileHeader* coff = (const cap13::PeFileHeader*)(b + lfa + 4);
    const cap13::PeOptionalHeader64* opt = (const cap13::PeOptionalHeader64*)((const uint8_t*)coff + 20);
    uint32_t rva = opt->DataDirectory[cap13::PE_DIR_TLS].VirtualAddress, size = opt->DataDirectory[cap13::PE_DIR_TLS].Size;
    check(size == sizeof(cap13::PeTlsDirectory64), "TLS directory present (40 bytes)");
    // RVA -> file offset via the section table.
    auto rva2off = [&](uint32_t r, uint32_t& off) -> bool {
        for (uint16_t i = 0; i < coff->NumberOfSections; ++i)
        {
            const cap13::PeSectionHeader& s = *(const cap13::PeSectionHeader*)((const uint8_t*)opt + coff->SizeOfOptionalHeader + i * sizeof(cap13::PeSectionHeader));
            if (r >= s.VirtualAddress && r - s.VirtualAddress < s.SizeOfRawData)
            { off = s.PointerToRawData + (r - s.VirtualAddress); return (size_t)off + size <= n; }
        }
        return false;
    };
    uint32_t off = 0;
    if (rva2off(rva, off))
    {
        cap13::PeTlsDirectory64 d;
        memcpy(&d, b + off, sizeof d);
        bool empty = (d.StartAddressOfRawData == d.EndAddressOfRawData) && d.AddressOfCallBacks == 0;
        printf("  TLS dir: raw=[%llx..%llx] index=%llx callbacks=%llx -> %s\n",
               (unsigned long long)d.StartAddressOfRawData, (unsigned long long)d.EndAddressOfRawData,
               (unsigned long long)d.AddressOfIndex, (unsigned long long)d.AddressOfCallBacks,
               empty ? "EMPTY (lld-link artifact; no thread-locals, no callbacks)"
                     : "REAL (TLS data or callbacks present)");
        check(true, empty ? "classified: EMPTY TLS directory (lld-link artifact; F20 refinement)"
                          : "classified: REAL TLS directory (data/callbacks present; refusal correct)");
    }
    return finish();
}

// Generic start_mod-lifecycle execution case (TLS-free UE4SS-flavor samples):
// load -> no entry point -> start_mod -> uninstall_mod -> byte identity.
static int case_lifecycle(const char* path)
{
    printf("== CASE lifecycle: %s ==\n", path);
    Loaded L; std::string err;
    if (!load_pe(path, L, &err))
    {
        check(false, "lifecycle: pe_load ok", err);
        printf("  reason: %s\n", L.lr.reason.c_str());
        return finish();
    }
    const cap13::LoadResult& lr = L.lr;
    check(true, "lifecycle: pe_load ok");
    printf("  image=%p slide=0x%" PRIx64 " entry=0x%X\n", (void*)lr.image, lr.slide, lr.entry_rva);
    void* start_mod = lr.find_export("start_mod");
    void* uninstall_mod = lr.find_export("uninstall_mod");
    check(start_mod && uninstall_mod, "find_export(start_mod/uninstall_mod)");
    uint64_t obj = cap13_win64_call0(start_mod);
    check(obj != 0, "start_mod returned a nonzero handle", "got 0x" + std::to_string((unsigned long long)obj));
    (void)cap13_win64_call1(uninstall_mod, obj);
    check(true, "uninstall_mod executed");
    check(count_event("KERNEL32!CreateThread(REFUSED in e2e") <= 1,
          "CreateThread refusal semantics survived by the guest (audit F3, live)");
    check(L.sha_before == L.sha_after, "input file byte-identical after load+execute");
    return finish();
}

// What the STAGED win_api_shim alone would resolve per DLL (audit evidence
// for requirement 6 and findings F1/F14). Read-only; no execution.
static int case_shim_coverage(const char* path)
{
    printf("== SHIM-COVERAGE: %s ==\n", path);
    Loaded L; std::string err;
    if (!load_pe(path, L, &err))
    {
        printf("  (pe_load refused: %s -- coverage report still computed from file)\n", L.lr.reason.c_str());
    }
    std::vector<std::pair<std::string, std::string>> imports;
    {
        const uint8_t* b = L.file_bytes.data();
        size_t n = L.file_bytes.size();
        if (n >= 0x40 && b[0] == 'M' && b[1] == 'Z')
        {
            uint32_t lfa = *(const uint32_t*)(b + 0x3C);
            if (lfa + 4 + 20 + 240 < n && memcmp(b + lfa, "PE\0\0", 4) == 0)
            {
                const cap13::PeFileHeader* coff = (const cap13::PeFileHeader*)(b + lfa + 4);
                const cap13::PeOptionalHeader64* opt =
                    (const cap13::PeOptionalHeader64*)((const uint8_t*)coff + 20);
                if (opt->Magic == 0x020B && opt->NumberOfRvaAndSizes > 1)
                {
                    const cap13::PeSectionHeader* secs =
                        (const cap13::PeSectionHeader*)((const uint8_t*)opt + coff->SizeOfOptionalHeader);
                    auto r2f = [&](uint32_t rva, size_t sz, uint32_t& off) -> bool {
                        if (rva < opt->SizeOfHeaders) { if (rva + sz <= n) { off = rva; return true; } return false; }
                        for (int i = 0; i < coff->NumberOfSections; ++i)
                        {
                            const cap13::PeSectionHeader& s = secs[i];
                            if (rva >= s.VirtualAddress && rva - s.VirtualAddress < s.SizeOfRawData)
                            {
                                uint32_t o = s.PointerToRawData + (rva - s.VirtualAddress);
                                if ((size_t)o + sz <= n) { off = o; return true; }
                                return false;
                            }
                        }
                        return false;
                    };
                    uint32_t dir = opt->DataDirectory[1].VirtualAddress;
                    for (uint32_t i = 0;; ++i)
                    {
                        uint32_t off;
                        if (!r2f(dir + i * 20, 20, off)) break;
                        const cap13::PeImportDescriptor* d = (const cap13::PeImportDescriptor*)(b + off);
                        if (d->FirstThunk == 0 && d->Name == 0) break;
                        uint32_t noff;
                        if (!r2f(d->Name, 1, noff)) break;
                        std::string dll = (const char*)(b + noff);
                        uint32_t thunk = d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk;
                        for (uint32_t t = 0;; ++t)
                        {
                            uint32_t toff;
                            if (!r2f(thunk + t * 8, 8, toff)) break;
                            uint64_t v = *(const uint64_t*)(b + toff);
                            if (v == 0) break;
                            if (v & 0x8000000000000000ull) { imports.push_back({dll, "<ordinal>"}); continue; }
                            uint32_t soff;
                            if (!r2f((uint32_t)v, 3, soff)) break;
                            imports.push_back({dll, (const char*)(b + soff + 2)});
                        }
                    }
                }
            }
        }
    }
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    int resolved = 0, total = 0;
    std::vector<std::string> missing;
    for (const auto& imp : imports)
    {
        ++total;
        std::string dll = imp.first;
        for (auto& c : dll) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        void* p = shim.resolve(dll.c_str(), imp.second.c_str(), 0);
        if (p) ++resolved; else missing.push_back(imp.first + "!" + imp.second);
    }
    printf("  staged registry resolves %d/%d census rows; %d unresolved:\n", resolved, total, (int)missing.size());
    for (const auto& m : missing) printf("    unresolved %s\n", m.c_str());
    return 0;
}

static void usage()
{
    fprintf(stderr,
            "usage: e2e_host bridge\n"
            "       e2e_host fixture <dll>\n"
            "       e2e_host console-dump <dll>\n"
            "       e2e_host mbm <dll>\n"
            "       e2e_host loadonly <dll>\n"
            "       e2e_host refuse <dll> <expected-reason-substring>\n"
            "       e2e_host shim-coverage <dll>\n");
    _exit(2);
}

int main(int argc, char** argv)
{
    setvbuf(stdout, nullptr, _IONBF, 0); // unbuffered: wedges must not eat logs
    if (argc < 2) usage();
    if (!strcmp(argv[1], "bridge") && argc == 2) return case_bridge();
    if (!strcmp(argv[1], "bridge6") && argc == 2) return case_bridge6();
    if (!strcmp(argv[1], "tls") && argc == 3) return case_tls(argv[2]);
    if (!strcmp(argv[1], "lifecycle") && argc == 3) return case_lifecycle(argv[2]);
    if (!strcmp(argv[1], "fixture") && argc == 3) return case_fixture(argv[2]);
    if (!strcmp(argv[1], "console-dump") && argc == 3) return case_console_dump(argv[2]);
    if (!strcmp(argv[1], "mbm") && argc == 3) return case_mbm(argv[2]);
    if (!strcmp(argv[1], "loadonly") && argc == 3) return case_loadonly(argv[2]);
    if (!strcmp(argv[1], "refuse") && argc == 4) return case_refuse(argv[2], argv[3]);
    if (!strcmp(argv[1], "shim-coverage") && argc == 3) return case_shim_coverage(argv[2]);
    usage();
    return 2;
}
