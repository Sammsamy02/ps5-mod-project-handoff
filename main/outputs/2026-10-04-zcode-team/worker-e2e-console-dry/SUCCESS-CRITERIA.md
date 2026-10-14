# SUCCESS-CRITERIA.md — what constitutes a PASS for the full console E2E

Origin: worker-e2e-console-dry, 2026-10-04. Scope: one operator sequence of
stages 0 / 1-owner / 2a / 2b / 3 / 4-5 / 6 on console ps5-host-not-configured.invalid with the
D1-rel merged-chain payload, per `CONSOLE-E2E-EXECUTION.md`. Inherits
`worker-e2e-console-staging/GO-NOGO.md` §"Post-run acceptance rule" and
`worker-e2e-rehearsal/READINESS.md` §5. PASS requires EVERY criterion below;
partial runs are reported as partial, never rounded up.

## 1. Identity and environment gates (before any dispatch)

- [ ] G0 reachability: `GET /list_payloads` → 200.
- [ ] G2 payload identity: `MODBRIDGE_INST_E2E.elf` FTP readback = 681528 B,
      sha256 `e3489297193a639aab19567bad680f7a4687d7a37d841ee3545ea4502b4a0436`
      + ELF magic (F-R3 size-law receipt; import-set identity per
      STAGING-MANIFEST-D1REL.md). MODE B additionally requires the
      config-entry revision's own fresh re-pin — the `e3489297…` build never
      runs MODE B.
- [ ] G3 FTP write: STOR+RETR round-trip of `movspeed.zip` succeeds with
      sha `3850a2b5…d6ad`; `mm-skin.mfb` `7b9888b9…70af`; `mm-lua.mfb`
      `eefc774a…9ec` (G5 intake identity).
- [ ] G4 environment: game NOT running during stages 0-3; no Townfall launch;
      only the `MODBRIDGE_INST_E2E` alias is touched.
- [ ] G1 resolved: MODE A (argv probe GO, grammar recorded) or MODE B
      (config-entry revision staged). An unresolved G1 is not a run.

## 2. Dispatch discipline (every dispatch, both modes)

- [ ] MODE B: exactly one sha-verified cmd file per dispatch — pre-STOR
      absence check passed, readback sha equal, consumed-rename verified
      (`e2e-cmd.json` gone, `.consumed` hash-equal). `cmdfiles.log` has one
      line per dispatch.
- [ ] Every dispatched command appears in e2e-status.jsonl with a `begin`
      and a MATCHED `end` (exit code recorded). Any unmatched begin from a
      controlled command is a FAILURE of that stage (only `_exit(86)` crash
      probes leave unmatched begins, and this run performs none).
- [ ] No identical failed request is ever replayed (studio contract): a
      failed stage ends the run or proceeds via a DISTINCT, documented
      corrective command.

## 3. Per-stage verbatim outcomes (the PASS blocks)

All blocks exactly as written in CONSOLE-E2E-EXECUTION.md; summary:

| Stage | Verdict requires |
|---|---|
| 0 baseline | begin `records` stage-0-baseline + `end exit 0`; records.txt header-only |
| 1-owner | begin/end `register-mod` stage-1-download exit 0; records row movspeed `downloaded` |
| 2a | 4 lines: begin + `manifest-v1.bin` + `auto-bundle composed mfb` + end 0; movspeed `prepared`, bundle `state/bundles/movspeed.mfb` exists on console |
| 2b | 2 register begin/end pairs + prepare begin/end exit 0; mm-skin, mm-lua `prepared` |
| 3 | begin + terminal status `{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}` + end 0; root `PPSA33286` with `.modforge-root` + `loadlist.txt`; game row generation 1 with loadlist sha |
| 4-5 | two simulate begin/end pairs stage-4-5-runtime-ack exit 0; records show activated running=0 then mods-started running=1 — **labeled SIMULATED in the report; never claimed as real runtime acceptance** |
| 6 | three removal begin/end pairs exit 0 with NO status line; generations advance; removed mods end `prepared`; final root = empty managed set; closing records begin/end |

Ledger: 33 new e2e-status lines total, 0 unmatched, 0 failed ends on the
happy path. A controlled refusal (exit 2) anywhere is a STAGE FAILURE —
reported distinctly with its begin + matched failed end + quarantine
retained; it is honest evidence, not a run killer by itself, but the run
cannot be called PASS with any stage failed or skipped.

## 4. Truthfulness of records

- [ ] Every durable transition observed in collected `records.txt` matches
      the command that caused it (downloaded → prepared ×3 → installed gen 1
      → activated → mods-started → removals → all `prepared`, final
      generation 4, empty managed root).
- [ ] The loadlist sha in the game row is treated as the only install commit
      truth; it is recorded, never fabricated.
- [ ] Collected artifacts (`MODBRIDGE_INST_E2E-e2e-status.jsonl`,
      `MODBRIDGE_INST_E2E-records.txt`, `cmdfiles.log`, readback shas) are
      the raw files, not summaries; raw reports stay unedited.

## 5. Uninstall restores state

- [ ] After stage 6: zero `installed` mod rows; every mod that participated
      is `prepared`; the install root contains only `.modforge-root` and a
      header-only `loadlist.txt` (no mod files, no `.next`/`.prev`, no
      journal, no `.tmp`, no runtime status file).
- [ ] Nothing outside `/data/modbridge/**` and the `MODBRIDGE_INST_E2E`
      alias was created, modified, or deleted (verified by the controller's
      own before/after listing of `/data/pldmgr/payloads`).

## 6. Honesty gates on the report

- [ ] Stage 4-5 `running=1` labeled SIMULATED (R-ACK-01); no real runtime
      acceptance claimed.
- [ ] Stage 1 native download reported DEFERRED (R-DL-01); the owner-placed
      archive path is what ran.
- [ ] G1 mode recorded with its evidence (argv probe record or config-entry
      revision sha); no improvised wrappers or unreceipted payloads.
- [ ] Console results never retroactively upgrade host-side [HOST-PROVEN]
      claims; unperformed steps are never reported as passed; quality
      language obeys docs/QUALITY-RUBRIC.md (this is a functional E2E, not a
      quality score).
- [ ] Any deviation (missing auto-bundle line, refusal, size/sha mismatch,
      ledger count off 33) is reported distinctly with stage id and console
      artifacts, and stops further stages that depend on it.

PASS = sections 1-5 all checked in ONE operator sequence with the collected
raw evidence attached; anything less is NO-GO/PARTIAL with the exact failing
criterion named.
