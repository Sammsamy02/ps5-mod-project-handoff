// K2 host tests (Mac-only, Team I2). Built by ./run.sh against the SHIPPING header
//   new/include/Platform/KbmEngineKeys.hpp   (the exact file the runtime includes)
// and the REAL R16 input code, read-only from the source pins:
//   Handler.cpp      worker-pad-coexist/staging/input-handler-coexist (unit "input-handler")
//   KeyDef.cpp/.hpp  native-coherent-runtime-1 port (unit "input-keydef")
//   PS5PadSource.cpp l22-runtime/candidate (unit "native-pad-PS5PadSource")
// with a fake pad backend, a fake key-state reader and a fake REFLECTION model (for the reader's checks).
// Nothing here touches a console. A run that executes no test or no check fails.
#include <Platform/KbmEngineKeys.hpp>

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

using namespace RC::Platform::Kbm;
using RC::Input::Handler;
using RC::Input::Key;
using RC::Input::ModifierKey;
using Field = KeyReflection::Field;
using Kind = KeyReflection::Kind;
using Frame = KeyStateReader::Frame;

// ---- the composition's in-link diagnostic sink (PS5PadSource / Handler call it) ----
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

static constexpr uint64_t k_frame_ns = 16'700'000; // one 60 Hz engine frame

// =====================================================================================================================
// Fake key-state reader (sampler-level tests)
// =====================================================================================================================
struct FakeEngine
{
    std::map<std::string, bool> down;
    std::set<std::string> unknown; // names this "game" does not have
    Frame status = Frame::Ready;
    uint64_t generation = 1;
    int begin_calls = 0, read_calls = 0;
    uint64_t* clock = nullptr;
    uint64_t cost_per_read_ns = 0;
    bool throw_on_read = false;
    const char* reason = "layout:return";
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
    std::optional<bool> is_down(uint16_t, const char* name) override
    {
        ++m_engine.read_calls;
        if (m_engine.throw_on_read) throw std::runtime_error("engine read exploded");
        if (m_engine.clock) *m_engine.clock += m_engine.cost_per_read_ns;
        if (m_engine.unknown.count(name)) return std::nullopt;
        auto it = m_engine.down.find(name);
        return it != m_engine.down.end() && it->second;
    }
    const char* unsupported_reason() const override { return m_engine.reason; }

  private:
    FakeEngine& m_engine;
};

// =====================================================================================================================
// Fake pad backend for the REAL PS5PadSource
// =====================================================================================================================
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
    std::vector<RC::Input::PadBinding> rows{{PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3, Key::F7, {}}, {PS5_PAD_BUTTON_CROSS, Key::RETURN, {}}};
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

// One test "world": real Handler + the runtime's own composition helper (Merged{pad?, engine keys}).
struct World
{
    FakeEngine engine;
    uint64_t clock = 1'000'000'000;
    std::shared_ptr<EngineKeyChannel> channel = std::make_shared<EngineKeyChannel>();
    std::unique_ptr<EngineKeySampler> sampler;
    std::shared_ptr<RC::Input::PS5PadSource> pad;
    std::shared_ptr<CompositeInputSource> merged;
    Handler handler;
    std::vector<std::string> fired;
    std::vector<std::string> log;

    explicit World(bool with_pad, bool with_keys = true, SamplerConfig cfg = {}, std::unique_ptr<KeyStateReader> reader = nullptr)
    {
        if (with_pad) pad = make_pad();
        if (with_keys)
        {
            engine.clock = &clock;
            if (!reader) reader = std::make_unique<FakeReader>(engine);
            sampler = std::make_unique<EngineKeySampler>(channel, std::move(reader), cfg, [this] { return clock; },
                                                         [this](const char* line) { log.emplace_back(line); });
            merged = make_merged_input(pad, channel);
        }
        else
        {
            merged = std::make_shared<CompositeInputSource>(std::vector<std::shared_ptr<RC::Input::PlatformInputSource>>{pad});
        }
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
    void tick()
    {
        clock += k_frame_ns;
        if (sampler) sampler->tick();
    }
    // One game frame (sampler on the "game thread") + one update-loop pass.
    void frame()
    {
        tick();
        handler.process_event();
    }
};

// =====================================================================================================================
// Fake reflection model (reader-level tests): a tiny "engine" with a name table, objects and a reflected layout.
// The fake engine's call() reads the key name and writes the bool at ITS OWN layout, like ProcessEvent would.
// =====================================================================================================================
static int g_pc1, g_pc2, g_pc3, g_cls1, g_cls2, g_fn, g_struct;

struct FakeGame
{
    void* controller = &g_pc1; // what find_local_controller returns (nullptr = none)
    std::set<void*> live{&g_pc1, &g_pc2, &g_pc3};
    std::map<void*, void*> classes{{&g_pc1, &g_cls1}, {&g_pc2, &g_cls1}, {&g_pc3, &g_cls2}};
    std::map<void*, int32_t> indices{{&g_pc1, 10}, {&g_pc2, 11}, {&g_pc3, 12}};
    int freed_object_reads = 0;  // object_index / class_of on an object that is no longer live (= reading freed memory)
    uint64_t probe_cost_ms = 0;  // how long one controller search takes
    std::vector<uint64_t> probe_times; // when each controller search started
    int frame_violations = 0;    // engine call touched bytes at or past ParmsSize
    bool has_function = true;
    bool layout_readable = true;
    int32_t parms_size = 32, return_offset = 24;
    std::vector<Field> params;
    std::string struct_name = "Key";
    std::vector<Field> members;
    size_t name_size = 8;
    std::vector<std::string> names{"IsInputKeyDown", "F7", "F8", "NumPadFive", "RightMouseButton",
                                   "LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt"};
    bool self_check_ok = true;
    std::map<std::string, bool> down;
    bool write_key_param = false;
    uint64_t now = 10'000;
    int find_controller_calls = 0, find_function_calls = 0, find_name_calls = 0, calls = 0;
    std::map<std::string, int> name_lookups;
};

static Field fld(Kind kind, int32_t offset, int32_t size, bool parm = true, bool ret = false)
{
    Field f{};
    f.kind = kind;
    f.parm = parm;
    f.ret = ret;
    f.offset = offset;
    f.size = size;
    return f;
}

// UE5-style IsInputKeyDown(FKey Key) const: Key (24 bytes: FName KeyName + unreflected TSharedPtr) then bool return.
static void ue5_layout(FakeGame& g)
{
    Field key = fld(Kind::Struct, 0, 24);
    key.struct_type = &g_struct;
    Field rv = fld(Kind::Bool, 24, 1, true, true);
    rv.out = true; // UE flags the return value CPF_OutParm | CPF_ReturnParm
    rv.bool_byte_offset = 0;
    rv.bool_field_mask = 0xFF;
    g.params = {key, rv};
    g.parms_size = 32;
    g.return_offset = 24;
    Field keyname = fld(Kind::Name, 0, 8, false);
    keyname.name_is_keyname = true;
    g.members = {keyname};
    g.name_size = 8;
}

// A deliberately different layout: return value first, bitfield-style bool, KeyName not at the struct start.
// The reader must use ONLY these reflected numbers.
static void other_layout(FakeGame& g)
{
    Field rv = fld(Kind::Bool, 0, 1, true, true);
    rv.out = true;
    rv.bool_byte_offset = 0;
    rv.bool_field_mask = 0x04;
    Field key = fld(Kind::Struct, 8, 32);
    key.struct_type = &g_struct;
    g.params = {rv, key};
    g.parms_size = 40;
    g.return_offset = 0;
    Field other = fld(Kind::Other, 0, 8, false);
    Field keyname = fld(Kind::Name, 16, 8, false);
    keyname.name_is_keyname = true;
    g.members = {other, keyname};
    g.name_size = 8;
}

class FakeReflection final : public KeyReflection
{
  public:
    explicit FakeReflection(FakeGame& g) : g(g) {}
    Handle find_local_controller() override
    {
        ++g.find_controller_calls;
        g.probe_times.push_back(g.now);
        g.now += g.probe_cost_ms;
        return g.controller;
    }
    int32_t object_index(Handle o) override
    {
        if (!g.live.count(o)) ++g.freed_object_reads;
        auto it = g.indices.find(o);
        return it == g.indices.end() ? -1 : it->second;
    }
    bool is_live_at(Handle o, int32_t index) override
    {
        auto it = g.indices.find(o);
        return g.live.count(o) != 0 && it != g.indices.end() && it->second == index;
    }
    Handle class_of(Handle o) override
    {
        if (!g.live.count(o)) ++g.freed_object_reads;
        auto it = g.classes.find(o);
        return it == g.classes.end() ? nullptr : it->second;
    }
    Handle find_function(Handle, const char* name) override
    {
        ++g.find_function_calls;
        return g.has_function && std::string(name) == "IsInputKeyDown" ? &g_fn : nullptr;
    }
    bool function_layout(Handle, int32_t& parms_size, int32_t& return_offset, std::vector<Field>& params) override
    {
        if (!g.layout_readable) return false;
        parms_size = g.parms_size;
        return_offset = g.return_offset;
        for (const auto& f : g.params) params.push_back(f);
        return true;
    }
    bool struct_is_named(Handle st, const char* name) override { return st == &g_struct && g.struct_name == name; }
    bool struct_members(Handle, std::vector<Field>& members) override
    {
        for (const auto& f : g.members) members.push_back(f);
        return true;
    }
    size_t name_size() override { return g.name_size; }
    bool find_name(const char* ascii, uint8_t* out, size_t cap) override
    {
        ++g.find_name_calls;
        ++g.name_lookups[ascii];
        for (size_t i = 0; i < g.names.size(); ++i)
        {
            if (g.names[i] != ascii) continue;
            if (cap < g.name_size || g.name_size < 4) return false;
            std::memset(out, 0, g.name_size);
            const uint32_t id = static_cast<uint32_t>(i + 1);
            std::memcpy(out, &id, sizeof id);
            return true;
        }
        return false; // find-only: a miss never adds a name
    }
    bool name_matches_function(Handle fn, const uint8_t* bytes) override
    {
        uint32_t id{};
        std::memcpy(&id, bytes, sizeof id);
        return g.self_check_ok && fn == &g_fn && id == 1;
    }
    void call(Handle, Handle, void* frame) override
    {
        ++g.calls;
        auto* f = static_cast<uint8_t*>(frame);
        const Field *key = nullptr, *ret = nullptr, *name = nullptr;
        for (const auto& p : g.params)
            if (p.parm) (p.ret ? ret : key) = &p;
        for (const auto& m : g.members)
            if (m.name_is_keyname) name = &m;
        // like the engine: everything it touches must be inside ParmsSize
        if (key->offset + key->size > g.parms_size || ret->offset + ret->bool_byte_offset >= g.parms_size ||
            key->offset + name->offset + static_cast<int32_t>(g.name_size) > g.parms_size)
            ++g.frame_violations;
        uint32_t id{};
        std::memcpy(&id, f + key->offset + name->offset, sizeof id);
        const std::string n = id >= 1 && id <= g.names.size() ? g.names[id - 1] : std::string{};
        const bool d = g.down.count(n) && g.down[n];
        uint8_t& b = f[ret->offset + ret->bool_byte_offset];
        b = d ? static_cast<uint8_t>(b | ret->bool_field_mask) : static_cast<uint8_t>(b & ~ret->bool_field_mask);
        if (g.write_key_param) f[key->offset + key->size - 1] = 0x5A; // a by-ref engine would fill KeyDetails
    }
    uint64_t now_ms() override { return g.now; }

  private:
    FakeGame& g;
};

static std::unique_ptr<EngineKeyReader> make_reader(FakeGame& g) { return std::make_unique<EngineKeyReader>(std::make_unique<FakeReflection>(g)); }

static const char* refusal(FakeGame& g)
{
    auto reader = make_reader(g);
    const auto info = reader->begin_frame();
    return info.status == Frame::Unsupported ? reader->unsupported_reason() : "ready";
}

// =====================================================================================================================
// Mapping table (same checks as the prototype, on the shipping header)
// =====================================================================================================================
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
            for (const auto& mod : modifier_table()) CHECK(row.hid != mod.hid_left && row.hid != mod.hid_right);
        }
        RC::StringType wide(row.name, row.name + std::strlen(row.name));
        CHECK(RC::Input::string_to_key(wide) == row.key); // real runtime spelling round trip (KeyDef.cpp)
    }
    for (const auto& alias : hid_aliases()) CHECK(!hids.count(alias.hid));
    for (const auto& mod : modifier_table())
    {
        CHECK(!fkeys.count(mod.fkey_left) && !fkeys.count(mod.fkey_right));
        CHECK(mod.hid_left >= 0xE0 && mod.hid_right >= 0xE0);
    }
    std::printf("  rows=%zu engine-named=%zu hid=%zu aliases=%zu modifiers=%zu name-ids=%u\n", key_table().size(), with_fkey, with_hid,
                hid_aliases().size(), modifier_table().size(), static_cast<unsigned>(k_name_id_count));
    CHECK(key_table().size() == 163);
    CHECK(with_fkey == 100);
}

TEST(table_lookups)
{
    CHECK(fkey_for(Key::F7) && std::strcmp(fkey_for(Key::F7), "F7") == 0);
    CHECK(key_for_fkey("SpaceBar") == Key::SPACE);
    CHECK(key_for_fkey("LeftMouseButton") == Key::LEFT_MOUSE_BUTTON);
    CHECK(key_for_fkey("NumPadThree") == Key::NUM_THREE);
    CHECK(!key_for_fkey("NoSuchKey"));
    CHECK(key_for_hid(0x40) == Key::F7);
    CHECK(key_for_hid(0x04) == Key::A);
    CHECK(key_for_hid(0x58) == Key::RETURN);
    CHECK(!key_for_hid(0));
    CHECK(!key_for_hid(0xE1));
    CHECK(fkey_for(Key::F13) == nullptr);
    CHECK(row_for(static_cast<Key>(0x07)) == nullptr);
    for (const auto& row : key_table()) CHECK(static_cast<unsigned>(row.key) < k_modifier_id_base); // key ids never hit modifier ids
}

// =====================================================================================================================
// Sampler + source (ported prototype tests)
// =====================================================================================================================
TEST(key_down_fires_once_no_repeat)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    w.engine.down["F8"] = true;
    for (int i = 0; i < 5; ++i) w.frame();
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
    w.engine.down["F8"] = true;
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
    w.engine.status = Frame::NoController;
    w.frame();
    w.engine.status = Frame::Ready;
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

TEST(only_bound_keys_are_asked_modifiers_only_on_edges)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    int before = w.engine.read_calls;
    w.engine.down["F9"] = true; // not bound: never asked, never fires
    w.frame();
    w.frame();
    CHECK(w.engine.read_calls - before == 2); // steady state: F8 only, no modifier reads
    CHECK(w.fired.empty());
    before = w.engine.read_calls;
    w.engine.down["F8"] = true; // edge frame: F8 + 3 modifiers x left/right
    w.frame();
    CHECK(w.engine.read_calls - before == 1 + 6);
    CHECK(w.fired.size() == 1);
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
    for (int i = 0; i < 50; ++i) w.frame();
    CHECK(w.fired.empty());
    CHECK(w.merged->is_available());
    CHECK(w.channel->read_failures.load() == 0);
    CHECK(w.channel->events.load() == 0);
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
    w.engine.status = Frame::Unsupported;
    w.frame(); // publishes the binds (no engine call yet)
    w.frame(); // first engine call: Unsupported
    CHECK(w.engine.begin_calls == 1);
    const int reads = w.engine.read_calls, begins = w.engine.begin_calls;
    for (int i = 0; i < 20; ++i) w.frame();
    CHECK(w.channel->support.load() == -1);
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
    const uint32_t script[] = {0, PS5_PAD_BUTTON_L3, PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3, PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3, 0,
                               PS5_PAD_BUTTON_CROSS, PS5_PAD_BUTTON_CROSS, 0, PS5_PAD_BUTTON_R3,
                               PS5_PAD_BUTTON_R3 | PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_CROSS, 0};
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
    for (int i = 0; i < 600; ++i) // 300 presses before the update loop drains
    {
        w.engine.down["F8"] = (i % 2 == 0);
        w.tick();
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
    w.engine.cost_per_read_ns = 300'000; // one read = 0.3 ms per sampled frame (too slow)
    w.bind(Key::F8, "F8");
    for (int i = 0; i < 400; ++i) w.frame();
    CHECK(w.channel->divisor.load() == 8); // 1 -> 2 -> 4 -> 8, then capped
    CHECK(w.channel->throttle_steps.load() == 3);
    w.engine.down["F8"] = true; // a press held for 8+ frames is still seen at the slowest rate
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
    for (int i = 0; i < 6; ++i) w.frame();
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
    for (int i = 0; i < 5; ++i) w.tick();
    CHECK(w.engine.begin_calls == begins);
    CHECK(!w.merged->is_available());
}

TEST(deferred_attach_pad_held_by_game)
{
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
    uint64_t clock = 0;
    EngineKeySampler sampler(channel, std::make_unique<FakeReader>(engine), {}, [&] { return clock += k_frame_ns; });
    auto merged = std::make_shared<CompositeInputSource>(
            std::vector<std::shared_ptr<RC::Input::PlatformInputSource>>{std::make_shared<ThrowingSource>(), std::make_shared<EngineKeySource>(channel)});
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
    std::vector<std::string> log;
    EngineKeySampler sampler(channel, nullptr, {}, {}, [&](const char* l) { log.emplace_back(l); });
    sampler.tick();
    sampler.tick();
    CHECK(channel->support.load() == -1);
    CHECK(log.size() == 1 && log[0] == "[kbm] engine-keys supported=0 reason=no-reader bound=0");
}

// =====================================================================================================================
// New sampler behaviour (K2): pump pacing, exception containment, exact log lines
// =====================================================================================================================
TEST(pump_called_several_times_per_frame_samples_at_most_every_8ms)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    const int begins = w.engine.begin_calls;
    for (int i = 0; i < 5; ++i) // five pump calls 1 ms apart = one 8 ms window
    {
        w.clock += 1'000'000;
        w.sampler->tick();
    }
    CHECK(w.engine.begin_calls - begins == 0); // the last sample was < 8 ms before the first of these
    w.clock += 8'000'000;
    w.sampler->tick();
    CHECK(w.engine.begin_calls - begins == 1);
    CHECK(w.channel->frames.load() >= 7);
}

TEST(reader_exception_fails_closed_pad_continues)
{
    World w(true);
    w.bind(Key::F7, "F7");
    w.bind(Key::F8, "F8");
    w.frame();
    w.engine.throw_on_read = true;
    w.frame(); // the read throws inside tick(): contained
    CHECK(w.channel->support.load() == -1);
    const int begins = w.engine.begin_calls;
    for (int i = 0; i < 5; ++i) w.frame();
    CHECK(w.engine.begin_calls == begins);
    // frame 2 already said supported=1 (the engine answered), then the read threw: one "stopped" line, once
    CHECK((w.log == std::vector<std::string>{"[kbm] engine-keys supported=1 reason=ok bound=2", "[kbm] engine-keys stopped reason=exception"}));
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3;
    w.frame();
    CHECK(w.fired.size() == 1 && w.fired[0] == "F7");
    fakepad::reset();
}

TEST(once_line_supported_exact_text_and_bound_count)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.bind(Key::F8, {ModifierKey::SHIFT}, "SHIFT+F8");
    w.bind(Key::NUM_FIVE, "NUM_FIVE");
    w.bind(Key::RIGHT_MOUSE_BUTTON, "RMB");
    w.bind(Key::F13, "F13"); // no engine key: not counted, never asked
    for (int i = 0; i < 30; ++i) w.frame();
    CHECK(w.log.size() == 1);
    CHECK(!w.log.empty() && w.log[0] == "[kbm] engine-keys supported=1 reason=ok bound=3");
    CHECK(w.channel->bound.load() == 3);
}

TEST(once_line_unsupported_reason_exact_text)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.engine.status = Frame::Unsupported;
    w.engine.reason = "layout:key-param";
    for (int i = 0; i < 10; ++i) w.frame();
    CHECK(w.log.size() == 1 && w.log[0] == "[kbm] engine-keys supported=0 reason=layout:key-param bound=1");
}

TEST(sum_line_every_60s_exact_text)
{
    World w(false);
    w.bind(Key::F8, "F8");
    w.frame();
    w.frame();
    w.engine.down["F8"] = true;
    w.frame();
    w.engine.down["F8"] = false;
    const size_t lines_before = w.log.size();
    for (int i = 0; i < 3600; ++i) w.frame(); // 3600 x 16.7 ms = 60.1 s
    CHECK(w.log.size() == lines_before + 1);
    const std::string& sum = w.log.back();
    CHECK(sum.rfind("[kbm] SUM frames=", 0) == 0);
    // frames/sampled/reads depend on where the 60 s boundary fell: parse the exact format, check the fixed fields
    unsigned long long f{}, s{}, r{}, fl{}, e{}, d{};
    unsigned div{};
    const int n = std::sscanf(sum.c_str(), "[kbm] SUM frames=%llu sampled=%llu reads=%llu fails=%llu events=%llu drops=%llu divisor=%u", &f, &s, &r, &fl, &e, &d, &div);
    CHECK(n == 7);
    CHECK(f >= 3590 && s >= 3590 && r >= 3590 && fl == 0 && e == 1 && d == 0 && div == 1);
    char rebuilt[224];
    std::snprintf(rebuilt, sizeof rebuilt, "[kbm] SUM frames=%llu sampled=%llu reads=%llu fails=%llu events=%llu drops=%llu divisor=%u", f, s, r, fl, e, d, div);
    CHECK(sum == rebuilt); // nothing else on the line
    for (int i = 0; i < 3600; ++i) w.frame();
    CHECK(w.log.size() == lines_before + 2); // bounded: one line per minute
}

// =====================================================================================================================
// Composition (the engine-free half of KbmComposeInput)
// =====================================================================================================================
TEST(compose_with_pad_merges_both_and_pad_chords_work)
{
    fakepad::reset();
    auto pad = make_pad();
    auto channel = std::make_shared<EngineKeyChannel>();
    std::shared_ptr<RC::Input::PlatformInputSource> input = pad;
    auto merged = make_merged_input(input, channel);
    CHECK(merged->children() == 2);
    CHECK(std::string(merged->get_name()) == "Merged");
    Handler handler;
    CHECK(handler.init(merged));
    int fired = 0;
    handler.register_keydown_event(Key::F7, [&] { ++fired; });
    handler.process_event();
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3;
    handler.process_event();
    fakepad::buttons = 0;
    handler.process_event();
    CHECK(fired == 1);
    CHECK(handler.shutdown());
    fakepad::reset();
}

TEST(compose_without_pad_profile_gives_keyboard_binds)
{
    // No PS5Pad.ini: today the PS5 handler has NO source and RegisterKeyBind throws; with K2 the merged
    // engine-keys source is attached (deferred: unavailable until the engine answers) and binds work.
    {
        // PS5 build state with no source: m_source_injected_mode is true by default under __PROSPERO__ (Handler.hpp);
        // on this host build init(nullptr) puts the real Handler into that same state (and returns false).
        Handler before;
        CHECK(!before.init(nullptr));
        bool threw = false;
        try
        {
            before.register_keydown_event(Key::F8, [] {});
        }
        catch (const std::exception&)
        {
            threw = true;
        }
        CHECK(threw); // reference: the pre-K2 behaviour of the real coexist Handler on this target
    }
    auto channel = std::make_shared<EngineKeyChannel>();
    FakeEngine engine;
    uint64_t clock = 0;
    EngineKeySampler sampler(channel, std::make_unique<FakeReader>(engine), {}, [&] { return clock += k_frame_ns; });
    auto merged = make_merged_input(nullptr, channel);
    CHECK(merged->children() == 1);
    CHECK(!merged->is_available());
    Handler handler;
    CHECK(handler.init(merged)); // deferred attach: startup never aborts
    int fired = 0;
    bool threw = false;
    try
    {
        handler.register_keydown_event(Key::F8, [&] { ++fired; });
    }
    catch (...)
    {
        threw = true;
    }
    CHECK(!threw);
    for (int i = 0; i < 2; ++i)
    {
        sampler.tick();
        handler.process_event();
    }
    CHECK(merged->is_available());
    engine.down["F8"] = true;
    sampler.tick();
    handler.process_event();
    CHECK(fired == 1);
}

// =====================================================================================================================
// Reader: reflection checks with a fake reflection model
// =====================================================================================================================
TEST(reader_good_layout_reads_engine_state)
{
    FakeGame g;
    ue5_layout(g);
    auto reader = make_reader(g);
    const auto info = reader->begin_frame();
    CHECK(info.status == Frame::Ready);
    CHECK(reader->is_down(Key::F8, "F8") == std::optional<bool>(false));
    g.down["F8"] = true;
    CHECK(reader->is_down(Key::F8, "F8") == std::optional<bool>(true));
    CHECK(reader->is_down(Key::F7, "F7") == std::optional<bool>(false));
    CHECK(g.calls == 3);
    CHECK(g.find_function_calls == 1); // resolved once per controller class
}

TEST(reader_uses_reflected_offsets_only)
{
    FakeGame g;
    other_layout(g); // return first, bitfield bool mask 0x04, KeyName at +16 inside a Key at +8
    auto reader = make_reader(g);
    CHECK(reader->begin_frame().status == Frame::Ready);
    g.down["NumPadFive"] = true;
    CHECK(reader->is_down(Key::NUM_FIVE, "NumPadFive") == std::optional<bool>(true));
    g.down["NumPadFive"] = false;
    CHECK(reader->is_down(Key::NUM_FIVE, "NumPadFive") == std::optional<bool>(false));
    CHECK(g.frame_violations == 0);
}

TEST(reader_refusals_name_the_failed_check)
{
    auto expect = [](const char* want, auto mutate) {
        FakeGame g;
        ue5_layout(g);
        mutate(g);
        const std::string got = refusal(g);
        if (got != want) std::printf("  refusal: want=%s got=%s\n", want, got.c_str());
        CHECK(got == want);
    };
    expect("ready", [](FakeGame&) {});
    expect("no-function", [](FakeGame& g) { g.has_function = false; });
    expect("layout:unreadable", [](FakeGame& g) { g.layout_readable = false; });
    expect("layout:parms-size", [](FakeGame& g) { g.parms_size = 0; });
    expect("layout:parms-size", [](FakeGame& g) { g.parms_size = 1 << 20; });
    expect("layout:param-count", [](FakeGame& g) { g.params.push_back(g.params[0]); });               // two Key params
    expect("layout:param-count", [](FakeGame& g) { g.params.erase(g.params.begin()); });              // return only
    expect("layout:param-count", [](FakeGame& g) { g.params.pop_back(); });                           // no return value
    expect("layout:return", [](FakeGame& g) { g.params.push_back(g.params[1]); });                    // two return values
    expect("layout:return", [](FakeGame& g) { g.params[1].kind = Kind::Other; });                     // not bool
    expect("layout:return", [](FakeGame& g) { g.return_offset = 28; });                               // offset disagrees
    expect("layout:return", [](FakeGame& g) { g.params[1].offset = 32; g.return_offset = 32; });      // outside ParmsSize
    expect("layout:return", [](FakeGame& g) { g.params[1].bool_field_mask = 0; });
    expect("layout:return", [](FakeGame& g) { g.params[1].bool_byte_offset = 1; });                   // byte outside the bool
    expect("layout:key-param", [](FakeGame& g) { g.params[0].kind = Kind::Name; });                   // not a struct
    expect("layout:key-param", [](FakeGame& g) { g.params[0].ref = true; });                          // by reference
    expect("layout:key-param", [](FakeGame& g) { g.params[0].out = true; });                          // out param
    expect("layout:key-param", [](FakeGame& g) { g.params[0].array_dim = 2; });
    expect("layout:key-param", [](FakeGame& g) { g.params[0].struct_type = nullptr; });
    expect("layout:key-param", [](FakeGame& g) { g.params[0].size = 40; });                           // past ParmsSize
    expect("layout:overlap", [](FakeGame& g) { g.params[1].offset = 8; g.return_offset = 8; });       // return inside Key
    expect("layout:key-struct", [](FakeGame& g) { g.struct_name = "InputChord"; });
    expect("layout:keyname", [](FakeGame& g) { g.members.clear(); });                                  // no KeyName
    expect("layout:keyname", [](FakeGame& g) { g.members.push_back(g.members[0]); });                  // two KeyName
    expect("layout:keyname", [](FakeGame& g) { g.members[0].kind = Kind::Other; });                    // not an FName
    expect("layout:keyname", [](FakeGame& g) { g.members[0].size = 12; });                             // != runtime FName
    expect("layout:keyname", [](FakeGame& g) { g.members[0].offset = 20; });                           // past the Key
    expect("layout:keyname", [](FakeGame& g) { g.name_size = 24; g.members[0].size = 24; });           // name too big
    expect("fname-find", [](FakeGame& g) { g.self_check_ok = false; });                                // two sources disagree
    expect("fname-find", [](FakeGame& g) { g.names[0] = "NotTheFunctionName"; });                     // find-only miss
}

TEST(reader_refusal_is_permanent_and_never_calls_the_engine)
{
    FakeGame g;
    ue5_layout(g);
    g.params[0].ref = true;
    auto reader = make_reader(g);
    for (int i = 0; i < 5; ++i) CHECK(reader->begin_frame().status == Frame::Unsupported);
    CHECK(!reader->is_down(Key::F8, "F8"));
    CHECK(g.calls == 0);
    CHECK(g.find_controller_calls == 1);
    CHECK(std::string(reader->unsupported_reason()) == "layout:key-param");
}

TEST(reader_unknown_key_looked_up_once_never_added)
{
    FakeGame g;
    ue5_layout(g);
    auto reader = make_reader(g);
    CHECK(reader->begin_frame().status == Frame::Ready);
    const size_t names_before = g.names.size();
    for (int i = 0; i < 10; ++i) CHECK(!reader->is_down(Key::F9, "F9")); // "F9" is not in this fake game
    CHECK(g.name_lookups["F9"] == 1);  // miss cached as "key not in this game"
    CHECK(g.names.size() == names_before); // find-only: nothing added
    for (int i = 0; i < 10; ++i) CHECK(reader->is_down(Key::F8, "F8").has_value());
    CHECK(g.name_lookups["F8"] == 1); // hit cached too
    CHECK(g.calls == 10);             // a missing name never reaches the engine
}

TEST(reader_controller_cache_rebuilt_on_miss_with_generation)
{
    FakeGame g;
    ue5_layout(g);
    auto reader = make_reader(g);
    auto info = reader->begin_frame();
    CHECK(info.status == Frame::Ready);
    const uint64_t gen1 = info.controller_generation;
    for (int i = 0; i < 5; ++i) CHECK(reader->begin_frame().controller_generation == gen1); // cached: no search
    CHECK(g.find_controller_calls == 1);
    // level change: the controller dies, no new one yet
    g.live.erase(&g_pc1);
    g.controller = nullptr;
    CHECK(reader->begin_frame().status == Frame::NoController);
    CHECK(!reader->is_down(Key::F8, "F8")); // no controller -> no engine call
    // the search is throttled to twice a second while missing
    const int calls = g.find_controller_calls;
    for (int i = 0; i < 10; ++i) CHECK(reader->begin_frame().status == Frame::NoController);
    CHECK(g.find_controller_calls == calls);
    g.controller = &g_pc2; // new controller, same class
    g.now += EngineKeyReader::k_probe_interval_ms;
    info = reader->begin_frame();
    CHECK(info.status == Frame::Ready);
    CHECK(info.controller_generation == gen1 + 1);
    CHECK(g.find_function_calls == 1); // same class: the checked function is reused
}

TEST(reader_new_controller_class_is_checked_again)
{
    FakeGame g;
    ue5_layout(g);
    auto reader = make_reader(g);
    CHECK(reader->begin_frame().status == Frame::Ready);
    g.live.erase(&g_pc1);
    g.controller = &g_pc3; // another controller class
    g.now += EngineKeyReader::k_probe_interval_ms;
    CHECK(reader->begin_frame().status == Frame::Ready);
    CHECK(g.find_function_calls == 2);
    // a cached object whose slot now holds another class (pointer reuse) counts as lost
    g.classes[&g_pc3] = &g_cls1;
    g.controller = nullptr;
    g.now += EngineKeyReader::k_probe_interval_ms;
    CHECK(reader->begin_frame().status == Frame::NoController);
}

TEST(reader_stops_if_engine_writes_the_key_parameter)
{
    FakeGame g;
    ue5_layout(g);
    g.write_key_param = true; // a by-reference engine would fill FKey::KeyDetails in our frame
    auto reader = make_reader(g);
    CHECK(reader->begin_frame().status == Frame::Ready);
    CHECK(!reader->is_down(Key::F8, "F8"));
    CHECK(reader->begin_frame().status == Frame::Unsupported);
    CHECK(std::string(reader->unsupported_reason()) == "key-param-written");
    CHECK(!reader->is_down(Key::F8, "F8"));
    CHECK(g.calls == 1); // stopped after the first suspicious answer
}

TEST(sampler_logs_stopped_when_engine_writes_key_parameter)
{
    FakeGame g;
    ue5_layout(g);
    g.write_key_param = true;
    World w(false, true, {}, make_reader(g));
    w.bind(Key::F8, "F8");
    for (int i = 0; i < 20; ++i) w.frame();
    CHECK((w.log == std::vector<std::string>{"[kbm] engine-keys supported=1 reason=ok bound=1", "[kbm] engine-keys stopped reason=key-param-written"}));
    CHECK(w.channel->support.load() == -1);
    CHECK(g.calls == 1);
    CHECK(w.fired.empty());
}

TEST(reader_without_model_is_unsupported)
{
    EngineKeyReader reader(nullptr);
    CHECK(reader.begin_frame().status == Frame::Unsupported);
    CHECK(std::string(reader.unsupported_reason()) == "no-reflection");
    CHECK(!reader.is_down(Key::F8, "F8"));
}

TEST(reader_rejects_out_of_range_name_ids)
{
    FakeGame g;
    ue5_layout(g);
    auto reader = make_reader(g);
    CHECK(reader->begin_frame().status == Frame::Ready);
    CHECK(!reader->is_down(k_name_id_count, "F8"));
    CHECK(!reader->is_down(Key::F8, nullptr));
    CHECK(g.calls == 0);
}

TEST(end_to_end_real_reader_sampler_handler)
{
    FakeGame g;
    ue5_layout(g);
    fakepad::reset();
    World w(true, true, {}, make_reader(g));
    w.bind(Key::F8, "F8");
    w.bind(Key::F8, {ModifierKey::SHIFT}, "SHIFT+F8");
    w.bind(Key::RIGHT_MOUSE_BUTTON, "RMB");
    w.bind(Key::F7, "F7");
    for (int i = 0; i < 3; ++i) w.frame();
    g.down["F8"] = true;
    w.frame();
    g.down["F8"] = false;
    w.frame();
    g.down["LeftShift"] = true;
    g.down["F8"] = true;
    w.frame();
    g.down["F8"] = false;
    g.down["LeftShift"] = false;
    g.down["RightMouseButton"] = true;
    w.frame();
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3; // the controller chord still works next to it
    w.frame();
    fakepad::buttons = 0;
    w.frame();
    CHECK((w.fired == std::vector<std::string>{"F8", "SHIFT+F8", "RMB", "F7"}));
    CHECK(w.log.size() == 1 && w.log[0] == "[kbm] engine-keys supported=1 reason=ok bound=3");
    CHECK(g.names.size() == 11); // nothing added to the name table
    CHECK(g.frame_violations == 0);
    fakepad::reset();
}

TEST(end_to_end_bad_layout_switches_off_pad_unaffected)
{
    FakeGame g;
    ue5_layout(g);
    g.params[1].kind = Kind::Other; // the engine's IsInputKeyDown does not return a bool here
    fakepad::reset();
    World w(true, true, {}, make_reader(g));
    w.bind(Key::F7, "F7");
    w.bind(Key::F8, "F8");
    for (int i = 0; i < 20; ++i) w.frame();
    CHECK(w.channel->support.load() == -1);
    CHECK(g.calls == 0);
    CHECK(w.log.size() == 1 && w.log[0].rfind("[kbm] engine-keys supported=0 reason=layout:return bound=", 0) == 0);
    fakepad::buttons = PS5_PAD_BUTTON_L3 | PS5_PAD_BUTTON_R3;
    w.frame();
    fakepad::buttons = 0;
    w.frame();
    CHECK(w.fired.size() == 1 && w.fired[0] == "F7");
    fakepad::reset();
}


// =====================================================================================================================
// Review follow-ups (K2 review 2026-10-14)
// =====================================================================================================================
TEST(zero_bound_keys_means_zero_engine_calls)
{
    {
        World w(true); // pad present, nothing bound
        for (int i = 0; i < 50; ++i) w.frame();
        CHECK(w.engine.begin_calls == 0 && w.engine.read_calls == 0);
    }
    FakeGame g;
    ue5_layout(g);
    World w(false, true, {}, make_reader(g));
    w.bind(Key::F13, "F13"); // a bind, but no engine key name: still nothing to ask
    for (int i = 0; i < 50; ++i) w.frame();
    CHECK(g.find_controller_calls == 0 && g.find_function_calls == 0 && g.calls == 0);
}

static std::vector<uint64_t> gaps_of(const std::vector<uint64_t>& times)
{
    std::vector<uint64_t> gaps;
    for (size_t i = 1; i < times.size(); ++i) gaps.push_back(times[i] - times[i - 1]);
    return gaps;
}

TEST(controller_search_backs_off_and_resets_after_a_find)
{
    FakeGame g;
    ue5_layout(g);
    g.controller = nullptr;
    auto reader = make_reader(g);
    for (int i = 0; i < 4000; ++i) // 40 s in 10 ms steps
    {
        CHECK(reader->begin_frame().status == Frame::NoController);
        g.now += 10;
    }
    const auto gaps = gaps_of(g.probe_times);
    CHECK(gaps.size() >= 6);
    CHECK(gaps.size() >= 6 && (std::vector<uint64_t>(gaps.begin(), gaps.begin() + 6) == std::vector<uint64_t>{500, 1000, 2000, 4000, 8000, 8000}));
    CHECK(reader->probe_misses() == g.probe_times.size());
    g.controller = &g_pc1; // found: the next loss searches at once again, then 0.5 s
    while (reader->begin_frame().status != Frame::Ready) g.now += 10;
    g.live.erase(&g_pc1);
    g.controller = nullptr;
    const int before = g.find_controller_calls;
    CHECK(reader->begin_frame().status == Frame::NoController);
    CHECK(g.find_controller_calls == before + 1);
    g.now += 499;
    CHECK(reader->begin_frame().status == Frame::NoController);
    CHECK(g.find_controller_calls == before + 1);
    g.now += 1;
    reader->begin_frame();
    CHECK(g.find_controller_calls == before + 2);
}

TEST(slow_controller_search_waits_the_maximum)
{
    FakeGame g;
    ue5_layout(g);
    g.controller = nullptr;
    g.probe_cost_ms = 40; // a full object walk
    auto reader = make_reader(g);
    for (int i = 0; i < 2000; ++i) // 20 s in 10 ms steps (plus the search time)
    {
        reader->begin_frame();
        g.now += 10;
    }
    const auto gaps = gaps_of(g.probe_times);
    // a slow search is never repeated sooner than the 8 s cap
    CHECK(gaps.size() >= 2 && gaps[0] == 40 + EngineKeyReader::k_probe_max_ms && gaps[1] == 40 + EngineKeyReader::k_probe_max_ms);
}

TEST(cached_controller_never_read_after_it_was_freed)
{
    FakeGame g;
    ue5_layout(g);
    auto reader = make_reader(g);
    CHECK(reader->begin_frame().status == Frame::Ready);
    g.live.erase(&g_pc1); // the engine freed it (level change)
    g.controller = nullptr;
    for (int i = 0; i < 20; ++i)
    {
        reader->begin_frame();
        g.now += 100;
    }
    CHECK(g.freed_object_reads == 0); // only its object-array slot was checked
}

TEST(divisor_recovers_after_a_slow_patch)
{
    SamplerConfig cfg;
    cfg.recover_frames = 10;
    World w(false, true, cfg);
    w.engine.cost_per_read_ns = 300'000;
    w.bind(Key::F8, "F8");
    for (int i = 0; i < 200; ++i) w.frame();
    CHECK(w.channel->divisor.load() == 8);
    w.engine.cost_per_read_ns = 0; // the slow patch is over
    for (int i = 0; i < 600; ++i) w.frame();
    CHECK(w.channel->divisor.load() == 1);
}

TEST(waiting_line_when_nothing_was_decided_in_the_first_minute)
{
    {
        World w(false);
        w.bind(Key::F8, "F8");
        w.engine.status = Frame::NoController;
        for (int i = 0; i < 3700; ++i) w.frame();
        CHECK(w.log.size() == 2);
        CHECK(w.log.size() == 2 && w.log[1] == "[kbm] engine-keys waiting reason=no-controller bound=1");
        w.engine.status = Frame::Ready; // a controller later: the one-time line still comes, once
        for (int i = 0; i < 5; ++i) w.frame();
        CHECK(w.log.size() == 3 && w.log[2] == "[kbm] engine-keys supported=1 reason=ok bound=1");
    }
    {
        World w(false); // nothing bound at all
        for (int i = 0; i < 7300; ++i) w.frame();
        CHECK(w.log.size() == 3); // SUM, waiting (once), SUM
        CHECK(w.log.size() >= 2 && w.log[1] == "[kbm] engine-keys waiting reason=no-binds bound=0");
    }
}

TEST(runtime_glue_compose_once_failure_keeps_input_retire_reentry)
{
    // factory that throws: input untouched, nothing composed
    {
        fakepad::reset();
        std::shared_ptr<RC::Input::PlatformInputSource> input = make_pad();
        auto* before = input.get();
        EngineKeysRuntime rt([]() -> std::unique_ptr<KeyStateReader> { throw std::runtime_error("no memory"); });
        CHECK(!rt.compose(input));
        CHECK(input.get() == before && !rt.composed());
        rt.frame(); // no-op, no crash
        rt.retire();
    }
    // normal: compose once, frame samples, re-entry samples nothing, retire stops
    struct ReenteringReader final : public KeyStateReader
    {
        EngineKeysRuntime* rt{};
        int begins = 0, nested = 0;
        FrameInfo begin_frame() override
        {
            ++begins;
            if (rt && begins == 1)
            {
                const int b = begins;
                rt->frame(); // the engine re-entering the pump from inside our call
                nested += begins - b;
            }
            return {Frame::Ready, 1};
        }
        std::optional<bool> is_down(uint16_t, const char*) override { return false; }
    };
    ReenteringReader* view = nullptr;
    uint64_t clock = 0;
    EngineKeysRuntime rt(
            [&]() -> std::unique_ptr<KeyStateReader> {
                auto r = std::make_unique<ReenteringReader>();
                view = r.get();
                return r;
            },
            {}, [&] { return clock += k_frame_ns; });
    std::shared_ptr<RC::Input::PlatformInputSource> input; // no PS5Pad.ini
    CHECK(rt.compose(input));
    CHECK(input && std::string(input->get_name()) == "Merged");
    auto first = input;
    CHECK(!rt.compose(input) && input == first); // once
    view->rt = &rt;
    Handler handler;
    CHECK(handler.init(input));
    handler.register_keydown_event(Key::F8, [] {});
    handler.process_event();
    rt.frame();
    CHECK(view->begins == 1 && view->nested == 0);
    rt.frame();
    CHECK(view->begins == 2);
    CHECK(handler.shutdown()); // teardown order: handler shutdown ...
    rt.retire();               // ... -> pump retirement
    for (int i = 0; i < 5; ++i) rt.frame();
    CHECK(view->begins == 2 && rt.retired());
}

namespace
{
    class StuckSource final : public RC::Input::PlatformInputSource
    {
      public:
        bool is_available() override { return true; }
        bool activate() override { return true; }
        bool deactivate() override { return false; } // e.g. the pad close failed
        std::vector<RC::Input::InputEvent>& process_event(Handler*) override { return m_events; }
        const char* get_name() override { return "Stuck"; }

      private:
        std::vector<RC::Input::InputEvent> m_events;
    };
} // namespace

TEST(failed_pad_deactivate_still_reported_through_merged)
{
    // Before K2 Handler::shutdown returned false here and the program terminated on purpose; Merged keeps that.
    auto stuck = std::make_shared<StuckSource>();
    {
        Handler pad_only;
        CHECK(pad_only.init(stuck));
        CHECK(!pad_only.shutdown());
    }
    auto merged = make_merged_input(stuck, std::make_shared<EngineKeyChannel>());
    Handler handler;
    CHECK(handler.init(merged));
    CHECK(!merged->deactivate());
    CHECK(!handler.shutdown());
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
        out << row.name << '\t' << value << '\t' << (row.fkey ? row.fkey : "-") << '\t' << hid << '\t' << (row.note ? row.note : "") << '\n';
    }
}

int main(int argc, char** argv)
{
    for (int i = 1; i + 1 < argc; ++i)
        if (std::strcmp(argv[i], "--dump-table") == 0) dump_table(argv[i + 1]);
    int passed = 0;
    for (const auto& t : tests())
    {
        const int before = g_failures;
        t.fn();
        const bool ok = g_failures == before;
        passed += ok ? 1 : 0;
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", t.name);
    }
    std::printf("tests=%zu passed=%d checks=%d failures=%d\n", tests().size(), passed, g_checks, g_failures);
    if (tests().empty() || g_checks == 0)
    {
        std::printf("FAIL: zero tests or zero checks ran\n");
        return 3;
    }
    return g_failures ? 1 : 0;
}
