#!/bin/zsh
# Reproduces the worker-runtime-ack host test from scratch:
# pristine bootstrap copies -> apply runtime-ack.diff -> compile with
# /usr/bin/clang++ against the vendored mfa status component -> run.
set -euo pipefail
HERE=${0:A:h}
ROOT=$HERE/../../../..                   # worktree root
BOOT=$ROOT/outputs/ps5-mod-bridge/native-runtime-startup/bootstrap
VEND=$ROOT/outputs/2026-10-04-zcode-team/worker-installer-ps5io/vendored
VEND7=$ROOT/outputs/2026-10-04-zcode-team/worker-7-installer/vendored
WORKB="$HERE/apply/outputs/ps5-mod-bridge/native-runtime-startup/bootstrap"

mkdir -p "$HERE/out"
rm -rf "$HERE/apply"
# 1. Fresh pristine copies of the four affected files (+ untouched test).
mkdir -p "$HERE/base/bootstrap"
cp "$BOOT"/{runtime_owner.hpp,runtime_owner.cpp,ue4ss_backend.hpp,ue4ss_backend.cpp} \
   "$BOOT/tests/runtime_owner_test.cpp" "$HERE/base/bootstrap/"
# 2. Apply the deliverable diff exactly as the controller would, against a
#    replica of the worktree layout (the worktree itself stays read-only).
mkdir -p "$HERE/apply/outputs/ps5-mod-bridge/native-runtime-startup"
cp -R "$HERE/base/bootstrap" "$WORKB"
( cd "$HERE/apply" && patch -p1 < "$HERE/../runtime-ack.diff" )
# 3. Vendored C component (mfa_status_*).
/usr/bin/clang -c -std=c99 -O2 -Wall -Wextra -I"$VEND" -I"$VEND7" \
    "$VEND/modforge_activation.c" -o "$HERE/out/modforge_activation.o"
# 4. Compile the PATCHED bootstrap + REAL activated_mod_source + stubs.
CXX=(/usr/bin/clang++ -std=c++20 -O2 -Wall -Wextra
     -I"$WORKB" -I"$HERE/stubs" -I"$VEND" -I"$VEND7" -I"$BOOT")
"${CXX[@]}" "$HERE/backend_ack_test.cpp" \
    "$WORKB/ue4ss_backend.cpp" "$WORKB/runtime_owner.cpp" \
    "$BOOT/activated_mod_source.cpp" "$HERE/out/modforge_activation.o" \
    -o "$HERE/out/backend_ack_test"
# 5. Regression: the UNCHANGED original owner test against the patched owner.
cp "$HERE/base/bootstrap/runtime_owner_test.cpp" "$WORKB/"
"${CXX[@]}" "$WORKB/runtime_owner_test.cpp" \
    "$WORKB/runtime_owner.cpp" "$HERE/out/modforge_activation.o" \
    -o "$HERE/out/runtime_owner_regression"
# 6. Sanitized variant of the ack test.
SAN=(/usr/bin/clang++ -std=c++20 -O1 -g -fsanitize=address,undefined)
"${SAN[@]}" -I"$WORKB" -I"$HERE/stubs" -I"$VEND" -I"$VEND7" -I"$BOOT" \
    "$HERE/backend_ack_test.cpp" \
    "$WORKB/ue4ss_backend.cpp" "$WORKB/runtime_owner.cpp" \
    "$BOOT/activated_mod_source.cpp" "$HERE/out/modforge_activation.o" \
    -o "$HERE/out/backend_ack_test_asan"
# 7. Run everything.
"$HERE/out/backend_ack_test"
"$HERE/out/runtime_owner_regression"
"$HERE/out/backend_ack_test_asan" > /dev/null && echo "PASS sanitized ack test"
echo "ALL HOST TESTS PASSED"
