#!/bin/bash
# Mac-only. Runs the compile-based tests ONE AT A TIME, each only when the Mac has >= 2 GB free (the rule from the task:
# skip compiles under 2 GB). Polls every 20 s for up to 100 minutes per stage, retries a stage that SKIPs. Output:
# compile-tests-r8.txt (this dir). Exit 0 only when every stage ran and passed; 1 on a failed stage; 4 when memory never
# came back (stages not run are listed, never reported as passed).
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=$HERE/compile-tests-r8.txt
SCR=${SCR:-$(mktemp -d "${TMPDIR:-/tmp}/ctr8.XXXXXX")}
free_mb() { p=$(vm_stat | awk '/Pages free/ {gsub("\\.","",$3); print $3}'); echo $((p * 16384 / 1048576)); }
: > "$OUT"
log() { echo "$@" | tee -a "$OUT"; }
stage() {  # name cmd...
  name=$1; shift
  for i in $(seq 1 300); do
    m=$(free_mb)
    if [ "$m" -ge 2048 ]; then
      log "== $name  (free ${m} MB, attempt $i, $(date +%H:%M:%S))"
      "$@" >> "$OUT" 2>&1; rc=$?
      if [ $rc -eq 2 ] || [ $rc -eq 3 ]; then log "   SKIP (memory dropped), retrying"; sleep 20; continue; fi
      log "   exit $rc"
      return $rc
    fi
    sleep 20
  done
  log "== $name  NOT RUN (memory stayed under 2 GB)"; return 4
}
fail=0; notrun=0
stage "B5 host test (host-test/build.sh)" sh "$HERE/host-test/build.sh"; rc=$?; [ $rc = 4 ] && notrun=$((notrun+1)) || [ $rc = 0 ] || fail=$((fail+1))
stage "B5 sites host test (sites/hosttest/build_and_run.sh)" sh "$HERE/sites/hosttest/build_and_run.sh" "$SCR/sites"; rc=$?; [ $rc = 4 ] && notrun=$((notrun+1)) || [ $rc = 0 ] || fail=$((fail+1))
stage "B6 host + pump + shim tests (b6-diagnostics/tests/build_and_run.sh)" sh "$HERE/b6-diagnostics/tests/build_and_run.sh"; rc=$?; [ $rc = 4 ] && notrun=$((notrun+1)) || [ $rc = 0 ] || fail=$((fail+1))
stage "B6 mutation check (b6-diagnostics/tests/mutation_check.sh)" sh "$HERE/b6-diagnostics/tests/mutation_check.sh"; rc=$?; [ $rc = 4 ] && notrun=$((notrun+1)) || [ $rc = 0 ] || fail=$((fail+1))
log "compile tests: failed=$fail notrun=$notrun"
[ $fail -eq 0 ] && [ $notrun -eq 0 ]
