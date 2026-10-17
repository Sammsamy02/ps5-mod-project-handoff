# #1 kernel reads — THE ONE ANSWER

Agent KC, 2026-10-13. Mac-only; answers the single question ("which concrete
provider/primitive would give verified kernel reads on this console firmware,
and what single test would prove it") from the existing analysis only. No new
investigation, no builds, no console contact. Every mechanism claim below was
re-verified today against the pinned extraction
`/tmp/kb-kc/kstuff-lite-1.11/` (kstuff-lite v1.11, archive SHA256
`b4fcf169a12aa184c812f4243c5f0cca40f1ad932d9a14530bbc667aef47ddf6` per
cap01-03/SOURCE-AUDIT.json).

```
Provider: ONE new bounded, read-only kekcall sub-op in kstuff-lite
          (e.g. sub 8 "KREAD": copy_from_kernel(user_buf, kernel_va, len)
          with a hard length cap), consumed by a standalone no-game probe
          ELF loaded via `pldmgr /loadpayload`, using the already
          source-demonstrated syscall-39 client ABI.

Why it works:
  1. kstuff-lite v1.11 is the ONLY kernel-resident component already proven
     running on THIS console at FW 13.60 (it is loaded every boot by the
     current WebKit -> elfldr -> kstuff-lite.elf flow). No new entry
     exploit, no new kernel framework.
  2. Arbitrary kernel reads ALREADY WORK inside the UELF today. The
     primitive exists and is exercised: copy_from_kernel /
     copy_u64_from_kernel / kpeek64_checked
     (ps5-kstuff/uelf/utils.h:28-33, 78-83) are used by REMOTE_SYSCALL's
     own handler to walk kernel proc lists --
     kpeek64_checked(tail[8]+td_proc, &proc_u)
     (ps5-kstuff/uelf/kekcall.c:158-177). The only gap is that NO sub-op
     exposes the primitive to a client (dispatch table kekcall.c:19-107:
     subs 1-7 + CHECK only; everything else ENOSYS at kekcall.c:107 --
     so a new sub-op number is free).
  3. The client channel is real and demonstrated: syscall 39 (getppid)
     with the sub-op in RAX's high bits
     (ps5-kstuff/debug-reader.c:23-46), gated on
     allow_kekcall && IS_PPR(getppid) (uelf/main.c:65). The CHECK sub-op
     0xffffffff00000027 (kekcall.c:102-106, returns 0 when alive) gives
     the probe a fail-safe liveness gate BEFORE any read is attempted.
  4. It is read-only by construction (one direction only: kernel -> user
     buffer, capped length), which is exactly capability #1's acceptance
     shape: "read-only kernel reads ... no game running ... several
     reboots without a panic ... three logged console passes"
     (PS5-MODDING-MISSION.md PHASE B test order; PROVIDER-ROUTE-VERDICT-V2.md
     section 2c defines the probe model).

  Why NOT the named alternatives (one line each):
  - kstuff REMOTE_SYSCALL (sub 5): executes STOCK syscalls in a target
    pid; stock syscalls interpret pointers as user pointers, so no
    arbitrary kernel-VA read can be constructed (VERDICT-V2 section 1.2).
    READ_DR (sub 1) copies out DR0-7 only (kekcall.c:21-48).
  - prosper0gdb's socket-corruption + pipe-buffer route: it IS a real
    kernel read (r0gdb.c copyout, lines 173-186), but EVERY read is set
    up by kwrite20 -- kernel WRITES to socket pktoptions / pipebuf memory
    (r0gdb.c:111-120) -- and it only works inside the exploit session
    that owns the corrupted socket pair (specter_args, lib/crt-elf-c.c:5-16;
    bootstrap r0gdb.c:1696-1711). Session-local and not read-only; the
    probe spawned by pldmgr does not demonstrably inherit those fds.
  - Byepervisor Reborn: 13.60 is listed as supported, but the repo is
    ~1 day old (commit ea04f71, 2026-10-12) with open 13.60 crash reports
    (issue #6 kernel trap 12 on PS5 Pro, issue #8 power-off on resume);
    verdict HOLD (byepervisor/ASSESSMENT.md sections 2, 7).

Single test:
  Fresh boot -> FTP up -> klog on -> NO game launched. Build the extended
  kstuff under a NEW filename (never overwrite kstuff-lite_v1.11.elf) and
  ONE standalone probe ELF. Load the probe once via
  `pldmgr /loadpayload cap01-kread-probe.elf`. The probe:
    (a) issues kekcall CHECK;
    (b) if CHECK returns 0, issues the new KREAD sub-op ONCE to read the
        kernel firmware-version string (the UELF's own `fwver` symbol,
        uelf/utils.h:10 -- the same string the console reports about
        itself via sysctl, the independent known answer;
        cf. r0gdb.c:1737-1744 sysctl read pattern);
    (c) prints the byte-exact result + count to its own log; exits.
  PASS = CHECK alive AND read bytes == the sysctl-reported version string
  AND clean exit AND FTP still up afterwards. Per the standing 3-restart
  law, acceptance repeats this same single test on three separate clean
  reboots (one test per boot).

Risk:
  The sub-op is NEW kernel-resident code -- the exact risk class that got
  kstuffSharp quarantined (a bad pointer/offset at kernel level panics the
  console; kekcall.c's own EFAULT paths at kekcall.c:158-230 show how
  easily a kernel-side helper can fail). If it fails you may get a kernel
  panic on the first kekcall -- recovery is reboot, and the UNTOUCHED
  kstuff-lite_v1.11.elf remains the rollback (the distinct-filename rule
  is the recovery path; mission lines 315-320: FTP must still come up
  after loading any modified kstuff). A CHECK failure, by contrast, is a
  clean informative fail: no kernel contact, nothing to recover.
  AUTHORIZATION: this is Phase B work. It is NOT authorized by this
  answer. It requires (a) the user's explicit OK for each step, and
  (b) the Phase B gate order -- only AFTER the Townfall Item Spawner
  successfully spawns an item (VERDICT-V2 section 4 lists every binding
  rule: distinct filename, written recovery path, read-only first,
  regression launches, 3-restart law).
```

One-sentence version: **the UELF already contains a working arbitrary
kernel-read primitive (`copy_from_kernel`); exposing it through one new
read-only kekcall sub-op and calling it once from a no-game pldmgr probe to
read the kernel's own version string is the single provider + single test
that would prove #1 — and it stays Phase-B-gated until the user says
otherwise.**
