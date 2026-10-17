# Capability #3: offline detour transaction, stage 57

## Scope

Stage `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-cap03-detour-transaction-20261013-57` is an isolated, exact-stage-50 increment. It adds `DetourTransaction` without editing the shared candidate, kernel, original DLLs, or Claude's #2/#4–12 work.

The transaction owns the RX near trampoline and checked patch backup together. `prepare` only plans and seals the trampoline. `install` requires a caller-supplied quiescence lease, then compares target bytes against the planned snapshot through `patch_code_if_original_ex`. `remove` restores through `unpatch_code_ex` and releases the trampoline **before** quiescence ends. The callback receives both target and trampoline spans. Its contract requires draining in-flight handler uses that might later call the trampoline. Failed patch restoration, failed unpatch, or failed quiescence exit marks the transaction dirty and retains a potentially reachable trampoline. Destroying an installed or dirty transaction terminates the process instead of silently freeing its code mapping.

This composes the existing stage-45 checked patch protocol and stage-50 near trampoline. It does **not** supply or verify a console quiescence provider. The host transaction test substitutes the patch boundary to check ordering and ownership; the separate protocol suite exercises the real `CodePatch.cpp` under mocked system calls. The target build links the real components together, but does not execute them on a console.

## Verification

| Check | Result |
| --- | --- |
| Transaction lifecycle and failure paths, x86-64 host | 11/11 |
| Same checks with AddressSanitizer | 11/11 |
| Existing near trampoline tests, normal and AddressSanitizer | 12/12 each |
| Existing detour planner | 24/24 |
| Existing patch/restore protocol and CHECK instruction disassembly | 124/124; static checks pass |
| Pinned PS5 SDK compile and offline ELF link | Pass; `DetourTransaction.o` linked |
| Exact-stage-50 scratch patch replay | Pass; four changed sources match byte for byte |
| Evidence ZIP integrity | CRC pass |

The source increment is [cap03-detour-transaction-increment-20261013.patch](cap03-detour-transaction-increment-20261013.patch). File hashes, base identity, test counts, and evidence hashes are in the [manifest](cap03-detour-transaction-manifest-20261013.json). The [evidence ZIP](cap03-detour-transaction-evidence-20261013.zip) contains the host and target logs, target objects/ELF, symbol table, patch, manifest, and replay receipt. The patch SHA-256 is `89d16224b12ecad6a89c480829529d898fb8d1e53f1e9756703ee896278d70d7`.

## Remaining gates

This is **not a console-only wait**. A console provider must prove it can suspend or otherwise exclude all threads executing either span and drain future handler-to-trampoline uses. PS5 mapping/protection/cache and full restore behavior still need target checks. The x64 relocation planner deliberately refuses unsupported prologues. Function identity and call/instruction compatibility still need automatic, authenticated PC-to-PS5 matching. #1 register-read behavior and the unchanged-DLL Windows/CRT/GS route remain separate blockers. No game hook was installed, and no intended gameplay effect was observed.
