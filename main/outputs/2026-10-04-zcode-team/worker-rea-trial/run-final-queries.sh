#!/bin/zsh
# worker-rea-trial — final REA queries via persistent-project -process mode.
# Requires: the launcher patch (REA_GHIDRA_PROCESS_* env) and a completed
# analysis in /tmp/rea_trial_ghidra_proj (program decrypted-eboot.bin).
set -u
export PATH=/opt/homebrew/opt/openjdk@21/bin:$PATH
export GHIDRA_INSTALL_DIR=/WORKSPACE/Projects/re-tools/ghidra_12.1.4_PUBLIC
export GHIDRA_HEADLESS_MAXMEM=8G
export REA_GHIDRA_PROCESS_PROJECT_DIR=/tmp/rea_trial_ghidra_proj
export REA_GHIDRA_PROCESS_PROJECT_NAME=fnamepool
export REA_GHIDRA_PROCESS_PROGRAM=decrypted-eboot.bin
BIN=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin
OUT=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-rea-trial

run_one() {
  local sub="$1"; local addr="$2"; local out="$3"
  echo "=== rea $sub $addr start $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
  /usr/bin/time -p rea "$sub" "$BIN" "$addr" --provider ghidra --format json > "$OUT/$out" 2> "$OUT/$out.time"
  echo "=== rea $sub exit=$? end $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
}

run_one function 0x23d590 function-23d590.json
run_one xrefs    0x9365ec0 xrefs-pool.json
run_one xrefs    0x23d590  xrefs-ctor.json
run_one decompile 0x23a5d0 decompile-pool-init.json
echo "ALL FINAL SESSIONS DONE"
