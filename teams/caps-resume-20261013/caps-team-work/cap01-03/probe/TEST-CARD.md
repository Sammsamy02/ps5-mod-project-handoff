# TEST CARD — cap01-kread-probe-v0 (STAGED, not authorized to load)

Status: **STAGED DELIVERABLE.** Built Mac-only in `cap01-03/probe/`. Nothing
has been uploaded, loaded, or run on the console. Loading requires the
user's explicit OK — say so in plain words (e.g. "OK, load the kread probe
once") — before ANY of the steps below run. Design rationale: DESIGN.md.

## What the probe does (plain words)

A small standalone program, loaded by the console's payload loader with NO
game running. It asks the already-resident kstuff (the jailbreak kernel
component that loads every boot) five read-only questions and writes the
answers to its own log:

1. "Are you there?" (kekcall CHECK — 0 means yes)
2. "What is your syscall entry address?" (RDMSR LSTAR — must be a kernel
   address; this is REAL kernel data crossing into userspace)
3. "What is your EFER register?" (RDMSR — logged, informational)
4. "Is your TSC counter ticking?" (RDMSR twice — must advance)
5. "Send me your debug registers DR0-DR7" (kekcall READ_DR — 48 bytes
   copied kernel->user; buffer pre-filled with 0xA5 so a real copy is
   provable)

Then it prints PASS or FAIL and exits. It NEVER writes anything to the
kernel, never loads a PRX, never touches a game, and never calls the
debug-register-write or remote-syscall kekcalls. It runs against the STOCK
kstuff-lite v1.11 — no modified kernel component is involved.

Honest boundary: this proves kernel-register reads (the kekcall channel
works and kernel data arrives byte-exact). It does NOT prove arbitrary
kernel-memory reads — that needs the Phase-B KREAD sub-op in kstuff-lite
(see ../KERNEL-READ-ONE-ANSWER.md, not built, not authorized).

## Build receipt (this exact ELF)

| item | value |
|---|---|
| file | `cap01-kread-probe-v0.elf` |
| sha256 | `12ae76a08fd7afb2b6c0da06d6c51ccc34f9d9fc8726bd2fc7847a85c3192dcb` |
| size | 119432 bytes (ELF64 x86-64, SDK crt1 entry -> main, stdio imports only) |
| source | `KernelReadProbe.c` sha256 `28b60996132c836c0b8a387dba6231940fd045c680213595dbeae15fd332fb56` |
| rebuild | `make` in this directory (Mac-only; no deploy target exists) |

Only THIS ELF (hash above) may be loaded. A different hash = do not load,
rebuild and re-receipt first.

## How to load (after the user's explicit OK — one command)

Preconditions: fresh boot; FTP up; klog capture on; **NO game launched**;
ploader active (it is the last line of autoload.txt).

1. Upload the ELF to `/data/plugins/cap01-kread-probe-v0.elf` via FTP.
2. Verify the upload by readback: sha256 of the downloaded file MUST equal
   the receipt hash above BEFORE launching anything (PS5 upload law;
   this file is a plain ELF, not a SELF — no fake-signing involved, and
   never upload it as eboot.bin anywhere).
3. Load it ONCE:

   ```
   pldmgr /loadpayload /data/plugins/cap01-kread-probe-v0.elf
   ```

4. Fetch the probe's own log: `/data/cap01-kread-probe-v0.log` (FTP).
   The probe's stdout also goes to the payload-daemon log, but the file
   above is the authoritative evidence.
5. Confirm FTP still works afterwards (part of pass).

ONE load per boot. Never rapid-fire; if you need another run in the same
boot, that is a NEW decision — this acceptance runs one test per boot only.

## What PASS looks like

`/data/cap01-kread-probe-v0.log` ends with:

```
check=CHECK rc=0 verdict=PASS meaning=kstuff_resident_and_eligible
check=RDMSR_LSTAR value=0xffffffff........ top32=0xffffffff canonical_kernel=yes verdict=PASS
evidence=RDMSR_EFER value=0x............ verdict=LOGGED_ONLY
check=RDMSR_TSC t1=0x............ t2=0x............ delta=.... verdict=PASS
check=READ_DR rc=0 verdict=PASS
evidence=READ_DR byte_off=.. value=0x............   (6 lines)
summary checks=4 failures=0
overall=PASS exit=0
```

plus: the probe process exits cleanly (0), FTP still up, console not
rebooted, no panic in klog.

## What FAIL looks like (each mode is informative — do not retry blindly)

| log shows | meaning | required response |
|---|---|---|
| `evidence=CHECK rc=<nonzero>` then `check=CHECK verdict=FAIL` `overall=FAIL exit=2` | kstuff not resident, OR the payload-daemon process is not on the hooked PPR syscall table (open question U1 — a REAL answer either way), or a syscall errno. The raw rc is logged; if it is a huge value (`rc=0xffff...`) it is a carry-flag errno (see the table in the source header). | STOP. Record the full log. No retries, no variation spam. Reconcile with director before anything else. Zero kernel contact occurred. |
| `check=RDMSR_LSTAR ... verdict=FAIL` / `check=RDMSR_TSC ... FAIL` / `check=READ_DR ... FAIL` (`overall=FAIL exit=3`) | kekcall channel alive but a kernel-data gate failed — a new, specific fact (e.g. sub-1 copyout path misbehaving on 13.60) | STOP, upload the full log, reconcile. Do NOT re-run to "see if it goes away". |
| console panic during load | worst realistic case: the never-yet-console-proven kernel-side READ_DR gadget path faulted | reboot; stock kstuff-lite is UNTOUCHED (the probe loads no kernel code) — after a clean reboot the console is back to baseline. Do NOT reload without explicit user OK AND a written reconciliation of the panic. |
| no log file produced | probe died before logging or upload/load mismatch | verify sha256 readback + path, check payload-daemon log, report. |

## The 3-restart requirement

A single pass is NOT acceptance. Per the standing law, the SAME single test
must pass on THREE separate clean reboots — one test per boot, three logged
`overall=PASS exit=0` logs from three different boots — with no panic in
any of them and FTP up after each. Each boot gets its own dated copy of the
log (e.g. `cap01-kread-probe-v0.boot1.log`) before the next reboot.

## Authorization gates (restated)

- The probe is a console action: it needs the user's explicit OK before the
  FIRST load and before ANY reload after a failure or panic.
- It runs against stock kstuff-lite v1.11: no new kernel code, so it is NOT
  the Phase-B kstuff extension. But #1 (arbitrary kernel reads) still
  requires that separate, separately-authorized Phase-B work after the
  Townfall Item Spawner gate — this probe does not satisfy or shortcut it.
- No failed/reserved-job replay: a failed test is reconciled, never rerun
  identically hoping for a different result.
