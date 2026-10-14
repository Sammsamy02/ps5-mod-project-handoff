# DLL execution evidence and the next acceptance target

Mac-only analysis, 2026-10-13. No console contact or new console evidence. Source IDs resolve to absolute paths and complete SHA256s in SOURCE-PINS.json. Original artifacts are unchanged. The handoff supplied context; it did not authorize console actions, kernel changes, external dispatch, or publication.

KF’s report is present (S11), with a completed final log S12. This continuation read the files; it did not rerun the wedge-prone suite. Snapshot hashes in SOURCE-PINS identify exactly what was reviewed, without claiming the agent’s live ownership status.

## Loading

S12 records **98 PASS check lines, zero FAIL check lines, 19 RESULT PASS blocks**, and a separate bridge6 case wedged on all three attempts. Four diagnostic WEDGE lines describe that single case; a fifth line prints the summary count. These are not independent test cases. The runner also invokes a nonexistent `cap13-mod-feature-probe.dll` and logs `cannot open` without failing the suite. Thus “all scheduled cases passed” is false. The 98 checks remain actual logged passes; the suite’s completion status is not a reliable aggregate verdict.

S13 explains why: run_case polls liveness without `wait`/exit-status collection, returns zero even after permanent failure, and the compile pipeline lacks pipefail. Final grep counts are informational, not an exit gate. Correct a future runner revision to require successful build and child exit codes, expected named case inventory and nonzero check counts, with TIMEOUT/MISSING/FAIL separate from PASS. Preserve old raw logs and do not blindly replay wedged attempts.

Inputs are not all #9-ABI fixtures. KF executed a hand-crafted PE fixture plus locally compiled UE4SS C-lifecycle samples (`cap13-ue4ss-flavor-*`), using a test completion resolver and host-text bridge entry stubs. Their provenance is still **local fixture/sample**, not downloaded upstream gameplay DLLs. The local mbm_mod_entry_v1 samples were chiefly TLS-refusal cases. No real C++ UE4SS object-model flow or genuine CRT initialization was established by the direct-DllMain/noentry samples.

Unchanged upstream framework S21 is rejected for real static TLS; unchanged proxy S22 is rejected for unresolved imports. They are legitimate negative tests, not successful upstream execution. Staged shim coverage differs from the custom test resolver; S11 reports 0/4 for the console-dump sample under the staged registry alone.

## Target resolution

Mapping, relocation and selected test import crossings have recorded Mac evidence. The custom UE4SS service bodies record strings and return canned values; they do not establish that the intended engine action occurred. Actual game functions/cross-build correspondence were not exercised.

## Working patch behavior

No unchanged upstream gameplay DLL behavior is proven. A recorded execute_lua_in_mod call with the expected string proves the crossing, not actor dumping. CreateThread is explicitly refused, so poller lifecycle return is not working polling. The bridge6 wedge is unresolved; its attribution to Rosetta is a hypothesis, not proof of PS5 correctness.

## Acceptance target decision

Choose **(b) a genuine upstream C++ gameplay mod release binary** as the next positive target; positive execution is currently blocked on artifact acquisition and the ABI/runtime gaps in C2. Option (a) framework stress is already a useful negative test, but real static TLS prevents a positive run under the current loader policy. Do not remove the guard simply to make that binary pass.

Artifact needed: the original release DLL or ZIP, upstream release URL/tag, intended game and build, UE4SS/SDK version and an observable expected behavior. Verify license/provenance and SHA256, analyze without modifying the binary, then select a supported envelope or report the precise refusal. No user upload is the only theoretical acquisition route, but no verified suitable upstream release binary was located in the supplied project evidence. Do not invent one or silently substitute a rebuilt example.

Positive acceptance requires immutable input hashes; actual entry→CRT→constructors where present; resolved and executed imports reported separately; real callback behavior; observable game-side outcome and cleanup. Mac execution and subsequent PS5 behavior receive distinct verdicts. No console action is authorized by this decision.
