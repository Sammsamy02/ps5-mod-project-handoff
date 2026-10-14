// RuntimePakMount.cpp - see RuntimePakMount.hpp. Data-only, generic, fail-closed.
#include "RuntimePakMount.hpp"

namespace ModBridge::PakMount
{
// Fail-closed prologue signatures (two-source-verified, evidence/pins.json).
// These are IDENTITY checks of the pinned functions at their rebased live
// addresses, not code we execute or write. Keeping them here (not raw offsets)
// means any image drift refuses before a call.
//   P4 GetPlatformFile(): mov rax,[rdi]; test rax,rax; je; ret
static const uint8_t SIG_GETPF[] = {0x48, 0x8B, 0x07, 0x48, 0x85, 0xC0};
bool EnginePins::valid() const
{
    return pak_vtable && platform_mgr_get && platform_mgr_getpf &&
           slot_get_lower_level && slot_delegate_execute && mountpak_delegate &&
           image_sha256[0] != 0;
}

bool is_pak_layer(uint64_t live_vtable, uint64_t expect_live)
{
    return live_vtable != 0 && live_vtable == expect_live;
}

// Walk the platform-file chain from the top, descending via GetLowerLevel
// (slot P6 = [this+8]), and return the first layer whose vptr == the rebased
// FPakPlatformFile vtable. Bounded; no writes. 0 if not found.
static uint64_t find_pak_layer(const EnginePins& pins, IEngineBridge& eng,
                               uint64_t base)
{
    const uint64_t expect_vt = base + pins.pak_vtable;
    // The integrator resolves the chain head via the verified Get()/GetPlatformFile
    // pins (P3/P4); we only follow GetLowerLevel ([this+8], P6) data-only from here.
    uint64_t layer = eng.platform_chain_head();
    if (!layer)
        return 0;

    for (int depth = 0; depth < 16 && layer; ++depth)
    {
        uint64_t vt = 0;
        if (!eng.read_ptr(layer, vt))
            return 0;
        if (is_pak_layer(vt, expect_vt))
            return layer;
        // descend: GetLowerLevel == mov rax,[this+8]; the slot is data-only read.
        uint64_t lower = 0;
        if (!eng.read_ptr(layer + 8, lower) || lower == layer)
            break;
        layer = lower;
    }
    return 0;
}

MountOutcome mount(const EnginePins& pins, const MountRequest& req, IEngineBridge& eng)
{
    MountOutcome o;
    if (!pins.valid())
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "pins incomplete";
        return o;
    }
    if (req.pak_path_abs.empty())
    {
        o.verdict = Verdict::RefusedBadRequest;
        o.why = "empty path";
        return o;
    }
    // Fail-closed admission: running image must match the pinned image.
    const std::string live_sha = eng.image_sha256();
    if (live_sha.empty() || live_sha != std::string(pins.image_sha256))
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "image sha mismatch";
        eng.log("[PAKMOUNT] refuse: image sha mismatch (fail-closed)");
        return o;
    }
    const uint64_t base = eng.module_view_base();
    if (!base)
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "no module base";
        return o;
    }
    // Mount must happen on the game thread (MountedPaks append is not thread-safe).
    if (!eng.is_game_thread())
    {
        o.verdict = Verdict::RefusedNotGameThread;
        o.why = "not game thread";
        return o;
    }
    // Re-verify the accessor pin we depend on (P4), fail-closed.
    if (!eng.verify_code(base + pins.platform_mgr_getpf, SIG_GETPF, sizeof SIG_GETPF))
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "GetPlatformFile sig mismatch";
        eng.log("[PAKMOUNT] refuse: P4 signature mismatch (fail-closed)");
        return o;
    }

    // Census: find the FPakPlatformFile layer (confirms pak support is live and,
    // for the fallback route, gives us `this`).
    o.pak_layer = find_pak_layer(pins, eng, base);
    if (!o.pak_layer)
    {
        o.verdict = Verdict::RefusedNoPakLayer;
        o.why = "no FPakPlatformFile in chain";
        eng.log("[PAKMOUNT] refuse: pak layer not found (fail-closed)");
        return o;
    }
    eng.mounted_pak_count(o.pak_layer, o.count_before);

    // Preferred route: the engine's own bound MountPak delegate (P1).
    const uint64_t deleg = base + pins.mountpak_delegate;
    uint64_t inst = 0;
    uint32_t size = 0;
    if (!eng.read_ptr(deleg + pins.delegate_inst_off, inst) ||
        !eng.read_u32(deleg + pins.delegate_size_off, size))
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "delegate read failed";
        return o;
    }
    if (size == 0 || inst == 0)
    {
        o.verdict = Verdict::RefusedUnbound;
        o.why = "MountPak delegate unbound";
        eng.log("[PAKMOUNT] refuse: MountPak delegate unbound (engine not ready)");
        return o;
    }
    uint64_t inst_vt = 0;
    if (!eng.read_ptr(inst, inst_vt))
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "delegate vtable read failed";
        return o;
    }
    uint64_t exec_fn = 0;
    if (!eng.read_ptr(inst_vt + pins.slot_delegate_execute, exec_fn) || !exec_fn)
    {
        o.verdict = Verdict::RefusedPins;
        o.why = "Execute slot read failed";
        return o;
    }

    // Marshal the path and invoke the engine's own mount. No file/code write.
    void* fstr = eng.make_fstring(req.pak_path_abs);
    if (!fstr)
    {
        o.verdict = Verdict::RefusedCallFailed;
        o.why = "FString marshal failed";
        return o;
    }
    const uint64_t ret = eng.call_execute(exec_fn, inst, fstr, req.order);
    eng.free_fstring(fstr);

    eng.mounted_pak_count(o.pak_layer, o.count_after);
    if (ret != 0 && o.count_after > o.count_before)
    {
        o.verdict = Verdict::Mounted;
        o.why = "engine mounted (count grew)";
        eng.log("[PAKMOUNT] mounted via engine MountPak delegate (data-only)");
    }
    else
    {
        o.verdict = Verdict::RefusedCallFailed;
        o.why = (ret == 0) ? "engine returned null" : "mounted count did not grow";
        eng.log("[PAKMOUNT] refuse: engine mount did not take (fail-closed, vanilla)");
    }
    return o;
}

} // namespace ModBridge::PakMount
