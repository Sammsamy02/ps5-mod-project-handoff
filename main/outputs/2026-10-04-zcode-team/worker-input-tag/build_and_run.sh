#!/bin/zsh
# worker-input-tag host-test build+run receipt generator.
# Same recipe as worker-pad-coexist host-1-coexist/compile/command.json, with
# the two STAGED TUs swapped in for the coexist ones (the only delta).
set -euo pipefail
ROOT=/WORKSPACE/.codex/worktrees/translation-core/what
SPIKE=$ROOT/.superpowers/native-dependency-spike
TEAM=$ROOT/outputs/2026-10-04-zcode-team
ME=$TEAM/worker-input-tag
OUT=$ME/host-test/compile
mkdir -p "$OUT"

ARGV=(
/usr/bin/clang++
-I "$SPIKE/ue4ss-native-pad-config-1/src"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/first/include"
-I "$SPIKE/ue4ss-native-pad-provider-1/port"
-I "$SPIKE/ue4ss-native-input-port-1/port/first/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/ue4ss/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/first/include"
-std=c++23
-fms-extensions
-O0
-fno-lto
-fPIC
-ferror-limit=8
-Wno-unknown-pragmas
-Wno-unused-parameter
-Wno-unused-variable
-DRC_UNREAL_BUILD_STATIC
-DRC_FILE_BUILD_STATIC
-DRC_DYNAMIC_OUTPUT_BUILD_STATIC
-DRC_HELPERS_BUILD_STATIC
-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC
-DRC_ASM_HELPER_BUILD_STATIC
-DZYDIS_STATIC_BUILD
-DZYCORE_STATIC_BUILD
-DASMJIT_STATIC
-DASMTK_STATIC
-D_LIBCPP_ENABLE_EXPERIMENTAL
-I "$SPIKE/ue4ss-polyhook-platform-port-1/patched/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/first/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers"
-I "$SPIKE/ue4ss-real-lua-link-route-1/source-dependencies/imgui-textedit"
-I "$SPIKE/ue4ss-real-lua-link-route-1/source-dependencies/imgui"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/generated_include"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/unreal/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/unreal/generated_include"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/unreal/include/Unreal"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/unreal/include/Unreal/Core"
-I "$SPIKE/ue4ss-unreal-native-build-1/port/fmt/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/fmt/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/polyhook"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/asmjit/src"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/asmtk/src"
-I "$SPIKE/ue4ss-virtual-dispatch-dependencies-1"
-I "$SPIKE/ue4ss-virtual-dispatch-dependencies-1/zydis/include"
-I "$SPIKE/ue4ss-virtual-dispatch-dependencies-1/zycore/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ASMHelper/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ArgsParser/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Constructs/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/File/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Function/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Input/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/JSON/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/MProgram/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Profiler/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ScopedTimer/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include"
-I "$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include"
-DUE4SS_LIB_VERSION_MAJOR=3
-DUE4SS_LIB_VERSION_MINOR=0
-DUE4SS_LIB_VERSION_HOTFIX=1
-DUE4SS_LIB_VERSION_PRERELEASE=0
-DUE4SS_LIB_VERSION_BETA=0
-DRC_UE4SS_EXPORTS
-DUE_BUILD_SHIPPING=1
-DOVERRIDE_PLATFORM_HEADER_NAME=Prospero
-DHAS_INPUT
-DRC_INPUT_BUILD_STATIC
-ffunction-sections
-fdata-sections
-DFORCE_U16
-pthread
-fsanitize=address,undefined
-fno-omit-frame-pointer
"$SPIKE/ue4ss-unreal-native-build-1/port/first/src/Input/KeyDef.cpp"
"$SPIKE/ue4ss-unreal-native-build-1/port/ue4ss/src/Platform/NativeModSource.cpp"
"$SPIKE/ue4ss-unreal-native-build-1/port/first/src/DynamicOutput/Output.cpp"
"$SPIKE/ue4ss-unreal-native-build-1/port/first/src/DynamicOutput/OutputDevice.cpp"
"$SPIKE/ue4ss-unreal-native-build-1/port/first/src/DynamicOutput/WriterDevice.cpp"
"$SPIKE/ue4ss-unreal-native-build-1/port/first/src/Helpers/Time.cpp"
"$SPIKE/ue4ss-unreal-native-build-1/source-dependencies/fmt/src/format.cc"
"$ME/host-test/host_input_tag.cpp"
"$SPIKE/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp"
"$ME/staging/ue4ss-native-pad-provider-input-tag/port/PS5PadSource.cpp"
"$ME/staging/input-handler-input-tag/Handler.cpp"
"$TEAM/worker-pad-coexist/staging/native-pad-input-coexist/NativePadInput.cpp"
-o "$ME/host-test/host_input_tag"
)

python3 - "$OUT/command.json" "${ARGV[@]}" <<'PY'
import json,sys
open(sys.argv[1],"w").write(json.dumps({"argv":sys.argv[2:]},indent=1))
PY

set +e
"${ARGV[@]}" >"$OUT/stdout.txt" 2>"$OUT/stderr.txt"
rc=$?
set -e
echo "compile exit: $rc" | tee "$OUT/exit.txt"
exit $rc
