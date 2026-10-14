// dump_profile.cpp — Team S1 (B3). Load a profile .ini through the REAL
// ModProfileLoader (host build) and print every per-game value as flat
// `name=value` lines. check_equivalence.py compares this to the values the
// generator extracted from the compiled headers, proving the loader
// reproduces today's Townfall values byte-for-byte.
//
// Build (host): c++ -std=c++23 -I ../new dump_profile.cpp -o dump_profile
// Run:          ./dump_profile ../gen/townfall-profile.ini
#include "ModProfileLoader.hpp"
#include "sha256_min.hpp"
#include <cstdio>
#include <string>
#include <fstream>
#include <sstream>

using namespace rc::modprofile;

static void p_hex(const char* k, uint64_t v) { std::printf("%s=0x%llx\n", k, (unsigned long long)v); }
static void p_u(const char* k, uint64_t v) { std::printf("%s=%llu\n", k, (unsigned long long)v); }
static void p_s(const char* k, const char* v) { std::printf("%s=%s\n", k, v); }
static void p_bytes(const char* k, const Bytes& b) {
    std::printf("%s=", k);
    for (uint32_t i = 0; i < b.size; ++i) std::printf("%02x", b.data[i]);
    std::printf("\n");
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: dump_profile <profile.ini>\n"); return 2; }
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::stringstream ss; ss << in.rdbuf();
    std::string buf = ss.str();  // kept alive: layout TextRefs point into it

    sha256min::Ctx ctx;
    Sha256Ops sha{sha256min::init, sha256min::update, sha256min::final_hex, &ctx};

    Profile pr; Diag d;
    Result r = Loader::parse(buf.data(), buf.size(), /*expected_sha=*/nullptr, sha, pr, d);
    if (r != Result::Ok) {
        std::fprintf(stderr, "LOAD REFUSED: result=%d line=%d detail=%s\n", (int)r, d.line, d.detail);
        return 1;
    }
    std::fprintf(stderr, "LOAD OK: sections=%u keys=%u required_checked=%u\n",
                 d.sections_seen, d.keys_seen, d.required_checked);

    p_s("name", pr.name);
    p_s("digest", pr.digest);
    p_s("body_sha256", pr.body_sha256);
    p_s("image_sha256", pr.image_sha256);
    p_u("image_size", pr.image_size);
    p_hex("image_extent", pr.image_extent);
    p_hex("image_base", pr.image_base);
    p_s("exe_path", pr.exe_path);
    p_u("format", pr.format); p_u("pointer_size", pr.pointer_size);
    p_u("engine_major", pr.major); p_u("engine_minor", pr.minor);
    p_s("target", pr.target); p_s("engine_revision", pr.engine_revision); p_s("build_id", pr.build_id);
    p_u("build_configuration", pr.build_configuration); p_u("stats", pr.stats);
    p_s("build_source", pr.build_source); p_s("stats_source", pr.stats_source);
    p_u("non_shipping", pr.non_shipping); p_u("stats_layout", pr.stats_layout);
    p_u("fname_case_preserving", pr.fname_case_preserving);
    p_u("fname_output_bytes", pr.fname_output_bytes); p_u("fname_output_alignment", pr.fname_output_alignment);
    p_hex("pool_current_block_offset", pr.pool_current_block_offset);
    p_hex("pool_current_byte_offset_offset", pr.pool_current_byte_offset_offset);
    p_hex("pool_blocks_offset", pr.pool_blocks_offset);
    p_hex("pool_none_id_offset", pr.pool_none_id_offset);
    p_hex("pool_byte_property_id_offset", pr.pool_byte_property_id_offset);
    p_u("pool_block_offset_bits", pr.pool_block_offset_bits);
    p_u("pool_max_blocks", pr.pool_max_blocks);
    p_u("required_set_complete", pr.required_set_complete);
    p_hex("fname_pool_label", pr.fname_pool_label);
    p_hex("guobject_array_label", pr.guobject_array_label);
    p_hex("fname_ctor_label", pr.fname_ctor_label);
    p_hex("gmalloc_label", pr.gmalloc_label);
    p_hex("gmalloc_instance_label", pr.gmalloc_instance_label);
    p_hex("gmalloc_vtable_label", pr.gmalloc_vtable_label);
    p_hex("sco_label", pr.sco_label);
    p_bytes("fname_ctor_prefix", pr.fname_ctor_prefix);
    p_bytes("sco_prefix", pr.sco_prefix);
    p_bytes("listener_add_prefix", pr.listener_add_prefix);
    p_bytes("listener_remove_prefix", pr.listener_remove_prefix);
    p_bytes("thread_name_prefix", pr.thread_name_prefix);
    p_hex("listener_add_label", pr.listener_add_label);
    p_hex("listener_remove_label", pr.listener_remove_label);
    p_hex("tostring_label", pr.tostring_label);
    p_bytes("tostring_prefix", pr.tostring_prefix);
    p_hex("tostring_probe_id", pr.tostring_probe_id);
    p_s("tostring_probe_expect", pr.tostring_probe_expect);
    p_u("tostring_probe_expect_units", pr.tostring_probe_expect_units);
    p_u("tostring_out_units", pr.tostring_out_units);
    p_hex("thread_accessor", pr.thread_accessor);
    p_hex("thread_game_name", pr.thread_game_name);
    p_hex("thread_render_name", pr.thread_render_name);
    p_hex("thread_empty_name", pr.thread_empty_name);
    p_hex("pak_vtable", pr.pak_vtable);
    p_hex("pak_platform_mgr_get", pr.pak_platform_mgr_get);
    p_hex("pak_platform_mgr_getpf", pr.pak_platform_mgr_getpf);
    p_hex("pak_mountpak_delegate", pr.pak_mountpak_delegate);
    p_hex("pak_handle_mount_pak", pr.pak_handle_mount_pak);
    p_hex("pak_delegate_vtable", pr.pak_delegate_vtable);
    p_hex("pak_execute", pr.pak_execute);
    p_hex("pak_lower_impl", pr.pak_lower_impl);
    p_hex("pak_snapshot_delegate", pr.pak_snapshot_delegate);
    p_hex("pak_snapshot_vtable", pr.pak_snapshot_vtable);
    p_hex("pak_snapshot_execute", pr.pak_snapshot_execute);
    for (int i = 0; i < 6; ++i) {
        char k[32];
        std::snprintf(k, sizeof k, "pak_proof%d_linked", i); p_hex(k, pr.pak_proof_linked[i]);
        std::snprintf(k, sizeof k, "pak_proof%d_bytes", i); p_bytes(k, pr.pak_proof[i]);
    }
    // layout offsets looked up through the parsed INI text blocks (proves the
    // staged pair is usable; a representative set compared by the checker).
    struct LQ { const char* sec; const char* klass; const char* member; };
    const LQ mq[] = {
        {"m","FUObjectArray","UObjectCreateListeners"}, {"m","FUObjectArray","UObjectDeleteListeners"},
        {"m","FUObjectArray","PrimarySerialNumber"}, {"m","UObjectBase","NamePrivate"},
        {"m","FField","Next"}, {"m","FProperty","Offset_Internal"}, {"m","UFunction","Func"},
        {"m","FUObjectItem","UEP_TotalSize"}, {"m","UClass","ClassDefaultObject"},
    };
    for (auto& q : mq) {
        uint32_t v = 0;
        bool ok = layout_offset(pr.layout_members, q.klass, q.member, v);
        std::printf("layout.members.%s.%s=%s\n", q.klass, q.member, ok ? "0x" : "MISS");
        if (ok) std::printf("layout.members.%s.%s_val=0x%x\n", q.klass, q.member, v);
    }
    const LQ vq[] = {
        {"v","UObject","ProcessEvent"}, {"v","UEngine","LoadMap"}, {"v","UEngine","Tick"},
        {"v","AGameModeBase","InitGameState"}, {"v","FMalloc","Malloc"}, {"v","FMalloc","Free"},
    };
    for (auto& q : vq) {
        uint32_t v = 0;
        bool ok = layout_offset(pr.layout_vtables, q.klass, q.member, v);
        if (ok) std::printf("layout.vtables.%s.%s_val=0x%x\n", q.klass, q.member, v);
        else std::printf("layout.vtables.%s.%s_val=MISS\n", q.klass, q.member);
    }
    return 0;
}
