#!/bin/sh
# target_check.sh - Mac-only PS5-toolchain checks of the STAGED entry (no PRX,
# no packaging, no console). Logs every command, the toolchain path and
# version, and each exit code, so the evidence log can be re-checked.
#  1. -fsyntax-only with the packaging's exact flags (pass1-controller.log:7)
#  2. -c to one object (symbol listing only; never packaged)
#  3. llvm-nm -u of that object vs the B4 entry object; prints the new
#     undefined symbols and checks each is a row in EXACT-IMPORT-IDENTITIES
# Memory guard: skip under 2 GB available / memory_pressure free < 15%.
# Prints how many files it checked; exits 1 on any failure or zero files.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WHAT=/WORKSPACE/.codex/worktrees/translation-core/what
PKG=$WHAT/outputs/2026-10-08-l26-package-A
LIN=$PKG/lineage
SDKBIN=$WHAT/outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin
LLVM=$WHAT/outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin
SRC=$HERE/staged/lifecycle/runtime_module_entry.c
OBJ=$HERE/evidence/staged-runtime_module_entry.o
B4OBJ=$PKG/pass-1/build/runtime_module_entry.o
IMPORTS=$PKG/pass-1/EXACT-IMPORT-IDENTITIES.json
avail_gb=$(vm_stat | awk '/Pages free|Pages inactive|Pages speculative|Pages purgeable/ {gsub("\\.","",$NF); s+=$NF} END {printf "%.2f", s*16384/1e9}')
mp_free=$(memory_pressure 2>/dev/null | awk -F': ' '/free percentage/ {gsub("%","",$2); print $2}')
echo "TARGET-CHECK: memory available ${avail_gb} GB, memory_pressure free ${mp_free:-unknown}%"
if [ "$(echo "$avail_gb < 2" | bc)" = "1" ] || [ -z "$mp_free" ] || [ "$mp_free" -lt 15 ]; then
  echo "TARGET-CHECK: SKIP - low memory"; exit 2; fi
[ -f "$SRC" ] || { echo "TARGET-CHECK: FAIL - staged file missing; checked 0 files"; exit 1; }
export PATH="$LLVM:$PATH"
echo "TARGET-CHECK: PATH prefix $LLVM"
echo "TARGET-CHECK: toolchain $("$SDKBIN/prospero-clang" --version 2>&1 | head -1)"
echo "TARGET-CHECK: staged sha256 $(shasum -a 256 "$SRC" | cut -d' ' -f1)"
FLAGS="-Wall -Wextra -Werror -O1 -fPIC -fno-stack-protector -fvisibility=hidden -ffreestanding -fno-builtin"
INC="-I$LIN/generated -I$LIN/probe -I$LIN/lifecycle -I$LIN -I$WHAT/.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include"
fail=0
echo "TARGET-CHECK: run: $SDKBIN/prospero-clang $FLAGS $INC -fsyntax-only $SRC"
# shellcheck disable=SC2086
"$SDKBIN/prospero-clang" $FLAGS $INC -fsyntax-only "$SRC"; rc=$?
echo "TARGET-CHECK: syntax exit=$rc"; [ $rc -eq 0 ] || fail=1
echo "TARGET-CHECK: run: $SDKBIN/prospero-clang $FLAGS $INC -c $SRC -o $OBJ"
# shellcheck disable=SC2086
"$SDKBIN/prospero-clang" $FLAGS $INC -c "$SRC" -o "$OBJ"; rc=$?
echo "TARGET-CHECK: object exit=$rc"; [ $rc -eq 0 ] || fail=1
"$LLVM/llvm-nm" -u "$OBJ" | awk '{print $NF}' | sort -u > "$HERE/evidence/staged-entry-undefined.txt"
"$LLVM/llvm-nm" -u "$B4OBJ" | awk '{print $NF}' | sort -u > "$HERE/evidence/b4-entry-undefined.txt"
new=$(comm -23 "$HERE/evidence/staged-entry-undefined.txt" "$HERE/evidence/b4-entry-undefined.txt")
gone=$(comm -13 "$HERE/evidence/staged-entry-undefined.txt" "$HERE/evidence/b4-entry-undefined.txt")
echo "TARGET-CHECK: undefined symbols staged=$(wc -l < "$HERE/evidence/staged-entry-undefined.txt" | tr -d ' ') b4=$(wc -l < "$HERE/evidence/b4-entry-undefined.txt" | tr -d ' ')"
echo "TARGET-CHECK: new undefined: ${new:-none}"
echo "TARGET-CHECK: dropped undefined: ${gone:-none}"
for s in $new; do
  if grep -q "\"name\": *\"$s\"" "$IMPORTS"; then echo "TARGET-CHECK: $s is an existing PRX import row (EXACT-IMPORT-IDENTITIES.json)"
  else echo "TARGET-CHECK: FAIL - $s is NOT an existing PRX import"; fail=1; fi
done
echo "TARGET-CHECK: checked 1 file(s)"
[ $fail -eq 0 ] && echo "TARGET-CHECK: PASS" || echo "TARGET-CHECK: FAIL"
exit $fail
