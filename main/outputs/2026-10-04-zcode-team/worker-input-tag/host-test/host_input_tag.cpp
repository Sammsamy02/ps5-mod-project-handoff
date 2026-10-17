// worker-input-tag host proof.
//
// Drives the REAL STAGED units end-to-end - staging/ue4ss-native-pad-provider-
// input-tag/port/PS5PadSource.cpp (once-per-boot pad_first_state_read) and
// staging/input-handler-input-tag/Handler.cpp (bounded pad_key_edge /
// pad_key_edge_silent arrival tags) - plus the ADMITTED, byte-unchanged
// coexist baselines for everything else (native-pad-input-coexist/
// NativePadInput.cpp deferred-ATTACH configure path; the staged provider
// header PS5PadSource.hpp is carried byte-identical, sha256 2d63c094...).
// The hardware factory boundary is the lineage owned PadBackend stub; the
// composition's diagnostic.c sink is replaced by a capturing stub with the
// same C signature (defined in-link, like the composition). No SDK,
// hardware, Lua mod or engine execution.
//
// THE PROCESS IS ONE BOOT: the staged once-per-boot flag (file-local in the
// provider TU) and the per-boot tag budget (function-local statics in
// Handler::process_event) persist across scenarios BY DESIGN. Scenario order
// is therefore load-bearing and the expected counts are cumulative:
//   (a) read half, both polarities, before any key edge exists
//   (b) six distinct mod keybind edges -> tag values; budget exhaustion at
//       exactly 60 tags + one pad_key_edge_silent; presses STILL fire
//       callbacks after the cap (no behavior change)
//   (c) disconnect -> reconnect: still exactly one pad_first_state_read
//   (d) allow_input=false: no events, no tags, no fires (G-AP3 preserved)
//   (e) input-dead control: open never succeeds -> arrival tag counts
//       never move, backoff schedule exactly as admitted
#include "../staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.hpp"
#include "NativePadInput.hpp"
#include <Input/KeyDef.hpp>
#include <Platform/NativeModSource.hpp>
#include <Platform/NativeCppMod.hpp>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace RC::Input;

// (b) prerequisite: the six mod keybind codes are pairwise distinct, and
// OPTIONS -> F7 = 0x76 so the first OPTIONS press is unambiguous.
static_assert(Key::F7 == 0x76 && Key::RETURN == 0x0D &&
              Key::LEFT_ARROW == 0x25 && Key::UP_ARROW == 0x26 &&
              Key::RIGHT_ARROW == 0x27 && Key::DOWN_ARROW == 0x28,
              "mod keybind codes must be the documented distinct values");

struct Control
{
    int factory_calls{}, close_calls{}, reads{}, user_initialize_calls{}, user_terminate_calls{}, selected_user{-1};
    int pad_initialize_calls{};
    int open_attempts{};
    int user_initialize_result{0};       // 0 = owned; 0x80960003 = borrowed (live shape)
    int open_error{0};                   // 0 = open succeeds; negative scePad error otherwise
    int read_error{0};                   // nonzero = read_state failure injection
    bool connected{true};
    PS5_PadData sample{};
} control;

static constexpr int kLiveOpenError = -2137915388; // 0x80920004 PS5_PAD_ERROR_ALREADY_OPENED, pid 355/370
static constexpr unsigned int kTagCap = 60;        // mirrors staged Handler.cpp pad_key_edge_tag_cap

static int user_initialize(void*) { ++control.user_initialize_calls; return control.user_initialize_result; }
static int pad_initialize() { ++control.pad_initialize_calls; return 0; }
static int user_list(int users[4]) { users[0]=12;users[1]=13;users[2]=users[3]=-1;return 0; }
static int open_pad(int user,int port,int index,void* extra)
{
    assert(user==12||user==13);assert(port==PS5_PAD_PORT_TYPE_STANDARD&&index==0&&!extra);
    ++control.open_attempts;
    control.selected_user=user;
    return control.open_error ? control.open_error : 7;
}
static int read_pad(int handle,PS5_PadData* data)
{
    assert(handle==7);++control.reads;
    if (control.read_error) return control.read_error; // fail BEFORE touching the out param
    *data=control.sample;data->connected=control.connected;return 0;
}
static int close_pad(int handle) { assert(handle==7);++control.close_calls;return 0; }
static int user_terminate() { ++control.user_terminate_calls;return 0; }
static PadBackend backend{user_initialize,pad_initialize,user_list,open_pad,read_pad,close_pad,user_terminate};

namespace RC::Input {
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

namespace RC::Platform {
    auto NativeCppMod::Prepare(NativeCppModSpec) -> std::shared_ptr<const NativeCppMod>
    { throw std::logic_error("cppmod boundary unused in the input-tag host test"); }
    auto NativeCppMod::Retire() const -> void {}
}

static auto files(std::string text,bool include=true) -> RC::Platform::NativeModSource
{
    auto owner=std::make_shared<const std::string>(std::move(text));
    std::vector<RC::Platform::NativeModFile> list;
    if(include)list.push_back({"ue4ss/PS5Pad.ini",{owner,*owner}});
    return RC::Platform::NativeModSource(std::move(list));
}
// The SIX mod keybinds, mirroring the TownfallItemSpawner main.lua surface
// (Key.F7 = the OPTIONS menu bind, Key.RETURN, Key.UP/DOWN/LEFT/RIGHT_ARROW),
// wired through the REAL PS5Pad.ini parser: pad button -> UE4SS key.
static constexpr const char* config=R"([PS5Pad]
User = 13
[Bindings]
OPTIONS = F7
CIRCLE = RETURN
UP = UP_ARROW
DOWN = DOWN_ARROW
LEFT = LEFT_ARROW
RIGHT = RIGHT_ARROW
)";
static void tick(Handler& handler) { handler.process_event(); }
static void bump(unsigned int buttons) { control.sample.buttons=buttons; ++control.sample.timestamp; }

struct Fires { int f7{};int ret{};int up{};int down{};int left{};int right{}; };
static Fires fires;
static void register_six(Handler& handler)
{
    handler.register_keydown_event(Key::F7,[]{++fires.f7;});
    handler.register_keydown_event(Key::RETURN,[]{++fires.ret;});
    handler.register_keydown_event(Key::UP_ARROW,[]{++fires.up;});
    handler.register_keydown_event(Key::DOWN_ARROW,[]{++fires.down;});
    handler.register_keydown_event(Key::LEFT_ARROW,[]{++fires.left;});
    handler.register_keydown_event(Key::RIGHT_ARROW,[]{++fires.right;});
}
static int total_fires()
{
    return fires.f7+fires.ret+fires.up+fires.down+fires.left+fires.right;
}

int main()
{
    int checks=0;
    const int baseline_fires_at_start=total_fires();

    // ===== (a) G-AP1 read half: read-dead-after-open vs reads-alive =====
    {
        control={};control.user_initialize_result=static_cast<int>(0x80960003u); // borrowed, live shape
        control.open_error=kLiveOpenError;
        assert(diag_count("pad_first_state_read")==0&&diag_count("pad_key_edge")==0);++checks;
        std::shared_ptr<PlatformInputSource> input;
        // Preserved deferral semantics (admitted wpd1/coexist startup floor).
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input); // no throw
        assert(diag_has("pad_input_deferred",static_cast<unsigned int>(PadPhase::Open)));++checks;
        assert(diag_has("pad_input_deferred_error",static_cast<unsigned int>(kLiveOpenError)));++checks;
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&!pad->is_available());++checks;
        Handler handler;
        assert(handler.init(input));++checks;              // deferred attach
        assert(!handler.is_input_available());++checks;
        register_six(handler);++checks;                    // binds live at load (coexist R2)

        // Poll-200 retry opens (exact admitted schedule); success poll is empty.
        control.open_error=0;
        for(int i=0;i<199;++i) tick(handler);
        assert(control.open_attempts==1);++checks;         // construction open only
        tick(handler);                                     // poll #200: retry #1 succeeds
        assert(pad->is_available()&&pad->status().handle==7);++checks;
        assert(control.open_attempts==2);++checks;
        assert(diag_count("pad_open_class")==1&&diag_has("pad_open_class",0));++checks;
        assert(diag_count("pad_retry")==1&&diag_has("pad_retry",1));++checks;
        assert(diag_has("pad_available",7u));++checks;
        // NO reads yet (the success poll returns empty) -> no arrival tags.
        assert(control.reads==0&&diag_count("pad_first_state_read")==0&&diag_count("pad_key_edge")==0);++checks;

        // POLARITY 1 - reads dead AFTER open: pad_available exists but the
        // read path fails; NO pad_first_state_read, NO edges, NO fires.
        control.read_error=-2;
        bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
        assert(pad->status().phase==PadPhase::Read&&pad->status().error==-2);++checks;
        assert(diag_count("pad_first_state_read")==0&&diag_count("pad_key_edge")==0);++checks;
        assert(total_fires()==baseline_fires_at_start);++checks;

        // POLARITY 2 - reads heal: the FIRST err-free AND connected read
        // emits pad_first_state_read=<raw buttons> (here the OPTIONS
        // baseline sample 0x8), exactly once, before the duplicate guard,
        // with NO edge swallowed or invented (baseline discipline intact).
        control.read_error=0;
        tick(handler);
        assert(control.reads==2);++checks;                 // failed read + this healed read
        assert(diag_count("pad_first_state_read")==1);++checks;
        assert(diag_has("pad_first_state_read",PS5_PAD_BUTTON_OPTIONS));++checks;
        assert(pad->status().phase==PadPhase::Active);++checks;
        assert(diag_count("pad_key_edge")==0);++checks;    // baseline: no edge, no tag
        assert(total_fires()==baseline_fires_at_start);++checks;
        // Duplicate timestamp: read happens, tag does not repeat.
        tick(handler);
        assert(control.reads==3&&diag_count("pad_first_state_read")==1);++checks;

        // (a) teardown: shutdown closes OUR handle once (owned discipline).
        assert(handler.shutdown()&&control.close_calls==1);++checks;
        // Handler.h deatil: shutdown deactivates the source; once-per-boot
        // state is per PROCESS, not per source - asserted by (b)/(c)/(e).
    }

    // ===== (b) G-AP1 dispatch half: six distinct edges, budget, silence =====
    {
        control={};control.user_initialize_result=0;control.open_error=0; // available at configure
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&pad->is_available());++checks;
        Handler handler;
        assert(handler.init(input));++checks;
        register_six(handler);++checks;
        assert(diag_count("pad_first_state_read")==1);++checks; // per-boot, NOT per source

        // Baseline first: release-all sample is swallowed (no edge).
        bump(0);tick(handler);
        assert(diag_count("pad_key_edge")==0&&total_fires()==baseline_fires_at_start);++checks;

        // Each of the six buttons: one press -> ONE tag with the RIGHT
        // distinct keycode and ONE callback fire; release -> nothing.
        struct Walk { unsigned bit; Key key; int* fired; };
        const Walk walk[]{
            {PS5_PAD_BUTTON_OPTIONS,Key::F7,&fires.f7},
            {PS5_PAD_BUTTON_CIRCLE,Key::RETURN,&fires.ret},
            {PS5_PAD_BUTTON_UP,Key::UP_ARROW,&fires.up},
            {PS5_PAD_BUTTON_DOWN,Key::DOWN_ARROW,&fires.down},
            {PS5_PAD_BUTTON_LEFT,Key::LEFT_ARROW,&fires.left},
            {PS5_PAD_BUTTON_RIGHT,Key::RIGHT_ARROW,&fires.right}};
        int edges=0;
        for(const auto& w:walk)
        {
            bump(w.bit);tick(handler);
            ++edges;
            assert(diag_count("pad_key_edge")==edges);++checks;      // exactly one new tag
            assert(diag_has("pad_key_edge",static_cast<unsigned int>(w.key)));++checks;
            assert(*w.fired==1);++checks;                            // callback fired (delivery intact)
            bump(0);tick(handler);                                   // release: no edge, no tag
            assert(diag_count("pad_key_edge")==edges);++checks;
        }
        assert(diag_count("pad_key_edge")==6);++checks;              // six DISTINCT codes seen
        assert(diag_has("pad_key_edge",0x76u)&&diag_has("pad_key_edge",0x0Du)&&
               diag_has("pad_key_edge",0x25u)&&diag_has("pad_key_edge",0x26u)&&
               diag_has("pad_key_edge",0x27u)&&diag_has("pad_key_edge",0x28u));++checks;

        // Budget: 54 more OPTIONS edges -> exactly 60 tags, then ONE
        // pad_key_edge_silent marker (value = the cap) and never again.
        for(int i=0;i<54;++i)
        {
            bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
            bump(0);tick(handler);
        }
        assert(diag_count("pad_key_edge")==kTagCap);++checks;
        assert(diag_count("pad_key_edge_silent")==1);++checks;
        assert(diag_has("pad_key_edge_silent",kTagCap));++checks;
        assert(fires.f7==55);++checks;                     // 1 (walk) + 54 (budget run)

        // THE NO-BEHAVIOR-CHANGE CORE: after the cap, presses still dispatch.
        for(int i=0;i<3;++i)
        {
            bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
            bump(0);tick(handler);
        }
        assert(fires.f7==58);++checks;                     // callbacks unaffected by silence
        assert(diag_count("pad_key_edge")==kTagCap);++checks;
        assert(diag_count("pad_key_edge_silent")==1);++checks;

        assert(handler.shutdown()&&control.close_calls==1);++checks;
    }

    // ===== (c) disconnect -> reconnect: one first-read tag per BOOT =====
    {
        control={};control.user_initialize_result=0;control.open_error=0;
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&pad->is_available());++checks;
        Handler handler;
        assert(handler.init(input));++checks;
        register_six(handler);++checks;
        bump(0);tick(handler);
        const int fires_before=total_fires();
        // Disconnect: phase moves, edges cleared, NO new first-read tag.
        control.connected=false;bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);
        assert(pad->status().phase==PadPhase::Disconnected);++checks;
        assert(diag_count("pad_first_state_read")==1);++checks;
        // Reconnect: first connected read after the gap - still ONE tag
        // (once-per-boot), baseline re-swallowed, delivery intact after.
        control.connected=true;bump(0);tick(handler);
        assert(pad->status().phase==PadPhase::Active);++checks;
        assert(diag_count("pad_first_state_read")==1);++checks;
        assert(diag_count("pad_key_edge")==kTagCap);++checks; // silence holds
        bump(PS5_PAD_BUTTON_CIRCLE);tick(handler);            // RETURN bind fires, no tag
        assert(fires.ret==2&&total_fires()==fires_before+1);++checks;
        assert(diag_count("pad_key_edge")==kTagCap);++checks;
        bump(0);tick(handler);
        assert(handler.shutdown()&&control.close_calls==1);++checks;
    }

    // ===== (d) allow_input=false: upstream swallow, G-AP3 silence =====
    {
        control={};control.user_initialize_result=0;control.open_error=0;
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        Handler handler;
        assert(handler.init(input));++checks;
        register_six(handler);++checks;
        bump(0);tick(handler);
        handler.set_allow_input(false);
        const int fires_before=total_fires();
        bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);       // press while disallowed
        bump(0);tick(handler);                            // release while disallowed
        assert(total_fires()==fires_before);++checks;     // no delivery (documented)
        assert(diag_count("pad_key_edge")==kTagCap);++checks; // no tag either: the
        // event never ARRIVES while allow=false (poll-side gate), so the
        // arrival tag correctly stays silent - the handler site cannot see
        // what the source never emits.
        handler.set_allow_input(true);
        bump(PS5_PAD_BUTTON_OPTIONS);tick(handler);       // fresh press: delivers
        assert(fires.f7==59);++checks;
        bump(0);tick(handler);
        assert(diag_count("pad_key_edge")==kTagCap);++checks; // still silent (per-boot)
        assert(handler.shutdown()&&control.close_calls==1);++checks;
    }

    // ===== (e) input-dead control: arrival tags NEVER move =====
    {
        control={};control.user_initialize_result=static_cast<int>(0x80960003u);
        control.open_error=kLiveOpenError;
        const int f_read=diag_count("pad_first_state_read");
        const int f_edge=diag_count("pad_key_edge");
        const int f_silent=diag_count("pad_key_edge_silent");
        const int f_avail=diag_count("pad_available");
        const int f_retry=diag_count("pad_retry");
        const int f_failed=diag_count("pad_retry_failed");
        const int reads_before=control.reads;
        const int fires_before=total_fires();
        std::shared_ptr<PlatformInputSource> input;
        modbridge::native::ConfigureRetainedPS5PadInput(files(config),input);
        auto pad=std::dynamic_pointer_cast<PS5PadSource>(input);
        assert(pad&&!pad->is_available());++checks;
        Handler handler;
        assert(handler.init(input));++checks;
        register_six(handler);++checks;
        // 1000 dead polls: attempts at polls 200 (interval 200) and 600
        // (interval 400) only - the admitted bounded backoff, no busy loop.
        for(int i=0;i<1000;++i) tick(handler);
        assert(control.pad_initialize_calls==1+2);++checks; // construction + 2 retries
        assert(diag_count("pad_retry")-f_retry==2);++checks;      // attempts at polls 200, 600
        assert(diag_count("pad_retry_failed")-f_failed==2);++checks;
        // THE INPUT-DEAD LOG SHAPE: no availability, no reads, no arrivals.
        assert(diag_count("pad_available")==f_avail);++checks;
        assert(diag_count("pad_first_state_read")==f_read);++checks;
        assert(diag_count("pad_key_edge")==f_edge);++checks;
        assert(diag_count("pad_key_edge_silent")==f_silent);++checks;
        assert(control.reads==reads_before);++checks;       // poll never reaches read_state
        assert(total_fires()==fires_before);++checks;
        // Borrowed discipline unchanged: no terminate, no close, ever.
        assert(control.user_terminate_calls==0&&control.close_calls==0);++checks;
        recorded_source.reset();                            // borrowed source drop
        assert(control.user_terminate_calls==0&&control.close_calls==0);++checks;
    }

    std::printf("%d input-tag host checks passed (STAGED PS5PadSource + Handler real; admitted coexist "
                "NativePadInput; owned backend; pad_first_state_read once-per-boot both polarities; six "
                "distinct pad_key_edge codes 0x76/0x0D/0x25-0x28; exactly 60 tags then one "
                "pad_key_edge_silent; callbacks keep firing after the cap; no SDK, hardware, Lua mod or "
                "engine execution)\n",checks);
    return 0;
}
