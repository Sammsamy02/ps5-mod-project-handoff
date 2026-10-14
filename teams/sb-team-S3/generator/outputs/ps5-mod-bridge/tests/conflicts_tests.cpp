#include "test.hpp"
#include "modbridge/conflicts.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <vector>
using namespace modbridge;
namespace {
std::string digest(char c) { return std::string(64, c); }
Json file(std::string path, char hash = 'a') { return {{"path", std::move(path)}, {"sha256", digest(hash)}}; }
Json mod(std::string id, std::uint64_t rank, Json files) {
    return {{"id", id}, {"name", "Name of " + id}, {"rank", rank}, {"files", std::move(files)}};
}
Json policy(const char* case_name = "ascii_insensitive", const char* winner = "last_wins") {
    return {{"case", case_name}, {"winner", winner}};
}
Json run(const Json& mods, const Json& p = policy()) { return detect_conflicts(mods, p); }
// The refusal message, or "" when the input was accepted.
std::string refusal(const Json& mods, const Json& p = policy()) {
    try { detect_conflicts(mods, p); } catch (const std::invalid_argument& error) { return error.what(); }
    return {};
}
bool refused_with(const Json& mods, const std::string& needle, const Json& p = policy()) {
    const auto message = refusal(mods, p);
    return message.starts_with("conflicts: ") && message.find(needle) != std::string::npos;
}
Json one_path(const std::string& path) { return Json::array({mod("a", 1, Json::array({file(path)}))}); }
}

TEST(conflicts_overwrite_names_both_mods_and_the_later_one_wins) {
    const auto result = run(Json::array({
        mod("A", 1, Json::array({file("Data/X.pak", 'a'), file("Data/only_a.pak")})),
        mod("B", 2, Json::array({file("data\\x.pak", 'b'), file("only_b.pak")}))}));
    CHECK_EQ(result, Json::parse(R"({"conflicts":[{"identical":false,"kind":"overwrite","mods":["A","B"],
        "path":"data/x.pak","winner":"B"}],
        "pairs":[{"a":"A","b":"B","paths":1,"winner":"B"}],
        "summary":{"conflicting_paths":1,"harmless_duplicates":0,"mods_involved":2}})"));
}

TEST(conflicts_empty_and_disjoint_inputs_have_no_conflicts) {
    const auto none = Json::parse(R"({"conflicts":[],"pairs":[],
        "summary":{"conflicting_paths":0,"harmless_duplicates":0,"mods_involved":0}})");
    CHECK_EQ(run(Json::array()), none);
    CHECK_EQ(run(Json::array({mod("A", 1, Json::array())})), none);
    CHECK_EQ(run(Json::array({mod("A", 1, Json::array({file("a/one"), file("a/two")})),
                              mod("B", 2, Json::array({file("a/three"), file("b/one")}))})), none);
    // Mods that only share directories never conflict.
    CHECK_EQ(run(Json::array({mod("A", 1, Json::array({file("dir/sub/a")})), mod("B", 2, Json::array({file("dir/sub/b")}))})), none);
}

TEST(conflicts_identical_duplicates_are_harmless_and_form_no_pair) {
    const auto result = run(Json::array({
        mod("A", 1, Json::array({file("lib/shared.dll", 'c'), file("x", 'a')})),
        mod("B", 2, Json::array({file("LIB/Shared.DLL", 'c')}))}));
    CHECK_EQ(result, Json::parse(R"({"conflicts":[{"identical":true,"kind":"overwrite","mods":["A","B"],
        "path":"lib/shared.dll","winner":"B"}],"pairs":[],
        "summary":{"conflicting_paths":0,"harmless_duplicates":1,"mods_involved":0}})"));
}

TEST(conflicts_three_way_overwrite_is_identical_only_when_every_copy_matches) {
    const auto result = run(Json::array({
        mod("A", 1, Json::array({file("p", 'a'), file("q", 'a')})),
        mod("B", 2, Json::array({file("p", 'a'), file("q", 'a')})),
        mod("C", 3, Json::array({file("p", 'a'), file("q", 'b')}))}));
    CHECK_EQ(result.at("conflicts"), Json::parse(R"([
        {"identical":true,"kind":"overwrite","mods":["A","B","C"],"path":"p","winner":"C"},
        {"identical":false,"kind":"overwrite","mods":["A","B","C"],"path":"q","winner":"C"}])"));
    CHECK_EQ(result.at("summary"), Json::parse(R"({"conflicting_paths":1,"harmless_duplicates":1,"mods_involved":3})"));
    CHECK_EQ(result.at("pairs"), Json::parse(R"([
        {"a":"A","b":"B","paths":1,"winner":"B"},{"a":"A","b":"C","paths":1,"winner":"C"},
        {"a":"B","b":"C","paths":1,"winner":"C"}])"));
}

TEST(conflicts_file_versus_directory_is_its_own_kind) {
    const auto result = run(Json::array({
        mod("A", 1, Json::array({file("mods/x", 'a')})),
        mod("B", 2, Json::array({file("mods/x/y.lua", 'b'), file("mods/x/deep/z.lua", 'b')}))}));
    CHECK_EQ(result, Json::parse(R"({"conflicts":[{"identical":false,"kind":"file_vs_directory",
        "mods":["A","B"],"path":"mods/x","winner":"B"}],
        "pairs":[{"a":"A","b":"B","paths":1,"winner":"B"}],
        "summary":{"conflicting_paths":1,"harmless_duplicates":0,"mods_involved":2}})"));
    // The relationship does not depend on which mod loads first.
    const auto flipped = run(Json::array({
        mod("A", 2, Json::array({file("mods/x", 'a')})),
        mod("B", 1, Json::array({file("mods/x/y.lua", 'b')}))}));
    CHECK_EQ(flipped.at("conflicts")[0].at("mods"), Json::array({"B", "A"}));
    CHECK_EQ(flipped.at("conflicts")[0].at("winner"), "A");
}

TEST(conflicts_file_versus_directory_and_overwrite_can_share_a_path) {
    const auto result = run(Json::array({
        mod("A", 1, Json::array({file("x", 'a')})),
        mod("B", 2, Json::array({file("x", 'b')})),
        mod("C", 3, Json::array({file("x/y", 'c')}))}));
    CHECK_EQ(result.at("conflicts"), Json::parse(R"([
        {"identical":false,"kind":"file_vs_directory","mods":["A","B","C"],"path":"x","winner":"C"},
        {"identical":false,"kind":"overwrite","mods":["A","B"],"path":"x","winner":"B"}])"));
    CHECK_EQ(result.at("summary"), Json::parse(R"({"conflicting_paths":2,"harmless_duplicates":0,"mods_involved":3})"));
    // Two identical copies of the file are harmless to each other but still collide with the directory.
    const auto same = run(Json::array({mod("A", 1, Json::array({file("x")})), mod("B", 2, Json::array({file("x")})),
                                       mod("C", 3, Json::array({file("x/y")}))}));
    CHECK_EQ(same.at("summary"), Json::parse(R"({"conflicting_paths":1,"harmless_duplicates":1,"mods_involved":3})"));
}

TEST(conflicts_directory_prefix_ignores_lexically_adjacent_siblings) {
    // "a-b" and "a.txt" sort between "a" and "a/x" in byte order but are not beneath "a".
    const auto result = run(Json::array({
        mod("A", 1, Json::array({file("a")})),
        mod("B", 2, Json::array({file("a-b"), file("a.txt"), file("a b")})),
        mod("C", 3, Json::array({file("a/x")}))}));
    CHECK_EQ(result.at("conflicts").size(), 1u);
    CHECK_EQ(result.at("conflicts")[0].at("mods"), Json::array({"A", "C"}));
    CHECK_EQ(result.at("conflicts")[0].at("path"), "a");
    // A lone directory name that is also a longer file name elsewhere is fine.
    CHECK_EQ(run(Json::array({mod("A", 1, Json::array({file("ab")})), mod("B", 2, Json::array({file("a/b")}))})).at("conflicts").size(), 0u);
}

TEST(conflicts_nested_file_chain_reports_every_ancestor_once) {
    Json mods = Json::array();
    for (std::uint64_t i = 0; i < 300; ++i) {
        std::string path = "a";
        for (std::uint64_t k = 0; k < i; ++k) path += "/a";
        mods.push_back(mod("m" + std::to_string(1000 + i), i, Json::array({file(path)})));
    }
    const auto result = run(mods);
    CHECK_EQ(result.at("conflicts").size(), 299u);
    CHECK_EQ(result.at("summary").at("mods_involved"), 300u);
    // The shortest path is held by m1000 and every other mod sits beneath it.
    const auto& first = result.at("conflicts")[0];
    CHECK_EQ(first.at("path"), "a");
    CHECK_EQ(first.at("mods").size(), 300u);
    CHECK_EQ(first.at("winner"), "m1299");
    // Mods i < j share one entry for each of the i + 1 shortest ancestors.
    const auto& pairs = result.at("pairs");
    CHECK_EQ(pairs.size(), 300u * 299u / 2u);
    CHECK_EQ(pairs[0], Json::parse(R"({"a":"m1000","b":"m1001","paths":1,"winner":"m1001"})"));
    CHECK_EQ(pairs.back(), Json::parse(R"({"a":"m1298","b":"m1299","paths":299,"winner":"m1299"})"));
}

TEST(conflicts_case_policies) {
    const Json mods = Json::array({mod("A", 1, Json::array({file("Data/Mod.PAK", 'a')})),
                                   mod("B", 2, Json::array({file("data/mod.pak", 'b')}))});
    CHECK_EQ(run(mods, policy("byte_exact")).at("conflicts").size(), 0u);
    const auto folded = run(mods, policy("ascii_insensitive"));
    CHECK_EQ(folded.at("conflicts").size(), 1u);
    CHECK_EQ(folded.at("conflicts")[0].at("path"), "data/mod.pak");
    // byte_exact reports the path exactly as normalized, case kept.
    const Json same = Json::array({mod("A", 1, Json::array({file("Data\\Mod.PAK")})), mod("B", 2, Json::array({file("./Data//Mod.PAK")}))});
    CHECK_EQ(run(same, policy("byte_exact")).at("conflicts")[0].at("path"), "Data/Mod.PAK");
    // Directory prefixes follow the same policy.
    const Json mixed = Json::array({mod("A", 1, Json::array({file("Dir/X")})), mod("B", 2, Json::array({file("dir/x/y")}))});
    CHECK_EQ(run(mixed, policy("byte_exact")).at("conflicts").size(), 0u);
    CHECK_EQ(run(mixed, policy("ascii_insensitive")).at("conflicts")[0].at("kind"), "file_vs_directory");
}

TEST(conflicts_non_ascii_is_exact_or_refused) {
    const std::string cafe = "caf\xc3\xa9/x", upper = "CAF\xc3\xa9/x";
    const Json mods = Json::array({mod("A", 1, Json::array({file(cafe)})), mod("B", 2, Json::array({file(upper)}))});
    CHECK(refused_with(mods, "non-ASCII", policy("ascii_insensitive")));
    const auto exact = run(mods, policy("byte_exact"));
    CHECK_EQ(exact.at("conflicts").size(), 0u);
    const Json same = Json::array({mod("A", 1, Json::array({file(cafe, 'a')})), mod("B", 2, Json::array({file(cafe, 'b')}))});
    CHECK_EQ(run(same, policy("byte_exact")).at("conflicts")[0].at("path"), cafe);
}

TEST(conflicts_winner_policies) {
    const Json mods = Json::array({mod("A", 1, Json::array({file("p", 'a'), file("x", 'a')})),
                                   mod("B", 5, Json::array({file("p", 'b')})),
                                   mod("C", 9, Json::array({file("p", 'c'), file("x/y", 'c')}))});
    const auto last = run(mods, policy("ascii_insensitive", "last_wins"));
    const auto first = run(mods, policy("ascii_insensitive", "first_wins"));
    CHECK_EQ(last.at("conflicts"), Json::parse(R"([
        {"identical":false,"kind":"overwrite","mods":["A","B","C"],"path":"p","winner":"C"},
        {"identical":false,"kind":"file_vs_directory","mods":["A","C"],"path":"x","winner":"C"}])"));
    CHECK_EQ(first.at("conflicts"), Json::parse(R"([
        {"identical":false,"kind":"overwrite","mods":["A","B","C"],"path":"p","winner":"A"},
        {"identical":false,"kind":"file_vs_directory","mods":["A","C"],"path":"x","winner":"A"}])"));
    CHECK_EQ(last.at("pairs")[0], Json::parse(R"({"a":"A","b":"B","paths":1,"winner":"B"})"));
    CHECK_EQ(first.at("pairs")[0], Json::parse(R"({"a":"A","b":"B","paths":1,"winner":"A"})"));
    CHECK_EQ(first.at("summary"), last.at("summary"));
}

TEST(conflicts_rank_is_load_order_not_input_order) {
    const auto result = run(Json::array({mod("high", 100, Json::array({file("p", 'a')})),
                                         mod("low", 5, Json::array({file("p", 'b')})),
                                         mod("mid", 7, Json::array({file("p", 'c')}))}));
    CHECK_EQ(result.at("conflicts")[0].at("mods"), Json::array({"low", "mid", "high"}));
    CHECK_EQ(result.at("conflicts")[0].at("winner"), "high");
    CHECK_EQ(result.at("pairs")[0].at("a"), "low");
    CHECK_EQ(result.at("pairs")[0].at("b"), "mid");
    // Rank zero and the largest rank are both valid.
    const Json extremes = Json::array({mod("zero", 0, Json::array({file("p")})),
        mod("max", std::numeric_limits<std::uint64_t>::max(), Json::array({file("p", 'b')}))});
    CHECK_EQ(run(extremes).at("conflicts")[0].at("winner"), "max");
}

TEST(conflicts_equal_ranks_are_refused) {
    const Json tied = Json::array({mod("A", 3, Json::array({file("p", 'a')})), mod("B", 3, Json::array({file("p", 'b')}))});
    CHECK(refused_with(tied, "share rank 3"));
    CHECK(refused_with(tied, "\"A\""));
    CHECK(refused_with(tied, "\"B\""));
    CHECK(refused_with(tied, "ranks must be unique", policy("byte_exact", "first_wins")));
    // Even when the tied mods share no path and nothing would be decided.
    CHECK(refused_with(Json::array({mod("A", 3, Json::array()), mod("B", 3, Json::array())}), "share rank"));
    // The message and the refusal do not depend on input order.
    CHECK_EQ(refusal(tied), refusal(Json::array({tied[1], tied[0]})));
    CHECK(refused_with(Json::array({mod("A", 1, Json::array()), mod("A", 2, Json::array())}), "duplicate mod id"));
}

TEST(conflicts_rank_must_be_an_unsigned_integer) {
    for (const Json& bad : {Json(-1), Json(1.5), Json(2.0), Json("1"), Json(true), Json(nullptr), Json::array({1}), Json::object()}) {
        Json m = mod("A", 1, Json::array());
        m["rank"] = bad;
        CHECK(refused_with(Json::array({m}), "rank must be an unsigned integer"));
    }
    Json signed_positive = mod("A", 1, Json::array());
    signed_positive["rank"] = static_cast<std::int64_t>(7);
    CHECK_EQ(run(Json::array({signed_positive})).at("pairs").size(), 0u);
    CHECK(refused_with(Json::parse(R"([{"id":"A","name":"n","rank":-3,"files":[]}])"), "rank"));
    CHECK(refused_with(Json::parse(R"([{"id":"A","name":"n","rank":1e2,"files":[]}])"), "rank"));
}

TEST(conflicts_invalid_paths_are_refused_with_the_reason) {
    const std::vector<std::pair<std::string, std::string>> bad = {
        {"../x", "parent traversal"}, {"a/../b", "parent traversal"}, {"a/..", "parent traversal"}, {"..\\x", "parent traversal"},
        {"/abs", "absolute or empty path"}, {"\\abs", "absolute or empty path"}, {"", "absolute or empty path"},
        {".", "empty logical destination"}, {"./", "empty logical destination"}, {"//", "absolute or empty path"},
        {"a:b", "unsafe path character"}, {"C:\\x", "unsafe path character"}, {std::string("a\0b", 3), "unsafe path character"},
        {"a\tb", "unsafe path character"}, {"a\x7f", "unsafe path character"}, {"a\n", "unsafe path character"},
        {"trailing.", "ambiguous destination spelling"}, {"dir /x", "ambiguous destination spelling"},
        {"x/trail ", "ambiguous destination spelling"}, {"...", "ambiguous destination spelling"},
        {"\xff", "not valid UTF-8"}, {"a\xc0\x80", "not valid UTF-8"}, {"\xed\xa0\x80", "not valid UTF-8"},
        {"\xf4\x90\x80\x80", "not valid UTF-8"}, {"a\xe2\x82", "not valid UTF-8"},
        {std::string(4097, 'a'), "exceeds 4096 bytes"}};
    for (const auto& [path, reason] : bad) {
        for (const char* case_name : {"byte_exact", "ascii_insensitive"}) {
            const auto message = refusal(one_path(path), policy(case_name));
            CHECK(message.starts_with("conflicts: mod \"a\" path "));
            CHECK(message.find(reason) != std::string::npos);
        }
    }
    for (const auto& [path, key] : std::vector<std::pair<std::string, std::string>>{
             {std::string(4096, 'a'), std::string(4096, 'a')}, {"a/./b//c/", "a/b/c"}, {"a\\b\\c", "a/b/c"},
             {".hidden/.x", ".hidden/.x"}, {"a b/c d", "a b/c d"}, {"a/ b", "a/ b"}, {"a/b.c/d", "a/b.c/d"}}) {
        const Json mods = Json::array({mod("a", 1, Json::array({file(path)})), mod("b", 2, Json::array({file(path, 'b')}))});
        CHECK_EQ(run(mods, policy("byte_exact")).at("conflicts")[0].at("path"), key);
    }
}

TEST(conflicts_messages_never_echo_unprintable_input) {
    const auto message = refusal(one_path(std::string("bad\x01name/../x\"\\", 15)));
    CHECK(message.find('\x01') == std::string::npos);
    CHECK(message.find("bad?name") != std::string::npos);
    const auto longer = refusal(one_path("../" + std::string(500, 'z')));
    CHECK(longer.size() < 200);
    CHECK(longer.find("...") != std::string::npos);
}

TEST(conflicts_repeats_and_prefixes_inside_one_mod_are_refused) {
    auto in_one = [](Json files, const char* case_name) { return refusal(Json::array({mod("A", 1, std::move(files))}), policy(case_name)); };
    CHECK(in_one(Json::array({file("x"), file("./x")}), "byte_exact").find("more than once") != std::string::npos);
    CHECK(in_one(Json::array({file("x", 'a'), file("x", 'b')}), "ascii_insensitive").find("more than once") != std::string::npos);
    CHECK(in_one(Json::array({file("A.txt"), file("a.TXT")}), "ascii_insensitive").find("more than once") != std::string::npos);
    CHECK_EQ(in_one(Json::array({file("A.txt"), file("a.TXT")}), "byte_exact"), "");
    CHECK(in_one(Json::array({file("x"), file("x/y")}), "byte_exact").find("has both file \"x\"") != std::string::npos);
    CHECK(in_one(Json::array({file("x/y/z"), file("x")}), "byte_exact").find("has both file") != std::string::npos);
    CHECK(in_one(Json::array({file("x/y"), file("x/w"), file("x/y/z")}), "byte_exact").find("has both file \"x/y\"") != std::string::npos);
    CHECK(in_one(Json::array({file("X"), file("x/y")}), "ascii_insensitive").find("has both file") != std::string::npos);
    CHECK_EQ(in_one(Json::array({file("X"), file("x/y")}), "byte_exact"), "");
    CHECK_EQ(in_one(Json::array({file("x/y"), file("x/z"), file("xy")}), "byte_exact"), "");
}

TEST(conflicts_schema_is_strict) {
    const Json good = mod("A", 1, Json::array({file("p")}));
    CHECK_EQ(refusal(Json::array({good})), "");
    CHECK(refused_with(Json::object(), "mods must be an array"));
    CHECK(refused_with(Json::array({"A"}), "must be an object"));
    for (const char* field : {"id", "name", "rank", "files"}) {
        Json broken = good;
        broken.erase(field);
        CHECK(refused_with(Json::array({broken}), std::string("missing field ") + field));
    }
    Json extra = good;
    extra["note"] = "x";
    CHECK(refused_with(Json::array({extra}), "unknown fields"));
    for (const char* field : {"id", "name"}) {
        for (const Json& bad : {Json(""), Json(7), Json(nullptr), Json::array()}) {
            Json broken = good;
            broken[field] = bad;
            CHECK(refused_with(Json::array({broken}), field));
        }
    }
    Json long_id = good;
    long_id["id"] = std::string(257, 'i');
    CHECK(refused_with(Json::array({long_id}), "field id"));
    long_id["id"] = std::string(256, 'i');
    CHECK_EQ(refusal(Json::array({long_id})), "");
    Json long_name = good;
    long_name["name"] = std::string(1025, 'n');
    CHECK(refused_with(Json::array({long_name}), "field name"));
    Json bad_id = good;
    bad_id["id"] = "bad\xff";
    CHECK(refused_with(Json::array({bad_id}), "not valid UTF-8"));
    Json bad_name = good;
    bad_name["name"] = "bad\xff";
    CHECK(refused_with(Json::array({bad_name}), "not valid UTF-8"));
    Json files_not_array = good;
    files_not_array["files"] = Json::object();
    CHECK(refused_with(Json::array({files_not_array}), "files must be an array"));
    // File records.
    for (const Json& bad : {Json::object({{"path", "p"}}), Json::object({{"sha256", digest('a')}}),
             Json::object({{"path", "p"}, {"sha256", digest('a')}, {"size", 1}}), Json::object({{"path", 7}, {"sha256", digest('a')}}),
             Json("p"), Json(nullptr)}) {
        Json broken = good;
        broken["files"] = Json::array({bad});
        CHECK(refusal(Json::array({broken})).starts_with("conflicts: "));
    }
    for (const Json& bad : {Json(std::string(63, 'a')), Json(std::string(65, 'a')), Json(std::string(64, 'A')),
                            Json(std::string(64, 'g')), Json(7), Json(nullptr), Json("")}) {
        Json broken = good;
        broken["files"] = Json::array({Json{{"path", "p"}, {"sha256", bad}}});
        CHECK(refused_with(Json::array({broken}), "sha256 must be 64 lowercase hex digits"));
    }
}

TEST(conflicts_policy_is_strict) {
    const Json mods = Json::array();
    CHECK_EQ(refusal(mods, policy("byte_exact", "first_wins")), "");
    CHECK(refused_with(mods, "policy", Json::array()));
    CHECK(refused_with(mods, "missing field case", Json{{"winner", "last_wins"}}));
    CHECK(refused_with(mods, "missing field winner", Json{{"case", "byte_exact"}}));
    CHECK(refused_with(mods, "unknown fields", Json{{"case", "byte_exact"}, {"winner", "last_wins"}, {"extra", 1}}));
    CHECK(refused_with(mods, "case must be", policy("Byte_Exact")));
    CHECK(refused_with(mods, "case must be", policy("insensitive")));
    CHECK(refused_with(mods, "winner must be", policy("byte_exact", "highest")));
    CHECK(refused_with(mods, "must be strings", Json{{"case", 1}, {"winner", "last_wins"}}));
    CHECK(refused_with(mods, "must be strings", Json{{"case", "byte_exact"}, {"winner", nullptr}}));
}

TEST(conflicts_limits) {
    Json many = Json::array();
    for (std::uint64_t i = 0; i < conflict_max_mods; ++i) many.push_back(mod("m" + std::to_string(i), i, Json::array({file("shared.bin", static_cast<char>('a' + i % 6))})));
    const auto at_limit = run(many);
    CHECK_EQ(at_limit.at("conflicts")[0].at("mods").size(), 512u);
    CHECK_EQ(at_limit.at("conflicts")[0].at("winner"), "m511");
    CHECK_EQ(at_limit.at("pairs").size(), 512u * 511u / 2u);
    CHECK_EQ(at_limit.at("summary").at("mods_involved"), 512u);
    many.push_back(mod("overflow", 9999, Json::array()));
    CHECK(refused_with(many, "too many mods"));

    Json big = Json::array();
    for (int i = 0; i < 100000; ++i) big.push_back(file("d/" + std::to_string(i)));
    CHECK_EQ(run(Json::array({mod("A", 1, big)})).at("conflicts").size(), 0u);
    Json first = Json::array({mod("A", 1, big)});
    first.push_back(mod("B", 2, Json::array({file("one-too-many")})));
    CHECK(refused_with(first, "too many files"));
    Json split_a = Json::array(), split_b = Json::array();
    for (int i = 0; i < 60000; ++i) split_a.push_back(file("a/" + std::to_string(i)));
    for (int i = 0; i < 40001; ++i) split_b.push_back(file("b/" + std::to_string(i)));
    CHECK(refused_with(Json::array({mod("A", 1, split_a), mod("B", 2, split_b)}), "too many files"));
    split_b.erase(split_b.size() - 1);
    CHECK_EQ(run(Json::array({mod("A", 1, split_a), mod("B", 2, split_b)})).at("conflicts").size(), 0u);

    // Total raw path bytes are capped independently of the file count.
    Json heavy = Json::array();
    for (int i = 0; i < 8400; ++i) heavy.push_back(file(std::string(4000, 'a') + std::to_string(i)));
    CHECK(refused_with(Json::array({mod("A", 1, heavy)}), "path bytes exceed"));
}

TEST(conflicts_results_do_not_depend_on_input_order) {
    std::vector<Json> mods;
    for (std::uint64_t m = 0; m < 7; ++m) {
        Json files = Json::array();
        for (int f = 0; f < 14; ++f)
            if ((m * 5 + static_cast<std::uint64_t>(f) * 3) % 4 != 0)
                files.push_back(file((f % 3 == 0 ? "Data/" : f % 3 == 1 ? "data/sub/" : "Other\\") + std::to_string(f),
                                     static_cast<char>('a' + (m + static_cast<std::uint64_t>(f)) % 3)));
        // Parent and child names across mods, never inside one mod.
        files.push_back(file(m % 2 ? "tree/node" : "tree/node/leaf" + std::to_string(m)));
        mods.push_back(mod("mod-" + std::to_string(m), 10 + m * 3, files));
    }
    const auto baseline = run(Json(mods));
    CHECK(baseline.at("conflicts").size() > 5);
    CHECK(baseline.at("pairs").size() > 5);
    std::mt19937 random(12345);
    for (int round = 0; round < 40; ++round) {
        auto shuffled = mods;
        for (auto& m : shuffled) {
            auto files = m.at("files").get<std::vector<Json>>();
            std::shuffle(files.begin(), files.end(), random);
            m["files"] = files;
        }
        std::shuffle(shuffled.begin(), shuffled.end(), random);
        CHECK_EQ(run(Json(shuffled)).dump(), baseline.dump());
    }
    // Sorted output: paths by byte order then kind, pairs by rank.
    const auto& conflicts = baseline.at("conflicts");
    for (std::size_t i = 1; i < conflicts.size(); ++i) {
        const auto before = conflicts[i - 1].at("path").get<std::string>(), after = conflicts[i].at("path").get<std::string>();
        CHECK(before < after || (before == after && conflicts[i - 1].at("kind").get<std::string>() < conflicts[i].at("kind").get<std::string>()));
    }
}

TEST(conflicts_results_carry_only_ids_and_normalized_paths) {
    const auto text = run(Json::array({mod("nexus:g:1:2", 1, Json::array({file("A/B")})), mod("nexus:g:3:4", 2, Json::array({file("a/b", 'b')}))})).dump();
    CHECK(text.find("Name of") == std::string::npos);
    CHECK(text.find(digest('a')) == std::string::npos);
    CHECK(text.find("nexus:g:1:2") != std::string::npos);
}

TEST(conflicts_exact_key_order_of_the_result_is_serializable) {
    const auto result = run(Json::array({mod("\xc3\xa9", 1, Json::array({file("p")})), mod("b", 2, Json::array({file("p", 'b')}))}));
    CHECK_EQ(Json::parse(result.dump()), result);
    CHECK_EQ(result.size(), 3u);
}
