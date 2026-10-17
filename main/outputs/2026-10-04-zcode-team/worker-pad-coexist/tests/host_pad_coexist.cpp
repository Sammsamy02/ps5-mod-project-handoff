// worker-pad-coexist host proof.
//
// Drives the REAL staged units end-to-end - staging/native-pad-input-coexist/
// NativePadInput.cpp (deferred-ATTACHED), staging/input-handler-coexist/
// Handler.cpp (deferred attach + reachable poll + tolerant registration),
// staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.cpp (bounded
// event-driven retry inside poll()) - with an explicitly owned PadBackend stub
// replacing only the hardware factory boundary (the lineage host-1 pattern).
// The composition's shared diagnostic sink is replaced by a capturing stub
// with the same C signature; no SDK, hardware, Lua mod or engine execution.
//
// Cases:
//   (a) unavailable-then-available (THE RETRY PATH): live ALREADY_OPENED at
//       configure; the source is deferred-ATTACHED; Handler::init accepts it;
//       F7 (OPTIONS) registration succeeds at load time; process_event ticks
//       drive the poll()-side retry; the open succeeds; availability is
//       observed; a rising OPTIONS edge dispatches to the registered callback
//       (outcome B end-to-end at host level).
//   (b) attempt-class control (the SDK route): every open - construction and
//       every retry - uses ONLY the standard class (user, PS5_PAD_PORT_TYPE_
//       STANDARD, index 0, param null); pad_open_class=0 is emitted exactly
//       once. No second-handle class is attempted (SDK-ANALYSIS.md verdict).
//   (c) permanent unavailability: open never succeeds; retry attempts follow
//       the bounded backoff schedule exactly (no busy loop); startup still
//       returns normally with the source attached (outcome A preserved);
//       borrowed user service never terminated; no close of an unacquired
//       handle.
//   (d) lifecycle contract: all retry-phase backend calls run on the thread
//       that drives process_event (sole processing thread); construction-time
//       calls on the configuring thread; a second Handler::init never
//       re-injects and performs zero backend calls.
// ODR/layout: the host-1 recipe -I order resolves "PS5PadSource.hpp" to the
// BASE provider header, but the staged provider TU compiles against the
// STAGED header (three appended retry members). This TU allocates the object
// (make_shared in the factory control below), so it MUST see the staged
// definition. The pinned SDL header carries no include guard and the two
// provider headers are distinct files, so this TU includes ONLY the staged
// provider header and NOTHING that chains to the base one (NativePadInput.hpp
// forward-declares; PS5PadConfig.hpp is unused here - the parser runs in the
// NativePadInput TU). ASan proved the mismatch: heap-buffer-overflow in the
// ctor writing the appended members past the base-size allocation.
#include "../staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.hpp"
#include "NativePadInput.hpp"
#include <Input/KeyDef.hpp>
#include <Platform/NativeModSource.hpp>
#include <Platform/NativeCppMod.hpp>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
using namespace RC::Input;

struct Control
{
    int factory_calls{}, close_calls{}, reads{}, user_initialize_calls{}, user_terminate_calls{}, selected_user{-1};
    int pad_initialize_calls{};
    int user_initialize_result{0};       // 0 = we own the service; 0x80960003 = borrowed (already initialized)
    int open_error{0};                   // 0 = open succeeds; negative scePad error otherwise
    bool connected{true};
    PS5_PadData sample{};
    // worker-pad-coexist (d): thread identity of every backend call.
    std::vector<std::thread::id> open_threads, read_threads, close_threads, init_threads;
} control;

static constexpr int kLiveOpenError = -2137915388; // 0x80920004 PS5_PAD_ERROR_ALREADY_OPENED, pid 355/370

static int user_initialize(void*) { ++control.user_initialize_calls; control.init_threads.push_back(std::this_thread::get_id()); return control.user_initialize_result; }
static int pad_initialize() { ++control.pad_initialize_calls; control.init_threads.push_back(std::this_thread::get_id()); return 0; }
static int user_list(int users[4]) { users[0]=12;users[1]=13;users[2]=users[3]=-1;return 0; }
static int open_pad(int user,int port,int index,void* extra)
{
    assert(user==12||user==13);
    assert(port==PS5_PAD_PORT_TYPE_STANDARD&&index==0&&!extra); // (b) standard class ONLY, every attempt
    control.selected_user=user;
    control.open_threads.push_back(std::this_thread::get_id());
    return control.open_error ? control.open_error : 7;
}
static int read_pad(int handle,PS5_PadData* data) { assert(handle==7);++control.reads;control.read_threads.push_back(std::this_thread::get_id());*data=control.sample;data->connected=control.connected;return 0; }
static int close_pad(int handle) { assert(handle==7);++control.close_calls;control.close_threads.push_back(std::this_thread::get_id());return 0; }
static int user_terminate() { ++control.user_terminate_calls;return 0; }
static PadBackend backend{user_initialize,pad_initialize,user_list,open_pad,read_pad,close_pad,user_terminate};

namespace RC::Input {
// Host control replaces only the hardware factory boundary; the production
// (staged) PS5PadSource is driven through the staged composition path.
std::shared_ptr<PS5PadSource> recorded_source;
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding> bindings,int user)
{
    ++control.factory_calls;
    recorded_source=std::make_shared<PS5PadSource>(std::move(bindings),backend,user);
    recorded_source->activate();return recorded_source;
}
}

// Capturing stand-in for the composition's diagnostic.c sink (same C symbol,
// same signature; diagnostic_event is defined in-link in the composition).
std::vector<std::pair<std::string,unsigned int>> diag_log;
extern "C" void diagnostic_event(const char* tag, unsigned int value)
{
    diag_log.emplace_back(tag,value);
}
static int diag_count(const char* tag)
{
    int n=0; for(const auto& e:diag_log) if(e.first==tag) ++n; return n;
}
static bool diag_has(const char* tag, unsigned int value)
{
    for(const auto& e:diag_log) if(e.first==tag&&e.second==value) return true;
    return false;
}

// Host link boundary stubs: the port NativeModSource.cpp references the cppmod
// subsystem, which this test never exercises (files-only NativeModSource).
namespace RC::Platform {
    auto NativeCppMod::Prepare(NativeCppModSpec) -> std::shared_ptr<const NativeCppMod>
    { throw std::logic_error("cppmod boundary unused in the pad-coexist host test"); }
    auto NativeCppMod::Retire() const -> void {}
}

static auto files(std::string text,bool include=true) -> RC::Platform::NativeModSource
{
    auto owner=std::make_shared<const std::string>(std::move(text));
    std::vector<RC::Platform::NativeModFile> list;
    if(include)list.push_back({"ue4ss/PS5Pad.ini",{owner,*owner}});
    return RC::Platform::NativeModSource(std::move(list));
}
static constexpr const char* config=R"([PS5Pad]
User = 13
[Bindings]
OPTIONS = F7
)";
// Retry pacing mirrors staging PS5PadSource.cpp (asserted by (c) exactly).
static constexpr uint32_t kRetryInitial=200, kRetryMax=3200;

// One UpdateThread-equivalent tick through the staged Handler.
static void tick(Handler& handler) { handler.process_event(); }
static void bump(unsigned int buttons) { control.sample.buttons=buttons; ++control.sample.timestamp; }

int main()
{
    int checks=0;
    const auto main_tid=std::this_thread::get_id();

    // Preserved semantics: absent INI is a no-op, no factory call.
    {
        control={};control.user_initialize_result=0;
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files("",false),input);
        assert(!input&&control.factory_calls==0);++checks;
    }
    // Preserved semantics: parse refusal still throws before the factory.
    {
        std::shared_ptr<PlatformInputSource> input;
        try{modbridge::native::ConfigureRetainedPS5PadInput(files("bad"),input);assert(false);}
        catch(const std::invalid_argument&){}
        assert(!input&&control.factory_calls==0);++checks;
    }
    // Preserved semantics: available source is assigned, second INI conflicts.
    {
        control={};control.user_initialize_result=static_cast<int>(0x80960003u);control.open_error=0;
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&pad->is_available()&&pad->status().user_id==13);++checks;
        auto existing=input;
        try{modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);assert(false);}
        catch(const std::invalid_argument&){}
        assert(input==existing&&control.factory_calls==1);++checks;
        Handler h;assert(h.init(input)&&h.is_input_available());++checks;
        assert(control.close_calls==0);
        assert(h.shutdown()&&control.close_calls==1);++checks;
    }

    // ===== (a) unavailable-then-available: THE RETRY PATH, outcome B at host level =====
    {
        control={};control.user_initialize_result=0;control.open_error=kLiveOpenError;
        std::shared_ptr<PlatformInputSource> input;
        // Startup floor (wpd1 preserved): no throw, exact deferral evidence.
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        assert(diag_has("pad_input_deferred",static_cast<unsigned int>(PadPhase::Open)));++checks;
        assert(diag_has("pad_input_deferred_error",static_cast<unsigned int>(kLiveOpenError)));++checks;
        // THE COEXIST DELTA: the deferred source is ATTACHED, not dropped.
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&!pad->is_available());++checks;
        // Handler::init accepts the deferred source (no cross-thread re-injection
        // exists anywhere; this init runs here exactly as the program runs it).
        Handler handler;
        assert(handler.init(input));++checks;
        assert(!handler.is_input_available());++checks;
        // R2 enabler: binds register AT LOAD despite unavailability.
        std::atomic<int> fired{0};
        handler.register_keydown_event(Key::F7,[&]{++fired;});++checks;
        // (b) construction open ran once on THIS (configuring) thread.
        assert(control.open_threads.size()==1&&control.open_threads[0]==main_tid);++checks;

        // The handle frees while running; the poll-side retry must connect.
        control.open_error=0;
        for(int i=0;i<199;++i) tick(handler);            // no attempt before the interval
        assert(control.open_threads.size()==1);++checks; // still only the construction open
        tick(handler);                                    // poll #200: attempt #1 succeeds
        assert(pad->is_available()&&pad->status().handle==7&&pad->status().user_id==13);++checks;
        assert(diag_count("pad_open_class")==1&&diag_has("pad_open_class",0));++checks;
        assert(diag_count("pad_retry")==1&&diag_has("pad_retry",1));++checks;
        assert(diag_has("pad_available",7u));++checks;
        assert(handler.is_input_available());++checks;
        // Baseline sample is swallowed (rising-edge discipline).
        bump(0);tick(handler);
        assert(fired.load()==0&&control.reads==1);++checks;
        // Fresh rising OPTIONS edge dispatches to the registered F7 callback.
        bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
        assert(fired.load()==1);++checks;
        // Held button: duplicate timestamp and held edge produce no re-fire
        // (fired stays at 1; the count is cumulative).
        tick(handler);
        assert(fired.load()==1);++checks;
        bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
        assert(fired.load()==1);++checks; // edge only, no autorepeat
        bump(0);tick(handler);            // release clears
        assert(fired.load()==1);++checks;
        bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
        assert(fired.load()==2);++checks; // next press fires again
        // (d) every retry/read call ran on THIS thread (the sole processing
        // thread); the configuring-thread calls are exactly the first four.
        for(auto tid:control.open_threads) assert(tid==main_tid);
        for(auto tid:control.read_threads) assert(tid==main_tid);
        for(auto tid:control.init_threads) assert(tid==main_tid);
        ++checks;
        // (d) no re-injection: a second init of the now-available source is a
        // no-op (same attachment) and performs zero backend calls.
        {
            int opens=static_cast<int>(control.open_threads.size());
            int inits=static_cast<int>(control.init_threads.size());
            assert(handler.init(input));++checks;
            assert(static_cast<int>(control.open_threads.size())==opens&&
                   static_cast<int>(control.init_threads.size())==inits);++checks;
        }
        // Owned-handle teardown unchanged: shutdown closes OUR handle once.
        assert(handler.shutdown()&&control.close_calls==1);++checks;
        recorded_source.reset();
        assert(control.user_terminate_calls==1&&control.close_calls==1);++checks;
    }

    // ===== (c) permanent unavailability: bounded retries, outcome A preserved =====
    {
        control={};control.user_initialize_result=static_cast<int>(0x80960003u); // borrowed service, the live shape
        control.open_error=kLiveOpenError;
        diag_log.clear();
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input); // STILL no throw
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&!pad->is_available());++checks;
        Handler handler;
        assert(handler.init(input));++checks;      // deferred attach
        assert(!handler.is_input_available());++checks;
        handler.register_keydown_event(Key::F7,[]{});++checks; // binds still live
        // Drive 1000 ticks: attempts at polls 200 (interval 200) and 600
        // (interval 400); the next would be 1400 > 1000. Exactly 2 retries.
        const int construction_opens=1;
        for(int i=0;i<1000;++i) tick(handler);
        const int opens=static_cast<int>(control.open_threads.size())-construction_opens;
        assert(opens==2);++checks;                       // backoff schedule, no busy loop
        assert(control.pad_initialize_calls==3);++checks; // construction + 2 retries
        assert(diag_count("pad_retry")==2);++checks;
        assert(diag_count("pad_retry_failed")==2);++checks;
        assert(diag_count("pad_open_class")==1);++checks; // class emitted once per session
        assert(diag_has("pad_retry_failed",static_cast<unsigned int>(kLiveOpenError)));++checks;
        assert(!pad->is_available()&&!handler.is_input_available());++checks;
        // Outcome A preserved: the source stays attached and startup facts hold.
        assert(handler.get_current_input_source()==std::string("PS5Pad"));++checks;
        // Borrowed discipline: never terminate the host's service, never close
        // a handle that was never acquired.
        assert(control.user_terminate_calls==0&&control.close_calls==0);++checks;
        assert(pad->deactivate()); // explicit teardown path, still no close/terminate
        assert(control.user_terminate_calls==0&&control.close_calls==0);++checks;
    }

    // ===== (d) lifecycle contract, cross-thread driver =====
    {
        control={};control.user_initialize_result=0;control.open_error=kLiveOpenError;
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        Handler handler;
        assert(handler.init(input));++checks;
        handler.register_keydown_event(Key::F7,[]{});
        control.open_error=0; // the handle frees while the update thread runs
        std::thread driver([&]{ for(int i=0;i<201;++i) tick(handler); }); // 201: retry at 200 + one read
        driver.join();
        assert(pad->is_available());++checks; // retry succeeded on the driver thread
        // EVERY construction/retry open, read and service call is attributed:
        // the construction quartet (user_initialize, pad_initialize, user_list,
        // open) belongs to the configuring (main) thread; ALL retry-phase calls
        // belong to the driver thread. No call ever ran on a third thread; no
        // cross-thread Handler::init re-injection exists.
        assert(!control.read_threads.empty());++checks;
        const auto driver_tid=control.open_threads.back();
        assert(driver_tid!=main_tid);++checks;
        assert(control.open_threads.size()==2&&control.open_threads[0]==main_tid);++checks;
        for(size_t i=1;i<control.open_threads.size();++i) assert(control.open_threads[i]==driver_tid);
        for(auto tid:control.read_threads) assert(tid==driver_tid);
        assert(control.init_threads.size()==3);++checks; // user_initialize, pad_initialize, retry pad_initialize
        assert(control.init_threads[0]==main_tid&&control.init_threads[1]==main_tid);++checks;
        assert(control.init_threads[2]==driver_tid);++checks;
        assert(handler.shutdown()&&control.close_calls==1);++checks;
        assert(control.close_threads.size()==1&&control.close_threads[0]==main_tid);++checks;
    }

    std::printf("%d pad-coexist host checks passed (staged NativePadInput + Handler + PS5PadSource real; owned backend; "
                "deferred-attach, poll-side retry, standard-class-only opens, bounded backoff, same-thread lifecycle; "
                "no SDK, hardware, Lua mod or engine execution)\n",checks);
    return 0;
}
