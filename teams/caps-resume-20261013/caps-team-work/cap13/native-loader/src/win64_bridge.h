// win64_bridge.h -- Win64 <-> SysV x86_64 calling-convention bridge.
//
// Design basis (DESIGN/native-dll-loader.md section 4a):
//   host -> guest (SysV caller -> Win64 callee): register remap only
//     (RDI->RCX, RSI->RDX, RDX->R8, RCX->R9) + 32 B shadow space; the guest
//     preserves a superset of SysV non-volatiles, so nothing extra to save.
//   guest -> host (Win64 caller -> SysV callee, via stamped trampolines):
//     every thunk preserves RSI/RDI/XMM6-15 (SysV callees may clobber them;
//     the Win64 caller expects them preserved), remaps registers into SysV
//     positions, copies stack args down by the shadow-space delta.
//
// PER-SIGNATURE HONESTY: these templates cover ALL-INTEGER signatures with
// up to 8 args (4 register + 4 stack). Mixed int/float signatures assign
// XMM regs differently between the conventions (Win64: XMM0-3 by float
// position; SysV: interleaved by position) and REQUIRE per-signature thunks
// generated from the signature table (M1 generator). 9-16-byte aggregate
// returns differ (Win64 hidden sret vs SysV RAX:RDX) and are refused until
// the descriptor generator exists.
//
// The assembly in win64_bridge.S is hand-written and UNVERIFIED until the
// M0 host unit tests run (macOS is SysV; guest register states are
// simulated with crafted asm fixtures). Do not trust it before that.
#ifndef CAP13_WIN64_BRIDGE_H
#define CAP13_WIN64_BRIDGE_H

#include <cstdint>
#include <cstring>

extern "C" {

// ---- host -> guest (SysV caller invokes these; fn is Win64 code) ----
uint64_t cap13_win64_call0(void* fn);
uint64_t cap13_win64_call1(void* fn, uint64_t a1);
uint64_t cap13_win64_call2(void* fn, uint64_t a1, uint64_t a2);
uint64_t cap13_win64_call3(void* fn, uint64_t a1, uint64_t a2, uint64_t a3);
uint64_t cap13_win64_call4(void* fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4);
uint64_t cap13_win64_call5(void* fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);
uint64_t cap13_win64_call6(void* fn, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6);

// ---- guest -> host shared thunks (r11 = SysV target at entry) ----
// Win64 total args = 4 register (+ n_stack stack args). Selection:
// n_stack = (total_args <= 4) ? 0 : total_args - 4.
void cap13_g2h_thunk4s0();
void cap13_g2h_thunk4s1();
void cap13_g2h_thunk4s2();
void cap13_g2h_thunk4s3();
void cap13_g2h_thunk4s4();

} // extern "C"

namespace cap13 {

// A per-export guest->host trampoline, stamped with BOTH the SysV target
// (into r11) and the shared arity thunk (into rax), then `jmp rax`.
// 22 bytes; lives inside executable memory the adapter already owns
// (J1 dependency; on macOS, unit tests map one PROT_EXEC page).
struct BridgeTramp
{
    uint8_t bytes[22];
};
static_assert(sizeof(BridgeTramp) == 22, "trampoline must stay 22 bytes");

// Layout of the stamped trampoline (patched in the copy):
//   49 BA <imm64>   movabs r11, sysv_target
//   48 B8 <imm64>   movabs rax, shared_thunk
//   FF E0           jmp rax
// r11 survives into the shared thunk (the thunk remaps only RDI/RSI/RDX/RCX
// and uses R10 as its staging scratch; it calls R11).
inline void bridge_stamp_tramp(void* dst_exec, void* sysv_target, const void* shared_thunk)
{
    uint8_t t[22] = {0x49, 0xBA, 0, 0, 0, 0, 0, 0, 0, 0,   // movabs r11
                     0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,   // movabs rax
                     0xFF, 0xE0};                          // jmp rax
    const uint64_t tgt = reinterpret_cast<uint64_t>(sysv_target);
    const uint64_t thk = reinterpret_cast<uint64_t>(shared_thunk);
    std::memcpy(t + 2, &tgt, 8);
    std::memcpy(t + 12, &thk, 8);
    std::memcpy(dst_exec, t, 22);
}

// Select the shared thunk for an all-integer signature.
inline const void* bridge_select_thunk(uint32_t total_args)
{
    const uint32_t n_stack = total_args <= 4 ? 0 : total_args - 4;
    switch (n_stack)
    {
        case 0: return reinterpret_cast<const void*>(&cap13_g2h_thunk4s0);
        case 1: return reinterpret_cast<const void*>(&cap13_g2h_thunk4s1);
        case 2: return reinterpret_cast<const void*>(&cap13_g2h_thunk4s2);
        case 3: return reinterpret_cast<const void*>(&cap13_g2h_thunk4s3);
        case 4: return reinterpret_cast<const void*>(&cap13_g2h_thunk4s4);
        default: return nullptr; // refuse: outside the bridge envelope
    }
}

} // namespace cap13

#endif // CAP13_WIN64_BRIDGE_H
