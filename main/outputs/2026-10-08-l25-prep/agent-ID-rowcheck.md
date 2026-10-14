# agent-ID — loadlist row deploy gate (Claude round-8 item 6)

2026-10-11, Mac-only, battery-safe. No console contact, no builds — all
verification ran in offline fixture mode (`--from-dir`).

## What landed

| file | change |
|---|---|
| `harvest/prelaunch_rowcheck.py` | NEW gate tool. Fetches the console `loadlist.txt` (default `/data/modbridge/roots/PPSA33286/loadlist.txt`, FTP :2121), parses every `file\t<role>\t<path>\t<size>` row (header rows skipped; malformed rows flagged), fetches each file, verifies it EXISTS at its EXACT declared size; `--expect pins.json` optionally adds sha256/size pins; reports EVERY mismatch with the EXACT row text; `--json` mode for tooling; durable receipt + readback spool under `--out-dir` (default `evidence/rowcheck/<ts>/`). Exit 0 all-pass / 1 gate-fail / 2 usage. Refusals: loadlist absent, loadlist with ZERO file rows, malformed row. Warns on executable-class row paths (ftpsrv fakes SIZE for SELF/SPRX, AGENTS.md PS5 rule 1). `--from-dir DIR` fixture mode = zero network (test surface). |
| `harvest/prep_modded_launch.py` | EDIT. New mandatory **STEP 5.5** between the uploads (steps 3–5) and the arm dispatch (step 6) / LAUNCH-READY handoff: subprocess-calls the rowcheck tool (`--json`, contract rc 0/1), feeds the doc through the pure `_rowcheck_verdict()` helper, records a ledger row, and on failure adds EVERY failing row (exact row text) to the ledger then ABORTs with the fix instructions — "upload the missing rows or fix the loadlist (re-pin each row size-exactly to the actual bytes), then re-run from STEP 1. Do NOT launch: MFA admission is all-or-nothing" — same failure-report style as the existing gates. No skip flag (mandatory). Game-closed law holds: STEP 1 already refuses while eboot.bin runs, and FTP reads return empty under game load. Docstring item 5.5 added; `--dry-run` prints the full 5.5 block. Missing rowcheck script = ABORT (gate can't be silently dropped). |
| `harvest/test_prelaunch_rowcheck.sh` | NEW offline fixture suite (receipts under `evidence/rowcheck-tests/<ts>/`). |
| `WAKE-RUNBOOK-20261010.md` | §10 item 6 (one note): the gate is automatic now; a failing row aborts prep before arming/LAUNCH-READY, killing the -6 relaunch class. |

## Gate behavior (why it stops the -6 class)

The 2026-10-11 incident: loadlist declared `ueselftest/enabled.txt`,
never uploaded → MFA refused the whole module at boot (`mfa_activate_failed
-6`) → wasted launch. The gate re-checks every row BEFORE anything is
armed or handed to the player, so the failure surfaces at prep time with
the exact row, e.g.:

```
  MISSING      L13   ue4ss/Mods/ueselftest/enabled.txt   declared=0 actual=(absent)
        detail: declared but ABSENT on the console ... -- MFA refuses the WHOLE module at boot (the -6 incident)
        row  : 'file\tlua_mod\tue4ss/Mods/ueselftest/enabled.txt\t0'
[rowcheck] VERDICT: FAIL -- 9 rows: 8 pass, 1 missing, ...
[rowcheck] FIX: upload the missing rows or fix the loadlist: re-pin each row size-exactly to the actual bytes ...
```

prep side: `STEP 5.5 LOADLIST ROW GATE FAILED -- <summary or refusal>. FIX:
upload the missing rows or fix the loadlist ... Do NOT launch ...` →
LAUNCH-PREP-FAILED receipt, exit 1, arm never dispatched.

## Test results — 25/25 PASS (suite rc 0)

Run `evidence/rowcheck-tests/20261011T140532Z/RECEIPT.md` (fixtures, per-case
stdout/stderr, expect manifests and rowcheck receipts all durable there):

- case1 all-present → rc 0, 9/9 rows PASS, receipt written durably
- case2 one-missing = the exact -6 row (`ueselftest/enabled.txt`, 0 B) → rc 1, MISSING reported with the exact row text + -6 citation
- case3 wrong-size (file 3 bytes larger than row) → rc 1, SIZE-MISMATCH, declared=42 actual=45, code=-8 citation
- case4 header-only loadlist → rc 1 REFUSED (ZERO file rows); 4b 0-byte loadlist → rc 1 REFUSED
- case5 loadlist itself missing → rc 1 REFUSED
- case6 malformed row (3 fields) → rc 1, MALFORMED with the raw row
- case7 `--expect` sha mismatch → rc 1 SHA-MISMATCH; case8 full `--expect` pass + an extra non-row pin → rc 0 with warning
- case9 usage error → rc 2
- case10 wiring: `py_compile` both scripts OK; `prep_modded_launch.py --dry-run` rc 0 and shows the STEP 5.5 block + fix text; `_rowcheck_verdict()` unit-tested from the saved case docs (pass-doc → gate PASS; -6 doc → abort with the exact row)

## Scope notes / limits

- Live-path (FTP) execution is for the next real prep run by ROOT; the
  offline suite covers the tool via fixture mode only, per task rules.
- prep does not pass `--expect` (no pinned row-sha manifest exists for a
  launch; the loadlist's own declared sizes are the admission authority);
  operators can run the tool standalone with `--expect` for sha coverage.
- Rowcheck reads text-class files without `-Q "SITE SELF"` (same as every
  other loadlist-surface tool); executable-class row paths get a warning.

— agent ID (rowcheck task), 2026-10-11.
