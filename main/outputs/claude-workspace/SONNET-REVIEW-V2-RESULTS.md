# Deployment Artifact Verification v2 - Results

Run: 2026-10-06 (build ran 20:14:37Z - 20:15:15Z). Scope followed as instructed: no kernel emitter source read, no test source read. Paths below are relative to `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/`.

Summary table

| # | Check | Result |
|---|-------|--------|
| 1 | ELF validity (SampleApp.elf, InstallerApp.elf) | PASS |
| 2 | Build completes all 3 stages | PASS |
| 3 | Eboot patch verification | PASS (with a path discrepancy, see below) |
| 4 | Integration test results | FAIL (no test result output exists to read) |
| 5 | mb.prx is a valid ELF | FAIL as literally stated (it is an FSELF, not a bare ELF; see below) |

---

## 1. ELF file validity - PASS

Checked both before and after the check-2 rebuild. The rebuild regenerated both files, so the "after" values are the current ones.

`2026-10-04-zcode-team/worker-kernel-deploy/source/samples/kstuffSharp/out/SampleApp.elf` (5,743,072 bytes)
- `file`: `ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, not stripped`
- Header bytes: `7f45 4c46 02 01 01 00 ...` (magic `\x7fELF`, class 2 = 64-bit, little-endian), `e_type` = `0x0003` (ET_DYN), `e_machine` = `0x003e` (x86-64)
- sha256 (post-rebuild): `22385fbc145bf87d3feb77e5f20bf152efe9e9594f8f3cef193de2a93f80120f`

`2026-10-04-zcode-team/worker-kernel-deploy/source/samples/kstuffSharp-installer/out/InstallerApp.elf` (2,890,736 bytes)
- `file`: `ELF 64-bit LSB pie executable, x86-64, version 1 (SYSV), dynamically linked, not stripped`
- Header bytes: `7f45 4c46 02 01 01 00 ...`, `e_type` = `0x0003`, `e_machine` = `0x003e`
- sha256 (post-rebuild): `9f2f586cff7d04d8834a7ec8f31067a5436990a5ecd02c271ca5ab4ad6719f44`

Answer: yes, both are valid x86-64 ELF files by `file` and by magic bytes.

## 2. Build completion - PASS

Ran `/bin/bash 2026-10-04-zcode-team/worker-kernel-deploy/build-mac.sh`. Full log (361 lines, including start/finish timestamp lines I added) is at `claude-workspace/BUILD-LOG-V2.txt`.

- Exit code: 0 (`== build finished 2026-10-06T20:15:15Z, exit code 0 ==`)
- Stage 1/3 Installer payload: compiled `InstallerApp.o` for linux-x64, gathered 13 runtime-support archives, linked, `Wrote .../InstallerApp.elf (2890736 bytes)`
- Stage 2/3 Embed installer: `Embedded 2890736 bytes into .../samples/kstuffSharp/InstallerBlob.g.cs`
- Stage 3/3 Loader payload: compiled `SampleApp.o`, linked, `Wrote .../SampleApp.elf (5743072 bytes)`
- Final banner `==================== Complete ==================================` printed, followed by `ls -l` of both ELFs.

Observations from the log (not failures, but worth knowing):
- Two `ranlib ... the table of contents is empty` warnings (one per payload, on the `.a` archives from `dotnet publish`). The script only consumes the `.o`.
- The linker summary reports `Imports: 0` and `Unresolved: 132` symbols for InstallerApp and `Unresolved: 158` for SampleApp. These are listed as `? symbol` lines (e.g. `__cxa_atexit`, `write`, `waitpid`) and the link still succeeded and wrote output. I did not analyze whether these are expected to resolve at load time on the console.
- Side effect of running the build as instructed: it rewrote `source/samples/kstuffSharp/InstallerBlob.g.cs` (tracked in git, now shows as modified) and regenerated both `.elf` files and `obj/` and `bin/` contents. I did not commit anything.
- The pre-existing `worker-kernel-deploy/build.log` is truncated (ends after the first `SharpProspero ->` line), so it was not usable as evidence of a complete build. I did not modify it.

## 3. Eboot patch verification - PASS (path discrepancy)

The file named in the task, `2026-10-04-zcode-team/worker-early-load/verify-eboots.txt`, does NOT exist. `worker-early-load/` contains only `dynamic-table.txt`, `EARLY-LOAD-DESIGN.md`, `eboot-earlyload-A1.bin`, `eboot-earlyload-A2.bin`, `patch-apply.py`, `patch-verification.txt`. The file of that name is at `2026-10-04-zcode-team/worker-eboot-patch/verify-eboots.txt`, and I read that one (plus `worker-early-load/patch-verification.txt` as a cross-check).

Evidence from `worker-eboot-patch/verify-eboots.txt` (header: "console contact: false"):
- Base eboot pins confirmed: `base_toplevel` sha256 `afe0a2a5...30b7e` CONFIRMED; `base_decrypted` sha256 `3b6cfad4...622fc` CONFIRMED.
- A1 (194,241,424 bytes, sha256 `3374b5ab...1960`): `result: PASS`. ELF magic `7f454c46`, class 2, machine `0x3e`, e_type `0xfe10`, phnum 14; `PT_DYNAMIC` has 58 DT_NEEDED with DT_NULL intact at slot 260; strtab entry `'mb.prx'`; diff vs base_toplevel is 10 bytes, matching the expected 10.
- A2 (194,241,424 bytes, sha256 `cbb4ee4d...5e34`): `result: PASS`. Same ELF/dynamic checks; diff vs base_toplevel is 31 bytes, matching the expected 31.
- `worker-early-load/patch-verification.txt` agrees: the same A1 sha256 `3374b5ab...` (10 bytes changed) and A2 sha256 `cbb4ee4d...` (31 bytes changed), DT_NEEDED count 48 -> 58, DT_NULL intact, `exit 0`.

Answer: yes, the file confirms the patched eboots (A1 and A2) pass structural verification. Caveat: this is an offline byte/structure verification only ("console contact: false"); it says nothing about whether the console loads the patched eboot.

## 4. Integration test results - FAIL (no results to read)

`2026-10-04-zcode-team/worker-ue4ss-kernel/host-test/` contains:
- `facade_test.cpp`, `pad_adapter_test.cpp`, `pool_gap_test.cpp`, `mock_kekcall.hpp` (test sources; not read, per instructions)
- `run.sh` (runner script; not read)
- `facade_test.out` - this is a compiled executable (`Mach-O 64-bit executable arm64`, 233,232 bytes), not a text result log
- `facade_test.out.dSYM/` - debug symbols for that binary

There is no log, report, or captured stdout/stderr from any test run in that directory. Only one of the three test sources (`facade_test`) has a compiled binary at all; there is no binary for `pad_adapter_test` or `pool_gap_test`. So I cannot say that tests pass. I did not execute `run.sh` or `facade_test.out`, because the instruction was to read existing output files only and executing would be outside that scope. If a pass/fail answer is needed, the next step is to run `run.sh` and capture its output.

## 5. mb.prx validity - FAIL as literally stated (it is an FSELF, not a bare ELF)

`2026-10-04-zcode-team/worker-eboot-patch/mb.prx` (30,151,623 bytes)
- `file`: `data` (not recognized as ELF)
- First bytes: `4f15 3d1d 0001 0112 0101 ...` - magic `4f 15 3d 1d` is the PS4/PS5 SELF/FSELF container magic, not `7f 45 4c 46`.

So the literal answer to "is it a valid ELF?" is no: it is not a raw ELF. Supporting context so this isn't misread:
- `worker-eboot-patch/verify-mbprx.txt` (existing output, not generated by me) reports `result: PASS` and describes `mb.prx` as `FSELF`: fake-sign mode 1, little-endian, attribs 0x12, key type 0x101, six wrapped PT segments, and all wrapped segments byte-equal to the bare ELF.
- The bare ELF counterpart `worker-eboot-patch/mb-plain.prx` (30,146,024 bytes): `file` reports `ELF 64-bit LSB x86-64, version 1 (FreeBSD)`, magic `7f454c46`, OSABI 9, e_type `0xfe18` (SCE dynlib). verify-mbprx.txt records it as byte-equivalent to the frozen bare SDK build and identifies the module_start/module_stop exports.

My read: `mb.prx` is the expected wrapped (FSELF) form of a valid ELF, and the check's wording ("valid ELF") does not match the format the file is supposed to have. I am still marking it FAIL because the check as written is not met by `file`/magic bytes, and I only ran what the task listed. A reviewer who accepts "valid FSELF wrapping a valid ELF (mb-plain.prx)" as the criterion would mark this PASS.

---

## OVERALL: NO-GO

Reasons:
- Check 4 has no evidence: there are no test output files, only an unexecuted compiled binary. Test status is unknown, not known-good.
- Check 5 does not meet the literal criterion (`file` says `data`; it is an FSELF). The supporting verification output suggests this is a criterion mismatch rather than a defect, but it was not confirmed under the stated test.
- Checks 1, 2, and 3 pass. Check 3 relies on a file found at a different path than the one named, and all of the checks here are offline artifact checks with no console contact.

What would flip this to GO: a captured passing test run for the host-test suite (check 4), and acceptance that "valid FSELF wrapping `mb-plain.prx`" satisfies check 5.
