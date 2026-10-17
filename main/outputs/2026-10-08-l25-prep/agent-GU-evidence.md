# agent GU — /tmp volatile evidence preservation receipt

Date: 2026-10-10 (local). Scope: Mac-only, battery-safe (file copies + sha256 hashing;
no console, no builds). Sources read-only (copy2, never moved/deleted).

Destination: `outputs/2026-10-08-l25-prep/evidence/tmp-preserved-20261010/`
- `MANIFEST.json` — per-file: original path, size, sha256, mtime, class, why-preserved,
  cited-by (auto-indexed over ~1,570 .md docs under outputs/).
- `CRITICAL-NOTES.md` — dedicated notes for the critical items.
- `tmp/…` — preserved artifacts, path scheme `/tmp/X` → `tmp/X`.

## Counts

| Class | Count |
|---|---|
| Files copied + hashed | 1,240 (278,347,274 B ≈ 265 MB) |
| Large binaries listed + hashed, NOT copied | 29 (8×194 MB eboot bins incl. console-decrypted-check.bin, eboot_strings.txt 15 MB, check-prx/check-tog.prx, 12 in-dir ≥5 MB build artifacts) |
| Already-archived, sha-match verified (no re-copy) | 10 (+ zzdiag v3/v7/v9/v10/v11 already durable under outputs/) |
| Purged gaps (cited but absent from /tmp) | 4 entries below |

Post-copy integrity: every preserved file re-hashed after the layout fix —
0 missing, 0 sha mismatches.

## Critical three — all VERIFIED

1. **Menu-up session log** `/tmp/claude2-live.log` sha
   `9112d495e5d69fbdff17ed91df86e2ac4e0b6899d7836cdc129451800fd0b9a4` —
   identical to `evidence/claude2-menu-session-20261010.log` (root's archive: MATCH) and
   byte-identical to `/tmp/claude2-live.norm.log`. Both preserved.
2. **FY serial-driver baseline** `/tmp/build_variants.py.pre-FY.bak` — EXISTS,
   85,614 B, sha `815127c2e24eb3893d8d8c87eb59db0caf131187e2d3d3c8cc037bd3521c5ab9`,
   preserved. Equivalence proof needs no reconstruction; pair plan
   `outputs/2026-10-08-l26-build-variants/plans/PLAN.preserved-pre-FY-20261010.json`
   was already durable.
3. **PHASEC eboot monitor** `/tmp/l25-variant-A/eboot-monitor-20261010T051653Z.log` —
   EXISTS, 11,520 B, sha `cb936f854f1759a81a2f3c49eb5a92f1c09d6a7916eb42995cecca88fbd62bc6`,
   preserved with the full l25-variant-A dir. Its prx readback sha `efbfc656…`
   cross-matches the value recorded in `harvest/LAUNCH-READY-A-20261010T051653Z.md`.

Bonus: `/tmp/mod-main.lua` sha `f586043a…` == `evidence/townfall-main.lua.console-copy`
(root archive verified matching). `/tmp/eboot-backup.bin` hashed sha begins
`afe0a2a5…`, matching the prefix recorded in agent-AH/agent-Z11 docs.

## Purged gaps (honest list for the receipt)

- `/tmp/console-enabled.txt` — cited by FIRST-LAUNCH-AUTOMATION.md, WAKE-RUNBOOK-20261010.md,
  FROZEN-KNOWN-GOOD-20261010.md — ABSENT (purged before preservation).
- `/tmp/console-main.lua` — cited by FROZEN-KNOWN-GOOD/menu-up docs — ABSENT.
  Possibly identical to evidence/townfall-main.lua.console-copy (sha f586043a…) but
  unverifiable now.
- `/tmp/l25-harvest` — harvest output dir cited in agent-BD/BE harvest docs — ABSENT;
  only `/tmp/l25-harvest-rehearsal` survived (preserved).
- `/tmp/zzdiag-v3/-v7/-v9/-v10/-v11` — purged from /tmp, but their main.lua + loadlist
  versions are already durably archived under `outputs/2026-10-08-l25-prep/zzdiag-vN/` — no loss.

## Classification policy applied

- PRESERVE: decisive/cited artifacts (claude2-live*, mod-main.lua, build_variants.py.pre-FY.bak,
  full l25-variant-A incl. 2×30 MB decrypt-off prx readbacks, l24-backup-prx.self 31 MB
  pristine backup cited by 5 docs, plan-claude*.jsonl, bind-pipeline, klog/eboot taps).
- ARCHIVE-CHEAP: session/plan/build logs <5 MB + small work dirs (zzdiag v4/v5/v6/v8,
  l24/l25/l26 harness and verify dirs; selective <5 MB copy inside 12 larger dirs,
  ≥5 MB contents listed+hashed instead).
- SKIP (listed): 8×194 MB eboot-era binaries (aggregate ~1.4 GB; battery-safe decision,
  individually under the 200 MB cap but hash-recorded so doc citations stay anchored),
  recreatable local binaries (bench*, auth.o, bin-dbg), empty dirs (bisect,
  codex-browser-use), Claude app runtime dir (claude-501).

## Layout note

One layout defect occurred mid-job (dir trees initially landed at BASE/<dir> instead of
BASE/tmp/<dir>); it was fixed by moving them under tmp/, every preserved_path in
MANIFEST.json updated, and ALL 1,240 copied files re-hashed afterwards: 0 mismatches.
MANIFEST.json `verification` block records this.
