# Pre-Deployment Review Task (Sonnet only — Opus must not read this file)

The runtime team finished building all deployment artifacts. Review them for correctness before deployment.

## Files to review:

1. `outputs/2026-10-04-zcode-team/worker-kernel-deploy/build.log` — did the build succeed cleanly?
2. `outputs/2026-10-04-zcode-team/worker-kernel-deploy/source/tools/SharpProspero.Link/KernelModuleEmitter.cs` — check lines 4452-4600 (new service handlers) and lines 5097-5900 (trap continuations). Complete and correct?
3. `outputs/2026-10-04-zcode-team/worker-early-load/verify-eboots.txt` — verify the patched eboot
4. `outputs/2026-10-04-zcode-team/worker-ue4ss-kernel/host-test/` — do the integration tests pass?
5. `outputs/2026-10-04-zcode-team/worker-deploy-plan/DEPLOYMENT-RUNBOOK.md` — is the deployment sequence safe?

## For each artifact answer:
- Is it complete and correct?
- Any bugs, missing pieces, or unsafe steps?
- Risk level (low/medium/high)?
- Fix something first or proceed?

## Also verify:
- Built ELF files at `worker-kernel-deploy/source/samples/*/out/` are valid (magic bytes, architecture, size)
- Emitter changes vs original at `.superpowers/native-dependency-spike/sharpprospero-legacy-ctors-1/source/tools/SharpProspero.Link/KernelModuleEmitter.cs` — what changed, anything broken?
- `worker-eboot-patch/mb.prx` is a valid loadable library

## Output:
Write your findings to `outputs/claude-workspace/SONNET-REVIEW-RESULTS.md`
Give a GO/NO-GO verdict with specific reasons.
