// worker-pad-defer-1 host proof for the FIXED NativePadInput.cpp
// (staging/native-pad-input-wpd1/NativePadInput.cpp).
//
// Drives the REAL ConfigureRetainedPS5PadInput and the REAL PS5PadSource with
// an explicitly owned PadBackend stub replacing only the hardware factory
// boundary (same pattern as the lineage owned_config_controls.cpp host test).
// The composition's shared diagnostic sink is replaced by a capturing stub
// with the same C signature; no SDK, hardware, Lua mod or engine execution.
//
// Cases:
//   (a) unavailable-then-available: deferral proceeds, diag recorded, source
//       not supplied; Handler::init correctly refuses the unavailable source
//       (the documented composition gate); a later explicit activate() on the
//       factory-recorded source DOES connect (capability exists at source
//       level; the composition currently wires no retry - documented limit).
//   (b) permanently unavailable (the live case, pids 317/341/355/370):
//       startup function STILL returns normally - the runtime starts
//       input-less; borrowed user service is never terminated; no close on a
//       handle that was never acquired.
//   + preserved semantics: absent INI no-op, parse refusal, conflict throw,
//     available-source assignment.
#include "PS5PadConfig.hpp"
#include "NativePadInput.hpp"
#include <Platform/NativeModSource.hpp>
#include <Platform/NativeCppMod.hpp>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
using namespace RC::Input;

struct Control
{
    int factory_calls{}, close_calls{}, reads{}, user_initialize_calls{}, user_terminate_calls{}, selected_user{-1};
    int user_initialize_result{0};       // 0 = we own the service; 0x80960003 = borrowed (already initialized)
    int open_error{0};                   // 0 = open succeeds; negative scePad error otherwise
    bool connected{true};
    PS5_PadData sample{};
} control;

static constexpr int kLiveOpenError = -2137915388; // 0x80920004 PS5_PAD_ERROR_ALREADY_OPENED, pid 355/370

static int user_initialize(void*) { ++control.user_initialize_calls; return control.user_initialize_result; }
static int pad_initialize() { return 0; }
static int user_list(int users[4]) { users[0]=12;users[1]=13;users[2]=users[3]=-1;return 0; }
static int open_pad(int user,int port,int index,void* extra)
{
    assert(user==12||user==13);assert(port==0&&index==0&&!extra);
    control.selected_user=user;
    return control.open_error ? control.open_error : 7;
}
static int read_pad(int handle,PS5_PadData* data) { assert(handle==7);++control.reads;*data=control.sample;return 0; }
static int close_pad(int handle) { assert(handle==7);++control.close_calls;return 0; }
static int user_terminate() { ++control.user_terminate_calls;return 0; }
static PadBackend backend{user_initialize,pad_initialize,user_list,open_pad,read_pad,close_pad,user_terminate};

namespace RC::Input {
// Host control replaces only the hardware factory boundary with the existing
// production PS5PadSource driven by the explicitly owned backend above. The
// created source is recorded so the test can observe later availability on
// the same object (the composition itself discards it on deferral).
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

// Host link boundary stubs: the port NativeModSource.cpp references the cppmod
// subsystem, which this test never exercises (files-only NativeModSource).
// Only the two symbols the linker needs are stubbed; the pad path is real.
namespace RC::Platform {
    auto NativeCppMod::Prepare(NativeCppModSpec) -> std::shared_ptr<const NativeCppMod>
    { throw std::logic_error("cppmod boundary unused in the pad-defer host test"); }
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
static bool diag_has(const char* tag, unsigned int value)
{
    for(const auto& e:diag_log) if(e.first==tag&&e.second==value) return true;
    return false;
}

int main()
{
    int checks=0;

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

    // (a) unavailable-then-available: live ALREADY_OPENED on the first open.
    {
        control={};control.user_initialize_result=0;control.open_error=kLiveOpenError;
        std::shared_ptr<PlatformInputSource> input;
        // THE FIX: no throw where the original refused startup.
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        assert(!input);++checks; // input-less start, nothing supplied
        assert(control.factory_calls==1&&control.selected_user==13);++checks;
        // Exact deferral evidence on the diag channel: phase and raw error.
        assert(diag_has("pad_input_deferred",static_cast<unsigned int>(PadPhase::Open)));++checks;
        assert(diag_has("pad_input_deferred_error",static_cast<unsigned int>(kLiveOpenError)));++checks;
        // Documented composition gate at Handler level: an unavailable source
        // is refused by init - which is exactly why the deferral must not
        // supply it (UE4SSProgram would otherwise throw on Handler::init).
        Handler handler;
        assert(!handler.init(recorded_source));++checks;
        // Capability proof (NOT wired into the composition): once the handle
        // frees, an explicit activate() on the same source connects.
        control.open_error=0;
        assert(recorded_source->activate());++checks;
        assert(recorded_source->is_available()&&recorded_source->status().handle==7);++checks;
        Handler handler2;assert(handler2.init(recorded_source));++checks;
        // Owned-handle discipline: shutdown closes OUR handle; dropping the
        // recorded reference then undoes OUR user service exactly once.
        assert(handler2.shutdown()&&control.close_calls==1);++checks;
        recorded_source.reset();
        assert(control.user_terminate_calls==1&&control.close_calls==1);++checks;
    }

    // (b) permanently unavailable (the empirically NORMAL live case): the
    // borrowed service path - initialize reports already-initialized, so the
    // host owns it and we must never terminate it.
    {
        control={};control.user_initialize_result=static_cast<int>(0x80960003u);control.open_error=kLiveOpenError;
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input); // STILL no throw
        assert(!input&&control.factory_calls==1);++checks;
        assert(diag_has("pad_input_deferred",static_cast<unsigned int>(PadPhase::Open)));++checks;
        assert(diag_has("pad_input_deferred_error",static_cast<unsigned int>(kLiveOpenError)));++checks;
        // Borrowed discipline: no terminate of the host's service, no close.
        assert(control.user_terminate_calls==0&&control.close_calls==0);++checks;
        recorded_source.reset(); // borrowed source drop: still no terminate/close
        assert(control.user_terminate_calls==0&&control.close_calls==0);++checks;
    }

    // Preserved semantics: available source is still assigned, and a second
    // INI with an existing source still conflicts.
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
    }

    std::printf("%d pad-defer host checks passed (real ConfigureRetainedPS5PadInput + real PS5PadSource, owned backend; "
                "deferral phase=%d error=%d recorded; no SDK, hardware, Lua mod or engine execution)\n",
        checks,static_cast<int>(PadPhase::Open),kLiveOpenError);
    return 0;
}
