#include "test.hpp"
#include "modbridge/compat.hpp"
#include "modbridge/content_index.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
using namespace modbridge;
namespace {

using Bytes = std::vector<std::uint8_t>;
constexpr std::uint32_t NONE = 0xFFFFFFFFu;

// Every invalid input must be an std::invalid_argument with the "content_index: " prefix.
#define CHECK_CIDX_THROWS(expression) do { bool caught_ = false; \
    try { (void)(expression); } \
    catch (const std::invalid_argument& error_) { \
        caught_ = std::string(error_.what()).rfind("content_index: ", 0) == 0; } \
    CHECK(caught_); } while (false)

void put32(Bytes& out, std::uint32_t v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
void put64(Bytes& out, std::uint64_t v) { for (int i = 0; i < 8; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i))); }
void put_raw(Bytes& out, const Bytes& more) { out.insert(out.end(), more.begin(), more.end()); }

// FString encoders.
Bytes ansi(const std::string& s) {
    Bytes out;
    put32(out, static_cast<std::uint32_t>(s.size() + 1));
    for (char c : s) out.push_back(static_cast<std::uint8_t>(c));
    out.push_back(0);
    return out;
}
Bytes wide(const std::u16string& s) {
    Bytes out;
    put32(out, static_cast<std::uint32_t>(0u - static_cast<std::uint32_t>(s.size() + 1)));
    for (char16_t c : s) { out.push_back(static_cast<std::uint8_t>(c & 0xff)); out.push_back(static_cast<std::uint8_t>(c >> 8)); }
    out.push_back(0); out.push_back(0);
    return out;
}

// --------------------------------------------------------------- directory index
struct Raw {
    Bytes mount = ansi("../../../");
    std::vector<std::array<std::uint32_t, 4>> dirs;   // name, child, sibling, first file
    std::vector<std::array<std::uint32_t, 3>> files;  // name, next, user data
    std::vector<Bytes> strings;
    Bytes trailing;
};
Bytes encode(const Raw& raw) {
    Bytes out = raw.mount;
    put32(out, static_cast<std::uint32_t>(raw.dirs.size()));
    for (const auto& d : raw.dirs) for (auto v : d) put32(out, v);
    put32(out, static_cast<std::uint32_t>(raw.files.size()));
    for (const auto& f : raw.files) for (auto v : f) put32(out, v);
    put32(out, static_cast<std::uint32_t>(raw.strings.size()));
    for (const auto& s : raw.strings) put_raw(out, s);
    put_raw(out, raw.trailing);
    return out;
}

// A small tree builder. Directory names starting with '~' are encoded as UTF-16.
struct Node {
    std::string name;
    std::vector<Node> kids;
    std::vector<std::string> files;
};
std::uint32_t add_string(Raw& raw, const std::string& s) {
    if (!s.empty() && s[0] == '~') {
        std::u16string w;
        for (char c : s.substr(1)) w += static_cast<char16_t>(static_cast<unsigned char>(c));
        raw.strings.push_back(wide(w));
    } else {
        raw.strings.push_back(ansi(s));
    }
    return static_cast<std::uint32_t>(raw.strings.size() - 1);
}
std::uint32_t flatten(Raw& raw, const Node& node, bool is_root, const std::string& root_name) {
    const auto row = static_cast<std::uint32_t>(raw.dirs.size());
    raw.dirs.push_back({NONE, NONE, NONE, NONE});
    if (is_root) raw.dirs[row][0] = root_name.empty() ? NONE : add_string(raw, root_name);
    else raw.dirs[row][0] = add_string(raw, node.name);
    std::uint32_t previous = NONE;
    for (const auto& name : node.files) {
        raw.files.push_back({add_string(raw, name), NONE, 0});
        const auto index = static_cast<std::uint32_t>(raw.files.size() - 1);
        if (previous == NONE) raw.dirs[row][3] = index; else raw.files[previous][1] = index;
        previous = index;
    }
    std::uint32_t last = NONE;
    for (const auto& kid : node.kids) {
        const auto child = flatten(raw, kid, false, "");
        if (last == NONE) raw.dirs[row][1] = child; else raw.dirs[last][2] = child;
        last = child;
    }
    return row;
}
Raw from_tree(const Node& root, const std::string& mount = "../../../", const std::string& root_name = "") {
    Raw raw;
    raw.mount = ansi(mount);
    flatten(raw, root, true, root_name);
    return raw;
}
std::vector<std::string> listing_of(const Bytes& blob) {
    const auto result = list_iostore_directory(blob);
    return result.at("files").get<std::vector<std::string>>();
}

// A realistic shape: ../../../Townfall/{Content/{Items/{BP_A.uasset,BP_A.uexp},Maps/L1.umap},
// Plugins/Mod/Content/M.uasset}, Engine/Content/E.uasset, Engine/Plugins/Fx/Content/F.uasset.
Node townfall() {
    return Node{"", {
        Node{"Townfall", {
            Node{"Content", {Node{"Items", {}, {"BP_A.uasset", "BP_A.uexp", "BP_A.ubulk"}},
                             Node{"Maps", {}, {"L1.umap"}}}, {"Root.uasset"}},
            Node{"Plugins", {Node{"Mod", {Node{"Content", {}, {"M.uasset"}}}, {"Mod.uplugin"}}}, {}},
            Node{"Config", {}, {"DefaultGame.ini"}},
        }, {}},
        Node{"Engine", {
            Node{"Content", {}, {"E.uasset"}},
            Node{"Plugins", {Node{"Runtime", {Node{"Fx", {Node{"Content", {}, {"F.uasset"}}}, {}}}, {}}}, {}},
        }, {}},
    }, {}};
}

// ------------------------------------------------------------------- .utoc header
struct Toc {
    std::uint8_t version = 8;
    std::uint32_t header_size = 144, chunks = 2, blocks = 3, block_entry = 12, methods = 1, method_bytes = 32,
        block_size = 65536, dir_bytes = 100, partitions = 1, seeds = 1, overflow = 1;
    std::uint64_t partition_bytes = 1ull << 30;
    std::uint8_t flags = 8;
    std::string magic = "-==--==--==--==-";
    Bytes tail;  // bytes after the header (the test places a signature size here when needed)
};
Bytes header(const Toc& t) {
    Bytes out(t.magic.begin(), t.magic.end());
    out.push_back(t.version); out.push_back(0); out.push_back(0); out.push_back(0);
    put32(out, t.header_size); put32(out, t.chunks); put32(out, t.blocks); put32(out, t.block_entry);
    put32(out, t.methods); put32(out, t.method_bytes); put32(out, t.block_size); put32(out, t.dir_bytes);
    put32(out, t.partitions); put64(out, 0x1122334455667788ull);
    out.insert(out.end(), 16, 0xAB);            // encryption key guid
    out.push_back(t.flags); out.push_back(0); out.push_back(0); out.push_back(0);
    put32(out, t.seeds); put64(out, t.partition_bytes); put32(out, t.overflow);
    put32(out, 0); out.insert(out.end(), 40, 0); // reserved7, reserved8[5]
    return out;
}
// Header + zero filled tables up to `tables_bytes`, then `tail` (used for signed TOCs).
Bytes toc_file(const Toc& t, std::size_t tables_bytes, const Bytes& tail = {}) {
    Bytes out = header(t);
    out.resize(144 + tables_bytes, 0);
    put_raw(out, tail);
    return out;
}
// 2 chunks, 3 blocks, 1 method of 32 bytes, 1 seed, 1 overflow:
// 24 ids + 20 offlens + 4 seeds + 4 overflow + 36 blocks + 32 methods = 120.
constexpr std::size_t tables = 24 + 20 + 4 + 4 + 36 + 32;

std::uint64_t u(const Json& j, const char* key) { return j.at(key).get<std::uint64_t>(); }

// ------------------------------------------------------------ profile integration
Json profile_with(const Json& index) {
    return {{"schema", "modbridge.game-profile/1"}, {"title_id", "PPSA33286"},
            {"nexus_domain", "silenthilltownfall"}, {"engine", {{"family", "unreal"}, {"version", "5.6"}}},
            {"builds", Json::array({Json{{"console_content_version", "02.003.000"},
                                         {"pc_build", {{"label", "1.03"}, {"released_utc", 1758153600}}}}})},
            {"content_index", index}};
}
Json refs(Json assets = Json::array(), Json classes = Json::array(), Json functions = Json::array()) {
    return {{"assets", assets}, {"classes", classes}, {"functions", functions},
            {"short_class_names", Json::array()}, {"dynamic", false}};
}
Json dump(const Json& functions, const char* platform = "ps5") {
    return {{"schema", "modbridge.reflection-dump/1"}, {"platform", platform},
            {"image", {{"sha256", std::string(64, 'b')}, {"address_kind", "vaddr"}}},
            {"engine_version", "5.6"}, {"functions", functions}};
}
Json inputs(const Json& listings, bool complete, const Json& reflection = nullptr) {
    return {{"console_content_version", "02.003.000"}, {"project", "Townfall"}, {"listings", listings},
            {"listings_complete", complete}, {"reflection_dump", reflection}};
}
Json listing(const std::string& mount, std::initializer_list<const char*> files) {
    Json list = Json::array();
    for (const char* f : files) list.push_back(f);
    return {{"mount_point", mount}, {"files", list}};
}

} // namespace

// =============================================================== directory index
TEST(cidx_listing_nested_plugins_engine_and_non_packages) {
    const auto blob = encode(from_tree(townfall()));
    const auto result = list_iostore_directory(blob);
    CHECK_EQ(result.at("mount_point").get<std::string>(), "../../../");
    const std::vector<std::string> expected = {
        "../../../Engine/Content/E.uasset",
        "../../../Engine/Plugins/Runtime/Fx/Content/F.uasset",
        "../../../Townfall/Config/DefaultGame.ini",
        "../../../Townfall/Content/Items/BP_A.ubulk",
        "../../../Townfall/Content/Items/BP_A.uasset",
        "../../../Townfall/Content/Items/BP_A.uexp",
        "../../../Townfall/Content/Maps/L1.umap",
        "../../../Townfall/Content/Root.uasset",
        "../../../Townfall/Plugins/Mod/Content/M.uasset",
        "../../../Townfall/Plugins/Mod/Mod.uplugin"};
    auto sorted = expected;
    std::sort(sorted.begin(), sorted.end());
    CHECK(result.at("files").get<std::vector<std::string>>() == sorted);
}

TEST(cidx_listing_ansi_and_utf16_names_and_root_name) {
    Raw raw = from_tree(Node{"", {Node{"Mods", {}, {"a.uasset"}}}, {}}, "../../../Game/", "Root");
    // Replace the "Mods" string (row 1) by a UTF-16 name with a BMP character and a surrogate pair.
    raw.strings[1] = wide(u"Möds\U0001F600");
    // Replace the root name by wide ASCII.
    raw.strings[0] = wide(u"Root");
    const auto files = listing_of(encode(raw));
    CHECK_EQ(files.size(), 1u);
    CHECK_EQ(files[0], "../../../Game/Root/M\xc3\xb6" "ds\xf0\x9f\x98\x80/a.uasset");
}

TEST(cidx_listing_root_name_and_empty_directory_names) {
    // Non-empty root name is part of the virtual path.
    CHECK_EQ(listing_of(encode(from_tree(Node{"", {}, {"x.uasset"}}, "../../../Game/", "Content")))[0],
             "../../../Game/Content/x.uasset");
    // Empty directory names collapse into the parent (directory_merge.c append_path).
    CHECK_EQ(listing_of(encode(from_tree(Node{"", {Node{"", {Node{"A", {}, {"f.uasset"}}}, {}}}, {}})))[0],
             "../../../A/f.uasset");
    // Backslash-terminated mount points are accepted.
    CHECK_EQ(listing_of(encode(from_tree(Node{"", {}, {"x.uasset"}}, "..\\..\\")))[0], "..\\..\\x.uasset");
    // Empty (count 0) strings are accepted for directory names.
    Raw raw = from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}}, {}});
    raw.strings[0] = Bytes{0, 0, 0, 0};
    CHECK_EQ(listing_of(encode(raw))[0], "../../../f.uasset");
}

TEST(cidx_listing_empty_index_is_rejected_but_empty_listing_is_fine) {
    Raw raw;
    raw.dirs.push_back({NONE, NONE, NONE, NONE});
    const auto result = list_iostore_directory(encode(raw));
    CHECK_EQ(result.at("files").size(), 0u);
    CHECK_CIDX_THROWS(list_iostore_directory(Bytes{}));
}

TEST(cidx_listing_rejects_bad_fstrings) {
    const auto base = [] { return from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}}, {}}); };
    {   // ANSI without NUL terminator
        auto raw = base(); raw.strings[1] = Bytes{2, 0, 0, 0, 'f', 'g'}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // invalid UTF-8 in ANSI
        auto raw = base(); raw.strings[1] = Bytes{2, 0, 0, 0, 0xff, 0}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // embedded NUL in ANSI
        auto raw = base(); raw.strings[1] = Bytes{3, 0, 0, 0, 'a', 0, 0}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // 0x80000000 count
        auto raw = base(); raw.strings[1] = Bytes{0, 0, 0, 0x80}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // oversized (> 4096 raw bytes)
        auto raw = base(); raw.strings[1] = ansi(std::string(5000, 'x')); CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // truncated string payload
        auto raw = base(); raw.strings[1] = Bytes{200, 0, 0, 0, 'a', 0}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // wide: lone high surrogate, lone low surrogate, embedded NUL
        auto raw = base(); raw.strings[1] = wide(std::u16string(1, char16_t(0xd800))); CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
        raw.strings[1] = wide(std::u16string(1, char16_t(0xdc00))); CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
        raw.strings[1] = wide(std::u16string(u"a\0b", 3)); CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // wide without terminator
        auto raw = base(); raw.strings[1] = Bytes{0xfe, 0xff, 0xff, 0xff, 'a', 0, 'b', 0}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    {   // bad mount: empty, or no trailing separator
        auto raw = base(); raw.mount = Bytes{0, 0, 0, 0}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
        raw.mount = ansi("../../../Game"); CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
}

TEST(cidx_listing_rejects_bad_names) {
    for (const std::string bad : {".", "..", "a/b", "a\\b"}) {
        auto raw = from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}}, {}});
        raw.strings[1] = ansi(bad);   // file name
        CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
        raw = from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}}, {}});
        raw.strings[0] = ansi(bad);   // directory name
        CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    }
    auto raw = from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}}, {}});
    raw.strings[1] = ansi("");        // empty file name
    CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    raw = from_tree(Node{"", {}, {"f.uasset"}}, "../../../", "bad/root");
    CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
    raw = from_tree(Node{"", {}, {"f.uasset"}}, "../../../", "..");
    CHECK_CIDX_THROWS(list_iostore_directory(encode(raw)));
}

TEST(cidx_listing_rejects_graph_violations) {
    const auto two = [] { return from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}, Node{"B", {}, {"g.uasset"}}}, {}}); };
    CHECK_EQ(listing_of(encode(two())).size(), 2u);
    {   auto raw = two(); raw.dirs[0][2] = 1; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }       // root sibling
    {   auto raw = two(); raw.dirs[1][0] = NONE; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }    // non-root without name
    {   auto raw = two(); raw.dirs[1][0] = 99; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }      // name out of range
    {   auto raw = two(); raw.dirs[0][1] = 99; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }      // child out of range
    {   auto raw = two(); raw.dirs[1][2] = 99; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }      // sibling out of range
    {   auto raw = two(); raw.dirs[1][3] = 99; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }      // first file out of range
    {   auto raw = two(); raw.files[0][0] = 99; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }     // file name out of range
    {   auto raw = two(); raw.files[0][1] = 99; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }     // next file out of range
    {   auto raw = two(); raw.dirs[2][2] = 1; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }       // sibling cycle 1 <-> 2
    {   auto raw = two(); raw.dirs[1][1] = 0; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }       // child back to root
    {   auto raw = two(); raw.dirs[1][1] = 1; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }       // self child
    {   auto raw = two(); raw.dirs[2][3] = 0; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }       // file reached twice
    {   auto raw = two(); raw.files[0][1] = 0; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }      // file next-cycle
    {   auto raw = two(); raw.dirs[0][1] = NONE; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }    // unreachable directories
    {   auto raw = two(); raw.dirs.clear(); CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }         // zero directories
    {   auto raw = two(); raw.trailing = {0}; CHECK_CIDX_THROWS(list_iostore_directory(encode(raw))); }       // trailing bytes
}

TEST(cidx_listing_rejects_duplicates_and_truncation) {
    // Same name twice in one directory.
    CHECK_CIDX_THROWS(list_iostore_directory(encode(from_tree(Node{"", {}, {"f.uasset", "f.uasset"}}))));
    // Same virtual path via an empty-named directory.
    CHECK_CIDX_THROWS(list_iostore_directory(encode(from_tree(Node{"", {Node{"", {}, {"f.uasset"}}}, {"f.uasset"}}))));
    // Every strict prefix of a valid blob is rejected.
    const auto blob = encode(from_tree(Node{"", {Node{"A", {}, {"f.uasset"}}}, {}}));
    for (std::size_t n = 0; n < blob.size(); ++n)
        CHECK_CIDX_THROWS(list_iostore_directory(std::span<const std::uint8_t>(blob.data(), n)));
    // Directory count larger than the data, file count larger than the data.
    Bytes huge = ansi("../../../"); put32(huge, 0x7fffffff);
    CHECK_CIDX_THROWS(list_iostore_directory(huge));
    Bytes many_strings = encode(from_tree(Node{"", {}, {}}));
    many_strings.resize(many_strings.size() - 4); put32(many_strings, 200001);
    CHECK_CIDX_THROWS(list_iostore_directory(many_strings));
    // Path longer than 16 KiB.
    Node deep{"", {}, {}};
    Node* at = &deep;
    for (int i = 0; i < 6; ++i) { at->kids.push_back(Node{std::string(3000, 'a' + i), {}, {}}); at = &at->kids.back(); }
    at->files.push_back("f.uasset");
    CHECK_CIDX_THROWS(list_iostore_directory(encode(from_tree(deep))));
    // Oversized blob.
    Bytes big(8u * 1024u * 1024u + 1, 0);
    CHECK_CIDX_THROWS(list_iostore_directory(big));
}

// ========================================================================= header
TEST(cidx_header_location_unflagged_signatures) {
    const auto loc = read_utoc_directory_location(toc_file(Toc{}, tables));
    CHECK_EQ(u(loc, "version"), 8u);
    CHECK_EQ(u(loc, "container_flags"), 8u);
    CHECK(!loc.at("encrypted").get<bool>());
    CHECK(loc.at("indexed").get<bool>());
    CHECK_EQ(u(loc, "directory_index_offset"), 144u + tables);   // 264
    CHECK_EQ(u(loc, "directory_index_bytes"), 100u);
    // Only the 144 byte header is required when nothing is signed.
    const Bytes only_header = header(Toc{});
    CHECK_EQ(u(read_utoc_directory_location(only_header), "directory_index_offset"), 264u);
}

TEST(cidx_header_location_flag_meanings) {
    Toc t;
    t.flags = 1 | 8;   // Compressed + Indexed
    auto loc = read_utoc_directory_location(header(t));
    CHECK(!loc.at("encrypted").get<bool>() && loc.at("indexed").get<bool>());
    t.flags = 2 | 8;   // Encrypted + Indexed
    loc = read_utoc_directory_location(header(t));
    CHECK(loc.at("encrypted").get<bool>() && loc.at("indexed").get<bool>());
    CHECK_EQ(u(loc, "container_flags"), 10u);
    CHECK_EQ(u(loc, "directory_index_offset"), 264u);
    t.flags = 1; t.dir_bytes = 0;   // not indexed
    loc = read_utoc_directory_location(header(t));
    CHECK(!loc.at("indexed").get<bool>());
    CHECK_EQ(u(loc, "directory_index_bytes"), 0u);
    t.flags = 0;
    CHECK(!read_utoc_directory_location(header(t)).at("indexed").get<bool>());
}

TEST(cidx_header_location_signed_section) {
    Toc t;
    t.flags = 4 | 8;   // Signed + Indexed
    // hash size 5: 4 + 2*5 + 20*3 blocks = 74 bytes before the directory index.
    Bytes tail; put32(tail, 5);
    const auto loc = read_utoc_directory_location(toc_file(t, tables, tail));
    CHECK_EQ(u(loc, "directory_index_offset"), 144u + tables + 4 + 10 + 60);
    // The 4-byte hash size must be inside the capture.
    CHECK_CIDX_THROWS(read_utoc_directory_location(toc_file(t, tables)));
    CHECK_CIDX_THROWS(read_utoc_directory_location(header(t)));
    Bytes zero; put32(zero, 0);
    CHECK_CIDX_THROWS(read_utoc_directory_location(toc_file(t, tables, zero)));
    Bytes huge; put32(huge, 0x7fffffff);
    CHECK_CIDX_THROWS(read_utoc_directory_location(toc_file(t, tables, huge)));
    // Signed + Encrypted + Compressed + Indexed.
    t.flags = 15;
    CHECK_EQ(u(read_utoc_directory_location(toc_file(t, tables, tail)), "directory_index_offset"), 144u + tables + 74);
    // Signed without Indexed is still located (directory bytes zero).
    t.flags = 4; t.dir_bytes = 0;
    const auto unindexed = read_utoc_directory_location(toc_file(t, tables, tail));
    CHECK(!unindexed.at("indexed").get<bool>());
    CHECK_EQ(u(unindexed, "directory_index_offset"), 144u + tables + 74);
}

TEST(cidx_header_location_geometry_variants) {
    Toc t;
    t.chunks = 0; t.blocks = 0; t.methods = 0; t.method_bytes = 0; t.seeds = 0; t.overflow = 0;
    CHECK_EQ(u(read_utoc_directory_location(header(t)), "directory_index_offset"), 144u);
    t.chunks = 1000; t.blocks = 7; t.methods = 3; t.method_bytes = 16; t.seeds = 250; t.overflow = 9;
    // 144 + 1000*12 ids + 1000*10 offset/lengths + 250*4 seeds + 9*4 overflow + 7*12 blocks + 3*16 methods.
    CHECK_EQ(u(read_utoc_directory_location(header(t)), "directory_index_offset"), 23312u);
}

TEST(cidx_header_location_rejections) {
    CHECK_CIDX_THROWS(read_utoc_directory_location(Bytes{}));
    const auto good = header(Toc{});
    CHECK_CIDX_THROWS(read_utoc_directory_location(std::span<const std::uint8_t>(good.data(), 143)));
    { Toc t; t.magic = "-==--==--==--==="; t.magic += 'x'; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.version = 7; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.version = 9; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.header_size = 128; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.flags = 0x10 | 8; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.flags = 0x80 | 8; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.block_entry = 8; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.method_bytes = 0; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.block_size = 0; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.partitions = 0; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.partitions = 17; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.partition_bytes = 0; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.partition_bytes = UINT64_MAX; t.partitions = 2; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.partition_bytes = UINT64_MAX; t.partitions = 1; (void)read_utoc_directory_location(header(t)); }
    { Toc t; t.chunks = 1048577; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.blocks = 4194305; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.methods = 17; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.method_bytes = 65; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }
    { Toc t; t.flags = 1; t.dir_bytes = 5; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }   // bytes without Indexed
    { Toc t; t.dir_bytes = 0xFFFFFFFFu; CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }      // beyond 256 MiB
    { Toc t; t.chunks = 1048576; t.blocks = 4194304; t.dir_bytes = 200u * 1024u * 1024u;
      CHECK_CIDX_THROWS(read_utoc_directory_location(header(t))); }                                        // layout end beyond 256 MiB
}

// =================================================================== package paths
TEST(cidx_package_path_table) {
    struct Row { const char* path; const char* expected; };   // expected nullptr = not a package
    const Row rows[] = {
        {"../../../Townfall/Content/Items/BP_A.uasset", "/Game/Items/BP_A"},
        {"../../../Townfall/Content/Maps/L1.umap", "/Game/Maps/L1"},
        {"../../../Townfall/Content/Root.uasset", "/Game/Root"},
        {"../../../Townfall/Content/A-b/C_d1.uasset", "/Game/A-b/C_d1"},
        {"../../../Townfall/Plugins/Mod/Content/M.uasset", "/Mod/M"},
        {"../../../Townfall/Plugins/GameFeatures/Cool/Content/X/Y.uasset", "/Cool/X/Y"},
        {"../../../Townfall/Plugins/Mod/Content/Content/Z.uasset", "/Mod/Content/Z"},
        {"../../../Engine/Content/E.uasset", "/Engine/E"},
        {"../../../Engine/Content/Maps/Template.umap", "/Engine/Maps/Template"},
        {"../../../Engine/Plugins/Runtime/Fx/Content/F.uasset", "/Fx/F"},
        {"../../../Engine/Plugins/Fx/Content/F.uasset", "/Fx/F"},
        // not packages
        {"../../../Townfall/Content/Items/BP_A.uexp", nullptr},
        {"../../../Townfall/Content/Items/BP_A.ubulk", nullptr},
        {"../../../Townfall/Content/Items/BP_A.uasset.bak", nullptr},
        {"../../../Townfall/Content/Items/BP_A.UASSET", nullptr},
        {"../../../Townfall/Content/Items/BP_A.Umap", nullptr},
        {"../../../Townfall/Content/.uasset", nullptr},
        {"../../../Townfall/Content/Items/.umap", nullptr},
        {"../../../Townfall/Config/DefaultGame.ini", nullptr},
        {"../../../Townfall/Config/X.uasset", nullptr},
        {"../../../Townfall/content/X.uasset", nullptr},            // exact case
        {"../../../townfall/Content/X.uasset", nullptr},
        {"../../../engine/Content/X.uasset", nullptr},
        {"../../../Townfall/plugins/Mod/Content/X.uasset", nullptr},
        {"../../../Townfall/Plugins/Mod/content/X.uasset", nullptr},
        {"../../../Other/Content/X.uasset", nullptr},               // other project
        {"../../../Townfall/Plugins/Content/X.uasset", nullptr},    // no plugin name
        {"../../../Townfall/Plugins/Mod/Content/", nullptr},
        {"../../../Townfall/Plugins/Mod/Content.uasset", nullptr},  // no Content directory
        {"../../../Townfall/Plugins/Mod/X.uasset", nullptr},
        {"../../../Townfall/Content.uasset", nullptr},
        {"../../../Townfall.uasset", nullptr},
        {"../../Townfall/Content/X.uasset", nullptr},               // wrong prefix
        {"Townfall/Content/X.uasset", nullptr},
        {"/Game/X.uasset", nullptr},
        {"../../../Townfall/Content//X.uasset", nullptr},
        {"../../../Townfall/Content/../X.uasset", nullptr},
        {"../../../Townfall/Content/./X.uasset", nullptr},
        {"..\\..\\..\\Townfall\\Content\\X.uasset", nullptr},
        {"../../../Townfall/Content/My Item.uasset", nullptr},      // outside the profile grammar
        {"../../../Townfall/Content/A.B.uasset", nullptr},
        {"", nullptr},
    };
    for (const auto& row : rows) {
        const auto got = package_path_from_virtual(row.path, "Townfall");
        if (row.expected) { CHECK(got.has_value()); CHECK_EQ(*got, std::string(row.expected)); }
        else CHECK(!got.has_value());
    }
    // The project is matched exactly, other projects' paths are not packages.
    CHECK(!package_path_from_virtual("../../../Townfall/Content/X.uasset", "Other").has_value());
    CHECK_EQ(*package_path_from_virtual("../../../Other/Content/X.uasset", "Other"), "/Game/X");
    CHECK(!package_path_from_virtual("../../../Townfall/Content/" + std::string(2000, 'a') + ".uasset", "Townfall").has_value());
    CHECK_CIDX_THROWS(package_path_from_virtual("../../../X/Content/Y.uasset", ""));
    CHECK_CIDX_THROWS(package_path_from_virtual("../../../X/Content/Y.uasset", "a/b"));
    CHECK_CIDX_THROWS(package_path_from_virtual("../../../Engine/Content/Y.uasset", "Engine"));
}

// ================================================================ content index
TEST(cidx_build_completeness_flags) {
    const Json listings = Json::array({listing("../../../", {"../../../Townfall/Content/B.uasset",
        "../../../Townfall/Content/A.uasset", "../../../Townfall/Content/A.uexp"})});
    const Json fn = Json::array({Json{{"name", "/Script/Engine.Actor:K2_DestroyActor"}, {"address", "0x10"}},
                                 Json{{"name", "/Script/Engine.Actor:GetName"}, {"address", "0x20"}},
                                 Json{{"name", "/Game/X/BP_Y.BP_Y_C:Go"}, {"address", "0x30"}}});
    // No dump, listings not complete.
    auto out = build_content_index(inputs(listings, false));
    CHECK_EQ(out.dump(), R"({"assets":["/Game/A","/Game/B"],"classes":[],"complete":{"assets":false,"classes":false,"functions":false},"console_content_version":"02.003.000","functions":[]})");
    // Complete assets, dump without classes: functions complete, classes not.
    out = build_content_index(inputs(listings, true, dump(fn)));
    CHECK(out.at("complete").at("assets").get<bool>());
    CHECK(out.at("complete").at("functions").get<bool>());
    CHECK(!out.at("complete").at("classes").get<bool>());
    CHECK_EQ(out.at("classes").dump(), R"(["/Game/X/BP_Y.BP_Y_C","/Script/Engine.Actor"])");
    CHECK_EQ(out.at("functions").dump(),
             R"(["/Game/X/BP_Y.BP_Y_C:Go","/Script/Engine.Actor:GetName","/Script/Engine.Actor:K2_DestroyActor"])");
    // Dump with a classes array: classes complete, merged with function classes, sorted unique.
    auto with_classes = dump(fn);
    with_classes["classes"] = Json::array({"/Script/Engine.Pawn", "/Script/Engine.Actor", "/Script/Engine.Pawn"});
    out = build_content_index(inputs(listings, true, with_classes));
    CHECK(out.at("complete").at("classes").get<bool>());
    CHECK_EQ(out.at("classes").dump(),
             R"(["/Game/X/BP_Y.BP_Y_C","/Script/Engine.Actor","/Script/Engine.Pawn"])");
    // An empty classes array still counts as "has classes".
    with_classes["classes"] = Json::array();
    CHECK(build_content_index(inputs(listings, false, with_classes)).at("complete").at("classes").get<bool>());
    CHECK(!build_content_index(inputs(listings, false, with_classes)).at("complete").at("assets").get<bool>());
    // Missing reflection_dump key behaves like null; no listings and not complete is allowed.
    Json bare = {{"console_content_version", "02.003.000"}, {"project", "Townfall"},
                 {"listings", Json::array()}, {"listings_complete", false}};
    out = build_content_index(bare);
    CHECK_EQ(out.at("assets").size(), 0u);
    // Several listings merge; plugin/engine assets included; non-packages dropped; names outside grammar dropped.
    const Json many = Json::array({
        listing("../../../Townfall/", {"../../../Townfall/Content/A.uasset", "../../../Townfall/Plugins/Mod/Content/M.uasset",
                                        "../../../Townfall/Config/x.ini"}),
        listing("../../../", {"../../../Engine/Content/E.uasset", "../../../Townfall/Content/A.uasset",
                              "../../../Townfall/Content/Bad Name.uasset"})});
    CHECK_EQ(build_content_index(inputs(many, true)).at("assets").dump(),
             R"(["/Engine/E","/Game/A","/Mod/M"])");
}

TEST(cidx_build_rejections) {
    const Json ok_listing = Json::array({listing("../../../", {"../../../Townfall/Content/A.uasset"})});
    CHECK_EQ(build_content_index(inputs(ok_listing, true)).at("assets").size(), 1u);
    const auto with = [&](auto&& mutate) { Json in = inputs(ok_listing, true); mutate(in); return in; };
    CHECK_CIDX_THROWS(build_content_index(Json::array()));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["extra"] = 1; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j.erase("project"); })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j.erase("listings"); })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j.erase("listings_complete"); })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j.erase("console_content_version"); })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["console_content_version"] = "2.3.0"; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["console_content_version"] = 2; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["project"] = ""; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["project"] = "a/b"; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["project"] = 5; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings_complete"] = "yes"; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"] = Json::object(); })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"] = Json::array(); })));  // complete + none
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["extra"] = 1; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["mount_point"] = "../../.."; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["mount_point"] = 7; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0].erase("files"); })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["files"] = "x"; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["files"][0] = 4; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["files"][0] = "Other/Content/A.uasset"; })));
    CHECK_CIDX_THROWS(build_content_index(with([](Json& j) { j["listings"][0]["files"][0] = "../../../"; })));
    // Reflection dump rules.
    const Json fns = Json::array({Json{{"name", "/Script/Engine.Actor:Go"}, {"address", "0x10"}}});
    CHECK_EQ(build_content_index(inputs(ok_listing, true, dump(fns))).at("functions").size(), 1u);
    CHECK_THROWS(build_content_index(inputs(ok_listing, true, dump(fns, "pc"))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, dump(fns, "pc"))));
    CHECK_THROWS(build_content_index(inputs(ok_listing, true, Json::object())));        // validate_reflection_dump
    CHECK_THROWS(build_content_index(inputs(ok_listing, true, "dump")));
    const auto with_classes = [&](Json classes) { auto d = dump(fns); d["classes"] = std::move(classes); return d; };
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::object()))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({5})))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({"Engine.Actor"})))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({"/Engine/X.Actor"})))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({"/Script/Engine.Actor:Go"})))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({"/Script/Engine"})))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({"/Script/Engine."})))));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(Json::array({"/Script/" + std::string(1100, 'a') + ".B"})))));
    Json many = Json::array();
    for (int i = 0; i < 500001; ++i) many.push_back("/Script/Engine.C" + std::to_string(i));
    CHECK_CIDX_THROWS(build_content_index(inputs(ok_listing, true, with_classes(many))));
    many.erase(many.size() - 1);
    CHECK_EQ(build_content_index(inputs(ok_listing, true, with_classes(many))).at("classes").size(),
             500001u);   // 500000 listed + the class of the one function
}

TEST(cidx_build_skips_function_names_outside_profile_grammar) {
    const Json fns = Json::array({Json{{"name", "/Game/Blueprints/Hero_C:Jump"}, {"address", "0x1"}},     // no .Object
                                  Json{{"name", "/Script/Engine.Actor:Go"}, {"address", "0x2"}},
                                  Json{{"name", "/Script/Engine.A.B:Go"}, {"address", "0x3"}}});             // dotted object
    const auto out = build_content_index(inputs(Json::array(), false, dump(fns)));
    CHECK_EQ(out.at("functions").dump(), R"(["/Script/Engine.Actor:Go"])");
    CHECK_EQ(out.at("classes").dump(), R"(["/Script/Engine.Actor"])");
    CHECK(out.at("complete").at("functions").get<bool>());
}

TEST(cidx_build_output_validates_as_profile_content_index) {
    const Json fns = Json::array({Json{{"name", "/Script/Engine.Actor:K2_DestroyActor"}, {"address", "0x10"}},
                                  Json{{"name", "/Game/Items/BP_A.BP_A_C:Use"}, {"address", "0x20"}}});
    auto d = dump(fns);
    d["classes"] = Json::array({"/Script/Engine.Pawn"});
    const Json out = build_content_index(inputs(Json::array({listing("../../../", {
        "../../../Townfall/Content/Items/BP_A.uasset", "../../../Townfall/Plugins/Mod/Content/M.uasset",
        "../../../Engine/Content/E.uasset"})}), true, d));
    const Json profile = validate_game_profile(profile_with(out));
    CHECK_EQ(profile.at("content_index").at("assets").dump(), R"(["/Engine/E","/Game/Items/BP_A","/Mod/M"])");
    const auto checked = check_references(
        refs(Json::array({"/Game/Items/BP_A", "/Mod/M", "/Engine/E"}),
             Json::array({"/Script/Engine.Pawn", "/Game/Items/BP_A.BP_A_C"}),
             Json::array({"/Script/Engine.Actor:K2_DestroyActor"})), profile);
    CHECK_EQ(checked.at("status").get<std::string>(), "all_present");
    CHECK_EQ(checked.at("checked").get<int>(), 6);
    // Complete asset and class and function lists make absence a definite "missing".
    const auto missing = check_references(refs(Json::array({"/Game/Items/Gone"}), Json::array({"/Script/Engine.Nope"}),
                                               Json::array({"/Script/Engine.Actor:Nope"})), profile);
    CHECK_EQ(missing.at("status").get<std::string>(), "missing");
    CHECK_EQ(missing.at("missing").at("assets").size(), 1u);
    CHECK_EQ(missing.at("missing").at("classes").size(), 1u);
    CHECK_EQ(missing.at("missing").at("functions").size(), 1u);
    // Incomplete assets: absence is only unverified.
    const Json partial = build_content_index(inputs(Json::array(), false));
    const auto unverified = check_references(refs(Json::array({"/Game/Items/Gone"})), validate_game_profile(profile_with(partial)));
    CHECK_EQ(unverified.at("status").get<std::string>(), "unverified");
}

// =============================================== header -> index -> compat roundtrip
TEST(cidx_end_to_end_header_index_compat) {
    const Bytes index = encode(from_tree(townfall()));
    Toc t;
    t.dir_bytes = static_cast<std::uint32_t>(index.size());
    Bytes file = toc_file(t, tables);
    put_raw(file, index);
    file.resize(file.size() + t.chunks * 24, 0);   // chunk metadata
    const auto loc = read_utoc_directory_location(file);
    CHECK(loc.at("indexed").get<bool>() && !loc.at("encrypted").get<bool>());
    const auto offset = static_cast<std::size_t>(u(loc, "directory_index_offset"));
    const auto bytes = static_cast<std::size_t>(u(loc, "directory_index_bytes"));
    CHECK_EQ(offset, 144u + tables);
    CHECK_EQ(bytes, index.size());
    CHECK(offset + bytes <= file.size());
    const Bytes slice(file.begin() + static_cast<std::ptrdiff_t>(offset), file.begin() + static_cast<std::ptrdiff_t>(offset + bytes));
    CHECK(slice == index);
    const Json listed = list_iostore_directory(slice);

    // A signed container stores the same index after the signature section.
    Toc signed_toc = t;
    signed_toc.flags = 4 | 8;
    Bytes tail; put32(tail, 4);
    Bytes signed_file = toc_file(signed_toc, tables, tail);
    signed_file.resize(144 + tables + 4 + 8 + 60, 0);
    put_raw(signed_file, index);
    const auto signed_loc = read_utoc_directory_location(signed_file);
    const auto signed_offset = static_cast<std::size_t>(u(signed_loc, "directory_index_offset"));
    CHECK_EQ(signed_offset, 144u + tables + 72);
    CHECK(list_iostore_directory(std::span<const std::uint8_t>(signed_file.data() + signed_offset, index.size())) == listed);

    const Json reflection = dump(Json::array({Json{{"name", "/Script/Engine.Actor:K2_DestroyActor"}, {"address", "0x10"}}}));
    const Json content = build_content_index(inputs(Json::array({listed}), true, reflection));
    CHECK_EQ(content.at("assets").dump(), R"(["/Engine/E","/Fx/F","/Game/Items/BP_A","/Game/Maps/L1","/Game/Root","/Mod/M"])");
    const Json profile = validate_game_profile(profile_with(content));
    const auto present = check_references(refs(Json::array({"/Game/Items/BP_A", "/Mod/M", "/Fx/F"})), profile);
    CHECK_EQ(present.at("status").get<std::string>(), "all_present");
    const auto absent = check_references(refs(Json::array({"/Game/Items/BP_Missing", "/Game/Items/BP_A"})), profile);
    CHECK_EQ(absent.at("status").get<std::string>(), "missing");
    CHECK_EQ(absent.at("missing").at("assets").dump(), R"(["/Game/Items/BP_Missing"])");
    CHECK_EQ(absent.at("present").get<int>(), 1);
}
