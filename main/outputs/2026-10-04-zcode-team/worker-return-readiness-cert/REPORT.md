# REPORT — worker-return-readiness-cert (2026-10-04)

FINAL read-only certification pass over the return-session claim list. Deliverable order kept: CLAIM.json (first) → READINESS-CERT.md → this REPORT.md (last). Budget: ~30 tool calls, used ~30.

## Method

1. **Hash everything decisive, trust no doc.** sha256 recomputed on disk this pass for: the five probe fselfs + bare-sdk ELFs (readback body constants recomputed from bytes `64..readback_bytes`, not copied); the three candidate payloads (+ the C/console mirror copies); five gate receipts (parsed for verdict + failed_checks, then hashed); settings v3 pair; 9-entry canonical ini; `console_batch.py` + `post_launch_check.py` + `rung_check.py` + `run_host_tests.sh` vs `receipts-final-sha256.txt` (4/4 match); the ledger snapshot vs its frozen index; the r11 slot source.
2. **Trace the tool's receipt lineage.** `console_batch.py` has four documented generations: `870ee937…` (BUILD-RECEIPT, selftest 122/122) → `b00b3a69…` (8-entry adoption, 372/0) → `456f9372…` (ninth-slot adoption, selftest 405/0 receipt) → `5ca8d1c7…` (current; ONE label word "eight"→"seven" in the ge.2 step title, receipted by worker-evidence-tools-final with selftest 405/0 + fake drive 92/0 re-run on the final bytes). The current bytes are therefore receipt-covered; the stale pins live only in docs (cert item f).
3. **Consumer check.** For each artifact, the docs that cite it were grepped for the correct hash/size/law: cheat sheet rev c, brief, launch guide, USER-SESSION-GUIDE, EVE-STATE, digest, INDEX, both matrices, FINAL-INI-LAYOUT, CONSTANTS-BRIEF.
4. **No execution.** Read-only: no selftest/drive re-run, no console contact, no nested subagents. Receipts were accepted via hash identity between receipted and on-disk bytes.

## Findings

### The one NOT-READY: documentation chain (item f)
The workspace evolved past its own docs in the final hours: the ninth-slot ini adoption (02:27) and the final tool relabel (03:39→`5ca8d1c7…`) postdate the digest (01:45), EVE-STATE (00:09), INDEX-FINAL (02:41, half-updated), and the cheat-sheet/runbook tool-rev pins (02:40/02:55). Concretely: digest line 33 + INDEX lines 44/68/86/88/123-124 + EVE-STATE line 66/L11 teach "8-entry ini, 400 B `bbc34c16…`, 7 registered, tool `b00b3a69…`/`456f9372…`", while the operative law everywhere else is "9 entries, 440 B `fd9c7393…`, 8 registered, tool `5ca8d1c7…`" (README §4 and the launch guide already say EXACTLY 8 OK lines — correct). The brief's "eight evidence sinks" (line 185) and the cheat sheet's `composition4-console-backup.prx` r11-restore pointer complete the list. None of these touch artifact bytes; all are one-line doc edits. Under the workspace's own law (divergence = STOP + reconcile), they must be reconciled before the session, not during it.

### Everything else verified clean
- The five probes are byte-exact against the tool's pinned constants, including the readback-body windows — the cc1 brief-constant divergence is receipted and correctly resolved in-tool (`131ff1c2…`).
- The three candidates are on disk with GO gate receipts (failed_checks empty); the two preserved NO-GOs (launch-9's first packaging attempt; dry-compose a-vs-l10 evidence-form attempt) are documented in their workers' failure-history sections per the never-replay law.
- Settings v3 pair, freeze inventory (v2 rollback pair), rollback flows, archive scaffold (4 flows incl. game-launch10), and the ledger snapshot all check out.
- Restore-path completeness: even the r11 slot content exists locally (`worker-composition-5-rebuild/prx-candidate-composition5-1` = `0911af0c…`), so the comp6 slot is recoverable without console cooperation.

### Honest limits
- Console-side state (S1–S3, DOWN) is not certifiable from the Mac and is not certified; EVE-STATE itself marks those identities as last-VERIFIED reads to re-confirm at open (commands 1–2).
- Desk rehearsal (78/0) exercised tool generation `456f9372…`; the delta to `5ca8d1c7…` is one doc-string with no classifier/flow change (per the final audit's diff + unchanged 405/0 and 92/0 counts), so the rehearsal result carries over by identity of behavior, not by re-run.
- This cert rates readiness of artifacts and receipts, not in-game outcomes: launch-9's `[Seeds]` stop remains a prediction, Item Spawner activation remains never-observed.

## Deliverables
- `CLAIM.json` — verdicts, blocking items, decisive hashes (first)
- `READINESS-CERT.md` — the one-page certificate
- `REPORT.md` — this file (last)
