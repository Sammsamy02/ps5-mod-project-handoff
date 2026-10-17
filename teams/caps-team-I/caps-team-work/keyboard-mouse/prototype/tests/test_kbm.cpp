// Team I2 host tests (Mac-only). Built by ../run.sh against the REAL R16 input
// code, read-only from the source pins:
//   Handler.cpp      worker-pad-coexist/staging/input-handler-coexist (R16 unit "input-handler")
//   KeyDef.cpp/.hpp  native-coherent-runtime-1 port (R16 unit "input-keydef")
//   PS5PadSource.cpp l22-runtime/candidate (R16 unit "native-pad-PS5PadSource")
// with a fake pad backend and a fake engine reader. Nothing here touches a console.
#include <kbm/CompositeInputSource.hpp>
#include <kbm/EngineKeySource.hpp>
#include <kbm/KeyboardMouseMap.hpp>

#include <Input/Handler.hpp>
#include <PS5PadSource.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace modbridge::kbm;
using RC::Input::Handler;
using RC::Input::Key;
using RC::Input::ModifierKey;

// ---- the composition's in-link diagnostic sink (PS5PadSource/Handler call it) ----
static int g_diag_count = 0;
extern "C" void diagnostic_event(const char*, unsigned int) { ++g_diag_count; }

// ---- tiny test framework ----
static int g_checks = 0, g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        ++g_checks;                                                                                                    \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            ++g_failures;                                                                                              \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                             \
        }                                                                                                              \
    } while (0)
struct TestCase
{
    const char* name;
    void (*fn)();
};
static std::vector<TestCase>& tests()
{
    static std::vector<TestCase> all;
    return all;
}
struct Register
{
    Register(const char* n, void (*f)()) { tests().push_back({n, f}); }
};
#define TEST(name)                                                                                                     \
    static void name();                                                                                                \
    static Register reg_##name(#name, name);                                                                           \
    static void name()

// ---- fake engine (what the real reader would get from IsInputKeyDown) ----
struct FakeEngine
{
    std::map<std::string, bool> down;
    std::set<std::string> unknown; // names this "game" does not have
    KeyStateReader::Frame status = KeyStateReader::Frame::Ready;
    uint64_t generation = 1;
    int begin_calls = 0, read_calls = 0;
    uint64_t* clock = nullptr;
    uint64_t cost_per_read_ns = 0;
};
class FakeReader final : public KeyStateReader
{
  public:
    explicit FakeReader(FakeEngine& engine) : m_engine(engine) {}
    FrameInfo begin_frame() override
    {
        ++m_engine.begin_calls;
        return {m_engine.status, m_engine.generation};
    }
    std::optional<bool> is_down(const char* name) override
    {
        ++m_engine.read_calls;
        if (m_engine.clock) *m_engine.clock += m_engine.cost_per_read_ns;
        if (m_engine.unknown.count(name)) return std::nullopt;
        auto it = m_engine.down.find(name);
        return it != m_engine.down.end() && it->second;
    }

  private:
    FakeEngine& m_engine;
};

// ---- fake pad backend for the REAL PS5PadSource ----
namespace fakepad
{
    int open_result = 7;
    uint32_t buttons = 0;
    uint64_t timestamp = 0;
    void reset()
    {
        open_result = 7;
        buttons = 0;
        timestamp = 0;
    }
    int user_initialize(void*) { return 0; }
    int pad_initialize() { return 0; }
    int user_list(int users[4])
    {
        users[0] = 1;
        users[1] = users[2] = users[3] = -1;
        return 0;
    }
    int open(int, int, int, void*) { return open_result; }
    int read_state(int, PS5_PadData* data)
    {
        std::memset(data, 0, sizeof(*data));
        data->buttons = buttons;
        data->connected = 1;
        data->timestamp = ++timestamp;
        return 0;
    }
    int close(int) { return 0; }
    int user_terminate() { return 0; }
    RC::Input::PadBackend backend() { return {user_initialize, pad_initialize, user_list, open, read_state, close, user_terminate}; }
} // namespace fakepad

static std::shared_ptr<RC::Input::PS5PadSource> make_pad()
{
    // Today's console-proven rows: L3+R3 = F7, CROSS = RETURN.
    std::vector<RC::Input::PadBinding> rows{{PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3, Key::F7, {}},
                                            {PS5_PAD_BUTTON_CROSS, Key::RETURN, {}}};
    auto pad = std::make_shared<RC::Input::PS5PadSource>(rows, fakepad::backend(), -1);
    pad->activate();
    return pad;
}

static Handler::ModifierKeyArray mods_of(std::initializer_list<ModifierKey> list)
{
    Handler::ModifierKeyArray out{};
    size_t i = 0;
    for (auto m : list) out[i++] = m;
    return out;
}

// One test "world": real Handler + composite(pad?, engine keys).
struct World
{
    FakeEngine engine;
    uint64_t clock = 0;
    std::shared_ptr<EngineKeyChannel> channel = std::make_shared<EngineKeyChannel>();
    std::unique_ptr<EngineKeySampler> sampler;
    std::shared_ptr<EngineKeySource> keys;
    std::shared_ptr<RC::Input::PS5PadSource> pad;
    std::shared_ptr<CompositeInputSource> merged;
    Handler handler;
    std::vector<std::string> fired;

    explicit World(bool with_pad, bool with_keys = true, SamplerConfig cfg = {})
    {
        std::vector<std::shared_ptr<RC::Input::PlatformInputSource>> children;
        if (with_pad)
        {
            pad = make_pad();
            children.push_back(pad);
        }
        if (with_keys)
        {
            engine.clock = &clock;
            sampler = std::make_unique<EngineKeySampler>(channel, std::make_unique<FakeReader>(engine), cfg,
                                                         [this] { return clock; });
            keys = std::make_shared<EngineKeySource>(channel);
            children.push_back(keys);
        }
        merged = std::make_shared<CompositeInputSource>(children);
        CHECK(handler.init(merged)); // deferred attach is fine (startup never aborts)
    }
    void bind(Key key, const char* label)
    {
        handler.register_keydown_event(key, [this, label] { fired.push_back(label); });
    }
    void bind(Key key, std::initializer_list<ModifierKey> mods, const char* label)
    {
        handler.register_keydown_event(key, mods_of(mods), [this, label] { fired.push_back(label); });
    }
    // One game frame (sampler on the "game thread") + one update-loop pass.
    void frame()
    {
        if (sampler) sampler->tick();
        handler.process_event();
    }
};

// =====================================================================
// Mapping table
// =====================================================================
TEST(table_rows_unique_and_named)
{
    std::set<int> keys, hids;
    std::set<std::string> fkeys;
    size_t with_fkey = 0, with_hid = 0;
    for (const auto& row : key_table())
    {
        CHECK(keys.insert(row.key).second);
        CHECK(row.name && *row.name);
        if (row.fkey)
        {
            ++with_fkey;
            CHECK(fkeys.insert(row.fkey).second);
        }
        if (row.hid)
        {
            ++with_hid;
            CHECK(hids.insert(row.hid).second);
            for (const auto& mod : modifier_table()) // SHIFT/CONTROL/ALT usages are modifiers, never keys
                CHECK(row.hid != mod.hid_left && row.hid != mod.hid_right);
        }
        // Real runtime spelling round trip (KeyDef.cpp string_to_key).
        RC::StringType wide(row.name, row.name + std::strlen(row.name));
        CHECK(RC::Input::string_to_key(wide) == row.key);
    }
    for (const auto& alias : hid_aliases()) CHECK(!hids.count(alias.hid));
    for (const auto& mod : modifier_table())
    {
        CHECK(!fkeys.count(mod.fkey_left) && !fkeys.count(mod.fkey_right));
        CHECK(mod.hid_left >= 0xE0 && mod.hid_right >= 0xE0);
    }
    std::printf("  rows=%zu engine-named=%zu hid=%zu aliases=%zu modifiers=%zu\n", key_table().size(), with_fkey, with_hid,
                hid_aliases().size(), modifier_table().size());
    CHECK(key_table().size() > 150);
}

TEST(table_lookups)
{
    CHECK(fkey_for(Key::F7) && std::strcmp(fkey_for(Key::F7), "F7") == 0);
    CHECK(key_for_fkey("SpaceBar") == Key::SPACE);
    CHECK(key_for_fkey("LeftMouseButton") == Key::LEFT_MOUSE_BUTTON);
    CHECK(key_for_fkey("NumPadThree") == Key::NUM_THREE);
    CHECK(!key_for_fkey("NoSuchKey"));
    CHECK(key_for_hid(0x40) == Key::F7);     // HID F7
    CHECK(key_for_hid(0x04) == Key::A);      // HID a
    CHECK(key_for_hid(0x58) == Key::RETURN); // keypad Enter alias
    CHECK(!key_for_hid(0));
    CHECK(!key_for_hid(0xE1)); // a modifier is never a key
    CHECK(fkey_for(Key::F13) == nullptr);  // engine has F1-F12 only
    CHECK(row_for(static_cast<Key>(0x07)) == nullptr); // undefined value
}

// =====================================================================
// Engine-keys sampler + source
// =====================================================================
TEST(key_down_fires_once_no_repeat)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame(); // publishes subscriptions; baseline frame
    w.frame();
    w.engine.down["F8"] = true;
    for (int i = 0; i < 5; ++i) w.frame(); // held: one event only
    w.engine.down["F8"] = false;
    w.frame();
    w.engine.down["F8"] = true;
    w.frame();
    CHECK(w.fired.size() == 2);
    CHECK(w.channel->events.load() == 2);
}

TEST(held_at_start_is_not_a_press)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.engine.down["F8"] = true; // already held when sampling starts
    for (int i = 0; i < 4; ++i) w.frame();
    CHECK(w.fired.empty());
    w.engine.down["F8"] = false;
    w.frame();
    w.engine.down["F8"] = true;
    w.frame();
    CHECK(w.fired.size() == 1);
}

TEST(controller_change_or_loading_resets_baseline)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    // Level load: no controller for a while, then a NEW controller with F8 held.
    w.engine.status = KeyStateReader::Frame::NoController;
    w.frame();
    w.engine.status = KeyStateReader::Frame::Ready;
    w.engine.generation = 2;
    w.engine.down["F8"] = true;
    w.frame();
    w.frame();
    CHECK(w.fired.empty()); // no phantom press
    w.engine.down["F8"] = false;
    w.frame();
    w.engine.down["F8"] = true;
    w.frame();
    CHECK(w.fired.size() == 1);
}

TEST(only_bound_keys_are_asked)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    const int before = w.engine.read_calls;
    w.engine.down["F9"] = true; // not bound: never asked, never fires
    w.frame();
    w.frame();
    const int per_frame = (w.engine.read_calls - before) / 2;
    CHECK(per_frame == 1 + 6); // F8 + 3 modifiers x left/right
    CHECK(w.fired.empty());
}

TEST(modifiers_exact_match_like_pc_ue4ss)
{
    World w(false);
    w.bind(Key::F7, "F7");
    w.bind(Key::F7, {ModifierKey::SHIFT}, "SHIFT+F7");
    w.frame();
    w.frame();
    w.engine.down["RightShift"] = true;
    w.engine.down["F7"] = true;
    w.frame();
    CHECK(w.fired.size() == 1 && w.fired[0] == "SHIFT+F7");
    w.engine.down["F7"] = false;
    w.engine.down["RightShift"] = false;
    w.frame();
    w.engine.down["F7"] = true;
    w.frame();
    CHECK(w.fired.size() == 2 && w.fired[1] == "F7");
}

TEST(no_keyboard_plugged_in)
{
    World w(true);
    w.bind(Key::F8, "F8");
    w.bind(Key::F7, "F7");
    for (int i = 0; i < 50; ++i) w.frame(); // engine answers "up" for every key
    CHECK(w.fired.empty());
    CHECK(w.keys->is_available()); // the route works, there is just no keyboard
    CHECK(w.channel->read_failures.load() == 0);
    CHECK(w.channel->events.load() == 0);
    // The controller still works exactly as today.
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3;
    w.frame();
    fakepad::buttons = 0;
    w.frame();
    CHECK(w.fired.size() == 1 && w.fired[0] == "F7");
    fakepad::reset();
}

TEST(unsupported_engine_fails_closed_pad_continues)
{
    World w(true);
    w.bind(Key::F7, "F7");
    w.engine.status = KeyStateReader::Frame::Unsupported;
    w.frame();
    const int reads = w.engine.read_calls, begins = w.engine.begin_calls;
    for (int i = 0; i < 20; ++i) w.frame();
    CHECK(w.channel->support.load() == -1);
    CHECK(!w.keys->is_available());
    CHECK(w.engine.begin_calls == begins && w.engine.read_calls == reads); // stopped for good
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3;
    w.frame();
    CHECK(w.fired.size() == 1);
    fakepad::reset();
}

TEST(unknown_key_name_in_this_game_is_skipped)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.bind(Key::F9, "F9");
    w.engine.unknown.insert("F9");
    w.frame();
    w.frame();
    w.engine.down["F8"] = true;
    w.frame();
    CHECK(w.fired.size() == 1 && w.fired[0] == "F8");
    CHECK(w.channel->read_failures.load() > 0);
}

TEST(controller_events_identical_with_and_without_keyboard_route)
{
    // Same pad script, run (A) pad-only exactly like today, (B) merged with the
    // keyboard route. The controller-driven callbacks must be identical.
    const uint32_t script[] = {0,
                               PS5_PAD_BUTTON_L3,
                               PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3,
                               PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3,
                               0,
                               PS5_PAD_BUTTON_CROSS,
                               PS5_PAD_BUTTON_CROSS,
                               0,
                               PS5_PAD_BUTTON_R3,
                               PS5_PAD_BUTTON_R3 | PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_CROSS,
                               0};
    auto run = [&](bool with_keys) {
        fakepad::reset();
        World w(true, with_keys);
        w.bind(Key::F7, "F7");
        w.bind(Key::RETURN, "RETURN");
        std::vector<std::string> log;
        int frame = 0;
        for (uint32_t buttons : script)
        {
            fakepad::buttons = buttons;
            w.frame();
            for (auto& f : w.fired) log.push_back(std::to_string(frame) + ":" + f);
            w.fired.clear();
            ++frame;
        }
        return log;
    };
    const auto pad_only = run(false);
    const auto merged = run(true);
    CHECK(!pad_only.empty());
    CHECK(pad_only == merged);
    fakepad::reset();
}

TEST(pad_chord_and_real_key_same_poll_fire_once)
{
    World w(true);
    w.bind(Key::F7, "F7");
    w.frame();
    w.frame();
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3;
    w.engine.down["F7"] = true;
    w.frame();
    CHECK(w.fired.size() == 1);
    CHECK(w.merged->duplicates_merged() == 1);
    fakepad::reset();
}

TEST(mouse_buttons_bindable)
{
    World w(false);
    w.bind(Key::RIGHT_MOUSE_BUTTON, "RMB");
    w.frame();
    w.frame();
    w.engine.down["RightMouseButton"] = true;
    w.frame();
    CHECK(w.fired.size() == 1);
}

TEST(queue_overflow_drops_counted_no_crash)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    // 300 presses (600 frames) before the update loop drains (it normally drains every ~5 ms).
    for (int i = 0; i < 600; ++i)
    {
        w.engine.down["F8"] = (i % 2 == 0);
        w.sampler->tick();
    }
    w.handler.process_event();
    CHECK(w.fired.size() == EngineKeyChannel::ring_slots - 1);
    CHECK(w.channel->drops.load() == 300 - (EngineKeyChannel::ring_slots - 1));
}

TEST(over_budget_slows_down_and_caps)
{
    SamplerConfig cfg;
    cfg.budget_ns = 250'000;
    World w(false, true, cfg);
    w.engine.cost_per_read_ns = 100'000; // 7 reads = 0.7 ms per sampled frame (too slow)
    w.bind(Key::F8, "F8");
    for (int i = 0; i < 400; ++i) w.frame();
    CHECK(w.channel->divisor.load() == 8);      // stepped 1 -> 2 -> 4 -> 8, then capped
    CHECK(w.channel->throttle_steps.load() == 3);
    // A press held for 8+ frames is still seen at the slowest rate.
    w.engine.down["F8"] = true;
    for (int i = 0; i < 9; ++i) w.frame();
    CHECK(w.fired.size() == 1);
}

TEST(round_robin_above_key_cap)
{
    SamplerConfig cfg;
    cfg.max_keys_per_frame = 4;
    World w(false, true, cfg);
    const Key ks[] = {Key::F1, Key::F2, Key::F3, Key::F4, Key::F5, Key::F6, Key::F8, Key::F9, Key::F10, Key::F11};
    for (Key k : ks) w.bind(k, "F");
    w.frame();
    for (int i = 0; i < 6; ++i) w.frame(); // every key gets a baseline
    for (const char* n : {"F1", "F2", "F3", "F4", "F5", "F6", "F8", "F9", "F10", "F11"}) w.engine.down[n] = true;
    for (int i = 0; i < 6; ++i) w.frame();
    CHECK(w.fired.size() == 10);
}

TEST(allow_input_false_delivers_nothing)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    w.handler.set_allow_input(false);
    w.engine.down["F8"] = true;
    w.frame();
    CHECK(w.fired.empty());
}

TEST(deactivate_stops_sampling)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    CHECK(w.handler.shutdown());
    const int begins = w.engine.begin_calls;
    w.engine.down["F8"] = true;
    for (int i = 0; i < 5; ++i) w.sampler->tick();
    CHECK(w.engine.begin_calls == begins);
    CHECK(!w.keys->is_available());
}

TEST(deferred_attach_pad_held_by_game)
{
    // module_start case: the game holds the pad (ALREADY_OPENED) and the engine
    // has not answered yet -> nothing available. Startup must not abort, binds
    // must register, and real keys must work once the engine answers.
    fakepad::reset();
    fakepad::open_result = static_cast<int>(PS5_PAD_ERROR_ALREADY_OPENED);
    World w(true);
    CHECK(!w.merged->is_available());
    w.bind(Key::F8, "F8"); // must not throw
    w.frame();
    w.frame();
    w.engine.down["F8"] = true;
    w.frame();
    CHECK(w.fired.size() == 1);
    fakepad::reset();
}

namespace
{
    class ThrowingSource final : public RC::Input::PlatformInputSource
    {
      public:
        bool is_available() override { return true; }
        bool activate() override { return true; }
        bool deactivate() override { return true; }
        std::vector<RC::Input::InputEvent>& process_event(Handler*) override { throw std::runtime_error("broken child"); }
        const char* get_name() override { return "Throwing"; }
    };
} // namespace

TEST(broken_child_never_silences_the_others)
{
    auto channel = std::make_shared<EngineKeyChannel>();
    FakeEngine engine;
    EngineKeySampler sampler(channel, std::make_unique<FakeReader>(engine));
    auto merged = std::make_shared<CompositeInputSource>(std::vector<std::shared_ptr<RC::Input::PlatformInputSource>>{
            std::make_shared<ThrowingSource>(), std::make_shared<EngineKeySource>(channel)});
    Handler handler;
    CHECK(handler.init(merged));
    int fired = 0;
    handler.register_keydown_event(Key::F8, [&] { ++fired; });
    for (int i = 0; i < 2; ++i)
    {
        sampler.tick();
        handler.process_event();
    }
    engine.down["F8"] = true;
    sampler.tick();
    handler.process_event();
    CHECK(fired == 1);
    CHECK(merged->child_exceptions() == 3);
}

TEST(no_reader_is_unsupported)
{
    auto channel = std::make_shared<EngineKeyChannel>();
    EngineKeySampler sampler(channel, nullptr);
    sampler.tick();
    CHECK(channel->support.load() == -1);
}

static void dump_table(const char* path)
{
    std::ofstream out(path);
    out << "ue4ss_key\tvalue\tengine_key\thid_usage\tnote\n";
    for (const auto& row : key_table())
    {
        char value[8], hid[8];
        std::snprintf(value, sizeof value, "0x%02X", static_cast<unsigned>(row.key));
        if (row.hid)
            std::snprintf(hid, sizeof hid, "0x%02X", row.hid);
        else
            std::snprintf(hid, sizeof hid, "-");
        out << row.name << '\t' << value << '\t' << (row.fkey ? row.fkey : "-") << '\t' << hid << '\t'
            << (row.note ? row.note : "") << '\n';
    }
    for (const auto& mod : modifier_table())
    {
        char hl[8], hr[8];
        std::snprintf(hl, sizeof hl, "0x%02X", mod.hid_left);
        std::snprintf(hr, sizeof hr, "0x%02X", mod.hid_right);
        out << "MODIFIER:" << (mod.modifier == ModifierKey::SHIFT ? "SHIFT" : mod.modifier == ModifierKey::CONTROL ? "CONTROL" : "ALT")
            << "\t-\t" << mod.fkey_left << '|' << mod.fkey_right << '\t' << hl << '|' << hr << "\tmodifier (left or right)\n";
    }
    for (const auto& alias : hid_aliases())
    {
        char hid[8];
        std::snprintf(hid, sizeof hid, "0x%02X", alias.hid);
        out << "ALIAS:" << row_for(alias.key)->name << "\t-\t-\t" << hid << '\t' << alias.note << '\n';
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--dump-table") == 0) dump_table(argv[i + 1]);
    for (const auto& t : tests())
    {
        const int before = g_failures;
        t.fn();
        std::printf("%s %s\n", g_failures == before ? "PASS" : "FAIL", t.name);
    }
    std::printf("tests=%zu checks=%d failures=%d\n", tests().size(), g_checks, g_failures);
    if (tests().empty() || g_checks == 0) return 3; // a test run that checks nothing is a failure
    return g_failures ? 1 : 0;
}
