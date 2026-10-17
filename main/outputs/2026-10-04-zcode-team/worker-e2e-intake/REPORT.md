# REPORT.md — worker-e2e-intake (2026-10-05)

## Verdict

**The parked E2E installer intake blocker is RESOLVED.** `/data/modbridge/intake/` now exists on the live console and accepts STOR with verified readback. The config-file-driven installer design is consumed **unchanged** — no payload rebuild, no path adjustment.

## What the mission feared vs what is true now

| Mission assumption (from KB `Lreturn_e2e_blocked`) | Verified today (2026-10-05, ftpsrv v0.21.1) |
|---|---|
| ftpsrv refuses MKD (`rc 9` silent) | `MKD /data/modbridge/intake` → `226 Directory created`, curl rc 0 — **succeeds when the parent `/data/modbridge/` exists**. The parent appeared 2026-10-04 22:22 (`roots/`), which is why the historical refusal no longer reproduces. Not re-tested with a missing parent (out of write bounds). |
| STOR into a missing dir fails silently | Dir now exists; `STOR /data/modbridge/intake/test.txt` → `226 Data transfer complete`, curl rc 0, RETR readback **byte-identical** (88 B, sha256 `818dafb31d5836f9cebd80381e71f9fab2ad7dee143baac16b5a63a15a455783`). The rc-9-silent failure class was not re-triggered. |

## Route results

- **FTP MKD (works — the chosen route):** intake dir created; exactly one test file staged (`test.txt`, 88 B). ftpsrv replies `226` (not RFC 257) to MKD — parsing quirk to remember for controller scripts.
- **pldmgr `manage:upload?filename=/data/modbridge/intake/probe.txt` (dead end):** HTTP **500** `Error during upload`; creates no parent dirs, accepts no absolute paths (matches upstream handler: filename is a basename into `/data/pldmgr/payloads/`). Zero console side effects.
- **Flat `/data/modbridge/` intake (not needed, would be a design change):** installer `E2E_CMD_PATH` is a compile-time constant `/data/modbridge/intake/e2e-cmd.json` (`worker-e2e-console-dry/E2E-CMD-SCHEMA.md` §1 — no env vars, no relative resolution; `modinst_core.c` `mkdirs()` never creates `intake/`). A flat path would require a new payload revision. Since MKD worked, nothing changes.

## Reconciliation statement (task 3)

**The dir now exists → the installer path is NOT adjusted.** The E2E controller can run the staged plan (`worker-e2e-staging-final/STAGED-E2E-COMMANDS.md`) as written: its STOR probe gate (P1/G3) is the exact operation proven green, and `/data/modbridge/intake/` contains only `test.txt` — `e2e-cmd.json` is absent so the pre-STOR absence check passes. The controller may DELE `test.txt` in its own intake cleanup for a pristine dir; this worker did not delete anything.

## Console writes (complete list — nothing else touched)

1. `MKD /data/modbridge/intake` → created.
2. `STOR /data/modbridge/intake/test.txt` (88 B) → created, readback verified.
3. `POST /manage:upload?filename=%2Fdata%2Fmodbridge%2Fintake%2Fprobe.txt` → rejected HTTP 500, wrote nothing.

No `ploader.ini`, no `plugins/`, no mod tree, no DELE/REN, no dispatch, no payload upload. All other console interaction was read-only LIST/RETR.

## Honest limits

- This unblocks the E2E run; it did **not** run any E2E stage, dispatch the installer, or stage `e2e-cmd.json` / mod bundles (those are the controller's sha-verified per-dispatch jobs).
- The exact boundary of the historical "MKD refused" (missing-parent vs older ftpsrv build) is a stated hypothesis, not a re-tested fact; re-testing would need out-of-bounds console writes.
- Console evidence: LIST/RETR transcripts in `INTAKE-RESULT.md`; local probe file at `/tmp/modbridge-intake-test.txt` (sha pinned above).

## Deliverables

- `CLAIM.json` — scope, exact console writes, verification, bounds, honesty note
- `INTAKE-RESULT.md` — full probe transcript (task 1 + 2) and historical-fact reconciliation
- `REPORT.md` — this verdict + reconciliation (task 3/4; blocker resolved, so no blocker statement needed)
