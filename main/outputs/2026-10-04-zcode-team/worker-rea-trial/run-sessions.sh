#!/bin/zsh
export PATH=/opt/homebrew/opt/openjdk@21/bin:$PATH
export GHIDRA_INSTALL_DIR=/WORKSPACE/Projects/re-tools/ghidra_12.1.4_PUBLIC
BIN=/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/townfall-static-executable-2026-10-02/decrypted-eboot.bin
OUT=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-rea-trial
echo "=== rea function 0x23d590 start $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
/usr/bin/time -p rea function "$BIN" 0x23d590 --provider ghidra --format json > "$OUT/function-23d590.json" 2> "$OUT/function-23d590.time"
echo "=== rea function exit=$? end $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
echo "=== rea xrefs 0x9365ec0 start $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
/usr/bin/time -p rea xrefs "$BIN" 0x9365ec0 --provider ghidra --format json > "$OUT/xrefs-pool.json" 2> "$OUT/xrefs-pool.time"
echo "=== rea xrefs exit=$? end $(date -u +%Y-%m-%dT%H:%M:%SZ) ==="
echo "ALL SESSIONS DONE"
