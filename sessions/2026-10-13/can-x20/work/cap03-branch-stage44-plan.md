# Stage 44 bounded short-branch islands

Base: exact stage 43. Keep its existing source and vendor decoder byte-identical
except DetourPlan.hpp/.cpp and focused tests. A copied short conditional or
unconditional branch whose original destination is out of rel8 range from the
trampoline may jump to a nearby 14-byte absolute-jump island appended after
the trampoline's jump-back. Fallthrough must encounter the jump-back first.
Preserve original instruction lengths and internal branch targets. If the
island itself is not in rel8 range, or bounded capacity is exhausted, refuse.
Never branch through an island for a RIP-relative memory instruction.

Verify with a red test for external JNE, host execution of both taken and
untaken paths, a target compile/link, stage 43's host protocol, exact-base
replay, and package. Console use remains outside scope.
