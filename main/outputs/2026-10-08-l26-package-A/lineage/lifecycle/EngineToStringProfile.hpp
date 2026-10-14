// EngineToStringProfile.hpp — [Seeds][L27] engine FName::ToStringInternal
// profile (agent-EI, 2026-10-08; Claude review approved with the added
// runtime dual-check guard).
//
// [Seeds][L27-supersede] (agent-ER, 2026-10-09; Claude directive 2): the
// BIND this header gated is DELETED — FName::ToString is now the PURE
// IN-PORT pool decoder (FNamePoolDecoder, candidate/src/NameTypes.cpp)
// which needs no engine address and no admission. The header is RETAINED:
// kToStringInternalLabel/kToStringInternalPrefix still document the
// byte-level provenance below, and kProbeId/kProbeExpect/kProbeExpectUnits
// are LIVE inputs — the decoder's first-use dual-check decodes
// kProbeId and requires exactly kProbeExpect
// ('[FN] pool-decoder verified (ByteProperty)').
//
// Offline-derived constants for the ENGINE'S OWN
// FName::ToStringInternal(TCHAR* Out, uint32 OutSize) on the SAME
// sha-pinned authenticated image the seed table pins — exactly the doctrine
// of rc::fnameprofile::kPrefix (slot 2, FNameAbiProfile.hpp),
// rc::codeprofile::kScoPrefix (slot 4, CodeSignatureProfile.hpp) and
// rc::englistener::kAddPrefix/kRemovePrefix (EngineListenerOpsProfile.hpp):
//
//   image: outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin
//   sha256 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc
//          (== SeedTable.h kImageSha256; exec PT_LOAD off 0x4000 / vaddr 0,
//          so image-vaddr label + 0x4000 == file offset)
//   observed runtime base 0x400000 (== SeedTable.h kBaseObserved); the label
//   translation rule is the seed table's: runtime = base + label.
//
// ADDRESS NOTE (agent-EI, byte-verified before embedding): agent-CJ's table
// (agent-CJ-engine-fname.md §2b) says ToStringInternal @ IMAGE 0x23FA00,
// RUNTIME 0x63FA00 — NOT "file-image 0x63FA00 / runtime 0xA3FA00". The
// 32 bytes below were re-extracted from the pinned eboot at file offset
// 0x23FA00 + 0x4000 = 0x243A00 and capstone-decoded end-to-end:
//   push rbp; mov rbp,rsp; push r15/r14/r13/r12/rbx; push rax;
//   cmp byte ptr [rip+0x912648b],0        ; lazy FNamePool init guard
//                                        ; (pool-0x20 flag @ image 0x9365EA0)
//   mov r15d,[rdi]                        ; RDI = this (8-byte FName)
//   mov r14,rsi                           ; RSI = Out TCHAR buffer
//   lea r13d,[rdx-1]                      ; EDX = OutSize; r13d = OutSize-1
//   mov rbx,rdi ...
//   lea rcx,[rip+0x912647b]               ; -> FNamePool global image
//                                        ;   0x9365EC0 (49-xref anchor)
//   mov rax,[rcx+rax*8+0x10]              ; Blocks[id>>16] @ pool+0x10
//   movzx edx,word ptr [rax+rcx*2]        ; entry header (id&0xFFFF)*2
//   shr r12d,6                            ; len = header>>6
//   cmp r12d,r13d; cmovb r15d,r12d        ; truncate to OutSize-1
//   test dl,1; jne <wide-memmove>         ; WIDE bit; else ANSI copy +
//                                        ;   in-place widen dance
//   ... tail @0x23FBD4: mov ecx,[rbx+4]   ; Number = this->Number
//   test ecx,ecx; sete al                 ; Number==0 -> plain NUL route
//   ... 0x23FBEE: dec ecx                 ; internal -> external number
//   lea rdx,[rip+0x76395ff]               ; "%i"-family format
//   call 0x97630                          ; snprintf-style '_N' append
//   ... 0x23FC0F: mov word ptr [r14+r15*2],0 ; NUL terminator
//   0x23FC16: mov eax,r15d                ; RETURN: TCHAR count (no sret)
//
// Calling ABI (byte-verified; contrast slot 2's sret ctor):
//   RDI = const FName* this ({ComparisonIndex u32 @0, Number u32 @4}),
//   RSI = TCHAR* Out, EDX = uint32 OutSize -> EAX = TCHAR count written
//   (excludes the NUL the function itself writes; truncates at OutSize-1;
//   appends the external '_N' suffix when Number != 0 and room remains).
//   Zero allocator involvement: writes only into the caller buffer — the
//   CJ-recommended form (§3 "strictly safer than 0x23F150").
//
// Admission contract (fail closed, NON-TERMINAL): UnrealInitializer.cpp's
// SeedArming::consume_required_slot_seeds Phase 4c verifies this 32-byte
// prologue through the authenticated code-cache
// (rc::fnameabi::signature — the same gate slot 2 uses) and then runs the
// behavioral probe + runtime dual-check:
//   ToStringInternal(FName{ByteProperty pool id 0x3, number 0})
//     must equal "ByteProperty" AND
//     must equal the port's own pool decoder (rc::fnameabi::roundtrip over
//     the admitted pool words — the init-probe decode that verified
//     ByteProperty at slot-2 admission).
// ANY mismatch/empty output disables the bind loudly
// ('[FN] tostring-bind disabled (mismatch/empty)') and the legacy Kismet
// Conv_NameToString route keeps FName::ToString. Nothing is published on
// refusal, so nothing can ever call through an unverified address. This
// bind is OPTIONAL (the Kismet fallback exists), so unlike slots 0/2/3/4
// its refusal does not retire the seed table. A different title/build
// requires re-derivation against its image (seed doctrine); never reuse
// these constants.
//
// This header contains no switch tokens and no code — constants only — so
// it never widens any variant's define scope.
#ifndef RC_ENGINE_TO_STRING_PROFILE_HPP_L27_
#define RC_ENGINE_TO_STRING_PROFILE_HPP_L27_
#include <cstdint>
#include <cstddef>
namespace rc::engtostring
{
    // FName::ToStringInternal image-vaddr label (runtime = kBaseObserved +
    // label = 0x63FA00 on the pinned image). NOT 0xA3FA00: the +0x400000
    // slide applies to the IMAGE vaddr 0x23FA00 (CJ §2b, re-verified).
    inline constexpr uint64_t kToStringInternalLabel = 0x23FA00; // runtime 0x63FA00

    // 32-byte prologue signature (slot-2 kPrefix size discipline),
    // byte-extracted from the pinned image at file offset 0x243A00.
    inline constexpr uint8_t kToStringInternalPrefix[] = {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56,
        0x41, 0x55, 0x41, 0x54, 0x53, 0x50, 0x80, 0x3D,
        0x8B, 0x64, 0x12, 0x09, 0x00, 0x44, 0x8B, 0x3F,
        0x49, 0x89, 0xF6, 0x44, 0x8D, 0x6A, 0xFF, 0x48,
    };
    static_assert(sizeof(kToStringInternalPrefix) == 32);

    // Behavioral probe (slot-2 mirror): ByteProperty's FName pool id.
    // EName table @ pool+0x14040 (None@+0, ByteProperty@+4); block 0,
    // byte 6 -> off16 3 -> id 0x3 (agent-CJ §2a + agent-EB DECISION-MEMO).
    // The live pool word must still equal this pin (title-drift guard).
    inline constexpr uint32_t kProbeId = 0x3;
    inline constexpr char16_t kProbeExpect[] = u"ByteProperty";
    inline constexpr uint32_t kProbeExpectUnits = 12;
    static_assert(sizeof(kProbeExpect) / sizeof(char16_t) == kProbeExpectUnits + 1);

    // Caller buffer size in TCHAR units: engine NAME_SIZE+11 (CJ §2b) —
    // the engine's own ToStringTruncate guarantee: no legal name (<=
    // NAME_SIZE TCHARs) plus the '_NNN' suffix can truncate in 1035 units.
    inline constexpr uint32_t kOutUnits = 1024 + 11;
    static_assert(kOutUnits == 1035);

    // Pin string (informational mirror of SeedTable.h kImageSha256; the
    // authenticated code-cache already carries the image identity that
    // authenticated the cached signatures).
    inline constexpr const char* kImageSha256 = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc";
}
#endif
