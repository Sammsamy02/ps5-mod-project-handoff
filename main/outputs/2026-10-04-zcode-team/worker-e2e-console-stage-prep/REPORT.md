# REPORT — worker-e2e-console-stage-prep, 2026-10-04

Task: prepare the controller's zero-decision console staging for the revised
config-entry installer payload (MODE B E2E run). No console contact, no
nested subagents; all writes inside this worker dir. CLAIM.json first; this
report last.

## Deliverables (this dir, required order)

1. `CLAIM.json` — scope, pins, constraints.
2. `CONSOLE-STAGE-PLAN.md` — exact ordered controller commands: local
   pre-flight, Step P upload (POST `/manage:upload?filename=MODBRIDGE_INST_E2E.elf`),
   `/manage:check` + FTP readback sha gate, the 15-dispatch stage 0→6 cycle
   (`cmd_put`/`dispatch`/`cmd_gone`/`evid`), Step C evidence collection, and
   the full-tree reset at the end.
3. `EXPECTED-LEDGER.md` — verbatim expected e2e-status.jsonl shapes per stage
   (0, 1, 2a, 2b, 3, 4-5, 6 + closing), exit taxonomy incl. the acked
   EX_CONFIG 6 delta, and the 15-row failure table with recovery.
4. `REPORT.md` — this file.

## Verifications actually performed this session (tool evidence, not assumed)

- Payload: re-hashed `build-r1/modbridge-installer.payload.elf` →
  `a21771f894dd43be3dc650b12ffa65ccdc6413d150c0b9fb338302836901a054`, 681888
  bytes, magic `7f 45 4c 46` — matches PAYLOAD-DELIVERY exactly. The staged
  bytes are the pin; r2 (`ec8acdfc…`) is determinism proof only.
- Cmd files: `shasum -a 256 -c SHASUMS.txt` in `worker-e2e-staging-final/cmd/`
  → 13/13 OK.
- Run inputs re-hashed: movement-speed-2x.zip `3850a2b5…`, mm-skin.mfb
  `7b9888b9…`, mm-lua.mfb `eefc774a…` — all match the STAGED §4 expectations.
- Readback law confirmed from precedent, not assumption: ALLFAM_DLL_TEST plan
  (`worker-dll-console-stage-prep/CONSOLE-STAGE-PLAN.md` §2) cites 6d295f56
  byte-identical readback and 1a6b37b7 `ftp_readback_sha256 == artifact sha,
  attempts: 1` (re-read the 1a6b37b7 receipt directly: `upload.ftp_readback_
  sha256` equals `artifact_sha256`). Payload ELFs go through `manage:upload`
  verbatim — the PRX/fself SHT-strip law (packaged PRX readback has section
  headers stripped, `e_shnum=0`, cf. worker-import-audit-weakify) does NOT
  apply to this route. Ledger demands readback == `a21771f8…` / 681888 EXACT.

## Key findings the controller must ack

1. **Port erratum**: PAYLOAD-DELIVERY.md §3 says `HTTP=…:9223`; every proven
   source (console-context.json `manager_version_endpoint` → 200 "0.5.2",
   STAGED-E2E-COMMANDS §1, prior stage plan, all controller receipts) says
   **8084**. All "9223" hits elsewhere are sha substrings. Plan pins 8084;
   never probe alternates.
2. **Sequencing (task 2)**: run this stage BEFORE the UE4SS composition-5
   session, payload-only (game closed, manager up, no ploader arm) — same rule
   as all four family payload runs (`game_launched: false`). Current critical
   path is pre-console (comp5 gate offline, anchor-host-test just GREEN), so
   the console should be idle now. Hard conflicts: S3 requires game NOT
   running (C4); never interleave with a live ploader/PRX session; never run a
   partial sequence (stages 0-2 leave registrations); do not let a UE4SS/bridge
   session observe the tree between S3 and S6. Post-run full-tree reset hands
   back the pre-E2E state.
3. **Refusal delta**: present-but-refused exits **6 (EX_CONFIG)**, not the
   schema §6/§8 EX_BAD — controller-acked delta (design sec 7), with at most
   one `{"event":"status","stage":"e2e-cmd","outcome":"refused","detail":…}`
   line when `state_root` validated; zero such lines in a green run.
4. **Stage-6 correction carried forward**: removals emit the F-R2 status line
   while mods remain (detail = remaining mods in registration order); only the
   final removal is silent. Green-run total = 35 lines, not the dry doc's 33.

## Honesty / bounds

Everything here is HOST-side preparation; console behavior (STOR/readback/
consumed-rename, timings, every expected line on target) remains [TARGET]
until the controller run. No >8-style quality claim, no completion claim, no
console contact. The plan reuses the STAGED §4 expected blocks verbatim
(host-proven 35/35, `31a4764a…`) rather than re-deriving them; the only
additions are the MODE B consumed-twin proof, the refusal-line shape, the port
pin, and sequencing/reset framing. Writes: 4 files in this dir only.
