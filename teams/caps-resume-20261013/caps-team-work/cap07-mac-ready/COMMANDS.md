# Exact preparation and verification commands

Working directory for every command:
`/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013`.
Existing stage/output names are deliberately exclusive; reruns require fresh
owned output names. Compiler invocations themselves are recorded as full argv
arrays in each `PREPARE.json` and `COMPILE.json`.

```sh
python3 caps-team-work/cap07-mac-ready/tools/build_integration.py \
  --root /WORKSPACE/.codex/worktrees/translation-core/what \
  --out caps-team-work/cap07-mac-ready/derived/review-final

python3 caps-team-work/cap07-mac-ready/tests/test_build_integration.py -v

python3 caps-team-work/cap07-mac-ready/tools/dry_run_build_selection.py \
  --stage caps-team-work/cap07-mac-ready/derived/review-final \
  --out caps-team-work/cap07-mac-ready/evidence/BUILD-SELECTION.json \
  --audit-out caps-team-work/cap07-mac-ready/evidence/ORIGINAL-AUDIT.json

python3 caps-team-work/cap07-mac-ready/tools/prepare_compile_stage.py \
  --out caps-team-work/cap07-mac-ready/derived/target-ready

python3 caps-team-work/cap07-mac-ready/tools/compile_target.py \
  --stage caps-team-work/cap07-mac-ready/derived/target-ready \
  --out caps-team-work/cap07-mac-ready/target/attempt2 --compile

python3 caps-team-work/cap07-mac-ready/tools/compile_target.py \
  --stage caps-team-work/cap07-mac-ready/derived/target-ready \
  --out caps-team-work/cap07-mac-ready/target/cache-consumers \
  --units luamod,native-thread-name-nativethreadnameadapter,native-thread-name-threadnameruntime \
  --compile

python3 caps-team-work/cap07-mac-ready/tools/compile_target.py \
  --stage caps-team-work/cap07-mac-ready/derived/target-ready \
  --out caps-team-work/cap07-mac-ready/target/program-cap10-composed-fixed \
  --units program \
  --program-source /WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cap10-integration-ready/derived/target-ready/outputs/2026-10-08-l26-runtime/typescan-r4/candidate/src/UE4SSProgram.cpp \
  --program-source-sha256 4fab1e61549bdb8ff6bcc10cdfa35c47b350086e80ec937d9b8c0b4996ba2eba \
  --extra-include caps-team-work/cap10-integration-ready/derived/target-ready/outputs/2026-10-08-l26-runtime/cap10/native \
  --extra-include caps-team-work/cap10-port-closure/handoff/runtime-headers \
  --extra-include caps-team-work/cap10-port-closure/handoff/runtime-headers/mono/eglib \
  --cap10-program --compile

python3 caps-team-work/cap07-mac-ready/tests/test_mac_ready.py -v

python3 caps-team-work/cap07-mac-ready/tests/test_mac_ready.py \
  MacReady.test_real_compile_preparation_has_current_routes_and_immutable_headers -v

python3 caps-team-work/cap07-mac-ready/tools/verify_target_evidence.py
```

`TARGET-COMPILE-attempt1.log` records the initial strict stale-header-pin refusal
before any compiler invocation. `TARGET-PROGRAM-CAP10-COMPOSED.log` and
`target/program-cap10-composed/` preserve the relative-include preparation
failure. The fixed program command differs only in the normalized supplemental
include paths emitted by the owned helper. The target compile budget was one
job, all invocations sequential. No driver main or game link was invoked.
