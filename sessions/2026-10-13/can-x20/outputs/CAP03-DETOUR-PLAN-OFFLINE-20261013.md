# Capability #3: isolated x64 detour planning, offline

## Result

Stage 43 at `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-plan-20261013-43` adds a **read-only** detour planner to the exact stage-30 #3 source. It decodes a bounded original x64 prologue, chooses whole instructions covering a 14-byte absolute jump, and produces both handler-patch bytes and a relocated trampoline ending in a jump back. It does not allocate executable memory, stop threads, write a target, or run on the console. Existing stage-30 files are byte-identical.

The decoder is the official Zydis v4.1.1 generated amalgamation, pinned to source commit `a2278f1d254e492f6a6b39f6cb5d1f5d515659dc` and Zycore `0b2432ced0884fd152b471d97ecf0258ff4d859f`, with the MIT license included. The [increment patch](cap03-detour-plan-increment-20261013.patch) contains all seven added source/test/vendor files. Its SHA-256 is `b7e61f9c2f3994b884813115817e7fd83e7209952bb680bd53f666d7907f1625`.

## Verified behavior

- **19/19 focused Mac checks** pass. They include an actual call through generated trampoline code in private executable host memory: the original and relocated function returned 42, and the patched target returned the handler's 99. The cases also cover RIP-relative loads and `LEA`, internal and external branches, internal `CALL` refusal, the first byte after the stolen span, EIP-relative and self-modifying-write refusals, overlap, overflow, and truncated input.
- The unchanged #3 host patch/restore protocol still passes **112/112** checks. Its CHECK immediate and `syscall` are present in both host and PS5 target objects.
- The pinned PS5 SDK compiled the Zydis decoder, planner, CodePatch, and diagnostic objects, then linked an offline `x86_64-sie-ps5` ELF. The planner symbol is present. The ELF was **not** uploaded or run. The ELF SHA-256 is `010ca183a2e26e2d777fe2d767697a38c63c1e37812cdd127f6c0dce2f5ed13a`.
- The patch passed `git apply --check`, applied to a scratch copy of the exact stage-30 base, and reproduced all seven added files byte for byte. The [manifest](cap03-detour-plan-manifest-20261013.json) records all source and build hashes; the [evidence ZIP](cap03-detour-plan-evidence-20261013.zip) includes the patch, manifest, test/build logs, target objects, ELF, and a replay receipt. ZIP CRC verification passed.

## Review and next work

This is **not a console-only wait** and does not prove hooks for every DLL or game. The planner refuses layouts it cannot preserve, including out-of-range short branches, EIP-relative address-size wrapping, writes into the stolen prologue, an internal `CALL` that would change a call/pop return address, and an instruction-boundary violation. External `CALL` relocation preserves control flow but can change a return address observed by stack inspection or unwind logic. Generic safe use still needs a near trampoline allocator or branch widening, compatibility checks for unwind/return-address-sensitive code, executable-memory preparation, a quiescent install/remove transaction integrated with CodePatch, and target-resolved call/instruction compatibility. Console access is needed to verify actual PS5 mapping, protection, cache/coherence, GS, and restoration behavior. Intended gameplay remains a separate game-build-dependent gate.

The exact base is `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-v2-query-guard-20261013-30`. Apply this increment only to that base. Claude's #2/#4–12 work, shared candidate, kernel, and original DLLs were not edited.
