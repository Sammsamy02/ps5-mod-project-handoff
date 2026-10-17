# Context-bound Win64 import bridge: isolated Mac stage

**Result:** The isolated `cg-context-bridge-20261013-14` stage adds a reusable ABI seam for Windows x64 calls into SysV handlers that need a per-runtime context pointer. A 32-byte per-export trampoline carries the context, target, and shared thunk address. The new shared thunks shift **zero through eight integer/pointer guest arguments** behind the context and place later arguments on the SysV stack. This stage does not publish Windows API imports or execute an unchanged DLL.

## Host evidence

The new x86-64 host runner passed **13/13** checks with real RW-to-RX stamped trampolines and synthetic `ms_abi` callers. It verified context and exact argument values for every arity 0–8, refused arity 9, checked the 32-byte slot boundary and SysV stack alignment, and showed that a SysV target deliberately clobbering RSI, RDI, XMM6, and XMM15 did not corrupt the Win64 caller. A mutation check removed the XMM15 restore in a temporary assembly copy; the nonvolatile test then failed as intended.

Fresh stage regression: import-registry **7/7**, TEB lifecycle **12/12**, static TLS **19/19**, loader preflight **27/27**, five unchanged DLLs' read-only TLS preparation **5/5**, existing bridge fixture checks **2/2** and **1/1**, and synthetic PE fixture **11/11** with its pinned source hash unchanged. Clang assembled the bridge for an x86-64 FreeBSD ELF target as a syntax check; that is **not** a PS5 build or run.

The [incremental patch](context-bridge-increment-20261013.patch) changes the bridge header/assembly and adds three host test files against the exact prior `cg-import-callable-gate-20261013-13` stage. The [hash manifest](context-bridge-manifest-20261013.json) pins the base and staged files. A scratch `git apply --check`, application, and byte-for-byte comparison passed. The [evidence ZIP](context-bridge-evidence-20261013.zip) contains the fresh logs, mutation result, patch, and manifest. The shared candidate, Claude's work, kernel code, and original DLL binaries were untouched.

## Boundary

The bridge covers fixed integer/pointer signatures and integer/pointer returns. It does not handle mixed floating-point arguments, variadic calls, 9–16-byte aggregate returns, Windows exception unwinding, or API semantics. The executable-memory transition succeeded on the Mac; the corresponding PS5 permission and cache-coherence behavior remain a console gate. The import shim still refuses raw handlers by default. Typed signature metadata, safe publication after executable-memory validation, and real Windows/CRT/UE4SS implementations are the next offline integration work. This stage does **not** make the project a console-only wait.
