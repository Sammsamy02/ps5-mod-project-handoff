# REPORT — worker-final-cert-spot-audit (2026-10-04)

## Task
One final independent spot-audit of exactly the six previously-broken items (the RE-VERIFICATION blockers in `worker-return-readiness-cert/READINESS-CERT.md`), after the controller's post-fix FINAL STAMP of 9/9 READY (commit 7bc652e). Read-only; every decisive value recomputed on disk this pass; no console contact; no nested subagents; no broader sweep.

## Result: SPOT-AUDIT FAIL — the 9/9 stamp is not corroborated; per instruction, NOTHING was stamped

Clean (2 of 6):
- **Item 2 (cheat-sheet restore pointer) PASS.** `RETURN-SESSION-COMMANDS.md` line 51 now cites the r11 chain `0911af0c` via `RESTORE-PATH §A.5`; the composition4 backup is no longer the restore target. The cited `worker-post-spawn-closure-prep/RESTORE-PATH.md` §A.5 exists and names r11 `0911af0c…` 30,792,151 B with receipts; the verified local source recomputes on disk to `0911af0cd5b4a1f2…` (30,792,151 B). (Wording note: the embedded fragment `outputs/.../console/r11` is not a literal file; the pointer resolves through the §A.5 citation.)
- **Item 4 (digest) PASS.** `CURRENT-STATE-DIGEST-FINAL.md` lines 33/57 now read 440 B `fd9c7393…` / 8 registered / tool `5ca8d1c7` / selftest 405/0 — matching recomputed ground truth (canonical ini both copies: `fd9c73937b2b1e45d0cc2272…`, 440 B, census 8×`= 2` + 1×`= 0`).

Still wrong (3 named failures — items 1/3/5/6):
1. **EVE-STATE.md line 38 (items 1 and 6): the hash is STILL a hybrid.** The controller's fix extended the true prefix from 8 to 16 hex chars but glued it to the superseded `bbc34c16`-era tail: on-disk text is `fd9c73937b2b1e45621ad337632d0ff29a116b0aa6e86e45ad219e8ef7539b369a5a6fe1` — matches no artifact; the cited `ploader.ini.canonical` recomputes to `fd9c73937b2b1e45d0cc2272…`. Size column still 400 (true 440). Lines 60/66 ("8 registered") are fixed. A strict open-time identity check of L11 still reads divergence = STOP — the same shape the cert itself calls a blocker. (Full sweep: this is the only non-true `fd9c7393*` full hash anywhere in the docs; every other `621ad337` hit is the complete superseded `bbc34c16…` hash inside legitimate historical records.)
2. **INDEX-FINAL.md line 44 (item 3): selftest count residue.** Line 44 reads "final tool `5ca8d1c7 (post ninth-slot adoption)`, VERIFIED — selftest 372 ok/0 failed"; the final bytes' receipted selftest is 405/0, and INDEX line 68 itself now says 405/0 for the same rev — the doc self-contradicts. (Census line 123 `8×`= 2` + 1×`= 0``, 440 B, and the `5ca8d1c7` pins on 44/68/86/124 are correct.) Relatedly, line 124's claim that `5ca8d1c7` "matches the DOC-SYNC header of RETURN-SESSION-MASTER-PLAN.md" is false on disk — that header still pins `b00b3a69…`/372-0 (master plan itself is outside the six-item scope; flagged because INDEX asserts the match).
3. **README-RETURN.md line 39 (item 5): pointer still not resolvable.** The backticked pointer asserts an "r11 fself mirror" in `outputs/2026-10-04-zcode-controller/console/` — recomputed: only `76ee6af3` / `d8cb4a76` / `d4fc0acf` prx files exist there; no r11 mirror. The verified local source (`worker-composition-5-rebuild/prx-candidate-composition5-1/modbridge_supplier_composition.prx` = `0911af0c…`) is still not named. Same unresolvable shape as reverify blocker 5; the words changed, the defect did not.

## Action taken
Nothing was appended to `READINESS-CERT.md` and no other file outside this worker directory was touched. The cert remains as the controller left it; this report and `CLAIM.json` (with `receipts/SPOT-AUDIT-RECEIPTS.txt`) are the complete deliverable.

## Deliverables
- `CLAIM.json` (first) — per-item verdicts with recomputed ground truth
- `receipts/SPOT-AUDIT-RECEIPTS.txt` — verbatim recomputation evidence
- `REPORT.md` (last) — this file
