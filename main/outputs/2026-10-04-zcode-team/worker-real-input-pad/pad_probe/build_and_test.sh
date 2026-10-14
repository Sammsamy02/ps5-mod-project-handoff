#!/bin/zsh
# Build+run host tests for the standalone pad probe (worker-real-input-pad, 2026-10-04).
# Compiles ONLY files in this pad_probe/ dir plus read-only retained sources
# (PS5PadSource.cpp, Handler.cpp — the same objects the composed runtime links).
# Objects/binaries stay in this dir. Same host toolchain convention as
# worker-real-input/vendored/build_and_test.sh: the pinned LLVM distribution
# carries no C++ standard library, so host builds use Apple clang++ + macOS SDK.
set -euo pipefail
HERE="${0:A:h}"
WS="${HERE:A:h:h:h:h}"           # .../pad_probe -> worker-real-input-pad -> team -> outputs -> what
CXX="/usr/bin/clang++"
SDK="$(xcrun --sdk macosx --show-sdk-path)"
PROVIDER="$WS/.superpowers/native-dependency-spike/ue4ss-native-pad-provider-1/port"
PORT="$WS/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first"
# Retained shim satisfying KeyDef.hpp's <String/StringType.hpp> on host
# (same shim family the retained cppmods tests compile against).
SHIMS="$WS/.superpowers/native-dependency-spike/native-retained-cppmods-1/tests/shims"

mkdir -p "$HERE/build"
"$CXX" -std=c++20 -O1 -g -fsanitize=address,undefined -isysroot "$SDK" \
  -DHAS_INPUT -DRC_INPUT_BUILD_STATIC \
  -I"$HERE" -I"$PROVIDER" -I"$PORT/include" -I"$SHIMS" \
  "$HERE/PadProbe.cpp" \
  "$HERE/pad_probe_main.cpp" \
  "$HERE/keydef_modifier_ops.cpp" \
  "$PROVIDER/PS5PadSource.cpp" \
  "$PORT/src/Input/Handler.cpp" \
  "$HERE/host_test.cpp" \
  -o "$HERE/build/pad_probe_host_test"
"$HERE/build/pad_probe_host_test"
