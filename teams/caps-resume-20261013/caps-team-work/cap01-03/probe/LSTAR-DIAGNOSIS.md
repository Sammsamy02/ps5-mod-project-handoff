# LSTAR RDMSR failure — offline diagnosis

Probe: cap01-kread-probe-v0 (KernelReadProbe.c)
Console evidence: `evidence-cap01-kread-console.log` / `EVIDENCE-CONSOLE-20261013.log`
(2026-10-13): `check=RDMSR_LSTAR verdict=FAIL why=msr_read_faulted_or_errno`,
all other checks (CHECK, RDMSR_EFER, RDMSR_TSC ×2, READ_DR) PASSED.

Kstuff source analyzed: kstuff-lite v1.11 pinned source zip
(`what/outputs/kstuff-lite-pinned/kstuff-lite-v1.11-source.zip`,
sha256 `b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6`),
extracted to /tmp/kb-src/kstuff-lite-1.11/. This analysis is Mac-only, offline,
read-only. All citations below are file:line into that tree unless prefixed
`probe/` (this directory).

## Verdict (one line)

**Hypothesis (d) is the root cause: the probe's result classifier
(`classify_msr_result`, probe/KernelReadProbe.c:126-134) misclassifies a
SUCCESSFUL LSTAR read as an error, because a canonical kernel-half LSTAR value
has bit 63 set and the classifier's first rule treats any negative int64 as a
carry-flag errno. No kstuff-side fault is required and none is supported by the
source.**

## The exact path a successful RDMSR LSTAR takes

1. Probe issues `syscall` with RAX = `0x0000000300000027` (sub 3 | SYS_getppid)
   and RDI = `0xC0000082` (probe/KernelReadProbe.c:60-76 wrapper, :81
   KEKCALL_RDMSR, :88 MSR_LSTAR, call site :185-186).
2. kstuff dispatches it (uelf/main.c:65-84): args[] is a copy of the trap frame
   with `args[RAX]` = user RAX (main.c:68 static assert `RAX == 6`; args copied
   from `td_frame` at main.c:69-73); sub-op number = `args[RAX]>>32`
   (main.c:74).
3. Sub 3 (uelf/kekcall.c:59-62):
   ```c
   else if(nr == 3)
   {
       return rdmsr(args[RDI], &args[RAX]) ? 0 : EFAULT;
   }
   ```
   `args[RDI]` (the MSR address) goes to `rdmsr()` **unfiltered**.
4. `rdmsr()` (uelf/utils.c:799-812) runs the kernel `RDMSR` gadget with
   `RCX = which` (utils.c:802-805), then on success writes
   `*ans = regs[RDX] << 32 | (uint32_t)regs[RAX]` (utils.c:810). There is NO
   comparison of `which` against any value anywhere in rdmsr().
5. On success kekcall returns 0 (kekcall.c:61), kstuff copies `args[RAX]` (the
   MSR value) into `td_retval[0]` and sets trap-frame RAX = 0
   (uelf/main.c:77-81). The user thread's RAX after the syscall = the raw MSR
   value, carry flag clear.
6. Probe wrapper returns `(int64_t)ret` when CF is clear
   (probe/KernelReadProbe.c:75), the call site casts back to uint64
   (:185-186), and `classify_msr_result` runs (:126-134):
   ```c
   if((int64_t)rc < 0)      /* carry-flag error */
       return 1;
   ```
   LSTAR is a kernel-half canonical address (`value >> 47 == 0x1FFFF` is the
   probe's own shape gate, probe/KernelReadProbe.c:177-188), so bit 63 is set,
   `(int64_t)rc < 0` is TRUE, and a perfectly good value is reported as
   `msr_read_faulted_or_errno` via check_fail (:187). This happens for **100%
   of successful LSTAR reads** — it is deterministic, not transient.

Why the other MSRs pass: EFER returned `0x11D01` and TSC
`0x000006a16ddc88af` (console log lines 4-5) — both have bit 63 clear, so they
reach the value branch (:130-133). TSC would itself be misclassified only after
~92 years uptime at ~3.2 GHz (2^63 cycles). The design comment at
probe/KernelReadProbe.c:30-38 even promises "rc >= ERRNO_CLASS_MAX -> a real
returned VALUE (MSR)" (:37), but the `(int64_t)rc < 0` rule at :128 executes
first and swallows the entire upper half of the value space ([2^63, 2^64)) —
the comment and the code contradict each other for exactly the one MSR whose
true value lives up there.

Note the failing branch (:187) logs no raw rc — only the
`not_canonical_kernel_address` branch (:190) prints the value. That is why the
console log shows no raw LSTAR rc and why the FAIL line alone is formally
ambiguous; the source-level analysis below resolves the ambiguity.

## Hypothesis-by-hypothesis findings

### a. MSR whitelist/blocklist in kstuff sub 3? — NO

- uelf/kekcall.c:59-62 passes `args[RDI]` directly to `rdmsr()`.
- uelf/utils.c:799-812 places `which` straight into RCX (utils.c:804) with no
  comparisons.
- Case-insensitive search of the whole uelf and ps5-kstuff/main.c for
  `lstar|c0000082|c0000080|efer|whitelist|blocklist|allowlist` finds no filter.
  The only literal MSR address in the tree is `0xc0000084` (SFMASK) at
  ps5-kstuff/main.c:1534, written via the separate r0gdb mechanism during
  load — not kekcall.

### b. Is LSTAR treated differently from EFER? — NO

Both take the identical nr==3 path (kekcall.c:59-62 → utils.c:799-812). No
source reference to LSTAR exists at all (see search under (a)). On AMD64,
EFER (0xC0000080) and LSTAR (0xC0000082) are adjacent entries in the same
architectural long-mode MSR block; RDMSR of LSTAR is implemented on every
AMD64 processor including the PS5's Zen 2, and the gadget executes at CPL 0
(kernel text, CS 0x20 loaded in the gadget frame, utils.c:803), so there is no
privilege difference either (see (e)).

### c. Return-value convention different for certain MSR ranges? — NO

`rdmsr()` reconstructs EDX:EAX uniformly for every MSR
(uelf/utils.c:810: `*ans = regs[RDX] << 32 | (uint32_t)regs[RAX]`) and returns
1 on success / 0 on failure regardless of address; kekcall.c:61 maps that to
0 / EFAULT, and main.c:77-81 delivers the value via `td_retval` either way.
One uniform (non-gating) caveat: `rdmsr_start` is resolved as a bare `0f 32`
opcode in kernel text (tools/validate_ppr_offsets.py:56; per-FW deltas e.g.
prosper0gdb/offsets/13_60.h:16 `DEF(rdmsr_start, -0xA69BFA)`), so the gadget
keeps executing whatever instructions follow until a natural RET. If that tail
ever transformed RAX/RDX, it would do so identically for EFER/TSC/LSTAR — and
the observed FAIL is the classify branch (:187), not the shape branch
(:188-192, which would have logged a raw value), so on console the full
64-bit value demonstrably arrived intact and was eaten by classification.
(`rdmsr_end` at uelf/utils.c:666 is a dead extern: declared, never defined
( absent from prosper0gdb/offsets/offset_list.txt) and never used.)

### d. Probe misclassification? — YES, root cause

Full mechanism in "The exact path" above. Predicts: LSTAR FAIL with
`msr_read_faulted_or_errno` on every run; EFER/TSC/READ_DR PASS. The console
log matches exactly (lines 3-6). The internal contradiction is between
probe/KernelReadProbe.c:128 (`(int64_t)rc < 0` = error) and :37 ("rc >=
ERRNO_CLASS_MAX -> a real returned VALUE"): a kernel-half address satisfies
`rc >= 0x1000` AND `(int64_t)rc < 0` simultaneously, and :128 wins.

### e. Privilege restriction on LSTAR specifically? — NO

The gadget runs at CPL 0 (kernel text, CS 0x20, utils.c:802-805) — RDMSR of
LSTAR is CPL-0-readable on all AMD64. The only gate kstuff applies is
process-level kekcall eligibility (`allow_kekcall && IS_PPR(getppid)`,
uelf/main.c:65), identical for every sub-op and already proven green by
`check=CHECK rc=0` (console log line 2).

## Residual formal caveat

A one-off transient gadget failure (run_gadget_checked → EFAULT,
uelf/utils.c:616-644) hitting exactly the LSTAR call cannot be excluded by the
log alone because the failing branch logs no raw rc (:187). It is rejected as
the cause because: (1) there is no MSR-specific mechanism in the source (a, b);
(2) four subsequent gadget runs (EFER, TSC ×2, READ_DR) all succeeded
immediately after; (3) misclassification (d) predicts the observed outcome with
certainty while a transient predicts it with probability ~0.

## Recommended fix (for the next probe revision, NOT implemented here)

1. Pass CF out of the wrapper explicitly — `is_error` already exists at
   probe/KernelReadProbe.c:71 but is collapsed at :75
   (`return is_error ? -(int64_t)ret : (int64_t)ret;`). Return the raw RAX and
   a separate error flag; classify on the flag first, never on the value's
   sign.
2. Log the raw rc in the classify-failure branch (:187) so any future FAIL is
   self-documenting (this would have made the present diagnosis
   one-look-obvious).
3. With the fix, a re-run should print
   `check=RDMSR_LSTAR value=0xffff… top32=0xffffffff canonical_kernel=yes
   verdict=PASS`, which also retroactively confirms the kstuff side was
   correct all along.

Note: the identical wrapper shape exists in kstuff's own
ps5-kstuff/debug-reader.c:21-46 (`is_error ? -(int64_t)ret : (int64_t)ret`),
so any future tool reusing that client shape inherits the same
kernel-half-value ambiguity for RDMSR results.
