# EventViewer global backend: concrete admission boundary

**All three retained global hook families ultimately request an x64 code detour. None selects a genuine shared data-slot backend on the target platform.** For discovered live game code, the retained owned-memory backend refuses the first prologue read unless it is genuinely admitted owned storage; its public contract expressly excludes registering discovered game mappings. API symbols/configuration therefore do not establish compatible shipped behavior. This is source evidence, not an observed console failure or final-link attestation. No patch staged.

`port/` abbreviates `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/`; candidate means `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/`. Exact file hashes/stat stability are in SOURCE-RECEIPT.json. No candidate override exists at the three inspected Hooks.cpp/DetourInstance.hpp/Metadata.hpp locations; retained shared sources were inspected. This does not certify what any final binary linked.

| EventViewer family | Actual retained route | Gate and failure category |
| --- | --- | --- |
| ProcessEvent pre/post | port/unreal/src/Hooks/Hooks.cpp:97–105 -> Metadata.hpp:149–155 -> DetourTraits.hpp:104–113 -> DetourInstance | Metadata uses ALWAYS_TRUE, **not** config.bHookUObjectProcessEvent. That setting controls discovery in candidate/UnrealInitializer.cpp:3026–3033. Disabled discovery can leave unavailable target; it is not an installation-policy switch. Profiler build selects FProcessEventDetour, but that subclass only changes Invoke (DetourSubclasses.hpp:515 onward), not install/activation. |
| ProcessInternal pre/post | Hooks.cpp:126–134 -> Metadata.hpp:173–179 -> DetourTraits.hpp:116–121 -> same DetourInstance | Config directly gates installation; candidate/UE4SSProgram.cpp:1467 copies setting. Discovery shares the gated derivation block at candidate/UnrealInitializer.cpp:3088–3103. |
| ProcessLocalScriptFunction pre/post | Hooks.cpp:155–163 -> Metadata.hpp:181–187 -> DetourTraits.hpp:124–129 -> same DetourInstance | Config directly gates installation; candidate/UE4SSProgram.cpp:1468 copies setting. Discovery at candidate/UnrealInitializer.cpp:3106–3140 derives another function address by decoding ProcessInternal. No target-platform alternative in these traits. |

The shared path is exact: DetourInstance.hpp:282–287 calls EnsureHooked **before callback insertion**; :232–238 calls InstallHook then ActivateHook; :151–168 rejects disabled/unready targets or constructs PLH::x64Detour from the target function address to StaticHookFn with trampoline storage; :175–182 calls PLHDetour->hook(). Native platform branches in this class govern lifetime/removal, not a replacement data-slot install backend. The target pointer is the *code address to patch*, not a writable shared callback slot.

The retained backend path is also concrete:

- `.superpowers/native-dependency-spike/ue4ss-native-owned-default-detour-1/port/x64Detour.cpp:348–358` disassembles the first100bytes and returns false on empty instructions.
- `.superpowers/native-dependency-spike/ue4ss-native-owned-detour-1/port/sources/ZydisDisassembler.cpp:60–65` registers only its local destination buffer, requires safe_mem_read of the complete source extent, and returns empty instructions on refusal.
- `.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/src/OwnedMemory.cpp:76–82,246–247` safe_mem_read delegates to copy, which requires both registered covering extents and R/W permissions before dereference. Absent source ownership returns false, read=0.
- `.superpowers/native-dependency-spike/ue4ss-native-bounded-owned-1/include/polyhook2/OwnedMemory.hpp:22–28` explicitly says ScopedRegion is not an API for registering discovered process/game mappings. Merely learning a target address does not satisfy ownership. Even a separately supplied readable copy would not authorize code mutation: x64Detour.cpp:484–495 requires destination protection/copy admission too; OwnedMemory.cpp:84–87 restricts protection to page-owned entries.

Thus, if flags/target discovery already reject a family, installation stops there; if an ordinary discovered live target reaches this retained backend, ownership rejects prologue access before patching. We did not inspect effective packaged INI or loaded target state and do not label a particular deployed family disabled/unready. Existing compiled owned-buffer tests, if any, would prove only that distinct owned-buffer domain. No supported game-code authority follows from them.

## Smallest honest next action

Record the three separate generic admission results at the genuine InstallHook/ActivateHook boundary: configuration gate, target readiness, then backend source/protection admission. ProcessEvent needs explicit reporting of its ALWAYS_TRUE install gate versus discovery setting. This can prevent misleading API-present/registration-success claims; it cannot enable EventViewer.

Actual unchanged EventViewer support requires a genuine authorized global-entry backend feeding the existing three typed DetourInstances, with original arguments/trampoline semantics and retained retirement rules. No safe generic data-only shared slot was found in these selected routes. The missing enabling seam is live-entry read/patch/protection plus whole-entry admission/drain authority, not another mod wrapper or invented slot/address. Do not register game text as owned to bypass the existing contract. The runtime owner must choose and establish that backend authority before a runtime implementation can be concrete.

One additional source-bound caveat: EventViewer Middleware.cpp:203–209 rolls back its controllers on false/ERROR_ID returns, but has no exception catch around chained registration. Middleware.hpp:132/161 directly invokes registration; DetourInstance.hpp:308–387 can allocate callback/vector state after EnsureHooked. Therefore rollback is not established for a thrown later registration after an earlier success. This is a potential partial-registration ownership case, not an observed failure; no broad allocation-hardening work proposed.

Native GUI remains an independent missing prerequisite as recorded in cap11/NATIVE-GUI-SEAM.md. This audit adds no gate to current B7/hooks/15-field work, and no compiled, native-GUI, or unchanged-mod support claim.
