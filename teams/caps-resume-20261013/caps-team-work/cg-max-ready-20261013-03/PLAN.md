# Mac-only #1/#3/DLL readiness plan (2026-10-13)

Goal: prepare a source-pinned handoff for integration without writing to Claude's candidate, #2, or #4–#12 work; distinguish remaining external blockers from local gaps.

Scope: own `cg-max-ready-20261013-03/` only. Read prior `cg-ready-20261013-02`, `cg-resume-20261013-01`, and the candidate read-only. No console contact, kernel edit, deployment build, mod-binary mutation, or shared branch commit.

- [x] Confirm baseline source hashes, test results, and candidate integration points. Identify actual Mac gaps rather than repeat the prior handoff.
- [x] Repair the runner grammar mismatch against the real e2e output with a failing adversarial test first; rerun bounded tests and the two known TLS-refusal upstream tools from a new manifest/revision.
- [x] Re-run #1 and #3 isolated Mac checks and offline syntax/compile checks where they add evidence; do not treat them as console acceptance.
- [x] Prepare guarded integration instructions for the current candidate without editing it; record exact source and target hashes plus conflict states.
- [x] Acquire two unchanged upstream C++ gameplay DLLs from official releases. Inspect imports and TLS and test expected loader refusal without editing or executing the DLLs.
- [x] Publish a readiness matrix with loading, target resolution, working patch behavior, verified evidence, and the remaining gates.
