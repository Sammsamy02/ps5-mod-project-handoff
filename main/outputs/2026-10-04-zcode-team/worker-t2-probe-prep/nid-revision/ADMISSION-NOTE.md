# ASMHelper.guarded NID-routed mprotect revision

## What changed
The fold-1 walkguard's `sys_mprotect` implementation replaced: inline raw
syscall (`__asm__ volatile("syscall")` with SYS_mprotect=74) → NID-routed
`sceKernelMprotect(void*, uint64_t, int)` call.

## Why
Session 934 proved raw `syscall` instructions are process-fatal in the
sandboxed game process (0xa002030a SYSTEM_ILLEGAL_FUNCTION_CALL). The T2
probe prep (2026-10-06) confirmed by disassembly that the fold-1 objects
(f5627427, 6604ce2d) each carry exactly one raw syscall instruction — making
them UNSAFE to boot regardless of T2's rc.

## Dependencies
1. The import contract must be extended (196→197) to include sceKernelMprotect
   (the xwp probe's contract has it as a derived libkernel NID — NOT yet
   console-proven; the T2 probe's import-validation pass IS the test).
2. The T2 probe must still run to prove:
   a. The derived sceKernelMprotect NID resolves (import validation)
   b. The kernel permits X→RX transitions on the exec-only text page class
3. The composition's undefined-symbol gates must accept the new import.

## Usage
This file replaces ASMHelper.guarded.cpp in the composition when:
- T2 proves the NID resolves AND the X→RX transition works
- The import contract is extended to 197 strong imports
- The pin cascade records the contract change

## Measurements
- Original fold-1 (raw syscall): object f5627427 (1 syscall instruction)
- This revision: NO syscall instructions; 1 new strong import expected
- Source sha: 750fde30a0c2...
