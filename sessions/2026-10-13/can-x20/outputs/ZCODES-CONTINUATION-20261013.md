# ZCodes continuation — Mac-only correction handoff

The 2026-10-13 independent review of `cg-ready-20261013-02` found three important and two minor issues. I checked those findings against the staged source, reproduced the important failures with host tests, and corrected all five in the existing staged package. This document reports Mac-only evidence. It does not rate console behavior.

## Changes and evidence

| Finding | Staged correction | Fresh Mac check |
|---|---|---|
| I1, liveness accepts CF=1/RAX=0 | Requires both raw RAX zero and CF clear; logs both observations | 4/4 liveness observations pass; CF=1/RAX=0 failed before the fix |
| I2, failed scope recovery leaves dirty latch clear | Failed armed-scope revert or destructor recovery latches dirty and refuses new patches | New pre-write refusal case failed before the fix; cap03 suite 22/22 cases passes |
| I3, runner silently ignores failed CHECK | Every CHECK and RESULT protocol line must parse fully before PASS | Adversarial `FAIL:` case failed before the fix; runner 11/11 tests pass |
| M1, repeated revert retries | Repeated `revert()` returns the recorded result without another unpatch cycle | New scope-refusal case checks no second restoration cycle |
| M2, probe understates gates | Summary counts five failure gates, including EFER transport/classifier failure | New EFER failure case failed before the correction; probe 6/6 flows and classifier 13/13 pass |

Construction code was unchanged in this continuation; its host test passed 26/26 checks. All checks above ran locally on the Mac. No target build or console action occurred.

## Source pins

Stage root: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-ready-20261013-02`

| File under stage root | SHA-256 |
|---|---|
| `cap03/src/CodePatch.cpp` | `9dc639c124340f00f6ca6fe7041e38f3164b4d34b98c681dc38f868805aa092f` |
| `cap03/src/CodePatch.hpp` | `793bc7158cc91d870172f69a3c59e3202a69fe9ae5ff6953f1d1031a0d7a6815` |
| `cap01/KernelReadProbe-v2.c` | `21486d16fc9ca6fe57f8d8f42f55382776301097c7396ed65e88e1c565fa770a` |
| `cap13/runner/runner.py` | `08f0ae42e4368320cf86185002fc86191bd6b444388fd68e04b3455792f6f150` |
| `cap13/construct/include/Mod/NativeModAbi.h` (unchanged) | `53e4c59a9a987649060265ed193aafab777722253c4700d9b34125379cad9ff4` |

## Capability status

- **Loading:** Mac-only fixture and dependency evidence exists. No unchanged upstream gameplay DLL has demonstrated behavior. The downloaded upstream EventViewerMod and KismetDebuggerMod DLLs are framework tools, not gameplay mods.
- **Target resolution:** The three-stage retargeting proposal is in `cg-resume-20261013-01/C5-RETARGETING-PROPOSAL.md`; name resolution, cross-build correspondence, and instruction compatibility remain separate acceptance steps. This continuation did not validate a PS5 target.
- **Working patch behavior:** Mac-only protocol tests pass. Executable-memory patching, restoration, hook behavior, and native construction in the intended game remain unverified on the console. Phase B and each console action require the user's separate authorization under the worktree rules; the PS5 was marked unavailable in the handoff.

## Next integration step

The integrator can compare the pinned staged sources and tests with the shared candidate, then run the candidate's normal gates. Preserve the prior review and this correction receipt together. Do not label the package console-proven or unchanged-gameplay-DLL compatible until those behaviors are observed with fresh evidence.
