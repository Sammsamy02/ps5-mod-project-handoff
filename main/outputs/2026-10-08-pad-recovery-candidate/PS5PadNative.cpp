#include "PS5PadSource.hpp"
#include "PadServiceLease.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#ifndef __PROSPERO__
#error Native pad provider requires real PS5 target
#endif
extern "C" int sceUserServiceTerminate(void); // Retained SDK samples/browser/main.c:27.
// Shared composition diagnostic sink (diagnostic.c), the same in-link C symbol
// PS5PadSource.cpp / NativePadInput.cpp already declare; adds no import.
extern "C" void diagnostic_event(const char* tag, unsigned int value);
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
// All caller-side operations are serialized; the worker publishes only the two
// atomics. One live borrowed lease is supported, matching the sole input source.
// It is never closed by this provider. A different lease must await its retirement.
constinit std::atomic<bool> borrow_owner_ready{false};
std::mutex borrow_mutex;
struct BorrowWorkerOwner {
    std::thread worker;
    ~BorrowWorkerOwner() { if (worker.joinable()) worker.join(); }
} borrow_owner;
struct BorrowReadyMarker {
    BorrowReadyMarker() { borrow_owner_ready.store(true,std::memory_order_release); }
    ~BorrowReadyMarker() { borrow_owner_ready.store(false,std::memory_order_release); }
} borrow_ready_marker;
bool borrow_stopping=false;
int borrow_user=-1,borrow_type=-1,borrow_index=-1;
bool same_tuple(int user,int type,int index) {
    return borrow_user==user && borrow_type==type && borrow_index==index;
}
void retire_completed_borrow() {
    // Caller owns borrow_mutex. Running calls cannot be reset or overlapped.
    if (borrow_owner.worker.joinable()) borrow_owner.worker.join();
    borrow_result.store(-1,std::memory_order_release);
    borrow_state.store(kBorrowIdle,std::memory_order_release);
    borrow_user=borrow_type=borrow_index=-1;
}
void start_borrow(int user, int type, int index) {
    if (borrow_stopping || borrow_state.load(std::memory_order_acquire)!=kBorrowIdle) return;
    borrow_user=user;borrow_type=type;borrow_index=index;
    borrow_state.store(kBorrowRunning,std::memory_order_release);
    diagnostic_event("pad_borrow_begin",static_cast<unsigned int>(user));
    try {
        borrow_owner.worker=std::thread([user,type,index] {
            const int game_handle=scePadGetHandle(user,type,index);
            diagnostic_event("pad_borrow_returned",static_cast<unsigned int>(game_handle));
            borrow_result.store(game_handle,std::memory_order_release);
            borrow_state.store(kBorrowDone,std::memory_order_release);
        });
    } catch (...) {
        // No worker exists when construction fails; permit a later paced retry.
        borrow_result.store(-1,std::memory_order_release);
        borrow_state.store(kBorrowDone,std::memory_order_release);
        diagnostic_event("pad_borrow_thread_start_failed",1u);
    }
}
int open_or_borrow(int user, int type, int index, void* param) {
    if (!borrow_owner_ready.load(std::memory_order_acquire)) return kPadAlreadyOpened;
    std::lock_guard lock(borrow_mutex);
    if (borrow_stopping) return kPadAlreadyOpened;
    const int handle=scePadOpen(user,type,index,param);
    if (handle!=kPadAlreadyOpened) return handle;
    if (open_calls.fetch_add(1,std::memory_order_acq_rel)==0) {
        first_open_ms.store(steady_ms(),std::memory_order_release);
        return handle; // module_start: never borrow here (old deferral, byte-for-byte behavior).
    }
    if (borrow_state.load(std::memory_order_acquire)==kBorrowDone &&
        (!same_tuple(user,type,index) || borrow_result.load(std::memory_order_acquire)<0)) {
        // Existing PS5PadSource retries provide pacing; this does not busy-loop.
        retire_completed_borrow();
    }
    if (borrow_state.load(std::memory_order_acquire)!=kBorrowDone) {
        if (steady_ms()-first_open_ms.load(std::memory_order_acquire)>=PAD_BORROW_DELAY_MS)
            start_borrow(user,type,index);
        return handle; // Deferred, exactly as before; the poll()-side retry comes back.
    }
    const int game_handle=borrow_result.load(std::memory_order_acquire);
    if (game_handle<0) return handle;
    const int current=borrowed_handle.load(std::memory_order_acquire);
    if (current>=0 && current!=game_handle) return handle; // sole active borrowed lease
    int expected=-1;
    if (borrowed_handle.compare_exchange_strong(expected,game_handle,std::memory_order_acq_rel))
        diagnostic_event("pad_open_class",1u);
    return game_handle;
}
int close_owned(int handle) {
    if (!borrow_owner_ready.load(std::memory_order_acquire)) return kPadAlreadyOpened;
    std::lock_guard lock(borrow_mutex);
    int expected=handle;
    if (borrowed_handle.compare_exchange_strong(expected,-1,std::memory_order_acq_rel)) {
        if (borrow_state.load(std::memory_order_acquire)==kBorrowDone) retire_completed_borrow();
        return 0;
    }
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
}
// Required module-stop consumer: close future helper starts BEFORE querying
// residence. Nonzero/unknown refuses unload; a stuck OS call is never canceled.
// This candidate is not deployable until the real entry calls this export.
extern "C" int bridge_native_pad_code_residence_required(void) {
    if (!borrow_owner_ready.load(std::memory_order_acquire)) return -1;
    std::lock_guard lock(borrow_mutex);
    borrow_stopping=true;
    if (borrow_state.load(std::memory_order_acquire)==kBorrowRunning) return 1;
    if (borrow_owner.worker.joinable()) borrow_owner.worker.join();
    return 0;
}
namespace RC::Input {
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding> bindings, int user) {
    auto source=std::make_shared<PS5PadSource>(std::move(bindings),PadBackend{
        acquire_service,pad_initialize_once,sceUserServiceGetLoginUserIdList,open_or_borrow,
        scePadReadState,close_owned,release_service},user);
    source->activate();
    return source;
}
}
