# REPORT — worker-cert-reverify (2026-10-05)

## Task
Read-only spot re-verification of the six documentation fixes (commit `4e6c995` + the README pointer) that closed worker-return-readiness-cert item (f), intended to let the cert honestly read 9/9. Scope: re-check the fixed cites on disk, spot-check for new inconsistencies introduced by the mechanical swaps, append a RE-VERIFICATION section to READINESS-CERT.md. No console contact, no nested subagents.

## Method
1. Read the original cert (preserved verbatim; snapshot at `receipts/READINESS-CERT.as-of-pre-reverify.md`) and the `4e6c995` diff to enumerate exactly what the controller changed (4 files edited: README-RETURN.md, RETURN-SESSION-COMMANDS.md, INDEX-FINAL.md; 5 files newly committed as-is: digest, brief, EVE-STATE, runbook, REPORT/CLAIM of index worker).
2. Recomputed every decisive hash on disk: batch tool, r11 slot source, composition4 backup, canonical 9-entry ini (both copies, plus byte size and `= 2`/`= 0` census).
3. Read the current on-disk text of each named line (cheat sheet 5/51, runbook 5/31, digest 33/57, INDEX 44/68/86/88/123/124, EVE-STATE 38/60/66, brief 185) and grep-swept all seven documents for superseded values (`456f9372`, `b00b3a69`, `bbc34c16`, `composition4-console-backup`, "count 7", "eight-sink").
4. Existence-checked the README's new `RESTORE-PATH` pointer target.

All evidence is frozen in `receipts/REVERIFY-RECEIPTS.txt`.

## Findings
**The cert cannot honestly read 9/9. Two of the six named fixes were never (fully) applied, and one mechanical swap corrupted a hash. (f) remains NOT-READY; the cert stays 8 of 9.** Details in CLAIM.json and the appended cert section; the short form:

Fixed and verified: f.1 (tool rev `5ca8d1c7` in cheat sheet + runbook, = recomputed tool sha), f.5 (brief sink count "seven"), and the 9-entry/8-registered law text in digest/INDEX/EVE-STATE line 66 (ground truth confirmed: `fd9c73937b2b1e45…`, 440 B, census 8×`= 2` + 1×`= 0`).

Still blocking:
1. Cheat sheet line 51 — the comp6-slot restore still points at `composition4-console-backup.prx` (`d4fc0acf…`), never edited (half of original blocker 6).
2. EVE-STATE L11 — the swap produced a hybrid hash (`fd9c7393` prefix + superseded `bbc34c16` tail) matching no on-disk artifact, with stale size 400 (true 440). This is a NEW inconsistency introduced by the fix (task-2 spot-check hit).
3. INDEX line 123 — stale census "7×`= 2` + 1×`= 0`" now contradicts its own "exactly 8 registered"; "400 B" retained on lines 68/123.
4. Digest — "400 B" retained on lines 33/57; line 57 pairs `5ca8d1c7` with "selftest 372/0" (final bytes are 405/0).
5. README line 39 — r11 identity cited, but the pointer string "`C/console/r11 restore chain 0911af0c (RESTORE-PATH)`" is not a resolvable path and omits the verified local source (`worker-composition-5-rebuild/prx-candidate-composition5-1/modbridge_supplier_composition.prx`).

Pre-existing residuals outside the six (documented for the next pass, not introduced by the fixes): INDEX lines 44/68/86/124 still pin `456f9372`/372-0 as "final/VERIFIED in place" (on-disk = `5ca8d1c7`); EVE-STATE line 60 "registered count 7"; runbook line 31 "ge.2 eight-sink fetch"; brief lines 9/293 "FINAL `b00b3a69…`".

## Verdict delivered
READINESS-CERT.md now carries the appended RE-VERIFICATION section: **(f) NOT-READY (partially fixed); overall remains 8 of 9 READY.** The original verdict is untouched history. Every blocker is a one-line text edit; no artifact, gate receipt, or restore byte is in question. Stamping 9/9 against this evidence would have repeated the unread-docs claim pattern the workspace's own quality law forbids.

## Deliverables
- `CLAIM.json` (first) — per-item re-check results, recomputed hashes, honesty notes
- `../worker-return-readiness-cert/READINESS-CERT.md` — updated cert (append-only; snapshot of the pre-append original in `receipts/`)
- `receipts/REVERIFY-RECEIPTS.txt` — hashes, named-line citations, residual grep, existence checks
- `REPORT.md` (last) — this file
