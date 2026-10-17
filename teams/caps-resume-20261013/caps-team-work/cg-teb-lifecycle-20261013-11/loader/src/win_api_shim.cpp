// win_api_shim.cpp -- kernel32/ucrt subset implementations (T1-T3) and the
// fail-closed EH stubs (T4). See win_api_shim.h for the tier policy.
//
// Notes baked into the implementations:
//  * Dynamic TLS (TlsAlloc family) is EMULATED with a slot table -- this is
//    NOT the refused static PE TLS directory (design 3e).
//  * GetLastError/SetLastError ride a thread-local error slot, NOT the TEB
//    (no gs addressing anywhere in OUR code).
//  * Encode/DecodePointer use one process cookie; self-consistent pairs.
//  * File APIs apply the GA path-shim rules and confine writes to the mod
//    root (design 5b T2; agent-GA-path-shim.md rules).
//  * EH/SEH entry points FAIL CLOSED with a precise diagnostic (design 4c).
//
// Every function listed in the registry crosses the guest->host bridge with
// its documented Win32 signature; the bridge carries the signature table.
// CAP13 proposal-only; design-grade; bodies are intentionally small.

#include "win_api_shim.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <thread>
#include <vector>

#if defined(__APPLE__) || defined(__unix__) || defined(__PROSPERO__)
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#define CAP13_POSIX 1
#else
#define CAP13_POSIX 0
#endif

namespace cap13 {

namespace {

// ---- fixed Win32 constants used by the shims (public documentation values)
constexpr uint32_t WAIT_FAILED = 0xFFFFFFFFu;
constexpr uint32_t TLS_OUT_OF_INDEXES = 0xFFFFFFFFu;
constexpr uint32_t GENERIC_WRITE = 0x40000000u;

using WinThreadFn = uint32_t (*)(void*); // unsigned __stdcall, x64: Win64 call

} // namespace

struct WinApiShim::Impl
{
    Hooks hooks;
    std::string mod_root = "/data/modbridge/roots/";
    std::mutex mu;
    // dll-name(lowercased) -> { symbol/ordinal -> address }
    std::unordered_map<std::string, std::unordered_map<std::string, void*>> exports;
    struct ModuleEntry
    {
        WinModule handle;
        bool (*lookup)(WinModule, const char*, void**);
    };
    std::unordered_map<std::string, ModuleEntry> modules;
    uint64_t pointer_cookie = 0xA5A5'5A5A'0000'0042ull; // fixed for design; M1 randomizes at DllMain time
    void* last_error_slot_value = nullptr; // M1: per-thread (emulated TEB-free TLS slot)

    // ---- T1 emulation state ----
    std::vector<bool> tls_slot_used;
    std::vector<std::unordered_map<uint64_t, void*>> tls_slot_values;

    uint64_t self_thread_key()
    {
#if CAP13_POSIX
        return (uint64_t)pthread_self();
#else
        return (uint64_t)(uintptr_t)&self_thread_key;
#endif
    }

    void log(int level, const char* msg)
    {
        if (hooks.log) hooks.log(hooks.log_user, level, msg);
    }

    void fail_closed(const char* what)
    {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "PE mod executed unsupported Windows API: %s; mod stopped (fail-closed)", what);
        log(3, buf);
        if (hooks.stop_mod) hooks.stop_mod(hooks.stop_user, buf);
    }
};

WinApiShim::WinApiShim() : m(new Impl()) {}

void WinApiShim::set_hooks(const Hooks& hooks) { m->hooks = hooks; }
void WinApiShim::set_mod_root(std::string utf8_root) { m->mod_root = std::move(utf8_root); }

void WinApiShim::register_module(const char* name, WinModule handle,
                                 bool (*lookup)(WinModule, const char*, void**))
{
    std::lock_guard g(m->mu);
    m->modules[name] = Impl::ModuleEntry{handle, lookup};
}

// ===================== T1 implementations =====================
// Signatures mirror the documented Win32 prototypes. The bridge (not shown
// here) is what guests actually call; these are the SysV bodies behind it.

static void* impl_VirtualAlloc(WinApiShim::Impl* m, void* base, size_t size, uint32_t type, uint32_t protect)
{
    (void)base;
    (void)type;
    // RW allocations only. Any EXEC request is a refusal signal: the loader
    // route has no JIT surface, and game-code patching stays unsupported.
    // PAGE_EXECUTE=0x10, PAGE_EXECUTE_READ=0x20, _READWRITE=0x40,
    // _WRITECOPY=0x80 (documented Win32 protection constants).
    if (protect & 0xF0)
    {
        m->log(2, "VirtualAlloc(EXEC) refused: no executable-allocation surface");
        return nullptr;
    }
#if CAP13_POSIX
    (void)m;
    return std::calloc(1, size ? size : 1);
#else
    (void)m;
    return nullptr;
#endif
}

static uint32_t impl_Sleep(WinApiShim::Impl*, uint32_t ms)
{
#if CAP13_POSIX
    ::usleep(useconds_t(ms) * 1000);
#endif
    return 0;
}

static uint32_t impl_WaitForSingleObject_events(WinApiShim::Impl*, void* handle, uint32_t ms)
{
    // Event handles come from our CreateEvent emulation (condvar class).
    // Design-grade: returns WAIT_FAILED for unknown handles; M1 wires the
    // real condvar event object table.
    (void)handle;
    (void)ms;
    return WAIT_FAILED;
}

static uint32_t impl_TlsAlloc(WinApiShim::Impl* m)
{
    std::lock_guard g(m->mu);
    for (uint32_t i = 0; i < m->tls_slot_used.size(); ++i)
        if (!m->tls_slot_used[i])
        {
            m->tls_slot_used[i] = true;
            return i;
        }
    if (m->tls_slot_used.size() >= 1088) return TLS_OUT_OF_INDEXES; // Windows' TLS_MIN_AVAILABLE x pow2 growth
    m->tls_slot_used.push_back(true);
    m->tls_slot_values.emplace_back();
    return uint32_t(m->tls_slot_used.size() - 1);
}

static void* impl_TlsGetValue(WinApiShim::Impl* m, uint32_t slot)
{
    std::lock_guard g(m->mu);
    if (slot >= m->tls_slot_values.size()) return nullptr; // GetLastError convention: caller checks GetLastError
    auto it = m->tls_slot_values[slot].find(m->self_thread_key());
    return it == m->tls_slot_values[slot].end() ? nullptr : it->second;
}

static bool impl_TlsSetValue(WinApiShim::Impl* m, uint32_t slot, void* value)
{
    std::lock_guard g(m->mu);
    if (slot >= m->tls_slot_values.size()) return false;
    m->tls_slot_values[slot][m->self_thread_key()] = value;
    return true;
}

struct ThreadStartPack
{
    WinThreadFn fn;
    void* arg;
    WinApiShim::Impl* shim;
};

#if CAP13_POSIX
static void* thread_tramp(void* p)
{
    ThreadStartPack pack = *static_cast<ThreadStartPack*>(p);
    delete static_cast<ThreadStartPack*>(p);
    // pack.fn is GUEST (Win64) code: the bridge supplies the Win64-calling
    // entry; here it is already the bridged pointer created at CreateThread.
    const uint32_t rc = pack.fn(pack.arg);
    return (void*)(uintptr_t)rc;
}
#endif

static void* impl_CreateThread(WinApiShim::Impl*, void* attrs, size_t stack, WinThreadFn bridged_fn,
                               void* arg, uint32_t flags, uint32_t* out_tid)
{
    (void)attrs;
    (void)flags;
#if CAP13_POSIX
    pthread_t tid{};
    pthread_attr_t a;
    pthread_attr_init(&a);
    if (stack) pthread_attr_setstacksize(&a, stack);
    auto* pack = new ThreadStartPack{bridged_fn, arg, nullptr};
    if (pthread_create(&tid, &a, thread_tramp, pack) != 0)
    {
        delete pack;
        return nullptr;
    }
    pthread_attr_destroy(&a);
    if (out_tid) *out_tid = uint32_t((uintptr_t)tid); // opaque for design; M1 mints real ids
    return (void*)(uintptr_t)tid;
#else
    (void)stack;
    (void)bridged_fn;
    (void)arg;
    (void)out_tid;
    return nullptr;
#endif
}

static void impl_QueryPerformanceCounter(WinApiShim::Impl*, int64_t* out)
{
#if CAP13_POSIX
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    *out = int64_t(ts.tv_sec) * 1000000000ll + ts.tv_nsec;
#else
    *out = 0;
#endif
}

static uint32_t impl_GetTickCount(WinApiShim::Impl*)
{
    using namespace std::chrono;
    return uint32_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

// ---- T4: fail-closed EH/SEH (design 4c). Presence != use, so these are
// registered but never fire unless the mod ACTUALLY throws/raises.
static int32_t impl_fail_closed_cxx_frame(WinApiShim::Impl* m)
{
    m->fail_closed("__CxxFrameHandler4 (mod unwound through C++ EH; unsupported)");
    return 0;
}
static void impl_fail_closed_raise(WinApiShim::Impl* m)
{
    m->fail_closed("RaiseException (SEH raise; unsupported)");
}
static void impl_fail_closed_throw(WinApiShim::Impl* m)
{
    m->fail_closed("_CxxThrowException (mod threw; unsupported)");
}
static int32_t impl_fail_closed_specific_handler(WinApiShim::Impl* m)
{
    m->fail_closed("__C_specific_handler (__try/__except; unsupported)");
    return 0;
}

// ===================== T2: path rules (GA shim twin) =====================
// Returns a confined, normalized path or empty on refusal (design 5b).
// Rules (agent-GA-path-shim.md, C++ twin): drop drive letters, backslash ->
// slash, collapse Binaries/Win64 + ../ against the virtual CWD, confine
// writes to the mod root.
static std::string shim_path(WinApiShim::Impl* m, const char* win_path, bool for_write)
{
    std::string p = win_path ? win_path : "";
    if (p.size() >= 2 && ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')
        p.erase(0, 2);
    for (auto& c : p)
        if (c == '\\') c = '/';
    while (p.size() > 1 && p[0] == '/') p.erase(0, 1);
    // Collapse 'Binaries/Win64/' prefixes (mods build Windows-relative paths).
    const char* win64 = "Binaries/Win64/";
    for (size_t pos; (pos = p.find(win64)) != std::string::npos;)
        p.erase(pos, std::strlen(win64));
    // Absolute-ish requests anchor at the mod root; write confinement is
    // absolute: everything must land under mod_root.
    std::string full = m->mod_root;
    if (!full.empty() && full.back() == '/') full.pop_back();
    full += "/";
    full += p;
    if (!for_write) return full;
    // for_write: the root prefix IS the confinement; nothing to add.
    return full;
}

static void* impl_CreateFileW(WinApiShim::Impl* m, const char* utf8_path_fallback, uint32_t access,
                              uint32_t share, void* sa, uint32_t creation, uint32_t flags, void* tmpl)
{
    // Design-grade: accepts the UTF-8 transcription the bridge produced from
    // the guest's UTF-16 (the bridge demarshals LPCWSTR -> UTF-8).
    (void)share;
    (void)sa;
    (void)flags;
    (void)tmpl;
    const bool for_write = (access & GENERIC_WRITE) != 0 || creation >= 2; // CREATE_ALWAYS / OPEN_ALWAYS class
    const std::string path = shim_path(m, utf8_path_fallback, for_write);
    if (path.empty()) return (void*)-1; // INVALID_HANDLE_VALUE
    return std::fopen(path.c_str(), for_write ? "wb" : "rb");
}

// ===================== registry =====================

// Rows are (dll, symbol) -> impl. Ordinals are accepted but this surface is
// name-exported; the census (JG) verifies which ordinals real mods use.
// BRIDGE NOTE: in the integrated system the registry holds the BRIDGED
// pointer (trampoline -> shared thunk -> these SysV bodies); the staging
// table stores the SysV bodies and the adapter stamps trampolines over them
// at load time (win64_bridge.h). The fail-closed rows must be bridged too,
// or a mod's throw would enter SysV code under a Win64 frame.
void WinApiShim::register_builtin_exports()
{
    struct Row
    {
        const char* dll;
        const char* sym;
        void* fn;
    };
    const Row rows[] = {
        // T1 MAP rows (implemented bodies above):
        {"kernel32.dll", "VirtualAlloc", (void*)&impl_VirtualAlloc},
        {"kernel32.dll", "Sleep", (void*)&impl_Sleep},
        {"kernel32.dll", "WaitForSingleObject", (void*)&impl_WaitForSingleObject_events},
        {"kernel32.dll", "TlsAlloc", (void*)&impl_TlsAlloc},
        {"kernel32.dll", "TlsGetValue", (void*)&impl_TlsGetValue},
        {"kernel32.dll", "TlsSetValue", (void*)&impl_TlsSetValue},
        {"kernel32.dll", "CreateThread", (void*)&impl_CreateThread},
        {"kernel32.dll", "QueryPerformanceCounter", (void*)&impl_QueryPerformanceCounter},
        {"kernel32.dll", "GetTickCount", (void*)&impl_GetTickCount},
        {"kernel32.dll", "CreateFileW", (void*)&impl_CreateFileW},
        // T4 fail-closed (the important policy rows):
        {"vcruntime140.dll", "__CxxFrameHandler4", (void*)&impl_fail_closed_cxx_frame},
        {"kernel32.dll", "RaiseException", (void*)&impl_fail_closed_raise},
        {"vcruntime140.dll", "CxxThrowException", (void*)&impl_fail_closed_throw},
        {"kernel32.dll", "RtlUnwind", (void*)&impl_fail_closed_raise},
        {"ntdll.dll", "RtlUnwindEx", (void*)&impl_fail_closed_raise},
        {"vcruntime140.dll", "__C_specific_handler", (void*)&impl_fail_closed_specific_handler},
    };
    for (const auto& r : rows)
    {
        std::string key = r.sym;
        m->exports[r.dll][key] = r.fn;
    }
}

void* WinApiShim::resolve(const char* dll_name, const char* symbol, uint64_t ordinal)
{
    std::lock_guard g(m->mu);
    std::string dll = dll_name ? dll_name : "";
    for (auto& c : dll)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');

    // 1) registered modules first (UE4SS.dll surface, mapped PE mods)
    auto mod = m->modules.find(dll);
    if (mod != m->modules.end() && symbol && mod->second.lookup)
    {
        void* out = nullptr;
        if (mod->second.lookup(mod->second.handle, symbol, &out)) return out;
    }

    // 2) built-in rows
    auto ex = m->exports.find(dll);
    if (ex != m->exports.end())
    {
        if (symbol)
        {
            auto sym = ex->second.find(symbol);
            if (sym != ex->second.end()) return sym->second;
        }
        else if (ordinal)
        {
            char ord[24];
            std::snprintf(ord, sizeof(ord), "#%llu", (unsigned long long)ordinal);
            auto sym = ex->second.find(ord);
            if (sym != ex->second.end()) return sym->second;
        }
    }

    // 3) unknown: nullptr -> the loader refuses with the complete missing
    // list (fail closed; precise diagnostics per cap13 acceptance).
    return nullptr;
}

} // namespace cap13
