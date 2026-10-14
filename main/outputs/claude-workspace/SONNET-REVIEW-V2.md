# Deployment Artifact Verification (v2 — limited scope)

Verify these deployment artifacts are ready. Do NOT read or analyze any kernel emitter source code. Only check file validity and test results.

## Check these 5 things:

1. **ELF file validity**: Run `file` and check magic bytes on:
   - outputs/2026-10-04-zcode-team/worker-kernel-deploy/source/samples/kstuffSharp/out/SampleApp.elf
   - outputs/2026-10-04-zcode-team/worker-kernel-deploy/source/samples/kstuffSharp-installer/out/InstallerApp.elf
   Are they valid x86-64 ELF files?

2. **Build completion**: Run the build script at outputs/2026-10-04-zcode-team/worker-kernel-deploy/build-mac.sh and capture the FULL log. Does it complete all 3 stages? Write the new log to outputs/claude-workspace/BUILD-LOG-V2.txt

3. **Eboot patch verification**: Read outputs/2026-10-04-zcode-team/worker-early-load/verify-eboots.txt — does it confirm the patched eboot is valid?

4. **Integration test results**: Read any test output files in outputs/2026-10-04-zcode-team/worker-ue4ss-kernel/host-test/ — do tests pass? (Just read the output files, don't read the test source code.)

5. **mb.prx validity**: Run `file` on outputs/2026-10-04-zcode-team/worker-eboot-patch/mb.prx — is it a valid ELF?

## Write results to: outputs/claude-workspace/SONNET-REVIEW-V2-RESULTS.md

Format: for each of the 5 checks, state PASS/FAIL with the evidence you observed.
End with: OVERALL GO or NO-GO.
