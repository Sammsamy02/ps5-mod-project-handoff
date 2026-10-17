#include "PS5PadSource.hpp"
#include "PadServiceLease.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#ifndef __PROSPERO__
#error Native pad provider requires real PS5 target
#endif
extern "C" int sceUserServiceTerminate(void); // Retained SDK samples/browser/main.c:27.
// Shared composition diagnostic sink (diagnostic.c), the same in-link C symbol
// PS5PadSource.cpp / NativePadInput.cpp already declare; adds no import.
extern "C" void diagnostic_event(const char* tag, unsigned int value);
// [kernel services r15] (worker-ue4ss-kernel, 2026-10-06): the kernel input
// reader adapter (kekcall nr=9, worker-kernel-input INPUT-SERVICE.md §6.1).
// Included AFTER PS5PadSource.hpp so PS5_PadData is defined (adapter
// requirement). Host-tested byte-exact in worker-ue4ss-kernel/host-test.
#include "KekcallPadAdapter.hpp"
static_assert(sizeof(PS5_PadData)==120);
static_assert(offsetof(PS5_PadData,buttons)==0 && offsetof(PS5_PadData,connected)==76);
static_assert(offsetof(PS5_PadData,timestamp)==80 && offsetof(PS5_PadData,count)==104);
namespace {
RC::Input::Detail::PadServiceLease user_service(sceUserServiceInitialize,sceUserServiceTerminate);
int acquire_service(void* arg) { return user_service.acquire(arg); }
int release_service() { return user_service.release(); }

// [claude 2026-10-06] Borrowed-handle open (ROUTE P3 of worker-pad-defer
// PROPOSAL-ALTERNATE-COEXIST-ROUTE). The game already holds the standard port in THIS
// process, so scePadOpen(user, STANDARD, 0) returns PS5_PAD_ERROR_ALREADY_OPENED.
// scePadGetHandle(user, type, index) (SDL_ps5joystick.h:106) should return that handle;
// scePadReadState on it is a non-consuming snapshot. A borrowed handle is never closed.
//
// rev 2 (launch ca7e071f, session 162): the synchronous GetHandle call never returned
// on console - module_start stopped inside the pad factory (no pad tag, no
// runtime_start_ok) and ploader reported FAIL. GetHandle now runs ONCE on a detached
// helper thread; open_or_borrow never blocks. Until the helper finishes, the open
// reports the original ALREADY_OPENED (exactly the old deferred behavior), and
// PS5PadSource's poll()-side retry picks the handle up once it exists.
// Evidence: pad_borrow_begin (user id) before the call, pad_borrow_returned (raw rc)
// after it, pad_open_class=1 when the borrowed handle is first used. begin without
// returned = the call hangs on this firmware.
//
// rev 3 (session 174 crashed after pad_borrow_begin; root cause in packaging, not
// here: package_prx.py cloned the scePadGetHandle import identity from sceKernelOpen,
// publishing it under libkernel instead of libScePad, so the call could not bind).
// Belt and braces: the borrow is never attempted during module_start (the factory's
// first open) and only from a poll()-side retry at least PAD_BORROW_DELAY_MS later,
// i.e. after the runtime and mods have started and their evidence is written.
constexpr int kPadAlreadyOpened = static_cast<int>(PS5_PAD_ERROR_ALREADY_OPENED);
#ifndef PAD_BORROW_DELAY_MS
#define PAD_BORROW_DELAY_MS 10000
#endif
enum : int { kBorrowIdle = 0, kBorrowRunning = 1, kBorrowDone = 2 };
std::atomic<int> open_calls{0};
std::atomic<long long> first_open_ms{0};
long long steady_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
std::atomic<int> borrow_state{kBorrowIdle};
std::atomic<int> borrow_result{-1};
std::atomic<int> borrowed_handle{-1};
void start_borrow(int user, int type, int index) {
    int expected=kBorrowIdle;
    if (!borrow_state.compare_exchange_strong(expected,kBorrowRunning,std::memory_order_acq_rel)) return;
    diagnostic_event("pad_borrow_begin",static_cast<unsigned int>(user));
    std::thread([user,type,index] {
        const int game_handle=scePadGetHandle(user,type,index);
        diagnostic_event("pad_borrow_returned",static_cast<unsigned int>(game_handle));
        borrow_result.store(game_handle,std::memory_order_release);
        borrow_state.store(kBorrowDone,std::memory_order_release);
    }).detach();
}
int open_or_borrow(int user, int type, int index, void* param) {
    const int handle=scePadOpen(user,type,index,param);
    if (handle!=kPadAlreadyOpened) return handle;
    if (open_calls.fetch_add(1,std::memory_order_acq_rel)==0) {
        first_open_ms.store(steady_ms(),std::memory_order_release);
        return handle; // module_start: never borrow here (old deferral, byte-for-byte behavior).
    }
    if (borrow_state.load(std::memory_order_acquire)!=kBorrowDone) {
        if (steady_ms()-first_open_ms.load(std::memory_order_acquire)>=PAD_BORROW_DELAY_MS)
            start_borrow(user,type,index);
        return handle; // Deferred, exactly as before; the poll()-side retry comes back.
    }
    const int game_handle=borrow_result.load(std::memory_order_acquire);
    if (game_handle<0) return handle; // GetHandle refused: keep ALREADY_OPENED (one attempt only).
    int expected=-1;
    if (borrowed_handle.compare_exchange_strong(expected,game_handle,std::memory_order_acq_rel))
        diagnostic_event("pad_open_class",1u);
    return game_handle;
}
int close_owned(int handle) {
    int expected=handle;
    if (borrowed_handle.compare_exchange_strong(expected,-1,std::memory_order_acq_rel)) return 0;
    return scePadClose(handle);
}
// The poll()-side retry re-runs activate(), which calls pad_initialize each time; only
// the first scePadInit is real (its success is cached), so a retry can never fail on
// an already-initialized library (worker-pad-defer AUDIT FINDING-1).
std::atomic<bool> pad_initialized{false};
int pad_initialize_once() {
    if (pad_initialized.load(std::memory_order_acquire)) return 0;
    const int err=scePadInit();
    if (err==0) pad_initialized.store(true,std::memory_order_release);
    return err;
}

// [kernel services r15] KERNEL INPUT READER ROUTE (assignment item 2c):
// when the rebuilt kstuff module carries kekcall nr=9, pad state comes from
// the kernel service — NO scePadOpen, NO scePadGetHandle borrow, NO handle
// exclusivity (the ALREADY_OPENED failure class does not exist on this
// route), NO deferred helper thread, NO PadServiceLease ownership. The
// route is decided ONCE per source construction (make_native_ps5_pad_source
// below); on a pre-rebuild boot (ENOSYS -78 / NOCHANNEL) or a firmware
// without derived offsets (KINPUT_ERR_NOSTATE -19), try_open refuses and
// the borrowed-handle native route above runs BYTE-FOR-BYTE (assignment
// item 4 fallback law). The retained PS5PadSource edge semantics
// (duplicate-timestamp guard, baseline swallow, rising edge, Intercepted
// discard, PadEdgeAdapter debounce) are UNTOUCHED: the adapter satisfies
// the same read_state contract scePadReadState does.
int pad_open_kernel(int /*user*/, int /*type*/, int index, void* /*param*/) {
    const int handle=KekcallPad::try_open(index);
    if (handle>=0) diagnostic_event("pad_open_class",2u); // 2 = kernel input reader
    return handle>=0?handle:-1;
}
int pad_read_state_kernel(int handle, PS5_PadData* data) {
    return KekcallPad::read_state(handle,data);
}
int pad_close_kernel(int handle) {
    return KekcallPad::close(handle);
}
int user_initialize_kernel(void* arg) {
    (void)arg; return 0; // no sceUserService lease on the kernel route
}
int pad_initialize_kernel() {
    return 0; // no scePadInit on the kernel route
}
int user_terminate_kernel() {
    return 0; // no lease was acquired: nothing to terminate
}
}
namespace RC::Input {
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding> bindings, int user) {
    // [kernel services r15] ROUTE DECISION, once, before construction. The
    // transport install is idempotent (first caller wins; the initializer
    // and the bridge install the same vtable). try_open(0) scans ports 0..3
    // and locks the first connected pad through one full KIN1 snapshot; a
    // clean refusal (old module / no channel / no offsets) selects the
    // borrowed-handle native route with the EXACT pre-r15 backend
    // expression — the fallback path is byte-for-byte, not a re-derivation.
#ifdef __PROSPERO__
    ksrv::init(kekcall_console::vtable());
#endif
    const bool kernel_route=(KekcallPad::try_open(0)==KekcallPad::kKernelPadHandle);
    diagnostic_event("pad_route",kernel_route?1u:0u); // 1 = kernel reader, 0 = native borrow
    std::shared_ptr<PS5PadSource> source;
    if (kernel_route) {
        source=std::make_shared<PS5PadSource>(std::move(bindings),PadBackend{
            user_initialize_kernel,pad_initialize_kernel,sceUserServiceGetLoginUserIdList,pad_open_kernel,
            pad_read_state_kernel,pad_close_kernel,user_terminate_kernel},user);
    } else {
        source=std::make_shared<PS5PadSource>(std::move(bindings),PadBackend{
            acquire_service,pad_initialize_once,sceUserServiceGetLoginUserIdList,open_or_borrow,
            scePadReadState,close_owned,release_service},user);
    }
    source->activate();
    return source;
}
}
