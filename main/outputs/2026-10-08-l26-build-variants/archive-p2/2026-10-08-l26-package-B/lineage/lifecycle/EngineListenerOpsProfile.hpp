// EngineListenerOpsProfile.hpp — [Seeds][L26] VARIANT C engine delete-listener
// op profile (agent EE integration, 2026-10-08; Claude review 3 approved).
//
// Offline-derived constants for the ENGINE'S OWN
// FUObjectArray::AddUObjectDeleteListener / RemoveUObjectDeleteListener on the
// SAME sha-pinned authenticated image the seed table pins — exactly the
// doctrine of rc::fnameprofile::kPrefix (slot 2, FNameAbiProfile.hpp) and
// rc::codeprofile::kScoPrefix (slot 4, CodeSignatureProfile.hpp):
//
//   image: outputs/2026-10-04-zcode-team/worker-game-version/console-decrypted-eboot.bin
//   sha256 3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc
//          (== SeedTable.h kImageSha256; exec PT_LOAD off 0x4000 / vaddr 0,
//          so image-vaddr label + 0x4000 == file offset)
//   observed runtime base 0x400000 (== SeedTable.h kBaseObserved); the label
//   translation rule is the seed table's: runtime = base + label.
//
// Derivation receipts (disassembly + call-site census): variant-c-proposal/
// engine-listener-binary-receipts.txt (agent AE); labels corroborated by agent
// AA. The prologue bytes below were re-verified by agent EE against the pinned
// image file (label+0x4000: 0x4881A0 / 0x488240) BEFORE embedding:
//
//   Add    @ image-vaddr 0x4841A0 (runtime 0x8841A0)
//     55 48 89 e5 41 57 41 56 53 50 48 8d 5f 78 49 89
//   Remove @ image-vaddr 0x484240 (runtime 0x884240)
//     55 48 89 e5 41 57 41 56 41 54 53 48 8d 5f 78 49
//
// Admission contract (fail closed): UnrealInitializer.cpp's
// SeedArming::consume_required_slot_seeds verifies BOTH 16-byte prologues
// through the authenticated code-cache (code_identity + cached_code_signature,
// the same pair slot 4 uses) and only then publishes the two runtime pointers
// via UObjectArray::SetupEngineDeleteListenerOps. The registration callee
// (UObjectArray.cpp, MODBRIDGE_L25_ENGINE_ADD_LISTENER path) refuses loudly
// and NEVER calls while the admission-published pointers are unset or disagree
// with the label-derived addresses. A different title/build requires
// re-derivation against its image (seed doctrine); never reuse these labels.
//
// This header contains no switch tokens and no code — constants only — so it
// never widens any variant's define scope.
#ifndef RC_ENGINE_LISTENER_OPS_PROFILE_HPP_L26_
#define RC_ENGINE_LISTENER_OPS_PROFILE_HPP_L26_
#include <cstdint>
#include <cstddef>
namespace rc::englistener
{
    // GUObjectArray image-vaddr label (seed slot 0 label; BSS). Used only as
    // the runtime module-base derivation cross-check inside the registration
    // callee: module_base = (uintptr_t)GUObjectArray - kGuObjectArrayLabel.
    inline constexpr uint64_t kGuObjectArrayLabel = 0x93BA110;

    // Engine op image-vaddr labels (runtime = kBaseObserved + label).
    inline constexpr uint64_t kAddDeleteListenerLabel = 0x4841A0;    // runtime 0x8841A0
    inline constexpr uint64_t kRemoveDeleteListenerLabel = 0x484240; // runtime 0x884240

    // 16-byte prologue signatures, byte-verified against the pinned image.
    inline constexpr uint8_t kAddPrefix[] = {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56,
        0x53, 0x50, 0x48, 0x8D, 0x5F, 0x78, 0x49, 0x89,
    };
    inline constexpr uint8_t kRemovePrefix[] = {
        0x55, 0x48, 0x89, 0xE5, 0x41, 0x57, 0x41, 0x56,
        0x41, 0x54, 0x53, 0x48, 0x8D, 0x5F, 0x78, 0x49,
    };
    static_assert(sizeof(kAddPrefix) == 16);
    static_assert(sizeof(kRemovePrefix) == 16);

    // Pin string (informational mirror of SeedTable.h kImageSha256; admission
    // compares nothing here at runtime — the authenticated code-cache already
    // carries the image identity that authenticated the cached signatures).
    inline constexpr const char* kImageSha256 = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc";
}
#endif
