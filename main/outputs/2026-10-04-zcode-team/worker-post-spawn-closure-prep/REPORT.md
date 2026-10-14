# REPORT.md — worker-post-spawn-closure-prep

Origin: worker-post-spawn-closure-prep, 2026-10-04. Read-only synthesis + spec; no
console contact, no network commands, no nested subagents. All writes under
`outputs/2026-10-04-zcode-team/worker-post-spawn-closure-prep/`.

## 1. Result

Four deliverables after CLAIM.json, preparing everything the project owes AFTER the
acceptance goal ("visible menu + successful in-game spawn") is met:

| Deliverable | What it locks down |
|---|---|
| `STABILITY-GATE.md` | Post-spawn closure criteria SG-A…SG-E (30-min residence PROPOSAL with T+5/15/30 milestones; N≥5 repeated spawns anchored to EVIDENCE-SPEC rung-5; save/load cycles as a 3-outcome EXPLORATORY probe since a map-change reload was never expected to keep the mod alive; observer growth records as boot-window corroboration with their honest 60 s limit; clean exit) + the crash-report procedure: D1 aligned-entry log as the crash record, the probes' persist-before-mutate (map-persist) discipline lifted to session level (intent in events.log before every risky op; pre-state records flushed before mutation; restore + post_restore re-proof; hard caps; fail-open module_start), rollback §5 recovery ritual, and the crash-taxonomy rule that each crash adds its own do-not rule. |
| `ACCEPTANCE-RECORD.md` | The artifact manifest of a COMPLETE acceptance: archive law (fresh dir per flow run, `runs/<YYYYMMDD-HHMMSS>-<flow>/`, receipt.json as index, controller backups under `outputs/2026-10-04-zcode-controller/console/`), canonical run-dir layout with the exact artifact names verified in `console_batch.py` (cp0-cp4/cp5/cp6-verdict.json, probe-verdicts.json, game-evidence-receipt.json, ini-bootstrap-verdict.txt, allfam8-console-receipt.json, readbacks), per-track tables for the runtime ladder (CP0–CP7 + rungs 1–5 with PASS-law citations), DLL track (dll8 records 37–40, dll7 fallback 41–44), installer E2E track, host-only app track, and staging receipts — plus the completeness rule: `NOT-ATTEMPTED` keeps the acceptance honestly INCOMPLETE. |
| `RESTORE-PATH.md` | Both closure end-states. §A full teardown in journal-safe order: pre-flight hashes → ini FIRST (restore `.bak`/pre-image `8d331b3f…`, stock empty config per pinned `main.c`, or selective `=0`) → only then file removal (never leave `=2` pointing at a deleted file — the proven 25 s attach + 120 s timeout FAIL path) → settings v2 lockstep restore (834 B `9570ead3…` + 446 B `3afd10ce…`) or `/data/modbridge/**` removal → slot stock or r11 restore under the PT_LOAD readback law → verification launch as the teardown receipt. §B leave-installed safety case: fail-open proven across launches L1–L8, activation-root isolation (`/data/modbridge/**` + slots + log channels; game files untouched; tick detour dies with the process), parser-law inertness (mode 0 skip, 7/10 slots), residual-risk table (wrong-address detour, ploader FAIL-retry quirk, title updates void pinned offsets), and the one-byte kill switch; §B.4 dormancy for sessions without a stability pass. |

## 2. Key synthesis decisions (and why)

- **The gate never gates.** SG is positioned strictly after CP5/CP6, read-only, and its
  PASS is a precondition of leave-installed (RESTORE-PATH §B), never of the acceptance.
- **Observer honesty.** The observer PRX (30×2 s, read-only, `ue4ss_log_grew` +
  tail lines + summary counters) proves the log channel was alive at boot; it cannot
  observe post-spawn residence — that gap is filled by host-side size/mtime milestones,
  and a longer window would need a NEW revision (new build ID + re-pinned ini), never a
  silent re-parameterization of the frozen `1afa0997…` PRX.
- **Crash-capture canon adopted from the probes.** The xwp probe's
  `row → vmmap_persist → attempt → restore → post_restore` sequence plus the FINAL-INI
  ordering (probe verdicts flushed BEFORE the composition ladder: "if the ladder dies
  the title, fc1's verdict is already flushed") generalize to the session rule: nothing
  risky happens without an aligned pre-state record on disk and in the run dir.
- **Teardown ordering is the journal.** With no on-console journal for manual DELE, the
  documented end-state + ini-first ordering is the guard (lesson from the app track's
  D1 records-lie window).

## 3. Divergences found and preserved (not silently resolved)

1. dll8 ledger positions: master plan says "records 41–44", the doc-synced decision
   brief says 37–40 — reconciled as dll8=37–40 / dll7 fallback=41–44 (the brief is
   newer and dll7 is explicitly the 41–44 flow); flagged in ACCEPTANCE-RECORD §4.
2. r11 slot identity: rollback runbook's staged composition-5 upload `04869ab3…`
   30,786,775 B (with its own flagged 30,786,663 console-view discrepancy) vs
   EVE-STATE's r11 pldmgr payload `0911af0c…` 30,792,151 B — different artifact classes
   (staged plugin slot vs dispatchable payload); RESTORE-PATH §A.5 requires the Step-0
   fresh RETR as ground truth and forbids whole-file-sha comparison for fself.
3. Stock empty-config teardown (deleting ploader.ini) is pinned-source-proven safe for
   the ploader but NOT verified against pldmgr expectations — kept as option (b) with a
   caveat and a preference for `.bak` restore or `=0` flips.

## 4. Honest limits

- Nothing in the three docs has been console-executed; the acceptance goal itself
  remains [TARGET] (console was down at eve; slot still r11).
- The 30-minute residence threshold, the 2-cycle save/load bound, and the
  `stability-gate-verdict.md` / `spawn-count-table.csv` filenames are PROPOSALs marked
  as such; the N≥5 spawn bound is anchored to EVIDENCE-SPEC rung-5 but still needs
  controller ratification.
- Host-side fetches during the residence window assume FTP tolerates the bounded
  milestone cadence; the never-hammer law (fresh connection, ~60 s backoff) is carried
  over unchanged rather than re-derived.
- No >8-quality, completion, or parity claim is made anywhere; per the standing law,
  these docs define what WOULD count — they certify nothing.
