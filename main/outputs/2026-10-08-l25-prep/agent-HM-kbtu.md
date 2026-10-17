# agent HM (root inline) — [KB] probe TU

Deliverables: candidate/include/KernelBridgeProbes.hpp + candidate/src/
KernelBridgeProbes.cpp (NEW files only; no other tree edits).

- Double-gated: __PROSPERO__ AND MODBRIDGE_KB_PROBES (default OFF — zero
  code in ungated builds, preserving the audit-verified "no KB code ships"
  law). NOT registered as a build unit; a gated build is a separate,
  Claude-reviewed step.
- ABI grounded in the PINNED v1.11 source, read directly this session:
  trampoline asm verbatim-shape from ps5-kstuff/main.c:297 (7th arg ->
  RAX, jmp *gate), gate resolution dlsym(RTLD_NEXT,"getpid")+7 (:1334),
  sub constants (:301-306), CHECK polarity resident==0 from the caller's
  own usage (:1336), R0 sysctl mib {1,46} firmware=version>>16
  (r0gdb_get_fw_version).
- R0 only: one CHECK kekcall (gate proof) + plain sysctl (value) ->
  '[KB] R0 check=resident fw=0x%x expect=0x1360 verdict=PASS|FAIL';
  not-resident/no-gadget log-and-skip. R1/R2 declared + refuse loudly.
- Markers (conditional — manifest entries ONLY when a gated build exists):
  '[KB] R0 check=resident fw=' / '[KB] R0 skipped reason=' / '[KB] R1' /
  '[KB] R2' — contiguous literals.
- static_asserts pin the composition (CHECK sub 0xffffffff, gate 0x27).
- Syntax evidence: prospero-clang++ -fsyntax-only, flags cloned from the
  unreal-unrealinitializer receipt: GATE-OFF rc=0, GATE-ON (-D gate) rc=0.
- Scope laws kept: no writes anywhere in the TU; rungs beyond R0 need a
  Claude-reviewed spec (PHASEB R-ladder law).
