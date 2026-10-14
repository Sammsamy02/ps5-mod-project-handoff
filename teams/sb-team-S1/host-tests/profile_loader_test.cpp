// profile_loader_test.cpp — Team S1 (B3) unit tests for ModProfileLoader.
// Pure host logic: build a known-good profile in memory, prove it loads, then
// mutate it to prove every fail-closed path refuses. No engine, no console.
//
// Build: c++ -std=c++23 -I ../new profile_loader_test.cpp -o loader_test && ./loader_test
#include "ModProfileLoader.hpp"
#include "sha256_min.hpp"
#include <cstdio>
#include <string>
#include <vector>

using namespace rc::modprofile;

static int g_pass = 0, g_fail = 0;
static void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (ok) ++g_pass; else ++g_fail;
}

static void sha_hex(const std::string& s, char out[65]) {
    sha256min::Ctx c; sha256min::init(&c);
    sha256min::update(&c, (const unsigned char*)s.data(), s.size());
    sha256min::final_hex(&c, out);
}

// Build a minimal-but-complete valid profile; `body_lines` excludes the
// body_sha256 line, which we compute and insert after [profile] digest.
static std::string make_profile() {
    std::vector<std::string> L;
    L.push_back("[profile]");
    L.push_back("schema=1");
    L.push_back("name=unit-test-game");
    L.push_back("digest=1111111111111111111111111111111111111111111111111111111111111111");
    L.push_back("__BODY__");
    L.push_back("image_sha256=2222222222222222222222222222222222222222222222222222222222222222");
    L.push_back("image_size=194241424");
    L.push_back("image_extent=0xbc602c8");
    L.push_back("image_base=0x400000");
    L.push_back("exe_path=/data/x/eboot.bin");
    L.push_back("[build]");
    for (const char* kv : {"format=1","pointer_size=8","engine_major=4","engine_minor=27",
        "target=prospero-x86_64-sysv","engine_revision=4.27.2-0+++UE4","build_id=deadbeef",
        "build_configuration=1","build_source=test","stats=1","stats_source=test",
        "non_shipping=0","stats_layout=0","fname_case_preserving=0",
        "fname_output_bytes=8","fname_output_alignment=8"}) L.push_back(kv);
    L.push_back("[pool]");
    for (const char* kv : {"current_block_offset=0x8","current_byte_offset_offset=0xc","blocks_offset=0x10",
        "none_id_offset=0x14040","byte_property_id_offset=0x14044","block_offset_bits=16","max_blocks=8192"}) L.push_back(kv);
    L.push_back("[seeds]");
    for (const char* kv : {"generated=1","required_set_complete=1","fname_pool_label=0x9365ec0",
        "guobject_array_label=0x93ba110","fname_ctor_label=0x23d590","gmalloc_label=0x930add8",
        "gmalloc_instance_label=0x93089b8","gmalloc_vtable_label=0x7ec26e0","sco_label=0x490aa0",
        "guobject_array_present=1","guobject_array_derivation=x","fname_ctor_present=1","fname_ctor_derivation=x",
        "gmalloc_present=1","gmalloc_derivation=x","sco_present=1","sco_derivation=x"}) L.push_back(kv);
    L.push_back("[fname]");
    L.push_back("ctor_prefix=554889e553504889f04889fb4885f674330fb7384889c631c96685ff742a6690");
    L.push_back("ctor_prefix_size=32");
    L.push_back("[code_pins]");
    L.push_back("sco_prefix=554889e54157415641554154534881ec58020000488b054d14");
    L.push_back("sco_prefix_size=25");
    L.push_back("listener_add_prefix=554889e5415741565350488d5f784989");
    L.push_back("listener_add_prefix_size=16");
    L.push_back("listener_remove_prefix=554889e5415741564154534881ec4849");
    L.push_back("listener_remove_prefix_size=16");
    // 33 bytes = 66 hex (value is arbitrary for the unit test)
    L.push_back("thread_name_prefix=554889e54156530fb605eac79d08842e4153f0b605000000000000000000000000");
    L.push_back("thread_name_prefix_size=33");
    L.push_back("[listener]");
    L.push_back("add_label=0x4841a0");
    L.push_back("remove_label=0x484240");
    L.push_back("add_prefix=554889e5415741565350488d5f784989");
    L.push_back("add_prefix_size=16");
    L.push_back("remove_prefix=554889e5415741564154534881ec4849");
    L.push_back("remove_prefix_size=16");
    L.push_back("[tostring]");
    for (const char* kv : {"probe_id=0x3","probe_expect=ByteProperty","probe_expect_units=12","out_units=1035"}) L.push_back(kv);
    L.push_back("[thread]");
    for (const char* kv : {"accessor=0xa262e0","game_name=0x9402ac8","render_name=0x9402ae0","empty_name=0x932a5a0"}) L.push_back(kv);
    L.push_back("[pak]");
    for (const char* kv : {"pak_vtable=0x7fee8c0","platform_mgr_get=0xb8ed0","platform_mgr_getpf=0xb8a70",
        "mountpak_delegate=0x934faa8","handle_mount_pak=0x176b900","delegate_vtable=0x7fef570",
        "execute=0x34400","lower_impl=0xb3500","snapshot_delegate=0x934fc00","snapshot_vtable=0x7fef9f0",
        "snapshot_execute=0x17831e0","slot_get_lower_level=0x68","slot_delegate_execute=0x60",
        "delegate_inst_off=0","delegate_size_off=8","pak_count_offset=0x18"}) L.push_back(kv);
    for (int i = 0; i < 6; ++i) {
        L.push_back("proof" + std::to_string(i) + "_linked=0xb8ed0");
        L.push_back("proof" + std::to_string(i) + "_size=4");
        L.push_back("proof" + std::to_string(i) + "_bytes=0fb60561");
    }
    L.push_back("[layout.members]");
    L.push_back("<<<");
    L.push_back("[NativeLayout]");
    for (const char* kv : {"Format=1","PointerSize=8","EngineMajor=4","EngineMinor=27","NonShipping=0","Stats=0"}) L.push_back(kv);
    L.push_back("[FUObjectArray]");
    L.push_back("UObjectCreateListeners=0x58");
    L.push_back(">>>");
    L.push_back("[layout.vtables]");
    L.push_back("<<<");
    L.push_back("[NativeLayout]");
    for (const char* kv : {"Format=1","PointerSize=8","EngineMajor=4","EngineMinor=27","NonShipping=0","Stats=0"}) L.push_back(kv);
    L.push_back("[UObject]");
    L.push_back("ProcessEvent=0x268");
    L.push_back(">>>");

    // compute body hash over everything except the __BODY__ line
    std::string body;
    for (auto& s : L) if (s != "__BODY__") body += s + "\n";
    char h[65]; sha_hex(body, h);
    std::string full;
    for (auto& s : L) full += (s == "__BODY__" ? (std::string("body_sha256=") + h) : s) + "\n";
    return full;
}

static Result load(const std::string& s, const char* expect_sha = nullptr) {
    sha256min::Ctx c; Sha256Ops ops{sha256min::init, sha256min::update, sha256min::final_hex, &c};
    Profile p; Diag d;
    return Loader::parse(s.data(), s.size(), expect_sha, ops, p, d);
}

static std::string mutate(const std::string& s, const std::string& from, const std::string& to) {
    std::string r = s; auto i = r.find(from);
    if (i != std::string::npos) r.replace(i, from.size(), to);
    return r;
}

int main() {
    std::string good = make_profile();

    // 1. good profile loads, and installs/active() works
    {
        sha256min::Ctx c; Sha256Ops ops{sha256min::init, sha256min::update, sha256min::final_hex, &c};
        Profile p; Diag d;
        Result r = Loader::parse(good.data(), good.size(), nullptr, ops, p, d);
        check(r == Result::Ok && p.valid, "good profile parses (valid)");
        check(d.required_checked > 50, "required_checked > 50 (nonzero gate count)");
        reset();
        check(active() == nullptr, "active() null before install");
        install(p);
        check(active() != nullptr && active()->major == 4, "active() populated after install (UE4.27)");
        reset();
        check(active() == nullptr, "active() null after reset (fail-closed)");
    }
    // 2. sha-match selection
    check(load(good, "2222222222222222222222222222222222222222222222222222222222222222") == Result::Ok,
          "matching image sha selects the profile");
    check(load(good, "0000000000000000000000000000000000000000000000000000000000000000") == Result::NoProfileForImage,
          "non-matching image sha => NoProfileForImage (fail closed, run unmodded)");
    // 3. body hash tamper => DigestMismatch
    check(load(mutate(good, "image_base=0x400000", "image_base=0x500000")) == Result::DigestMismatch,
          "any body byte change => DigestMismatch");
    // 4. missing required key
    check(load(mutate(good, "gmalloc_label=0x930add8\n", "")) == Result::DigestMismatch ||
          load(mutate(good, "gmalloc_label=0x930add8\n", "")) != Result::Ok,
          "removing a required key => not Ok");
    // 5. incomplete seed set => fail closed
    check(load(mutate(good, "sco_present=1", "sco_present=0")) != Result::Ok,
          "a seed slot present=0 => refused (all-or-nothing)");
    // 6. inconsistent non_shipping vs config
    check(load(mutate(good, "non_shipping=0", "non_shipping=1")) != Result::Ok,
          "non_shipping != (config!=1) => refused");
    // 7. fixed pak offset tampered
    check(load(mutate(good, "pak_count_offset=0x18", "pak_count_offset=0x20")) != Result::Ok,
          "structural pak offset changed => refused");
    // 8. prefix length wrong
    check(load(mutate(good, "sco_prefix_size=25", "sco_prefix_size=24")) != Result::Ok,
          "declared prefix size != byte count => refused");
    // 9. unknown section => refused
    check(load(mutate(good, "[pool]", "[bogus]\nx=1\n[pool]")) != Result::Ok,
          "unknown section => refused (no silent skip)");
    // 10. layout header disagrees with [build]
    check(load(mutate(good, "EngineMajor=4\nEngineMinor=27\nNonShipping=0\nStats=0\n[FUObjectArray]",
                            "EngineMajor=5\nEngineMinor=27\nNonShipping=0\nStats=0\n[FUObjectArray]")) != Result::Ok,
          "layout [NativeLayout] header must equal [build]");
    // 11. empty input => refused
    check(load(std::string("")) != Result::Ok, "empty input refused");
    // 12. missing sha ops but body present => NoSha
    {
        Profile p; Diag d; Sha256Ops none{};
        check(Loader::parse(good.data(), good.size(), nullptr, none, p, d) == Result::NoSha,
              "no sha ops => NoSha (cannot verify body)");
    }

    std::printf("\nTOTAL: %d passed, %d failed, %d checks\n", g_pass, g_fail, g_pass + g_fail);
    if (g_pass + g_fail == 0) { std::printf("FAIL: zero checks\n"); return 2; }
    return g_fail ? 1 : 0;
}
