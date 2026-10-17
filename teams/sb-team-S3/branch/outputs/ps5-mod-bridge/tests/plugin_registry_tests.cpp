#include "test.hpp"
#include "modbridge/compat.hpp"
#include "modbridge/core.hpp"
#include "modbridge/drivers.hpp"
#include "modbridge/plugin_registry.hpp"
#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace modbridge;
namespace {

// Every rejection must be an std::invalid_argument with the "plugin: " prefix.
#define CHECK_PLUGIN_THROWS(expression) do { bool caught_ = false; \
    try { (void)(expression); } \
    catch (const std::invalid_argument& error_) { \
        caught_ = std::string(error_.what()).rfind("plugin: ", 0) == 0; } \
    CHECK(caught_); } while (false)

constexpr const char* response_schema = "modbridge.plugin-response/1";

std::string text(const Json& value) { return value.dump(); }

// A counting fake driver. Every owned result it hands out is a heap block whose
// release callback counts; a double release is a double free and trips the
// sanitizers as well as the counters. The api's handle points at the fake, so a
// Fake never moves.
struct Fake {
    struct Block { Fake* fake; std::string data; };
    std::string descriptor;
    std::string response;
    mb_plugin_status status = MB_PLUGIN_OK;
    bool describe_returns_null = false;
    bool descriptor_without_release = false;
    bool response_returns_null = false;
    bool response_without_release = false;
    int describe_calls = 0, invoke_calls = 0, issued = 0, released = 0, bad_release = 0;
    mb_plugin_operation last_operation = MB_PLUGIN_ANALYZE;
    std::string last_request;
    const mb_plugin_host_services* last_services = nullptr;
    std::vector<Block*> unreleasable;   // blocks handed out without a release callback
    mb_plugin_api api{};

    explicit Fake(std::string descriptor_text, std::string response_text = "") : descriptor(std::move(descriptor_text)), response(std::move(response_text)) {
        api.abi_version = MB_PLUGIN_ABI_VERSION;
        api.struct_size = sizeof(mb_plugin_api);
        api.handle = reinterpret_cast<mb_plugin_handle*>(this);
        api.describe = &Fake::describe;
        api.invoke = &Fake::invoke;
        api.destroy = nullptr;
    }
    Fake(const Fake&) = delete;
    Fake& operator=(const Fake&) = delete;
    ~Fake() { for (auto* block : unreleasable) delete block; }

    bool balanced() const { return issued == released && bad_release == 0; }

    mb_owned_json make(const std::string& data, bool with_release) {
        auto* block = new Block{this, data};
        if (!with_release) {
            unreleasable.push_back(block);
            return mb_owned_json{block->data.data(), block->data.size(), block, nullptr};
        }
        ++issued;
        return mb_owned_json{block->data.data(), block->data.size(), block, &Fake::release};
    }
    static void release(void* owner, const char* bytes, std::size_t length) {
        auto* block = static_cast<Block*>(owner);
        if (bytes != block->data.data() || length != block->data.size()) ++block->fake->bad_release;
        ++block->fake->released;
        delete block;
    }
    static Fake* self(mb_plugin_handle* handle) { return reinterpret_cast<Fake*>(handle); }
    static mb_owned_json describe(mb_plugin_handle* handle) {
        auto* fake = self(handle);
        ++fake->describe_calls;
        if (fake->describe_returns_null) return mb_owned_json{nullptr, 0, nullptr, nullptr};
        return fake->make(fake->descriptor, !fake->descriptor_without_release);
    }
    static mb_plugin_status invoke(mb_plugin_handle* handle, mb_plugin_operation operation, mb_json_view request,
                                   const mb_plugin_host_services* services, mb_owned_json* out) {
        auto* fake = self(handle);
        ++fake->invoke_calls;
        fake->last_operation = operation;
        fake->last_request.assign(request.bytes, request.length);
        fake->last_services = services;
        *out = fake->response_returns_null ? mb_owned_json{nullptr, 0, nullptr, nullptr}
                                           : fake->make(fake->response, !fake->response_without_release);
        return fake->status;
    }
};

Json descriptor(const std::string& id, const std::string& mode = "transformer", Json kinds = Json::array({"unreal_pak"}),
                Json operations = Json::array({3, 5})) {
    return {{"schema", "modbridge.plugin-driver/1"}, {"id", id}, {"version", "1.0.0"}, {"mode", mode}, {"kinds", kinds},
            {"capabilities", Json::array()}, {"target_fingerprints", Json::array({"executable"})},
            {"metadata", {{"type", "data"}, {"operations", operations}, {"engines", Json::array({"unreal"})},
                          {"roles", Json::array({"container"})}, {"activation", "none"}}}};
}
Json ok_response() { return {{"schema", response_schema}, {"result", {{"x", 1}}}}; }

std::shared_ptr<const PluginRegistry> seal(const std::vector<const mb_plugin_api*>& drivers, std::uint64_t generation = 1,
                                           const PluginLimits& limits = {}) {
    return seal_plugin_registry(drivers, generation, limits);
}

// Seals one fake, expecting rejection; the descriptor must have been released exactly once.
void expect_rejected(Fake& fake) {
    CHECK_PLUGIN_THROWS(seal({&fake.api}));
    CHECK_EQ(fake.describe_calls, 1);
    CHECK(fake.balanced());
}

// ------------------------------------------------------------------ sealing
TEST(plugin_seal_success_sorted_and_released_once) {
    Fake zeta(text(descriptor("zeta.driver"))), alpha(text(descriptor("alpha.driver", "analyzer", Json::array({"lua"}), Json::array({1, 2})))),
        mid(text(descriptor("mid.driver", "runtime", Json::array({"prx"}), Json::array({7}))));
    const auto registry = seal({&zeta.api, &alpha.api, &mid.api}, 42);
    CHECK_EQ(plugin_registry_generation(*registry), 42u);
    const auto descriptors = plugin_descriptors(*registry);
    CHECK_EQ(descriptors.size(), 3u);
    CHECK_EQ(descriptors[0].at("id"), "alpha.driver");
    CHECK_EQ(descriptors[1].at("id"), "mid.driver");
    CHECK_EQ(descriptors[2].at("id"), "zeta.driver");
    CHECK_EQ(descriptors[1], descriptor("mid.driver", "runtime", Json::array({"prx"}), Json::array({7})));
    for (Fake* fake : {&zeta, &alpha, &mid}) {
        CHECK_EQ(fake->describe_calls, 1);   // describe runs once per driver
        CHECK_EQ(fake->issued, 1);
        CHECK(fake->balanced());
        CHECK_EQ(fake->invoke_calls, 0);
    }
}

TEST(plugin_seal_empty_registry_and_generation) {
    const auto registry = seal({}, 7);
    CHECK_EQ(plugin_registry_generation(*registry), 7u);
    CHECK_EQ(plugin_descriptors(*registry), Json::array());
    CHECK_EQ(planner_backends(*registry), Json::array());
    CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "anything", MB_PLUGIN_ANALYZE, Json::object()));
}

TEST(plugin_registry_outlives_the_input_list) {
    Fake fake(text(descriptor("unreal.pak")), text(ok_response()));
    std::shared_ptr<const PluginRegistry> registry;
    {
        std::vector<const mb_plugin_api*> drivers = {&fake.api};
        registry = seal_plugin_registry(drivers, 1);
    }   // the caller's list is gone; the registry kept the api pointers
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("status"), "ok");
    // Returned JSON is a copy: editing it cannot change the sealed registry.
    auto descriptors = plugin_descriptors(*registry);
    descriptors[0]["id"] = "mutated";
    auto backends = planner_backends(*registry);
    backends[0]["kinds"].push_back("extra");
    CHECK_EQ(plugin_descriptors(*registry)[0].at("id"), "unreal.pak");
    CHECK_EQ(planner_backends(*registry)[0].at("kinds"), Json::array({"unreal_pak"}));
}

TEST(plugin_seal_rejects_duplicate_ids) {
    Fake a(text(descriptor("same.id"))), b(text(descriptor("same.id", "transformer", Json::array({"other_kind"}))));
    CHECK_PLUGIN_THROWS(seal({&a.api, &b.api}));
    CHECK(a.balanced());
    CHECK(b.balanced());
    CHECK_EQ(b.describe_calls, 1);
}

TEST(plugin_seal_rejects_null_and_repeated_drivers) {
    Fake a(text(descriptor("a.one")));
    CHECK_PLUGIN_THROWS(seal({&a.api, nullptr}));
    CHECK_PLUGIN_THROWS(seal({&a.api, &a.api}));
    CHECK_EQ(a.describe_calls, 0);   // a malformed list runs no driver code at all
    CHECK(a.balanced());
}

TEST(plugin_seal_rejects_null_function_pointers) {
    Fake no_describe(text(descriptor("a.one"))), no_invoke(text(descriptor("b.two")));
    mb_plugin_api broken_describe = no_describe.api, broken_invoke = no_invoke.api;
    broken_describe.describe = nullptr;
    broken_invoke.invoke = nullptr;
    CHECK_PLUGIN_THROWS(seal({&broken_describe}));
    CHECK_PLUGIN_THROWS(seal({&broken_invoke}));
    CHECK_EQ(no_describe.describe_calls, 0);
    CHECK_EQ(no_invoke.describe_calls, 0);   // rejected before calling anything
    Fake good(text(descriptor("c.three")));
    mb_plugin_api null_destroy = good.api;   // destroy may be NULL
    CHECK_EQ(plugin_descriptors(*seal({&null_destroy})).size(), 1u);
}

TEST(plugin_seal_rejects_wrong_abi_and_short_struct) {
    Fake fake(text(descriptor("a.one")));
    mb_plugin_api wrong_abi = fake.api, old_abi = fake.api, short_struct = fake.api, zero_size = fake.api, longer = fake.api;
    wrong_abi.abi_version = MB_PLUGIN_ABI_VERSION + 1;
    old_abi.abi_version = 1;
    short_struct.struct_size = static_cast<std::uint32_t>(sizeof(mb_plugin_api)) - 1;
    zero_size.struct_size = 0;
    longer.struct_size = static_cast<std::uint32_t>(sizeof(mb_plugin_api)) + 64;   // a newer, longer layout is fine
    for (const mb_plugin_api* bad : {&wrong_abi, &old_abi, &short_struct, &zero_size}) CHECK_PLUGIN_THROWS(seal({bad}));
    CHECK_EQ(fake.describe_calls, 0);   // checked before calling the driver
    CHECK_EQ(plugin_descriptors(*seal({&longer})).size(), 1u);
    CHECK(fake.balanced());
}

TEST(plugin_seal_rejects_too_many_drivers) {
    Fake a(text(descriptor("a.one"))), b(text(descriptor("b.two"))), c(text(descriptor("c.three")));
    PluginLimits limits;
    limits.max_drivers = 2;
    CHECK_PLUGIN_THROWS(seal({&a.api, &b.api, &c.api}, 1, limits));
    CHECK_EQ(a.describe_calls + b.describe_calls + c.describe_calls, 0);
    CHECK_EQ(plugin_descriptors(*seal({&a.api, &b.api}, 1, limits)).size(), 2u);
    // The documented default is 64.
    CHECK_EQ(PluginLimits{}.max_drivers, 64u);
    CHECK_EQ(PluginLimits{}.max_descriptor_bytes, 65536u);
    CHECK_EQ(PluginLimits{}.max_response_bytes, 8388608u);
}

TEST(plugin_seal_one_bad_driver_rejects_the_whole_registry) {
    Fake good(text(descriptor("good.one"))), bad("not json");
    CHECK_PLUGIN_THROWS(seal({&good.api, &bad.api}));
    CHECK(good.balanced());
    CHECK(bad.balanced());
    CHECK_EQ(good.issued, 1);
    // An empty or missing descriptor is rejected too, without a release for nothing.
    Fake silent("");
    silent.describe_returns_null = true;
    CHECK_PLUGIN_THROWS(seal({&silent.api}));
    CHECK_EQ(silent.issued, 0);
    Fake zero_length("");
    expect_rejected(zero_length);   // bytes non-null, length 0: still released once
    CHECK_EQ(zero_length.issued, 1);
}

TEST(plugin_seal_descriptor_without_release_is_rejected_not_read) {
    Fake fake(text(descriptor("a.one")));
    fake.descriptor_without_release = true;
    CHECK_PLUGIN_THROWS(seal({&fake.api}));
    CHECK_EQ(fake.describe_calls, 1);
    CHECK_EQ(fake.issued, 0);
    CHECK_EQ(fake.released, 0);
}

TEST(plugin_seal_descriptor_size_cap) {
    const auto body = text(descriptor("a.one"));
    PluginLimits limits;
    limits.max_descriptor_bytes = body.size();
    Fake exact(body);
    CHECK_EQ(plugin_descriptors(*seal({&exact.api}, 1, limits)).size(), 1u);
    limits.max_descriptor_bytes = body.size() - 1;
    Fake over(body);
    CHECK_PLUGIN_THROWS(seal({&over.api}, 1, limits));
    CHECK_EQ(over.describe_calls, 1);
    CHECK_EQ(over.issued, 1);
    CHECK(over.balanced());   // released although never read
}

TEST(plugin_seal_descriptor_json_limits) {
    PluginLimits roomy;
    roomy.max_descriptor_bytes = 4u << 20;
    auto with_metadata = [&](Json extra) { auto d = descriptor("a.one"); d["metadata"]["probe"] = std::move(extra); return text(d); };
    auto nested = [](int depth) { std::string s(static_cast<std::size_t>(depth), '['); s += std::string(static_cast<std::size_t>(depth), ']'); return Json::parse(s, nullptr, true, false); };
    // 100 levels of nesting (well under 128) is accepted and kept; 200 is rejected.
    {
        std::string body = with_metadata(nested(100));
        Fake fake(body);
        CHECK_EQ(plugin_descriptors(*seal({&fake.api}, 1, roomy))[0].at("metadata").at("probe"), nested(100));
    }
    {
        auto d = descriptor("a.one");
        const std::string deep = std::string(200, '[') + std::string(200, ']');
        std::string body = text(d);
        body.insert(body.find("\"metadata\":{") + 12, "\"probe\":" + deep + ",");
        Fake fake(body);
        CHECK_PLUGIN_THROWS(seal({&fake.api}, 1, roomy));
        CHECK(fake.balanced());
    }
    // Node limit 100000.
    {
        Json many = Json::array();
        for (int i = 0; i < 50000; ++i) many.push_back(0);
        Fake fake(with_metadata(many));
        CHECK_EQ(plugin_descriptors(*seal({&fake.api}, 1, roomy)).size(), 1u);
        for (int i = 0; i < 60000; ++i) many.push_back(0);
        Fake over(with_metadata(many));
        CHECK_PLUGIN_THROWS(seal({&over.api}, 1, roomy));
        CHECK(over.balanced());
    }
}

TEST(plugin_seal_invalid_descriptor_text) {
    for (const std::string& body : {std::string("{"), std::string("nonsense"), std::string("[]"), std::string("null"), std::string("\"x\""),
                                   std::string("42"), text(descriptor("a.one")) + " trailing", std::string("{\"id\":\"\xff\"}")}) {
        Fake fake(body);
        expect_rejected(fake);
    }
}

TEST(plugin_seal_rejects_invalid_descriptors) {
    using Mutation = std::function<void(Json&)>;
    const std::vector<std::pair<std::string, Mutation>> cases = {
        {"unknown top-level key", [](Json& d) { d["extra"] = 1; }},
        // Now optional descriptor fields: only wrong types are invalid.
        {"provenance not an object", [](Json& d) { d["provenance"] = Json::array(); }},
        {"supports_reference_cycles not a boolean", [](Json& d) { d["supports_reference_cycles"] = 1; }},
        {"wrong schema", [](Json& d) { d["schema"] = "modbridge.plugin-driver/2"; }},
        {"non-string schema", [](Json& d) { d["schema"] = 1; }},
        {"empty id", [](Json& d) { d["id"] = ""; }},
        {"uppercase id", [](Json& d) { d["id"] = "Unreal.pak"; }},
        {"leading dot id", [](Json& d) { d["id"] = ".pak"; }},
        {"leading dash id", [](Json& d) { d["id"] = "-pak"; }},
        {"space in id", [](Json& d) { d["id"] = "un real"; }},
        {"slash in id", [](Json& d) { d["id"] = "a/b"; }},
        {"non-ascii id", [](Json& d) { d["id"] = "caf\xc3\xa9"; }},
        {"65 byte id", [](Json& d) { d["id"] = std::string(65, 'a'); }},
        {"numeric id", [](Json& d) { d["id"] = 7; }},
        {"empty version", [](Json& d) { d["version"] = ""; }},
        {"version with space", [](Json& d) { d["version"] = "1 0"; }},
        {"version with control", [](Json& d) { d["version"] = "1\t0"; }},
        {"version non-ascii", [](Json& d) { d["version"] = "1.\xc3\xa9"; }},
        {"33 byte version", [](Json& d) { d["version"] = std::string(33, '1'); }},
        {"numeric version", [](Json& d) { d["version"] = 1; }},
        {"unknown mode", [](Json& d) { d["mode"] = "installer"; }},
        {"capitalised mode", [](Json& d) { d["mode"] = "Transformer"; }},
        {"numeric mode", [](Json& d) { d["mode"] = 3; }},
        {"no kinds", [](Json& d) { d["kinds"] = Json::array(); }},
        {"33 kinds", [](Json& d) { d["kinds"] = Json::array(); for (int i = 0; i < 33; ++i) d["kinds"].push_back("kind_" + std::to_string(i)); }},
        {"uppercase kind", [](Json& d) { d["kinds"] = Json::array({"Lua"}); }},
        {"dash kind", [](Json& d) { d["kinds"] = Json::array({"un-real"}); }},
        {"empty kind", [](Json& d) { d["kinds"] = Json::array({""}); }},
        {"non-string kind", [](Json& d) { d["kinds"] = Json::array({1}); }},
        {"65 byte kind", [](Json& d) { d["kinds"] = Json::array({std::string(65, 'a')}); }},
        {"duplicate kind", [](Json& d) { d["kinds"] = Json::array({"lua", "lua"}); }},
        {"kinds not an array", [](Json& d) { d["kinds"] = "lua"; }},
        {"capabilities not an array", [](Json& d) { d["capabilities"] = "x"; }},
        {"65 capabilities", [](Json& d) { d["capabilities"] = Json::array(); for (int i = 0; i < 65; ++i) d["capabilities"].push_back("cap." + std::to_string(i)); }},
        {"empty capability string", [](Json& d) { d["capabilities"] = Json::array({""}); }},
        {"numeric capability", [](Json& d) { d["capabilities"] = Json::array({1}); }},
        {"capability without id", [](Json& d) { d["capabilities"] = Json::array({Json::object()}); }},
        {"capability empty id", [](Json& d) { d["capabilities"] = Json::array({{{"id", ""}}}); }},
        {"capability unknown key", [](Json& d) { d["capabilities"] = Json::array({{{"id", "x"}, {"bogus", 1}}}); }},
        {"capability empty version", [](Json& d) { d["capabilities"] = Json::array({{{"id", "x"}, {"version", ""}}}); }},
        {"capability empty context object", [](Json& d) { d["capabilities"] = Json::array({{{"id", "x"}, {"context", Json::object()}}}); }},
        {"capability scope not array", [](Json& d) { d["capabilities"] = Json::array({{{"id", "x"}, {"scope", "a"}}}); }},
        {"capability scope empty item", [](Json& d) { d["capabilities"] = Json::array({{{"id", "x"}, {"scope", Json::array({""})}}}); }},
        {"capability evidence not object", [](Json& d) { d["capabilities"] = Json::array({{{"id", "x"}, {"evidence", "e"}}}); }},
        {"33 fingerprints", [](Json& d) { d["target_fingerprints"] = Json::array(); for (int i = 0; i < 33; ++i) d["target_fingerprints"].push_back("fp" + std::to_string(i)); }},
        {"empty fingerprint", [](Json& d) { d["target_fingerprints"] = Json::array({""}); }},
        {"non-string fingerprint", [](Json& d) { d["target_fingerprints"] = Json::array({1}); }},
        {"fingerprints not an array", [](Json& d) { d["target_fingerprints"] = "executable"; }},
        {"metadata not an object", [](Json& d) { d["metadata"] = Json::array(); }},
        {"missing type", [](Json& d) { d["metadata"].erase("type"); }},
        {"unknown type", [](Json& d) { d["metadata"]["type"] = "firmware"; }},
        {"non-string type", [](Json& d) { d["metadata"]["type"] = 1; }},
        {"missing operations", [](Json& d) { d["metadata"].erase("operations"); }},
        {"empty operations", [](Json& d) { d["metadata"]["operations"] = Json::array(); }},
        {"operations not an array", [](Json& d) { d["metadata"]["operations"] = "3"; }},
        {"transformer without PREPARE", [](Json& d) { d["metadata"]["operations"] = Json::array({1, 2, 5}); }},
        {"analyzer without ANALYZE", [](Json& d) { d["mode"] = "analyzer"; d["metadata"]["operations"] = Json::array({2, 3}); }},
        {"runtime without SET_ACTIVE", [](Json& d) { d["mode"] = "runtime"; d["metadata"]["operations"] = Json::array({1, 3, 6}); }},
        {"operation 0", [](Json& d) { d["metadata"]["operations"] = Json::array({0, 3}); }},
        {"operation above max", [](Json& d) { d["metadata"]["operations"] = Json::array({3, 10}); }},
        {"negative operation", [](Json& d) { d["metadata"]["operations"] = Json::array({-1, 3}); }},
        {"fractional operation", [](Json& d) { d["metadata"]["operations"] = Json::array({3.0}); }},
        {"string operation", [](Json& d) { d["metadata"]["operations"] = Json::array({"3"}); }},
        {"unsorted operations", [](Json& d) { d["metadata"]["operations"] = Json::array({5, 3}); }},
        {"duplicate operations", [](Json& d) { d["metadata"]["operations"] = Json::array({3, 3}); }},
        {"unknown activation", [](Json& d) { d["metadata"]["activation"] = "boot"; }},
        {"non-string activation", [](Json& d) { d["metadata"]["activation"] = true; }},
        {"engines not an array", [](Json& d) { d["metadata"]["engines"] = "unreal"; }},
        {"empty role", [](Json& d) { d["metadata"]["roles"] = Json::array({""}); }},
    };
    for (const auto& [name, mutate] : cases) {
        auto d = descriptor("a.one");
        mutate(d);
        Fake fake(text(d));
        try { expect_rejected(fake); }
        catch (const std::exception& error) { throw std::runtime_error("case '" + name + "': " + error.what()); }
    }
    for (const char* field : {"schema", "id", "version", "mode", "kinds", "capabilities", "target_fingerprints", "metadata"}) {
        auto d = descriptor("a.one");
        d.erase(field);
        Fake fake(text(d));
        expect_rejected(fake);
    }
}

TEST(plugin_seal_accepts_valid_descriptor_variants) {
    std::vector<Json> variants;
    variants.push_back(descriptor("a.one", "analyzer", Json::array({"lua"}), Json::array({1})));
    variants.push_back(descriptor("a.two", "runtime", Json::array({"prx"}), Json::array({1, 2, 3, 4, 5, 6, 7, 8, 9})));
    variants.push_back(descriptor(std::string("a") + std::string(63, '-'), "transformer", Json::array({"unreal_pak"}), Json::array({3})));
    variants.push_back(descriptor("a_b.c-d9"));
    {   // 32 kinds, 64 capabilities (strings and objects), 32 fingerprints, opaque metadata kept
        auto d = descriptor("a.three");
        d["kinds"] = Json::array();
        for (int i = 0; i < 32; ++i) d["kinds"].push_back("kind_" + std::to_string(i));
        d["capabilities"] = Json::array({"runtime.ue4ss", {{"id", "translation.unreal_iostore"}, {"version", ">=1"}, {"context", "preparation"},
                                                            {"scope", Json::array({"execute"})}, {"evidence", {{"k", "v"}}}, {"consumer", "c"},
                                                            {"discovery", "d"}, {"status", "unresolved"}, {"provenance", Json::object()}},
                                         {{"id", "ctx"}, {"context", {{"a", 1}}}}});
        while (d["capabilities"].size() < 64) d["capabilities"].push_back("cap." + std::to_string(d["capabilities"].size()));
        d["target_fingerprints"] = Json::array();
        for (int i = 0; i < 32; ++i) d["target_fingerprints"].push_back("fp" + std::to_string(i));
        d["metadata"]["vendor_extension"] = {{"anything", Json::array({1, 2, 3})}};
        d["metadata"]["type"] = "analysis";
        d["version"] = "1.0.0-rc.1+build.5";
        variants.push_back(d);
    }
    variants.push_back([] { auto d = descriptor("a.four"); d["metadata"].erase("engines"); d["metadata"].erase("roles"); d["metadata"].erase("activation"); return d; }());
    for (const auto& d : variants) {
        Fake fake(text(d));
        const auto registry = seal({&fake.api});
        CHECK_EQ(plugin_descriptors(*registry), Json::array({d}));   // validated, copied, unchanged
        CHECK(fake.balanced());
    }
}

// ------------------------------------------------------------------- invoke
TEST(plugin_invoke_success_shape_and_release_once) {
    Fake fake(text(descriptor("unreal.pak")), text(ok_response()));
    const auto registry = seal({&fake.api});
    mb_plugin_host_services services{};
    services.struct_size = sizeof(services);
    const Json request = {{"b", 2}, {"a", Json::array({1, "x"})}};
    const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, request, &services);
    CHECK_EQ(result, (Json{{"driver", "unreal.pak"}, {"operation", 3}, {"status", "ok"}, {"response", ok_response()}, {"response_error", nullptr}}));
    CHECK_EQ(result.size(), 5u);
    CHECK_EQ(fake.invoke_calls, 1);
    CHECK_EQ(fake.last_operation, MB_PLUGIN_PREPARE);
    CHECK_EQ(fake.last_request, request.dump());
    CHECK(fake.last_services == &services);
    CHECK_EQ(fake.issued, 2);   // descriptor + response
    CHECK(fake.balanced());
    // Default services are NULL; the same call is deterministic.
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, request), result);
    CHECK(fake.last_services == nullptr);
    CHECK(fake.balanced());
}

TEST(plugin_invoke_status_mapping_keeps_valid_response) {
    const std::vector<std::pair<mb_plugin_status, std::string>> statuses = {
        {MB_PLUGIN_OK, "ok"}, {MB_PLUGIN_BAD_REQUEST, "bad_request"}, {MB_PLUGIN_UNSUPPORTED, "unsupported"},
        {MB_PLUGIN_REFUSED, "refused"}, {MB_PLUGIN_UNRESOLVED, "unresolved"}, {MB_PLUGIN_CONFLICT, "conflict"},
        {MB_PLUGIN_LIMIT, "limit"}, {MB_PLUGIN_CANCELLED, "cancelled"}, {MB_PLUGIN_IO, "io"}, {MB_PLUGIN_INTERNAL, "internal"}};
    Fake fake(text(descriptor("unreal.pak")), text(ok_response()));
    const auto registry = seal({&fake.api});
    for (const auto& [status, name] : statuses) {
        fake.status = status;
        const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
        CHECK_EQ(result.at("status"), name);
        CHECK_EQ(result.at("response"), ok_response());   // an error status still carries its validated response
        CHECK_EQ(result.at("response_error"), nullptr);
        CHECK(fake.balanced());
    }
    CHECK_EQ(fake.invoke_calls, 10);
}

TEST(plugin_invoke_invalid_json_releases_once) {
    Fake fake(text(descriptor("unreal.pak")), "{\"schema\":");
    const auto registry = seal({&fake.api});
    auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "internal");   // an OK driver with a bad response is not OK
    CHECK_EQ(result.at("response"), nullptr);
    CHECK_EQ(result.at("response_error"), "invalid_json");
    CHECK(fake.balanced());
    fake.status = MB_PLUGIN_CONFLICT;            // the driver's own error status is kept
    result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "conflict");
    CHECK_EQ(result.at("response_error"), "invalid_json");
    CHECK(fake.balanced());
    CHECK_EQ(fake.issued, 3);
}

TEST(plugin_invoke_response_json_limits) {
    Fake fake(text(descriptor("unreal.pak")));
    const auto registry = seal({&fake.api});
    const std::string head = std::string("{\"schema\":\"") + response_schema + "\",\"result\":{\"deep\":";
    fake.response = head + std::string(100, '[') + std::string(100, ']') + "}}";
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("status"), "ok");
    fake.response = head + std::string(200, '[') + std::string(200, ']') + "}}";
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("response_error"), "invalid_json");
    std::string many = "0";
    for (int i = 0; i < 100000; ++i) many += ",0";
    fake.response = head + "[" + many + "]}}";
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("response_error"), "invalid_json");
    fake.response = head + "\"\xff\"}}";   // not UTF-8
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("response_error"), "invalid_json");
    fake.response = head + "1e999}}";      // not finite
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("response_error"), "invalid_json");
    CHECK(fake.balanced());
}

TEST(plugin_invoke_authority_claims_rejected_everywhere) {
    Fake fake(text(descriptor("unreal.pak")));
    const auto registry = seal({&fake.api});
    const std::vector<Json> claims = {
        {{"schema", response_schema}, {"installation_supported", true}},
        {{"schema", response_schema}, {"runtime_supported", true}},
        {{"schema", response_schema}, {"result", {{"installation_supported", true}}}},
        {{"schema", response_schema}, {"result", {{"a", {{"b", Json::array({{{"runtime_supported", true}}})}}}}}},
        {{"schema", response_schema}, {"conflicts", Json::array({{{"installation_supported", true}}})}},
        {{"schema", response_schema}, {"reasons", Json::array({{{"code", "x"}, {"detail", "y"}, {"runtime_supported", true}}})}},
        {{"schema", response_schema}, {"result", {{"installation_supported", "true"}}}},   // never a truthy non-false value
        {{"schema", response_schema}, {"result", {{"runtime_supported", 1}}}},
        {{"schema", "wrong"}, {"installation_supported", true}},   // reported ahead of the schema defect
    };
    for (mb_plugin_status status : {MB_PLUGIN_OK, MB_PLUGIN_REFUSED}) {
        fake.status = status;
        for (const auto& claim : claims) {
            fake.response = text(claim);
            const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
            CHECK_EQ(result.at("response"), nullptr);
            CHECK_EQ(result.at("response_error"), "authority_claim");
            CHECK_EQ(result.at("status"), status == MB_PLUGIN_OK ? "internal" : "refused");
            CHECK(fake.balanced());
        }
    }
    // The same keys set to false are not claims.
    fake.status = MB_PLUGIN_OK;
    const Json denied = {{"schema", response_schema}, {"result", {{"installation_supported", false}, {"n", {{"runtime_supported", false}}}}}};
    fake.response = text(denied);
    const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "ok");
    CHECK_EQ(result.at("response"), denied);
    // Values merely containing the words are not claims.
    fake.response = text(Json{{"schema", response_schema}, {"result", {{"note", "installation_supported"}, {"installation_supported_x", true}}}});
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("status"), "ok");
    CHECK(fake.balanced());
}

TEST(plugin_invoke_oversized_response_releases_once) {
    Fake fake(text(descriptor("unreal.pak")), text(ok_response()));
    PluginLimits limits;
    limits.max_response_bytes = text(ok_response()).size();
    const auto registry = seal({&fake.api});
    auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object(), nullptr, limits);
    CHECK_EQ(result.at("status"), "ok");   // exactly at the cap
    limits.max_response_bytes -= 1;
    result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object(), nullptr, limits);
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(result.at("response"), nullptr);
    CHECK_EQ(result.at("response_error"), "too_large");
    CHECK(fake.balanced());
    fake.status = MB_PLUGIN_LIMIT;
    result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object(), nullptr, limits);
    CHECK_EQ(result.at("status"), "limit");
    CHECK_EQ(result.at("response_error"), "too_large");
    CHECK(fake.balanced());
    CHECK_EQ(fake.issued, 4);
}

TEST(plugin_invoke_empty_response) {
    Fake fake(text(descriptor("unreal.pak")));
    fake.response_returns_null = true;
    const auto registry = seal({&fake.api});
    auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(result.at("response"), nullptr);
    CHECK_EQ(result.at("response_error"), "empty");
    fake.status = MB_PLUGIN_UNSUPPORTED;   // an error status with no body is ordinary
    result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "unsupported");
    CHECK_EQ(result.at("response_error"), "empty");
    CHECK_EQ(fake.issued, 1);   // nothing but the descriptor was ever handed out
    CHECK(fake.balanced());
    fake.response_returns_null = false;   // a zero-length, non-NULL result is released once, then empty
    fake.response = "";
    fake.status = MB_PLUGIN_OK;
    result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("response_error"), "empty");
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(fake.issued, 2);
    CHECK(fake.balanced());
}

TEST(plugin_invoke_response_without_release_is_invalid_not_read) {
    Fake fake(text(descriptor("unreal.pak")), text(ok_response()));
    fake.response_without_release = true;
    const auto registry = seal({&fake.api});
    const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(result.at("response"), nullptr);
    CHECK_EQ(result.at("response_error"), "invalid_schema");
    CHECK_EQ(fake.issued, 1);   // only the descriptor was releasable
    CHECK(fake.balanced());
}

TEST(plugin_invoke_response_schema_validation) {
    const std::string sha(64, 'a');
    auto with = [&](const char* key, Json value) { Json r = ok_response(); r[key] = std::move(value); return r; };
    const Json artifact = {{"path", "a/b.bin"}, {"sha256", sha}, {"size", 5}, {"role", "container"}};
    const Json file = {{"role", "container"}, {"rel", "a/b.bin"}, {"sha256", sha}};
    const std::vector<Json> invalid = {
        Json::array(), Json("x"), Json(nullptr), Json::object(),
        {{"schema", "modbridge.plugin-response/2"}}, {{"schema", 1}},
        with("surprise", 1), with("installation", Json::object()),
        with("result", Json::array()), with("result", "x"),
        with("artifacts", Json::object()), with("artifacts", Json::array({1})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", sha}, {"size", 1}}})),                            // missing role
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", sha}, {"size", 1}, {"role", "r"}, {"x", 1}}})),   // extra key
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", sha}, {"size", -1}, {"role", "r"}}})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", sha}, {"size", 1.5}, {"role", "r"}}})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", sha}, {"size", "1"}, {"role", "r"}}})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", sha}, {"size", 1}, {"role", ""}}})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", std::string(63, 'a')}, {"size", 1}, {"role", "r"}}})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", std::string(64, 'A')}, {"size", 1}, {"role", "r"}}})),
        with("artifacts", Json::array({{{"path", "a"}, {"sha256", std::string(64, 'g')}, {"size", 1}, {"role", "r"}}})),
        with("files", Json::object()), with("files", Json::array({1})),
        with("files", Json::array({{{"role", "r"}, {"rel", "a"}}})),
        with("files", Json::array({{{"role", ""}, {"rel", "a"}, {"sha256", sha}}})),
        with("files", Json::array({{{"role", "r"}, {"rel", "a"}, {"sha256", sha}, {"x", 1}}})),
        with("conflicts", Json::object()), with("conflicts", Json::array({"c"})),
        with("reasons", Json::object()), with("reasons", Json::array({"c"})),
        with("reasons", Json::array({{{"code", "c"}}})),
        with("reasons", Json::array({{{"code", ""}, {"detail", "d"}}})),
        with("reasons", Json::array({{{"code", "c"}, {"detail", 1}}})),
        with("reasons", Json::array({{{"code", "c"}, {"detail", "d"}, {"x", 1}}})),
        with("evidence_needed", "x"), with("evidence_needed", Json::array({1})), with("evidence_needed", Json::array({""})),
    };
    Fake fake(text(descriptor("unreal.pak")));
    const auto registry = seal({&fake.api});
    for (std::size_t i = 0; i < invalid.size(); ++i) {
        for (mb_plugin_status status : {MB_PLUGIN_OK, MB_PLUGIN_UNRESOLVED}) {
            fake.status = status;
            fake.response = text(invalid[i]);
            const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
            if (result.at("response_error") != "invalid_schema") throw std::runtime_error("invalid response #" + std::to_string(i) + " was not rejected: " + fake.response);
            CHECK_EQ(result.at("response"), nullptr);
            CHECK_EQ(result.at("status"), status == MB_PLUGIN_OK ? "internal" : "unresolved");
            CHECK(fake.balanced());
        }
    }
    // A full response with every optional field is returned unchanged.
    Json full = ok_response();
    full["artifacts"] = Json::array({artifact});
    full["files"] = Json::array({file});
    full["conflicts"] = Json::array({{{"code", "overlap"}, {"components", Json::array({"a", "b"})}}});
    full["reasons"] = Json::array({{{"code", "x"}, {"detail", ""}}});
    full["evidence_needed"] = Json::array({"host.native_module"});
    fake.status = MB_PLUGIN_OK;
    fake.response = text(full);
    const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "ok");
    CHECK_EQ(result.at("response"), full);
    // The bare schema is a valid, empty response.
    fake.response = text(Json{{"schema", response_schema}});
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object()).at("status"), "ok");
    CHECK(fake.balanced());
}

TEST(plugin_invoke_relative_path_grammar) {
    const std::string sha(64, 'b');
    Fake fake(text(descriptor("unreal.pak")));
    const auto registry = seal({&fake.api});
    auto status_for = [&](const std::string& path) {
        Json r = ok_response();
        r["artifacts"] = Json::array({{{"path", path}, {"sha256", sha}, {"size", 0}, {"role", "r"}}});
        r["files"] = Json::array({{{"role", "r"}, {"rel", path}, {"sha256", sha}}});
        fake.response = text(r);
        const auto result = invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json::object());
        CHECK(fake.balanced());
        return result.at("response_error") == nullptr ? std::string("accepted") : result.at("response_error").get<std::string>();
    };
    for (const std::string& good : {std::string("a"), std::string("a/b/c.txt"), std::string("Mods/My Mod/x.pak"), std::string(".hidden"),
                                    std::string("a..b"), std::string("..."), std::string(4096, 'a')})
        CHECK_EQ(status_for(good), "accepted");
    for (const std::string& bad : {std::string(""), std::string("/abs"), std::string("a//b"), std::string("a/"), std::string("./a"),
                                   std::string("a/./b"), std::string("a/../b"), std::string(".."), std::string("."), std::string("a\\b"),
                                   std::string("c:x"), std::string("a\nb"), std::string("a") + std::string(1, '\x7f'), std::string(4097, 'a')})
        CHECK_EQ(status_for(bad), "invalid_schema");
}

TEST(plugin_invoke_rejected_before_calling_the_driver) {
    Fake fake(text(descriptor("unreal.pak")), text(ok_response()));   // operations [3,5]
    const auto registry = seal({&fake.api});
    CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "missing.driver", MB_PLUGIN_PREPARE, Json::object()));
    CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "", MB_PLUGIN_PREPARE, Json::object()));
    CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "Unreal.pak", MB_PLUGIN_PREPARE, Json::object()));
    for (mb_plugin_operation operation : {MB_PLUGIN_ANALYZE, MB_PLUGIN_ASSESS, MB_PLUGIN_VALIDATE, MB_PLUGIN_PLAN_REMOVE, MB_PLUGIN_SET_ACTIVE,
                                          MB_PLUGIN_CONFLICTS, MB_PLUGIN_RECONCILE, static_cast<mb_plugin_operation>(0),
                                          static_cast<mb_plugin_operation>(10)})
        CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "unreal.pak", operation, Json::object()));
    for (const Json& request : {Json(nullptr), Json::array(), Json("text"), Json(7), Json(true), Json::array({Json::object()})})
        CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, request));
    CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PREPARE, Json{{"bad", "\xff"}}));   // not serializable
    CHECK_EQ(fake.invoke_calls, 0);
    CHECK_EQ(invoke_plugin(*registry, "unreal.pak", MB_PLUGIN_PLAN_INSTALL, Json::object()).at("status"), "ok");
    CHECK_EQ(fake.invoke_calls, 1);
    CHECK(fake.balanced());
}

// -------------------------------------------------------- planner interplay
Json entry(const std::string& path, const std::string& kind) {
    return {{"path", path}, {"kind", kind}, {"sha256", "source-sha"}, {"requirements", Json::array()}};
}
Json inventory_of(Json entries) { return {{"entries", entries}, {"dependencies", Json::array()}}; }
Json target() { return {{"fingerprints", {{"executable", "target-sha"}}}, {"capabilities", Json::array()}}; }

struct DriverSet {
    Fake pak{text(descriptor("unreal.pak", "transformer", Json::array({"unreal_pak"}), Json::array({3, 5, 6})))};
    Fake ini{text(descriptor("unreal.ini", "transformer", Json::array({"unreal_ini"}), Json::array({3})))};
    Fake iostore{[] {
        auto d = descriptor("unreal.iostore", "transformer", Json::array({"unreal_iostore"}), Json::array({3, 5, 6, 9}));
        d["capabilities"] = Json::array({"translation.unreal_iostore"});
        d["target_fingerprints"] = Json::array({"executable", "base_utoc_sha256"});
        return text(d);
    }()};
    std::vector<const mb_plugin_api*> all() { return {&pak.api, &ini.api, &iostore.api, ue4ss_lua_driver()}; }
};

const Json* unresolved_for(const Json& plan, const std::string& component, const std::string& code) {
    for (const auto& item : plan.at("unresolved"))
        if (item.at("component") == component && item.at("code") == code) return &item;
    return nullptr;
}

TEST(plugin_describe_to_planner_golden) {
    DriverSet drivers;
    const auto registry = seal(drivers.all());
    const auto backends = planner_backends(*registry);
    CHECK_EQ(backends.size(), 4u);
    const auto inventory = inventory_of(Json::array({entry("mods/a.pak", "unreal_pak"), entry("mods/b.ini", "unreal_ini"),
                                                     entry("scripts/main.lua", "lua"), entry("mods/c.utoc", "unreal_iostore")}));
    // The generated backends are accepted unchanged: no exception, no unsupported_backend_constraint.
    const auto plan = build_plan(inventory, target(), backends);
    CHECK_EQ(plan.at("status"), "unresolved");
    // Transformer drivers become eligible steps.
    std::vector<std::string> eligible;
    for (const auto& step : plan.at("eligible_steps")) {
        eligible.push_back(step.at("component").get<std::string>() + "=" + step.at("backend_id").get<std::string>() + "/" + step.at("mode").get<std::string>());
    }
    std::sort(eligible.begin(), eligible.end());
    CHECK_EQ(Json(eligible), Json::array({"mods/a.pak=unreal.pak/transformer", "mods/b.ini=unreal.ini/transformer"}));
    // The ue4ss.lua analyzer is matched by kind and rejected only as analyzer_only.
    const auto* lua = unresolved_for(plan, "scripts/main.lua", "missing_backend");
    CHECK(lua != nullptr);
    CHECK_EQ(lua->at("candidates"), Json::array({{{"backend_id", "ue4ss.lua"}, {"code", "analyzer_only"}}}));
    // A string capability requirement is normalised by the planner and reported as missing, not malformed.
    const auto* iostore = unresolved_for(plan, "mods/c.utoc", "missing_backend");
    CHECK(iostore != nullptr);
    CHECK_EQ(iostore->at("candidates").size(), 1u);
    CHECK_EQ(iostore->at("candidates")[0].at("backend_id"), "unreal.iostore");
    CHECK_EQ(iostore->at("candidates")[0].at("missing_fingerprints"), Json::array({"base_utoc_sha256"}));
    CHECK_EQ(iostore->at("candidates")[0].at("requirements")[0].at("code"), "missing_capability");
    CHECK_EQ(iostore->at("candidates")[0].at("requirements")[0].at("requirement"), (Json{{"id", "translation.unreal_iostore"}}));
    CHECK(unresolved_for(plan, "mods/a.pak", "missing_backend") == nullptr);
    // A lua-only package: the one component is unresolved exactly as analyzer_only, and the plan is deterministic.
    const auto lua_only = build_plan(inventory_of(Json::array({entry("scripts/main.lua", "lua")})), target(), backends);
    CHECK_EQ(lua_only.at("status"), "unresolved");
    CHECK(lua_only.at("eligible_steps").empty());
    CHECK_EQ(lua_only.at("unresolved").size(), 1u);
    CHECK_EQ(lua_only.at("unresolved")[0],
             (Json{{"component", "scripts/main.lua"}, {"code", "missing_backend"},
                   {"candidates", Json::array({{{"backend_id", "ue4ss.lua"}, {"code", "analyzer_only"}}})}}));
    CHECK_EQ(build_plan(inventory_of(Json::array({entry("scripts/main.lua", "lua")})), target(), planner_backends(*registry)), lua_only);
}

TEST(plugin_planner_backends_exact_fields) {
    DriverSet drivers;
    const auto registry = seal(drivers.all());
    const auto backends = planner_backends(*registry);
    const std::set<std::string> fields = {"id", "version", "mode", "kinds", "capabilities", "target_fingerprints", "metadata"};
    const auto descriptors = plugin_descriptors(*registry);
    CHECK_EQ(backends.size(), descriptors.size());
    for (std::size_t i = 0; i < backends.size(); ++i) {
        std::set<std::string> keys;
        for (auto it = backends[i].begin(); it != backends[i].end(); ++it) keys.insert(it.key());
        CHECK(keys == fields);
        for (const auto& field : fields) CHECK_EQ(backends[i].at(field), descriptors[i].at(field));
        CHECK_EQ(descriptors[i].at("schema"), "modbridge.plugin-driver/1");
    }
}

TEST(plugin_same_backends_for_any_driver_order) {
    DriverSet drivers;
    auto order = drivers.all();
    std::sort(order.begin(), order.end());
    const auto baseline = seal(order);
    const auto expected_backends = planner_backends(*baseline);
    const auto expected_descriptors = plugin_descriptors(*baseline);
    std::vector<std::string> ids;
    for (const auto& backend : expected_backends) ids.push_back(backend.at("id").get<std::string>());
    CHECK(std::is_sorted(ids.begin(), ids.end()));
    CHECK_EQ(Json(ids), Json::array({"ue4ss.lua", "unreal.ini", "unreal.iostore", "unreal.pak"}));
    const auto inventory = inventory_of(Json::array({entry("mods/a.pak", "unreal_pak"), entry("scripts/main.lua", "lua")}));
    const auto expected_plan = build_plan(inventory, target(), expected_backends);
    int permutations = 0;
    do {
        const auto registry = seal(order, 9);
        CHECK_EQ(planner_backends(*registry), expected_backends);
        CHECK_EQ(plugin_descriptors(*registry), expected_descriptors);
        CHECK_EQ(build_plan(inventory, target(), planner_backends(*registry)), expected_plan);
        ++permutations;
    } while (std::next_permutation(order.begin(), order.end()));
    CHECK_EQ(permutations, 24);
}

TEST(plugin_same_plan_across_mod_ids) {
    DriverSet drivers;
    const auto registry = seal(drivers.all());
    auto inventory = inventory_of(Json::array({entry("mods/a.pak", "unreal_pak"), entry("scripts/main.lua", "lua")}));
    inventory["provider_mod_id"] = "first";
    const auto a = build_plan(inventory, target(), planner_backends(*registry));
    inventory["provider_mod_id"] = "unfamiliar";
    CHECK_EQ(build_plan(inventory, target(), planner_backends(*registry)), a);
}

// ---------------------------------------------------- the ue4ss.lua driver
struct Direct {
    mb_plugin_status status = MB_PLUGIN_INTERNAL;
    std::string body;
    bool releasable = false;
};
Direct call_direct(const mb_plugin_api* api, mb_plugin_operation operation, const std::string& request) {
    mb_owned_json out{nullptr, 0, nullptr, nullptr};
    Direct direct;
    direct.status = api->invoke(api->handle, operation, mb_json_view{request.data(), request.size()}, nullptr, &out);
    direct.releasable = out.release != nullptr;
    if (out.bytes != nullptr) {
        direct.body.assign(out.bytes, out.length);
        if (out.release != nullptr) out.release(out.owner, out.bytes, out.length);   // exactly once
    }
    return direct;
}

Json build(const char* version, Json pc_build) { return {{"console_content_version", version}, {"pc_build", pc_build}}; }
Json pc(const char* label, std::uint64_t released) { return {{"label", label}, {"released_utc", released}}; }
Json profile() {
    return {{"schema", "modbridge.game-profile/1"}, {"title_id", "PPSA33286"}, {"nexus_domain", "silenthilltownfall"},
            {"engine", {{"family", "unreal"}, {"version", "5.6"}}},
            {"builds", Json::array({build("01.000.000", pc("1.00", 1000)), build("02.000.000", pc("1.01", 2000)),
                                    build("04.000.000", pc("1.03", 4000))})}};
}
Json content_index() {
    return {{"console_content_version", "02.000.000"},
            {"complete", {{"assets", true}, {"classes", false}, {"functions", false}}},
            {"assets", Json::array({"/Game/A/X", "/Game/B/Y"})},
            {"classes", Json::array({"/Script/Engine.Actor"})},
            {"functions", Json::array({"/Script/Engine.Actor:K2_DestroyActor"})}};
}
Json nexus_file(std::uint64_t id, std::uint64_t uploaded) {
    return {{"file_id", id}, {"name", "n"}, {"file_name", "f"}, {"version", "1"}, {"description", ""}, {"size_kb", 1},
            {"source_url", "u"}, {"uploaded_timestamp", uploaded}};
}
const char* good_script = "local a = \"/Game/B/Y\"\nlocal c = \"/Script/Engine.Actor:K2_DestroyActor\"\nlocal d = FindFirstOf(\"Actor\")\n";

Json assess_request(const Json& references, bool with_index = true) {
    Json request = {{"profile", profile()}, {"param", {{"contentVersion", "02.000.000"}}},
                    {"files", Json::array({nexus_file(1, 2500)})}, {"references", references}};
    if (with_index) request["content_index"] = content_index();
    return request;
}

TEST(plugin_ue4ss_driver_descriptor_and_identity) {
    const auto* api = ue4ss_lua_driver();
    CHECK(api != nullptr);
    CHECK(api == ue4ss_lua_driver());   // one static instance
    CHECK_EQ(api->abi_version, MB_PLUGIN_ABI_VERSION);
    CHECK(api->struct_size >= sizeof(mb_plugin_api));
    CHECK(api->describe != nullptr && api->invoke != nullptr);
    mb_owned_json owned = api->describe(api->handle);
    CHECK(owned.bytes != nullptr && owned.release != nullptr);
    const Json expected = {{"schema", "modbridge.plugin-driver/1"}, {"id", "ue4ss.lua"}, {"version", "1.0.0"}, {"mode", "analyzer"},
                           {"kinds", Json::array({"lua"})}, {"capabilities", Json::array()}, {"target_fingerprints", Json::array()},
                           {"metadata", {{"type", "script"}, {"operations", Json::array({1, 2})}, {"engines", Json::array({"unreal"})},
                                         {"roles", Json::array({"script"})}, {"activation", "runtime"}}}};
    CHECK_EQ(Json::parse(std::string(owned.bytes, owned.length)), expected);
    owned.release(owned.owner, owned.bytes, owned.length);
    const auto registry = seal({api});
    CHECK_EQ(plugin_descriptors(*registry), Json::array({expected}));
    const auto backends = planner_backends(*registry);
    CHECK_EQ(backends.size(), 1u);
    CHECK(!backends[0].contains("schema"));
}

TEST(plugin_ue4ss_analyze_end_to_end) {
    const auto registry = seal({ue4ss_lua_driver()});
    const std::string script = good_script;
    const auto result = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", script}});
    CHECK_EQ(result.at("status"), "ok");
    CHECK_EQ(result.at("response_error"), nullptr);
    CHECK_EQ(result.at("response"), (Json{{"schema", response_schema}, {"result", extract_lua_references(script)}}));
    CHECK_EQ(result.at("response").at("result").at("assets"), Json::array({"/Game/B/Y"}));
    CHECK_EQ(result.at("response").at("result").at("functions"), Json::array({"/Script/Engine.Actor:K2_DestroyActor"}));
    CHECK_EQ(result.at("response").at("result").at("short_class_names"), Json::array({"Actor"}));
    CHECK_EQ(result.at("response").at("result").at("dynamic"), false);
    // Deterministic, an empty script is fine, and dynamic lookups are reported.
    CHECK_EQ(invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", script}}), result);
    CHECK_EQ(invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", ""}}).at("response").at("result"),
             extract_lua_references(""));
    CHECK_EQ(invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", "FindFirstOf(name)"}}).at("response").at("result").at("dynamic"), true);
    // The script is at most 4 MiB: exactly 4 MiB is accepted.
    const std::string big(4u * 1024u * 1024u, ' ');
    CHECK_EQ(invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", big}}).at("status"), "ok");
}

TEST(plugin_ue4ss_assess_end_to_end) {
    const auto registry = seal({ue4ss_lua_driver()});
    const auto analyzed = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", good_script}});
    const Json references = analyzed.at("response").at("result");   // ANALYZE feeds ASSESS
    const auto result = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ASSESS, assess_request(references));
    CHECK_EQ(result.at("status"), "ok");
    CHECK_EQ(result.at("response_error"), nullptr);
    const auto& verdict = result.at("response").at("result");
    CHECK_EQ(verdict.at("verdict"), "compatible");
    CHECK_EQ(verdict.at("content").at("status"), "all_present");
    CHECK_EQ(result.at("response").at("schema"), response_schema);
    // Exactly the core's verdict for the profile with the index attached.
    CHECK_EQ(verdict, assess_mod_compatibility(attach_content_index(profile(), content_index()), {{"contentVersion", "02.000.000"}},
                                                Json::array({nexus_file(1, 2500)}), references));
    // Without a content index there is nothing to verify against: the verdict stays unknown.
    const auto no_index = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ASSESS, assess_request(references, false));
    CHECK_EQ(no_index.at("status"), "ok");
    CHECK_EQ(no_index.at("response").at("result").at("verdict"), "unknown");
    CHECK_EQ(no_index.at("response").at("result").at("reasons")[0].at("code"), "no_index");
    // A reference the index cannot find is reported, not hidden.
    const auto missing_refs = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", "local a = \"/Game/Nope/Z\""}}).at("response").at("result");
    CHECK_EQ(invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ASSESS, assess_request(missing_refs)).at("response").at("result").at("verdict"), "incompatible");
    // An explicit file id reaches the core.
    auto explicit_request = assess_request(references);
    explicit_request["file_id"] = 99;
    const auto explicit_result = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ASSESS, explicit_request);
    CHECK_EQ(explicit_result.at("response").at("result"),
             assess_mod_compatibility(attach_content_index(profile(), content_index()), {{"contentVersion", "02.000.000"}},
                                      Json::array({nexus_file(1, 2500)}), references, 99));
    CHECK_EQ(explicit_result.at("response").at("result").at("file_selection").at("status"), "explicit_file_not_found");
}

TEST(plugin_ue4ss_bad_requests) {
    const auto* api = ue4ss_lua_driver();
    auto bad = [&](mb_plugin_operation operation, const std::string& request, const char* code) {
        const auto direct = call_direct(api, operation, request);
        CHECK_EQ(direct.status, MB_PLUGIN_BAD_REQUEST);
        CHECK(direct.releasable);
        const auto body = Json::parse(direct.body);
        CHECK_EQ(body.at("schema"), response_schema);
        CHECK_EQ(body.at("reasons").size(), 1u);
        CHECK_EQ(body.at("reasons")[0].at("code"), code);
        CHECK(body.at("reasons")[0].at("detail").is_string());
        CHECK(!body.contains("result"));
        return body;
    };
    // Malformed Lua: the lexer's own complaint, mapped to invalid_lua.
    const auto malformed = bad(MB_PLUGIN_ANALYZE, text(Json{{"script", "local s = \"abc"}}), "invalid_lua");
    CHECK(malformed.at("reasons")[0].at("detail").get<std::string>().find("unterminated") != std::string::npos);
    bad(MB_PLUGIN_ANALYZE, text(Json{{"script", "local s = [[never closed"}}), "invalid_lua");
    bad(MB_PLUGIN_ANALYZE, text(Json{{"script", "local s = \"\\q\""}}), "invalid_lua");
    // Not an object, not JSON, empty.
    for (const char* request : {"[]", "42", "\"s\"", "null", "true"}) bad(MB_PLUGIN_ANALYZE, request, "invalid_request");
    for (const char* request : {"{", "garbage", "{\"script\":\"x\"} trailing", "{\"script\":\"\xff\"}"}) bad(MB_PLUGIN_ANALYZE, request, "invalid_json");
    bad(MB_PLUGIN_ANALYZE, "", "invalid_request");
    {
        mb_owned_json out{nullptr, 0, nullptr, nullptr};
        CHECK_EQ(api->invoke(api->handle, MB_PLUGIN_ANALYZE, mb_json_view{nullptr, 0}, nullptr, &out), MB_PLUGIN_BAD_REQUEST);
        CHECK(out.bytes != nullptr);
        out.release(out.owner, out.bytes, out.length);
        CHECK_EQ(api->invoke(api->handle, MB_PLUGIN_ANALYZE, mb_json_view{nullptr, 0}, nullptr, nullptr), MB_PLUGIN_BAD_REQUEST);
    }
    // Wrong shape.
    bad(MB_PLUGIN_ANALYZE, "{}", "invalid_request");
    bad(MB_PLUGIN_ANALYZE, "{\"script\":1}", "invalid_request");
    bad(MB_PLUGIN_ANALYZE, "{\"script\":null}", "invalid_request");
    bad(MB_PLUGIN_ANALYZE, "{\"script\":\"x\",\"extra\":1}", "invalid_request");
    bad(MB_PLUGIN_ANALYZE, text(Json{{"script", std::string(4u * 1024u * 1024u + 1u, ' ')}}), "script_too_large");
    bad(MB_PLUGIN_ANALYZE, std::string("{\"script\":\"x\",\"deep\":") + std::string(200, '[') + std::string(200, ']') + "}", "request_too_large");
    // ASSESS: every defect is a reasoned BAD_REQUEST, never an exception.
    const Json references = extract_lua_references(good_script);
    auto request = assess_request(references);
    bad(MB_PLUGIN_ASSESS, "{}", "invalid_request");
    for (const char* key : {"profile", "param", "files", "references"}) {
        auto broken = request;
        broken.erase(key);
        bad(MB_PLUGIN_ASSESS, text(broken), "invalid_request");
    }
    auto extra = request; extra["extra"] = 1;
    bad(MB_PLUGIN_ASSESS, text(extra), "invalid_request");
    auto no_profile = request; no_profile["profile"] = Json::object();
    bad(MB_PLUGIN_ASSESS, text(no_profile), "invalid_request");
    auto profile_text = request; profile_text["profile"] = "x";
    bad(MB_PLUGIN_ASSESS, text(profile_text), "invalid_request");
    auto files_object = request; files_object["files"] = Json::object();
    bad(MB_PLUGIN_ASSESS, text(files_object), "invalid_request");
    auto references_array = request; references_array["references"] = Json::array();
    bad(MB_PLUGIN_ASSESS, text(references_array), "invalid_request");
    auto index_for_unlisted_build = request; index_for_unlisted_build["content_index"]["console_content_version"] = "09.000.000";
    bad(MB_PLUGIN_ASSESS, text(index_for_unlisted_build), "invalid_request");
    auto index_text = request; index_text["content_index"] = "x";
    bad(MB_PLUGIN_ASSESS, text(index_text), "invalid_request");
    for (const Json& file_id : {Json(-1), Json(1.5), Json("1"), Json(nullptr)}) {
        auto bad_id = request; bad_id["file_id"] = file_id;
        bad(MB_PLUGIN_ASSESS, text(bad_id), "invalid_request");
    }
    // Through the registry the same answers are valid, validated responses with status bad_request.
    const auto registry = seal({api});
    auto result = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json{{"script", "local s = \"abc"}});
    CHECK_EQ(result.at("status"), "bad_request");
    CHECK_EQ(result.at("response_error"), nullptr);
    CHECK_EQ(result.at("response").at("reasons")[0].at("code"), "invalid_lua");
    result = invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ASSESS, no_profile);
    CHECK_EQ(result.at("status"), "bad_request");
    CHECK_EQ(result.at("response_error"), nullptr);
    CHECK_EQ(result.at("response").at("reasons")[0].at("code"), "invalid_request");
    // A non-object request is refused by the host before the driver sees it.
    CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "ue4ss.lua", MB_PLUGIN_ANALYZE, Json::array()));
}

TEST(plugin_ue4ss_unsupported_operations) {
    const auto* api = ue4ss_lua_driver();
    for (mb_plugin_operation operation : {MB_PLUGIN_PREPARE, MB_PLUGIN_VALIDATE, MB_PLUGIN_PLAN_INSTALL, MB_PLUGIN_PLAN_REMOVE,
                                          MB_PLUGIN_SET_ACTIVE, MB_PLUGIN_CONFLICTS, MB_PLUGIN_RECONCILE}) {
        const auto direct = call_direct(api, operation, "{}");
        CHECK_EQ(direct.status, MB_PLUGIN_UNSUPPORTED);
        CHECK_EQ(Json::parse(direct.body).at("reasons")[0].at("code"), "unsupported_operation");
    }
    // The registry never even forwards them: the descriptor lists operations 1 and 2 only.
    const auto registry = seal({api});
    for (mb_plugin_operation operation : {MB_PLUGIN_PREPARE, MB_PLUGIN_SET_ACTIVE, MB_PLUGIN_RECONCILE})
        CHECK_PLUGIN_THROWS(invoke_plugin(*registry, "ue4ss.lua", operation, Json::object()));
    // Services are optional and ignored.
    mb_plugin_host_services services{};
    const std::string request = text(Json{{"script", "x = 1"}});
    mb_owned_json out{nullptr, 0, nullptr, nullptr};
    CHECK_EQ(api->invoke(api->handle, MB_PLUGIN_ANALYZE, mb_json_view{request.data(), request.size()}, &services, &out), MB_PLUGIN_OK);
    out.release(out.owner, out.bytes, out.length);
}

// --------------------------------------------------------- ABI-1 adapter
// A fake ABI-1 backend: the host's mb_owned_json release rules are the same.
struct FakeV1 {
    struct Block { FakeV1* fake; std::string data; };
    std::string descriptor;
    std::string response;
    bool response_null = false;
    bool response_empty_non_null = false;
    int describe_calls = 0, invoke_calls = 0, issued = 0, released = 0;
    mb_operation last_operation = MB_ANALYZE;
    std::string last_request;
    mb_backend_api api{};

    explicit FakeV1(std::string descriptor_text, std::string response_text = "") : descriptor(std::move(descriptor_text)), response(std::move(response_text)) {
        api.abi_version = MB_BACKEND_ABI_VERSION;
        api.struct_size = sizeof(mb_backend_api);
        api.describe = &FakeV1::describe;
        api.invoke = &FakeV1::invoke;
        api.destroy = nullptr;
    }
    FakeV1(const FakeV1&) = delete;
    FakeV1& operator=(const FakeV1&) = delete;
    mb_backend_handle* handle() { return reinterpret_cast<mb_backend_handle*>(this); }
    bool balanced() const { return issued == released; }
    mb_owned_json make(const std::string& data) {
        auto* block = new Block{this, data};
        ++issued;
        return mb_owned_json{block->data.data(), block->data.size(), block, &FakeV1::release};
    }
    static void release(void* owner, const char*, std::size_t) {
        auto* block = static_cast<Block*>(owner);
        ++block->fake->released;
        delete block;
    }
    static FakeV1* self(mb_backend_handle* handle) { return reinterpret_cast<FakeV1*>(handle); }
    static mb_owned_json describe(mb_backend_handle* handle) {
        auto* fake = self(handle);
        ++fake->describe_calls;
        return fake->make(fake->descriptor);
    }
    static mb_owned_json invoke(mb_backend_handle* handle, mb_operation operation, mb_json_view request) {
        auto* fake = self(handle);
        ++fake->invoke_calls;
        fake->last_operation = operation;
        fake->last_request.assign(request.bytes, request.length);
        if (fake->response_null) return mb_owned_json{nullptr, 0, nullptr, nullptr};
        return fake->make(fake->response_empty_non_null ? std::string() : fake->response);
    }
};

TEST(plugin_v1_adapter_describe_passthrough_and_registry) {
    const auto v1_descriptor = text(descriptor("legacy.backend", "transformer", Json::array({"legacy_kind"}), Json::array({1, 2, 3, 4})));
    FakeV1 backend(v1_descriptor, text(ok_response()));
    BackendV1Adapter adapter(&backend.api, backend.handle());
    const auto* api = adapter.api();
    CHECK_EQ(api->abi_version, MB_PLUGIN_ABI_VERSION);
    CHECK_EQ(api->struct_size, sizeof(mb_plugin_api));
    CHECK(api->handle != nullptr);
    CHECK(api->describe != nullptr && api->invoke != nullptr);
    CHECK(api->destroy == nullptr);   // the caller still owns and destroys the v1 backend
    CHECK(adapter.api() == api);
    // describe passes the v1 bytes through unchanged, released through the v1 callback.
    mb_owned_json owned = api->describe(api->handle);
    CHECK_EQ(std::string(owned.bytes, owned.length), v1_descriptor);
    CHECK(owned.release != nullptr);
    owned.release(owned.owner, owned.bytes, owned.length);
    CHECK(backend.balanced());
    // Sealed as an ordinary driver; ops 1-4 reach the backend untouched.
    const auto registry = seal({api});
    CHECK_EQ(plugin_descriptors(*registry)[0].at("id"), "legacy.backend");
    const Json request = {{"component", "x"}, {"n", 1}};
    for (std::uint32_t op = 1; op <= 4; ++op) {
        const auto result = invoke_plugin(*registry, "legacy.backend", static_cast<mb_plugin_operation>(op), request);
        CHECK_EQ(result.at("status"), "ok");
        CHECK_EQ(result.at("response"), ok_response());
        CHECK_EQ(static_cast<std::uint32_t>(backend.last_operation), op);
        CHECK_EQ(backend.last_request, request.dump());
    }
    CHECK_EQ(backend.invoke_calls, 4);
    CHECK(backend.balanced());
    // The adapter feeds the planner like any other driver.
    const auto plan = build_plan(inventory_of(Json::array({entry("x", "legacy_kind")})), target(), planner_backends(*registry));
    CHECK_EQ(plan.at("eligible_steps").size(), 1u);
}

TEST(plugin_v1_adapter_operations_above_validate_are_unsupported) {
    FakeV1 backend(text(descriptor("legacy.backend", "transformer", Json::array({"legacy_kind"}), Json::array({3, 5, 6, 7, 9}))), text(ok_response()));
    BackendV1Adapter adapter(&backend.api, backend.handle());
    const auto registry = seal({adapter.api()});
    for (mb_plugin_operation operation : {MB_PLUGIN_PLAN_INSTALL, MB_PLUGIN_PLAN_REMOVE, MB_PLUGIN_SET_ACTIVE, MB_PLUGIN_RECONCILE}) {
        const auto result = invoke_plugin(*registry, "legacy.backend", operation, Json::object());
        CHECK_EQ(result.at("status"), "unsupported");
        CHECK_EQ(result.at("response"), nullptr);
        CHECK_EQ(result.at("response_error"), "empty");
    }
    CHECK_EQ(backend.invoke_calls, 0);   // never called
    // Directly as well: any operation outside 1..4, including out-of-range numbers, and a NULL response slot.
    const auto* api = adapter.api();
    for (std::uint32_t op : {0u, 5u, 6u, 7u, 8u, 9u}) {
        mb_owned_json out{reinterpret_cast<const char*>(&op), 1, nullptr, nullptr};   // the adapter must reset it
        CHECK_EQ(api->invoke(api->handle, static_cast<mb_plugin_operation>(op), mb_json_view{"{}", 2}, nullptr, &out), MB_PLUGIN_UNSUPPORTED);
        CHECK(out.bytes == nullptr && out.length == 0 && out.release == nullptr);
    }
    CHECK_EQ(api->invoke(api->handle, MB_PLUGIN_ANALYZE, mb_json_view{"{}", 2}, nullptr, nullptr), MB_PLUGIN_BAD_REQUEST);
    CHECK_EQ(backend.invoke_calls, 0);
    CHECK(backend.balanced());
}

TEST(plugin_v1_adapter_empty_response_is_internal) {
    FakeV1 backend(text(descriptor("legacy.backend", "transformer", Json::array({"legacy_kind"}), Json::array({3}))));
    backend.response_null = true;
    BackendV1Adapter adapter(&backend.api, backend.handle());
    const auto registry = seal({adapter.api()});
    auto result = invoke_plugin(*registry, "legacy.backend", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(result.at("response"), nullptr);
    CHECK_EQ(result.at("response_error"), "empty");
    // Direct: an empty v1 result maps to MB_PLUGIN_INTERNAL.
    mb_owned_json out{nullptr, 0, nullptr, nullptr};
    CHECK_EQ(adapter.api()->invoke(adapter.api()->handle, MB_PLUGIN_PREPARE, mb_json_view{"{}", 2}, nullptr, &out), MB_PLUGIN_INTERNAL);
    CHECK(out.bytes == nullptr);
    // A zero-length, non-NULL v1 result is also empty, and is still released by the host exactly once.
    backend.response_null = false;
    backend.response_empty_non_null = true;
    result = invoke_plugin(*registry, "legacy.backend", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(result.at("response_error"), "empty");
    CHECK_EQ(backend.invoke_calls, 3);
    CHECK(backend.balanced());
    // A non-empty but invalid v1 answer is released once and is an internal error, not a success.
    backend.response_empty_non_null = false;
    backend.response = "not json";
    result = invoke_plugin(*registry, "legacy.backend", MB_PLUGIN_PREPARE, Json::object());
    CHECK_EQ(result.at("status"), "internal");
    CHECK_EQ(result.at("response_error"), "invalid_json");
    CHECK(backend.balanced());
}

TEST(plugin_v1_adapter_construction_and_descriptor_validation) {
    FakeV1 backend(text(descriptor("legacy.backend", "transformer", Json::array({"legacy_kind"}), Json::array({3}))));
    CHECK_PLUGIN_THROWS(BackendV1Adapter(nullptr, backend.handle()));
    auto wrong_abi = backend.api; wrong_abi.abi_version = 2;
    auto short_struct = backend.api; short_struct.struct_size = sizeof(mb_backend_api) - 1;
    auto no_describe = backend.api; no_describe.describe = nullptr;
    auto no_invoke = backend.api; no_invoke.invoke = nullptr;
    for (const mb_backend_api* bad : {&wrong_abi, &short_struct, &no_describe, &no_invoke}) CHECK_PLUGIN_THROWS(BackendV1Adapter(bad, backend.handle()));
    // A v1 descriptor that is not a modbridge.plugin-driver/1 record rejects the registry, after being released once.
    FakeV1 legacy_shape(R"({"id":"legacy","version":"1","operations":[1,2,3,4]})");
    BackendV1Adapter adapter(&legacy_shape.api, legacy_shape.handle());
    CHECK_PLUGIN_THROWS(seal({adapter.api()}));
    CHECK_EQ(legacy_shape.describe_calls, 1);
    CHECK(legacy_shape.balanced());
}

} // namespace

TEST(plugin_optional_planner_fields_pass_through) {
    auto with_cycles = descriptor("cycle.driver");
    with_cycles["supports_reference_cycles"] = true;
    with_cycles["provenance"] = {{"source", "built-in"}};
    Fake fake(text(with_cycles)), plain(text(descriptor("plain.driver")));
    const auto registry = seal({&fake.api, &plain.api});
    const auto backends = planner_backends(*registry);
    CHECK_EQ(backends[0].at("id"), "cycle.driver");
    CHECK_EQ(backends[0].at("supports_reference_cycles"), true);
    CHECK_EQ(backends[0].at("provenance"), Json({{"source", "built-in"}}));
    CHECK(!backends[1].contains("supports_reference_cycles"));
    CHECK(!backends[1].contains("provenance"));
    // The planner accepts the generated backends unchanged.
    (void)build_plan(inventory_of(Json::array({entry("mods/a.pak", "unreal_pak")})), target(), backends);
    // Wrong types reject the whole registry.
    auto bad_cycles = descriptor("bad.cycles");
    bad_cycles["supports_reference_cycles"] = "yes";
    Fake bad1(text(bad_cycles));
    expect_rejected(bad1);
    auto bad_provenance = descriptor("bad.provenance");
    bad_provenance["provenance"] = Json::array();
    Fake bad2(text(bad_provenance));
    expect_rejected(bad2);
}
