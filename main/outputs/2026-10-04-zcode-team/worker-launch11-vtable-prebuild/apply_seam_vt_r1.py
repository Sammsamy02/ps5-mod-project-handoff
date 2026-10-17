#!/usr/bin/env python3
"""Apply the L11PB VT-R1 seam to the staging copies (worker-launch11-vtable-prebuild).

Exact-anchored replacements only; a missing or ambiguous anchor aborts nonzero so a
drifted staging copy can never be silently seamed (worker-no-exec-mode-gate/
apply_gate.py discipline). Also usable as the anchor VERIFIER for the R2 build-ready
diffs with --verify-r2 (read-only checks against the current port sources).

Disarmed invariant: with UE4SS_DISPATCH_DATA_BACKEND undefined (=0) the seamed
DetourInstance.hpp preprocesses to the SAME token stream as the port header —
verified separately by prove_disarm_token_identity.py.
"""
import sys
from pathlib import Path

MY = Path(__file__).resolve().parent
STG = MY / "staging/initializer-vtable-pre"

def replace_once(text, old, new, tag):
    n = text.count(old)
    if n != 1:
        sys.exit(f"ANCHOR FAILURE [{tag}]: {n} matches (need exactly 1)")
    print(f"applied [{tag}]")
    return text.replace(old, new, 1)

# ---------------------------------------------------------------- R1 seam: DetourInstance.hpp
p = STG / "DetourInstance.hpp"
text = p.read_text(encoding="utf-8")

text = replace_once(
    text,
    "#include <Unreal/Hooks/Internal/AtomicSharedPtrFallback.hpp>\n",
    """#include <Unreal/Hooks/Internal/AtomicSharedPtrFallback.hpp>

// =====================================================================================
// [L11PB VT-R1 seam] Dispatch-data backend selection (worker-launch11-vtable-prebuild,
// 2026-10-04; VTABLE-SWAP-DESIGN section 5 R1, LAUNCH10-MATRIX U-SWAP-2). PREPARED-
// UNARMED: the staged default 0 keeps THIS header's preprocessed tokens IDENTICAL to
// the port header (measured by prove_disarm_token_identity.py) - PLH::x64Detour stays
// the backend and every TU sharing this header's vague-linkage instantiations stays
// ODR-identical. Arming (=1) selects DispatchDataDetour at the single construction
// site below; the controller owns the flip post-verdict (W_FAILS_R_WORKS rows R3/R4).
//
// MEASURED COMPOSITION MECHANICS CONSTRAINT: armed mode changes the TDetourInstance
// member type (PLHDetour), so EVERY TU including this header must see the SAME armed
// state (unreal-unrealinitializer AND unreal-hooks are the only src consumers,
// measured). The frozen comp6 fixed script cannot recompile unreal-hooks, so arming is
// an INTEGRATION-TIME composition decision (new fixed-script edition or equivalent),
// never a one-TU flip. See build-ready/INTEGRATION-NOTE.md.
// =====================================================================================
#ifndef UE4SS_DISPATCH_DATA_BACKEND
#define UE4SS_DISPATCH_DATA_BACKEND 0
#endif
#if UE4SS_DISPATCH_DATA_BACKEND
#include "DispatchDataDetour.hpp"
#endif
// Backend-type seam macro: expands to the EXACT original tokens when disarmed
// (PLH::x64Detour), so disarmed token identity is preserved at every construction
// site (measured by prove_disarm_token_identity.py; there are FIVE construction
// sites in the consumed tree - see build-ready/INTEGRATION-NOTE.md).
#if UE4SS_DISPATCH_DATA_BACKEND
#define L11PB_VT_BACKEND_TYPE DispatchDataDetour
#else
#define L11PB_VT_BACKEND_TYPE PLH::x64Detour
#endif
""",
    "VT-R1 S1 seam-block")

text = replace_once(
    text,
    """            PLHDetour = std::make_unique<PLH::x64Detour>(
                std::bit_cast<uint64_t>(TargetFunction->get_function_address()),
                std::bit_cast<uint64_t>(&StaticHookFn), &Trampoline);""",
    """#if UE4SS_DISPATCH_DATA_BACKEND
            // [L11PB VT-R1] swap backend: fnAddress = slot-registry key (the resolved
            // original entry), fnCallback = StaticHookFn, saved original written into
            // Trampoline (VTABLE-SWAP-DESIGN section 5 R1 / section 4.3). Returns false
            // on any gate refusal -> the existing loud-throw path fires unchanged.
            PLHDetour = std::make_unique<DispatchDataDetour>(
                std::bit_cast<uint64_t>(TargetFunction->get_function_address()),
                std::bit_cast<uint64_t>(&StaticHookFn), &Trampoline);
#else
            PLHDetour = std::make_unique<PLH::x64Detour>(
                std::bit_cast<uint64_t>(TargetFunction->get_function_address()),
                std::bit_cast<uint64_t>(&StaticHookFn), &Trampoline);
#endif""",
    "VT-R1 S2 construction-site")

text = replace_once(
    text,
    "        std::unique_ptr<PLH::x64Detour> PLHDetour{};",
    """#if UE4SS_DISPATCH_DATA_BACKEND
        std::unique_ptr<DispatchDataDetour> PLHDetour{};
#else
        std::unique_ptr<PLH::x64Detour> PLHDetour{};
#endif""",
    "VT-R1 S3 member-type")

p.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------- R1 seam: DetourSubclasses.hpp
p = STG / "DetourSubclasses.hpp"
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    "#include <Unreal/Hooks/Internal/DetourInstance.hpp>",
    "#include \"DetourInstance.hpp\" // [L11PB VT-R1] staged sibling: quoted include resolves to THIS staging dir's seamed copy (pragma-once identity differs from the port file; the TU must never see both)",
    "VT-R1 S4 subclasses-redirect")

# MEASURED DESIGN CORRECTION: VTABLE-SWAP-DESIGN section 5 R1 says "the single
# construction site (DetourInstance.hpp:164-166)", but the consumed tree has FOUR
# make_unique<PLH::x64Detour> sites in DetourSubclasses.hpp alone (FCallFunctionByName-
# WithArguments ~:119, StaticConstructObject ~:265, StaticConstructObject411 v411/:411
# arms ~:351/:365 — the armed compile failed at each). All four select the backend via
# the type-seam macro (disarmed expansion = exact original tokens).
n = text.count("std::make_unique<PLH::x64Detour>(")
if n != 4:
    sys.exit(f"ANCHOR FAILURE [VT-R1 S7 subclasses-construction-sites]: {n} matches (need exactly 4)")
text = text.replace("std::make_unique<PLH::x64Detour>(",
                    "std::make_unique<L11PB_VT_BACKEND_TYPE>(")
print("applied [VT-R1 S7 subclasses-construction-sites] (4 sites via type-seam macro)")

# S8: ProcessConsoleExec's WINE detour scheme (PLH::x64Detour::INPLACE_SHORT, a PLH
# patching detail with no meaning for a data swap; target not-on-path, VTABLE-SWAP-
# DESIGN section 2 row 4). Armed mode: no scheme to set.
text = replace_once(
    text,
    "                PLHDetour->setDetourScheme(PLH::x64Detour::INPLACE_SHORT);",
    """#if UE4SS_DISPATCH_DATA_BACKEND
                // [L11PB VT-R1] no detour-scheme concept in the swap backend (the WINE
                // INPLACE_SHORT scheme is a PLH prologue-patching detail; this target is
                // not-on-path, section 2 row 4).
#else
                PLHDetour->setDetourScheme(PLH::x64Detour::INPLACE_SHORT);
#endif""",
    "VT-R1 S8 detour-scheme")
p.write_text(text, encoding="utf-8")

# ---------------------------------------------------------------- R1 seam: UnrealInitializer.cpp
p = STG / "UnrealInitializer.cpp"
text = p.read_text(encoding="utf-8")
text = replace_once(
    text,
    "#include <Unreal/Hooks/Internal/DetourInstance.hpp>",
    "#include \"DetourInstance.hpp\" // [L11PB VT-R1] staged sibling (seamed, disarmed default = token-identical to the port header)",
    "VT-R1 S5 initializer-include-detourinstance")
text = replace_once(
    text,
    "#include <Unreal/Hooks/Internal/DetourSubclasses.hpp>",
    "#include \"DetourSubclasses.hpp\" // [L11PB VT-R1] staged sibling (redirects DetourInstance to the seamed staged copy)",
    "VT-R1 S6 initializer-include-detoursubclasses")
p.write_text(text, encoding="utf-8")

print("VT-R1 seam applied: staging copies are PREPARED-UNARMED (default 0 = PLH)")
