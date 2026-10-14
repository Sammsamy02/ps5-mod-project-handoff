# POST-BUILD-SUMMARY — l25 twin build aftermath

- Generated (UTC): 20261008T224732Z
- Mode: execute; packaging: required
- Driver: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/packet/aftermath.py`

## Headline artifacts (sha256)

- `target_compile` — ? B, `?` (/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/typescan-r4/results/TARGET-COMPILE.json)
- `runtime-l25-A.o` — 119999360 B, `599906eb6ba536fad8fcd12d867638d8ad6a3f3b78f66eeb969e1b7a226e4550` (/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-build-variants/l25-A/pass1/runtime-l25-A.o)
- `runtime-l25-B.o` — 119704408 B, `e9dad66fa326938d3d37c9d093180e7c507cd44a6c74400c8e3c0432df045ecc` (/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-build-variants/l25-B/pass1/runtime-l25-B.o)

## Gate results

- **runtime_A_ne_B**: `{"A_pass1_sha256": "599906eb6ba536fad8fcd12d867638d8ad6a3f3b78f66eeb969e1b7a226e4550", "A_pass2_sha256": "599906eb6ba536fad8fcd12d867638d8ad6a3f3b78f66eeb969e1b7a226e4550", "B_pass1_sha256": "e9dad66fa326938d3d37c9d093180e7c507cd44a6c74400c8e3c0432df045ecc", "B_pass2_sha256": "e9dad66fa326938d3d37c9d093180e7c507cd44a6c74400c8e3c0432df045ecc", "pass_twins_identical": true}`
- **execution_receipt**: `{"documented_noninvariant_waived": 1, "failed": 0, "gates": 61, "path": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-build-variants/EXECUTION-RECEIPT.json"}`
- **import_sets**: `{"A_count": 207, "B_count": 207, "sets_equal": true}`
- **B_unsafe_label**: `true`
- **static_A_pass-1**: `"20/20"`
- **static_A_pass-2**: `"20/20"`
- **static_B_pass-1**: `"20/20"`
- **static_B_pass-2**: `"20/20"`

## Suite re-runs

- **item6**: `{"path": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/tests/RESULTS.json", "results": "2/2", "status": "PASS_HOST_ONLY_MAC"}`
- **profile**: `{"path": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/profile/tests/RESULTS.json", "runner": "/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/profile/tests/run_tests.py", "runner_selected": "l25", "status": "PASS_HOST_ONLY_MAC"}`

## Review packet

- zip: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/packet/review-l25-20261008T224714Z.zip` (53636548 B, sha256 `02821ae35cbf94dc684a1019de65ef03c755485e7556babf033e2d89bd2593c0`)
- verify: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/packet/VERIFY-20261008T224714Z.md` (OVERALL: PASS)
- create-only: never overwrite; one name per review round

## Evidence copies

- `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/evidence/aftermath-20261008T224732Z` (receipts, gate tables, PRX twins, COPIES.md)

## Next

- Hand the zip + VERIFY to Claude BEFORE any console upload (LAUNCH-PLAN §0.3/§6 Stage 1).
- Launch order v3: variant B FIRST, then A; see LAUNCH-PLAN.md §v3.2 and matrix v4.1. B is NEVER left resident.
- No console contact happened in this run.
