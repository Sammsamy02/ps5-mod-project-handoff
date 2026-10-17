#!/bin/zsh
# Build+run host tests for the rung-5 input bridge (worker-real-input, 2026-10-04).
# Compiles ONLY files in this vendored/ dir plus read-only retained sources
# (KeyDef.cpp for ModifierKeys operators); objects/binaries stay in this dir.
# Pinned toolchain per team convention (worker-3 CLAIM); the downloaded LLVM
# distribution carries no C++ standard library, so host builds use Apple clang++
# with the macOS SDK (host-only; no source outside this dir is produced).
set -euo pipefail
HERE="${0:A:h}"
WS="${HERE:A:h:h:h:h}"          # .../outputs/2026-10-04-zcode-team -> what
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
  "$HERE/PadEdgeSource.cpp" \
  "$HERE/KeybindDispatchGate.cpp" \
  "$HERE/keydef_modifier_ops.cpp" \
  "$PROVIDER/PS5PadSource.cpp" \
  "$PORT/src/Input/Handler.cpp" \
  "$HERE/host_test.cpp" \
  -o "$HERE/build/pad_keybind_dispatch_host_test"
"$HERE/build/pad_keybind_dispatch_host_test"
