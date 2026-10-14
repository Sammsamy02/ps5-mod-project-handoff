#!/bin/zsh
# worker-ack-integration-deep host suite.
# 1. DRIFT GATE: fresh copy of the CURRENT outputs bootstrap + runtime-ack.diff
#    must reproduce the vendored tree byte-for-byte (all 8 files).
# 2. Build the delivered VENDORED ack-enabled bootstrap with the REAL current
#    activated_mod_source.cpp and the REAL vendored modforge_activation.c
#    against the stub port headers; run the original T1-T8 ack suite, the
#    original unchanged runtime_owner regression, and the NEW adversarial pass;
#    ASan/UBSan variants of both suites.
set -euo pipefail
HERE=${0:A:h}
ROOT=$HERE/../../../..                    # suite -> worker dir -> team dir -> outputs -> worktree root
W=$ROOT
BOOT=$W/outputs/ps5-mod-bridge/native-runtime-startup/bootstrap
VEND=$W/outputs/2026-10-04-zcode-team/worker-installer-ps5io/vendored
VEND7=$W/outputs/2026-10-04-zcode-team/worker-7-installer/vendored
VBOOT=$HERE/../vendored/bootstrap
DIFF=$W/outputs/2026-10-04-zcode-team/worker-runtime-ack/runtime-ack.diff
OUT=$HERE/out

mkdir -p "$OUT"
rm -rf "$OUT/drift"

# 1. Drift gate: current sources + diff == vendored tree.
STAGE=$OUT/drift/outputs/ps5-mod-bridge/native-runtime-startup
mkdir -p "$STAGE"
cp -R "$BOOT" "$STAGE/bootstrap"
( cd "$OUT/drift" && patch -p1 -s < "$DIFF" )
ok=1
for f in runtime_owner.hpp runtime_owner.cpp ue4ss_backend.hpp ue4ss_backend.cpp \
         activated_mod_source.hpp activated_mod_source.cpp README.md; do
  cmp -s "$STAGE/bootstrap/$f" "$VBOOT/$f" || { echo "DRIFT: $f"; ok=0; }
done
diff -rq "$BOOT/tests" "$VBOOT/tests" >/dev/null || { echo "DRIFT: tests/"; ok=0; }
[ $ok -eq 1 ] || { echo "FAIL drift gate: vendored tree diverged from current sources + diff"; exit 1; }
echo "DRIFT GATE OK: current bootstrap + runtime-ack.diff == vendored tree (8 files + tests)"

# 2. Vendored C component (mfa_status_*).
/usr/bin/clang -c -std=c99 -O2 -Wall -Wextra -I"$VEND" -I"$VEND7" \
    "$VEND/modforge_activation.c" -o "$OUT/modforge_activation.o"

CXX=(/usr/bin/clang++ -std=c++20 -O2 -Wall -Wextra
     -I"$VBOOT" -I"$HERE/stubs" -I"$VEND" -I"$VEND7" -I"$BOOT")
SRCS=("$VBOOT/ue4ss_backend.cpp" "$VBOOT/runtime_owner.cpp" "$BOOT/activated_mod_source.cpp" "$OUT/modforge_activation.o")

# 3. Original T1-T8 ack suite against the vendored tree.
"${CXX[@]}" "$HERE/backend_ack_test.cpp" "${SRCS[@]}" -o "$OUT/backend_ack_test"
"$OUT/backend_ack_test"

# 4. Adversarial pass.
"${CXX[@]}" "$HERE/adversarial_ack_test.cpp" "${SRCS[@]}" -o "$OUT/adversarial_ack_test"
"$OUT/adversarial_ack_test"

# 5. Regression: the UNCHANGED original owner test against the patched owner.
"${CXX[@]}" "$VBOOT/tests/runtime_owner_test.cpp" \
    "$VBOOT/runtime_owner.cpp" "$OUT/modforge_activation.o" \
    -o "$OUT/runtime_owner_regression"
"$OUT/runtime_owner_regression"

# 6. Sanitized variants.
SAN=(/usr/bin/clang++ -std=c++20 -O1 -g -fsanitize=address,undefined)
SANINC=(-I"$VBOOT" -I"$HERE/stubs" -I"$VEND" -I"$VEND7" -I"$BOOT")
"${SAN[@]}" "${SANINC[@]}" "$HERE/backend_ack_test.cpp" "${SRCS[@]}" -o "$OUT/backend_ack_test_asan"
"$OUT/backend_ack_test_asan" > /dev/null && echo "PASS sanitized ack test (T1-T8)"
"${SAN[@]}" "${SANINC[@]}" "$HERE/adversarial_ack_test.cpp" "${SRCS[@]}" -o "$OUT/adversarial_ack_test_asan"
"$OUT/adversarial_ack_test_asan" > /dev/null && echo "PASS sanitized adversarial pass (A1-A9)"

echo "ALL HOST SUITES PASSED (drift gate, T1-T8, A1-A9, owner regression, ASan/UBSan)"
