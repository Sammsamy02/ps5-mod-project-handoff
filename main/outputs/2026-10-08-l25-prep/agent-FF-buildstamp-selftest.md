# Agent FF — [BUILD] stamp consumer + toolchain self-test (l26 prep)

Date: 2026-10-08 (agent-FF). Scope per Claude build-discipline directive:
(a) [BUILD] stamp as the first log line (build id + change ids + snapshot id —
snapshot id supplied by agent FG's frozen tar), and the two in-binary
toolchain self-test lines that settle the short-wchar vs prebuilt-libc++
question on console.

Tree: `outputs/2026-10-08-l26-runtime/typescan-r4/candidate/` (not a git repo).

## Files touched (exhaustive)

NEW `candidate/include/BuildStamp.hpp` (25 lines)
NEW `candidate/src/BuildStamp.cpp` (72 lines)
NEW `candidate/include/ToolchainSelfTest.hpp` (26 lines)
NEW `candidate/src/ToolchainSelfTest.cpp` (134 lines)
EDIT `candidate/src/UnrealInitializer.cpp` — two small insertions only:
  - `src/UnrealInitializer.cpp:7-8` — includes (after the existing quoted
    include block at lines 1-4): `#include "../include/BuildStamp.hpp"` and
    `#include "../include/ToolchainSelfTest.hpp"`. File-relative because
    `candidate/include` itself is NOT on the build's -I set (only its
    subdirectories are — verified against
    `l26-A.lfb-203415/pass1/objects/*.command.json`).
  - `src/UnrealInitializer.cpp:2534-2549` — inside `Initialize(...)` (function
    now begins at line 2525), inserted after the `GuardStartupScope` block and
    BEFORE the `bPreInitCompleted`/`PreInitialize`/`ScanGame` path, i.e. before
    every log call the initializer can make (ScanGame's `[Seeds]` arming line,
    the `[Seeds][L14]/[L15]/[L18]` admission lines, `[BuildProfile]`, and all
    later init lines):
      2534-2546  [BUILD] emission — `Output::send(Unicode::DecodeUtf8(
                 fmt::format("{}\n", RC::kBuildStamp)))`, narrow-first
                 (send_narrow_diag doctrine), whole thing try/catch'd with a
                 wide-literal fallback line; cannot throw out.
      2549       `try { RC_toolchain_self_test(); } catch (...) {}` —
                 immediately after the stamp line; extern "C" callee is
                 itself fully try/catch'd, no throw escape (double barrier).

Nothing else touched: CHANGES.json, drivers, build scripts, all untouched.

## Ordering caveat (honest scope note)

`UE4SSProgram.cpp` prints its own banner lines (e.g. "UE4SS - v..." at its
line ~350) BEFORE it calls `UnrealInitializer::Initialize`. My edit scope was
restricted to UnrealInitializer.cpp, so the stamp is the first line of the
INITIALIZER startup (everything from Initialize onward — all [Seeds]/[L1x]/
[BuildProfile]/[ST] lines), not literally the process's first byte of log.
If the program banner must also be preceded, that is a one-move edit in
UE4SSProgram.cpp for whoever owns it.

## Markers declared (grep targets for the console log)

- `[BUILD] id=<..> changes=<..> snapshot=<..>` — emitted once at
  UnrealInitializer.cpp:2540. Fallback (generated header absent):
  `[BUILD] id=UNKNOWN changes=UNKNOWN snapshot=UNKNOWN (generated header
  missing — do not upload)` (em-dash U+2014, encoded as explicit
  \xE2\x80\x94 bytes in the literal). Widen-failure fallback:
  `[BUILD] id=UNKNOWN changes=UNKNOWN snapshot=UNKNOWN (stamp widen failed)`.
- `[ST] wcslen-16units expect=16 got=<n> stride=<sizeof(wchar_t)>` — expect
  got=16 stride=2; a 4-byte-stride wcslen returns 8 (pairs the 16 units).
- `[ST] wstring-rt size=<n> content-match=<yes/no> raw-first8=<hex bytes>` —
  expect size=19, content-match=yes (assign/push_back/append round trip of
  u"PlayerController"+`X`+`YZ`); raw-first8 prints the first 8 payload bytes
  as hex so a 4-byte-stride corruption is NAMED, not just flagged
  (sanity-gated: bytes are only read when data!=null && 4<=size<=4096, so a
  corrupt size can never fault the probe).
- Failure modes: `[ST] wcslen-16units FAIL exception` /
  `[ST] wstring-rt FAIL exception` / non-Prospero TU:
  `[ST] toolchain self-test skipped (non-Prospero TU)`.

## Probe mechanics (as directed, plus minimal hardening)

wcslen: `wchar_t buf[64]`; hand-loop copy of the 16 units of
u"PlayerController", then 48 `L'\0'` sentinels (either stride terminates
in-bounds). Runtime-call forcing: the directive's `volatile size_t got =
std::wcslen(p)` PLUS one strengthening indirection `wchar_t* volatile p =
buf` (volatile pointer read hides the buffer identity from constant folding).
static_assert(sizeof(wchar_t)==2) under __PROSPERO__ documents the ABI the
probe assumes (see note below — Prospero's native wchar_t is 2 bytes; the
defect under test is the PREBUILT libc++ BINARY's 4-byte-stride
instantiations, per the seam-fix evidence in
`candidate/include/Helpers/Utf16Utf8.hpp`).

wstring: `std::wstring w; w.assign(L"PlayerController"); w.push_back(L'X');
w.append(L"YZ");` then hand-walk of `w.data()` as char16 against a hand-built
19-unit expected image (same hand-loop discipline — the ONLY wide-libc calls
in the probe are the deliberate experiment; everywhere else the tree avoids
wide libc).

## FG contract (GeneratedBuildStamp.hpp)

`BuildStamp.cpp` resolves the generated header by `__has_include` in this
order: `<GeneratedBuildStamp.hpp>` (build put candidate/include or the
generated dir on -I — the directive's named form), then
`"GeneratedBuildStamp.hpp"` (same-dir/any -I), then
`"../include/GeneratedBuildStamp.hpp"` (FG's documented location
`candidate/include/`, file-relative — works with ZERO -I changes). When none
exists the tree compiles today with the UNKNOWN fallback. When present, the
header must define narrow STRING LITERALS: primary names `RC_BUILD_ID` /
`RC_CHANGE_IDS` / `RC_SNAPSHOT_ID` (bare `BUILD_ID` / `CHANGE_IDS` /
`SNAPSHOT_ID` also accepted). A present-but-misnamed header is a deliberate
`#error` at compile time — never a silent UNKNOWN upload. Until FG's
generator lands, new TUs BuildStamp.cpp/ToolchainSelfTest.cpp must also be
added to the build's TU list by the build integrator (drivers untouched here
per rules).

## Syntax evidence

Checker: `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/bin/prospero-clang++`
(flags copied from `2026-10-08-l26-build-variants/l26-A.lfb-203415/pass1/objects/`
command receipts; `-fsyntax-only` appended; the receipts' stale
`-ivfsoverlay .../l26-A/headers.vfs.json` path repaired to the same file at
its current `l26-A.lfb-203415/` location; toolchain env per build lineage:
`LLVM_CONFIG=<...>/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config`,
that bin prepended to PATH — mirrors `archive-20261010T025518/.../lineage/build_local.py`).

- `candidate/src/BuildStamp.cpp` — flags from `cppmod-base.command.json`:
  exit=0, zero diagnostics.
- `candidate/src/ToolchainSelfTest.cpp` — flags from `cppmod-base.command.json`:
  exit=0, zero diagnostics.
- `candidate/src/UnrealInitializer.cpp` — flags from
  `unreal-unrealinitializer.command.json` (this TU's own receipt; its source
  arg was the staging-copyin-probe copy — here pointed at the edited tree
  file): exit=0. Six warnings, ALL pre-existing in pinned dependency headers
  (Atomic.hpp -Wswitch x2, UnrealType.hpp enum-conversion x2 +
  -Wundefined-inline, asmjit pragma) — none reference the new files or
  insertions.
- `__PROSPERO__` verified predefined (=1) by `-dM -E` with the same wrapper —
  the self-test's Prospero branch (both probes + static_assert) is the
  compiled branch; the skip branch exists only for hypothetical non-Prospero
  builds of the tree.

No --execute, no console, no uploads. The generated GeneratedBuildStamp.hpp
does not exist yet; the __has_include fallback keeps the tree compiling.

## sha256 of new files (full listings are the files themselves)

    0bf97d2517f2a5ff6732fd3a240bcaefcb87b91e7e0bbca99e896978d8191bae  candidate/include/BuildStamp.hpp
    49d763033cb83d23d4fecf78d0bf6892e0b9b1baa1ac75d791c3ad54472326ad  candidate/src/BuildStamp.cpp
    8eb9d3d84de4ff4ff37ec5a0da08833b81ea9909e6593b6ac9943bfc37d7f906  candidate/include/ToolchainSelfTest.hpp
    a74d9c69ea82734300fabc86e8f8137e7d1d28cd82ad907a927e9107205f01d1  candidate/src/ToolchainSelfTest.cpp

Edited `candidate/src/UnrealInitializer.cpp` is 3323 lines after the two
insertions (was 3297).
