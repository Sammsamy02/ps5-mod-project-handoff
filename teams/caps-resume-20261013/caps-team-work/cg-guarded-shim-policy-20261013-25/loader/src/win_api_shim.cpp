// win_api_shim.cpp -- kernel32/ucrt subset implementations (T1-T3) and the
// fail-closed EH stubs (T4). See win_api_shim.h for the tier policy.
//
// Candidate bodies exist for dynamic TLS, files, threads, and EH/SEH, but
// their full Win32 behavior or guest-callable ABI is not established. They
// remain unresolved imports. Static PE TLS is a separate loader refusal.
// Imported GetLastError/SetLastError share a per-thread slot with the guest
// TEB lifecycle; pointer encoding is not implemented here.
//
// Raw built-in handlers are private metadata. No PE import receives one
// until a per-signature guest-callable wrapper is explicitly published.
// CAP13 proposal-only; design-grade; bodies are intentionally small.

#include "win_api_shim.h"
#include "win32_last_error_state.h"
#include "win64_bridge.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <new>
#include <shared_mutex>
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
constexpr uint32_t ERROR_TIMEOUT = 1460;
constexpr uint32_t ERROR_INVALID_PARAMETER = 87;
constexpr uint32_t TLS_OUT_OF_INDEXES = 0xFFFFFFFFu;
constexpr uint32_t GENERIC_WRITE = 0x40000000u;

using WinThreadFn = uint32_t (*)(void*); // unsigned __stdcall, x64: Win64 call

std::string ascii_module_key(const char* name)
{
    std::string key = name ? name : "";
    for (char& c : key)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return key;
}

} // namespace

struct WinApiShim::Impl
{
    struct RawExport
    {
        void* body = nullptr;
        uint8_t integer_arity = UINT8_MAX;
        bool reviewed_for_publication = false;
    };
    Hooks hooks;
    std::string mod_root = "/data/modbridge/roots/";
    std::mutex mu;
    // dll-name(lowercased) -> { symbol/ordinal -> address }
    std::unordered_map<std::string, std::unordered_map<std::string, RawExport>> raw_exports;
    std::unordered_map<std::string, std::unordered_map<std::string, void*>> exports;
    ExecutableMemoryBackend executable_backend{};
    struct ModuleEntry
    {
        WinModule handle;
        bool (*lookup)(WinModule, const char*, void**);
    };
    std::unordered_map<std::string, ModuleEntry> modules;
    struct ThreadNotificationPolicy
    {
        bool has_static_tls = false;
        bool (*disable)(void* user) = nullptr;
        void* user = nullptr;
    };
    std::unordered_map<WinModule, ThreadNotificationPolicy> notification_policies;
    uint64_t pointer_cookie = 0xA5A5'5A5A'0000'0042ull; // fixed for design; M1 randomizes at DllMain time

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

namespace {

struct HostSrwLock
{
    std::shared_mutex rw;
    std::mutex owners_mu;
    std::thread::id exclusive_owner{};
    std::unordered_map<std::thread::id, uint32_t> shared_owners;
};

struct HostCondition
{
    std::mutex mu;
    std::condition_variable cv;
    uint64_t generation = 0;
};

struct HostSyncRegistry
{
    std::mutex mu;
    std::unordered_map<void*, std::shared_ptr<HostSrwLock>> locks;
    std::unordered_map<void*, std::shared_ptr<HostCondition>> conditions;
};

HostSyncRegistry& host_sync_registry()
{
    static HostSyncRegistry registry;
    return registry;
}

std::shared_ptr<HostSrwLock> host_srw_lock(void* address, bool create)
{
    auto& registry = host_sync_registry();
    std::lock_guard guard(registry.mu);
    auto found = registry.locks.find(address);
    if (found != registry.locks.end()) return found->second;
    if (!create) return {};
    auto state = std::make_shared<HostSrwLock>();
    registry.locks.emplace(address, state);
    return state;
}

std::shared_ptr<HostCondition> host_condition(void* address)
{
    auto& registry = host_sync_registry();
    std::lock_guard guard(registry.mu);
    auto found = registry.conditions.find(address);
    if (found != registry.conditions.end()) return found->second;
    auto state = std::make_shared<HostCondition>();
    registry.conditions.emplace(address, state);
    return state;
}

} // namespace

WinApiShim::WinApiShim() : m(new Impl()) {}

void WinApiShim::set_hooks(const Hooks& hooks) { m->hooks = hooks; }
void WinApiShim::set_mod_root(std::string utf8_root) { m->mod_root = std::move(utf8_root); }

void WinApiShim::register_module(const char* name, WinModule handle,
                                 bool (*lookup)(WinModule, const char*, void**))
{
    if (!name || !*name || !lookup) return;
    const std::string key = ascii_module_key(name);
    std::lock_guard g(m->mu);
    m->modules[key] = Impl::ModuleEntry{handle, lookup};
}

bool WinApiShim::register_thread_notification_policy(
    WinModule module_handle, bool has_static_tls,
    bool (*disable)(void* user), void* user)
{
    if (!module_handle || (!has_static_tls && !disable)) return false;
    std::lock_guard guard(m->mu);
    if (m->notification_policies.count(module_handle)) return false;
    try
    {
        m->notification_policies.emplace(module_handle,
            Impl::ThreadNotificationPolicy{has_static_tls, disable, user});
    }
    catch (const std::bad_alloc&) { return false; }
    return true;
}

bool WinApiShim::unregister_thread_notification_policy(WinModule module_handle)
{
    if (!module_handle) return false;
    std::lock_guard guard(m->mu);
    return m->notification_policies.erase(module_handle) == 1;
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

static uint32_t impl_Sleep(WinApiShim::Impl* m, uint32_t ms)
{
#if CAP13_POSIX
    if (ms == 0)
    {
        std::this_thread::yield();
        return 0;
    }
    struct timespec remaining{};
    remaining.tv_sec = static_cast<time_t>(ms / 1000);
    remaining.tv_nsec = static_cast<long>(ms % 1000) * 1000000L;
    while (::nanosleep(&remaining, &remaining) != 0)
    {
        if (errno == EINTR) continue;
        m->fail_closed("Sleep host clock failed");
        break;
    }
#else
    (void)m;
    (void)ms;
#endif
    return 0;
}

static uint32_t impl_GetCurrentProcessId(WinApiShim::Impl* m)
{
#if CAP13_POSIX
    (void)m;
    return static_cast<uint32_t>(::getpid());
#else
    m->fail_closed("GetCurrentProcessId host identity unavailable");
    return 0;
#endif
}

static uint32_t impl_GetCurrentThreadId(WinApiShim::Impl* m)
{
    // A host thread keeps one Windows-visible ID across all shim instances.
    // Assignment is process-wide and never wraps a DWORD ID into reuse.
    static std::atomic<uint64_t> next_id{1};
    static thread_local uint64_t id = next_id.fetch_add(1, std::memory_order_relaxed);
    if (id > UINT32_MAX)
    {
        m->fail_closed("GetCurrentThreadId ID space exhausted");
        return 0;
    }
    return static_cast<uint32_t>(id);
}

static uint32_t impl_GetLastError(WinApiShim::Impl*)
{
    return win32_get_last_error();
}

static void impl_SetLastError(WinApiShim::Impl*, uint32_t error)
{
    win32_set_last_error(error);
}

static void impl_GetSystemTimeAsFileTime(WinApiShim::Impl* m, void* output)
{
    if (!output)
    {
        m->fail_closed("GetSystemTimeAsFileTime null output");
        return;
    }
#if CAP13_POSIX
    struct timespec utc{};
    if (::clock_gettime(CLOCK_REALTIME, &utc) != 0)
    {
        m->fail_closed("GetSystemTimeAsFileTime host clock failed");
        return;
    }
    // FILETIME is two little-endian DWORDs, counting 100 ns ticks since
    // 1601-01-01 UTC. The POSIX clock counts from 1970-01-01 UTC.
    constexpr uint64_t epoch_delta = 116444736000000000ull;
    const uint64_t ticks = epoch_delta +
        static_cast<uint64_t>(utc.tv_sec) * 10000000ull +
        static_cast<uint64_t>(utc.tv_nsec) / 100ull;
    const uint32_t words[2] = {static_cast<uint32_t>(ticks),
                               static_cast<uint32_t>(ticks >> 32)};
    std::memcpy(output, words, sizeof(words));
#else
    m->fail_closed("GetSystemTimeAsFileTime host clock unavailable");
#endif
}

static void impl_AcquireSRWLockExclusive(WinApiShim::Impl* m, void* address)
{
    if (!address) { m->fail_closed("AcquireSRWLockExclusive null lock"); return; }
    try
    {
        auto state = host_srw_lock(address, true);
        state->rw.lock();
        std::lock_guard guard(state->owners_mu);
        state->exclusive_owner = std::this_thread::get_id();
    }
    catch (...) { m->fail_closed("AcquireSRWLockExclusive host failure"); }
}

static void impl_ReleaseSRWLockExclusive(WinApiShim::Impl* m, void* address)
{
    if (!address) { m->fail_closed("ReleaseSRWLockExclusive null lock"); return; }
    try
    {
        auto state = host_srw_lock(address, false);
        if (!state) { m->fail_closed("ReleaseSRWLockExclusive unknown lock"); return; }
        bool owned = false;
        {
            std::lock_guard guard(state->owners_mu);
            owned = state->exclusive_owner == std::this_thread::get_id();
            if (owned) state->exclusive_owner = {};
        }
        if (!owned) { m->fail_closed("ReleaseSRWLockExclusive wrong owner"); return; }
        state->rw.unlock();
    }
    catch (...) { m->fail_closed("ReleaseSRWLockExclusive host failure"); }
}

static void impl_AcquireSRWLockShared(WinApiShim::Impl* m, void* address)
{
    if (!address) { m->fail_closed("AcquireSRWLockShared null lock"); return; }
    try
    {
        auto state = host_srw_lock(address, true);
        state->rw.lock_shared();
        try
        {
            std::lock_guard guard(state->owners_mu);
            ++state->shared_owners[std::this_thread::get_id()];
        }
        catch (...) { state->rw.unlock_shared(); throw; }
    }
    catch (...) { m->fail_closed("AcquireSRWLockShared host failure"); }
}

static void impl_ReleaseSRWLockShared(WinApiShim::Impl* m, void* address)
{
    if (!address) { m->fail_closed("ReleaseSRWLockShared null lock"); return; }
    try
    {
        auto state = host_srw_lock(address, false);
        if (!state) { m->fail_closed("ReleaseSRWLockShared unknown lock"); return; }
        bool owned = false;
        {
            std::lock_guard guard(state->owners_mu);
            auto found = state->shared_owners.find(std::this_thread::get_id());
            owned = found != state->shared_owners.end() && found->second != 0;
            if (owned) --found->second;
        }
        if (!owned) { m->fail_closed("ReleaseSRWLockShared wrong owner"); return; }
        state->rw.unlock_shared();
    }
    catch (...) { m->fail_closed("ReleaseSRWLockShared host failure"); }
}

static uint32_t impl_SleepConditionVariableSRW(
    WinApiShim::Impl* m, void* condition, void* lock, uint32_t milliseconds,
    uint32_t flags)
{
    if (!condition || !lock || (flags & ~1u))
    {
        m->fail_closed("SleepConditionVariableSRW invalid arguments");
        return 0;
    }
    try
    {
        auto srw = host_srw_lock(lock, false);
        if (!srw) { m->fail_closed("SleepConditionVariableSRW unknown lock"); return 0; }
        auto cv = host_condition(condition);
        const bool shared = (flags & 1u) != 0;
        const auto thread = std::this_thread::get_id();
        std::unique_lock cv_guard(cv->mu);
        bool owned = false;
        {
            std::lock_guard owner_guard(srw->owners_mu);
            if (shared)
            {
                auto found = srw->shared_owners.find(thread);
                owned = found != srw->shared_owners.end() && found->second != 0;
                if (owned) --found->second;
            }
            else
            {
                owned = srw->exclusive_owner == thread;
                if (owned) srw->exclusive_owner = {};
            }
        }
        if (!owned)
        {
            cv_guard.unlock();
            m->fail_closed(shared ? "SleepConditionVariableSRW shared lock not held"
                                  : "SleepConditionVariableSRW exclusive lock not held");
            return 0;
        }
        const uint64_t observed = cv->generation;
        if (shared) srw->rw.unlock_shared();
        else srw->rw.unlock();
        const auto signaled = [&] { return cv->generation != observed; };
        bool woken = true;
        if (milliseconds == 0xffffffffu)
            cv->cv.wait(cv_guard, signaled);
        else
            woken = cv->cv.wait_for(cv_guard,
                std::chrono::milliseconds(milliseconds), signaled);
        cv_guard.unlock();
        if (shared) srw->rw.lock_shared();
        else srw->rw.lock();
        {
            std::lock_guard owner_guard(srw->owners_mu);
            if (shared) ++srw->shared_owners[thread];
            else srw->exclusive_owner = thread;
        }
        if (!woken) { win32_set_last_error(ERROR_TIMEOUT); return 0; }
        return 1;
    }
    catch (...) { m->fail_closed("SleepConditionVariableSRW host failure"); return 0; }
}

static void impl_WakeAllConditionVariable(WinApiShim::Impl* m, void* condition)
{
    if (!condition) { m->fail_closed("WakeAllConditionVariable null condition"); return; }
    try
    {
        auto cv = host_condition(condition);
        {
            std::lock_guard guard(cv->mu);
            ++cv->generation;
        }
        cv->cv.notify_all();
    }
    catch (...) { m->fail_closed("WakeAllConditionVariable host failure"); }
}

static void impl_InitializeSListHead(WinApiShim::Impl* m, void* header)
{
    // The x64 Windows SDK declares SLIST_HEADER as 16 bytes, aligned to 16.
    if (!header || (reinterpret_cast<uintptr_t>(header) & 15u))
    {
        m->fail_closed("InitializeSListHead null or misaligned header");
        return;
    }
    std::memset(header, 0, 16);
}

static uint32_t impl_DisableThreadLibraryCalls(WinApiShim::Impl* m,
                                                WinModule module_handle)
{
    WinApiShim::Impl::ThreadNotificationPolicy policy{};
    bool found = false;
    {
        std::lock_guard guard(m->mu);
        auto row = m->notification_policies.find(module_handle);
        if (row != m->notification_policies.end())
        {
            policy = row->second;
            found = true;
        }
    }
    if (!found || policy.has_static_tls || !policy.disable)
    {
        win32_set_last_error(ERROR_INVALID_PARAMETER);
        return 0;
    }
    bool disabled = false;
    try { disabled = policy.disable(policy.user); }
    catch (...) { m->fail_closed("DisableThreadLibraryCalls adapter failure"); }
    if (!disabled) { win32_set_last_error(ERROR_INVALID_PARAMETER); return 0; }
    return 1;
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

static uint32_t impl_GetTickCount(WinApiShim::Impl* m)
{
#if CAP13_POSIX
    struct timespec now{};
    if (::clock_gettime(CLOCK_MONOTONIC, &now) != 0)
    {
        m->fail_closed("GetTickCount host clock failed");
        return 0;
    }
    return uint32_t(uint64_t(now.tv_sec) * 1000 + uint64_t(now.tv_nsec) / 1000000);
#else
    (void)m;
    return 0;
#endif
}

// ---- T4: fail-closed EH/SEH (design 4c). These raw bodies are cataloged,
// but do not enter a PE IAT until a valid Win64 wrapper is published.
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
// These SysV bodies require a hidden Impl* argument. Name presence alone
// cannot put one in a PE IAT; a guest-callable wrapper must inject context
// and honor the exact Win64 signature before publication.
void WinApiShim::register_builtin_exports()
{
    struct Row
    {
        const char* dll;
        const char* sym;
        void* fn;
        uint8_t integer_arity;
        bool reviewed_for_publication;
    };
    const Row rows[] = {
        // These identity/time rows have host-checked basic behavior.
        // The other raw bodies are catalogued but remain unresolved imports.
        {"kernel32.dll", "VirtualAlloc", (void*)&impl_VirtualAlloc, 4, false},
        {"kernel32.dll", "Sleep", (void*)&impl_Sleep, 1, true},
        {"kernel32.dll", "GetCurrentProcessId", (void*)&impl_GetCurrentProcessId, 0, true},
        {"kernel32.dll", "GetCurrentThreadId", (void*)&impl_GetCurrentThreadId, 0, true},
        {"kernel32.dll", "GetLastError", (void*)&impl_GetLastError, 0, true},
        {"kernel32.dll", "SetLastError", (void*)&impl_SetLastError, 1, true},
        {"kernel32.dll", "AcquireSRWLockExclusive", (void*)&impl_AcquireSRWLockExclusive, 1, true},
        {"kernel32.dll", "ReleaseSRWLockExclusive", (void*)&impl_ReleaseSRWLockExclusive, 1, true},
        {"kernel32.dll", "AcquireSRWLockShared", (void*)&impl_AcquireSRWLockShared, 1, true},
        {"kernel32.dll", "ReleaseSRWLockShared", (void*)&impl_ReleaseSRWLockShared, 1, true},
        {"kernel32.dll", "SleepConditionVariableSRW", (void*)&impl_SleepConditionVariableSRW, 4, true},
        {"kernel32.dll", "WakeAllConditionVariable", (void*)&impl_WakeAllConditionVariable, 1, true},
        {"kernel32.dll", "InitializeSListHead", (void*)&impl_InitializeSListHead, 1, true},
        {"kernel32.dll", "DisableThreadLibraryCalls", (void*)&impl_DisableThreadLibraryCalls, 1, true},
        {"kernel32.dll", "GetSystemTimeAsFileTime", (void*)&impl_GetSystemTimeAsFileTime, 1, true},
        {"kernel32.dll", "WaitForSingleObject", (void*)&impl_WaitForSingleObject_events, 2, false},
        {"kernel32.dll", "TlsAlloc", (void*)&impl_TlsAlloc, 0, false},
        {"kernel32.dll", "TlsGetValue", (void*)&impl_TlsGetValue, 1, false},
        {"kernel32.dll", "TlsSetValue", (void*)&impl_TlsSetValue, 2, false},
        {"kernel32.dll", "CreateThread", (void*)&impl_CreateThread, 6, false},
        {"kernel32.dll", "QueryPerformanceCounter", (void*)&impl_QueryPerformanceCounter, 1, false},
        {"kernel32.dll", "GetTickCount", (void*)&impl_GetTickCount, 0, true},
        {"kernel32.dll", "CreateFileW", (void*)&impl_CreateFileW, 7, false},
        // T4 fail-closed (the important policy rows):
        {"vcruntime140.dll", "__CxxFrameHandler4", (void*)&impl_fail_closed_cxx_frame, UINT8_MAX, false},
        {"kernel32.dll", "RaiseException", (void*)&impl_fail_closed_raise, UINT8_MAX, false},
        {"vcruntime140.dll", "CxxThrowException", (void*)&impl_fail_closed_throw, UINT8_MAX, false},
        {"kernel32.dll", "RtlUnwind", (void*)&impl_fail_closed_raise, UINT8_MAX, false},
        {"ntdll.dll", "RtlUnwindEx", (void*)&impl_fail_closed_raise, UINT8_MAX, false},
        {"vcruntime140.dll", "__C_specific_handler", (void*)&impl_fail_closed_specific_handler, UINT8_MAX, false},
    };
    std::lock_guard g(m->mu);
    for (const auto& r : rows)
    {
        std::string key = r.sym;
        m->raw_exports[r.dll][key] = Impl::RawExport{
            r.fn, r.integer_arity, r.reviewed_for_publication};
    }
}

BuiltinInstallStatus WinApiShim::install_builtin_integer_wrapper(
    const char* dll_name, const char* symbol, void* writable_slot,
    size_t slot_bytes, SealExecutable seal, void* seal_user)
{
    if (!dll_name || !*dll_name || !symbol || !*symbol || !writable_slot ||
        slot_bytes < sizeof(ContextBridgeTramp) || !seal)
        return BuiltinInstallStatus::InvalidInput;
    const std::string dll = ascii_module_key(dll_name);
    Impl::RawExport raw;
    {
        std::lock_guard g(m->mu);
        auto raw_dll = m->raw_exports.find(dll);
        if (raw_dll == m->raw_exports.end()) return BuiltinInstallStatus::Unsupported;
        auto row = raw_dll->second.find(symbol);
        if (row == raw_dll->second.end() || !row->second.reviewed_for_publication)
            return BuiltinInstallStatus::Unsupported;
        auto existing = m->exports.find(dll);
        if (existing != m->exports.end() && existing->second.count(symbol))
            return BuiltinInstallStatus::AlreadyPublished;
        raw = row->second;
    }
    const void* thunk = bridge_select_context_thunk(raw.integer_arity);
    if (!thunk) return BuiltinInstallStatus::Unsupported;
    bridge_stamp_context_tramp(writable_slot, m, raw.body, thunk);
    if (!seal(writable_slot, slot_bytes, seal_user))
        return BuiltinInstallStatus::SealFailed;
    {
        std::lock_guard g(m->mu);
        auto existing = m->exports.find(dll);
        if (existing != m->exports.end() && existing->second.count(symbol))
            return BuiltinInstallStatus::AlreadyPublished;
        try { m->exports[dll][symbol] = writable_slot; }
        catch (const std::bad_alloc&) { return BuiltinInstallStatus::OutOfMemory; }
    }
    return BuiltinInstallStatus::Ok;
}

bool WinApiShim::set_executable_backend(const ExecutableMemoryBackend& backend)
{
    if (!backend.allocate_writable || !backend.seal_executable || !backend.release)
        return false;
    std::lock_guard g(m->mu);
    m->executable_backend = backend;
    return true;
}

void* WinApiShim::resolve(const char* dll_name, const char* symbol, uint64_t ordinal)
{
    const std::string dll = ascii_module_key(dll_name);
    if (dll.empty()) return nullptr;

    // 1) registered modules first (UE4SS.dll surface, mapped PE mods)
    Impl::ModuleEntry selected{};
    bool registered = false;
    {
        std::lock_guard g(m->mu);
        auto mod = m->modules.find(dll);
        if (mod != m->modules.end())
        {
            selected = mod->second;
            registered = true;
        }
    }
    // A module's lookup may resolve forwarded imports itself. Never invoke
    // external module code while holding the registry mutex.
    if (registered && symbol && selected.lookup)
    {
        void* out = nullptr;
        if (selected.lookup(selected.handle, symbol, &out) && out) return out;
    }

    // 2) already-published built-ins, or a reviewed row eligible for lazy
    // installation. Never call platform callbacks while holding the mutex.
    ExecutableMemoryBackend backend{};
    bool eligible = false;
    {
        std::lock_guard g(m->mu);
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
        if (symbol)
        {
            auto catalog = m->raw_exports.find(dll);
            if (catalog != m->raw_exports.end())
            {
                auto row = catalog->second.find(symbol);
                eligible = row != catalog->second.end() &&
                           row->second.reviewed_for_publication;
            }
            backend = m->executable_backend;
        }
    }

    if (!eligible || !backend.allocate_writable || !backend.seal_executable ||
        !backend.release) return nullptr;

    // A sealing callback may inspect this same import. It must observe a
    // missing import rather than recursively allocate another trampoline.
    static thread_local bool in_lazy_install = false;
    if (in_lazy_install) return nullptr;
    in_lazy_install = true;
    struct ResetLazyFlag
    {
        bool& flag;
        ~ResetLazyFlag() { flag = false; }
    } reset{in_lazy_install};

    void* memory = nullptr;
    size_t bytes = 0;
    if (!backend.allocate_writable(sizeof(ContextBridgeTramp), &memory,
                                   &bytes, backend.user) ||
        !memory || bytes < sizeof(ContextBridgeTramp))
    {
        if (memory && bytes) backend.release(memory, bytes, backend.user);
        return nullptr;
    }
    const BuiltinInstallStatus status = install_builtin_integer_wrapper(
        dll_name, symbol, memory, bytes, backend.seal_executable, backend.user);
    if (status == BuiltinInstallStatus::Ok) return memory;
    backend.release(memory, bytes, backend.user);
    if (status == BuiltinInstallStatus::AlreadyPublished)
    {
        std::lock_guard g(m->mu);
        auto ex = m->exports.find(dll);
        if (ex != m->exports.end())
        {
            auto sym = ex->second.find(symbol);
            if (sym != ex->second.end()) return sym->second;
        }
    }

    // 3) Unknown, unsupported, or failed install: the loader reports the
    // complete missing list and refuses the image.
    return nullptr;
}

} // namespace cap13
