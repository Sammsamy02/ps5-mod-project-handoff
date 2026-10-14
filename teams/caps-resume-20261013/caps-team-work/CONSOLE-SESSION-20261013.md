# Console session results (2026-10-13, ~15:05-15:15 UTC)

## #1 kernel-read probe: EXECUTED, 3/4 PASS (console-proven)

ELF: cap01-kread-probe-v0.elf (sha256 12ae76a08fd7afb2...)
Source revision: KE agent build, 2026-10-13
Raw log: cap01-03/probe/EVIDENCE-CONSOLE-20261013.log (804 B, fetched from console)

| Check | Verdict | Detail |
|---|---|---|
| CHECK | PASS | kstuff resident, probe on hooked path (U1 answered: YES) |
| RDMSR LSTAR | FAIL | msr_read_faulted_or_errno — needs offline diagnosis |
| RDMSR EFER | LOGGED | 0x11d01 (correct for x86-64) |
| RDMSR TSC | PASS | Two reads, delta=24062 (real, not cached) |
| READ_DR | PASS | 8 registers, real kernel values (DR7=0x400, DR6=0xffff4ff0) |

Status: standalone kekcall reachability DEMONSTRATED.
Arbitrary kernel-VA reads: PENDING (KREAD sub-op, Phase-B-gated).
Restart validation: OUTSTANDING (needs 3 boots).
LSTAR failure: diagnose return contract offline before any retry.

## #3 code-patch diagnostic: LOADED but NO OUTPUT (needs diagnosis)

ELF: cap03-diag-v0.elf (sha256 8c46206406828446...)
Compile: prospero-clang -Wall -Werror -nostdlib (linked OK, _start present)
Load: pldmgr returned "OK", console survived (kstuff+ftpsrv alive after)
Output: NO log file found at any checked path, no process remaining.

Possible causes (offline diagnosis needed):
1. _start → main chain may be broken (crt initialization missing with -nostdlib)
2. Log file path may differ from expected
3. The probe may have exited before writing (check exit code semantics)
4. The -nostdlib link may have skipped essential initialization

Status: probe-process behavior NOT demonstrated. Game-process behavior
NOT tested. Phase B prerequisite still in effect.

## Console state at end of session
- kstuff resident, ftpsrv alive, no game running
- Both probe ELFs remain in /data/pldmgr/payloads/ (harmless, unused)
- No crashes, no kernel panics, no configuration changes
