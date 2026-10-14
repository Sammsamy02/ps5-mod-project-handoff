# Stage 43 offline detour planner

Purpose: move #3 toward automatic, game-independent x64 hooks by producing
checked target-patch and trampoline bytes without writing target memory.

Inputs: a bounded readable snapshot of the original target instructions,
the target address, a caller-owned trampoline address, and handler address.
Output: whole-instruction overwrite length (14–64 bytes), a register-free
14-byte absolute jump padded with NOPs, and a relocated stolen prologue
followed by a jump back. The caller owns executable allocation, thread
quiescence, and use of the existing compare-before-write patch/restore API.

Decode with pinned Zydis v4.1.1 (`a2278f1`) and Zycore
`0b2432c`, amalgamated by the official generator. Each instruction is
copied byte-exact except PC-relative disp/immediate fields, which are
recalculated for the trampoline address. Internal branch destinations are
remapped to matching relocated instruction starts. Refuse undecodable,
truncated, interior-branch, wrapping, overlapping, or out-of-range layouts.
No instruction widening or arbitrary near-memory allocation is claimed.

Verification: TDD host cases for ordinary, RIP-relative, external and
internal branch prologues plus refusal cases. Compile and link both the
decoder and planner for the pinned PS5 SDK, rerun #3's 112 host protocol
checks, and package an exact-stage-30 replay patch, hashes, and logs.
