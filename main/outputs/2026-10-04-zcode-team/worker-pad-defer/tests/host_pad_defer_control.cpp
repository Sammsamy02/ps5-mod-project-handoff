// worker-pad-defer-1 CONTROL: the ORIGINAL (spike) NativePadInput.cpp against
// the identical owned-backend stub used by host_pad_defer.cpp, in the
// unavailable-then-available scenario of live boots 317/341/355/370.
//
// Purpose: prove this harness DETECTS the original bug - the original code
// must THROW with the exact live refusal message:
//   "PS5Pad input unavailable: phase=6, error=-2137915388"
// The original source is compiled straight from the spike directory (read
// only; never edited in place).
#include "PS5PadConfig.hpp"
#include "NativePadInput.hpp"
#include <Platform/NativeModSource.hpp>
#include <Platform/NativeCppMod.hpp>
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
using namespace RC::Input;

// Host link boundary stubs: the port NativeModSource.cpp references the cppmod
// subsystem, which this test never exercises (files-only NativeModSource).
namespace RC::Platform {
    auto NativeCppMod::Prepare(NativeCppModSpec) -> std::shared_ptr<const NativeCppMod>
    { throw std::logic_error("cppmod boundary unused in the pad-defer control test"); }
    auto NativeCppMod::Retire() const -> void {}
}

struct Control
{
    int factory_calls{}, close_calls{}, reads{}, user_initialize_calls{}, user_terminate_calls{}, selected_user{-1};
    int user_initialize_result{0};
    int open_error{0};
    PS5_PadData sample{};
} control;

static constexpr int kLiveOpenError = -2137915388; // 0x80920004 PS5_PAD_ERROR_ALREADY_OPENED

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
std::shared_ptr<PS5PadSource> make_native_ps5_pad_source(std::vector<PadBinding> bindings,int user)
{
    ++control.factory_calls;
    auto source=std::make_shared<PS5PadSource>(std::move(bindings),backend,user);
    source->activate();return source;
}
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

int main()
{
    int checks=0;
    control={};control.user_initialize_result=0;control.open_error=kLiveOpenError;
    std::shared_ptr<PlatformInputSource> input;
    // The ORIGINAL code refuses startup exactly as pid 355/370 did on console.
    try
    {
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        assert(false); // the bug: this must be unreachable
    }
    catch(const std::runtime_error& error)
    {
        const std::string expected="PS5Pad input unavailable: phase="+
            std::to_string(static_cast<int>(PadPhase::Open))+", error="+std::to_string(kLiveOpenError);
        assert(std::string(error.what())==expected);++checks;
        std::printf("control: original code threw the live refusal verbatim: %s\n",error.what());
    }
    assert(!input&&control.factory_calls==1&&control.selected_user==13);++checks;
    // Borrowed-service discipline existed in the original too (owned here).
    assert(control.user_terminate_calls==1&&control.close_calls==0);++checks;
    std::printf("%d control checks passed: the harness reproduces the launch-10 blocker against the ORIGINAL spike source\n",checks);
    return 0;
}
