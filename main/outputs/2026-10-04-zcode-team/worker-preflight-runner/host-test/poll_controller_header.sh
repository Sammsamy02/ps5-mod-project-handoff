#!/bin/bash
# worker-preflight-runner: Stage-2 poller.
# Watches the controller's final-units SeedValidation.hpp for the corrected
# publication policy (image-range exclusion). Read-only observation of the
# controller file; on change, copies the NEW bytes into THIS worker's
# staging-corrected dir only. Never modifies the controller's file.
FU=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp
W=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-preflight-runner
LOG=$W/results/poll.log
BASE_SHA=b3d81b0f57188d0c18ff4d308890035c253625c9f3dc36d16038d38eb6552a01
echo "$(date -u +%FT%TZ) poll start; baseline sha=$BASE_SHA" > "$LOG"
for i in $(seq 1 25); do
  CUR=$(shasum -a 256 "$FU" | awk '{print $1}')
  MT=$(stat -f %m "$FU")
  echo "$(date -u +%FT%TZ) poll $i/25 sha=$CUR mtime=$MT" >> "$LOG"
  if [ "$CUR" != "$BASE_SHA" ]; then
    cp "$FU" "$W/staging-corrected/SeedValidation.controller.hpp"
    echo "$(date -u +%FT%TZ) CHANGED -> copied to staging-corrected/SeedValidation.controller.hpp" >> "$LOG"
    exit 0
  fi
  sleep 60
done
echo "$(date -u +%FT%TZ) TIMEOUT: no change in 25 min" >> "$LOG"
exit 2
