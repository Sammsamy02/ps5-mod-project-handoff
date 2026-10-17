#include "test.hpp"
#include "modbridge/symbol_map.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
using namespace modbridge;
namespace {
using Fns = std::vector<std::pair<std::string, std::string>>;

const std::string sha_pc(64, 'a');
const std::string sha_ps5(64, 'b');

Json dump(const std::string& platform, const std::string& kind, const Fns& fns,
          const std::string& engine = "5.6") {
    Json functions = Json::array();
    for (const auto& f : fns) functions.push_back({{"name", f.first}, {"address", f.second}});
    return {{"schema", "modbridge.reflection-dump/1"}, {"platform", platform},
            {"image", {{"sha256", platform == "pc" ? sha_pc : sha_ps5}, {"address_kind", kind}}},
            {"engine_version", engine}, {"functions", functions}};
}
Json pc_dump(const Fns& fns, const std::string& engine = "5.6") { return dump("pc", "rva", fns, engine); }
Json ps5_dump(const Fns& fns, const std::string& engine = "5.6") { return dump("ps5", "vaddr", fns, engine); }
Json identity() {
    return {{"title_id", "PPSA33286"}, {"console_content_version", "02.003.000"}, {"pc_build", "1.03"}};
}
Json build(const Fns& pc, const Fns& ps5) { return build_symbol_map(pc_dump(pc), ps5_dump(ps5), identity()); }

const std::string A = "/Script/Engine.Actor:K2_DestroyActor";
const std::string B = "/Script/Engine.Actor:GetActorLocation";
const std::string C = "/Game/Blueprints/Hero_C:Jump";
const std::string D = "/Script/Engine.Pawn:Possess";

// A valid map with explicit entries, bypassing build.
std::string hex(std::uint64_t v) {
    static const char digits[] = "0123456789abcdef";
    std::string out;
    do { out.insert(out.begin(), digits[v & 0xf]); v >>= 4; } while (v != 0);
    return "0x" + out;
}

Json map_of(const Fns& entries /* name, pc */, const Json& stats = Json()) {
    Json list = Json::array();
    for (const auto& e : entries) list.push_back({{"name", e.first}, {"pc_rva", e.second}, {"ps5_vaddr", "0x9000"}});
    Json out = {{"schema", "modbridge.symbol-map/1"}, {"title_id", "PPSA33286"},
                {"console_content_version", "02.003.000"}, {"pc_build", "1.03"},
                {"engine_version", "5.6"}, {"pc_image_sha256", sha_pc}, {"ps5_image_sha256", sha_ps5},
                {"entries", list}};
    out["stats"] = stats.is_null()
        ? Json{{"matched", entries.size()}, {"pc_only", 0}, {"ps5_only", 0}, {"ambiguous_names", 0}, {"folded_pc_addresses", 0}}
        : stats;
    return out;
}
}

TEST(symbol_map_matching_and_stats) {
    auto map = build({{A, "0x1000"}, {B, "0x2000"}, {C, "0x3000"}},
                     {{C, "0xC000"}, {A, "0xA000"}, {B, "0xB000"}});
    CHECK_EQ(map["schema"], "modbridge.symbol-map/1");
    CHECK_EQ(map["title_id"], "PPSA33286");
    CHECK_EQ(map["console_content_version"], "02.003.000");
    CHECK_EQ(map["pc_build"], "1.03");
    CHECK_EQ(map["engine_version"], "5.6");
    CHECK_EQ(map["pc_image_sha256"], sha_pc);
    CHECK_EQ(map["ps5_image_sha256"], sha_ps5);
    CHECK_EQ(map["entries"].size(), 3u);
    // Sorted by name: "/Game/..." < "/Script/Engine.Actor:GetActorLocation" < "...:K2_DestroyActor".
    CHECK_EQ(map["entries"][0]["name"], C);
    CHECK_EQ(map["entries"][0]["pc_rva"], "0x3000");
    CHECK_EQ(map["entries"][0]["ps5_vaddr"], "0xc000");
    CHECK_EQ(map["entries"][1]["name"], B);
    CHECK_EQ(map["entries"][2]["name"], A);
    CHECK_EQ(map["entries"][2]["ps5_vaddr"], "0xa000");
    CHECK_EQ(map["stats"]["matched"], 3u);
    CHECK_EQ(map["stats"]["pc_only"], 0u);
    CHECK_EQ(map["stats"]["ps5_only"], 0u);
    CHECK_EQ(map["stats"]["ambiguous_names"], 0u);
    CHECK_EQ(map["stats"]["folded_pc_addresses"], 0u);
    CHECK_EQ(validate_symbol_map(map), map);
}

TEST(symbol_map_pc_only_and_ps5_only) {
    auto map = build({{A, "0x1000"}, {B, "0x2000"}, {C, "0x3000"}}, {{A, "0xa000"}, {D, "0xd000"}});
    CHECK_EQ(map["entries"].size(), 1u);
    CHECK_EQ(map["entries"][0]["name"], A);
    CHECK_EQ(map["stats"]["matched"], 1u);
    CHECK_EQ(map["stats"]["pc_only"], 2u);
    CHECK_EQ(map["stats"]["ps5_only"], 1u);
    CHECK_EQ(map["stats"]["ambiguous_names"], 0u);
    auto empty = build({}, {});
    CHECK_EQ(empty["entries"].size(), 0u);
    CHECK_EQ(empty["stats"]["matched"], 0u);
    CHECK_EQ(translate_pc_address(empty, 0x1000)["status"], "not_found");
    CHECK_EQ(translate_function_name(empty, A)["status"], "not_found");
}

TEST(symbol_map_duplicate_names_excluded) {
    // A duplicated in PC, B duplicated in PS5, C duplicated in both (counted once); D unique.
    auto map = build({{A, "0x1000"}, {A, "0x1100"}, {B, "0x2000"}, {C, "0x3000"}, {C, "0x3100"}, {D, "0x4000"}},
                     {{A, "0xa000"}, {B, "0xb000"}, {B, "0xb100"}, {C, "0xc000"}, {C, "0xc100"}, {D, "0xd000"}});
    CHECK_EQ(map["entries"].size(), 1u);
    CHECK_EQ(map["entries"][0]["name"], D);
    CHECK_EQ(map["stats"]["matched"], 1u);
    CHECK_EQ(map["stats"]["ambiguous_names"], 3u);
    CHECK_EQ(map["stats"]["pc_only"], 0u);
    CHECK_EQ(map["stats"]["ps5_only"], 0u);
    // Duplicate with identical addresses is still ambiguous; duplicate-only-in-one-dump is not pc_only.
    auto same = build({{A, "0x1000"}, {A, "0x1000"}}, {});
    CHECK_EQ(same["stats"]["ambiguous_names"], 1u);
    CHECK_EQ(same["stats"]["pc_only"], 0u);
    CHECK_EQ(same["entries"].size(), 0u);
}

TEST(symbol_map_identical_code_folding_equal_targets) {
    auto map = build({{A, "0x1000"}, {B, "0x1000"}, {C, "0x3000"}},
                     {{A, "0xf00"}, {B, "0xf00"}, {C, "0x3300"}});
    CHECK_EQ(map["stats"]["matched"], 3u);
    CHECK_EQ(map["stats"]["folded_pc_addresses"], 1u);
    auto r = translate_pc_address(map, 0x1000);
    CHECK_EQ(r["status"], "translated");
    CHECK_EQ(r["ps5_vaddr"], "0xf00");
    CHECK_EQ(r["names"].size(), 2u);
    CHECK_EQ(r["names"][0], B);  // name order
    CHECK_EQ(r["names"][1], A);
    auto single = translate_pc_address(map, 0x3000);
    CHECK_EQ(single["status"], "translated");
    CHECK_EQ(single["ps5_vaddr"], "0x3300");
    CHECK_EQ(single["names"].size(), 1u);
}

TEST(symbol_map_identical_code_folding_differing_targets) {
    auto map = build({{A, "0x1000"}, {B, "0x1000"}, {C, "0x3000"}},
                     {{A, "0xf00"}, {B, "0xf80"}, {C, "0x3300"}});
    CHECK_EQ(map["stats"]["folded_pc_addresses"], 1u);
    auto r = translate_pc_address(map, 0x1000);
    CHECK_EQ(r["status"], "ambiguous");
    CHECK(r["ps5_vaddr"].is_null());
    CHECK_EQ(r["names"].size(), 2u);
    // The names are still individually translatable.
    CHECK_EQ(translate_function_name(map, A)["ps5_vaddr"], "0xf00");
    CHECK_EQ(translate_function_name(map, B)["ps5_vaddr"], "0xf80");
    // Three names at one address, two distinct targets -> still ambiguous.
    auto three = build({{A, "0x1000"}, {B, "0x1000"}, {D, "0x1000"}},
                       {{A, "0xf00"}, {B, "0xf00"}, {D, "0xf10"}});
    CHECK_EQ(three["stats"]["folded_pc_addresses"], 1u);
    CHECK_EQ(translate_pc_address(three, 0x1000)["status"], "ambiguous");
}

TEST(symbol_map_interior_and_not_found_boundaries) {
    auto map = build({{A, "0x1000"}, {B, "0x2000"}, {C, "0x3000"}},
                     {{A, "0xa000"}, {B, "0xb000"}, {C, "0xc000"}});
    CHECK_EQ(translate_pc_address(map, 0x0)["status"], "not_found");
    CHECK_EQ(translate_pc_address(map, 0xfff)["status"], "not_found");
    CHECK_EQ(translate_pc_address(map, 0x1000)["status"], "translated");
    auto lo = translate_pc_address(map, 0x1001);
    CHECK_EQ(lo["status"], "interior");
    CHECK(lo["ps5_vaddr"].is_null());
    CHECK_EQ(lo["names"].size(), 1u);
    CHECK_EQ(lo["names"][0], A);
    CHECK_EQ(translate_pc_address(map, 0x1fff)["status"], "interior");
    CHECK_EQ(translate_pc_address(map, 0x2000)["status"], "translated");
    CHECK_EQ(translate_pc_address(map, 0x2fff)["names"][0], B);
    CHECK_EQ(translate_pc_address(map, 0x3000)["status"], "translated");
    // Above the highest entry there is no known end: not_found.
    CHECK_EQ(translate_pc_address(map, 0x3001)["status"], "not_found");
    CHECK_EQ(translate_pc_address(map, UINT64_MAX)["status"], "not_found");
    CHECK(translate_pc_address(map, 0x3001)["names"].empty());
    CHECK_EQ(translate_pc_address(map, 0x1000)["ps5_vaddr"], "0xa000");
    // Interior of a folded address reports every folded name.
    auto fold = build({{A, "0x100"}, {B, "0x100"}, {C, "0x200"}}, {{A, "0x1"}, {B, "0x1"}, {C, "0x2"}});
    CHECK_EQ(translate_pc_address(fold, 0x150)["status"], "interior");
    CHECK_EQ(translate_pc_address(fold, 0x150)["names"].size(), 2u);
    // Address zero and maximum entries.
    auto edge = build({{A, "0x0"}, {B, "0xffffffffffffffff"}}, {{A, "0x10"}, {B, "0x20"}});
    CHECK_EQ(translate_pc_address(edge, 0)["status"], "translated");
    CHECK_EQ(translate_pc_address(edge, 0)["ps5_vaddr"], "0x10");
    CHECK_EQ(translate_pc_address(edge, UINT64_MAX)["status"], "translated");
    CHECK_EQ(translate_pc_address(edge, UINT64_MAX - 1)["status"], "interior");
    CHECK_EQ(translate_pc_address(edge, 1)["status"], "interior");
}

TEST(symbol_map_address_parsing) {
    auto ok = [](const std::string& pc) { return build({{A, pc}}, {{A, "0x1"}}); };
    CHECK_EQ(ok("0xffffffffffffffff")["entries"][0]["pc_rva"], "0xffffffffffffffff");
    CHECK_EQ(ok("0xFFFFFFFFFFFFFFFF")["entries"][0]["pc_rva"], "0xffffffffffffffff");
    CHECK_EQ(ok("0xAbCdEf")["entries"][0]["pc_rva"], "0xabcdef");
    CHECK_EQ(ok("0x0")["entries"][0]["pc_rva"], "0x0");
    CHECK_EQ(ok("0x0000")["entries"][0]["pc_rva"], "0x0");
    CHECK_EQ(ok("0x000000000000001a")["entries"][0]["pc_rva"], "0x1a");  // 16 digits with leading zeros
    CHECK_EQ(ok("0x1a2b")["entries"][0]["pc_rva"], "0x1a2b");
    CHECK_THROWS_AS(ok("0x10000000000000000"), std::invalid_argument);   // 2^64 overflows
    CHECK_THROWS_AS(ok("0xfffffffffffffffff"), std::invalid_argument);
    CHECK_THROWS_AS(ok("0x00000000000000001"), std::invalid_argument);   // 17 digits
    CHECK_THROWS_AS(ok(""), std::invalid_argument);
    CHECK_THROWS_AS(ok("0x"), std::invalid_argument);
    CHECK_THROWS_AS(ok("1000"), std::invalid_argument);
    CHECK_THROWS_AS(ok("x1000"), std::invalid_argument);
    CHECK_THROWS_AS(ok("0x10g0"), std::invalid_argument);
    CHECK_THROWS_AS(ok(" 0x1000"), std::invalid_argument);
    CHECK_THROWS_AS(ok("0x1000 "), std::invalid_argument);
    CHECK_THROWS_AS(ok("-0x1"), std::invalid_argument);
    CHECK_THROWS_AS(ok("0x-1"), std::invalid_argument);
    // Numbers are rejected: addresses must be strings.
    auto d = pc_dump({{A, "0x1"}});
    d["functions"][0]["address"] = 4096;
    CHECK_THROWS_AS(validate_reflection_dump(d), std::invalid_argument);
    // Messages carry the module prefix.
    try { ok("0x"); CHECK(false); } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).rfind("symbol_map: ", 0) == 0);
    }
}

TEST(symbol_map_platform_and_address_kind_mismatch) {
    Fns f = {{A, "0x1"}};
    CHECK_THROWS_AS(build_symbol_map(dump("pc", "vaddr", f), ps5_dump(f), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(pc_dump(f), dump("ps5", "rva", f), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(ps5_dump(f), pc_dump(f), identity()), std::invalid_argument);  // swapped
    CHECK_THROWS_AS(build_symbol_map(pc_dump(f), pc_dump(f), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(ps5_dump(f), ps5_dump(f), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(dump("xbox", "rva", f), ps5_dump(f), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(dump("pc", "offset", f), ps5_dump(f), identity()), std::invalid_argument);
}

TEST(symbol_map_engine_version_mismatch) {
    Fns f = {{A, "0x1"}};
    CHECK_THROWS_AS(build_symbol_map(pc_dump(f, "5.6"), ps5_dump(f, "5.5"), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(pc_dump(f, "5.6"), ps5_dump(f, "5.6.1"), identity()), std::invalid_argument);
    CHECK_THROWS_AS(build_symbol_map(pc_dump(f, ""), ps5_dump(f, ""), identity()), std::invalid_argument);
    CHECK_EQ(build_symbol_map(pc_dump(f, "5.5"), ps5_dump(f, "5.5"), identity())["engine_version"], "5.5");
}

TEST(symbol_map_identity_validation) {
    Fns f = {{A, "0x1"}};
    for (const char* key : {"title_id", "console_content_version", "pc_build"}) {
        auto missing = identity(); missing.erase(key);
        CHECK_THROWS_AS(build_symbol_map(pc_dump(f), ps5_dump(f), missing), std::invalid_argument);
        auto empty = identity(); empty[key] = "";
        CHECK_THROWS_AS(build_symbol_map(pc_dump(f), ps5_dump(f), empty), std::invalid_argument);
        auto number = identity(); number[key] = 3;
        CHECK_THROWS_AS(build_symbol_map(pc_dump(f), ps5_dump(f), number), std::invalid_argument);
    }
    CHECK_THROWS_AS(build_symbol_map(pc_dump(f), ps5_dump(f), Json::array()), std::invalid_argument);
}

TEST(symbol_map_reflection_dump_validation) {
    Fns f = {{A, "0xABC"}, {C, "0x10"}};
    auto normalized = validate_reflection_dump(pc_dump(f));
    CHECK_EQ(normalized["functions"][0]["address"], "0xabc");
    CHECK_EQ(normalized["functions"][0]["name"], A);  // input order is preserved
    CHECK_EQ(normalized["functions"][1]["name"], C);
    CHECK_EQ(validate_reflection_dump(normalized), normalized);
    // Duplicate names are legal in a dump (they are excluded at build time).
    CHECK_EQ(validate_reflection_dump(pc_dump({{A, "0x1"}, {A, "0x2"}}))["functions"].size(), 2u);

    auto bad = [](auto mutate) {
        auto d = pc_dump({{A, "0x1"}});
        mutate(d);
        CHECK_THROWS_AS(validate_reflection_dump(d), std::invalid_argument);
    };
    bad([](Json& d) { d["schema"] = "modbridge.reflection-dump/2"; });
    bad([](Json& d) { d.erase("schema"); });
    bad([](Json& d) { d["schema"] = 1; });
    bad([](Json& d) { d.erase("platform"); });
    bad([](Json& d) { d["platform"] = "PC"; });
    bad([](Json& d) { d.erase("image"); });
    bad([](Json& d) { d["image"] = "x"; });
    bad([](Json& d) { d["image"]["sha256"] = std::string(63, 'a'); });
    bad([](Json& d) { d["image"]["sha256"] = std::string(64, 'A'); });
    bad([](Json& d) { d["image"]["sha256"] = std::string(64, 'g'); });
    bad([](Json& d) { d["image"].erase("address_kind"); });
    bad([](Json& d) { d.erase("engine_version"); });
    bad([](Json& d) { d["engine_version"] = 5.6; });
    bad([](Json& d) { d.erase("functions"); });
    bad([](Json& d) { d["functions"] = Json::object(); });
    bad([](Json& d) { d["functions"][0] = "name"; });
    bad([](Json& d) { d["functions"][0].erase("address"); });
    bad([](Json& d) { d["functions"][0].erase("name"); });
    bad([](Json& d) { d["functions"][0]["name"] = 7; });
    bad([](Json& d) { d["functions"][0]["name"] = ""; });
    bad([](Json& d) { d["functions"][0]["name"] = "Actor:Foo"; });                // bad prefix
    bad([](Json& d) { d["functions"][0]["name"] = "/Engine/Actor.Actor:Foo"; });
    bad([](Json& d) { d["functions"][0]["name"] = "/Script/Engine.Actor"; });     // no ':'
    bad([](Json& d) { d["functions"][0]["name"] = "/Script/Engine.Actor:A:B"; }); // two ':'
    bad([](Json& d) { d["functions"][0]["name"] = "/Script/Engine.Actor:"; });    // empty function
    bad([](Json& d) { d["functions"][0]["name"] = "/Script/:Foo"; });             // empty class part
    bad([](Json& d) { d["functions"][0]["name"] = ":Foo"; });
    bad([](Json& d) { d["functions"][0]["name"] = "/Script/Engine.Actor:Fo\no"; });
    bad([](Json& d) { d["functions"][0]["name"] = "/Script/Engine.Actor:" + std::string(1100, 'x'); });
    CHECK_THROWS_AS(validate_reflection_dump(Json::array()), std::invalid_argument);
    CHECK_THROWS_AS(validate_reflection_dump(Json(nullptr)), std::invalid_argument);

    // Name length boundary: exactly 1024 bytes passes, 1025 fails.
    const std::string prefix = "/Script/Engine.Actor:";
    CHECK_EQ(validate_reflection_dump(pc_dump({{prefix + std::string(1024 - prefix.size(), 'x'), "0x1"}}))["functions"].size(), 1u);
    CHECK_THROWS_AS(validate_reflection_dump(pc_dump({{prefix + std::string(1025 - prefix.size(), 'x'), "0x1"}})), std::invalid_argument);
}

TEST(symbol_map_validate_symbol_map_rejects_bad_maps) {
    Fns good = {{C, "0x10"}, {B, "0x20"}, {A, "0x30"}};  // already name-sorted
    CHECK_EQ(validate_symbol_map(map_of(good))["entries"].size(), 3u);
    // Unsorted
    CHECK_THROWS_AS(validate_symbol_map(map_of({{A, "0x30"}, {B, "0x20"}})), std::invalid_argument);
    // Duplicate names
    CHECK_THROWS_AS(validate_symbol_map(map_of({{A, "0x30"}, {A, "0x40"}})), std::invalid_argument);

    auto bad = [&](auto mutate) {
        auto m = map_of(good);
        mutate(m);
        CHECK_THROWS_AS(validate_symbol_map(m), std::invalid_argument);
    };
    bad([](Json& m) { m["schema"] = "modbridge.symbol-map/2"; });
    bad([](Json& m) { m.erase("title_id"); });
    bad([](Json& m) { m["console_content_version"] = ""; });
    bad([](Json& m) { m["pc_build"] = 1; });
    bad([](Json& m) { m.erase("engine_version"); });
    bad([](Json& m) { m["pc_image_sha256"] = "abc"; });
    bad([](Json& m) { m["ps5_image_sha256"] = std::string(64, 'Z'); });
    bad([](Json& m) { m["entries"] = Json::object(); });
    bad([](Json& m) { m.erase("entries"); });
    bad([](Json& m) { m["entries"][0]["pc_rva"] = "10"; });
    bad([](Json& m) { m["entries"][0]["ps5_vaddr"] = "0x10000000000000000"; });
    bad([](Json& m) { m["entries"][0].erase("ps5_vaddr"); });
    bad([](Json& m) { m["entries"][0]["name"] = "bogus"; });
    bad([](Json& m) { m.erase("stats"); });
    bad([](Json& m) { m["stats"]["matched"] = 2; });                 // does not match entries
    bad([](Json& m) { m["stats"]["folded_pc_addresses"] = 1; });     // no folded address exists
    bad([](Json& m) { m["stats"]["pc_only"] = -1; });
    bad([](Json& m) { m["stats"]["ps5_only"] = "0"; });
    bad([](Json& m) { m["stats"].erase("ambiguous_names"); });
    CHECK_THROWS_AS(validate_symbol_map(Json::array()), std::invalid_argument);
    // translate_* validate the map too.
    CHECK_THROWS_AS(translate_pc_address(map_of({{A, "0x30"}, {B, "0x20"}}), 0x30), std::invalid_argument);
    CHECK_THROWS_AS(translate_function_name(map_of({{A, "0x30"}, {B, "0x20"}}), A), std::invalid_argument);
    // Folded count is verified: two entries at one address require folded_pc_addresses == 1.
    Json st = {{"matched", 2}, {"pc_only", 0}, {"ps5_only", 0}, {"ambiguous_names", 0}, {"folded_pc_addresses", 1}};
    CHECK_EQ(validate_symbol_map(map_of({{B, "0x20"}, {A, "0x20"}}, st))["stats"]["folded_pc_addresses"], 1u);
    st["folded_pc_addresses"] = 0;
    CHECK_THROWS_AS(validate_symbol_map(map_of({{B, "0x20"}, {A, "0x20"}}, st)), std::invalid_argument);
    // Addresses are canonicalized on validation.
    auto norm = validate_symbol_map(map_of({{A, "0xAB"}}));
    CHECK_EQ(norm["entries"][0]["pc_rva"], "0xab");
}

TEST(symbol_map_translate_function_name) {
    auto map = build({{A, "0x1000"}, {B, "0x2000"}, {C, "0x3000"}, {D, "0x4000"}},
                     {{A, "0xa000"}, {B, "0xb000"}, {C, "0xc000"}});
    auto hit = translate_function_name(map, A);
    CHECK_EQ(hit["status"], "translated");
    CHECK_EQ(hit["ps5_vaddr"], "0xa000");
    CHECK_EQ(translate_function_name(map, C)["ps5_vaddr"], "0xc000");
    auto pc_only = translate_function_name(map, D);
    CHECK_EQ(pc_only["status"], "not_found");
    CHECK(pc_only["ps5_vaddr"].is_null());
    CHECK_EQ(translate_function_name(map, "/Script/Nope.Nope:Nope")["status"], "not_found");
    CHECK_EQ(translate_function_name(map, "")["status"], "not_found");
    CHECK_EQ(translate_function_name(map, "bogus")["status"], "not_found");
    CHECK_EQ(translate_function_name(map, A.substr(0, A.size() - 1))["status"], "not_found");  // prefix only
    CHECK_EQ(translate_function_name(map, A + "x")["status"], "not_found");
    // Excluded duplicates are not found.
    auto dup = build({{A, "0x1"}, {A, "0x2"}}, {{A, "0x3"}});
    CHECK_EQ(translate_function_name(dup, A)["status"], "not_found");
}

TEST(symbol_map_output_is_deterministic) {
    Fns pc = {{A, "0x1000"}, {B, "0x2000"}, {C, "0x3000"}, {D, "0x4000"}, {B, "0x2500"}};
    Fns ps = {{D, "0xd"}, {C, "0xc"}, {A, "0xa"}, {B, "0xb"}};
    Fns pc2 = pc, ps2 = ps;
    std::reverse(pc2.begin(), pc2.end());
    std::reverse(ps2.begin(), ps2.end());
    auto m1 = build(pc, ps);
    auto m2 = build(pc2, ps2);
    CHECK_EQ(m1.dump(), m2.dump());
    CHECK_EQ(m1["stats"]["ambiguous_names"], 1u);
    CHECK_EQ(m1["entries"].size(), 3u);
    auto& entries = m1["entries"];
    CHECK(std::is_sorted(entries.begin(), entries.end(), [](const Json& x, const Json& y) {
        return x["name"].get<std::string>() < y["name"].get<std::string>(); }));
    // A stored map survives a serialize/parse round trip and revalidates identically.
    auto reparsed = Json::parse(m1.dump());
    CHECK_EQ(validate_symbol_map(reparsed), m1);
    // Stats are consistent: every distinct name lands in exactly one bucket.
    auto s = m1["stats"];
    CHECK_EQ(s["matched"].get<unsigned>() + s["pc_only"].get<unsigned>() + s["ps5_only"].get<unsigned>() +
             s["ambiguous_names"].get<unsigned>(), 4u);
}

TEST(symbol_map_large_map_performance_smoke) {
    constexpr std::uint64_t n = 100000;
    Fns pc, ps;
    pc.reserve(n);
    ps.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        const std::string name = "/Script/Engine.Class" + std::to_string(i) + ":Func" + std::to_string(i);
        pc.push_back({name, hex(0x1000 + i * 0x10)});
        ps.push_back({name, hex(0x5000000 + i * 0x20)});
    }
    // Reverse the PS5 side so the merge cannot rely on input order.
    std::reverse(ps.begin(), ps.end());
    // Disjoint tails keep pc_only / ps5_only non-trivial.
    pc.push_back({"/Script/Engine.PcOnly:Only", "0xfffffffff"});
    ps.push_back({"/Script/Engine.Ps5Only:Only", "0xeeeeeeeee"});

    const auto start = std::chrono::steady_clock::now();
    auto map = build(pc, ps);
    CHECK_EQ(map["entries"].size(), static_cast<std::size_t>(n));
    CHECK_EQ(map["stats"]["matched"], n);
    CHECK_EQ(map["stats"]["pc_only"], 1u);
    CHECK_EQ(map["stats"]["ps5_only"], 1u);
    auto v = validate_symbol_map(map);
    CHECK_EQ(v["entries"].size(), static_cast<std::size_t>(n));
    // Each call rebuilds an O(n log n) index; a handful of calls is plenty.
    const auto& e0 = map["entries"][12345];
    const std::uint64_t pc_value = std::stoull(e0["pc_rva"].get<std::string>().substr(2), nullptr, 16);
    auto hit = translate_pc_address(map, pc_value);
    CHECK_EQ(hit["status"], "translated");
    CHECK_EQ(hit["ps5_vaddr"], e0["ps5_vaddr"]);
    CHECK_EQ(hit["names"][0], e0["name"]);
    CHECK_EQ(translate_pc_address(map, pc_value + 1)["status"], "interior");
    CHECK_EQ(translate_pc_address(map, 0)["status"], "not_found");
    CHECK_EQ(translate_pc_address(map, UINT64_MAX)["status"], "not_found");
    CHECK_EQ(translate_function_name(map, e0["name"].get<std::string>())["ps5_vaddr"], e0["ps5_vaddr"]);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    CHECK(ms < 15000);
}
