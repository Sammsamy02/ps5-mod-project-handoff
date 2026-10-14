# PREREQS.md — preconditions for the staged console E2E run (stages 0 / 2a / 2b / 3 / 4-5 / 6)

Origin: worker-e2e-console-staging, 2026-10-04. Sources cited inline. Stage-1
NATIVE download is DEFERRED (`worker-e2e-rehearsal/READINESS.md` §5 NO-GO) and
has no prereqs listed here beyond the manifest note.

## P1. Environment (C4 class, `worker-installer-ps5io/CONSOLE-E2E-PLAN.md` C4)

- Jailbroken PS5 at ps5-host-not-configured.invalid; pldmgr HTTP :8084 responsive
  (`GET /list_payloads` -> 200; evidenced pattern, CONSOLE-LOG item 1/7).
- Anonymous FTP :2121 reachable for RETR readback (CONSOLE-LOG item 3/5).
- **FTP WRITE (STOR) is NOT evidenced anywhere in workspace records** —
  CONSOLE-LOG shows only HTTP `/manage:upload` (payload-dir-scoped) and FTP
  RETR (reads). `/data/modbridge/intake/<file>` placement therefore needs a
  bounded STOR probe first (STEP-BY-STEP step P2). If STOR fails, there is no
  evidenced route to place the intake files and the run is NO-GO (G3).
- FTP server refuses `/tmp` paths (CONSOLE-LOG item 8, recorded controller
  error): every path this run creates or reads must live under `/data/`.
- `/data/modbridge` writable by the payload (payload-written files under
  `/data` are proven: CONSOLE-LOG item 5 result log).
- Title under test: `PPSA33286` (all rehearsal/bridge evidence uses it).
- **Game NOT running during stages 0-3** (C4). No Townfall launch is needed
  at all for stages in scope: stage 4-5 is `simulate-runtime` (ack-file
  format proof only, READINESS §1 S4/5).

## P2. Payload identity gates

- `MODBRIDGE_INST_E2E.elf` readback via FTP RETR must equal
  681632 bytes and sha256
  `cfa96ab5aa408491c74d996f65058b737274a5bb109a7de6e213f9554bbda4f9`
  (full-hash equality is possible here: plain ELF, not FSELF; contrast the
  decryption-oracle caveat for FSELF in ZCODE-FORK-HANDOFF launch ritual).
- Intake files readback must match manifest shas before any dispatch.

## P3. Invocation mechanism — OPEN GATE G1 (highest risk)

The installer is argv-driven (`modinst.c:1495-1522` parses
`--state/--roots/--title/--mods/--remove/--id/--bundle/--rt-state/...`; entry
`main` at `modinst.c:1480`). **No workspace record evidences that pldmgr
`GET /loadpayload:<path>` forwards query/extra arguments**; every prior
dispatched payload (NEXT_DLL_THREADS, INITONCE_DLL_TEST, ingame probes) was
argument-free by design. Before any multi-step run, the controller must
resolve G1 with a bounded probe (STEP-BY-STEP step G1): dispatch one command
and check `/data/modbridge/state/e2e-status.jsonl` for the corresponding
begin/end. If no argv route exists, the staged run is NO-GO until the
controller provides an argv-capable dispatch (controller-owned decision; not
this worker's scope).

## P4. Honest-claim boundaries accepted before the run

- Stage 4-5 `running=1` this run is SIMULATED (`simulate-runtime` writes the
  status file through the `modforge_activation` mfa_status API — format
  proof only). A real `running=1` requires the runtime-ack emissions in the
  injected runtime + UE4SS present (READINESS §3 items 4-5, §5 NO-GO rule).
  The run report must label it simulated; no real-acceptance claim.
- Known open defects that may appear and are NOT failures of this run:
  F-R2 dangling `begin` on install-refusal paths (READINESS §5 minor;
  observed as `zb-off 1 unmatched begin`); bridge v1 bounds refuse archives
  >256 members / >64 MiB members / >1 MiB manifest (bridge REPORT §4).
- Role-bearing archives auto-bridge to `.mfb` at `prepare-archive`
  (`auto-bundle composed mfb` status line); role-less archives prepare but
  stay un-bridged and `install` will refuse them (probe A vs B in REPORT §3).
  Only stage corpus #2/#3/#4 from the manifest, which are proven.
- No credentials, no provider sessions, no game-file changes, no autoload
  changes; game-file HOLD honored (CONSOLE-LOG header).
- One dispatch per command instance; never replay a failed dispatch without
  a changed precondition (studio contract; RUNBOOK-V2 §1C failure rule).

## P5. Sequencing constraints

- Readback-verify each staged file BEFORE the step that uses it.
- Stages 0 -> 1-owner -> 2a -> 2b -> 3 -> 4-5 -> 6 in one operator sequence
  (CONSOLE-E2E-PLAN acceptance); each stage's expected records/e2e-status
  lines are in STEP-BY-STEP and were reproduced host-side on THIS binary
  (REPORT §3 probe B) — console execution remains the [TARGET-OPEN] truth.
