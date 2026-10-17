# READINESS-CERT — final host-side certification (2026-10-04, worker-return-readiness-cert)

One page. Workspace root: `.codex/worktrees/translation-core/what`; `W` = `outputs/2026-10-04-zcode-team`; `C` = `outputs/2026-10-04-zcode-controller`.
Every hash below was **recomputed on disk this pass** — not quoted from docs. Verdict rule: artifact exists + its own receipts pass + consumers cite it correctly.

## Verdict: 8 of 9 READY — 1 NOT-READY (documentation-only, items named below)

**(a) Launch candidates 9/10/11 + gate receipts — READY.**
- Launch-9 (safe): `W/worker-comp6-driver/prx-candidate-composition6-1` fself `76ee6af3…` 30,773,943 B on disk; FINAL gate **GO** (`gate-receipt-composition6-FINAL.json`, sha `11b6a054…`); copy at `C/console/comp6-staged-fself.prx` byte-identical.
- Launch-10 (armed): `W/worker-launch10-prebuild/prx-candidate-composition6-l10-a` fself `d8cb4a76…` 30,818,247 B on disk, a≡b byte-identical; gate **GO 15/15, failed_checks []** (`gate-receipt-launch10-armed-a-vs-b.json`, sha `84fa09c3…`); mirror at `C/console/comp6-armed-staged-fself.prx` = `d8cb4a76…`.
- Launch-11: vtable-prebuild candidate `l11-a` verified **byte-identical to launch-10**; gate **GO** (`…vtpre-a-vs-l10.json`, sha `ee753d48…`). Dry-compose two-patch candidate `d94bb99a…`; gate **GO** vs r11 (`…drycompose-a-vs-r11.json`, sha `67ef64e5…`); its earlier NO-GO vs l10 (`5dba72a0…`) is preserved + explained (evidence-form, not composition) per session law. MATRIX-ROW-NOTE reconciles it to the matrix owner.

**(b) Five probes + readback constants in the tool — READY.**
All five verified three ways (fself sha+size, bare-sdk sha+size, readback body sha256 recomputed from bytes `64..readback_bytes`): **xwp** `daea2ff4…`/67832 · **fc1** `7e2dce27…`/67848 · **qdrv1** `71406d3c…`/87112 · **observer** `3f89a5b6…`/30143976 · **cc1** `131ff1c2…`/71528. All match the pinned constants in `console_batch.py` (XWP_ARMED/FC1POOL/QDRV1/OBS/CC1) and FINAL-INI-LAYOUT §4. The brief's wrong cc1 window (`1d17a27d…`) is corrected in-tool with divergence receipt `worker-ninth-slot-ini-adoption/receipts/cc1-readback-constants-check.txt`.

**(c) Settings v3 pair + stage-settings flow — READY.**
On disk `W/worker-settings-v3-package/package-v3/`: settings `d8faa4ec…` 1448 B + loadlist `4d98ecd7…` 447 B = tool `SETTINGS_V3` pins = EVE-STATE L7/L8. `stage-settings` flow in the tool with v2 freeze gate (`9570ead3…`/834 + `3afd10ce…`/446 = the recorded rollback pair).

**(d) Batch tool selftest + drive receipts — READY.**
Current tool bytes `5ca8d1c7…` are the receipted final bytes: selftest **405 ok / 0 failed** + fake console drive **92/0** re-verified on the final bytes (`W/worker-evidence-tools-final/AUDIT-RECEIPTS.md`, sha `e56c60f5…`; `receipts-final-sha256.txt` 4/4 match disk). Receipt lineage intact: 122/122 (original `870ee937…`) → 372/0 (`b00b3a69…`) → 405/0 ninth-slot (`456f9372…`, `receipts/selftest-ninth-slot.txt`) → final label-only relabel. Drive run dirs regenerated next to the final bytes (`runs/fake-drive/`, 23 flows).

**(e) Three evidence tools + rehearsal suite — READY.**
`post_launch_check.py` `e6cea9ee…` · `rung_check.py` `287a2684…` · `console_batch.py` `5ca8d1c7…` — all match `receipts-final-sha256.txt` on disk. Pipeline rehearsal re-run on final bytes: 5 scenarios + both freshness refusals discriminate (`rerun-results.json`: s01 MODS_STARTED/PASS; s02 FAILED_STATUS/CANNOT_ASSERT; s03 FAILED_runtime_start_failed; s04 script-error trap; s05 stale NOT_STARTED/INCONCLUSIVE-STALE; gap4 stale-driver refused). Desk rehearsal 78/0 + all 6 branch shapes (`worker-session-desk-rehearsal/`).

**(f) Documentation chain internal consistency — NOT-READY.** Blocking items, named exactly:
1. `C/RETURN-SESSION-COMMANDS.md` line 5 and `W/worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md` lines 5/31 pin tool rev **`456f9372…`**; on-disk tool is **`5ca8d1c7…`** (one label word, receipted in AUDIT-RECEIPTS — but the two doc pins were never re-stamped). A strict open-time identity check of the tool against the cheat sheet reads as divergence = STOP.
2. `W/worker-index-final-edition/CURRENT-STATE-DIGEST-FINAL.md` lines 33+57 state the **superseded 8-entry ini law** (400 B `bbc34c16…`, **7 registered**) and stale tool `b00b3a69…` as CURRENT. The operative ninth-slot law is 9 entries / 440 B `fd9c7393…` / **8 registered** (cheat sheet rev c, brief, tool, launch guide). README-RETURN's read-order item 1 sends the user here first.
3. `INDEX-FINAL.md` lines 44/68/86/88/123-124 carry that same superseded law while labeling the tool "post ninth-slot adoption" — self-contradictory.
4. `EVE-STATE.md` quick-reference line 66 ("exactly 7 registered") + L11 (400 B `bbc34c16…`) — superseded.
5. `CONTROLLER-DECISION-BRIEF.md` line 185 "eight evidence sinks" vs corrected reality/tool label "seven" (flagged by worker-evidence-tools-final; owner-unfixed).
6. Cheat sheet line 51 + README line 39 point the comp6-slot restore at `composition4-console-backup.prx` (`d4fc0acf…`, 30,769,672 B) — **not** the S1 r11 identity `0911af0c…` (30,792,151 B). Fix: one-line relabel to the verified local source (below).

**(g) Archive scaffold — READY.** `W/worker-session-archive-scaffold/`: `session_run.sh` (`01cce335…`) + `session_run.py`, flows dll8/e2e/game-launch9/game-launch10 (4/4 checklists), `tests/TEST-RECEIPT.json`, `runs/INDEX.md`. Consumers (cheat sheet, README) cite it correctly.

**(h) Restore paths — READY** (one mislabeled pointer, item f.6). Ini: `rollback-ridealongs` in the tool (two single-char 2→0 flips + `.bak`). Settings: v2 pair in the tool's freeze inventory. Comp6 slot: r11 source **verified on disk** — `W/worker-composition-5-rebuild/prx-candidate-composition5-1/modbridge_supplier_composition.prx` = `0911af0c…` 30,792,151 B (the S1 slot identity); backup file `C/console/composition4-console-backup.prx` also present.

**(i) Ledger snapshot — READY.** `W/worker-ledger-archive-snapshot/ledger-session-eve.json` sha `5a31de8f…` = its own frozen index claim; 280 `zcode_*` entries, valid JSON.

## The one-line status for the user
Every artifact the return session will touch is on the Mac, hash-verified, and gate-receipted — **the console is the only missing piece** (down at close; slot still holds r11 `0911af0c…`; first act is the reachability check, then staging). Before walking in, spend ~15 minutes fixing the six named doc lines in (f) so no doc tells you "7 registered / tool b00b3a69 / rev 456f9372" while the tool enforces "8 registered / fd9c7393 / bytes 5ca8d1c7". Under this workspace's own law, an unreconciled identity divergence is a STOP — reconcile it on paper now, not mid-session.
