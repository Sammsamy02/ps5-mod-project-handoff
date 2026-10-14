# L21 allocator — source corroboration update

**The source-order gap is now resolved conditionally: the exact UE5.6.1 Shipping default branch assigns all 17 runtime methods and agrees with the saved image.** All 17 targets are executable; six method identities have independent byte-behavior anchors. No contradiction was found for the default branch. This remains a reviewed-candidate report; no profile was promoted and no runtime or PRX was built.

This supersedes the unavailable-source conclusion in the original `REPORT.md` and `measured/candidate-table.json`; those original files are preserved unchanged. Six narrow official source files were subsequently fetched read-only through the existing licensed GitHub authentication and verified by SHA256 and Git blob SHA1 at Epic commit `6978b63c8951e57d97048d8424a0bebd637dde1d`. [Exact pins](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/source-pins.json) and [line-addressed excerpts](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/source-excerpts.json) retain the provenance.

## Source branches and destructor accounting

[Exec.h](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/Exec.h:31) explicitly declares virtual `~FExec()`. Its complete/deleting pair occupies slots 0/1 under the Itanium rule, now checked against the actual declaration rather than assumed. `FUseSystemMallocForNew` has no virtual members; FExec is FMalloc's primary dynamic base. FMalloc's implicit destructor overrides the inherited pair. The FMalloc `Exec` override reuses its inherited slot and does not append one.

Under `UE_ALLOW_EXEC_COMMANDS=1`, the inherited order is: destructor pair, Exec, Exec_Runtime, Exec_Dev, Exec_Editor — **six slots**, followed by Malloc at `0x30`. Under `=0`, FExec declares a second final Exec overload — **seven slots**, followed by Malloc at `0x38`. Exec_Dev/Exec_Editor declarations remain present even when their enabling macros are false. Shipping removes static `MaxSingleAlloc`, not a virtual slot; STATS and UPDATE_MALLOC_STATS do not remove the stats virtual declarations.

[TargetRules.cs:1591](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-additional/TargetRules.cs:1591) sets `bUseExecCommandsInShipping` to **true by default**. [UEBuildTarget.cs:5818](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-additional/UEBuildTarget.cs:5818) emits `UE_ALLOW_EXEC_COMMANDS_IN_SHIPPING=1/0` from that rule. [Exec.h:11](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/official-headers/Exec.h:11) uses it for Shipping/non-editor when `UE_ALLOW_EXEC_COMMANDS` has not already been defined. Build.h supplies no separate default for this flag. Therefore the enabled branch is the independently documented source default, and the image agrees with it. Actual Townfall target-rule/compiler overrides were not captured; this is **default-source plus image correspondence**, not an independently recovered compiler command line. Both branches and all macro assumptions are saved in [macro-assumptions.json](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/macro-assumptions.json).

## All linked runtime methods

Offsets are bytes from the allocator vtable address point. Linked targets refer to the pinned Townfall image; add historical base `0x400000` for expected runtime addresses. Every listed target is in executable PT_LOAD 0. “Source order” rows have exact declaration/layout evidence and an executable image entry; shared empty/constant-return bodies cannot independently distinguish their method names.

| Method | Exec enabled — default/image | Exec disabled — alternative | Linked image target at default offset | Semantic confidence |
|---|---:|---:|---:|---|
| Malloc | `0x30` | `0x38` | `0xb48e0` | semantic anchor |
| TryMalloc | `0x38` | `0x40` | `0xb8a20` | source order + executable entry |
| Realloc | `0x40` | `0x48` | `0xb49b0` | semantic anchor |
| TryRealloc | `0x48` | `0x50` | `0xb8a30` | source order + executable entry |
| Free | `0x50` | `0x58` | `0xb4bf0` | semantic anchor |
| QuantizeSize | `0x68` | `0x70` | `0xb4cd0` | semantic anchor |
| GetAllocationSize | `0x70` | `0x78` | `0xb4db0` | semantic anchor |
| Trim | `0x78` | `0x80` | `0xb2820` | source order + executable entry |
| SetupTLSCachesOnCurrentThread | `0x80` | `0x88` | `0xb4df0` | source order + executable entry |
| ClearAndDisableTLSCachesOnCurrentThread | `0x98` | `0xa0` | `0xb4ee0` | source order + executable entry |
| InitializeStatsMetadata | `0xa0` | `0xa8` | `0xb290` | source order + executable entry |
| UpdateStats | `0xa8` | `0xb0` | `0xb290` | source order + executable entry |
| GetAllocatorStats | `0xb0` | `0xb8` | `0xb290` | source order + executable entry |
| DumpAllocatorStats | `0xb8` | `0xc0` | `0xb2c00` | source order + executable entry |
| IsInternallyThreadSafe | `0xc0` | `0xc8` | `0x120` | source order + executable entry |
| ValidateHeap | `0xc8` | `0xd0` | `0x120` | source order + executable entry |
| GetDescriptiveName | `0xd0` | `0xd8` | `0xb2810` | semantic anchor |

The enabled branch matches **all six preexisting independent semantic offsets**. The disabled branch shifts those offsets and does not match them; it was retained as an explicit alternative and was not applied. The image census still supplies ten direct GMalloc virtual dispatches at `+0x30/+0x40/+0x50`; QuantizeSize/GetAllocationSize/GetDescriptiveName have separate complete-body/string anchors. Full 32-slot and 33-slot source sequences, individual header lines, executable-target checks, and per-method confidence are in [updated candidate table](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/candidate-table.json).

## Verification and scope

Two synthetic compiler probes include the **unmodified official MemoryBase.h and Exec.h**, stub unrelated dependencies/types, and emit LLVM IR plus vtable layouts for both macro branches. They confirm FMalloc/FExec address point 0, inherited destructor pairs, reused Exec override, and every derived offset. This is an x86_64 Clang Itanium-family layout check, not a Prospero compiler build or game execution. [Probe commands and limitations](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/compiler-probes.json); [enabled dump](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/exec-1.layout.txt); [disabled dump](/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l21-audit/allocator/postfetch/exec-0.layout.txt).

Reproduce with `python3 outputs/2026-10-08-l21-audit/allocator/corroborate_declarations.py --out postfetch-new`. Both runs passed. The earlier image-measurement script, hash-pinned image, disassembly, and saved live-observation receipt remain unchanged. Image identity stays SHA256 `3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc`, build-id `04bb4330a7d71dcefe1cc2c4d19769d5`, selected revision `5.6.1-0+++UE5` (reconstructed). No unconditional other-game allocator ABI is claimed.

The old saved V1/V2 checks establish instance/vptr equality, not live slot contents or successful allocator calls. This source update does not change that distinction. Promotion remains held for parent review and the full runtime inventory/API gates; no additional console action is proposed.
