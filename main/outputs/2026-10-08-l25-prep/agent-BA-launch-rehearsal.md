# Agent BA — launch rehearsal (both variants, --dry-run) — 2026-10-08

Scope: Mac-local only. Zero network calls, zero console writes, zero file
writes outside this report and the LAUNCH-PLAN.md appendix. The two dry-runs
performed are the tool's own read-only local checks by design.

## 1. Prep tool behavior (harvest/prep_modded_launch.py)

`--help` confirms: `--variant {A,B}`, `--prx-path` (must carry SELF magic
`4f153d1d`/`5414f5ee`), `--skip-klog`, `--console-ip` (default ps5-host-not-configured.invalid),
`--dry-run` ("local read-only checks do run; exit 0"). Live mode aborts on any
mismatch; never touches autoload.txt, never kills processes.

**Missing --prx-path in dry-run does NOT abort** (source-verified):
`sha256_of()`/`magic_of()` return `None` on OSError (L197-215); `dry_run()`
(L951-1138) prints the STEP 2 local checks as FAIL (`file exists -> FAIL`,
`magic: None -> FAIL: unrecognized magic None`) and continues through all 8
steps, exiting 0. So no placeholder copy was needed; a control run with a real
signed PRX was added anyway (below) to demonstrate the PASS path.

## 2. Final PRX paths (confirmed from driver source)

`2026-10-08-l25-runtime/l25_packaging_driver.py` VARIANTS table:
`pkg = ROOT/'outputs/2026-10-08-l25-package-A'` / `-B`; `prx_name =
'modbridge_supplier_composition.prx'`; P9/P10 receipt rows pin
`pkg/'pass-1'/prx_name` (pass-1 = FINAL-RECEIPT canonical pin; pass-2 twin
must be byte-identical). This matches Z18's manifest rows exactly:

- A: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-A/pass-1/modbridge_supplier_composition.prx`
- B: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-B/pass-1/modbridge_supplier_composition.prx`

Both are PENDING agent R `--execute` (root-gated); neither package dir exists yet.

**Sign step convention**: agent AY's `sign_and_verify.sh` is NOT present
(searched `harvest/`, all of `l25-prep/`, all of `l25-runtime/` — no `*.sh`, no
match for `sign_and_verify`). Per fallback: propose `<prx>.self` next to the
PRX, i.e. `.../pass-1/modbridge_supplier_composition.prx.self`. IMPORTANT
mitigating fact: a separate sign hop should be unnecessary — the staged
pipeline `package_prx_l19.py` runs `make_fself.py` internally (its own step 9
"make_fself.py -> wrapped candidate"), and the actual l24 output of that
pipeline on disk (`2026-10-08-l24-package/pass-1/modbridge_supplier_composition.prx`,
31332783 bytes, sha256 `2e712a8b0e1ffc2b8c12af6e5b7a7a8fe63fe6e6a5c24d113655faebb79c92f4`)
already carries SELF magic `4f153d1d`. `--prx-path` points directly at the
pass-1 PRX; the `.self` name is reserved for a future re-sign/verify script.

## 3. Dry-run results (console-independent local checks)

| Run | exit | STEP 2 PRX local | STEP 4 settings local | STEP 5 ploader.ini local |
|---|---|---|---|---|
| `--variant A --prx-path .../l25-package-A/pass-1/modbridge_supplier_composition.prx --dry-run` | 0 | FAIL (file MISSING, magic None) — expected, artifact not built | PASS (1947 B, sha match) | PASS (440 B, sha match, inject row present) |
| `--variant B --prx-path .../l25-package-B/pass-1/modbridge_supplier_composition.prx --dry-run` | 0 | FAIL (file MISSING, magic None) — expected | PASS | PASS |
| Control: `--variant A --prx-path .../2026-10-08-l24-package/pass-1/modbridge_supplier_composition.prx --dry-run` | 0 | PASS (`file exists -> PASS`, `magic: '4f153d1d' -> PASS (signed SELF)`) | PASS | PASS |

The control proves the magic gate will pass once the l25 driver emits its
pass-1 PRXs (same pipeline as l24). Both variant runs printed the full
8-step plan correctly: daemon preconditions (kstuff/pldmgr/ftpsrv/klogsrv as
<base>.elf, no eboot.bin, ploader.lock RETR 78/550-or-stale law), decrypt-OFF
(`-Q "SITE SELF"`) resident backup + upload + byte-exact readback with
restore-on-mismatch, MFA size-exact settings law (1947 B), ploader.ini
restore + inject row, single `loadpayload` arm with fresh-pid poll (never
duplicate-send), klog tap + eboot monitor (detached), LAUNCH-READY-<variant>-
<timestamp>.md report. Full outputs saved at /tmp/ba-dryrun-A.txt,
/tmp/ba-dryrun-B.txt, /tmp/ba-dryrun-A-control.txt.

Also verified local live-run dependency: klog script exists at
`2026-10-07-l18-full-build/console-test-20261007T205350Z-f1310cc9/klog_capture.py`
(5469 B, referenced by STEP 7).

## 4. Backup integrity verdict

Pinned expectations (prep tool L113-123): settings 1947 B /
sha256 `b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2`;
ploader.ini 440 B / sha256 `3eab951051082de57de3dc3708b3c7210ae8ba4d99ad44b87e0d8c92e0cbb3d5`.

| File | Bytes | sha256 | Verdict |
|---|---|---|---|
| /tmp/l24-settings-backup.ini | 1947 | b980cea7...59d3ce2 | MATCH |
| /tmp/ploader-ini-backup-20261008.ini | 440 | 3eab9510...cbb3d5 | MATCH (inject row `/data/plugins/modbridge_supplier_composition.prx = 2` present at line 4) |
| evidence/l24-settings-backup.ini | 1947 | b980cea7...59d3ce2 | MATCH |
| evidence/ploader-ini-backup-20261008.ini | 440 | 3eab9510...cbb3d5 | MATCH |

**/tmp exposure**: the tool reads the backups from HARD-CODED /tmp constants
(SETTINGS_LOCAL / PLOADER_INI_LOCAL; no CLI override exists), so /tmp cleanup
WOULD abort a live run at STEP 4/5. The evidence/ copies are byte-identical
durable backups; remediation if /tmp is cleaned is a one-line re-copy
(`cp evidence/l24-settings-backup.ini /tmp/` + `cp evidence/ploader-ini-backup-20261008.ini /tmp/`)
— no tool change needed.

## 5. Command pairs (live run, after agent R --execute emits the PRXs)

Variant B (FIRST — registration SKIPPED, diagnostic-only, never left resident):

- prep: `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant B --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-B/pass-1/modbridge_supplier_composition.prx`
- harvest: `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/harvest.py --label l25-launch-1`

Variant A (SECOND — normal registration, confirms the mechanism):

- prep: `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/prep_modded_launch.py --variant A --prx-path /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-package-A/pass-1/modbridge_supplier_composition.prx`
- harvest: `python3 /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/harvest/harvest.py --label l25-launch-2`

In-session spawn steps (after the menu is up): D-pad DOWN three times to land
the selection on the first PICKUPS row, then CROSS ONCE (`spawnSelected` on
the game thread; one CROSS = one spawn, no re-press while STATUS is busy).

## 6. Doc update

Appended `## Final artifact paths (post-build)` to
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/LAUNCH-PLAN.md`
(both variants' prx/signed paths, one-line prep + harvest commands, B-first
reminder, DOWN x3 + CROSS-once spawn steps, /tmp backup dependency note).
No other files touched.
