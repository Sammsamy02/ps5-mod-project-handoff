# PROGRAM-UNIT-INVENTORY — composition-6 replace of retained unit `program`

Worker: worker-program-unit-inventory · 2026-10-04 · read-only analysis + rehearsal compiles, all writes in this dir.

Purpose: per worker-configured-roots-design §3.2, the composition-6 replace-receipt must pin "every port file
the fresh program.d dependency walk touches". This document is the exact input inventory the comp6-driver
consumes: the attested compile argv, the full fresh-dependency walk with per-file sha256, the
control-vs-patched delta, and the pin list.

## 1. Provenance chain (all verified before any compile)

| Item | Path (under workspace `/WORKSPACE/.codex/worktrees/translation-core/what/`) | sha256 |
|---|---|---|
| run-3 receipt | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-4/run-3/receipt.json` | (source of all pins below) |
| program unit source (frozen, on disk) | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp` | `dfdd5014a0bf74663152fa74a7f5d2b245517252788e6bc30225cdc71e3ee907` = receipt pin, VERIFIED |
| program unit object (historical) | `.superpowers/native-dependency-spike/native-coherent-runtime-1/coherent-full-1/program.o` | `cae3279b7b3ef63d50f4e01e931b3b24ad6b5ab77fee078c7851f33392df064a` = receipt pin, VERIFIED |
| patched UE4SSProgram.cpp (controller) | `outputs/2026-10-04-zcode-controller/comp6-integration/UE4SSProgram.cpp` | `75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574` |
| frozen Platform/NativeProgramContext.hpp | `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp` | `664996150d3fec6df2471b1bf8c0cb231cf9f7a54d17a9d52595cc60d08b1868` = frozen-source-hashes.json, VERIFIED |
| patched Platform-NativeProgramContext.hpp (controller) | `outputs/2026-10-04-zcode-controller/comp6-integration/Platform-NativeProgramContext.hpp` | `0a068bb9d9b25f9b7213d584dabf26cadc793c42c93a0bb125f37d83c0a58f48` |
| historical program.d | `.superpowers/native-dependency-spike/native-coherent-runtime-1/coherent-full-1/program.d` | 1328 deps (baseline) |

The controller patch is exactly two hunks in UE4SSProgram.cpp (PROSPERO branches only) plus the extended
`Platform/NativeProgramContext.hpp` (adds `configured_game_directory` / `configured_working_directory`,
rewrites `ValidateNativeProgramContext` lexical gates). No other source or header is touched by comp6 for
this unit.

## 2. Exact compile argv of unit `program` (composition-4 run-3 receipt, verbatim, 176 elements, in order)

- Toolchain argv[0]: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
- Env per packaging receipts/composition scripts: `LLVM_CONFIG=outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`
- Machine-readable authoritative copy: `program-argv.json` (this dir).
- Include dirs (each preceded by `-I`, in argv order; first dir repeats at argv indexes 2, 46, 54, 64):

1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include`
1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include`
1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include`
1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include`
1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal`
1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core`
1. `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-settings-defaults-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include/Unreal`
1. `.superpowers/native-dependency-spike/ue4ss-native-mapped-collector-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-mapped-image-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-read-lease-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-ps-scan-abi-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-instruction-owned-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-owned-detour-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-native-bounded-owned-1/include`
1. `.superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/patched/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers`
1. `.superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui-textedit`
1. `.superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/generated_include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/fmt/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/polyhook`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/asmjit/src`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/asmtk/src`
1. `.superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1`
1. `.superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include`
1. `.superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ASMHelper/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ArgsParser/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Constructs/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/File/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Function/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Input/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/JSON/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/MProgram/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Profiler/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ScopedTimer/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include`
1. `.superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include`

- Defines, in order: `-DRC_LUA_MADE_SIMPLE_BUILD_STATIC` `-DRC_INI_PARSER_BUILD_STATIC` `-DRC_PARSER_BASE_BUILD_STATIC` `-DRC_JSON_BUILD_STATIC` `-DUE4SS_LIB_BETA_STARTED=0` `-DUE4SS_LIB_IS_BETA=0` `-DUE4SS_LIB_BUILD_GITSHA="e3ba1016562d6c0868c410d0a71e88bfcdbf691b"` `-DUE4SS_CONFIGURATION="Native experiment"` `-DHAS_INPUT` `-DRC_INPUT_BUILD_STATIC` `-DRC_UNREAL_BUILD_STATIC` `-DRC_FILE_BUILD_STATIC` `-DRC_DYNAMIC_OUTPUT_BUILD_STATIC` `-DRC_HELPERS_BUILD_STATIC` `-DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC` `-DRC_ASM_HELPER_BUILD_STATIC` `-DZYDIS_STATIC_BUILD` `-DZYCORE_STATIC_BUILD` `-DASMJIT_STATIC` `-DASMTK_STATIC` `-D_LIBCPP_ENABLE_EXPERIMENTAL` `-DUE4SS_LIB_VERSION_MAJOR=3` `-DUE4SS_LIB_VERSION_MINOR=0` `-DUE4SS_LIB_VERSION_HOTFIX=1` `-DUE4SS_LIB_VERSION_PRERELEASE=0` `-DUE4SS_LIB_VERSION_BETA=0` `-DRC_UE4SS_EXPORTS` `-DUE_BUILD_SHIPPING=1` `-DOVERRIDE_PLATFORM_HEADER_NAME=Prospero`
- Flags, in order: `-std=c++23` `-fms-extensions` `-O0` `-fno-lto` `-fPIC` `-ferror-limit=8` `-Wno-unknown-pragmas` `-Wno-unused-parameter` `-Wno-unused-variable`
- Tail: `<source>` `-c` `-o` `<object>` `-MD` `-MF` `<dep-file>`; original source argv[169] = `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp`; original `-o` = `…/native-coherent-runtime-1/coherent-full-1/program.o`; original `-MF` = `…/coherent-full-1/program.d`.

Full argv, every element in original order (workspace prefix `…/what/` elided per line for width; JSON copy `program-argv.json` is byte-authoritative):

```
  0 outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++
  1 -I
  2 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include
  3 -I
  4 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include
  5 -I
  6 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include
  7 -I
  8 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include
  9 -I
 10 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include
 11 -I
 12 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal
 13 -I
 14 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core
 15 -I
 16 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include
 17 -I
 18 .superpowers/native-dependency-spike/ue4ss-native-layout-binding-1/include
 19 -I
 20 .superpowers/native-dependency-spike/ue4ss-native-layout-parser-1/include
 21 -I
 22 .superpowers/native-dependency-spike/ue4ss-native-layout-widths-1/include
 23 -I
 24 .superpowers/native-dependency-spike/ue4ss-native-settings-defaults-1/include
 25 -I
 26 .superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include
 27 -I
 28 .superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include/Unreal
 29 -I
 30 .superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include
 31 -I
 32 .superpowers/native-dependency-spike/ue4ss-native-mapped-collector-1/include
 33 -I
 34 .superpowers/native-dependency-spike/ue4ss-native-mapped-image-1/include
 35 -I
 36 .superpowers/native-dependency-spike/ue4ss-native-read-lease-1/include
 37 -I
 38 .superpowers/native-dependency-spike/ue4ss-native-ps-scan-abi-1/include
 39 -I
 40 .superpowers/native-dependency-spike/ue4ss-native-instruction-owned-1/include
 41 -I
 42 .superpowers/native-dependency-spike/ue4ss-native-owned-detour-1/include
 43 -I
 44 .superpowers/native-dependency-spike/ue4ss-native-bounded-owned-1/include
 45 -I
 46 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include
 47 -I
 48 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include
 49 -DRC_LUA_MADE_SIMPLE_BUILD_STATIC
 50 -DRC_INI_PARSER_BUILD_STATIC
 51 -DRC_PARSER_BASE_BUILD_STATIC
 52 -DRC_JSON_BUILD_STATIC
 53 -I
 54 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include
 55 -I
 56 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include
 57 -DUE4SS_LIB_BETA_STARTED=0
 58 -DUE4SS_LIB_IS_BETA=0
 59 -DUE4SS_LIB_BUILD_GITSHA="e3ba1016562d6c0868c410d0a71e88bfcdbf691b"
 60 -DUE4SS_CONFIGURATION="Native experiment"
 61 -DHAS_INPUT
 62 -DRC_INPUT_BUILD_STATIC
 63 -I
 64 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include
 65 -I
 66 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include
 67 -std=c++23
 68 -fms-extensions
 69 -O0
 70 -fno-lto
 71 -fPIC
 72 -ferror-limit=8
 73 -Wno-unknown-pragmas
 74 -Wno-unused-parameter
 75 -Wno-unused-variable
 76 -DRC_UNREAL_BUILD_STATIC
 77 -DRC_FILE_BUILD_STATIC
 78 -DRC_DYNAMIC_OUTPUT_BUILD_STATIC
 79 -DRC_HELPERS_BUILD_STATIC
 80 -DRC_SINGLE_PASS_SIG_SCANNER_BUILD_STATIC
 81 -DRC_ASM_HELPER_BUILD_STATIC
 82 -DZYDIS_STATIC_BUILD
 83 -DZYCORE_STATIC_BUILD
 84 -DASMJIT_STATIC
 85 -DASMTK_STATIC
 86 -D_LIBCPP_ENABLE_EXPERIMENTAL
 87 -I
 88 .superpowers/native-dependency-spike/ue4ss-polyhook-platform-port-1/patched/include
 89 -I
 90 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include
 91 -I
 92 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers
 93 -I
 94 .superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui-textedit
 95 -I
 96 .superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui
 97 -I
 98 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include
 99 -I
100 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/generated_include
101 -I
102 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include
103 -I
104 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include
105 -I
106 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal
107 -I
108 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core
109 -I
110 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include
111 -I
112 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/fmt/include
113 -I
114 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/polyhook
115 -I
116 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/asmjit/src
117 -I
118 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/asmtk/src
119 -I
120 .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1
121 -I
122 .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include
123 -I
124 .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include
125 -I
126 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ASMHelper/include
127 -I
128 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ArgsParser/include
129 -I
130 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Constructs/include
131 -I
132 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include
133 -I
134 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/File/include
135 -I
136 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Function/include
137 -I
138 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include
139 -I
140 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include
141 -I
142 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Input/include
143 -I
144 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/JSON/include
145 -I
146 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple/include
147 -I
148 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include
149 -I
150 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/MProgram/include
151 -I
152 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include
153 -I
154 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Profiler/include
155 -I
156 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ScopedTimer/include
157 -I
158 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include
159 -I
160 .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include
161 -DUE4SS_LIB_VERSION_MAJOR=3
162 -DUE4SS_LIB_VERSION_MINOR=0
163 -DUE4SS_LIB_VERSION_HOTFIX=1
164 -DUE4SS_LIB_VERSION_PRERELEASE=0
165 -DUE4SS_LIB_VERSION_BETA=0
166 -DRC_UE4SS_EXPORTS
167 -DUE_BUILD_SHIPPING=1
168 -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero
169 .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp
170 -c
171 -o
172 .superpowers/native-dependency-spike/native-coherent-runtime-1/coherent-full-1/program.o
173 -MD
174 -MF
175 .superpowers/native-dependency-spike/native-coherent-runtime-1/coherent-full-1/program.d
```

## 3. Rehearsal compiles (this dir only; frozen tree untouched)

Driver: `rehearse_compile.py`. Variants — only `argv[169]` (source), `argv[172]` (`-o`), `argv[175]` (`-MF`)
redirected; `patched-staged` additionally substitutes the FIRST `-I` dir (present at argv indexes 2, 46, 54,
64) with `staged/ue4ss-include/` — a symlink shadow of the frozen `source/port/ue4ss/include` tree where the
ONLY real file is `Platform/NativeProgramContext.hpp` = the controller's new header. Everything else in the
argv is byte-identical to the receipt.

| Variant | Source | Includes | Exit | Result |
|---|---|---|---|---|
| `control` | frozen UE4SSProgram.cpp | frozen (argv untouched) | 0 | `control.o` 3,881,064 B, sha256 `cae3279b7b3ef63d50f4e01e931b3b24ad6b5ab77fee078c7851f33392df064a` — BIT-IDENTICAL to the historical run-3 object pin |
| `patched-literal` | controller UE4SSProgram.cpp | frozen (argv untouched) | 1 | FAILED exactly as designed: 3 errors, all `no member named 'configured_game_directory' in 'RC::Platform::NativeProgramContext'` (patched lines 433, 775, 776); 5 warnings. Proves the driver MUST stage the new header before the fresh compile (see `patched-literal.stderr`) |
| `patched-staged` | controller UE4SSProgram.cpp | staged shadow | 0 | `patched-staged.o` 3,883,600 B, sha256 `135392038b21a9d8af6221480ba4a0099a325227ca2dae3d23a711bf715a4a24`; 6 pre-existing-style warnings, 0 errors |

Control determinism cross-checks:
- `control.o` == historical `program.o` byte-for-byte (same sha256, same 3,881,064 bytes).
- `control.d` dependency set == historical `program.d` dependency set: 1328 == 1328, zero difference.

## 4. Fresh .d dependency walk (patched) vs control: IDENTICAL FILE SETS

- `control.d` 1328 deps, `patched-staged.d` 1328 deps.
- Raw literal path diff: 11 ↔ 11 — exactly the 11 headers that resolve from the first `-I` dir
  (`source/port/ue4ss/include`), which in the rehearsal resolve into the staged shadow instead of the frozen
  tree. 10 of the 11 are symlinks (realpath-identical to the frozen files); 1 is the copied new
  `Platform/NativeProgramContext.hpp`. After realpath normalization the sets are identical except that single
  path-of-staging artifact: in the driver's real run (files staged at canonical port paths) the fresh `.d`
  file set is IDENTICAL to control's. No dependency is added or removed by the patch.

Dependency tree breakdown (1328 files): 394 under `.superpowers/…` (port + vendored spikes), 934 under
`outputs/ps5-mod-bridge/…` (PS5 SDK: target/include/c++/v1 libc++, clang x86 headers, target/include).

## 5. Object delta control vs patched (nm / objdump summary; raw evidence `*.nm`, `*.dis`, `*.strings`)

Symbol tables (llvm-nm -C): control 7889, patched 7892 lines. The diff is dominated by renumbered local data
labels (`.L.str.NNN`, `.LCPI…`, `GCC_except_tableNNNN`) — a numbering cascade from the new literals/labels,
NOT new code. Semantic delta after excluding renumbered labels:

Removed (only in control, 2): `std::initializer_list<path const*>::begin()/.end()` — emitted for the old
`for (const auto* path : {&module_file_path, &game_executable_path})` loop that the header
`ValidateNativeProgramContext` rewrite replaced.

Added (only in patched, 1): `RC::Platform::ValidateNativeProgramContext(...)::'lambda'(path const&)::operator()`
— the new `lexical_path_valid` lambda from the staged header.

Functions with size delta (6, llvm-nm -S):
| Function | control → patched | delta | attribution |
|---|---|---|---|
| `RC::Platform::ValidateNativeProgramContext(NativeProgramContext const&)` | 1218 → 766 | −452 | header hunk: rewritten validation (lambda + conditional gates) |
| `RC::UE4SSProgram::setup_paths(path const&)` | 1881 → 2045 | +164 | hunk 2: configured-roots directory derivation |
| `RC::UE4SSProgram::UE4SSProgram(NativeProgramContext, initializer_list<BinaryOptions>)` | 6253 → 6409 | +156 | hunk 1: unobserved-executable logging branch (ctor body) |
| `RC::Platform::NativeProgramContext::NativeProgramContext(NativeProgramContext&&)` | 181 → 235 | +54 | two new `std::filesystem::path` members |
| `RC::Platform::NativeProgramContext::~NativeProgramContext()` | 112 → 141 | +29 | same |
| `RC::UE4SSProgram::init()` | 1792 → 1795 | +3 | knock-on from changed inline validation/logging |

All other sized functions are byte-size identical. String-pool delta: exactly two new text literals
(`"Native program requires an observed game executable path or an explicit configured game directory"`,
`"Native configured roots must be absolute normalized directory paths without NUL"`) plus the label cascade.
No unexpected symbol, no missing symbol. The delta is fully attributable to the two source hunks + the one
header hunk. Object size delta: +2536 bytes.

## 6. Frozen-source-hashes comparison (patched-header tree)

Full per-file sha256 manifest: `dependency-manifest.json` (1328 entries; complete table in §8).
`frozen-source-hashes.json` covers 2920 port-tree files; the walk covers 309 of them.

- EXPECTED diffs (2, and only 2 — both staged contents the driver replaces):
  1. `source/port/ue4ss/src/UE4SSProgram.cpp`: frozen/on-disk `dfdd5014…` → staged content `75abf2ab…`
  2. `source/port/ue4ss/include/Platform/NativeProgramContext.hpp`: frozen/on-disk `66499615…` → staged content `0a068bb9…`
- UNEXPECTED diffs: **0**. Every other frozen-covered file in the walk matches its frozen hash on disk today.
- Not covered by frozen-source-hashes.json: 1018 (934 PS5-SDK files under `outputs/ps5-mod-bridge/`, 49 under
  `ue4ss-unreal-native-build-1`, 31 under `ue4ss-virtual-dispatch-dependencies-1`, 3 under
  `ue4ss-real-lua-link-route-1`, 1 under `ue4ss-native-layout-parser-1`) — the frozen file pins the port
  generation, not the SDK/vendor spikes; those are pinned by their own historical receipts.

## 7. Replace-receipt pin list for the comp6-driver (§3.2 contract)

Per §3.2.1–2 the driver pre-seeds `pins` and builds `rr` BEFORE the units loop, then requires fresh-`.d`
coverage by `pins ∪ rr`. The exact pins this inventory attests:

**rr entries (replaced retained unit `program`; staged contents, distinct from frozen hashes):**

| canonical port path | staged-content sha256 | frozen sha256 (superseded) |
|---|---|---|
| `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/UE4SSProgram.cpp` | `75abf2ab77791f4e0aa2c068164d95d7737aefdcf0536a791f40599cd4ffb574` | `dfdd5014a0bf74663152fa74a7f5d2b245517252788e6bc30225cdc71e3ee907` |
| `.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp` | `0a068bb9d9b25f9b7213d584dabf26cadc793c42c93a0bb125f37d83c0a58f48` | `664996150d3fec6df2471b1bf8c0cb231cf9f7a54d17a9d52595cc60d08b1868` |

**pins (every remaining fresh-.d dependency, on-disk sha256 verified this run):** all 1326 files other than
the two rr paths above — complete table in §8 and machine-readable `dependency-manifest.json`
(`path`, `category`, `sha256`, `effective_sha256`, `frozen_sha256`, `status`; `status=match` for the 309
frozen-covered entries, `status=not-in-frozen` for the 1018 SDK/vendor files). Gate equivalence is
demonstrated by rehearsal: the control replay is bit-exact, so any pin mismatch in the real run is genuine
staging drift, not argv drift.

## 8. Complete dependency file list with per-file sha256 (1328 entries)

Status annotations: `not-in-frozen` = absent from frozen-source-hashes.json (SDK/vendor); `staged:` = the
effective sha256 the driver must pin after staging (rr content).
- `199b6ae1b691507bf8a82b7e0f1a9988bc9323e644dbf0ffd13a9e717e5f8bdc` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/DynamicOutput/FileDevice.hpp
- `42438eff82a92b4a20bebea167f2f794da839f7565f88721e02f4081c04674cf` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/ErrorObject.hpp
- `7ed91983574ebb111a796bbe7b734896ec20d85cf3bfbca2bb43fbc339b3e93a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/Common.hpp
- `6a1b0ed9c7eaf58078e5d2bd81914bfd242f9bab1ffe73656c83216f477b24db` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/Enums.hpp
- `a21c6e555e82ccd338741b95e2acfc95b7b616609756d83fca023baa1e9536bd` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/Exceptions.hpp
- `b207c162b1835055386423af60565d9737871e500c4a45839e7f5233a7fd9b61` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/File.hpp
- `b3635de24a56e7c402499d94a9b456613d58f67bd3f348c91dfde781935d92f3` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/FileDef.hpp
- `d5b0f4f2801ced66bbe3b96a379fc6b25b1d9259b0bb8672a4997b0775dac9ab` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/FileType/FileBase.hpp
- `713ac6a2c4cbb50770d0d1e9f35fc29baf4d0c475e87750ea36468cf720ef7e4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/FileType/PosixFile.hpp
- `9b41c8791f727e99ef68ed532fdfd9462c14f5ba8f409fd303edb3d54867749b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/HandleTemplate.hpp
- `c49f3ea2b57fb411dc79ba08135271da22017d95626da102a917bb894d3e3b9e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/InternalFile.hpp
- `89a4bb25564b2661f536355bbe2fc4b85dfa2bc705858e40b460294a48a454e4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/File/Macros.hpp
- `7bbe4882aca745ad6ca09539aaf18b5a327ab2b80c5683e6977ff1b6a2db17ff` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Function/Function.hpp
- `b9249d0819f117c3a241f3c876e10dfc41d949a0c1537e84ed8dc665808df4ec` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/String.hpp
- `90c41a8545bb783af7aef82459fe4c49c1fb1657df02df9d6059b71d033bce45` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/UETargetModules.hpp
- `a77b4d397144af303496f50d1f34443be6ccdbba816f7835ab245fdea77bad84` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Helpers/Utf16Utf8.hpp
- `ad87ba28033d0579cd5d8d9ee947f879a7b79fcef7dfba14286a78e8b7ec9cf7` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Input/Common.hpp
- `da3c746856ad1db582bffa801678aef0babb0594988dce971912911b1a146442` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Input/Handler.hpp
- `61890df00f0974820d9d801520417f06056c9b4ba4d9e15976ab4a154a45afb7` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Input/KeyDef.hpp
- `dd70ba3232ebd2374bf533589b61b1c9d016a8088e59c86408d43cf2881554ec` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/Input/PlatformInputSource.hpp
- `19ba8e3475ce5ecc923d6177c06d349b878a5ba4b633f95ee4e348efba20314d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/LuaMadeSimple/LuaMadeSimple.hpp
- `802fc89354fac8d5f86fe7da419794c21cfdea38474635f96486ff20b4a6dc52` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/MProgram.hpp
- `ff0227f52094843f1a28889ce98d8411b0d2d069e5ccc08783724ea9e2cef790` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/ProgramFeatures/CanError.hpp
- `fd4c706eb980ae7c08efcf27a2d4051e09901b2a70fdca215e90b8c1170e1b8a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/first/include/SigScanner/SinglePassSigScanner.hpp
- `13c8d953fb2d98dc955289dd166999bd22dbabfeba7f486b3693c2c6d9d627db` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/base.h
- `aa4f1189f00ca287a9e5585cc3b6b57dfa8f1a000efafa881432f46eafbddaed` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/chrono.h
- `fbc34d42af762cd4e45ef2c93d050802d956d406c5c9e4c92ea9b0749fd02d8a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/color.h
- `b2b6692145b11c7c2774db106078c97575a47d18e80c7783c68566475325ebb4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/core.h
- `9ac34739eb99d5fe220c96d59519bf8da9fb0108d1cf5badbed157187e4c3cfb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/format.h
- `c49f07be57b7fb2abc89e34284574740c24c2de6d6df0b08aff1a0b8f537c090` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/ostream.h
- `4f6aa8150a1a87c60f0b16f586007634191a396061fa18639f7d65727a37d2f6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/ranges.h
- `d4aad2213ca59d6dac301570dbde00e0f0daa088bf8669070b0283de93f7b251` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/fmt/include/fmt/xchar.h
- `4ecad85aa88dd96b1294ccb6336b77dfde250f511e0b4220e9143748eb2e14ec` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/ExceptionHandling.hpp
- `6d158063b3c66e9ff22a7442d12a71b99fb7232f159dbf0bbb509b210a9aed5f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/GUI/LiveView.hpp
- `89cb7390085bfc12477195a3e3418cfdbe6537a4d31ef1836ba634718b401b82` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/LuaType/LuaUObject.hpp
- `f33e2eb80fb391a6ad238651e056db91596b136c3f38a1de52ebfa876ef1b8ed` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Mod/CppMod.hpp
- `e52d29a0b165a672ccc54232e1c8c12730eec3a5c80a7be0e124e53f0751e88c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Mod/LuaMod.hpp
- `97c762453a84dcc09b86d5d6ac598f6006dad727bad9cdcaa1f358de17e90487` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Mod/Mod.hpp
- `0ad1f9bb5a79a9548220d5e16811b196fcdac480f1ae837fc24acd158b94f4a1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/NativeLayoutBinding.hpp
- `2b44df14854934470a16fec6ef153501e08a8554ba51748b335fd0733856a450` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeModSource.hpp
- `664996150d3fec6df2471b1bf8c0cb231cf9f7a54d17a9d52595cc60d08b1868` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp DIFFERS staged:0a068bb9d9b25f9b7213d584dabf26cadc793c42c93a0bb125f37d83c0a58f48
- `a9b4abbe26f0cc1c9ec71029cb009e721dd88839395a72786dcbeda8f8b5aa2e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/SettingsManager.hpp
- `9529fe76aeab8946193b0fc39aa2416e96d41e9f383590ee926b8d618036d860` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/UE4SSProgram.hpp
- `0d02e7f412bf16107ee33e4922e20ad8fa3f23e93333d43ecdd02a25c092f100` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_AGameMode.hpp
- `84da767dd65e14f1944949b35060f3d17d1d931dd86bda774580c5e3cdf2ddec` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_AGameModeBase.hpp
- `4177a4fba0942b1f8822ecd2cf9640302cea0c332519aba7c5bce709cf69a347` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FArchive.hpp
- `98cd7b6a329fdd49a60915b0d87385dab535752c09be9dafd655b0c3e4e68dd3` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FArchiveState.hpp
- `199ef30f67a255a8d767ba04984ed0fe4955571b9304836da77f0f8f6427fae3` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FArrayProperty.hpp
- `701b2ceae1c6869cf8890d4d55b016b1323dc93d774ee7db91417f6acca82471` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FBoolProperty.hpp
- `adb8b99776e2a9d2767d8eb984abe0e866de261bf3e668717c4af86c6a514598` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FByteProperty.hpp
- `365c7a4e451c2b07ac7b8c1049cbaf1fc495e396609776a950db581e62043498` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FClassProperty.hpp
- `16261c437ce3fb7c37dab618a14f44072d9c9fc3c13a57cb65e53fd6d95d9f2d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FDelegateProperty.hpp
- `1c3c9f795451f87c7b9ca6352008b9af9c6e31927501f5fa781cb6828206de06` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FEnumProperty.hpp
- `310661bfaf4c9410b89ddae66994d6cc0ec364a7412c2d0b1ebf2bd1f9223501` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FField.hpp
- `e3191bfde5b911d0c12d3678febe2f2ab8b26368ca32d26399088714db6b96ec` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FFieldClass.hpp
- `c28551de704aba79b23b0d6c7aba3f48b6088afdd90e02344c924232083b6aa0` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FFieldPathProperty.hpp
- `107a7c181c6eceed2c6c10a0459fe613ac1a8a6842d86e92b59aef6bb1a87ba1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FInterfaceProperty.hpp
- `f190fbabf10b830e3872b5b8fe3434aedcaef25d3caeb849a8781330da258272` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FMapProperty.hpp
- `16261c437ce3fb7c37dab618a14f44072d9c9fc3c13a57cb65e53fd6d95d9f2d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FMulticastDelegateProperty.hpp
- `3c1c034c9a539d4f2e499a79a7b681bfc672bd9153191839464196abdc86a697` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FObjectPropertyBase.hpp
- `143cf98cd3944c5a8c58c413d9b96f945c5c23d7e9b0bfa5c6a2e19cfd45876e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FOutputDevice.hpp
- `2b24ed458e35b977bf635b1db89c57de2f3ecf2960e224249bc6b171d2e9e680` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FProperty.hpp
- `c5d1485a4043528b036d653865e6a3e868c83139a6ebbc57a1aba9daed869c43` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FSetProperty.hpp
- `365c7a4e451c2b07ac7b8c1049cbaf1fc495e396609776a950db581e62043498` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FSoftClassProperty.hpp
- `86059e2dc5860e2e11eca730953f91694232469514c1ba47b127cbd7f978940e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FStructProperty.hpp
- `0e1a3ebfc320c466116351c1f49976754d4682fc1d5122bff071cb40289761b7` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FUObjectArray.hpp
- `be259a1227367e8e179609cd08abaa44920e7848dd4080c22fb4fcc81097203b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FUObjectHashTables.hpp
- `cb39cfc615aef453a0ef4045bdb53cff66f5b044c9abce94c6301d6dbf49940a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FUObjectItem.hpp
- `bc7baf50417e643ed0a61b58e322718033816a7535f424952d321fecbc4721df` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_FWorldContext.hpp
- `27d3ec6ef6846a9a59abde7c47541efa226ae85f9fd4bd29d62ed879205618e7` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_TUObjectArray.hpp
- `db487fac9e69a6da95c2a0cfc674ea1e172f0f77832d79c2c9c26684dcee0758` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UClass.hpp
- `e30c9da25a81db3a7bb48f9d05ff920fd8cfe31dbd759282ba157db617db0fcb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UDataTable.hpp
- `55b7c169075ab36e2180ca987c74d0ed1ba9e176d33939b2eadac7f8df902a96` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UEnum.hpp
- `3dcd20463a88cd375600ad0202e97df3660c7cc7672c9b2af6844f96b0db393d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UField.hpp
- `b70b27d3ee332f6cf7b176dfc05066b2ec822653b99824c14622ff15d7311685` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UFunction.hpp
- `f9fafb93f0decb0d66e167328c11a74271649cb6eac43e1f19c2a0ced1299287` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_ULocalPlayer.hpp
- `1e7e677dd117bb0d90fe73d2fb3ffd8176c1c1917c98d97d023888bf699f3049` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UObjectBase.hpp
- `e075f3c324b01e8bff6afc67ca1db4231c78a7043cca17b7309cc9402e381fc4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UPlayer.hpp
- `5f43c1da86e234f7df196c8e0de140a3abab4730f60af53ce3e50647c293e1fc` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UScriptStruct.hpp
- `7270117aa6195cee6d0a39380773c63452c6a96df89a8275473c86094775043b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UScriptStruct__ICppStructOps.hpp
- `80fa65bd3a765406a985cc05ccca95e61e9cabbc624111d16ca3facd22291305` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_USparseDelegateFunction.hpp
- `60654b92387fd3988b8135073f6aaaff27e20ca8aa6143daa175b6ece0fb53ef` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UStruct.hpp
- `cfc87036fd3b139feab258f10bbf3e2e9320208a6fa0dca16bf32c672d846ccf` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/generated_include/MemberVariableLayout_HeaderWrapper_UWorld.hpp
- `4bc14264f37a37497653602799d2a47aecea060dc4f5c0e94ce142f784f42e95` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/AActor.hpp
- `8be73c2a80244ee3737b4b988ef79e1b3f9d4d046243a87e41fb8724875eb116` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/AGameMode.hpp
- `17f5021fdfa975d35722639800eea5d4947f42e8a0fcaf280bd1bc9f81296e2f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/AGameModeBase.hpp
- `3d466266a09db84da5b289f64868284d30f8a4715d174d3084e4b71b41ee3333` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/BitfieldProxy.hpp
- `1064ae21fb72980ff1bc3fad74ebb9b368feaa1d0d2ab4224995127c1e121892` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Common.hpp
- `5be97d92e55aaaca30b3a0e8a9335257f17c99d571c1d396aa290fa53e350a0c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/BinarySearch.hpp
- `ee8e39dc062e9141b7b5b5da8626ace54174de0fd608488f4af0693473e7937a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/Impl/BinaryHeap.hpp
- `6127fc29b22d77fca2f14cdda5a6bf8c2ff76ac08807a90863acc18e78005a3b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/IntroSort.hpp
- `a933e99074da44bd32c7b918759ce58aabe5f6c768d0b6a17b23f0b4c400574c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/Reverse.hpp
- `172b962a637450200a800c87412f3d2f72cf0583dca896afa2cc02cc9b8689f5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/Rotate.hpp
- `670d9a5e9640e17644999c7d39c1981cc61eda6907eafc2b69ee63c9f7cda861` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/Sort.hpp
- `4796228cbe60e4cc7878a1a9e40449825f23184f8d31623aa6ad8ae3cf211ba1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Algo/StableSort.hpp
- `c3314a6341c81814fba3d1f4f748898a3d8d9473d65f072028c130f56fe15c1f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Clang/ClangPlatform.hpp
- `18cb4758f75d33f4d2e8ba21579b9c7c4a803148897c353e57b2d0b234e8d8bc` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Clang/ClangPlatformCodeAnalysis.hpp
- `0180d985c9875473b2cfa0b20e0482bc7a91f2116d443430924448dfa1b919be` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Clang/ClangPlatformCompilerPreSetup.hpp
- `d86bc92aed5f6bdb5811f4e110f27c716ffb19ea031737c1ec2ded1080a983f2` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Concepts/FloatingPoint.hpp
- `b37d63b831dc9536fba3d5ab8866be732d62a09cd4799b8cab295210bb04cfc0` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Concepts/Integral.hpp
- `161d50bab020337995408e1dd595a8858e6bebdf4706c9e36a4b81426b6c49d5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Concepts/StaticClassProvider.hpp
- `10a7fe6dfd55155d76749446cd990a20af3a9a9b058bc0dba61ab44b816edc6d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Concepts/StaticStructProvider.hpp
- `a4df658893975f5c1de16f62d9437a47623b45d91b27d8f59092bed824fff34d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/AllowShrinking.hpp
- `7adae58f81215066e0697a13ed632cc65b3e09d82dc34d7d0077873b1a1bec51` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/Array.hpp
- `b4c4da16ce3c1ca80fc1393580a879558bdab872f5f0a05ec60f8437de8776ff` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/ArrayView.hpp
- `20ef49104e4c0894afcb34f8ee74d43560c099ed77d435cd75e60c5771616fae` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/BitArray.hpp
- `07203c544613c49649ebe9854fc10b9e02920112f4bb51a4402d383e9411763b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/ContainerAllocationPolicies.hpp
- `f3329501e8db5864285321a54a3c312d34683c06e6926fa913b77e895f80ed76` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/ContainerElementTypeCompatibility.hpp
- `9db9bd67907d08cafcb20e3489a4d1762370d6b89eb269db14aafbc7fe29679b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/ContainerHelpers.hpp
- `f2a3549ccbc93e23c1b523e52ae24c0e82709a9354fc0f494d98e7158cee2ebc` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/ContainersFwd.hpp
- `a8bca1f4ded472511657aa5cab93bf8e132a7bcfcd8ae7df045c295ce1a30070` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/EnumAsByte.hpp
- `56dd4cebb4e95c1bde5b5cb409bc09ef386d191cc2b39f855a41565f31bda80f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/FString.hpp
- `4840750bb7564b8ab755de2935aa527c52d957a2189cd843c586dd6412d5be47` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/LockFreeList.hpp
- `5425a6a6b577d41c601c9e9de996acf40f646d880dc3b473cf80c68d409d3068` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/Map.hpp
- `f36bdcdc52bde2971d7823c015c5a61664cb3ec582e1922c2e62aeff527d1fb5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/ScriptArray.hpp
- `1465de48c3a65d7d7ef6648fd6a13173aa4f2b1c87ed3164f31e4b0d0fb6df4e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/Set.hpp
- `c4f3d6fdb00a275e4ee643805c1307e2a6780183fd7b473dd605c8468bcc4eb6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/SparseArray.hpp
- `6b99bb0bd0add56adc394fb3cd0b1ed2302720746e8d2add94c2adce8dd0b484` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/StaticIndirectArray.hpp
- `6f74d4916ad3bb31c1687b5233fe9857bda6adec0acfeb4b4056d8547001d0f1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/StringFwd.hpp
- `328888aa4747600f231eaafd97310cef90ed599d4824c8d8e097af6344f240de` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/StringView.hpp
- `7ff7f3a5daa82adf5132fe6ef388c295ee7d70b9428c65df0e79ac9330af5536` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Containers/UnrealString.hpp
- `631f4218ac428b2faf85e4bf24b59ee9ded9cebcb1bd2e2609ff186d46f86371` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/CoreFwd.hpp
- `96bb926853fa6f860004008d669bcba95862f0df5db389adf7f886b8bebf72b9` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/CoreTypes.hpp
- `53efee71a5004de571977ade48480559db08369cb39cd2d292d9f75e8ae9f625` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Delegates/IntegerSequence.hpp
- `a604d2e6c421b9eddfbb1326f3eb752e0bd300c3c6494d8fdc69b13887cae4ea` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatform.hpp
- `2ed0f1c8a954a6e875af76a5829c638e3d206a854bc88fa639d963d30916836b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformAtomics.hpp
- `7963adfc2a8f5cd63536e2537260a2676407014e0ce931dfc623ecc08483569f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformCompilerPreSetup.hpp
- `2ac152b314a8d23b9c74f425624737ec5ead7330e50288ac9ab99d50d48056fb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformMath.hpp
- `02efa1ddc75a2c55389f9b249a8490fc8388d448ee4e6c2188b304f3f95f6efb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformMemory.hpp
- `b21201a38f4d638b160ca17967e521d17c79e9f9d6b47e5760a682b168a929b0` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformMisc.hpp
- `5633361aeed097ddb21883abebb9052dbd50c2c866e8fe9ebfc4a53746ec255f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformStricmp.hpp
- `10f1f9a95a5f49856bc5229d51446b9e34ce9f8cc0055f9ccea4936e7df7b837` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/GenericPlatform/GenericPlatformString.hpp
- `36fbefcce0052aad1976ed2800ff70b1591ee562b7fb57f933ffbf969ecb7fc0` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/MemoryBase.hpp
- `539b8323810193c2e08a05005984260fffa67c9b5bcb5ab8d87278596075888e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/Platform.hpp
- `e7b80ef067065d35e901eaf77a2e161e1cafd120a0c7ad05f81d7ed15f591447` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PlatformAtomics.hpp
- `dc980a968693421e2a7415095add77f94678e9d6664f738f9c8afdd1faf36ec1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PlatformCrt.hpp
- `5b5ea4c4f3bccb60e52a7bb501acbb004ff70b625bdc5c8ad9a83f3a216b0cbd` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PlatformMath.hpp
- `eef9332271d643fd12e23582a1dc965f43737525ddf154bf46f625e5509694cc` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PlatformMemory.hpp
- `fcb536b95bf4519c86f568b1607cd07d9fe0c02b03022e0a2da0453e6ee3b280` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PlatformMisc.hpp
- `d867eff7eecd651492763c5a8d07fee2eb9b28eee5f2f7fdab26b0cc0f22884f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PlatformString.hpp
- `e798ebff521138c2468c40848aa7dce98da1d6bc091b184150e46a973c9b9c80` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/PreprocessorHelpers.hpp
- `0d27aea0ad97d6a422a0f12d33059fd9e966783f59ed824499cf76224be91500` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/ThreadSafeCounter.hpp
- `1e4a017967213c646be0e037e8e39bcb31c2830bd95fd2bbd7d30cc1f10b49af` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/ThreadSafeCounter64.hpp
- `ca8835508badcb142c0d86e5ca31a7e419d399967747179929b8a74f6fafc645` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/HAL/UnrealMemory.hpp
- `c010a832cc886657b24bd831bb11d04efc2777834b54c805f17d647374ed3a88` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Math/MathFwd.hpp
- `812d866e318786a0ad0745025f9c4d89169f61625e5e0b567a2f2f772e951241` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Math/NumericLimits.hpp
- `48201e634c939cb8e20a7b6bfc418523bb4fd68cb2e42a91bce8b74e9f5ceaba` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Math/UnrealMathUtility.hpp
- `a99aa63a9684421a24d72a66f8496e903927171b9a619d7343c4e20390ba7700` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/AssertionMacros.hpp
- `ec4de3213c59803d0ff9d2afe3c09a397249cf8f96714413854f77efc43ba80e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/Build.hpp
- `4235a8a40743f318bfb9ac968ba07cf83b562a7281623520d81df05cc37b3c22` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/CString.hpp
- `c188eaa30d763888a16f7b3d77e1dbdb0b766c40422f5a768e18f58b84a34c9d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/Char.hpp
- `94d0d79529145a64ed01215f1b5210a08e91b97561ae2a73ba259f7846393b90` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/CompressionFlags.hpp
- `b6a7f58368594a2453b40d0b3b355076baf1bb58d632dda7769fae24333f8cda` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/CoreDefines.hpp
- `11e873e00e32a1461591d723685ed1512aaa0944ac972472401012d5d91fc327` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/CoreGlobals.hpp
- `8afb993658c1331b17feab7adde3ddb6399844378cf9d3a8fbd171be9bd9a908` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/CoreMiscDefines.hpp
- `0cfc718e82b3810e4114769f1d9b334d6465806675599a311dcd52e75881fa1e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/Crc.hpp
- `5ad3fd24fa20c6f997bcd3950176caad95d8973dcc532d20e793aaa64f97eabb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/EnumClassFlags.hpp
- `583571c759aeba019e2fb2058a4ad5f706152d428b0aab60feb98c0e80af6d86` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/IntrusiveUnsetOptionalState.hpp
- `bb633a8fa0941f4a58262ac47fc3a08729ea50407715abfc1c06aa9b562249cf` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/OptionalFwd.hpp
- `a846057aac3456d0424b2032ced38a778190a3d19f257768ea817bd146e80d83` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/ReverseIterate.hpp
- `e74243c463c36dfe46d02ac8119cafba0c0207cf2547e7dbedd2a14b6c1f5b6b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/StructBuilder.hpp
- `3086027f295955159ca7efb72e59122302fa5d9381ccf3ac4abb210962019eba` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Misc/VarArgs.hpp
- `9332c0b79bffb97095fe3d722f2ee1252cfc65106013ce99d4b484970f737da8` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatform.hpp
- `bace2b876ef820757b28702229d208f58e8ff86985c6b15e4fb8fbe2a24d9135` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformAtomics.hpp
- `ede21e377f23e5c4ad9ec0f4517fa118289056892b4171e904fa993b7d93987d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformCompilerPreSetup.hpp
- `6d6caaf0cd5c170cba2db000ad10197a923d57c7707ce4f63b229c84d3eaf437` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformCompilerSetup.hpp
- `4719a7113d3fcea1b047c2077edba6ea14cf656d7220175999f63bdc56ef0d18` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformMath.hpp
- `58fa3c4f8b86f611f61540c875c67b8a37f21a75e892db67b1c6069c8a99d8ed` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformMemory.hpp
- `b7a19e43ff1f5af4d28d23a5091f42775a7bfa1997a91d106f7674aa7e706275` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformMisc.hpp
- `8e3baa90fe4f6ff28fc02994607dd765592a08435b0fa45b60ca29413121556b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Prospero/ProsperoPlatformString.hpp
- `ddeb5aa4ce2bb167ef6f2982c8634394591af9f207b54efda1c58036d859fb5e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Serialization/MemoryImage.hpp
- `0b69e5a3ec73246a13997f9680533642836d1dfa7a64272150ef2caa41620e3c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Serialization/MemoryImageWriter.hpp
- `d05b6fff9b8319e552a74c489b550327f7d0fd070bca26b774a1c6956269ebd6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Serialization/MemoryLayout.hpp
- `e64a46cc3f8b9d0aa4f5e14f08899fb041ff476536d700ff6117b2bdddcb5770` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Serialization/StructuredArchive.hpp
- `2cad4845e7945a8aba6dfb560bf597f477ec257e8a3554d2223fc27c75c3d703` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/String/Find.hpp
- `3750238f9eea9c811b5ebb2dad70fb52902ed8544de484020b98ccca7a1a9d16` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/AlignmentTemplates.hpp
- `98ce74f531119ed2035366d8571d97e81dfc0f9773117846c1c25e65ff512111` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/AndOrNot.hpp
- `6a55c30e710f7ac991098dc12d7fc719a70362b6f1ff8c4e0e9a954b3c72100a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/AreTypesEqual.hpp
- `24036d150953ea8f68b72412e691c1cb00433533f850411b90b2af795d21cd36` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Atomic.hpp
- `842c701d0baf13162169f054e8f7e525619f38fd501da650f4f4e5898d403ea4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/ChooseClass.hpp
- `664db397f9bdf8a5c93abfc68f58bea49697065d65b5afe0a2b3b46e981322d5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/CopyQualifiersAndRefsFromTo.hpp
- `1418203f093f4342fcb048f0f0c3503cd69113da6f1458f7912c4070318c5909` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/CopyQualifiersFromTo.hpp
- `92af8a106fa98cdffa7b0e1b1adbb0876035fa61a71059e4348c850d58b1cecd` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Decay.hpp
- `1d64c429c555c15aea65ede0db29edeb94b753fac25c9fdfc3c573bf13a2d0da` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/EnableIf.hpp
- `7165e89e660f25670a44478bdb9cca660fc94ed3e2a62ef6daa80e45a9f0b426` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Function.hpp
- `242d9366130b349c404330272e763aa3ac11b80c6759c1276ca9a1b4a0c22a02` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Identity.hpp
- `ee5d12528bbf91c8ceafb049b1e8d2984df43e843ea7e065b2df89d034ef9158` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IdentityFunctor.hpp
- `e267dda00e5f0414ab21a0979da0970a2c8d581d44e2eb0ebfea51c01b052141` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Invoke.hpp
- `6a03b91020823c1afc91ca13d52965fa6d8035411819ec901adf832888e7fd0c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsAbstract.hpp
- `98c94128b2ca90c90cf83c7c4688f8b5084e388b2c9d6b63a9304b0a098a2e2b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsArithmetic.hpp
- `11d5d8d0d1846608bb2fb8df3f9c8e2172a177edf79e780cc735b460d85e8e02` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsArrayOrRefOfType.hpp
- `06e1bcc4191f750d2086e42e29af09d3d76b3a73e39657006a240c68a5b43d6f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsArrayOrRefOfTypeByPredicate.hpp
- `b4631c9f187258e4ba2608fc6cf309514c78fc21ecd6ec57df673d61d59d35fe` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsConstructible.hpp
- `d011158376896496686141fd49d1bf0f48188ccf6c3b7d7372a642ece21837e5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsEnum.hpp
- `3098a7989db318b8bebccca04ea1ae5672b7d0baf92264d6eac5e009f36e95c6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsEnumClass.hpp
- `7d127d72d1d9f713ef90388636087452f08c88a52f53437ee06bb0a52b8a766a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsFloatingPoint.hpp
- `639b3957fadecdfe329c5d5f7f19e663ab1ae7b9963a5db695a22d5c5e44e45c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsIntegral.hpp
- `ac7d1f205cdc01d75fbced9de2d3eac2d00cb51d5be0224dfbfd7593366229fc` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsInvocable.hpp
- `3ce02bbfe9c4f2b99ce3f12adb84278a2e451dbd85057b36c8752c5a2e856302` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsMemberPointer.hpp
- `ac86bdbda114a03d11f290d75bc43e88653cd6cc545120ffae75d1ad7254fc59` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsPODType.hpp
- `00ca2d5287a4c5d897f51d729d5dcba5aa4c71a00e4fec7f8e373c5fc25d3383` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsPointer.hpp
- `1d3b7bd29adb08de8ff31da5af2482cfa5536418192d179c36ecb74c477429b2` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsPolymorphic.hpp
- `28b352dc798f144b8825758670e01ec23aa49e53f2787ae243bfd4c08d14d145` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsSigned.hpp
- `4763bfac47c6fcaa933ce900ef84c11cca020c432f0174e9990275bf4fc92304` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsTrivial.hpp
- `aa9248f4dcbe31e65be19c00f2f7769eff1cfa60d1152692867b72fd8f80da3c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsTriviallyCopyAssignable.hpp
- `2c0d3b244243357badd94c7cbbe0d5fab596476bf552a845261006b37b50785d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsTriviallyCopyConstructible.hpp
- `93f5048dbc3f53e8c4e4b0bcc66197a51f0fb378acaa3572859be7d1906a9e5c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsTriviallyDestructible.hpp
- `51f20ce10e6651e76d1bbaf387b1b7b43c8176f494db17805368b26629d3f8b5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsUECoreType.hpp
- `85ec73dccaf042895c5fb3d50c5d8b3f00ee770f7eb02e7b97cbefd247128206` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/IsValidVariadicFunctionArg.hpp
- `a1aa421d07eef77ee2c905067b8483226bed84a2c97afbda56e7c9309673992a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Less.hpp
- `1599588d2fb4b7fc75d402eccf49620c613fd20834f0b5aa775c193929eba118` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/LosesQualifiersFromTo.hpp
- `3eea4df1ca779e17db25ee1f065dc2ac859bb61ce23d4d0b14e6b9a047d2dd50` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/MakeUnsigned.hpp
- `2e18d0b4ce7a0d329996d73460b7e1636cababc3b17beedb782888b4f9884294` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/MemoryOps.hpp
- `c91c8eee509df46a5c2b46583d7067b843beed49a79e96b5c4bf4fed3397242f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Models.hpp
- `48be1235df009d0e7504c4462ad8d5cf1deabb7839ce9886d738f61fc7f30589` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/RemoveReference.hpp
- `4513ee926cefd3900a1bec5c341d116b3c316c5a1e6f5f329f932bc51c4fdf58` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Requires.hpp
- `514b0a27b0a997cb442de04d7ac488c6831841097f5571fd45170ac10ae13cce` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/ResolveTypeAmbiguity.hpp
- `537999b71021e647838c1178937d61e2cd24366a3f4460362a5ff19b76db51bb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/RetainedRef.hpp
- `65f13fde003797dec8af9e43db0aca4cc75081c86e625f2e6d79b8ab1fd9ba8f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/ReversePredicate.hpp
- `41dd5244d6475eb02a9598fdb9d50ac6b22742d8b84b69647b1c05f1699f3c11` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Sorting.hpp
- `4e33a2b89f857d595a6c95c7e2b6c64a806577774c21c7936c4c5daab80c6e8e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/Tuple.hpp
- `4100bf192459a2050a5f6ef3402cf8d83026a151b7240d2700388ddff8a20ebb` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/TypeCompatibleBytes.hpp
- `784295f11ee8e3942a35222979c25a43e02fd43fa804b235d6f1f88d486bffc9` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/TypeHash.hpp
- `954536a181d7c6e6052087401c3075d398e606f1c17504668faa6764947d856e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/UnrealTemplate.hpp
- `bf375d43e0daed74b1b77f81f7188d9f26bbc00005f7282086a8d6c8e754d89e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Templates/UnrealTypeTraits.hpp
- `153bf210b2acddabfb80b2fa6e6866a34cb0fb83ae4b464d12b4f0ea4b156a46` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/ElementType.hpp
- `cef9d1d611514b47aa17eded0c48667b8b38dc50776288b200ac390f5f1f5965` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/IntType.hpp
- `f0831da0d3f6b5bca9b816d60516ca9319bb6be3e662fd7678f1eee8d762fab1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/IsCharEncodingCompatibleWith.hpp
- `b76fa79b2280705cdbdf1afe6c16cd2cbfc9caf2f5391624af7db0155c84b813` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/IsCharEncodingSimplyConvertibleTo.hpp
- `d66c2be76de4bdf01cf2b32c7df37e9fe16553cad87bed109b7b4e09870cfdde` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/IsCharType.hpp
- `1cda96cc436e6d96ab40897152d82ef4c14665dc5dc057d5e50eefb7f5b90d81` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/IsContiguousContainer.hpp
- `a39dcd58d08769ff320c2651e782bd5db88c8abf011916f834d2e9a9188e5e65` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/IsFixedWidthCharEncoding.hpp
- `309248b39b8364469f76c167238a1611e85ee0785ac58512b3d56147538f30d9` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/Traits/MemberFunctionPtrOuter.hpp
- `b80ef86bdeffdb2cc2603197deb605fa7c04f9978ff11f2efeb9e551884d12d6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/UObject/ObjectKey.hpp
- `969ab25cd37017ca97d0de72e5f80c0ac04097b095767b7fefbe98f30ffaf48c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Core/UObject/UObjectHierarchyFwd.hpp
- `0f5f4f9d595ded18a70cb6481e48be2bd2be00ff873e7c6554cf37518b8cbbb5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/Class.hpp
- `f139fdfb12bbfdc788773ae744c557dcb8f9d99b36e579b1841d144585365030` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/FStrProperty.hpp
- `9d1fa49995ccc99f79160b680a87e5deb4d4c8bc81087ba9d4c5bb09f033f038` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/TopLevelAssetPath.hpp
- `ae18e8c076ea9f501207a2a90d301c5a2a56225af4600056cef7b888eac10881` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/CoreUObject/UObject/UnrealType.hpp
- `32d3968e8074c04af13cf3eba4ad5e3de1f61344f5bd99aed89a8fac2ea1a2bf` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Engine/UDataTable.hpp
- `b60e25d8ca8ec81ed1429b5e3d9bf9a4b065371e898521857ea69477b049a5e6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/EnumHelper.hpp
- `6b0acef31914ff085383f47f58073de853b7796e32777c51d0c0482e7406df93` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FArchive.hpp
- `7de863bb421de59dbc9dd0fd3dfab3eb31c16286ac4b95b72e7631d1f3ecbce1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FAssetData.hpp
- `098c005d5d3f9fd3d1c6ced63acf0e781794fd8a8c2ac57f23c724c5dbc67c7b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FField.hpp
- `80368012d04e5a16758772d2b8b6cf102331f8d868ab18ea0c4664241b1aa98b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FFrame.hpp
- `e34a6fdb3b07aea8220bfaddfab95700608df13423ce9450d647ce8050644c10` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FHitResult.hpp
- `725344aedd54b5b9148ce76427461ee7fe1eae961aacd36ccc4044edfe365ee4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FMemory.hpp
- `42119e878a9e2d45d51fc2701fa177223e2dc033847b7d60e922dc03d733ed4a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FOutputDevice.hpp
- `3a62c63b136f7227392f66217cbd423640d095eee915bb0ba8d3ec0f8efd8fa8` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FPrimaryAssetId.hpp
- `1f841e22b31cb78c0c85753e49585edcb705d10a8eaeca0901715bac93bf8d42` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FSoftObjectPath.hpp
- `f4d870fd1d581271ff0218787cddb9b7d8f2d30fe52b6ece21d9c7759b76511c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FString.hpp
- `5f5f0496e67f26b79fdb2351cbc37901c98d63798a9ba003fcf7d484bd39fbd1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FText.hpp
- `b78abdfcfc9046f5d24f0d9bbe1eb8e248efc40973fee7905fd38b96a5b7a98b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FURL.hpp
- `b2808753bfc9ffa94b23b0f1258b96ff86f1e9339567955b71c346c9a034b79c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FWeakObjectPtr.hpp
- `46d92a0e4b790fe5458aaf2746250030658d0bc65776216abf0565c4a32aeb1b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FWorldContext.hpp
- `aea376c29ae1cb8e2860ab3f14b301b4dca8245faee473598890ae044fcb4e9f` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/FieldPath.hpp
- `c116ea8de7c774c70819fb80b599c014bfe9f0b0e92bcbaa2a85fed0c0f39ab4` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/GameplayStatics.hpp
- `53a8fb881296d7e6e67bce5c97403db923e8e3512f51bf79c2e647246e4bc970` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks.hpp
- `eae5bcf620947e2907dd5aa4545777c18f53054ede062accf922296c963d5a1e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/CallbackIterationData.hpp
- `05336eda3b6d7d3276f4ab7ccc9986d0e61bd3f59cffa71bda4cd737dff81107` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/GlobalCallbackId.hpp
- `01595078c91ec1d995c335c7f261b56d61b19d36085f255a05ee65d0ed0862dd` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Hooks.hpp
- `a8d3e32adae0b27af1f64fd6dd97ac6f199fd5901b94e051a41f31f2cd2c012d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/CallbackIterationDataCommonImpl.hpp
- `b85c34d92bf627448003ae9d48f59e501b97469635b612a2008dbd56f5976b3c` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/Common.hpp
- `0ad43072af76cd6654638f992b164a35ee6fdc820c0ce0c642458919260dbc5d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/Constants.hpp
- `5647e6e16702aa4133f63107f81ebed59805e856b37efcac0d8d4eee0750af46` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Hooks/Internal/SetOnceField.h
- `a7e18ea46dd15124c1d677727cb058da252e5e997e298096add8fcc47863ee09` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/ITextData.hpp
- `9eb897605057db88cdeb35c1181b1c1d7badd55cf2ea07c7809f19875be15259` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/LogVerbosity.hpp
- `5fa8d93e7c5d394f92c434e0a6db36d881953c5cce08d1e0e83e59cd22873e71` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/NameTypes.hpp
- `f15aeaf4c462bc8bedc0baa89ed32c97e96d07793ec2fb37abf8f77225e0313e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Net/Core/Connection/NetEnums.hpp
- `607475756c6a0293d572f7ff4624da5d89a172e77b41619399264987558b63ed` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/PersistentObjectPtr.hpp
- `3d05b06841a7e3f1bbc12834b47936cd2690c8eb974edaf5cb84a46f13cfa5ec` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/PlatformModuleSnapshot.hpp
- `b2e885a178cc3bcc40cf19c9b24ca8e8ce02adeab936c6e6040bf3ef3581d494` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Property/FEnumProperty.hpp
- `04cd01b2f1cdcc55612befd784a947ba49426a899750be102b8c5542ff03cec3` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Property/FFieldPathProperty.hpp
- `ce15774db9795e9b12c571b7d7da939828045d6c3a2622cb7518dde4fa516ad5` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Property/FTextProperty.hpp
- `8f5ab39333afd6e17ddc5c9930d2fcddae44d59e1838ab1a9d31694f0327fd81` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Quat.hpp
- `f3e7f25a36e799cf9633ff6dd228368c61213b737395c202f6da19f552c8d976` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/ReflectedFunction.hpp
- `e42a707d247059e51278e8e30c83b0b16775a50274bc5e2c0f83b9999f54bf66` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Rotator.hpp
- `5c7caebd4f3abbaf50f8a37c49ff24508340767938718923a2e773bb86b12988` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Script.hpp
- `e303625e2c6b57edd8cb958f9b561b1bc83779143b34289247940de684c7ca58` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Searcher/ObjectSearcher.hpp
- `542e2158b08555b34ede8eec440f9f38ff6f4cff20ea1f8ae2af0a200f886ea1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/SoftObjectPtr.hpp
- `03d5d355318aaf8f0f1fcbe3276549e2004ea933ad96da682cce3c62d0820ee7` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/StartupWaitControl.hpp
- `28931b8944aca50890693a6aeb3913df0103641b66328cd9ca17492c623e37de` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/TObjectPtr.hpp
- `9f2d372b7c334b2262d3fdc98eb86f49b6d7c7f136835cd5124610142de6e2aa` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/Transform.hpp
- `47951a3ade8f011d279ff5cca9ef39942e95bce2b120a56451bd04f98cb35c55` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/TypeChecker.hpp
- `940a0e37effd5041793607bf9d9dced4eb6985984d8a74a16f5dd9575f969881` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UActorComponent.hpp
- `c656d0b71151aae3f84e518501e37a3ae95b95d17f8ffc64a148e3b8a45f9502` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UAssetRegistry.hpp
- `12c70551e0463217d3b1774d4b892db02a5bab2a09f2a07eb306510766f1600a` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UAssetRegistryHelpers.hpp
- `3d6807c894d7337485bfeea30355ee1586ebb2c9e27e8fc62bade58e28f027e8` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UEngine.hpp
- `acdddb1b1f31c305aca678ad15184b3f9495eab1dfccba6e039b0e780d1dd6ad` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UFunctionStructs.hpp
- `42d20fc7553261a54751a614b5ca15dbbf7d71b0fb9a85c317919a27be897472` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UGameViewportClient.hpp
- `eefa79e3cc8b9cf827d63b5f85ec4187a01e1a077027ccec2cc77c562ee3249e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UInterface.hpp
- `da1304c13af12da895c25d695d9c4994b50e6894eb59860a8ae0def7b6585e14` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UKismetSystemLibrary.hpp
- `19c2d15b3fb21f542e83d895958c67c46fb7ba9fc8044ebee351cb0c6b5c6643` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UKismetTextLibrary.hpp
- `cafa372a0b2190799a7512e4f2e9ce8b76a5aab79362bac2acd1c9dce11401c9` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/ULocalPlayer.hpp
- `e3fdd1b01f966c7c52bcb6d78d971e27cc8af68ed0729310dd08a9f238b923f6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObject.hpp
- `b01bf4ef84638bf6fa2fb52386d3a3f53ad6f2bdd27de6c898eda417b5554ce1` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectArray.hpp
- `c33e1de0eefe586fac451050e959e2996127e1627c9ef0a8b9313ed4be871507` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectGlobals.hpp
- `0099a3709fc90ec1bb77c10904f84e6f511d0acd4c906306f5b5d0f56486a6a0` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectHashTables.hpp
- `f0944457eba6efe7e1183961287010e5303c17a0b9c7d9f53e2d90d705d7a5be` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UObjectMacros.hpp
- `49b0ba9621df34c70ae48dc7671770d85b160065acd9f0b9867a9805bbb47e99` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UPackage.hpp
- `0c5949d80d9e0a30a913385d0ca642bd3d60c4332b96b3a3fd9e864654e76db6` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UPlayer.hpp
- `3b405fe4c90347bed74ddbe831e1d6d8e07b3815531908226a39da05f30c2d7e` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealCoreStructs.hpp
- `f25d3dcf070d3da8627d2e95b24a29ca8a4bdd0f49d17284324f52520121efa8` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealFlags.hpp
- `4d5f1d84c16b8d540b2b659a502d6e30523be9112fecfa56f84b6f34a9b35643` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealInitializer.hpp
- `68e9254555d54b8f4792ddc855e3f9dfa0fb759f7ea48bf2efe4f43dead9372d` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/UnrealVersion.hpp
- `cab23ec43fc4d63d7b59330260bb28a2a389b3b43da061b7583bc174d7a4b2d9` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/VersionedContainer/Container.hpp
- `dc30ed3878c553f73a81e8e3dc7a81c712a6470a5310a9b712eddad868a6aee7` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/VersionedContainer/UObjectBaseVC.hpp
- `0bf29a3cc2c44fa43403fba408c973429a5e0e8a9ed0de8aa1c62551440ec91b` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/VersionedContainer/UnrealVirtualImpl/UnrealVirtualBaseVC.hpp
- `394f154fe9798675bc67e0183ce523adc43b2b97c6e55ba79155a3e359b8eb63` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/VirtualFunctionHelper.hpp
- `0bff220c344e0dab36d095fdbfca0c90cb1d6f2c29f27849a00a9c87d51f0623` .superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/unreal/include/Unreal/World.hpp
- `45e0adec6e6ed720c80ef9d537b28dde6ad188e4f0dc9a18d27443b049e0af5a` .superpowers/native-dependency-spike/ue4ss-native-layout-parser-1/include/NativeLayoutFiles.hpp not-in-frozen
- `9b022645442f23cad6203d32a1cad1b6bce36c2863c8defe72452c5447bbf116` .superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui-textedit/TextEditor.h not-in-frozen
- `a92534d918030048328fffcf0a50236b8c05f79558355b37e34b664e13c71091` .superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui/imconfig.h not-in-frozen
- `fa6d0dff157392227fd921f3d2a7c6813460e4c7c4ecb2efd743117658377b49` .superpowers/native-dependency-spike/ue4ss-real-lua-link-route-1/source-dependencies/imgui/imgui.h not-in-frozen
- `48ec3b11cc4a7c5f7af2f23229b1402440b2d89d53cf4ee023211fe957d466d4` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers/emmintrin.h not-in-frozen
- `5cd8a3d288d469c9e5a2fc8a8ee624f50889dbcd06dea1262ef7aaf510a204af` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers/mm_malloc.h not-in-frozen
- `b3a59c5b4bd653d0ea3b96b82fadc4a7adb97ae9dc7bb2abd8872e9348238a3b` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers/mmintrin.h not-in-frozen
- `ec3c096ef84470e34198986ea4fc90ad328b197e2f43693f654656c69051fa8f` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/clang-x86-headers/xmmintrin.h not-in-frozen
- `260cf6792c90be5182a12b7ade0114dc83815d3ddec2eabcf6065ecdf6fd8887` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/Common.hpp not-in-frozen
- `2de4d76c2d94981608c3a393694a73ac40688b5df214fad9042db3c05d7683f8` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/GUI/Console.hpp not-in-frozen
- `18b0640e4f928a027c4c87d05ca3ec0ece4a6a4a81266d446ee35d0474aa588f` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/GUI/GUI.hpp not-in-frozen
- `99232821c64c3c594e534b4493372f00019d171b58ab96dabcdfa37d291264e4` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/GUI/GUITab.hpp not-in-frozen
- `0b22123203186c5688eaacff34a7bccae1b269ae7f86ad6b5abda7106c174b7b` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/LuaLibrary.hpp not-in-frozen
- `9db0724a5d1e93d947f07e85db7862c45d4d4ce495c0ced7968b0b89eedc02cb` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/LuaType/LuaCustomProperty.hpp not-in-frozen
- `7e71b91e39354bba9e9349fb2bf526f16336de4d4b063993c3b6ae63c37ef5fc` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/ObjectDumper/ObjectToString.hpp not-in-frozen
- `4201eb48dcff19f79c5a45704b5a5dab69c8bbd57ce0e465ec388bcfd4af099b` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/SDKGenerator/Common.hpp not-in-frozen
- `f2dc21f72aa95b140a024e56fe3f9119388ed7d2391d40237b60e4ce546bd71d` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/SDKGenerator/Generator.hpp not-in-frozen
- `ce0140d3f080b737873fbb5ff5e4459976f6fa84d66f50b5714ceb5bc8d91409` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/SDKGenerator/UEHeaderGenerator.hpp not-in-frozen
- `175e74e208e44722991f275a72520c0190a47cc209644b8913cf735d9817ee50` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/Signatures.hpp not-in-frozen
- `cd4d24359b102c829685421b788df27ea1ccddd204db63c347cea4507e1a18da` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/UE4SSRuntime.hpp not-in-frozen
- `cd11fbde33586385c9d5cd7f207af9ea2483279502f49d3a3eb1faf825ac6249` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/UE4SS/include/UnrealDef.hpp not-in-frozen
- `1a8dabf3303ffe95d29caf70d2bb3a43004a19535fb57c79701ad22ee833d6a1` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Constructs/include/Constructs/Generator.hpp not-in-frozen
- `b32d78eb3d5e2719d021f9e30ea43b253ec114d90e6c0273f8eb0fab64e7ec37` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Constructs/include/Constructs/Loop.hpp not-in-frozen
- `a73caf52b660f4bc7ae4b1baf427bcc358c6528ee6d79ffcb8bf127ada4797d5` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include/DynamicOutput/Common.hpp not-in-frozen
- `6f25064e19f4efd55d1580fca8bf50fc01585c4d47968aafd80c3cab97ec06e2` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include/DynamicOutput/Macros.hpp not-in-frozen
- `26b45e6dba746aab8a6abe4606e754e275eb14df4f6a035913dd41dee2db3359` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include/DynamicOutput/NewFileDevice.hpp not-in-frozen
- `7ecbace1c679efc4d63744ebf16cd12a35002621d826286686744c3a9f9e019e` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include/DynamicOutput/Output.hpp not-in-frozen
- `82beba2bbf4fc6c4657f451347f1d7eea9a45a9c81e208cc468666c496af5ac4` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/DynamicOutput/include/DynamicOutput/OutputDevice.hpp not-in-frozen
- `5cc515d14ca057018af04ad8e10bff2e32ef6e2bfc9d6334915f92f63e5537a7` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include/Helpers/ASM.hpp not-in-frozen
- `9fff0758270d20b57645c36dfb8e1c89ac3749c1b5fd1eae6a08a578a7f92334` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include/Helpers/Casting.hpp not-in-frozen
- `e5286b1eeed8630b979dcb9905a1dc04654f507e019f31c50fd7cc0aed647cf9` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include/Helpers/Common.hpp not-in-frozen
- `6e5a55d3b2d5ae24a8c2aab0371a76fcd0bc0e2e0e27df89082f78fbcd0c7f1f` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include/Helpers/Format.hpp not-in-frozen
- `cf871faafd9264aec55d806d67a8ea513ed931343a9f331d25501dd99f2f4e40` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include/Helpers/Integer.hpp not-in-frozen
- `038e64f2708f983756f0020529cd2d982c8b3540daf32ffd55967ab7d880a2f4` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Helpers/include/Helpers/Time.hpp not-in-frozen
- `334f511522ad74726bd297e12d58d0f1e53da12baf7e6c4fee4ab6528ddfcaea` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include/IniParser/Common.hpp not-in-frozen
- `a0362eb9b3a9a46c69ebf936537fc76fac314e19c8f687f34c4abcc89d658b46` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include/IniParser/Ini.hpp not-in-frozen
- `255d95bdd357d9099de4be45f950e0fcf916d1ead9f6b93e26f3433e7eac63b8` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include/IniParser/Section.hpp not-in-frozen
- `9d11a081b213e49c013b48af65e2d5803b44251b670d018206cd88207b807ff0` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/IniParser/include/IniParser/Value.hpp not-in-frozen
- `31f01abfcdddbbb1e82aba06636f20c8749460276900246799b0d0c0da13c846` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple/include/LuaMadeSimple/Common.hpp not-in-frozen
- `0c807f2decfb531477c4fd37597e9a7defe8af3842ee36e9afe7bc6a60e7e74b` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaMadeSimple/include/LuaMadeSimple/LuaObject.hpp not-in-frozen
- `2d9e18e577a6062646268e7f7228264c28bd9b972c186e1af81b8357179c0ab4` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include/lauxlib.h not-in-frozen
- `048c6574ba5e8d225d47b66e83b479832ce353881f263ecf8389247442921f6c` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include/lua.h not-in-frozen
- `9f165f5cbbf9150b917008a38d37cf63ca8ed94dc5f9b1dcea3c281ec76ace32` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include/lua.hpp not-in-frozen
- `3e8e572899bc4fe1be486cbf195c91abc54ab96caea735c870c982a6fc00af80` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include/luaconf.h not-in-frozen
- `4ea0f67d3e467f5d6a327616e22a3c2da8e03d47a3c3660916275f2842f521ea` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/LuaRaw/include/lualib.h not-in-frozen
- `8527ee5391bf359c4575dbe60e990ed51bc98b74eac4109a3491a8a4f83c0e4f` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include/ParserBase/Common.hpp not-in-frozen
- `520b9d1e74d2d2aafae2b300953a94eb2f20cccf3c52999a612c1a219d2e3716` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include/ParserBase/Token.hpp not-in-frozen
- `6df646f698feac2b520075de28b5efa91b798159701517542914d9ca189001f0` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include/ParserBase/TokenParser.hpp not-in-frozen
- `42643182b7190a0bdc3cf21eeaf74e6e096c530273e916ec57ea751494bfba2c` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ParserBase/include/ParserBase/Tokenizer.hpp not-in-frozen
- `3e8ede1368414db6d137c905bf444fee1f60e7fbe09687f3d5f5625d046b926b` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/Profiler/include/Profiler/Profiler.hpp not-in-frozen
- `42b6acaf670e2a9ecc2b20f3baa3d7897f10155d0bd16c1e7b963d737fa67641` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/ScopedTimer/include/Timer/ScopedTimer.hpp not-in-frozen
- `88e708ada07525df4d849c8088ac7b0abeac932159bc0d221308ffcc38d5eed9` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/SinglePassSigScanner/include/SigScanner/Common.hpp not-in-frozen
- `1e821d829317898e76d4d91d5969f4315b937e1b2646d585c6cacd8668d22c11` .superpowers/native-dependency-spike/ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first/String/include/String/StringType.hpp not-in-frozen
- `38e1b13d4a8e03c2a4095ffb551112ab90f2507d131fe6467cb22537a2d6266d` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/ASMHelper/ASMHelper.hpp not-in-frozen
- `b8950bbfe3f2a0751b0158c183834d593df1d2b775a4017a14926f324102c992` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/ASMHelper/Common.hpp not-in-frozen
- `663935436553d75150425d87fd0d31f51c2a9b7921d2d428d4782fb4e7a5c496` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Allocator.h not-in-frozen
- `f14c7bd37a8f6529ec88d603dd13ed8def0814ffc4aa28685eb823ea88624e2b` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Comparison.h not-in-frozen
- `825ac1378ed0de3d10be321e8aae819b37b73fd8c949ea209ff2b15d20e2ee40` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Defines.h not-in-frozen
- `032643815e45c573942f77b3595af62767f1c2b103d8112dc55b2f496e09ce07` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Object.h not-in-frozen
- `a9407e4fbe3c4d2bcce036f33d74d5d9cb26236881b947048f64a940d87550c1` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Status.h not-in-frozen
- `f01412c6085a1eb4a017a7b3f0137674a883de5186c61237cf2b72bef5aa1c18` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/String.h not-in-frozen
- `cdd2df8c7a9bddb3d5618be232b47896adfa2f7abea17ed10136539e46a33d2d` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Types.h not-in-frozen
- `e81837aa540600ebb7d3a6bfaacded895736840b6cfa91ef1f61e2ea0ba4db40` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zycore/include/Zycore/Vector.h not-in-frozen
- `86d9630e129825504309e8dda723350b28e4bd6222d35c2438956582edccc399` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Decoder.h not-in-frozen
- `d7281023f0272ba247589c7ea75ab1414e73584ffe7f89b1c5aad11a521810f4` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/DecoderTypes.h not-in-frozen
- `dd02c9eae3c02a38bddcded170148279f41809abda0fee605b3ac522a4ddda5e` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Defines.h not-in-frozen
- `8eeee97fcadc6ba45c925816d39645377c7e9ff961593d4fd9a7d0571755d034` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Disassembler.h not-in-frozen
- `9f5f1af8faf8bab8e46d22267bdad338b70934756b804b394eb231abfd05f4c7` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Encoder.h not-in-frozen
- `e464dcf82b72c04b2251885251aea1e7da9151da5e1875bf5897b4153eb38de0` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Formatter.h not-in-frozen
- `ee8a0192f7444f460e78cb39283379d3bb965c78873b62537a138326f5865973` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/FormatterBuffer.h not-in-frozen
- `e46e01aba7fb01257a3c9dacc838943b685a6596313afae6182d746885a6dc47` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Generated/EnumISAExt.h not-in-frozen
- `f34268cf8534676f76a05344f1aaad25feff2b20ff633277b3170caf17c13f75` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Generated/EnumISASet.h not-in-frozen
- `aeecad2012c76199147013ebe75d48ec4f77753e54dd17b20b12200847b0e21b` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Generated/EnumInstructionCategory.h not-in-frozen
- `449812f4cb61095c93ffce8306f1fbcf27ccbc0b6ce0d6c0503e37a590c4e7b4` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Generated/EnumMnemonic.h not-in-frozen
- `18d6d84bba78b046c6de7acb4d9e824cb6a120c235284864e97c16b466fcfd22` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Generated/EnumRegister.h not-in-frozen
- `d7058d26cc2577dd73e879babf448cbb63c777bfc2b93d65235c3793029f46c1` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/MetaInfo.h not-in-frozen
- `96e0951dca96a4377bcd506a3afaedef2cba85730397a53ecb080150fda14a2d` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Mnemonic.h not-in-frozen
- `2827328e13030965b4638cedb5af2cd677764ccb45dbbd83f498a4070c209c11` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Register.h not-in-frozen
- `d048ee78f5482f538e4cafbb64996c35a318da8c872dfbe3ea2e5d3b6cf2dfea` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Segment.h not-in-frozen
- `4db57865cb3cb312e9e94ffd55f135bbaf52059d78754c8fde531910b8b32070` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/SharedTypes.h not-in-frozen
- `a71cf2b1d1d3662255e4f16a44bef6dd650453dcaa646e4790808282ffbbe14c` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/ShortString.h not-in-frozen
- `4773ef714fdf44e9edb26fb592231efd27a79cbe667824c3042e7f7140be50e4` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Status.h not-in-frozen
- `0282d65a3a1f251f062d9b8e369463ce16b4998c2cc034d007f7a847b5897a78` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Utils.h not-in-frozen
- `1481ee71bb4289fb7f6bb62cd0441e05229db66565ed8ba2c32a38486c77bb45` .superpowers/native-dependency-spike/ue4ss-virtual-dispatch-dependencies-1/zydis/include/Zydis/Zydis.h not-in-frozen
- `c36047093eaf127b3d51bec7b03bdc356a6b207be51ad65ea8669189bb6c1b51` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/_ctype.h not-in-frozen
- `d66ec78f56e38ea24b304e9168a20b7cba750a93658a6eb9ab8662517b5ede75` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/assert.h not-in-frozen
- `812b6862cd44fc73fba22f9c0847344a3d1d8f1fe8e28a23513544bbb8abe893` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/adjacent_find.h not-in-frozen
- `99bf4307a9726d472f297e77ad69e1f3c3f973d584b393a23b1e8e6ee8fa8095` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/all_of.h not-in-frozen
- `9a32b6159b52f7f81545ad932c40b2749f77829639a87f30080460870d0cd0c4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/any_of.h not-in-frozen
- `5051970b5a1f720497a8d36a2417b164e22589c5cbeda221b8d4543763d27485` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/binary_search.h not-in-frozen
- `b9556fc56c7e883e35fd4b7333d9c11b8b012c356da3eea78d9e2ca96953d6b8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/clamp.h not-in-frozen
- `e4e34e2c4b2308444cb861c197004a0653ebc21be1a2c01e295d0ce6b49896fc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/comp.h not-in-frozen
- `104fa0511ff5b8b6f089dddd824cc1f9e4b89afb7311d93b495ae4b4dd4019d3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/comp_ref_type.h not-in-frozen
- `829bddbd61e32852e57c379ee0947eee718318bb123a2b032aeb5baa2e4358aa` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/copy.h not-in-frozen
- `97c0ef4a6207fca75f79148057458d631a4053c6a1e087158bd80d86c3f1da6d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/copy_backward.h not-in-frozen
- `af67c1ca7b4cf86de31bda91c79550bda436265b0ecda5078287e7e6896b2b52` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/copy_if.h not-in-frozen
- `509241dfab7ac730c1e290e39ceb96b3f57bb6534c5fd161fbac9ecfac829e90` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/copy_move_common.h not-in-frozen
- `a2f9ffec32bbbec8a7812c9bd15d7bf403d032ea07133c17777ad6578ab656e6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/copy_n.h not-in-frozen
- `13478a9bf55cd1c6b0611c09e31e4b639a87c373df22a76b8c15c5e19a6fb162` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/count.h not-in-frozen
- `9b93a15ec4e7073eb3444b68837d98ad9e30da555a9524380a3ead106fe2e46e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/count_if.h not-in-frozen
- `007570597a68bddc3bf614b0707aa111d8b1005b5a09715dde19f061f0388acf` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/equal.h not-in-frozen
- `8d94b659585edc6e4e72065f222d279259a14b1e220d1dc8516a9620750ab331` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/equal_range.h not-in-frozen
- `6d072cb630d74ab192c9297d92c8f7b94bad5692fd66cf29876d5fbd36ec6665` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/fill.h not-in-frozen
- `26f45e697540bd1a0d45dbf38bfcfec2290d280ecb3755898eb3a8355a9ea48d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/fill_n.h not-in-frozen
- `7ddc672e2dbe0dcdaf325ca5006ed0ce9c73ee6189c2884fb10273d6930457fb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/find.h not-in-frozen
- `f70213fe712b81b253791fb4e2bcc60f9653810487e8d31fc7a057fe0c230ac3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/find_end.h not-in-frozen
- `4de23bceb48001c3c7265f577c446ca1e9a48ec67fd936e1a0cf9ab6044964f3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/find_first_of.h not-in-frozen
- `b1c3acd5189ba39f3f913f0d5726331250dd1cfe528f575a232d6073e614e27e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/find_if.h not-in-frozen
- `ba1119a732e4c1bd7155393fa449942ec01852033dcdda21a5ed1dd9dfdad8db` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/find_if_not.h not-in-frozen
- `ffddcc6f767efd655594e2feff2ebc84454c180680c2541214c304fd59620091` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/find_segment_if.h not-in-frozen
- `a24c96de4586cbdbcd58c538e50135d2b4cca894c80c47c8e18fd6e1bfc07a5c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/fold.h not-in-frozen
- `57249946bf9bc647e81de721be538d6b3900a9384de3958d95ac2597ecd8a089` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/for_each.h not-in-frozen
- `9ce1224212826724194af5f235cd55085c66a8b58a1b9da94c23772219195771` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/for_each_n.h not-in-frozen
- `3538977bd3f5318c59086b6001a6046eb019772426e1f70fb0b7f1e27ba5bc48` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/for_each_segment.h not-in-frozen
- `b171ff43815b25aa106994e643715c9816b42077d5f3d54d23f000cdb46b1e11` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/generate.h not-in-frozen
- `ee6d41151393be53c8c8ce00313e178dcdb2fc7c509b0763b7c42486580e396c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/generate_n.h not-in-frozen
- `f1492d7a43e994f4d61f66cd41f28d91686351415382fe92fb4c5f70d2dc4e8f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/half_positive.h not-in-frozen
- `20d16bfde1500da427e47cb42c1ebcd1dc0af6936ad70acfedbd43ffa54101dc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/in_found_result.h not-in-frozen
- `b5852335524085221b7100b93599c6a3cdcd3878e518729fa5e5d46f2feffb2a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/in_fun_result.h not-in-frozen
- `a6963fe5b1f76973881fddc93405cb7c519a5125725759dc50aeb840c7465779` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/in_in_out_result.h not-in-frozen
- `5f669fc3c50d578f4405ee4563b6e179e424dd526fadcf87cca647d1b7d5fdc0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/in_in_result.h not-in-frozen
- `8315269ae375934a62cce118c5e88b2bc4929ecc193b253f84615f4af20a9d24` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/in_out_out_result.h not-in-frozen
- `dac26981ded3f0d0dae643f4b902c2832bab2cf2939e0df66ba3153b61bd3be2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/in_out_result.h not-in-frozen
- `768b60e1b99799c240eb36a3bac5d0a27073d812ba104d60dc5c7229db7c958a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/includes.h not-in-frozen
- `bf24c4c1cb299c869d44d0d56c80a40729828165b1211af48e8aaf586a400dec` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/inplace_merge.h not-in-frozen
- `173730e0a3d5d842b84ae18a291f758871da2954fb700c505566d5cbb5d03c3e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/is_heap.h not-in-frozen
- `f3b6c2bdd5e733a259500e6de5e6860183291569a6395b29bea89f41ae0f6472` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/is_heap_until.h not-in-frozen
- `1821f8ce95a535f4673044b0f333aeb4fcdfe7c6c9716960c3b875d10698af9c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/is_partitioned.h not-in-frozen
- `0f9b0016f8206663941c472755d89636c0e0a0571199a21e83f9ff1d2ad69cd5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/is_permutation.h not-in-frozen
- `641b5bfc4d2fac2f55df17b3a26d8626b8e81ca11eb0e01e9deee74e019c8851` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/is_sorted.h not-in-frozen
- `43caa44509dc17f949b637d5be5219f817efc5086818dae78a35d6e11d4c23e8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/is_sorted_until.h not-in-frozen
- `e3c5d079bb836c67b9b1042fba2d63e3690a945fc823189722f4f51634df2a76` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/iter_swap.h not-in-frozen
- `f6c56c548422893fcf44a13f73a2c0bc0b1dcd1995d02ae4acbd99699ee1983b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/iterator_operations.h not-in-frozen
- `5af6a29e9227f4177a068c273e031619164734f2e362d61907cfbc96eaff05e7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/lexicographical_compare.h not-in-frozen
- `12eefa53993762b39f228bd0956894b6c6d2838c2096b69c7508c5fc671e30cc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/lexicographical_compare_three_way.h not-in-frozen
- `02c64596931d1767af660e6e7f8739a91efbc38bae3abaf97992574ce735b446` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/lower_bound.h not-in-frozen
- `2bd10216f89b2e595314b80639789d96aa8055b15f5820b60293cded57efda84` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/make_heap.h not-in-frozen
- `3d681a9b3d68f92a741c2e7fdfc6d111e3eeb5f2b6955986e9ea39ef147b47f6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/make_projected.h not-in-frozen
- `a9e661295c73b6022ee26803307df717ba5b7959f2ce7d38d278f58538faf74b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/max.h not-in-frozen
- `e7a2283303e1464636fe3d79fe22e50725521c7602dc4e77a4715aa33b5dddcf` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/max_element.h not-in-frozen
- `f97d5222d6f66c8643b7a5a2bb630bb2ccad6429067f28e42ddcbcc9a8f7aebe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/merge.h not-in-frozen
- `63a683c64a462011b9836ede439b34cb7855a99c2f434fe7bfc3c791731bf6a8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/min.h not-in-frozen
- `da07849eedf0e08d6648f74c743dd06d00c716160ba882734db9825223b74af6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/min_element.h not-in-frozen
- `45ad58f249444d5c6df7c013c8a22abdff632ab9be7d24c3725ac171a40d87dc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/min_max_result.h not-in-frozen
- `64e5302706caf90efd7c7ecdc0aa317802f79f0061781e03c88a279a750e6d95` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/minmax.h not-in-frozen
- `a093dfd1674545b456e4220557bb0c7c546712316b76c288349e1cb6cc465c12` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/minmax_element.h not-in-frozen
- `1217503d7f172743f3f2461bd34c7ee901996915cc698152d5f7a5f55bba359c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/mismatch.h not-in-frozen
- `5344a2144dd39cce2cfc043d4957aa9e657d96ef64ad4ae5f1d6d7413af9ecc3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/move.h not-in-frozen
- `44ca522d9503870aa7c79f2e4e9952a211f303b6fb9f8dd0d3ca3df1a78555a5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/move_backward.h not-in-frozen
- `97197b3dc58dbbb4fdc6d0887f557342762d96c7296006ec53a8d9a0566e52f9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/next_permutation.h not-in-frozen
- `b79e2649d14f52855d2bb99edd13d32f3aa1a77c6743d7ef00cb6a33ef6202d9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/none_of.h not-in-frozen
- `521550aa2c37e40ff717a076c13f660f350ec2fbdf64add747499469986efe51` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/nth_element.h not-in-frozen
- `1661259f38dc4f68c7baced7ce5c8939d0070f000888a3ab3231f513719789ef` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/partial_sort.h not-in-frozen
- `315c5e925357e469c9e51c83c194e74d5ee3e7cef649b1d38cf8fd0930e12325` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/partial_sort_copy.h not-in-frozen
- `b20a1c513657e0c40b946089e476ec14d56ba9289b99676fe2f2c4fa438b563c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/partition.h not-in-frozen
- `921060f3ab957bb63d043cf989507d29dd3a8b5e3b210d4eab620ba61a2a25ee` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/partition_copy.h not-in-frozen
- `94b2cd082810c5bcda27dc3c1d87dc68c19209ce645cf1384a880a304ffc478d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/partition_point.h not-in-frozen
- `fdb1a1d0926bfc3f68eae20f814fd3eaa18b70e0d5e0b4a4ce34ba4ea7465583` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pop_heap.h not-in-frozen
- `f8c0290f459d7ae7620a902a51324de2845514112f6d61c2ae3851b9b68edbf6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/prev_permutation.h not-in-frozen
- `8c8ad3c7aa026612320964bc18105ebe039b885b53dbc57e87ff7100540083fb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_any_all_none_of.h not-in-frozen
- `17a54416bbe6ffd26b0bd9dd9db684334dcdf9558632ffed08175aef59d1abe1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backend.h not-in-frozen
- `3d67c712634048998b268b21ce6e7723b179389f1d3c3afb64cca27a7e35ce15` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backend.h not-in-frozen
- `c43970d3283d1b4955b4e745d96f583f9743181642bc9db07d01beae960bc260` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/any_of.h not-in-frozen
- `fcb78ac3af61a56b3d4bdd77518607d39c263f598b66f0b449d6fa9157733957` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/backend.h not-in-frozen
- `6c18d0178adbb431a75df3d9c0fe259a26405cbbf822155ed2ed74df10f77403` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/fill.h not-in-frozen
- `256deafe01731136cd6bb954c5502136272c3eb54d339eefe59eb7f16a704fa1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/find_if.h not-in-frozen
- `8a3602a9aa58e08ae3058280da87e0c8ef2e13fab51d5ae57042c1eb690a8a68` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/for_each.h not-in-frozen
- `dc41e6344b4ffc8cfa83f923c397e6f547c609523252260e0cdec0c5e8b3bd39` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/merge.h not-in-frozen
- `deee127e4bc76f0df9ee9f678a8b13ccf04dbd093d2b1ca9f15f4b9556a587ea` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/stable_sort.h not-in-frozen
- `1d2f67b9f94dea3bb05706536327729adaac09e8825e7d94d15278ea10bc806b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/thread.h not-in-frozen
- `368fbc817297d7ec46d1a5ca7687f98b1bc8703c84a99f498cc171be8aa27799` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/transform.h not-in-frozen
- `97ef2a3614107f1b2cbb29deafaebb8a79629d205a28d97a40533eea00b38ed6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_backends/cpu_backends/transform_reduce.h not-in-frozen
- `f2506c00bd5940c89b1b504864c5a8b230eb085961620829af1a2a8334df43bd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_copy.h not-in-frozen
- `18a20fa1d3fc4ffb74ac6c53b17b23bf7fab8d3f8c28f5f01e0563e3f50587a4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_count.h not-in-frozen
- `461f3b4f7a60587ef25507b02ff1c55e1c55dc7015c136f0d6a9a1c2c95c22fe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_equal.h not-in-frozen
- `a5188eb2d55839768fade1407d114fb6c5f4fb0f43cfd0b51706a92bb1e46ed4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_fill.h not-in-frozen
- `c8f7915acd91155163bf072af9474abb7e38973f67b3cc976807c921096b429b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_find.h not-in-frozen
- `67155e9ad61a40a2063d9e6007619e6bf1af77f0873d797e3cd232da6ade23e1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_for_each.h not-in-frozen
- `1d047a2b87fd1b276d9ddb66bc409550a7a362e5ba56091b32d6d3e527d14de3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_frontend_dispatch.h not-in-frozen
- `cc7870f39b58c649e3d538b9c92610fec27f3a26be87a1d458001872ed52927d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_generate.h not-in-frozen
- `756f5abe8157307a2924d80b9275641b6bf990a0e3ba57ba6ff49474fe292f3f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_is_partitioned.h not-in-frozen
- `ef606a851804acf8d6cb6ca0a4059a175e33a7733d34258c0ee6998fa5fa64f2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_merge.h not-in-frozen
- `df5119a4411eebe22657c6739b5c155cd82e899091be08e2c8f359f61756f401` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_move.h not-in-frozen
- `5944e4285f4c1ead1952ed7e37350b6c84cb0ec239825211eef5e4926f39a298` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_replace.h not-in-frozen
- `a079a32c082b17f79aa089a6fe8fec6b440a22e5a14e764c19979936fd96db5e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_rotate_copy.h not-in-frozen
- `347c12652ddd540ce649c2090ba7b5858f348d0a5349fe8058901b254bcd4ae3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_sort.h not-in-frozen
- `dcaa8d62528f4570f6a0f305a2c1dc77825d6cbe3d2aa65ccf7b49608a1e0507` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_stable_sort.h not-in-frozen
- `e4b0ddbf67887d4222de63b06fc07dbbc061dcc8d9a9c6b1da4c3e5ad589a369` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/pstl_transform.h not-in-frozen
- `e9103283f553bdf1da579bda2ec030b5dfdde74ee874ae9f58aa1b3dea454358` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/push_heap.h not-in-frozen
- `4a8bb2de5bf67b1504a65503e66a25f9ee350d205a9c997d637401ee7dcd92fa` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_adjacent_find.h not-in-frozen
- `3960ce0a53080fb8410752961469b6b518205707834fa445e46adf48159e43ae` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_all_of.h not-in-frozen
- `46748aaca4b3deeb60703be2bfe0b72c225aea75b77c141afcc11a812d7fde34` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_any_of.h not-in-frozen
- `90038b85627c91908d7617745678977da7f2a83c396d59924b350d0ca36f9bbc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_binary_search.h not-in-frozen
- `9c2cb701c4ce3f9a80efdeb4b35742ac90b35ac22f61766673d9957e93822e5f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_clamp.h not-in-frozen
- `19ede824cdc237c3091907afa4adb198758e0345375780d74b2ec0b942e768a6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_contains.h not-in-frozen
- `8d503536d4ca9901776249e5461e29acd1291c79cfa1782a1150499b8afd25f4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_copy.h not-in-frozen
- `481e636adb656c1af353c701241a28c144bc309378f99de7c6274b198c7cdf3b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_copy_backward.h not-in-frozen
- `d249c192d61d4d1373b640a241d7076eef22222aec7f050fff5aa07361bedd21` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_copy_if.h not-in-frozen
- `41530b1bc05f3f56bc73f04a74007c6c2751917f730c725540eda8f06594354e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_copy_n.h not-in-frozen
- `0c80a1c74b75cbe51a840cf54c95a668dec5be0b20f96aadaa075b89a90cfee0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_count.h not-in-frozen
- `88f70ff4733e63c850b930893a7a362e7c8489dba286f2f39de70275ab7dfcf3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_count_if.h not-in-frozen
- `ae04a5c97b8f93a007ba8c4df0a3617386a46fe29d6085ae982d486451b30bf5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_ends_with.h not-in-frozen
- `1491e503b7acd625fdf39cb8d642b5b91aeac17791e770a0f8bfe20458a577c1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_equal.h not-in-frozen
- `87e120f3e2777d1f1f3b409d1b4b3f648a137887be7babbc00be9358048c6525` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_equal_range.h not-in-frozen
- `7896a05424d574d2533ec53489c0620c992d8f2302fcc38d3345d1450236ab6f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_fill.h not-in-frozen
- `0c426ab4dfc45479845b218de90379955070c212cc053cc35ac61d9a3dcbc673` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_fill_n.h not-in-frozen
- `9b9ab133343dfe0ca5f796f3c954f2006be1ed2f022e0395a52eaafd7fe76460` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_find.h not-in-frozen
- `d828b80b015cbffdc9ce642ba354c0b5a6a87d8d41803674b3d016ad3f05069e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_find_end.h not-in-frozen
- `977245502a7cef4c7ac18b10985ec71890f2fad72fc9938daf7bd93b10bfc7e2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_find_first_of.h not-in-frozen
- `3aa83787c5a005a9cce7f20da5c91595415d34b5afb79ae3a7571838c5005e30` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_find_if.h not-in-frozen
- `92922612d620e48f780180184931c85b15ae3eee75727307520d1e2e4d9d46a1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_find_if_not.h not-in-frozen
- `80d2e52f5f4da5e017c79449425f7ef2d8b3fa8a86a73dd0d0914d0a42154c6a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_for_each.h not-in-frozen
- `390751d3ac4577e5c125586b209761c3b5caf7e71d96b0ae85a8c531d07facdf` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_for_each_n.h not-in-frozen
- `fca0f3f7bf7aed83c675191eb6f6739dec4a92d12cdb630bd0d4f89a3585a9a8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_generate.h not-in-frozen
- `9352ec3f83a9d5dbbb7f4a0f67fb17363f5ff0e5bcaf2add80b72ec788da662d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_generate_n.h not-in-frozen
- `11e520a58ba6d0d9285d1bab49b76fab1be7d213ddcc68f5e7c1c755cc789a46` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_includes.h not-in-frozen
- `b671683696fe631fcb6ec777bbb051ace8bcda14e0c763fc0defa751e68adfc5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_inplace_merge.h not-in-frozen
- `3ec141b635d82d52831dfcccec37ef97606b94a74b96061ceefdbc86befb3a89` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_is_heap.h not-in-frozen
- `13e1b7fed71e8cef599d59d40021778f6f9333667662f34f05dcd88ffb756fa2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_is_heap_until.h not-in-frozen
- `44f7b41ce12d725153f5f9a5bba2096897ea05b82efac71f24e79e6d4b341b95` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_is_partitioned.h not-in-frozen
- `0c8c446c7ac85cb13597bcf159b570a43a8cb9f077f69265e48f021df62d93be` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_is_permutation.h not-in-frozen
- `63908cddb3648e50e8abdd86c7e5d3719c9316a799d8bf55f32835164530c824` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_is_sorted.h not-in-frozen
- `c3b031e96f4180f07ee9e2a2db2351aca597c06d8cb40ace840d1f83dbd5c2bb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_is_sorted_until.h not-in-frozen
- `0717e59d9bcce3874f9cce3c17dc4e726a6dcc2517f487b4476a7aa094dafd8a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_iterator_concept.h not-in-frozen
- `d62b122792eb71fbe739cc49fa9a72dba21d72d8b9a0c6e4790e8c6bb6543902` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_lexicographical_compare.h not-in-frozen
- `27f35c3dbf044bf52365aa2df584fd488c01b730edba2d863579fcb7058e626c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_lower_bound.h not-in-frozen
- `5751aaebd2928f2e10c324c3248013d1d1eb7b0714b1a7bcc14191fadd913d50` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_make_heap.h not-in-frozen
- `6b3d5d10ff662c2539dd5a75d2d215bf743fa52181a0695ed28bf544d14502b9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_max.h not-in-frozen
- `5c169026ce25607da3b713042b006ad732f57d5822b623f056b07a828e3c5af8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_max_element.h not-in-frozen
- `c6c9fc4de8b5b26a29c5bf70dbba24828e9cb7d5ddca980cb69d245dcc0f2102` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_merge.h not-in-frozen
- `fb8dd09877190c8ab254e0c077788f30fc4b5637c45ab429f39608a7a5ae48ab` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_min.h not-in-frozen
- `c0fcb6284dd27f39c436553ce7949852309954be736312f060f3b5d828020d39` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_min_element.h not-in-frozen
- `af0df0b6613389d54d9ed73c919a110242e959c225a31291d7df06f3aeddf5d6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_minmax.h not-in-frozen
- `82a5f54675ea5e0999cc5fe714bf61b75858d6ec7815cad8ad202f7fefec044e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_minmax_element.h not-in-frozen
- `06cc61d9ed6ca1c661ece354691d11ddcc2961f050e414e1123501ca070c0cf3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_mismatch.h not-in-frozen
- `be8fec7d0449ae383d0d3afe97ccd526dcbe8951a31ac1c0ba2906e7431921e0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_move.h not-in-frozen
- `2880f312c7249be7422cebdd99a4b0cd9520590833dbc1a012e51d0fddf121cc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_move_backward.h not-in-frozen
- `5fc3e87de0b9ab0478a6c8d97a8a1bc2a467c9e00ada3f175c132ced460d2733` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_next_permutation.h not-in-frozen
- `f37ac786d799e75fa53b325b11fe6f6cdaa412c2bb81055b579fc000cbbf87c5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_none_of.h not-in-frozen
- `cb2dfedff87c45031f9bb3b7cd3b7d90eb90c0ebde5474369123e5add3bffeb0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_nth_element.h not-in-frozen
- `288f40597bf7e33d542bd64cd7146647019c1985024a5fc4c999489b6144228d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_partial_sort.h not-in-frozen
- `d1cff6ba6932e18da6781e0e2ec851ce77b285f08bb293bf82744a3f1e2a1d02` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_partial_sort_copy.h not-in-frozen
- `a796e0620a78519ee65480065bbedb3a11d79426143fa2c9eac384418fbfb24b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_partition.h not-in-frozen
- `8086f858795a8162bd53af724531b11397bcf8510d996db7317c5b2a901a108f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_partition_copy.h not-in-frozen
- `1c3d08c4267e46a57b77574d47a12f9984de236d133f187cf1edf82f62182f99` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_partition_point.h not-in-frozen
- `4d84cfdd044071cec27544abccc86b850e37ea3c78d393c1d4f049f5fe9bd2b5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_pop_heap.h not-in-frozen
- `0064629db126b3fa84d882f14a9f4b94edcad00e9ec5553ff042897f6182b603` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_prev_permutation.h not-in-frozen
- `ebb91827afeb85423d912122fa99292e1e6d6365fbab51ae5ce72fd5bc06bc0d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_push_heap.h not-in-frozen
- `754b3a43e3c5ffb36c0228621ebcf4b33241d20b02e60a65fe368c8889670c3e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_remove.h not-in-frozen
- `0d1284a8dddb88f3fe096aabd4f03eeee2acb93bb50bdfb0d732128ccb0f2a26` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_remove_copy.h not-in-frozen
- `7042ee21524ca0b510ee4fa7063c850d0625cb1f9e0c30613c4b2f6aac69caff` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_remove_copy_if.h not-in-frozen
- `2c7662ff690064de972438e09093cf34dca9261c01d52db41afd6485260de42b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_remove_if.h not-in-frozen
- `67aa09a0a35e4535153d4e07f837a19482ea72a8508c49860f42da8ed893c8d8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_replace.h not-in-frozen
- `d3d2f929c66b2847e7126ddb8c0370c1853c5b4c75efd1c59b11598befee7d9e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_replace_copy.h not-in-frozen
- `69ba4c5e7b94f861f8c27cccd2b23126ae6829bbbe708edd774e95199d2a3198` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_replace_copy_if.h not-in-frozen
- `1e0b5310388008426fa765fac4edd13763e3726eca66b6dc231a0a48ee248f09` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_replace_if.h not-in-frozen
- `1ff48502cb73c49913a0fe6a5a5bb649fd4363d905e3d27d1c5ab08292dfb0f2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_reverse.h not-in-frozen
- `c494b53c4304e6a275c913c36e4cb4f48e940fe78d56ddfcf36c5ae2c247171a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_reverse_copy.h not-in-frozen
- `60e517b81d76005affd785613ecb86d9cf2613fdfb6125a7e1410619c19377a0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_rotate.h not-in-frozen
- `ff47484c50105f41053ef052489d2c242304296963fbaeb5db1beba853c70613` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_rotate_copy.h not-in-frozen
- `8f2ca52dcae63781a945b0bf7a7be31684ec31dc9f18e5da7ea6560980f94b58` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_sample.h not-in-frozen
- `1ecde16ccac70be1c5c32f72520af87566786208a73f039cb8bee275609fe577` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_search.h not-in-frozen
- `8f0bbfdb394e88ad177b2bec43987a1b5ffcbc9abe5d14a0219a732f66950f70` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_search_n.h not-in-frozen
- `93059e88a58a01c1bcb7c8c36757e67aacd6a0c64d6a37e372d9fa96b3c409ec` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_set_difference.h not-in-frozen
- `7cb53193fcc8e0ff1ab34cf64ea2483ca371d4b654fa101f6f0e3e05c915bf03` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_set_intersection.h not-in-frozen
- `7338bfb534e47b7fd96a41e2dc25546a4783ce9222b9ffca69b8cea1555f0c8a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_set_symmetric_difference.h not-in-frozen
- `2cae0a1074624bd50aea257598ea807eb11f65ad60d2a31b5f7f4989c29a0cec` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_set_union.h not-in-frozen
- `ad0f2b110a97ac4e5cb325debf835052c8cb8c52843a150ff3875cb864a10fc5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_shuffle.h not-in-frozen
- `d67ac223345408763e725e823a56aae27db1cca28eba04dca431ef101af9003a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_sort.h not-in-frozen
- `1d155c7ee69694c303924422608e9410f39982a261ceda8611b5e5fb1ccebf0a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_sort_heap.h not-in-frozen
- `ef5621ad0bcd35292408772f450789c3835c17b38959c7aa8b060f2fe49c9f9b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_stable_partition.h not-in-frozen
- `2a28a43dd96dbec58286d1f3364893cf32f892911b083dd5d3408dd2134c11d6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_stable_sort.h not-in-frozen
- `aef6d9399455566a6f4aa690f0db0c8e3d112a8f1be2f80f2509bfd369ba0526` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_starts_with.h not-in-frozen
- `9eb498b6df432aed08a231233acab400ada76ec3f9fff36b6ad8cd92c3df9112` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_swap_ranges.h not-in-frozen
- `7d61aa55c00c7c24da6bef71d2fd080a48f4a111bd924c2c380d8ec4a89033ff` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_transform.h not-in-frozen
- `18a2131009822fa681a6c736b58d00bf359d8d27162fc94fff387639fbc9de3d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_unique.h not-in-frozen
- `24498a4c2ef789af9b4eb892d940c8b45e056339d280ed1666937b4a1badc99d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_unique_copy.h not-in-frozen
- `617ee21172fe4aae96a191cc06bbcc343c1a4437c1248cb1cc7563625eee6070` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/ranges_upper_bound.h not-in-frozen
- `6fabf80c91be55c011a0ca6b7bb5493791b9649964c2bfddc239ec858e8b6c48` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/remove.h not-in-frozen
- `f8016ece8398c80bac2680cc8b2fcf849c90e2a3c5d285348eaa719cfe28acd3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/remove_copy.h not-in-frozen
- `6d6d0cc1d07f453dfeb297b4002f57e1fd7be020247907f7568755e78b4e5e46` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/remove_copy_if.h not-in-frozen
- `769a26d62ab091c97a249751cdc1734565f63130e1a64aadf2e3985b8b48325e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/remove_if.h not-in-frozen
- `6ad1946331f5e28c0455528c49a93dc5b5844c897df15348eb7a0c5e2e4bb225` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/replace.h not-in-frozen
- `757181c7dafbe8938c5590e0a33a57a1d171db90acc19cf7252cadf5ae5e373a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/replace_copy.h not-in-frozen
- `abacf024cc9869f63b3032ff66d442b9646a0b4deceea74f263dee17cc1b0d39` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/replace_copy_if.h not-in-frozen
- `0e891ffd96b84fdbbbf5e15e483c5ed149d21c7ce1ef56c0d2414b29712b473e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/replace_if.h not-in-frozen
- `5ca3f05ee497c14eacd7e64433a91a6da214269c2424742963ae708059a98d5e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/reverse.h not-in-frozen
- `4c003858b0d4b48d745a34aa0b5e1b71ee4b920aae387810c83a73e9b0bda301` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/reverse_copy.h not-in-frozen
- `2ca390ac28ce7968c87ee6e09f878cdd27282944797d30d16eaa163613060fd8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/rotate.h not-in-frozen
- `d49c29cb1c29b0f22b198e0e7de00ce58c80da8fe6691fd3e1e4200391657b74` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/rotate_copy.h not-in-frozen
- `6db42f2f6e0eef273549943629971b9eac34f2f43a4395cd42f3a43eb339b57e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/sample.h not-in-frozen
- `c2c87c1e239c61a1f9048037785f2de3884aaa760e8bfac2aeec34fe5d3c8a2c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/search.h not-in-frozen
- `631884e993ebac6ef61645d70894ff5b443581e27429a8cd04675306fb65110a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/search_n.h not-in-frozen
- `28bbab14767d14df47388973afd974282bb8c1c149fd135b897a09f7c291494b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/set_difference.h not-in-frozen
- `3137d6837c0adba8e17711ba59a1b9789af0ff11fdc3baadf7c33f6f60a6e459` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/set_intersection.h not-in-frozen
- `894e704839d01433730d9c0f3d772ee79d0452255281f048df8b7b82e59db134` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/set_symmetric_difference.h not-in-frozen
- `81eca56aeed0aca2076839635a7d07db845ab37479a7273018fe7c4ff9be4f9e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/set_union.h not-in-frozen
- `cfed3e3dafad588dc635feb580840b03dc9de72616c60b95bce3ea798f48b8cd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/shift_left.h not-in-frozen
- `7129b23e0a4bd1174dc079cbc8b6e52298ad7c97cfc023b824a0dca6ac103b96` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/shift_right.h not-in-frozen
- `2213486cb5407430fa5d5b3cd1731d22284c0c35a9c254c6c8b783ffda636307` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/shuffle.h not-in-frozen
- `3ecb05c593283f3c06e004ce3225839c3d4e45feaab899191b472a89a6f8db77` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/sift_down.h not-in-frozen
- `79faaf6ed5f8acd8e789824db6ddb6a66e4d6e9be0fcb0cba49682ad90993c79` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/sort.h not-in-frozen
- `c6a81be48dd77f944d6f4a5337c261d308331334c06446687f557c76b7f223a4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/sort_heap.h not-in-frozen
- `cdffa89fe2688265b0d2343797b4c3afd11a13e452fc69b1fe119706d77a0434` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/stable_partition.h not-in-frozen
- `298eb55d05574002ff12884a5872d4e2a88cea30898e2dc6c37ae1d8245a9962` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/stable_sort.h not-in-frozen
- `6b84ff28b40669509ef3e07c6614ebcf3150bade9470483579a5092e9be49cbf` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/swap_ranges.h not-in-frozen
- `e48d4b642d543860bce34582489a6f6546f8514af9e61d21c9b09babcd04855d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/three_way_comp_ref_type.h not-in-frozen
- `82a69b8f4b399b49ac6f475b1fa3a5623e4445b57c09c9f0d23bd35e4114e4ae` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/transform.h not-in-frozen
- `ec371dbe78b916987a52927f06f5f7b00862faed5e218d13e3c349ca15b59704` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/uniform_random_bit_generator_adaptor.h not-in-frozen
- `9ad3f1e086bf18a13557e632e548b463ba5f5180641a474f4fe861f0348811c9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/unique.h not-in-frozen
- `c7d9fc1f8b490ed5322d9fd9e01a4eda93bd799d06c320fc3dac14b1514a4d34` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/unique_copy.h not-in-frozen
- `40f156dbcd3a1cdafb1507d43ace778bfcff61712786a116c93b94d927d60607` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/unwrap_iter.h not-in-frozen
- `f8d22d0b22a0666548e9a7dff09a98e1541b1bb0aeba84a5f03b5d867ebfd30a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/unwrap_range.h not-in-frozen
- `4c67e1797af77427dde38780f33edfedf9d747d0668549717552c68837159b71` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__algorithm/upper_bound.h not-in-frozen
- `edc9d40f10d5d09565e3df14f4ac1bbe4ee0197afd64e4164f621009433bb7ef` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__assert not-in-frozen
- `2fa4dceacadd36ae06bf468667157b39019650581df64359e5c6c43e4ec5fc1b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__assertion_handler not-in-frozen
- `45617a639ce36c5e9a634b611a4aaa5f88eaa58e075a91e14e8442b2e667df8b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/aliases.h not-in-frozen
- `cbb6cfad8768ffe1bb99379a51b354663fb2159e97797d279bbb72c55b0d0810` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/atomic.h not-in-frozen
- `ac4fe1cdd4aa0688acf17eefe40ad5d4c0f32aae56cdc148ca601b75dd111bfe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/atomic_base.h not-in-frozen
- `52ff9bc3363b340438c18b14805bb7777ad5a0636e99781789e9d82e514a9d2e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/atomic_flag.h not-in-frozen
- `58df52df151c5f67e6bdaf624b813a6c810c4efc2e348dae30ab351846e0ee29` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/atomic_init.h not-in-frozen
- `7e3b6d812466c1c6cd4c4890cf9fe55e5861a464726391252de2caf98e114d3c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/atomic_lock_free.h not-in-frozen
- `0f82464cb8a9c4fe22ffd38038a3b306dcbeae381dc5db4ccf89ed39be9f9d89` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/atomic_sync.h not-in-frozen
- `33af34705984dbbfca8066e3dfe831306820a2e971cdbef82fbd0249172c087a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/check_memory_order.h not-in-frozen
- `009bce3c0bf47fd2714f69538c0b5cf7198e5a210dd6415e62b2cc168d511d9d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/contention_t.h not-in-frozen
- `49329bc0a401bf02b046462f8585396ec491d4363d52d29046c6a5742c15490c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/cxx_atomic_impl.h not-in-frozen
- `54da19610d1eb56d528962f7c089c2e5de0c0da3a9240cb11ddf35ad807b2a3c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/fence.h not-in-frozen
- `aaba8199679e9dd9171785407c03d45413b661eeba63e8f9294cf0a0d72591ee` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/is_always_lock_free.h not-in-frozen
- `fa4df0d9780085450aa6abb0b818ede2ee32d063ece6a69013fd03b2061d8023` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/kill_dependency.h not-in-frozen
- `1cff19d7bc09314366a5d21bb657f9e31ac099c0022da22eb1ce3cf03db626fb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__atomic/memory_order.h not-in-frozen
- `bdee2c608bc58b890cc5767ef4df8cbd95182bdf5b0cc4f734e02fd14601c1dd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__availability not-in-frozen
- `3c4f4bb7addf23e6e064040756829b5109624c4eb911e8934e61a92241f61021` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/bit_cast.h not-in-frozen
- `1e1fba907380a47263cd9e62f950038985ca45c72b5a5418c44bcfb8b09b1d01` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/bit_ceil.h not-in-frozen
- `fc6072089b214a2484f4da47577626eae5badcdf46dbd23dceec4750df58c52a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/bit_floor.h not-in-frozen
- `6ccf7bd7f62ce5b52522bc805dc18191f98b3363c4962c57dbc2ddf751585631` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/bit_log2.h not-in-frozen
- `1ddff74b0868347796729538be1c5d70999422d3e6bdb0feb40300953f5b29b4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/bit_width.h not-in-frozen
- `651cc6eb75d4af07c242689ddc1396cd53e2cb541102e6d9aa0926f7c72ffc40` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/blsr.h not-in-frozen
- `68a3a124017abe68757bb48dba1e1efe74af910a6611da8d6ba7985ff0d3cb1b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/byteswap.h not-in-frozen
- `ca0c2e5036803110c5d867e7c9d26d03b609f47670b4bbc6634aa9722b394d35` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/countl.h not-in-frozen
- `73f7187b535343c43cb896da73dab2e395a283d7380a91b5825d79b290187c68` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/countr.h not-in-frozen
- `7951789b2a2fdaacd7ca371531bdb6b9775119387107522bf428482989507a67` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/endian.h not-in-frozen
- `9fe47b94d427fbfb92587c472dffd9989bdf21e5a999564b662774bdd75016a0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/has_single_bit.h not-in-frozen
- `585527cd8118a3f0bdc7ae33e89b39220054cbc3cbfe537d8b261fe0655c92af` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/invert_if.h not-in-frozen
- `f303e9247f1883ba645131cd987b720276706873340b8384187cf29e010e33d4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/popcount.h not-in-frozen
- `0ce36d308550ee05c42fd452cd2a9806f43663901ea4955ab887d540a9be364e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit/rotate.h not-in-frozen
- `c6fd6cd6f850b2849abecbbdc0d328b7c43d111486a36e2ffdeed5a6562aa5e2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__bit_reference not-in-frozen
- `53631a1ddda64987a28424f651ed5eaae0c1300343d43b130caaf922ee1fd00c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/chars_format.h not-in-frozen
- `770bd58e436c3f6aa5fa5103ec135bd0f4b66fcabb6c5336f4d9a94076f70400` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/tables.h not-in-frozen
- `2e3096e985c0a613d6fda7efbbf2d1a5842499e69b523042fd5204522205e246` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/to_chars_base_10.h not-in-frozen
- `2817787dffae60cd4c72a119c27471cfad402a4652f782857140eee86e890237` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/to_chars_floating_point.h not-in-frozen
- `260dce738155deb8e4a156c3cd8f644fc7333a9042e10f1e0904a7363af9175c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/to_chars_integral.h not-in-frozen
- `b60f711d782decba7f4207c9cd5047ed3f98af191655baf5a1ce34e85fd3b1be` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/to_chars_result.h not-in-frozen
- `8bd60403422ce2284cbe6da95e2e76b2cb5dae093810f4d10b221fe87e925e5a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__charconv/traits.h not-in-frozen
- `3fa1360f8ea62172a550a7fd96ffc60ed9b60881082a84d32c1d5ae0c51e6cc5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/calendar.h not-in-frozen
- `caadab3a2377b93e9e90d9281945baa8b1595cb795218c15744fa1826f7d7682` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/concepts.h not-in-frozen
- `5355457b09a689ad3c9bbb47ea6babe7b2fe6110515fc3a5c230bebd94cdec13` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/convert_to_timespec.h not-in-frozen
- `eaa2f1a235cce94a3ced620fffa278880397cdc62bf368137f196eb4a152479e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/convert_to_tm.h not-in-frozen
- `18eefa6a0f6bce0094b6819e91d7bbf40783a92b2ff7314c2c66ff9e48828921` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/day.h not-in-frozen
- `7676cc85a55134a4630cbd83e331b342f1d55f2d6a84a471c089d60bad565ef1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/duration.h not-in-frozen
- `7a1b0f397b43a9f22143b36fa96e932abab947e75a47b7320b435fc173c6d837` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/file_clock.h not-in-frozen
- `b359d8aada3a86ac86bb80024d9da1d97589de1a92af1f74088598d6cfc7037f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/formatter.h not-in-frozen
- `89f52fab4f566a9edb6ad03ccd33714facddc8c952ff2780dfea9c336bb0db4e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/hh_mm_ss.h not-in-frozen
- `6371fab9ea084245c159edcd47aed40de40337abfee469f48a616c04b1110a5a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/high_resolution_clock.h not-in-frozen
- `c1f8959ca87f7f0362cbbf1d34021d36b4dafe2ff70532735cecbe9251dbd347` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/literals.h not-in-frozen
- `93b6d20270dfde897be48b6793f9dda3fcb2ee8e511da061b3d870d5e2a9ef8c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/month.h not-in-frozen
- `9770c5e802c22976b3d743ee16c98b781b9511512f825a64fab5839059cb28f6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/month_weekday.h not-in-frozen
- `a6d2cc06215bd8b76960a64a007f731967e49f1a29a645cdf11e89dbb0dcff0e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/monthday.h not-in-frozen
- `547cc5186b7e4068c83f2c625407b9dd7a37ce052384afc4d769e5e4f5813960` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/ostream.h not-in-frozen
- `dbc0df9976694bc2ecdc4bfae0c8bdb5bf76e61f1f1e0cf18efc4224ba5020e1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/parser_std_format_spec.h not-in-frozen
- `08d1443c6de300593b6a6597e41bc58583130023103428f3c2ae0c7b4f61fb44` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/statically_widen.h not-in-frozen
- `0b0a141313da2353a2cde2ba84aba735f9b50e23db0dc32635813b192a5f19c3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/steady_clock.h not-in-frozen
- `d8960a860dba8847e24964a218ee520352cd7d5c162f2b50e48db987ab0bfa8b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/system_clock.h not-in-frozen
- `8b4c4a179b7a080641ec08c55821e7c3e0b2ece405a3ac226c370d4e871f4a5e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/time_point.h not-in-frozen
- `91166fdc53da8636cf4e255c359fb73c947035b545aeebad4587335bf50fbbdd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/weekday.h not-in-frozen
- `3587a13decc72eaf252c26fb667001b2702bf4cd858c4963cf58e6a5435a8e04` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/year.h not-in-frozen
- `c522ab82fab9cc8b67a4f68383460e8170e8d070f6d2c9093396cdfdbeb15c36` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/year_month.h not-in-frozen
- `0fabcd463dbd3c8108796c4d8b24caa3afc59149a8770498377d34b8528d1480` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/year_month_day.h not-in-frozen
- `a020710a1d42b13e9637d5a76f0017b9e3f283301174c21258ee860ec95df807` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__chrono/year_month_weekday.h not-in-frozen
- `cfdd1854d669f6e7235a7da98eee1f0b34d3b06419ed27bb69472129960bd250` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/common_comparison_category.h not-in-frozen
- `68327be768b539ea21b85a5496ba167442a090bc721c45385b0848b7a188a1c2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/compare_partial_order_fallback.h not-in-frozen
- `779f2cd9cf2796f521dc82cd3ccd3e2d95c2123082f0e79468de93d69a786be0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/compare_strong_order_fallback.h not-in-frozen
- `e1533426dcc7cde8b55bb7c6fb52ca7cd192d509b2fff702b70cc80bd9b65b52` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/compare_three_way.h not-in-frozen
- `b9241fcf6aa9e783b930ab59468111a6af3d90a5621d65b52b22e95f9d925420` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/compare_three_way_result.h not-in-frozen
- `4996052101add2056e1e35144af287f665d362a1229ad6748d4ec1301f1831d3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/compare_weak_order_fallback.h not-in-frozen
- `71968af91dcba82a32e82805fb5b457aaaac72badf0db983095ecbc77815c2bc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/is_eq.h not-in-frozen
- `e3555047516f640d1805cf1a4e6aeba77bcfea304bee6880c2fa17771d0bdb92` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/ordering.h not-in-frozen
- `5317c793c4a8be3bd520adc0ce5c3114b2e59e63d1875758281a06e217d38299` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/partial_order.h not-in-frozen
- `c7214c86e0bfb7f5d6c75b3359c8350ab7e723a4da52bfdabbef1fce6e28ce6e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/strong_order.h not-in-frozen
- `3bb3ba08c9757895719d14e99572d5c5587497988d4cf1d730e5dca4d051ef3d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/synth_three_way.h not-in-frozen
- `b7dfbe1785c3dc4fac568a351eca3fa232210889924c0727488ce50932bf5e22` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/three_way_comparable.h not-in-frozen
- `c599255e93d81b3b16116dbe6fdbed66ffa70e3ebd864f4d60c7e54175c8bd8e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__compare/weak_order.h not-in-frozen
- `cd3e7513e940a5a41d63f59e7f5223fcf630d0b82e037e3820e9df59ddb73fea` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/arithmetic.h not-in-frozen
- `a084fe593666de14e929775f27b62c83300d8484d48f5d81a21d62bd103abae4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/assignable.h not-in-frozen
- `0e4f0847f07c4132993ebe7ff03b966f67cbc87aafe2410b74c49996372c6af8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/boolean_testable.h not-in-frozen
- `fa833a47ecb4852671c8911065a2e1c36d07443cec94d6e3a9afcf96d9da36fe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/class_or_enum.h not-in-frozen
- `c8a377ae7fd0ee94e04fd58f33ed1e8881581581145e8f7a120747ddf07e2dad` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/common_reference_with.h not-in-frozen
- `06cf3277d80d983770d00a21f4c7a0fad5fe3f0880b4ebf1b71d8f93ae4087bf` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/common_with.h not-in-frozen
- `b1f147b2dee62ecf8c690da43358fb1c61396d50fadc17e3adb0f8c8506d22dd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/constructible.h not-in-frozen
- `e0130321b4cb81bb7ae90eecad5e75188a2d81e1f8841e15a421e34e1ebc731c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/convertible_to.h not-in-frozen
- `6ba6614001023de29de8c27ff0e4694204bba9fe670b677ca539c097e3189952` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/copyable.h not-in-frozen
- `5b7fdf0fbda8750b4843cdf5d12f4caddd4e60d80d3d9d3fe02a329ab94f393e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/derived_from.h not-in-frozen
- `c69524a72675f8e0a68327c55e5614f44839a261ff5ec60e8f6c63be54103111` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/destructible.h not-in-frozen
- `862300e1f0e972a02d5cab4f81fcdba2eae92690c42e38cf1c77ffe175f4632d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/different_from.h not-in-frozen
- `a67db93cf701260b793701457364734a93c4c89b35fcb7d9c121081dced2544a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/equality_comparable.h not-in-frozen
- `af2eee44fea19d5861ac5eb86e03b643589cd6efd8bb717762a8d9237f33587f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/invocable.h not-in-frozen
- `ecec2c75a7808747a47c3d84f214977eb7daf6fcf73ed080489a89aacb826bfd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/movable.h not-in-frozen
- `10419e38aeec82298650b1f95dcd9ef8762d3bef7288a2ce6368613c29ba4a69` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/predicate.h not-in-frozen
- `6be99f051a063943a99ea3678c459c57f4b0adc374c9c9abc838b53f7c8f6f61` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/regular.h not-in-frozen
- `fb41897540dd1ade5860f573b6e7ee8483b1dd8112dda5df8deaa50d195e8f55` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/relation.h not-in-frozen
- `94e76a6f662c93d8d0dbe03fa901d79608ad8ea43f7276031c17c75f2aec0563` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/same_as.h not-in-frozen
- `850e0a2dc61eb9cb020eb37cb477b84b31768bae217099a9f334a93f7cf754b1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/semiregular.h not-in-frozen
- `1c687abf787d3ef077302bbb42f95da8b3174d741222e622c6e545c884ba53c5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/swappable.h not-in-frozen
- `802bb6786faeb2401a6031f14a5d431cee4462a3dc845048d545a684a300ce26` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__concepts/totally_ordered.h not-in-frozen
- `67b52daac74c94290df0cf231245a5ab5a793b40dc9cfb9d110bb666f4cadd77` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__condition_variable/condition_variable.h not-in-frozen
- `e8000acbe3880bb63f5e9bcb6a6aa36a7fa9ed3e4c2ef6ee51dc412948412e9a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__config not-in-frozen
- `07050e1e190b4b2c1964efd887149f63397f35a80c5c5f7a85922231f67bd267` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__config_site not-in-frozen
- `c9ad7dcc5df7e28130750bfe392a67845157020529ddc2390cd5d80623d8ada0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__coroutine/coroutine_handle.h not-in-frozen
- `03db64e5c88fe39fb4c38343d4f690a0d417b8a004e6be87d4a7c39bd67df4c4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__coroutine/coroutine_traits.h not-in-frozen
- `3f664114651e265324867d977b284d44f4e3cfa1234634bbd172c3366ce8be03` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__coroutine/noop_coroutine_handle.h not-in-frozen
- `5fb1e3b62e73b81950d186e2b4d1a36da6b5743bf49fef7f0f923e04f7e9ae26` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__coroutine/trivial_awaitables.h not-in-frozen
- `ed04ac54864bffe1577b762e3ea8e1d59387fbd2ed91096597709685b9f25700` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__debug_utils/randomize_range.h not-in-frozen
- `9f55cbf4ca5ad88d5f061e2bf875b562bf97de3d5e0ed50b29f724d5be8cedc6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__debug_utils/strict_weak_ordering_check.h not-in-frozen
- `f08d5fa8b4a9ce656c6218e898bf0f376a2a574d64c6f77710b21cb4f7f02c2d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__exception/exception.h not-in-frozen
- `0b9f4cd3c7278f688bddbc599cb4da5b2838e5b8f0e1216f6360e09ef04f1a3a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__exception/operations.h not-in-frozen
- `0f25957847e2693ba8989914e5bcffb5796df1d5f2e505d8e0be3e89ee74a234` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__exception/terminate.h not-in-frozen
- `432ad0dbf620ccea67f85bfec8a20b09771a235ac9b830e1e3eb6451454139c2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/copy_options.h not-in-frozen
- `358ad9ba3f7dbcbb0177d53d9353bbb8614e40a0d72b10eb1f7e6b58c01611a8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/directory_entry.h not-in-frozen
- `a83d19691c90050344ec28697ac91e5cd142cbc196b7a64ddc0cc025e1a02b7d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/directory_iterator.h not-in-frozen
- `276ed32702c66ac9b46f9c76db8ddc093cf3571fc258586fa3f79bf1fff4c9fe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/directory_options.h not-in-frozen
- `de066112aa4d5be862c6112fc7a75879b7bfa1ac8c7a22ec0937ece395d74b9c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/file_status.h not-in-frozen
- `c38efe71c888b2ad9b1271cc47709f0f87e4d938c807cca9d7d8719b090b1104` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/file_time_type.h not-in-frozen
- `f1a6eb72ef6266f9ba166605a75cc457374be836a3c32dc7e3b03ef876a7ea84` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/file_type.h not-in-frozen
- `7d7f513c55dd7595bad96de3eae9019dad7c8d673c15a9a05f03a5ac902c5963` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/filesystem_error.h not-in-frozen
- `21cad656ca8fd38ce9e0b5f7f73bb62b672e4c58c3e11f59fa6b995411dff884` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/operations.h not-in-frozen
- `e3f0887aee3569ec5f8b2bd987f5f8ed1db9ff9f48956445ce4611452f188948` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/path.h not-in-frozen
- `feb8bb3c9b12139777061ba4a56d170a7e0cd0b70d0ed5cd9f07aaaa3ca739a2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/path_iterator.h not-in-frozen
- `00eaec7e4d64271a8781e08b72125c99a92e408f3cd1d1d826cd0e204760cb99` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/perm_options.h not-in-frozen
- `84a65bb72ebcb76e1b3942658527e4917c9bc572474e529833d0f3480ac48c91` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/perms.h not-in-frozen
- `7140d9003f979eaacab324b655284885d4cfb89b43ede6336a6474279eee37be` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/recursive_directory_iterator.h not-in-frozen
- `9bbcf8216c02ba2cfa092045d3c9cb9e46b50c09e5105e86fc2b2170341c242a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/space_info.h not-in-frozen
- `92c8eac99f37ca053a8f0ce660adfff60bb7c0939be8414a73438c07149ae6f5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__filesystem/u8path.h not-in-frozen
- `7ced0f2b03499ca255f4f2c584b1d90d3f89f94678be07989fe0ce4efc14270d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/buffer.h not-in-frozen
- `f1e24f14ed83bcb362febe5c20e20c15c7a430ba640e81e85e4ca08dfc57bf19` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/concepts.h not-in-frozen
- `5062614267e1c8caa605e684e688026ebbf8570561191325ab3730666851ecd4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/container_adaptor.h not-in-frozen
- `644622daeda88d78ac5b04487ed441fa47b7bc5ab2f90c24518797593a6205cc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/enable_insertable.h not-in-frozen
- `615f4ff3ff034299aa863db2d6f28e8512a9b84d5fffc14e567f0043864de26c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/escaped_output_table.h not-in-frozen
- `c24b69f330351f9e884fcb38a5878106df3a810e1105ad635e8671631f0efa3c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/extended_grapheme_cluster_table.h not-in-frozen
- `47d3e0eee52890eecb3b1d010cc8567539b84bfb9be5a20364d0a0032f4f0bd2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_arg.h not-in-frozen
- `6c1f1615020ab087d4f6c0d7592b664908f40194794408aa2e97d2ec146c45e4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_arg_store.h not-in-frozen
- `4a17a2ac45121eab4d77ff3b9b4b283539b211d5ac742f9763d8cf6033199180` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_args.h not-in-frozen
- `235bbf22ff08e637dd8bdad01ea311dfeeb142ec9fe189db399743a2e8985f90` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_context.h not-in-frozen
- `4226c8c1a447e3d16a20386e19147c35cc3ce9ddaf40da019a1c690523d11430` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_error.h not-in-frozen
- `d4040d791874dea1d1ded2acede782e1cd7ca2229e5788a45084090353c84c8c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_functions.h not-in-frozen
- `02a3576a20f28f0469d646065df3d68702fb1640a03025a37afd6f15387531b1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_fwd.h not-in-frozen
- `6cbfdfbf00b48b777bd83620d8e8974460fdf6cbf07afad304c1c56d776110a3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_parse_context.h not-in-frozen
- `852bec7e29ae4abcdacd7b7de21c5a66fdd8433254a10bad9543205a6718fe7a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_string.h not-in-frozen
- `13af4d40f3eb1f19300490b16e55dd804f663a10e8844c627aee66f292927f2a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/format_to_n_result.h not-in-frozen
- `c3ea587950e4add73b647902c3d4da954bed849b881050cd7fbdeae62071fdfd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter.h not-in-frozen
- `afa15ec66eb9269220c6192accaef304e4e65ed1f54da6bda8f666cbae68672f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_bool.h not-in-frozen
- `3f5bd9e0050513bc74fce94f09f3919c1ed6edc595343e4e3f6586e8a702e1a1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_char.h not-in-frozen
- `309e3b2b1b6cee2086e7fd0386ddc3bcb4f9b0d2817e18def7c9dd3f50019545` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_floating_point.h not-in-frozen
- `ecd68900d7ddabb2eb5e535995ffd559e0ee4677344815f1f5be02d633c6cd1d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_integer.h not-in-frozen
- `fdd3ab9ed108b5ea6eb806143e29e5d4b8b6abb904532bb3265238332c29d6d3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_integral.h not-in-frozen
- `85b70f2342108c9214fbc94986cbd58d618076cbc1536e63735cab1f6035a0ca` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_output.h not-in-frozen
- `01ef48fb15f29116818fa67f87ed28f33a26c93834c85ef5380525380c9d0702` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_pointer.h not-in-frozen
- `c9c84915c696039820126c82366fa184e050bc93f649fa61b36de4014916e4d7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_string.h not-in-frozen
- `50c6afa8e3a42fab935b4dc37ed977b7e3ee4d937c3d601140aa9b86a68f2dc8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/formatter_tuple.h not-in-frozen
- `d7db20957f27178a986419218fe34e9ff79e8394c0196b9a4dfd5a8ec143e736` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/parser_std_format_spec.h not-in-frozen
- `6e527b76554fbcf617728b6e389508da81c7f7510b227cf1d0209b30c8e7a1b0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/range_default_formatter.h not-in-frozen
- `d719688f39277307a8f1ce4a453ef522036173908f5b336bc60febf3a47266b2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/range_formatter.h not-in-frozen
- `9de448096a7038afd78eaac229b7d2dbe611b8314a9356331ba3ff5de1d6f60a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/unicode.h not-in-frozen
- `dca60bacc27440746e8315a8a805314f709438db9924d5c9d2e277226323a1fe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/width_estimation_table.h not-in-frozen
- `fa0ca0c847b65b95d0b9b72a73c07c438563a57b0f50fa86eae8a6b37685bc61` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__format/write_escaped.h not-in-frozen
- `4dcfddb979817051602473d3f8ce2da15da9ddc1f71837df790505c5b70092c8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/binary_function.h not-in-frozen
- `f823ba67644e5ca9487c79c62a6e33bbaa549d13f08edaad1a8a9a11b9f25b6f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/binary_negate.h not-in-frozen
- `bf7468036389a0c4214ab712ea50851c36ed7e85356fc5f228587b30877832a2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/bind.h not-in-frozen
- `7c0fda2b750e675bb37c52d0f56ea8d192f9519c3a712e076dbfdc41c3147b59` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/bind_back.h not-in-frozen
- `f34e583cedad1d8773781c32a39cf2c88b01d118cb528b8a556f1f0fc651e941` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/bind_front.h not-in-frozen
- `6e3e12b04b3acdcd5142d3b2fd96755513bdf3345de7ac637444155c9292dda1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/binder1st.h not-in-frozen
- `1480de07e2a6db8b21866223650cfdf744f2d2a261a35b1cd76263f7636b96d0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/binder2nd.h not-in-frozen
- `21ccb015049d09906fb570b81acf76d790b6e3ab2ff70799bf9c840e73de29df` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/boyer_moore_searcher.h not-in-frozen
- `7f224138a6a3a51644ca76f7c20b002273749f31ac2b845a879a3fff13b4f4e2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/compose.h not-in-frozen
- `3c826fcb5c01655be36e7d016522f3751df1743a5e5075d8244cb92d37589444` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/default_searcher.h not-in-frozen
- `db71179895ac3f5374b31e3ea3e37d8d5f3168a29a5e0020b26b1fb99eeb525c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/function.h not-in-frozen
- `c1b188f50074d744723c3448e1d94166df014e6cb3378529970b69fc1a5ec0d3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/hash.h not-in-frozen
- `63b29ed5c0f7a51de14f6603882bb651f3e1152a29f6e570df934075ba7f9b4e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/identity.h not-in-frozen
- `378573f7b23fe6492575698be34eea8d14a2cc9bf815fe82a0e544451547451f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/invoke.h not-in-frozen
- `bd240ffa08cea58d61b411358a66ed8a3f0919da0a21147d53d85d2e1d6b678e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/is_transparent.h not-in-frozen
- `aac888465860d35823e1c571ec77bd28e8c6421ef87e2d74e38d8a1d69d37c7b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/mem_fn.h not-in-frozen
- `ccd4ca0a3caa377840e387e7f6bff08762dcd05600caa26ba988d175ecfbbe38` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/mem_fun_ref.h not-in-frozen
- `d19f83d3e6cc05988650c773a61192a370f09a0bbd6a9f1f519c634f5f197c04` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/not_fn.h not-in-frozen
- `a9ddd099f2cc21b6cc91bc223d43fbc2f56557f05c8d6977b1f013ffa4a726f2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/operations.h not-in-frozen
- `3bca61e2dc361308b5eb4ace1e2eafa6376ec5907c71e2b9e69107be60045046` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/perfect_forward.h not-in-frozen
- `808a66e0d6de725b19725828029d5d65002caf1601abb1ad5d83906af949812d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/pointer_to_binary_function.h not-in-frozen
- `9bb5850e56582dfe66a3276a5e8f3003290527ab0ad0799d0db56d6b504e26b5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/pointer_to_unary_function.h not-in-frozen
- `ec4a31e1d8cc4b3bf3e27b57c30ff70304762f86f1a1dcdd4e44c39034369ce3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/ranges_operations.h not-in-frozen
- `44a9decdfbfa067be179532f53f225edbd16ca73148b0ab2890648b2dc1c5f0a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/reference_wrapper.h not-in-frozen
- `d7568d1dfa9a2cbde0b876add752fcda09938344e176c0da5a171625d2343f12` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/unary_function.h not-in-frozen
- `9f9ea3257383c6ba1896d94d73ed5de1753702f403afc4361652a2d29df64956` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/unary_negate.h not-in-frozen
- `f3354a6012fe248607d05acc77aba5a7a9396d48ac17bfc947f906349ade1199` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__functional/weak_result_type.h not-in-frozen
- `467dddc23132b2d928b35a4a8b2e10c705824dd6d220cf838fa46f0b919e862e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/array.h not-in-frozen
- `6e2ef9e44bb11be49add548b42967dec6bc96982ac9b9c6c1dbc6640f5a839ba` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/bit_reference.h not-in-frozen
- `473ce9834e6eda7fd6aa9bd83b51380086fd73b87108063bf7a8552c72c4786c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/fstream.h not-in-frozen
- `a2558f29bada2889998ae11bed272aaf4034ab82cc47983fc22644a9e57e2c5c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/get.h not-in-frozen
- `27ffdff7264d86470db56c2891741fdfe88e45176086ea5394a2482ffe708a81` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/hash.h not-in-frozen
- `9c9f90dff82d77a6169c373157d8d159c3900b219a07215e760757fbbf500c59` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/ios.h not-in-frozen
- `e38b29310374e66dd1da7c6bf1888e42a025ed180468cb40b69882d96b939c1e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/istream.h not-in-frozen
- `c77990743f647be4c75cd5eee78e7fdd6fbeff2e78e7266282ca33176919321d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/memory_resource.h not-in-frozen
- `bd232de76bc8cd04bc02eae9a371b11473aae1c9cca0cd5faed040f02428bab3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/ostream.h not-in-frozen
- `a874ea0e28798c9784b55eb5469cbf0cc0e8a2a1ca7af5bde4d8af8001971d74` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/pair.h not-in-frozen
- `fc9099949b1ca61546d2d4aef0cc491b51972fadbd46217dbf26fa314e8823a0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/span.h not-in-frozen
- `dbf44acf3aa42d009205d2fd1129d18d089395a21cde5eceecdc6a0511181450` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/sstream.h not-in-frozen
- `66f863292210dd7820ee3ec8c1b4c732a6a25a818e7e18653bdc14cfd00360ac` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/streambuf.h not-in-frozen
- `0f75f4ae0cbe468f70db610de11606e6da1ac36a6371aa731a67eada3a3c32c6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/string.h not-in-frozen
- `265d8b5f359b281d20e049ff5a53bff798f38b55b31b87250c0c0e3e13bc2c68` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/string_view.h not-in-frozen
- `b30a40a8c252dc9debd0f90b1d38fb481a0e1fd8ae09868c6a0f2b585a3e183e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/subrange.h not-in-frozen
- `7d31c6350b3653cfed406065661e3f9e6c75e67b8ef23f8f4f03cd496264ac25` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__fwd/tuple.h not-in-frozen
- `88468a56efe602178610fc08f94510e45ac0a7dd57181af85a6ed4a146ef0575` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__hash_table not-in-frozen
- `9a7ba3bf481eab778e0f00ee22ce016b465df91864f755162b7a20d8320ee6c3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ios/fpos.h not-in-frozen
- `fcc20f3b576b64b883c084d90abab62e988c088a626e26e22a7692939cb15a9d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/access.h not-in-frozen
- `36d14c0f19d01c6176dc8ebb449ab9d53080148748e43216becbb6f5115c99b2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/advance.h not-in-frozen
- `ae3d720a2d148a9343554e3c350febe064ae77f214ff2e9412258ab08acb0ce8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/back_insert_iterator.h not-in-frozen
- `fa97955ca43956d3c63877949028cab9a6fa1f1ad1bc6f6ce5207d31db639b37` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/bounded_iter.h not-in-frozen
- `8c58860597ae3f95c376618d0bada391840fb09881f8a1865f41ee298f8a8fd7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/common_iterator.h not-in-frozen
- `fb9068382d692d750bf10f6191e413b3a976bc5a89d787968cef672ec1abfe96` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/concepts.h not-in-frozen
- `a5a56c8802ec4812bccdb920b76196ec20a4448ff9cefe1c271278813e0bf806` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/counted_iterator.h not-in-frozen
- `f67b9a8eec672bc9cb9235ce638213af7ef7c9ac98598b55b0ecb190427db5d7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/cpp17_iterator_concepts.h not-in-frozen
- `99df77b927b5e627447375c3d9e5447f74c0c5d0ef47dbeb5eca3d4f3ac0148f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/data.h not-in-frozen
- `d5e2128f2af1c4ee592de6c8f340e81d99e49a01266a92c925f97d7427933dfb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/default_sentinel.h not-in-frozen
- `62a9501b42d6a2926e807a905cf279f5aa1c9999d711471794a32c1dc2b06367` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/distance.h not-in-frozen
- `3f7d6dd80832876a4376c85912a2db02c3d226e96958b1a101e0fad88322f668` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/empty.h not-in-frozen
- `b89716184bd6ecee76c6150f17d1c81ec2cd2d956f9a8d9ee9b613f1ca3671a0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/erase_if_container.h not-in-frozen
- `c7cabde042585544fdc10d8ade52de4f264b07d7a4bac679ada9cc7481a63d04` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/front_insert_iterator.h not-in-frozen
- `0793e3e03375bb76e9b494dd7b474ff9906b1fca78799c34cf64c00b18d74215` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/incrementable_traits.h not-in-frozen
- `d256f902eeb6b85b73111a8d0b3d891006c86ffc1d1daaa379433bb782802fcd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/indirectly_comparable.h not-in-frozen
- `d197942683382eb8d230a235e160bc50f0ba64dcace3fba3dd7de02666dce453` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/insert_iterator.h not-in-frozen
- `cf50d2186f7e92865a497d908aaf2e7158949f47ec000fd8541dbe3e7d5288eb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/istream_iterator.h not-in-frozen
- `a55db024058e469531378a33f51ac3071f5ab0d59ad75a0d90a460cf2a51dae6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/istreambuf_iterator.h not-in-frozen
- `ada2ff8995dd36b31686664757763fa702954828aca961078e3c7296986cc03a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/iter_move.h not-in-frozen
- `85ce33fb9018369b4728d8b5c5780c1ddad2c123e7679d53fdf9d5dc258e48eb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/iter_swap.h not-in-frozen
- `9012d83ca2c8cc3ae2bf680add50f24c55fdcf2760242b3f343a2d7b718d3759` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/iterator.h not-in-frozen
- `d67f2eab55d1d936d3f6b3e4defa90fad88349bc79cc943b22df709be925b1a7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/iterator_traits.h not-in-frozen
- `2f57dfaca3b486772e55c7c1a8b5bfe991938fecdae24962247c8fd52ea06056` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/iterator_with_data.h not-in-frozen
- `1fd4bb433e7427c9f9c7cc0b55aaefc62c88f0d1119578d21ab1eeebec62ce8a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/mergeable.h not-in-frozen
- `778ec9dbd2f77cf2605dad7727491f69a631c8ebafb045df08a1c7b282e3fb1d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/move_iterator.h not-in-frozen
- `4a3baccd3006f858aa0e70a7e40af8c8d5fcabb5dc0a2e9758ff882d01ddf644` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/move_sentinel.h not-in-frozen
- `b6cf70c617d914d5717a43185c2ab77f6071ddcc3e149ddcd6e4f1626cb43957` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/next.h not-in-frozen
- `29e3b4c9c832b189c15959ea7804edf2d9a6354aa6719c2777b8d2753dbb111e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/ostream_iterator.h not-in-frozen
- `90d7324dc660f9b4f12c955852870988fa5a02d54f3e3899d9b94a3d937d539a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/ostreambuf_iterator.h not-in-frozen
- `d2f6c030d310fc7e767bc281b097252e46bee52e6504f54c741024875f5243f5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/permutable.h not-in-frozen
- `ebacfd0e6cdb09eda1e3dd59758a90c24b69924c6c8b87509ccfe5659d1dc579` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/prev.h not-in-frozen
- `ce3a277b3c761ad1e67f7d0faed29a2822f9c37a3f2cdfa09b3c288a7b4bb194` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/projected.h not-in-frozen
- `65eca2c4dadfcd265ee581ee52960f62814329ba1a49505cfbab4bcfdcb2c0b9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/ranges_iterator_traits.h not-in-frozen
- `2010a8faf858510c62a085569f43711e1780f8870a8e7ba399f2652a63f270ab` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/readable_traits.h not-in-frozen
- `2e00bfdb6d7331061650b88074ea72aa1bd7bae6a0e3e215b4e54585cd16a5ac` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/reverse_access.h not-in-frozen
- `1fa200b1e5109dba62570941a29fbb801435ced1442ea53a0f69e488cb2efcac` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/reverse_iterator.h not-in-frozen
- `5386abb872fab8fdd119220e9a04f250355276fbf8ea9e51151dd6ba4ba6fcf5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/segmented_iterator.h not-in-frozen
- `20be76818a7cd310159be4c802020bb7a81c0523cfb697332492feebe87e36e1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/size.h not-in-frozen
- `65a778ed97ef601d3239d6e025e60686e501c7070a0c063dc7a8bcfc82839f40` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/sortable.h not-in-frozen
- `f23c13fe34b1f11dcab06e1cd6b79f1b9b71c9da07a6ccd83cb8f8053655f2e9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/unreachable_sentinel.h not-in-frozen
- `f584f7425b17d52720c96f76b1a48905029e4567064e4d6ca13d8da83f67dbd6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__iterator/wrap_iter.h not-in-frozen
- `50299d08dacb52d0368911a8943c8b37ba84e2f3dd12791dda75fa1d31b1e0b1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__locale not-in-frozen
- `03f1f91a0179a7b152f8ccb5df880013775a8608326082b1f398c86497974672` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__locale_dir/locale_base_api/bsd_locale_defaults.h not-in-frozen
- `8e0cc7ee582b39a1be565539ee215b7d7806ebd213861d08b91709efa81dacb3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/abs.h not-in-frozen
- `09520a5672624a1b667f47ccd5ea2f8d18253f3e7923eb0402542c95e149dbfe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/copysign.h not-in-frozen
- `fa89421a0449506b2f2f3aa41eb00bd24bd10a852b6e4c2ce6d16e90bf098efb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/error_functions.h not-in-frozen
- `a86a4eeb43ba9a7b4f04454adb5ef0ba53177ea7c0cd6f5e18b1f32121426e92` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/exponential_functions.h not-in-frozen
- `d71912a5a01f8254b97a81f028affccc9893e868ec195023c0296d2988fd2fa5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/fdim.h not-in-frozen
- `c9fa70ae1f87da30da8e301fb7c46641c8a651ddc37c54fc8e81c517e80a905d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/fma.h not-in-frozen
- `be2051df98f846118eb92308af1043dc96a6cd057cea04b754a575e0487b2c32` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/gamma.h not-in-frozen
- `c842d8cb6f6d33a03314129242f95f13fa5ca449f035f4ae99b45dfd5faaa77e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/hyperbolic_functions.h not-in-frozen
- `e3809a38c5a2c39325e8ee54bdff69a0c68bb39926e5e431fc9dca95e36fd877` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/hypot.h not-in-frozen
- `80f7bc2ced8484502f671a453885da090386e2dfd13a41afe295904764c1c819` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/inverse_hyperbolic_functions.h not-in-frozen
- `3c039120f4a26f256e85c1ddb13fae5c173b61bad296223b1a742483ccd3540c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/inverse_trigonometric_functions.h not-in-frozen
- `80f4a7ce8d0697901775692a40100e810dac7638bd404bcf5747f2a0a8c8a137` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/logarithms.h not-in-frozen
- `23728a96b0887b61a5e0280ee6351bed8822cdeef824ebfa378c1f857d535384` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/min_max.h not-in-frozen
- `c6afb0dd365680ebc0c4f4375c2f2cb6554c87c26de115335b73fc2e843c61cd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/modulo.h not-in-frozen
- `99956736101b6f32baf5bff7301e200e63a5d0179ed8b2b502b0dfee7cca0dde` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/remainder.h not-in-frozen
- `cec896998a766d183fca3a042f38beecd545eca4eda12d6f13e65aad780f7568` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/roots.h not-in-frozen
- `068b8745b9ce835da9b066c175b1d5f34432092c2fb615c8b14acdb5a2c65eff` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/rounding_functions.h not-in-frozen
- `24e6318aa83f41756d49ca1c1cf12ff17a7b0ce2b4b6dcd6dd032db3d7571141` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/traits.h not-in-frozen
- `d5f51af11c3fb5a6d7ddc66cbbd1246e99026f7e0b0edd6e3122db7ce5033df8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__math/trigonometric_functions.h not-in-frozen
- `7b2b1f5d0acdea3b61cd3433fd23fd5517dfd0413ddb5f375a8a776ea176922c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__mbstate_t.h not-in-frozen
- `75393a0fbb3e0f88cb472c7ec66bc2edc1f925811dec365c84ad147e0dd5db6a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/addressof.h not-in-frozen
- `570a85eff21632a82b906e597de824e879268d89802a3cecd0b4d01f5a18ac0e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/align.h not-in-frozen
- `c37231eac4018b3b39d361545fdd733ed5aae18f3df4da052e21c75e69041599` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/allocate_at_least.h not-in-frozen
- `5a0614cd84305372fd4e0dfa5f4b8c059eca0fbc04d919d637df4728d0ef164d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/allocation_guard.h not-in-frozen
- `dc3d6bce8bb3061ba73573031e0fe157698fcc498cf17f3e827023b00ddd844c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/allocator.h not-in-frozen
- `02b00b85c40a08af4bb3876714721d7112e6b18dba4783772f69ee5b539a6d10` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/allocator_arg_t.h not-in-frozen
- `6cce8120c596ce17aafe0d4d1529d474c50719747bf58a1ecbd436c75c570648` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/allocator_destructor.h not-in-frozen
- `19403f9742873f1f5fe704695c24edc3d76a2b5c509936d287b9bca2e25199a0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/allocator_traits.h not-in-frozen
- `de096e825a55f5f3d084294e06948b11f8e744182f7d45fd0829e7a615c1bc23` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/assume_aligned.h not-in-frozen
- `cbba65b1766358f10814deee49dec1622b8808a8f9cafaaa0abd0bbcf476a642` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/auto_ptr.h not-in-frozen
- `d8b797208a3f1f87a9456db345cb7d7a7baf5ccba46a68c81abe4888f175e041` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/builtin_new_allocator.h not-in-frozen
- `623913473ef28518c05965e901eac34e5cb1a76324ebd582e9d86ac55e71f34a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/compressed_pair.h not-in-frozen
- `206c6b7477d45f1489f29bc316d55bee23d7876120efd61f542ea8216fa1ffa3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/concepts.h not-in-frozen
- `ab9c26ed7ba596ec0009c51bb261ff2b68322e9574621ae6aefc442613f68647` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/construct_at.h not-in-frozen
- `4d1d9e233562feed73dc0395b463bfb90f6421b65de6713df6305d7a7e4daa09` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/destruct_n.h not-in-frozen
- `6f1c4bd47de3c4069e765966b881134d82b27f760bd17085dc1e9211359ebf27` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/pointer_traits.h not-in-frozen
- `91690e17e506b703f1bf1ae9d698fd85c212565119dd6e6e73770b23977440c4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/ranges_construct_at.h not-in-frozen
- `db9741658f6041bcb84fd7ad5e21db7d8c11f2927a4781d0ed5c548e76b12cd6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/ranges_uninitialized_algorithms.h not-in-frozen
- `7e1cc68e2a86399c0c88bf7f7ac64af3792aedaded860bd419d00cc2d759de5b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/raw_storage_iterator.h not-in-frozen
- `a91e6c043d242d46cb6e45de489bd0646b6c6eed6804442d79fa0a3efd051664` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/shared_ptr.h not-in-frozen
- `6bfb70ad30ec4dee07e8037b6f2c00550ef278a868f3ca7374f594f2050abfbe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/swap_allocator.h not-in-frozen
- `22c81c1341365e3e48f21b0a868a59d7daee523e07e474d3c7524f2d1ea04669` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/temp_value.h not-in-frozen
- `480179c06657b1d7ed991a5eef42aaef07fdec367f84d2c28e6e142e7194ab17` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/temporary_buffer.h not-in-frozen
- `37e7eb6ada49615067838031b9720d01e29b209bef7103ee8d44c48d1364f8ce` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/uninitialized_algorithms.h not-in-frozen
- `636d50cc8eaff7d7ad2c3ee97802daec8812d39a6517a8217ac0cb13ad35c591` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/unique_ptr.h not-in-frozen
- `1388bfb330b6492d321806be238f85cfc014747aa15eb2fd3eb56ff622518e4a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/uses_allocator.h not-in-frozen
- `f9ce816585872bd325e618aea80d3f80563e7f817cb908d47906cfcd5b009b42` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/uses_allocator_construction.h not-in-frozen
- `9eb0f45bb690f715faa9e8a74d3fbd627b7d98872ff98fdae318457b0a3fa4c1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory/voidify.h not-in-frozen
- `faf46deaf3b04f85a6bac270fb74750e3346c85892b368beb9efd2c536fa9c24` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory_resource/memory_resource.h not-in-frozen
- `457dce9f901449a741f49a9bbfc84fd04dcce2bf8db004092fec37f1077a08d6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__memory_resource/polymorphic_allocator.h not-in-frozen
- `d6e5fd5823161d5dee90d9c299c449d2696ea479580ef08c9603b376f91f356b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__mutex/lock_guard.h not-in-frozen
- `368856e2d2a0ea14b565a4ab1c3c7564db77eda6d9b5d1e46eccdf8784bb0e61` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__mutex/mutex.h not-in-frozen
- `463331a66f96ad7c0be5ea22efe4a8c1797f4d68a30fea660f57368946dd3144` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__mutex/once_flag.h not-in-frozen
- `f10a60a3dd66ca06efbfa1a60d9b0c866795dd2f8f890025775c28e45dda71e6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__mutex/tag_types.h not-in-frozen
- `b03d2e8ca1ae583971524a85d2c83a37929ff29be20c345a6b199da5a278c38c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__mutex/unique_lock.h not-in-frozen
- `533807087b7be855b407946e017b0d3fd0106f059ff0ef6c020a77d97596fda6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__node_handle not-in-frozen
- `7628c656ed2c45537ce3cf1afefcf85b5bbe7ea4ec36397f0bff51699128ca28` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/accumulate.h not-in-frozen
- `9f29e25c380513f6fc10182410b285fde33b752f9db95006a5d50beb032fd885` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/adjacent_difference.h not-in-frozen
- `7c014b25db4e22757d008aff80983dc43829330a674363a2f3f12b3ae38ec464` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/exclusive_scan.h not-in-frozen
- `2c4e3448514bae3488f4debcfd99d00aeb5891de71621faf6711a4ba7f8eeb7b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/gcd_lcm.h not-in-frozen
- `724cce224801a986b05f198208097cc5b89811e4722d01f6a66cbaa618515fba` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/inclusive_scan.h not-in-frozen
- `6fa7a71437a5c606306dbce2541524350bceff59ff3b65309a9113ccca020660` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/inner_product.h not-in-frozen
- `bf7a064ffa56c649c3fcbc6719a084a714873f5a382ca6ebb152b5b010c34236` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/iota.h not-in-frozen
- `65b7507601f121b1fca9703bcae17fb725dd034152fd8bb2cf51ca6d6665ad50` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/midpoint.h not-in-frozen
- `c86cd3a78b52247a50107f80bd03d3934dfa21dfd9441fa8fa7ae0f89d25d994` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/partial_sum.h not-in-frozen
- `c64a533383018aaa66d703173b157a1e215c7e6dda42a20b8e15c9ab7c6df4b5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/pstl_reduce.h not-in-frozen
- `5bfd58bfceb9dddc9ca90df9ef9aeba01e48ad5719d5134fa2bdb8b03be9a583` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/pstl_transform_reduce.h not-in-frozen
- `6c47d327fea3ecbf9f870db2b3eb4ab6165171f094537a89fc99272b27f1078d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/reduce.h not-in-frozen
- `3139a84c1121a7dbd0ec6d5b45bbd246b342116ba1a157316a1a03bec4bdc245` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/saturation_arithmetic.h not-in-frozen
- `243bc3a525c0e2bb2cbb51fbad27caef17444b9a041db05437218d45c94f0a6d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/transform_exclusive_scan.h not-in-frozen
- `10395bfe812a8d60ba61a8c30a78bd57d316cdf2595d03d64cdaf4604d330845` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/transform_inclusive_scan.h not-in-frozen
- `26a1dbc9b44276b1c1c7b5a9bce37389ce793d87b448283c963ec58afcd6e07d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__numeric/transform_reduce.h not-in-frozen
- `66b79ab386fc50ed464f80402f8daee50201215dc2c4f326e002e8d2a726e202` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__random/is_valid.h not-in-frozen
- `b2ff9b4b81a175db0caf367ae49d955076dcad8a8ccf6b4531eeed3d0ab000fe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__random/log2.h not-in-frozen
- `645e8508fb69121315c7cc92ebb2b4a0969cd0869a03c0a47c7761dced1c0a4b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__random/uniform_int_distribution.h not-in-frozen
- `5d87c24c2f67f4e7fbf5d6eb5fe9df31fd489c1253226a370482ad8ac84d159f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__random/uniform_random_bit_generator.h not-in-frozen
- `3ec163c6be453f2d56f601117ec8d0f8edce95fa0805ea1df5d4571b786d46c1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/access.h not-in-frozen
- `e579556af608e7e8cf8adcc71a61f1c8a42e3d0573b611df943269ab65906919` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/all.h not-in-frozen
- `d142a446a50a9b0c1cde058c90ce887bd81cb1d10015b19e1139a98e7e4c3a8f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/as_rvalue_view.h not-in-frozen
- `bd0d7de121974070b38e0a4ff1e844c035091f7feb05f76ab21a58b5b56fd26c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/chunk_by_view.h not-in-frozen
- `343ff6812c7b3ce3be90521e55e392dd453b3b2beab7da325bc2f8e231acfbd2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/common_view.h not-in-frozen
- `e3a386f35fbd5cefc19c1e0fc26dadf0aea4b721e81d33fb8a09be7f936c3d72` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/concepts.h not-in-frozen
- `e76ede621ce8d5a0d1884aecaddb94ce2104a9ac19193be2b091f408f4405f01` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/container_compatible_range.h not-in-frozen
- `0fc6c099bb89b6728ec30918cfb5e67cef01888814a9cdcbcee9935e841a4505` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/counted.h not-in-frozen
- `1e6905e7ac85ff945a9ef05feb69237ca69932cf4db7d32b90a1de4054e39a7e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/dangling.h not-in-frozen
- `ff25a7220669459ba5779e965a3cce7f33558606e1716cf8ec555d50259141a4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/data.h not-in-frozen
- `9a28ac569bcc591d0453ead308fece9e95bce3c8357c738151c7541846996d51` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/drop_view.h not-in-frozen
- `c738809203cec8cc97e5505a829e116455c6c740b0897dc67163699fc5e5d2ca` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/drop_while_view.h not-in-frozen
- `ae32a7d7c785bd617e266037d4f78a3af294875267383c0568e1f4317710abde` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/elements_view.h not-in-frozen
- `7f41e028e84f28f74182420c5424842e4694360ac6cff82a994caca9526e8164` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/empty.h not-in-frozen
- `565a445826ce1a84ce9afc873eb180d5df8a1897ba858affb991296f8909dbcc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/empty_view.h not-in-frozen
- `623ba4c99ef1fb8c8489608dbca7a619089eb100072875315e7faf1d0c7a8fe1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/enable_borrowed_range.h not-in-frozen
- `f032394ec81f0060d54955ad09a554d8906b4b2b7404de79194dbcbe3728782a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/enable_view.h not-in-frozen
- `e58aa1e7ad5ecc447cf76467f56dfa35dff7d9a49ab1c1bb0d665181333d7aa6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/filter_view.h not-in-frozen
- `02252a9d8692e5de1df740bb7526dfa8fe1317f82e53aca4b26d8bc0922541ba` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/from_range.h not-in-frozen
- `0e2be9734e6a688824b75c450dcc08a935785b577a4eee395cb43642f7c839d8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/iota_view.h not-in-frozen
- `dd9e55bed975505e8a88ea0f3d035dcb328ba43538426d04d50fdfac8912b103` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/istream_view.h not-in-frozen
- `ddb9b7af2d8654bbeea48c07a019b03b4a42a4bda8e9d56a7726252ed937cf23` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/join_view.h not-in-frozen
- `dc29a33ee169b859cc95e5a9f827bfcea3f88663eef55d7b6d8347e7cf55e465` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/lazy_split_view.h not-in-frozen
- `83ec582765ae69ff49dfc3a0378f57c0210196e70e20d64195717743442371ed` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/movable_box.h not-in-frozen
- `1c3a6e6360b123b71642b379f6bd5e1f5f7845dc38d16eee0870ea53ed4472cd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/non_propagating_cache.h not-in-frozen
- `b98a3a47ee6a21e19fde1294a497146009c1cee8348c8c5cbd3d790e58c802c4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/owning_view.h not-in-frozen
- `0e3404761110fc455a5ba037e2a3a98968fa711ebde30365cceb666fa6b88070` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/range_adaptor.h not-in-frozen
- `3493dc242f6399554c0d2daeb2c2ca85f8eac80927cb2542393b83ff05967296` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/rbegin.h not-in-frozen
- `1568f5528230468192e9182ba26c8dea07853f6050a8308f3a5541fa8ad11a18` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/ref_view.h not-in-frozen
- `758e59d984a69c36a3692fb427d2c5902e9b76685ee3e8ae77e8a1f6018a0754` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/rend.h not-in-frozen
- `6b184cb7560f38c0a75d634165b95e6fb25f004d601a2c24043ee738852345a3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/repeat_view.h not-in-frozen
- `a63002c8092612c7edb7770b6f2911c4658348fa5645d67dbc66fd3370e8dcce` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/reverse_view.h not-in-frozen
- `d7314fc98ac92c4917702b4217c52f4e2478240350e1876dd9eac5ee07282a64` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/single_view.h not-in-frozen
- `12b7e848aaed5ea4bef212abc2c6d275e5bcd778864e4835f22c4c231ca1b20a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/size.h not-in-frozen
- `0b04ad6c2e6bc1186440f3e48a26b7532263d9649d1a9052f11b2007303077b8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/split_view.h not-in-frozen
- `247f595810c19ed2a2427bb5af808c1cc6c44f5e5795995afbac0562705d693d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/subrange.h not-in-frozen
- `3b7f56608470ff9cbfa41ceeae48b83b9cb9940aeba842c24334959806d35212` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/take_view.h not-in-frozen
- `c4581fc5b500921b6fbbed7a0882a2e8c51002151df5a500dc755e591373f6b7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/take_while_view.h not-in-frozen
- `9e56f050667731ba78853dd9331b539a74e2653fd1ededb8aee3679f7f839372` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/to.h not-in-frozen
- `cf5f01de5f78b2196ae30ef74428d1d3967e7e54c651f606372aae74d9160a52` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/transform_view.h not-in-frozen
- `d99ce91d81e9e2b688081476de4464dbe099d1b53733ab6161f8e6ebced4c79a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/view_interface.h not-in-frozen
- `ff4e164c986495ba957637e686cd884df62385ba2915050e9b4d0035f7a118c5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/views.h not-in-frozen
- `44fea11708d77e6d7dbf8619a627632d1776828d1812a47dd8d6e081021e6be6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__ranges/zip_view.h not-in-frozen
- `cc2029128a30b8924a49024a5c8f96d5723c51ca5e3e4efa6d8e306b57bcd909` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__split_buffer not-in-frozen
- `2677e8a6fe4469233279818ed7ff9eb5a204faacd0011f6706f3df5a8785df86` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__std_mbstate_t.h not-in-frozen
- `d272439060aa949f2ae8ddb4787a9be1d9d3bb63106979d2712584c5cdd8fb68` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__stop_token/atomic_unique_lock.h not-in-frozen
- `9ad2933be0806d9d1a38488a3b54329f09a3daa5861ac93e9b55035ad618f3dc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__stop_token/intrusive_list_view.h not-in-frozen
- `fb133d20f829487fe77873732a10d1497e1c78c01c1e5a5acffc3d8ca7eae9a1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__stop_token/intrusive_shared_ptr.h not-in-frozen
- `b27c5adf6dd59e8e77fce6f3c4193d6ebf6993e49f49bfa7fd44ccb95d7a13a6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__stop_token/stop_source.h not-in-frozen
- `3f99abb65573fa05392e64ffa0c895e3b06e26b9a4f8cccb9edf19e2d1d74b9a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__stop_token/stop_state.h not-in-frozen
- `a006553a313f99b046d96314d7be37ce0ddece4e0269ed15ca9d94c5ffac52c2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__stop_token/stop_token.h not-in-frozen
- `0efd155e336ba8298cfebc9041b68bdc296a1fb77b5d1fe88a911bfef2231b77` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__string/char_traits.h not-in-frozen
- `de6f15b315a231abbd2536e81e79cdedbaf7a6a24336f155b6906cf179b60155` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__string/constexpr_c_functions.h not-in-frozen
- `cd91c3369519d6baf3b5c0c80a9b7a8d5c2fb5fa8ab30a731d157909c9f53efc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__string/extern_template_lists.h not-in-frozen
- `d1043bb4533294734b3ae15413a6db0a29db8c4ac8949214f1c31df9cefa7261` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__system_error/errc.h not-in-frozen
- `5c0fd4f275bdc1d425bac95c48e52ad447c9d7a6368b66c4bc756f7ad765966f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__system_error/error_category.h not-in-frozen
- `2fb6bb24d946c63d01a3313753d3c6125a4c36ee9ba6f4937b4a8dc4af747a94` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__system_error/error_code.h not-in-frozen
- `bdcfd6617a18a64bf3b9902c20975aa2c18524c4bca1741e52035eaacd2d01f3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__system_error/error_condition.h not-in-frozen
- `5ce493bc7399d93327998d86f2be9a1be7b85393fa93a76ba708c8bd382109f2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__system_error/system_error.h not-in-frozen
- `c941c644a7a99b3265a4f588f253dc2893b2153d663d85ddee7fc3d46f6fca5c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__thread/formatter.h not-in-frozen
- `a98bb33427266fdb20fa69224d1f42ddd3f24ff7e2e2a6815e38c411be5c2436` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__thread/id.h not-in-frozen
- `cd45857b5b766bb0614c3bcc04844238b0f2956000b41d35f1963f3d5f892200` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__thread/jthread.h not-in-frozen
- `a653a68946209696422b9e65554d384a73e196965ec3e63dc64feca1eac589f6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__thread/poll_with_backoff.h not-in-frozen
- `a574bf2aefefd7e4a89175de84222e6a53f0310af3211b3e8a06ff38c18417ba` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__thread/this_thread.h not-in-frozen
- `0af569ff734d74019f40fa71685009802565a6ba2fe99427d5595631ed32df31` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__thread/thread.h not-in-frozen
- `a0505e385504a29e83d7762d56b859baf18643c57144eb3eb553c4033f8e13cd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__threading_support not-in-frozen
- `813c57cdced005702b09a1218c8632a344110bbe38e22fe3dbe6c27b5a98b804` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tree not-in-frozen
- `f4a6171df04be6331edcfc4a1638a64a1143d0b9f24190978198e6b11a702ecb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/make_tuple_types.h not-in-frozen
- `fad5b7145a9e2462692f5cff1771c4afc26be951b7818ff7f81606b4d815be9d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/pair_like.h not-in-frozen
- `6b37c484a9179b0070404d0bf3adec72e2befc423b2297605242c8a31805dd4e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/sfinae_helpers.h not-in-frozen
- `a77b92f924f7d459e48dc24e40e537d5c7b65ee3da91fc00848cfbf1642b735d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/tuple_element.h not-in-frozen
- `976f5f9d6c7c1fe2516fdbe0f39c368d8a154dc4c1e6edbb3d020ccd8286eb6d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/tuple_indices.h not-in-frozen
- `9eb61b4a0fd011ab85327fff5f8c0b8ea3ea0b29fbfabc5667a868007578fe06` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/tuple_like.h not-in-frozen
- `29d0af42f1348976e792e90f0b9ef383c2f0aba907f8c8186e1a767699d7a924` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/tuple_like_ext.h not-in-frozen
- `33ce53309cced2431d82acdc17927cfbeeed8e60452cbc7bbbef5411a9b87de1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/tuple_size.h not-in-frozen
- `24ec73242f8c1a629de8c253322bd922b3d8a292095ccae59355bf1f37f299e3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__tuple/tuple_types.h not-in-frozen
- `d845e9bcd4f3d00cac47baecf54c57e575ba7a13d49d9324faf429ebe9a3180a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/add_const.h not-in-frozen
- `952af694211a8c63630e413ac207bface5f4bf8a0d4416c16765e379c5d7ae30` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/add_cv.h not-in-frozen
- `dfa167344843e9d255cab9613c3705b78a84255da32b6a6a1ed8f070c6432051` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/add_lvalue_reference.h not-in-frozen
- `454f94e5fbed5210f60979978198ec2530a3fb7b42a2235b68fb3790859f9c14` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/add_pointer.h not-in-frozen
- `6c03400107b1c98e15531933b1c9d9f83f2204f764522677c59ad26f0699f57d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/add_rvalue_reference.h not-in-frozen
- `eba2f9082f1b79ca99b377ce3e7dac22fd3c819a40a802b6d38f0233d9064ff6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/add_volatile.h not-in-frozen
- `8a82af2b61bb1acc62ef318c9965041024cf60b0e7df7934cb060592eb24683f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/aligned_storage.h not-in-frozen
- `209cfd2f62133b9c4387dffc8009d549a5aef8fdfe2c2232bef03954d5a31ae7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/aligned_union.h not-in-frozen
- `66401ea67f5af388321c517829ed6997f717173a2520015263f6ca708d6c075c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/alignment_of.h not-in-frozen
- `39adf0bba5e835f40bf7a2e6d8ff57d7435e94a9fa09a4a5d65163f276275f52` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/apply_cv.h not-in-frozen
- `b8ddc385b7c969e8deebf523d58e5a4214eae7561c41085db7cb18ba447208b2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/can_extract_key.h not-in-frozen
- `52daf4327e24ab86feb4b734fc16ebb174d5857f4d3dbac267067ead88f6a37e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/common_reference.h not-in-frozen
- `f0bd60d4f80f7e52ccf854baa37c78142e05ba5436b071cce8c10a9d4492580d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/common_type.h not-in-frozen
- `86a5d12cd1bc3eda6b42e75e902e93679dfcff1e40d2e9adc7fad4816a7b4a06` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/conditional.h not-in-frozen
- `98504e924783c9fba61b25a62587946aa6dbaabb60d7881643d49aa5d7850eb2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/conjunction.h not-in-frozen
- `072c2f27b07ad78f3cc8e961534112487117aef18f92a79b89959523ca980447` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/copy_cv.h not-in-frozen
- `27fd0eadbbffbdd45668ed82713c4c93c20bce4db0dbca732f341072b36ad76a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/copy_cvref.h not-in-frozen
- `c41ae98f05d5a620e68543a57c4c3b0a239fa8bc2a39d4aebcfb3ebc2cd261db` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/datasizeof.h not-in-frozen
- `76b75318af82496ce9d0a9d401014adacb9dc162664f94bd2f01bcfcd498a532` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/decay.h not-in-frozen
- `51d3679f2ee4dedd7eec7eeba26dbb7b7633650b2533b64f6c3a50bab6666085` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/dependent_type.h not-in-frozen
- `ff2dde80d3e78f42b128dcf6b4fe7b1173908e4276a4ef9ac3819c09f668bbb2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/disjunction.h not-in-frozen
- `c4cd379e94ec4bc829149306b89cfd4483d2c374cdcbaa51206c4c77c7b0475c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/enable_if.h not-in-frozen
- `f32c06a10fb453d272a3527dc76c1a852b46deb38674fa4f278d0c6865cae16e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/extent.h not-in-frozen
- `6bbe6a3834b731f6748b9a2cc13d9f701334b92ab9a45ca64300cb4ce2b25104` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/has_unique_object_representation.h not-in-frozen
- `698af1a37343d2c273a8db3049f081ca2dd0d7084ac470e3a081f09d2b05d700` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/has_virtual_destructor.h not-in-frozen
- `8f30676d7ad2ec95a2e9c32e7436d8b87649fdd335bb812961ca2c5d5f63b5da` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/integral_constant.h not-in-frozen
- `2f00f05cc7627856c0ae2b5badc7a44f6fc869510c6033dad63b2ae3560de675` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/invoke.h not-in-frozen
- `5f126016ffe4cf4853f8ae4a9fca903ce1566ace655baea4e3e4d5c111543518` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_abstract.h not-in-frozen
- `a2ffa4948411644bd213b6e782d2169d8b88d65a951a3c8db8593759028f7950` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_aggregate.h not-in-frozen
- `d667bae64088ad0aa5e8bd23d9a3ed156ab72e3c69921757b4dee25a218c8d21` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_allocator.h not-in-frozen
- `40f92d883f43a5753aa8960dd2815dde89aea059d774e9b66784d5fcb26179b1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_always_bitcastable.h not-in-frozen
- `758ca813cce0434050f3b1dea5fcf8fabda6c76d5d14fbd3686c98ccbe43cb03` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_arithmetic.h not-in-frozen
- `898e8e7fb250817fdd7166d0a63531c3ea2609e0456afefa81dc7841bfc023eb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_array.h not-in-frozen
- `28287c346d567c7b386013489ab488cf9b6af3e641986e15063251201e66deae` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_assignable.h not-in-frozen
- `d45f81b513886916f8cd075a79da4eac993a1c21649c23946284fc4a6178f75a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_base_of.h not-in-frozen
- `7ea63f25d10e55bcbd85a30f0dad344985364e95aa76dd8e10ea1b6542140a03` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_bounded_array.h not-in-frozen
- `92f629a5b2900dc30ccc5f06d800dd06b203cd8fead96bab3625ecfe6c35b54b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_callable.h not-in-frozen
- `c07295ab7c85e28384e227a32ef97f08be128ab428617900a0c0c9c1c9a88ceb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_char_like_type.h not-in-frozen
- `171ca46850caf8fb4a3e3210ca7a423e043648381eb5d1f75110875dfc52a7b6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_class.h not-in-frozen
- `f7001dafb04231f52be886a3728c49319d41980f03cf152ce486a34d0a4879bd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_compound.h not-in-frozen
- `de390a52c77eede7118dbad29cf8d4aba441f08290a4409fca9d6339789beeec` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_const.h not-in-frozen
- `8da9d47af89ceae85e2b395dab809cedf98c9dfa8837d96804ab449c037bd8c3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_constant_evaluated.h not-in-frozen
- `913ba1477cc199643468fbca0791c1ed389780b07f33ce4ce9d6d14217b23d6c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_constructible.h not-in-frozen
- `179ede63cace7f35f564ff6b9acccddb3e100c5a584c4666444e53e13aa7047f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_convertible.h not-in-frozen
- `44026e78f0fecff36d90510c3d66715792bfc7a188589217af8df2dd5b7eba84` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_copy_assignable.h not-in-frozen
- `b4f309a59aa12cd852a9bf5a526a2d942458815b6b9bc60728b5b3f8ae517ae7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_copy_constructible.h not-in-frozen
- `5a0c16c46791bcf4fd08678694d3f032397491e2d877efd4cbd608f3bb976aee` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_core_convertible.h not-in-frozen
- `bf76f3f2ffca47012fa50c985204b4e57495a55b5431f89f70ee1db995cc890c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_default_constructible.h not-in-frozen
- `5dfd3e83fccdc77f121150eac2b77c3ecae57f3468329bd8ace028a487bb9749` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_destructible.h not-in-frozen
- `43216828927dd2b2bf4e136ec799f9a6ecb0fd926c022b46cfa4e7f4e9d62e65` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_empty.h not-in-frozen
- `80d95aac982a0b4a2a775acdb23587ed3a1bdc51dab7b63b158dc9352152a38f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_enum.h not-in-frozen
- `283ebc0af7fdbaa465531e1a0caec658de61184c4f66ca110a0919238103dc65` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_equality_comparable.h not-in-frozen
- `0552ab80cc3c7e09931f290c6ed583e817e0ab0a2dcdba04d53ab6671ec4f94a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_execution_policy.h not-in-frozen
- `4423c2f152d983bfab2f90f78e42e94e54a4e3dd37922876a10e0676a696f731` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_final.h not-in-frozen
- `5b1ecbf4e67683cdffb556638c9acb538e71b989584cb8f05df28f40b5feb979` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_floating_point.h not-in-frozen
- `fe854b1f620aa3cc7cfcaae462409a31dd668d4f9911a09a227be7e3582016d8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_function.h not-in-frozen
- `1238c1699ac4da1d2b64a52f1b15681de49cee36794abf69d1cb304800b488e9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_fundamental.h not-in-frozen
- `38561934804fe88628862cdf37e307ddf284b337b185bf61fed7d4179c8178a6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_implicitly_default_constructible.h not-in-frozen
- `fa549316d78db81e768f024331fc4acdd5efeb1ae255db700d0642ba9434622c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_integral.h not-in-frozen
- `a6aae90731ab128e7c5b3282d2168b1abfea75439765c7a34f21dfbbd3d94162` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_literal_type.h not-in-frozen
- `dd0ffb446870c3931cfb8122b24d72b42c4cc4334b4f873c2752f594bbef09d3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_member_function_pointer.h not-in-frozen
- `3892c002623e7c6b454b0d01c96d8649930538fd22a594a59e2780a2e67f7216` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_member_object_pointer.h not-in-frozen
- `ef70a1b3708b9a9da5572ba3c624c58b93373902e4b41d79a1af86e44d7734f7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_member_pointer.h not-in-frozen
- `200fb1d28d01447fd1bd42a0c8c15ee7f7a061658742a67cd2d3bfcbf780ec35` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_move_assignable.h not-in-frozen
- `aae08e84172f4966840ac43f8c3cb7aa819031cf9969199db99cc99770c54614` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_move_constructible.h not-in-frozen
- `750d09ad84cc754982e193ee75c8a696165873739f3b14e9359ad3962362af41` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_assignable.h not-in-frozen
- `c5ec18fa68d60e911fba18b800b9ea6bd99faafe9a2082c8629c76d0c3934b69` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_constructible.h not-in-frozen
- `88c277f75b1ecf406a6a8d435ff47c86619a381ad6495102e58b20ea436fee46` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_convertible.h not-in-frozen
- `1a2ad9d4fd2c6f0b78170551a16fc37d6bb857d327b76a42ce21b99bd1454205` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_copy_assignable.h not-in-frozen
- `b3b2695fa8c1ca2c32338408f461b0084830a0a7fca970df0d7abacb1787b70a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_copy_constructible.h not-in-frozen
- `54aaa1cebab49c875e3664feec0039bc53b73870a1f17e210c8ba5d78cca0c5a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_default_constructible.h not-in-frozen
- `8dfadf42b646bc948cd4da6a8e0299a1b42ac554fdfb49f7262f2d2a7cdc96ce` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_destructible.h not-in-frozen
- `3fb8501cdf6ae108248b5fee66a4244bf8ee6ece71cc0b3b97bcd03234af6b1c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_move_assignable.h not-in-frozen
- `a08df64741e6a5e7ac047cfd973f76ec4be9cc6ac9aeca49eb7f2b9db4d083f9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_nothrow_move_constructible.h not-in-frozen
- `3fe401db8e785a0c0cf207e8146872858b9002f9f81f20eadde1e9bfb2d53342` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_null_pointer.h not-in-frozen
- `64947fb8445381c8625e671fce0155c9b53d822e9f10026bc4e0d8945e103fd2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_object.h not-in-frozen
- `0b2ff45399b0e8aba37b8dbea4f4ba14b54a7e3520623b18b8c6a712f91c793b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_pod.h not-in-frozen
- `a6cf39aa02178c44a871b40dc1e0456787c52f1dc125f64ee4233fe14aeb8164` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_pointer.h not-in-frozen
- `1260685456eb0c608be69f34f83a4542be287ee254d795397e122a3706ece59f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_polymorphic.h not-in-frozen
- `7e9becdd881c08013e302d914e8c525e1d6bdbdc64aefb4802141c6148706bd5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_primary_template.h not-in-frozen
- `cfdea4b58446a17a9dfc8b4f6e9239ea4fd718ae17ca2bb4b847381d437fb6fe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_reference.h not-in-frozen
- `ecfde04bedcd213fca791909a374dbac6c2fb0d52758bc2b4a6d914d28e5d2a9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_reference_wrapper.h not-in-frozen
- `3bf797c7d42f01ce04cdbbd3a72d1b33f6c2f7beefba117b174b822c7298451a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_referenceable.h not-in-frozen
- `c5d1e8603ae6dbef413cd130477cfed956a142d957fba33541e1784b64a67260` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_same.h not-in-frozen
- `1f70fea6e276c013b710874166cd8ed5b053eac3ab0c3bf1a4fac881c9880345` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_scalar.h not-in-frozen
- `77c0305842d6578cd70cc802d26b3a78c389c9045b683cc3bb3b08be81b1ce97` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_scoped_enum.h not-in-frozen
- `62140710f4743c59c5607b6c30fd01fa0f5cf61058c666cfecf8f7041d3a62c4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_signed.h not-in-frozen
- `74b640fc612752634a70ed15a517182009aafb158a2b43c87369cf448e6d1839` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_signed_integer.h not-in-frozen
- `97aa27b586bc556ca96394689c36fb78e2bc9cfc58863e9b2fa2df1436ef1b62` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_specialization.h not-in-frozen
- `6a2530d5c97bb0b9f2eeff70e022629aa0bd74e5c383cc765ed02f03cddd74d9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_standard_layout.h not-in-frozen
- `c86f94a7f6599095878dc88c35f1161a712e8ca7780c6d559629bfbd0d95f419` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_swappable.h not-in-frozen
- `c3a4ca6e3edf86a97e3628dd1d0e09639f274447a7976dae95bcc91d15f3587c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivial.h not-in-frozen
- `40bf035630e20e58e5e66c738025b209546be90bf9b246d68dc65019a3238dad` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_assignable.h not-in-frozen
- `821d1260ffdd7399e244991885515bfe33672a85eab03d7e106698fc290a788f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_constructible.h not-in-frozen
- `7afc724cb296d51884badfadaeccf886484b4a0ee564a53c9121c33bc9c6c7a0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_copy_assignable.h not-in-frozen
- `17070b3ef8b9739e731a4c71b80a2c7925c0ab7f13cd00a5b8d2089b99582a7f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_copy_constructible.h not-in-frozen
- `09a7f3d0155793827f7fa57893c47fca36555b0887534d15bc127ccf920a4611` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_copyable.h not-in-frozen
- `08319aa106be604ac4aac93d5f20cdd7531f182b219530d3acf5e664cae48817` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_default_constructible.h not-in-frozen
- `42465c17142e33d319b3b35d137be00f5d6b38dd47ecadeda68111918df3ebb4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_destructible.h not-in-frozen
- `4a4c3c94d6685007219f40161b57cdb2f047d32ff710c0bc7e3dec548b84fd5b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_lexicographically_comparable.h not-in-frozen
- `b3cb8eb4414246fb97f4b7016225e2ca9a977b245bb8fd67914dc65005749849` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_move_assignable.h not-in-frozen
- `8ea4de86df115c7f8553fbf43546b3dc2b9342c1b1b3c40c10bee7d474b0db1b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_trivially_move_constructible.h not-in-frozen
- `ab578b3fbdb2bf22500acc7a532498d73675756b3212a5b6f1edee5c33b502b3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_unbounded_array.h not-in-frozen
- `464094bc45de0a8f6adb083cf7c48fdf510bdc389ee93dba9f8b88126ecd30d0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_union.h not-in-frozen
- `e9af0c9b011d05917f3364bae03dd202b15e6abe685e1666dd4307d5a8bc087c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_unsigned.h not-in-frozen
- `401974a19e4d82e459a65817bd10a77d39b3db2ff94b9db302df6eefd7d1361c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_unsigned_integer.h not-in-frozen
- `a0ba5c76688683163a3818b72f29cd802e1698db56f3a1e33189dbaf7c3a52ed` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_valid_expansion.h not-in-frozen
- `175e88dd26e225b06f1e7b9687f5402f17da25d24bdc2cfbefc5928aa71df9e7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_void.h not-in-frozen
- `9eb5a7c9691c8212e7cb6ad7208c801146089723b0c74f7b36659ebfc59f6c22` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/is_volatile.h not-in-frozen
- `2c936f87731a4a3971847c54dbb99aae0c77ec5eaa4598bd9b5e7aa6219a74a2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/lazy.h not-in-frozen
- `661d1825e084ffbc90f726b5f626639592c14d256dc16b25f9911ddbbfb5ef18` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/make_32_64_or_128_bit.h not-in-frozen
- `4c005e33f080b5303dda7d1f14a2c00895e6843fafe28447397ad9ced6602482` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/make_const_lvalue_ref.h not-in-frozen
- `192efdc7abc55d38d89822c7998b1de7313cd0d0b80c6c104852932828dcd10f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/make_signed.h not-in-frozen
- `30a74609a2335c3e3869f56cd13aded9546393adc3d8313ea02afa034d8113eb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/make_unsigned.h not-in-frozen
- `ae401b5f0cd10536e294651923ffe95843fab8f2e810eecf95082c862afad8fd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/maybe_const.h not-in-frozen
- `c0a3e7a7d8a0b183cff37090645142ffe5e90b7d31ba6bf3387968f7a58ee954` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/nat.h not-in-frozen
- `df5e52b47ec3509021d7df51bb02c3ec4d28b3456e650a95161ee608153542e4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/negation.h not-in-frozen
- `a036253dab1bb809482b7cf5f7ecae03bdfecde9ba69be987f14cfe854a2091a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/noexcept_move_assign_container.h not-in-frozen
- `e94189af5fb5e51de5cc9d4d5cc52b27f01adcdfe8693aa14ab58704443722d0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/operation_traits.h not-in-frozen
- `9e45a2140c2fbacd0d2189892c6c53073f3ad9b9bb151b0aed6b0d2fc4bdcf12` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/promote.h not-in-frozen
- `a3974b957e18b3f2f5784ea4098c12766ec3a1e1fa698b71d89d60112db17ec4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/rank.h not-in-frozen
- `969df001bd69aa5b2341863dbfdba5b2f92e2b6d757755ac813ca63f8a0c21ab` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_all_extents.h not-in-frozen
- `0520ffc8ef1769b35e91606d26bc643ff1582b119b001c70a96688fdaa1e1291` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_const.h not-in-frozen
- `91488531c3fbc27b166f10ca6902c938774b68c498e9b790a660610ff7f9e5db` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_const_ref.h not-in-frozen
- `44763753fff880ecf50a9fcfa3bc8d6ea6eabf8f954d85edffb51ab40fb5f91c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_cv.h not-in-frozen
- `906ff5888f26c42999ab2773286f4614eb20774c5cbb7aaf4fdafba77839938d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_cvref.h not-in-frozen
- `c931a9f64376d1cfb37961019496ae40c8a5bed229f2087e8d80737710583e90` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_extent.h not-in-frozen
- `232d79f1fe556617617479c59710ea989582fb5767a515c61d41a0a6508b230a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_pointer.h not-in-frozen
- `4aea748758ee1130a9c8d5e265a8820b30397accb00b8caad243d58c2a84e4f2` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_reference.h not-in-frozen
- `4febe25c887d6d85b21337b2f35a65a823addd53f16958ee0314c879242ec21e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/remove_volatile.h not-in-frozen
- `abf8126d36d5c2bd98ca8ae42b8215826100b66be4761efc5858fae0094f29c5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/result_of.h not-in-frozen
- `00fab804c9b7e9293e2deb3468020ef4bb8f5dc144be221dbabd09e06b2b9aa5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/strip_signature.h not-in-frozen
- `85ac78ed0193970cab40c506da2a4b78f5a4ceda125c80d47c96f72049808bcb` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/type_identity.h not-in-frozen
- `c19000ff49db91f56c8914653afe114d29453f305441e89166a39a35f6352b50` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/type_list.h not-in-frozen
- `8d848819a3ec453c94ac1a83a0c34f1f95d4a5daae07627bdb71d26d2c8c8450` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/underlying_type.h not-in-frozen
- `57b527214d119db2f99f32782f1a9953013a4e510691fa86f949a5a09e14140b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/unwrap_ref.h not-in-frozen
- `25cbade833223c6b649dbe9b411456aba1786812bc9092cea031fa42a0f787bd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__type_traits/void_t.h not-in-frozen
- `10c542f9493fc3f2ed46e498cda2c8d408ecc578ceef1df9272908143bd22b8f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__undef_macros not-in-frozen
- `6d6fc462c154a1969c28a92821ed8bf24b0c89d3bc472f30287ad62b20279d70` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/as_const.h not-in-frozen
- `633cb4815f856517d39ed5bedf7419dad1ef107823e30b62ccdb315ce39e2960` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/as_lvalue.h not-in-frozen
- `03387eccc20273c433c5f1ab17cfa4d5b6ec9cfed096f6b8a2b651e3f3f12b9d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/auto_cast.h not-in-frozen
- `7eed09c67eb5f6933fe65bdcc30d487baa82a4356e31042cbcf0b81a40ba20a8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/cmp.h not-in-frozen
- `f621a6704d4ac2498b752e82eff62c148c28504563c9aa41ed0617daba86f1aa` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/convert_to_integral.h not-in-frozen
- `4d7bc6b962cf0b3c43f8125c9b4e30bc5237f9b400d3792814c13a86c5a3cf75` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/declval.h not-in-frozen
- `22301ff062605f9dc3d716ca1dbcd3c79657d1d26f873de1e2543a1c94c39e17` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/empty.h not-in-frozen
- `db886c45c918764b43e56a0cd4738379f2a21b2a9ab5dca2fc4b0fbf7ea672be` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/exception_guard.h not-in-frozen
- `71369e598464e802dbffaa746a31db86b2c845cbdcdaab9e044f7e261fa45b47` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/exchange.h not-in-frozen
- `e6e102fccf43394c71319ac98cd4d425e2a03cbf7a7172bfd3e2defe77cc1364` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/forward.h not-in-frozen
- `f78f8731af078ca5d2659f41e57669c3e05079c6078987fa7b2a831f9525466c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/forward_like.h not-in-frozen
- `deaa75d14c24ca4965d777928058398cc94170db1ed4f7a882c4cee7f3c5d3a1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/in_place.h not-in-frozen
- `3ad43e68c597f0ff7aaced3a6671e09e17a5ad6e6a2a63f503988de53c0fb23e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/integer_sequence.h not-in-frozen
- `fee12266044f2517014a10b030f13420919a2090a2aa82a8efea78ff55adba30` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/is_pointer_in_range.h not-in-frozen
- `41514bebe2fbdc013a0e0abc5e16446a1c724caf687402e724226d9bfe95fd9c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/move.h not-in-frozen
- `8c440af82708e309b73eeaa5f4341aa88839f110a67c8abbf18dc09fa7000b3e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/no_destroy.h not-in-frozen
- `833c64ed6631ba6c0cb2444c57d3499ea74101fb24b6ed0777c1b602e3d89699` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/pair.h not-in-frozen
- `2477f8de88a6a87d87293eab9d3895c4c5acf8a3c139e19918b40b46216b2c03` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/piecewise_construct.h not-in-frozen
- `54c1c134161ece1281452b44d07155fa0b027b7869d03cd0081d3a3c08d12f8a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/priority_tag.h not-in-frozen
- `f40f4cac17c859b3a4ffc0078c73606face4778bef41a2ad5ca8c4927de5c871` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/rel_ops.h not-in-frozen
- `684f388503ca8303d2b10bd7cf43e594f5ee488b825ac60b9749133fba8cc251` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/swap.h not-in-frozen
- `c45d6d868f8ab1148d0b96dd5f7527ad90a59e6fab32ac8f189d005191e5ac21` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/to_underlying.h not-in-frozen
- `139668638246d1a23af5a76d22ae9a74a417711e4acf10ae4d00149aad107fd4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__utility/unreachable.h not-in-frozen
- `a54b92611782a470045a40fbcac497ea2968c764da4e4d9faf9a18977d7f4ef8` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__variant/monostate.h not-in-frozen
- `54458673e0a202005504c258e2276f192393b7f9b2418b983c1eea7320c16b84` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/__verbose_abort not-in-frozen
- `10fcae4cba5fe8449f95f9382b10b750790cba9e1814dbb5b2db6fd661f52f3a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/algorithm not-in-frozen
- `e4de062b982d0e79f07ec92b0df871568bdc908682be89cad0328e48429c1192` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/array not-in-frozen
- `db24c0f43ea2bf8c1957b5fabb7cbf6251fc457045cb134f54627aaea0d765b4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/atomic not-in-frozen
- `6bbe9dd5d5e1794dfd96ec55a0c7a2eb86a340b8bf5c21a24b4fd81e4ac4440d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/bit not-in-frozen
- `7b535c5a07b30d450280601a7c08b4656cd3088ab6790a629880bdb512754548` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/bitset not-in-frozen
- `be178c02b11e330e425b4a1ea5945625eb7c9e413b24f81641f3033695a10c77` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cassert not-in-frozen
- `c303867bc11815880818ed5925a0fb95dae87357914b6e6dc32ccbfff0dbbcbe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cctype not-in-frozen
- `af46248e725460a483d9c961c9604e99f064cf9dc1a02be0b7aa573b9372ecfc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cerrno not-in-frozen
- `29f799b24be95f356287076fb6c442fedc9a4fbaa163a8bf298d8653df304f3b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/chrono not-in-frozen
- `1c3d68cc31528a274ec170c4714c8c0971daf0bcda24aa6546f9ae82d3c57846` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/climits not-in-frozen
- `1f402b56b9e2bbbc64b77dc78a3db6ad77825c6075b4e84ef82b4ff1a5e0e71e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/clocale not-in-frozen
- `6442e92e3a5fb99fb5891948b160e9298d2cf9f02f4434f1603239d99d342acd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cmath not-in-frozen
- `9eae53a029b7e503dfbf8ea5fc905bb0b0d81528ad5fcce259551346bf9eb760` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/codecvt not-in-frozen
- `2ca74820a08e14af9c628b74cedda894f34724828207f6cd0af7aece293d1ba1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/compare not-in-frozen
- `bbc614e5bb4c0054742ea6ea46206e57e893c76764eb98929228f2c9b77404d4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/concepts not-in-frozen
- `e42b572d5fd62fd7c952cc9dfcf3af35676a4a82c11f460579bba9db1b4e3ac1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/coroutine not-in-frozen
- `66c4a1201792d555c87c3c4d80d65aefc8fd14566621cd1e03565521b4d0cc72` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cstddef not-in-frozen
- `47fcffb6aed3035ff2257fa7484a00413bece28c88d9e21d4592c78859df10b7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cstdint not-in-frozen
- `1e0ac0f7eb1d2cb8de11513e820d8696c878499dee262d775e96a8eb89036741` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cstdio not-in-frozen
- `c550d363567e4d837aeccb8b5a440fca6e2fb50c765ca62187274db4e23523fc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cstdlib not-in-frozen
- `7f45657e238d429210d20d93f7c8a8307cde355b8929c08477217fbdbdbe8ed7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cstring not-in-frozen
- `98bee4fce050f8b99df2038fd9ecafe257e60576aa944259dec7fc2f51e9edef` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/ctime not-in-frozen
- `fd4b7f1d8335049cb28ed36e84afe73d9dc4faae2114f5e722df7eaabb56890e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/ctype.h not-in-frozen
- `b4e2f3e98fc8cae1f4ea7e18d36ff0b8337415a18344af4782631947a9ebe589` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cwchar not-in-frozen
- `d60d207b41380bdafc77892a390d99e22675d5ee5a840dd2a04a21912c09021e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/cwctype not-in-frozen
- `96e2d7292d37ebd639d56f4c94788e4f2b7513cd0990078ca9c52174fcfb6e4d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/deque not-in-frozen
- `93b5251149cc2ff8d4929388c73311d4b55bd55b38dd39b9f08fcd840899544e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/errno.h not-in-frozen
- `ec9f8c314af6ef7cfa52eaadf3e14da10b361e72ac76983d383e90a70c718c1d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/execution not-in-frozen
- `b5ec8beed98efb6504ccb7c62d80f6e57ac80c3c66512483ab7720373bfff40d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/fenv.h not-in-frozen
- `5e900de5c4b146a5cb76297d95014fe38d8e90b56dde2bfc748768a7e28667b7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/filesystem not-in-frozen
- `7d90830ee185b3c80a5a42b680d622ebe2f06d9b63bfc0e126d6515a85781a5e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/float.h not-in-frozen
- `2ff0a72e11f19647503f89d980868433925ca8526651e9240ebe2e7bb8432271` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/format not-in-frozen
- `8abf2469b106ddb234812b7428924f1088fe60d50cb08821357ddb4a19798d58` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/fstream not-in-frozen
- `d5b8f88e44c508c5b191ffe790e94cd6f879e32548d5470b36b8268b59535aec` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/functional not-in-frozen
- `83d58077a394f9c4417ab9979ccbefdf6aa9b1067a9855acbbc3ecc0dbbc0795` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/initializer_list not-in-frozen
- `5fd57a37fad1d48e771cf6adca44842b943cc562a5ec2f886d6cbc445e309fa7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/iomanip not-in-frozen
- `c89929fd6e41dd31ec392cb636bf4ff3a3b6165f39d538686ac6fabe460a93c6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/ios not-in-frozen
- `54d262a5a2f5eadbda0d90c5cc5452b2d60ada028d3d75eba7da64f10eabc6a3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/iosfwd not-in-frozen
- `b2161a82c1052c945805baa387056760d70bd7081ee52ec0593f7f1627a23d83` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/istream not-in-frozen
- `5aab7089cf0711378bc6e40c84fa6c622e0ead4a28ece35834397249c029923c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/iterator not-in-frozen
- `bed85d9db085e260b98c0b0978523c1878de98c6419741617252c485302cd596` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/limits not-in-frozen
- `8c53f1c128fceba7e4bc35114592c6312436b09eca0c19c75c82afc279e8f5d6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/locale not-in-frozen
- `8b94ff936e68f21199628711b608a97ffb8b4909234e42c7155a2cf844a0060e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/locale.h not-in-frozen
- `553ac3b1c9656b993628664690c396cb636dbc1e29b40e4cb8600605f17ac8cf` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/map not-in-frozen
- `e38bfd2beb19ec42cb716ddb2e4fd1333ca8a0cb6ebd09351c3ff811958f0201` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/math.h not-in-frozen
- `80436a74ea01cb9c562e1daf3b0228efcd624ffa8b9ea69a30a3790a0c231809` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/memory not-in-frozen
- `f8944409e1066c9218917aeb401179ec7047f65a83ff913de5253947512d7bc0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/mutex not-in-frozen
- `cfbe1d6d8d579317867e472bf1d1f97abbc3bf93b1e0b153a1bb4722104763e4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/new not-in-frozen
- `1e671f58d6aeb7b4dde06228d1b68d11e0b52f504f01b35009ec57c768379a0c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/numeric not-in-frozen
- `b93a94b2070793f46020a0195eb928c79fb3d009e675790e9cf055809c300360` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/optional not-in-frozen
- `1c6e23d536834db36399849b97d92374ac7b890b0879139f292b03e08675c7d7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/ostream not-in-frozen
- `5d6e35aee54086ef9fd75f7dd68a51689c529825d3add64c7359f4bd75e8d140` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/print not-in-frozen
- `b7662afc3a4e4f9f827099368c810be1661c869c8cd9b9c5181ec099a61d2704` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/queue not-in-frozen
- `16663df113af7f919cf2ce02bba5a269c9b7fa80d928461dfebb0b4ddc2e515e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/ranges not-in-frozen
- `bef006d21bdbfeb7b4ca017a447e1efba0f60d3f7fc370c3daee681e9e094dab` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/ratio not-in-frozen
- `36805cface1728b3de4d595404f52cb57a26defe65143974c803aaedcfddf831` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/regex not-in-frozen
- `a750ddfab1dce7e0ad41df416a45780e7718262babf4d5228bb557906b11c9c4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/set not-in-frozen
- `3e1eeb0e42272f7539b081af433b083c8339e1e4e75f943557d2769993420834` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/shared_mutex not-in-frozen
- `812d4f7afdc4b2ffc93a03d0fe4a070700dffa5ab8eaa30c54ae4466f8d18346` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/source_location not-in-frozen
- `32c83fa1d30cb683acf5ece7d01b23719bf03fedd462f5a325e80507bba66122` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/span not-in-frozen
- `75b63dc835fcb6e87e946dd2629cc8c7db42e8e534e761387bb6e4d08b1e1fbe` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/sstream not-in-frozen
- `2ed60bac04d3a7c614830f6d5b080754b5c6003dafb25b80e5b4845a69f7ee9c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/stack not-in-frozen
- `8884ee009ba0eceb6c2d99d99acddad09b10337cb565ecdf010c8edfd314dd83` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/stddef.h not-in-frozen
- `78f7217d50987026400d99926d024bf8e0b6d56152dda16921f38f3338723d6e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/stdexcept not-in-frozen
- `66985c3cf78ab7f1d50e95d74682b1697a99a79a4fadee076f023aaae7fd0913` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/stdint.h not-in-frozen
- `5c2ba393e137673da4f13bb46761d5bd31c53782079a8fd87a7d79b5682edd6b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/stdio.h not-in-frozen
- `3220d1e7a6009834ac31ef45aad62a8adb6b6774d5ab48984092f183c264ba94` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/stdlib.h not-in-frozen
- `806f40084cbd6bab843f1af27b325e74d13465c909db054cbf11bdacede9bc11` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/streambuf not-in-frozen
- `32bca140aadc0dd8b1c9bda577e6fa2ca70231b49074d56d713ddd9f43fd0974` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/string not-in-frozen
- `9416631e4584d9e1afc557835adee34d5c6054e7c9781f5e7e98ed0568516e02` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/string.h not-in-frozen
- `fb6e042370f8612242eeb1d743e78eeb545f967e5f26ac4f61f52e28bd7d7d49` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/string_view not-in-frozen
- `9b1c5f88f2fadc6e58b55057a4defc33103bdd33f860d59b5b43645534ec62a7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/system_error not-in-frozen
- `fcaeb8ae6e5236fd68c7791061fc454a4ec6c47abd82fd2bcc8e636afdb43eb9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/thread not-in-frozen
- `2896a9dc80b84891a67fe5cb00d4e9ac99b5e081699973f5750e187fd7e215e5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/tuple not-in-frozen
- `f83fd2a36aecd356d0b833ad7327b07a3bbd744da6486e4e902a524e624267f3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/type_traits not-in-frozen
- `74c7fbdbc52ff1f1f2bdcbc16b4bbaa02ab0500641b3699633eb23d7358e3891` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/typeinfo not-in-frozen
- `a53464b48771abab1750ad2580c8a44e63be00e861ba677e38d9bc856ce80672` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/unordered_map not-in-frozen
- `5e641fb50f0f2136e95699ea859e80041a34acf3fff1c8f60a6fd35658e67ff9` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/unordered_set not-in-frozen
- `29e34135edec7a97b302c127d38abf257d1eaa333683c1bd2e039e846b3eb1de` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/utility not-in-frozen
- `236eba896e5ab622e0907ec2721abc0614d126737a5b172f186ac2885d3670d7` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/variant not-in-frozen
- `154ff0c5b67dc93cec4ee1aa8b8fa825f2007b820fb3db6b0da2ad40e6c088ca` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/vector not-in-frozen
- `00c9c30e6c912482e90ab190b5fac5f27f334943e84574e75616739f4e712a01` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/version not-in-frozen
- `f66255106679f76b3c6484367a61dd23ea9c0f2e459d2219f3c6b67d5e0c5633` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/wchar.h not-in-frozen
- `49aeb2704c881d2bdb0fc19596b77ba1a3fb90d30d2629088ce10a45a491c838` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/c++/v1/wctype.h not-in-frozen
- `65142b9f8347ced336de54a828541ef6381cd4bff82e192a0a2f5347c846c1e5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/ctype.h not-in-frozen
- `1fefa8eba3a0eaec642e0fca02f9a7e0535cc44b0b389fb90fd84e6e41ac3bb1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/errno.h not-in-frozen
- `3364eff01c85c6c3654515dc95ec4b021d1928f9e9ed18aa73a8f750fe998506` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/fenv.h not-in-frozen
- `04d020e33bc494240bb503671f4ae351dd1d2e32a479156d28038187926c06a6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/float.h not-in-frozen
- `f9bff5aa3bcfed3e7fc75d8b5534b4252e8d9a497a0947ba511bd70b89b6b5a1` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/ieeefp.h not-in-frozen
- `bb02d828e5ba08d401eab0eb2cde85871ce178dc65f04e02211f50c3252f09f0` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/limits.h not-in-frozen
- `edce59919b3a83c4c55e74e0ca04c7143fb07aa5a4b6a81237a99d05fc6268cd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/locale.h not-in-frozen
- `def93e9415d66750ae3c00ada3feaa1e61f8bf8524c7d491adc743869fe82acc` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/machine/_limits.h not-in-frozen
- `b26f452e7ee438f154ebef9de66793869e62101f36eb26017726d871e3607068` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/machine/_stdint.h not-in-frozen
- `28a99920e25187f2db6bbf815b21e3c956a2d30cec62fef8343fcbf4f5abfd16` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/machine/_types.h not-in-frozen
- `1f50385d15244a95cb2d5ad34c81667e6ec01124587f8d35b97578261aa8b72c` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/machine/endian.h not-in-frozen
- `a90e0e4edb038386c61026cf34c729494229b944cb24e04847ede8c4ca8c1d43` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/machine/ieeefp.h not-in-frozen
- `e25b39974943af81a2dc67781547304b4eb65e95b5090a1baa81332b115c88df` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/math.h not-in-frozen
- `9067980f9cf42c852ea9b097b95fc6a20e3c76723aa82221797bb894a45d2223` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/nl_types.h not-in-frozen
- `b8c84af542d20ed52124d7aa7df65a3b0c50ce8030dc5f54af0465115750fa18` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/pthread.h not-in-frozen
- `2171252d2d1e1bd529bb7a50702c9a4044d9710953d2da9a52f923584e5fee21` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/runetype.h not-in-frozen
- `84d333b2a249a3bb6ecf132024c014e9f83ac91c65ad2ce032106064c6484b21` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sched.h not-in-frozen
- `3647906a70897141edc23de99498b002ce767a054e7061f94c1cf2d435d1d440` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/stdarg.h not-in-frozen
- `f9b87dcef55880afd5c33d06c6da4e7621b3b86253096310139137ae38e75f69` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/stddef.h not-in-frozen
- `7959fedbccd25ff491378e21adde00ff30a4f114abfd6919b1f280503eb59174` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/stdint.h not-in-frozen
- `1e0b639eb07c7098f6618b782df76cef37356f1d8d3e1fb7fdeebae9f36b6618` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/stdio.h not-in-frozen
- `13f957e42e795b6c364959cf239c63daf6cab204349fdae7b5be772babab6a82` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/stdlib.h not-in-frozen
- `f87d89ae675a01333634a9965562778cbc26abce696ef304266f3a2efae98693` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/string.h not-in-frozen
- `609dd67f6e79412d92874df5968ec8e359fbb62f85c658d36a1fe9eaebf24fe6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/strings.h not-in-frozen
- `a99eb389e85e6670342c944eec802a42d65044c684100f1a4960a3778d34a97b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_null.h not-in-frozen
- `116216248126e80b1b727666c3a63ff76dc25df52482053516af7753beb73d3f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_pthreadtypes.h not-in-frozen
- `910da11ab45558cb313fc9095c2270a6ee5ed59dd7cbad4fcffb848a38030c5d` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_sigset.h not-in-frozen
- `67671ac35e1f81ecf45e65bc67c7f9cff646a35f1d42a13ac89319fac0d6161a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_stdint.h not-in-frozen
- `ffbc661bc94d6cb7f792620195d27e632f8a8ffbdf50be34eee6ccc6043b32aa` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_timespec.h not-in-frozen
- `5be80e2278ab4e0f29e4fd8af30dc28159926f059f82513b68d4acab890c5074` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_timeval.h not-in-frozen
- `2f8550bf75580c2685100d3da694da2bd8fa162a19bc94055568bf580b75ce93` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/_types.h not-in-frozen
- `4d57a1bc5c6cd4b0b0ae8d2d4d1486f868c03aeea02049610a49ce65a1f005b3` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/cdefs.h not-in-frozen
- `9e5af188cfbd9ca8d3d514f92e4b3ef218a018f66fab4d29f7c8fc81c0f248a4` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/limits.h not-in-frozen
- `705983f0088caabacf0d9ea3b5ec1550e96231a79a223f5945644fdfa78e0425` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/select.h not-in-frozen
- `109cc538df651767d5ce07b5f38be42e864fd0c53fdf2496c468f77eb0374f5f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/syslimits.h not-in-frozen
- `3541c41894e5357a509224cc4f168ad0d2d36fd12396e81c8cf949735d42cf33` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/timespec.h not-in-frozen
- `8c3c5962287dd7f23683f8c77563f79010f5eb874280953d852e07dab7de6fdd` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/sys/types.h not-in-frozen
- `1d3518b38a3ac3cd40308702e42ab7939fcad4357f58d85f5e12d91860fc6665` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/time.h not-in-frozen
- `a69fe86ee9267af53ededa0b3013dd2e62bc835ca5a6cbde151f78389063f20b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/unistd.h not-in-frozen
- `f6b24ec722ea6314b1f53ab0fadea79bd4e67c423fbb450acfcb1983badab75f` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/wchar.h not-in-frozen
- `eae6171a4166b4cf3331b3c55820b040d760186f249aa629f791effaa5c48763` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/wctype.h not-in-frozen
- `46e8ac5bd498f07c0ad68452f4bc8675d6cb26fb375fd22251541cc9e30760de` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/x86/_limits.h not-in-frozen
- `bf8f518683a8c0d6aa269dfabf473d4d8840050cf9ebcbf58f6f9cb360a145ff` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/x86/_stdint.h not-in-frozen
- `65976701a171449e759e8b53f491fb3df47a098f8d80eb1bda0213b5e19f4131` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/x86/_types.h not-in-frozen
- `786900a84764b9742e3050aafba4fc479d07c73a6b0c998bd93f8c2b0dba5ee6` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/x86/endian.h not-in-frozen
- `ff466dc397350f87f58f34505f5842a5c3a26bae48dd6c227ba5fd97c721dc84` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/x86/float.h not-in-frozen
- `6967beeb289abdc3574d1d629bc945a4df31ef05f3f7631b27a456ce9f910840` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/x86/stdarg.h not-in-frozen
- `838adfd14ad534f79118b07cb2147011550bb30634ffdb71d2452939dec3e3b5` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale.h not-in-frozen
- `44043cb01fa086f5b259478121db3be34961c03861fd9f42a9eda38ac343ca79` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_ctype.h not-in-frozen
- `9eb4d8e7c388c283adc8d198937449ccd35f1871e96a212ea6e5bfe05ae2920a` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_locale.h not-in-frozen
- `c1c9038cc79e08ae75b995c8a9c19692f6ceda5d7ce301e20e33d5bafb82a482` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_stdio.h not-in-frozen
- `bcd5862ca3d97b15057e2e5004874d3efce132d57984120abc462bac5a2929ff` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_stdlib.h not-in-frozen
- `6f24ae28a1a56883dd004b56738ff4fc809fe3485103ef1a0160b36982d5c873` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_string.h not-in-frozen
- `b05d977c9ac2d8a76d94093f10b2e6aaf69e1be237baf3711b8798638b994d4b` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_strings.h not-in-frozen
- `2f99046ab296d903a932c47e35285143b291b1f6263a1d8a8844d1dfe662672e` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_time.h not-in-frozen
- `b1ba22d6ef75ac6533d69c298722bde3897d8974673e60bc753472b3a4459942` outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/include/xlocale/_wchar.h not-in-frozen
