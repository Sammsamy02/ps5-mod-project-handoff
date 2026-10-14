# Phase B (kernel work) — one-page doc status

Prep summary, 2026-10-12 (Mac-only, read-only). Source: `../PHASEB-PLAN.md`
(agent GD, with the GP CORRECTIONS appendix) and the capability rows #1/#3/#6
in `../CLAUDE-CAPABILITY-STATUS.md`. Everything in `PHASEB-PLAN.md` is labelled
DESIGN ONLY; nothing in it has run on console, and nothing here authorizes any
console action.

## What is designed (all Mac-only / design-only unless cited)

Phase B is a plan to use the console's already-resident stock **kstuff-lite
v1.11** kernel payload — calling its existing services, never replacing or
modifying it — to back three capabilities:

- **#1 kernel reads** — a read ladder R0→R4. R0 (in-process reads) is
  console-proven (the port's own runtime, session log L551 cited in the #1 row).
  R1 `KEKCALL_CHECK`, R2 (own-pid `dynlib_get_info_ex 608` / sysctl `KERN_PROC`),
  R3 (`REMOTE_SYSCALL` sub-5 = cross-process reads) are design-only.
- **#3 code patching** — runtime-memory patches via `mdbg_call 573`
  (hooked-but-passthrough in v1.11). DATA-page patches first (vtables, GOT,
  flags). Pointer/table hooks are already console-proven in-process (#3 row,
  `claude2-menu-session` log); the kernel-mediated write channel is design-only.
- **#6 controller kernel route** — DECLINED by design (userspace ScePad is
  sufficient and is console-proven across sessions); no kernel input code.

GP CORRECTIONS (from reading the real v1.11 source) already fold into the plan:
CHECK polarity is inverted (resident ⇒ returns 0); there is NO `0xdeb7` extended
syscall range, so the old R4 "kernel-VA content read" rung is DEAD — cross-process
reads are REMOTE_SYSCALL only; ISA is x86-64 (coherent, no i-cache step; the
skeleton's aarch64 asm is void); unknown kekcall sub-numbers silently degrade to
real getppid (never probe support by return value).

## The 3-pass-across-restarts acceptance rule — what it applies to

"Done" = 3 console passes across 3 separate reboots with saved logs; Mac/host
tests never count (`PHASEB-PLAN.md` §4.6). It applies to EVERY console rung:
- the source-pin acceptance (§2 P5: 3 observe-only passes — residency + decrypt-off
  hash + version string identical across 3 boots);
- each risk-ladder rung L1→L3 (§4.1): climb one rung only after 3 passes on the
  rung below, one capability per session, written pass/fail + revert path first,
  saves backed up, ONE user-launched test per boot, regression launches
  (Astro Bot / Stellar Blade / Townfall) after any L2+ pass.
It does NOT retroactively bless the in-process primitives (#1 R0, #3 pointer
hooks) that are already proven by prior single-session receipts — those keep
their existing labels; the 3-pass rule governs the NEW kernel-adjacent steps.

## What is blocked

1. **kstuff-lite v1.11 SOURCE is not on the Mac** (§6 gap 1) — the byte-identical
   rebuild (§2 P4) cannot run; until then every §3 contract is family-level
   source, not lite-pinned. (The pinned BINARY sha `ab9a6cb4…` is matched
   binary-vs-catalog only — `../CLAUDE-CAPABILITY-STATUS.md` kstuff-lite pin
   event; not a source↔binary identity.)
2. **Lite kekcall subset + ABI** must be pinned from the fetched binary (§2 P3)
   before any client code is written against the interface. The GP appendix
   already lists the exact v1.11 kekcall table from source reading, but that is
   advisory until the binary inventory confirms it.
3. **L3 write channel** needs an FTP-up-after-crash recovery pre-proof before the
   first write test (§4.5); L0–L2 do not.
4. **The `[KB]` probe TU does not exist in the tree** and is default-OFF by
   design; it would only be built behind the §5.1 checklist after approval.
   (Confirmed Mac-side: `-DMODBRIDGE_KB_PROBES` appears only inside the plan doc,
   never in any build config — #1 row.)

## What needs explicit user approval before any step

Per `PHASEB-PLAN.md` §0, EVERY console step needs Claude review + explicit user
OK for that exact step; the standing AGENTS.md routine approvals do NOT cover
kernel-bridge work. Concretely, separate approval is required for: the §2 P1
decrypt-off binary fetch; the §2 P5 observe-only residency passes; building and
uploading the `[KB]` probe PRX (§5.1); loading it and running L1 `KEKCALL_CHECK`;
and each higher rung L2 (reads) and L3 (data-page writes). L4 — any NEW
kernel-resident code or a modified kstuff build — is PROHIBITED this phase and
would need its own authorization with a recovery pre-proof. The quarantined
kstuffSharp module is never touched. Universal rollback is reboot (kstuff has no
unload); stock `kstuff-lite_v1.11.elf` must stay byte-intact on console at all
times.
