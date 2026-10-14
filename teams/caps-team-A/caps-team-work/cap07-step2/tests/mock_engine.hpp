// mock_engine.hpp - in-memory fake of the engine surface the mount unit uses.
// Lets the pure mount logic be exercised off-console: a fake platform-file chain
// (physical -> pak), a fake FCoreDelegates::MountPak delegate object + instance +
// iface vtable, and a scripted engine mount that grows a mounted-pak counter.
#pragma once
#include "../PATCH/RuntimePakMount.hpp"
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace mock
{
using namespace ModBridge::PakMount;

// A tiny address space: we place objects at synthetic LIVE addresses and model
// pointer/dword reads over a map. base = 0x400000 (matches the live-base pin).
struct FakeImage
{
    static constexpr uint64_t BASE = 0x400000;
    std::map<uint64_t, uint64_t> mem64;     // live addr -> qword
    std::map<uint64_t, uint32_t> mem32;     // live addr -> dword
    std::string sha = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc";

    // image-relative pins -> live by +BASE
    uint64_t pak_vtable_live = BASE + 0x7fee8c0;
    uint64_t getpf_live = BASE + 0xb8a70;

    // synthetic object addresses (arbitrary, distinct)
    uint64_t physical_layer = 0x10000000;
    uint64_t pak_layer = 0x10000100;
    uint64_t physical_vtable = 0x20000000;   // NOT the pak vtable
    uint64_t delegate_obj = BASE + 0x934faa8;
    uint64_t delegate_inst = 0x30000000;
    uint64_t delegate_vtable = 0x40000000;
    uint64_t execute_fn = 0x50000000;        // live Execute thunk address

    void wire(bool bind_delegate = true, bool pak_present = true)
    {
        // chain: physical (top) -> pak (lower). GetLowerLevel reads [layer+8].
        mem64[physical_layer] = physical_vtable;
        mem64[physical_layer + 8] = pak_present ? pak_layer : 0;
        mem64[pak_layer] = pak_vtable_live;     // vptr identifies FPakPlatformFile
        mem64[pak_layer + 8] = 0;               // bottom of chain
        // delegate object {inst@0, size@8}
        mem64[delegate_obj + 0] = bind_delegate ? delegate_inst : 0;
        mem32[delegate_obj + 8] = bind_delegate ? 1u : 0u;
        // instance -> iface vtable; Execute slot +0x60 -> execute_fn
        mem64[delegate_inst] = delegate_vtable;
        mem64[delegate_vtable + 0x60] = execute_fn;
    }
};

class MockBridge : public IEngineBridge
{
public:
    FakeImage& img;
    bool game_thread = true;
    bool sig_ok = true;             // flip to simulate a fail-closed signature
    uint32_t mounted = 2;           // starting mounted-pak count
    bool engine_accepts = true;     // whether call_execute "mounts"
    std::string last_path;
    int fstrings_live = 0;
    int execute_calls = 0;

    explicit MockBridge(FakeImage& i) : img(i) {}

    uint64_t module_view_base() override { return FakeImage::BASE; }
    uint64_t platform_chain_head() override { return img.physical_layer; }
    std::string image_sha256() override { return img.sha; }

    bool read_ptr(uint64_t a, uint64_t& o) override
    {
        auto it = img.mem64.find(a);
        if (it == img.mem64.end()) return false;
        o = it->second; return true;
    }
    bool read_u32(uint64_t a, uint32_t& o) override
    {
        auto it = img.mem32.find(a);
        if (it == img.mem32.end()) return false;
        o = it->second; return true;
    }
    bool verify_code(uint64_t, const uint8_t*, size_t) override { return sig_ok; }
    bool is_game_thread() override { return game_thread; }

    void* make_fstring(const std::string& p) override
    {
        last_path = p; ++fstrings_live;
        return new std::string(p);
    }
    void free_fstring(void* f) override
    {
        --fstrings_live; delete static_cast<std::string*>(f);
    }
    uint64_t call_execute(uint64_t exec_fn, uint64_t inst, void* fstr, int32_t) override
    {
        ++execute_calls;
        // only the resolved pins should reach here
        if (exec_fn != img.execute_fn || inst != img.delegate_inst || !fstr)
            return 0;
        if (!engine_accepts) return 0;
        ++mounted;                 // engine "mounted" a pak
        return 0xBEEF;             // non-null IPakFile*
    }
    bool mounted_pak_count(uint64_t, uint32_t& o) override { o = mounted; return true; }
    void log(const char* l) override { (void)l; }
};

} // namespace mock
