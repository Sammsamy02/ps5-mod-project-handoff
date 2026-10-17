// cap02 host test: the generic first-launch generator reproduces Townfall's
// console-proven layout OFFSET rows exactly, selects fail-closed on unknown
// engine cells, and reads the GNU build-id note generically.
//
// "Reproduce exactly" is checked against the R11 shipped pair's section/offset
// rows (the addresses that must be correct). The header provenance lines
// (EngineRevision/BuildId) are image-derived and checked separately; see
// NOTES.md on the build-id note width.
//
//   /usr/bin/clang++ -std=c++20 -I ../PATCH/new-files test_autoprofile.cpp -o t && ./t
#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "AutoProfileGenerator.hpp"

using namespace rc::autoprofile;

static int g_checks = 0;
#define CHECK(c, msg) do { ++g_checks; if (!(c)) { std::printf("FAIL: %s\n", msg); return 1; } } while (0)

// Parse an INI pair side into {section -> {key -> value}} ignoring the header.
static std::map<std::string, std::map<std::string, uint32_t>> parse_sections(const std::string& text) {
    std::map<std::string, std::map<std::string, uint32_t>> out;
    std::istringstream in(text);
    std::string line, sec;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.front() == '[') { sec = line.substr(1, line.size() - 2); continue; }
        if (sec == "NativeLayout") continue;
        auto eq = line.find('=');
        out[sec][line.substr(0, eq)] = uint32_t(std::stoul(line.substr(eq + 1), nullptr, 16));
    }
    return out;
}

static std::string slurp(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}

int main(int argc, char** argv) {
    const std::string evdir = argc > 1 ? argv[1] : "../evidence";

    // T1: exact-cell selection returns the one shipped UE5.6 pack.
    EngineCell tf{1, 8, 5, 6, "prospero-x86_64-sysv", false, false};
    const Pack* p = select_pack(tf);
    CHECK(p != nullptr, "UE5.6 shipping/stats-off pack must be selected");
    CHECK(std::string(p->id) == "ue5.6-prospero-x86_64-sysv-shipping-statsoff", "pack id");

    // T2: rendering with image-derived identity succeeds (compare-only path).
    ImageIdentity id{"5.6.1-0+++UE5", "04bb4330a7d71dcefe1cc2c4d19769d500000000"};
    Generated g = render(tf, id);
    CHECK(g.ok, "render must succeed for a known cell with valid identity");

    // T3: every member/vtable OFFSET ROW equals the R11 shipped pair exactly.
    auto gen_m = parse_sections(g.members);
    auto gen_v = parse_sections(g.vtables);
    auto r11_m = parse_sections(slurp(evdir + "/r11-members.ini"));
    auto r11_v = parse_sections(slurp(evdir + "/r11-vtables.ini"));
    CHECK(!r11_m.empty() && !r11_v.empty(), "R11 reference INIs must load");
    CHECK(gen_m == r11_m, "generated member offset rows must byte-equal R11");
    CHECK(gen_v == r11_v, "generated vtable offset rows must byte-equal R11");
    int member_rows = 0, vtable_rows = 0;
    for (auto& s : gen_m) member_rows += int(s.second.size());
    for (auto& s : gen_v) vtable_rows += int(s.second.size());

    // T4: FAIL-CLOSED on an unknown engine era (UE4.26 has no shipped pack).
    EngineCell ue426{1, 8, 4, 26, "prospero-x86_64-sysv", false, false};
    CHECK(select_pack(ue426) == nullptr, "UE4.26 must have no shipped pack (fail-closed)");
    Generated g426 = generate(ue426, id);
    CHECK(!g426.ok, "UE4.26 generation must refuse");
    CHECK(!render(ue426, id).ok, "UE4.26 render must refuse");

    // T5: FAIL-CLOSED on cell deltas (stats on, non-shipping, wrong target, wrong minor).
    CHECK(select_pack({1, 8, 5, 6, "prospero-x86_64-sysv", false, true}) == nullptr, "stats-on refuses");
    CHECK(select_pack({1, 8, 5, 6, "prospero-x86_64-sysv", true, false}) == nullptr, "non-shipping refuses");
    CHECK(select_pack({1, 8, 5, 6, "win64-x86_64-msvc", false, false}) == nullptr, "wrong target refuses");
    CHECK(select_pack({1, 8, 5, 5, "prospero-x86_64-sysv", false, false}) == nullptr, "wrong minor refuses");
    CHECK(select_pack({1, 4, 5, 6, "prospero-x86_64-sysv", false, false}) == nullptr, "wrong pointer size refuses");

    // T6: invalid live identity refuses even for a known cell (fail-closed).
    CHECK(!render(tf, ImageIdentity{"", "04bb"}).ok, "empty revision refuses");
    CHECK(!render(tf, ImageIdentity{"5.6.1-0+++UE5", std::string("bad\x01id")}).ok, "non-printable build_id refuses");

    // T7: generic GNU build-id note reader reproduces the Townfall note bytes.
    // Minimal NT_GNU_BUILD_ID note: namesz=4, descsz=20, type=3, "GNU\0", 20-byte desc.
    std::vector<uint8_t> note;
    auto put32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) note.push_back(uint8_t(v >> (8 * i))); };
    put32(4); put32(20); put32(3);
    const char* nm = "GNU"; for (int i = 0; i < 3; ++i) note.push_back(uint8_t(nm[i])); note.push_back(0);
    const uint8_t desc[20] = {0x04,0xbb,0x43,0x30,0xa7,0xd7,0x1d,0xce,0xfe,0x1c,0xc2,0xc4,0xd1,0x97,0x69,0xd5,0,0,0,0};
    for (uint8_t b : desc) note.push_back(b);
    std::string bid = read_gnu_build_id(note.data(), note.size());
    CHECK(bid == "04bb4330a7d71dcefe1cc2c4d19769d500000000", "build-id note reader reproduces the note");
    CHECK(read_gnu_build_id(note.data(), 8).empty(), "truncated note yields no id");

    // T8: shadow_check re-renders from the R11 pair's own header and matches
    // byte-for-byte (the on-console proof path). Uses the full R11 texts
    // (header included), so this also checks header reproduction.
    std::string r11_members_full = slurp(evdir + "/r11-members.ini");
    std::string r11_vtables_full = slurp(evdir + "/r11-vtables.ini");
    ShadowResult sh = shadow_check(r11_members_full, r11_vtables_full);
    CHECK(sh.parsed, "shadow must parse the R11 header");
    CHECK(sh.matched, "shadow render must byte-equal the R11 pair (header + rows)");
    CHECK(std::string(sh.pack_id) == "ue5.6-prospero-x86_64-sysv-shipping-statsoff", "shadow pack id");

    // T9 (review F5): the shipped pack has single-source / config-dependent
    // rows, so the ARMING path refuses it for every image (fail-closed) while
    // the compare-only shadow still renders it.
    CHECK(!p->arming_cleared, "pack with single-source rows must not be cleared for arming");
    CHECK(p->single_source_rows == 11u, "pack records its 11 single-source rows");
    Generated armed = generate(tf, id);
    CHECK(!armed.ok, "generate() must refuse a pack that is not cleared for arming");
    CHECK(armed.members.empty() && armed.vtables.empty(), "refused generation stages nothing");

    // T10 (review F7): a malformed header never throws; the shadow reports parsed=0.
    std::string bad = r11_members_full;
    auto at = bad.find("EngineMajor=");
    CHECK(at != std::string::npos, "R11 header has EngineMajor");
    bad.replace(at, std::string("EngineMajor=5").size(), "EngineMajor=x5");
    bool threw = false;
    ShadowResult shbad{};
    try { shbad = shadow_check(bad, r11_vtables_full); } catch (...) { threw = true; }
    CHECK(!threw, "malformed header must not throw");
    CHECK(!shbad.parsed && !shbad.matched, "malformed header reports parsed=0, no MATCH");
    std::string huge = r11_members_full;
    at = huge.find("EngineMajor=");
    huge.replace(at, std::string("EngineMajor=5").size(), "EngineMajor=99999999999999999999");
    threw = false;
    try { shbad = shadow_check(huge, r11_vtables_full); } catch (...) { threw = true; }
    CHECK(!threw && !shbad.parsed, "out-of-range number must not throw");

    if (g_checks == 0) { std::printf("FAIL: zero checks ran\n"); return 1; }
    std::printf("PASS: %d checks (member_rows=%d vtable_rows=%d, R11 exact; UE4.26 fail-closed; "
                "identity image-derived; build-id note reader ok)\n", g_checks, member_rows, vtable_rows);
    return 0;
}
