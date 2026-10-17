# GO-NOGO.md — checklist for the staged console E2E run (stages 0 / 2a / 2b / 3 / 4-5 / 6)

Origin: worker-e2e-console-staging, 2026-10-04. GO conditions inherit
`worker-e2e-rehearsal/READINESS.md` §5 and update them for the CURRENT
bridge-linked payload (F-R1 closed by `worker-zip-bridge-console-link`;
stage-1 native remains DEFERRED).

## GO requires ALL of the following checked true, in order

- [ ] **G0 Reachability**: `GET /list_payloads` -> 200 (CONSOLE-LOG item 7 pattern).
- [ ] **G1 ARGV DISPATCH (hard gate, top risk)**: a bounded probe proves the
  controller can pass `--state/--roots/verb` arguments to the dispatched
  payload (e2e-status `records` begin+end observed on console). NO workspace
  evidence currently supports argument passing via `/loadpayload`; without
  this, the run cannot execute the argv-driven installer at all.
- [ ] **G2 Payload identity**: `MODBRIDGE_INST_E2E.elf` FTP readback =
  681632 B, sha256 `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9`
  (verified locally against `worker-zip-bridge-console-link/composition/build/console/modbridge-installer.payload.elf`).
- [ ] **G3 FTP write**: bounded STOR+RETR probe on
  `/data/modbridge/intake/` succeeds (upload route for intake files; not
  evidenced anywhere yet). Fallbacks: none evidenced -> NO-GO.
- [ ] **G4 Environment**: game NOT running during stages 0-3; no Townfall
  launch scheduled (stage 4-5 simulated); `/data` writable by payloads.
- [ ] **G5 Intake identity**: `movspeed.zip` readback sha
  `3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad`;
  `mm-skin.mfb` `7b9888b9…`; `mm-lua.mfb` `eefc774a…` (manifest values).
- [ ] **G6 Honesty terms accepted**: stage 4-5 reported as SIMULATED ack
  (no real `running=1` claim); F-R2 and bridge bounds documented in the run
  report; failures reported distinctly, never worked around live.

## Stage-level GO/NO-GO

| Stage | Verdict | Basis |
|---|---|---|
| 0 baseline | **GO** | host-proven (rehearsal S0; probe B); trivial read-only |
| 1 native on-console download | **NO-GO — DEFERRED** | READINESS §5: ps5_io RDWR + §1A relink + owner session missing; console payload has no download transport (bridge REPORT §4.2: import set identical to baseline proves it) |
| 1 owner-placed archive (alternative) | **GO** (conditional on G1/G3) | NEW: `register-mod` accepts a raw zip -> `downloaded` (probe A/B); no sidecar surgery needed |
| 2a prepare-archive | **GO** (conditional) | host-proven with auto-bridge on role-bearing corpus (probe B; zb-pak/zb-lua 75/75) |
| 2b bundle flow | **GO** (conditional) | rehearsal S2b PASS with exact fixtures staged here |
| 3 install | **GO** (conditional) | rehearsal S3 + probe B PASS; `recover` rollback proven in 26/26 suite |
| 4-5 runtime ack | **GO, SIMULATED ONLY** | `simulate-runtime` format proof; real ack NO-GO until runtime-ack runtime + UE4SS exist (READINESS §5) |
| 6 uninstall | **GO** (conditional) | rehearsal S6 + probe B PASS |

## NO-GO immediately (any one suffices)

1. G1 unresolved (no argv route) — run cannot start.
2. Any readback sha/size mismatch (payload or intake) — never dispatch
   unverified bytes.
3. FTP STOR probe fails and no evidenced alternative write route exists.
4. Console would require any game-file modification, autoload change, or
   Townfall launch for these stages — out of scope, HOLD stands (CONSOLE-LOG).
5. Payload-manager dispatch of a name we do not own would be overwritten —
   always the new `MODBRIDGE_INST_E2E` alias only.
6. A stage fails and the only "fix" is replaying an identical failed request
   (studio contract; RUNBOOK-V2 §1C).

## Post-run acceptance rule

E2E passes iff the executed stages all pass in one operator sequence, every
transition is observable in collected `records.txt`/`e2e-status.jsonl`, and
the report labels stage 4-5 `running=1` as SIMULATED. Any failure is reported
distinctly with its stage id and console artifacts; candidate console results
never retroactively upgrade host-side [HOST-PROVEN] claims, and unperformed
steps are never reported as passed.
