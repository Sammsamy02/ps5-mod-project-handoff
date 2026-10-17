#!/bin/bash
# worker-preflight-runner: official Stage 2 run + Stage 3 determinism runs.
# Run ONLY after the controller header has settled. Verifies the settled sha
# before and after every run; refuses to proceed if the header drifts mid-run.
set -u
W=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-preflight-runner
FU=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp
cd "$W" || exit 9

SETTLED=$(shasum -a 256 "$FU" | awk '{print $1}')
echo "settled controller header sha256=$SETTLED"
cp "$FU" staging-corrected/SeedValidation.controller.hpp
cp "$FU" staging-corrected/SeedValidation.hpp

HDR() { shasum -a 256 "$FU" | awk '{print $1}'; }

# build both binaries from the SAME harness source
SHA1=$(shasum -a 256 staging/SeedValidation.hpp | awk '{print $1}')
clang++ -std=c++20 -Wall -Wextra -O2 -Istaging host-test/test_preflight_host.cpp \
  -o host-test/bin/test_preflight_repro || exit 8
clang++ -std=c++20 -Wall -Wextra -O2 -Istaging-corrected host-test/test_preflight_host.cpp \
  -o host-test/bin/test_preflight_corrected || exit 8

run_all() { # $1 label
  local lbl="$1"
  [ "$(HDR)" = "$SETTLED" ] || { echo "HEADER DRIFTED before $lbl"; exit 7; }
  ./host-test/bin/test_preflight_repro repro "results/repro.$lbl.json" \
      "results/repro.$lbl.stable.txt" "results/repro.$lbl.receipt.txt" "$SHA1"
  echo "repro.$lbl exit=$?"
  ./host-test/bin/test_preflight_corrected corrected "results/corrected.$lbl.json" \
      "results/corrected.$lbl.stable.txt" "results/corrected.$lbl.receipt.txt" "$SETTLED"
  echo "corrected.$lbl exit=$?"
  [ "$(HDR)" = "$SETTLED" ] || { echo "HEADER DRIFTED after $lbl"; exit 7; }
}

# official stage-2 run (label run1) + two determinism reruns
run_all run1
run_all run2
run_all run3

# official stage-1 artifacts are the existing repro.json/stable/receipt
# (run1 rerun refreshes them with identical content; sync the canonical names)
cp results/repro.run1.json results/repro.json
cp results/repro.run1.stable.txt results/repro.stable.txt
cp results/repro.run1.receipt.txt results/repro.receipt.txt
cp results/corrected.run1.json results/corrected.json
cp results/corrected.run1.stable.txt results/corrected.stable.txt
cp results/corrected.run1.receipt.txt results/corrected.receipt.txt

# determinism diffs (timing-free stable outputs)
{
  echo "== repro: run1 vs run2 =="; diff results/repro.run1.stable.txt results/repro.run2.stable.txt && echo IDENTICAL
  echo "== repro: run2 vs run3 =="; diff results/repro.run2.stable.txt results/repro.run3.stable.txt && echo IDENTICAL
  echo "== corrected: run1 vs run2 =="; diff results/corrected.run1.stable.txt results/corrected.run2.stable.txt && echo IDENTICAL
  echo "== corrected: run2 vs run3 =="; diff results/corrected.run2.stable.txt results/corrected.run3.stable.txt && echo IDENTICAL
} > results/determinism.diff.txt 2>&1
cat results/determinism.diff.txt
