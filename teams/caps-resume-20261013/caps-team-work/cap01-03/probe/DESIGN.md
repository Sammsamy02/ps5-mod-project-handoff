# cap01 kernel-read probe v0 — DESIGN (staged deliverable, Mac-only build)

Agent KE, 2026-10-13. Scope: source + compile inside `cap01-03/probe/` only.
No console, no kernel contact, no game. This probe is LOADED only when the
user explicitly authorizes it (see TEST-CARD.md).

## 1. The real question and the short answer

Question: can a standalone payload loaded via `pldmgr /loadpayload` (no game)
read kernel data, WITHOUT the exploit-time context prosper0gdb needs?

Answer: YES for kernel-register data, using only the already-resident
kstuff-lite v1.11 kekcall surface. NO for arbitrary kernel-VA data — that
requires one new read-only kekcall sub-op inside kstuff-lite (Phase B,
`../KERNEL-READ-ONE-ANSWER.md`), which is NOT part of this deliverable.

## 2. Why prosper0gdb's mechanism is unavailable here

prosper0gdb's kernel read (`r0gdb.c:111-192`) is a real kernel R/W, but every
read is set up by `kwrite20` — kernel WRITES to a corrupted IPV6_PKTINFO
socket pair (`IPV6_PKTINFO` setsockopt overflow, `r0gdb.c:96-120`) — and the
read itself redirects a pipe buffer whose kernel address was captured at
exploit time (`init_pipe`, `r0gdb.c:158-171`, `copyout` `r0gdb.c:173-183`).
That context (master_fd/victim_fd/rpipe/victim_pktopts, `specter_args`) is
created by the r0gdb bootstrap inside its own exploit session
(`r0gdb.c:1696-1711`, `lib/crt-elf-c.c:5-16`). A pldmgr-loaded probe:

- does not inherit those corrupted fds (session-local objects), and
- could not even rebuild them without re-running the socket-corruption
  exploit — a kernel-WRITE primitive, i.e. new privileged action, not a
  read-only probe.

So the exploit-time route is excluded by design, matching
`../PROVIDER-ROUTE-VERDICT-V2.md` section 2d.

## 3. What IS available: the resident kekcall surface

kstuff-lite v1.11 is already kernel-resident on this console (loaded every
boot by the WebKit -> elfldr -> kstuff-lite flow). Its UELF dispatches
kekcall sub-ops on syscall 39 (getppid) with the sub-number in RAX[63:32]
(client ABI proven on-console: `debug-reader.c:21-46`; full verified table in
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/kstuff-lite-v1.11-VERIFIED.md` section 2):

| sub | number              | name           | v0 probe uses it | why / why not |
|-----|---------------------|----------------|------------------|---------------|
| 0xffffffff | 0xffffffff00000027 | CHECK          | YES, once  | 0 = resident AND caller eligible; nonzero = real getppid (parent pid). Liveness+eligibility gate. |
| 1   | 0x0000000100000027  | READ_DR        | YES       | copyout of 48 bytes (DR0 DR1 DR2 DR3 DR6 DR7) to a user buffer (`uelf/kekcall.c:21-48`). Kernel->user data movement from live CPU state. |
| 2   | 0x0000000200000027  | WRITE_DR       | refused   | installs debug registers into the current thread (`kekcall.c:110-139` via `install_dbgregs_checked`). Write primitive; probe is read-only. |
| 3   | 0x0000000300000027  | RDMSR          | YES       | returns the MSR value (`kekcall.c:59-62`, `uelf/utils.c:799-812`). Reads model-specific registers in kernel context. |
| 4   | reserved            | WRMSR          | absent    | not implemented in v1.11 (`kekcall.c:63`). |
| 5   | 0x0000000500000027  | REMOTE_SYSCALL | refused   | runs STOCK syscalls in a target pid; stock syscalls interpret pointers as USER pointers, so no arbitrary kernel-VA read can be built (VERDICT-V2 section 1.2). Also cross-process — out of read-only-probe scope. |
| 6   | 0x0000000600000027  | shared-area snapshot | no   | OBS-builds only; ENOSYS otherwise (`kekcall.c:77-90`). Not needed for the acceptance question. |
| 7   | 0x0000000700000027  | PPR plaintext control | refused | control channel (`kekcall.c:91-101`), not a read; out of scope. |

### The one genuinely unverified console property this probe answers (U1)

The kekcall path requires `allow_kekcall && IS_PPR(getppid)`
(`uelf/main.c:65`): the CALLER'S process must dispatch syscalls from the
native (PPR) sysent table. Whether the payload-daemon process created by
`pldmgr /loadpayload` lands on that hooked path is an unverified console
property (VERDICT-V2 section 2d, item "kekcall"). CHECK answers it directly:

- CHECK returns 0  -> resident AND eligible: every later sub-op is live.
- CHECK returns parent pid (>0) -> kstuff absent OR the payload process is
  not on the PPR table. Clean, informative, zero kernel contact.
- Per the refusal law (VERDICT-V2 section 4 / P1): a failed CHECK is ABORT,
  never retried blindly. The probe issues CHECK exactly once, no loop.

## 4. Probe flow (KernelReadProbe.c)

Error-convention robustness: the pinned source does not prove whether the
intercepted-syscall error return sets RFLAGS.CF (debug-reader.c:36 assumes
it does) or just leaves errno in RAX (`uelf/main.c:77-81` writes
`regs[RAX] = err` on the resumed path). The probe therefore classifies every
result on the RAW uint64 RAX so it is correct under BOTH conventions:
`rc == 0` success-with-zero; `(int64_t)rc < 0` carry-flag errno;
`0 < rc < 0x1000` errno-class (or, for CHECK, the real getppid parent pid —
both are terminal failures with the raw value logged); `>= 0x1000` a real
returned value. Sign alone is NEVER used to detect failure (a successful
LSTAR read is "negative" as int64_t).

1. Open own NEW log `/data/cap01-kread-probe-v0.log` (tee to stdout;
   debug-reader.c pattern). Log open failure is soft (stdout-only).
2. CHECK once (sub 0xffffffff). Nonzero -> log + exit 2. No retry, and NO
   further kekcalls are issued after a failed CHECK.
3. RDMSR MSR_LSTAR (0xC0000082) — by architectural definition the kernel
   syscall-entry RIP. Known answer is a SHAPE, not a pinned value: a valid
   result MUST be a canonical kernel-half address (`value >> 47 == 0x1FFFF`,
   i.e. >= 0xffff800000000000). Anything else = FAIL. Exact bytes logged.
4. RDMSR MSR_EFER (0xC0000080) — logged as evidence; expected stable
   (SCE|LME|LMA|NXE [+SVME] = 0xD01/0xD11-class) but NOT pinned, so it does
   not gate pass/fail.
5. RDMSR MSR_TSC (0x10) twice — second MUST be strictly greater than first
   (monotonic kernel counter, two live calls). Gate.
6. READ_DR once into a 48-byte buffer pre-filled with 0xA5. rc MUST be 0 and
   at least one byte MUST differ from 0xA5 (proves real kernel->user copyout,
   not a silent no-op). Gate. Layout (`uelf/utils.c:677-690`,
   `kekcall.c:23-47`): bytes 0-7 DR0, 8-15 DR1, 16-23 DR2, 24-31 DR3,
   32-39 DR6, 40-47 DR7. Soft (logged, non-gating) shape checks for an idle
   AMD thread: DR6[63:32]==0, DR7==0 or 0x400-class, DR0-3 expected 0.
7. Summary line with per-check verdicts; `fclose`; return from main.

Exit codes: 0 PASS (all gates green) · 2 CHECK failed (not resident or
payload process ineligible — answer to U1, clean fail) · 3 evidence gate
failed (RDMSR/TSC/READ_DR anomaly) · 1 internal error (could not produce any
log).

## 5. What this probe proves — and what it does NOT

Proves: kstuff-lite v1.11 alive; the payload-daemon process reaches the
hooked PPR getppid path; the kernel-side kekcall machinery (gadget dispatch +
real copyout) executes for our caller and returns genuine kernel-derived
data (LSTAR, EFER, TSC, DR0-7) byte-exactly into a user buffer.

Does NOT prove: arbitrary kernel-VA reads. RDMSR/READ_DR are register reads.
Do not upgrade the wording beyond that (AGENTS.md honesty law).

## 6. What a FULL kernel-read capability needs beyond READ_DR

Exactly one thing: a new READ-ONLY kekcall sub-op in kstuff-lite (working
name KREAD, e.g. sub 8): `copy_from_kernel(user_buf, kernel_va, capped_len)`
dispatched like sub 1. The primitive already exists and is exercised inside
the UELF (`uelf/utils.h:28-33` copy_from_kernel/copy_u64_from_kernel/
kpeek64_checked; used by REMOTE_SYSCALL's own proc-list walk,
`kekcall.c:158-177`); the gap is only that no sub-op exposes it
(`kekcall.c:19-107`, unknown subs ENOSYS at :107, so a new sub number is
free). Full plan, single test, risk and rollback in
`../KERNEL-READ-ONE-ANSWER.md`. Status: Phase-B-gated, requires the user's
explicit OK, an extended kstuff built under a NEW filename (never overwrite
kstuff-lite_v1.11.elf), and the Phase B gate order. NOT built here.

Named alternatives, refused:
- prosper0gdb route — kernel writes + session-local fds (section 2 above).
- REMOTE_SYSCALL (sub 5) — stock syscalls only, user pointers (section 3).
- Byepervisor Reborn — new kernel-resident framework, ~1 day old at pin,
  open 13.60 crash reports; HOLD (VERDICT-V2 section 2d, KC answer).

## 7. Provenance

- Mechanism/ABI: pinned extraction `/tmp/kb-src-jc/kstuff-lite-1.11/`
  (same archive bytes as `/tmp/kb-kc/kstuff-lite-1.11/` per
  `../SOURCE-AUDIT.json`).
- Client asm: `ps5-kstuff/debug-reader.c:21-46` (verbatim shape; inline
  syscall — no dlsym/gadget needed, VERIFIED.md section 3 ABI note).
- Sub-op numbers/semantics: `ps5-kstuff/uelf/kekcall.c:19-107`,
  `uelf/utils.c:677-690,799-812`; polarity + table verified in
  `kstuff-lite-v1.11-VERIFIED.md` G4/K2.
- Toolchain: ps5-payload-sdk v0.43 prospero-clang
  (`.../build/sdk-v0.43/ps5-payload-sdk/`), hello_world/Makefile pattern;
  entry is the SDK crt1.o `_start` -> `main()` (plain ELF executable —
  module_start is a PRX concept, not used by ploader payloads).
