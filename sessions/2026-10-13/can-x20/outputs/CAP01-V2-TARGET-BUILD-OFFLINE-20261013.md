# #1 v2 register-read probe: PS5 target build receipt

**Status:** the unchanged v2 source compiled and linked offline with the pinned PS5 SDK. It has not run on a console and does not implement arbitrary kernel-address reads.

Source: `/WORKSPACE/.codex/worktrees/translation-core/caps-resume-20261013/caps-team-work/cg-max-ready-20261013-03/cap01/KernelReadProbe-v2.c`

The pinned `prospero-clang` driver, with `LLVM_CONFIG` set to its bundled LLVM 21.1.8 tool, compiled the source as C11 with `_XOPEN_SOURCE=700` and `-Wall -Wextra -Werror`, then linked an x86-64 PS5/FreeBSD-style PIE ELF. Both commands exited 0 without diagnostics. The source SHA-256 remained `21486d16fc9ca6fe57f8d8f42f55382776301097c7396ed65e88e1c565fa770a`.

A scratch copy of that exact source passed **6/6** Mac control-flow cases and **13/13** classifier cases. Target object disassembly shows CHECK's `0xffffffff00000027` immediate followed by `syscall` and `setb` carry capture. The tests do not determine whether the pinned kernel syscall-exit path actually returns the expected RAX and carry-flag pair to the payload.

- [Source and binary hash manifest](cap01-v2-target-build-manifest-20261013.json)
- [Target object, ELF, disassembly, and host-test logs](cap01-v2-target-build-evidence-20261013.zip)

The remaining #1 gate is a console run with fresh raw RAX and carry evidence, plus a separate admitted provider for bounded arbitrary kernel-memory reads if that broader capability is required. The target ELF was neither uploaded nor executed. No kernel source, shared candidate, other capability, or original DLL was changed.
