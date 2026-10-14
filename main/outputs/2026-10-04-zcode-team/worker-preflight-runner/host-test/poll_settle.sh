#!/bin/bash
# Settling poller: watch for FURTHER edits of the controller header after the
# first change (sha 8fc7b694...). Every 30s, up to 10 min. Exit 0 on a new
# change (copies new bytes), exit 3 if stable (no further edit) for the window.
FU=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp
W=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-preflight-runner
LOG=$W/results/poll-settle.log
FIRST=db07ac602cd9e57aeb864780874aa7e9b16158d25aefdc200c32ffd564871f83
echo "$(date -u +%FT%TZ) settle poll start; first-change sha=$FIRST" > "$LOG"
for i in $(seq 1 20); do
  CUR=$(shasum -a 256 "$FU" | awk '{print $1}')
  echo "$(date -u +%FT%TZ) poll $i/20 sha=$CUR" >> "$LOG"
  if [ "$CUR" != "$FIRST" ]; then
    cp "$FU" "$W/staging-corrected/SeedValidation.controller.hpp"
    echo "$(date -u +%FT%TZ) FURTHER CHANGE -> recopied to staging-corrected/SeedValidation.controller.hpp" >> "$LOG"
    exit 0
  fi
  sleep 30
done
echo "$(date -u +%FT%TZ) STABLE: no further change in 10 min window" >> "$LOG"
exit 3
