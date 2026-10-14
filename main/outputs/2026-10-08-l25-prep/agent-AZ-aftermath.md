# Agent AZ — post-build orchestration script (l25 aftermath, one command)

Date: 2026-10-08. Mac-only, no console contact, no network, no source/engine
files touched. Single deliverable outside this report:
`outputs/2026-10-08-l25-prep/packet/aftermath.py`
(sha256 `63460c5aadb912a77da6496d657b1fd5ea5ae0c3bda731bba182bc71dab50f39`,
39243 B, executable, stdlib-only, `py_compile` clean).

## 1. What it is

`aftermath.py` runs EVERY remaining post-build step — artifact verification,
gates, both suite re-runs, packet assembly, evidence durability, handoff
printing — each with verification and abort-on-failure, so completion after
the twin build + packaging is ONE command:

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-08-l25-prep/packet/aftermath.py
```

Flags: `--dry-run` (print every check/command, execute nothing, absent
artifacts are EXPECTED-PENDING and never fail), `--skip-packaging`
(runtime-only case: skips packaging gates + packet assembly, verifies the
runtime half between the two driver stages).

## 2. Steps orchestrated (paths verified against the drivers' real layouts)

1. **Artifact inventory** — exact paths from `build_variants.py` /
   `l25_packaging_driver.py` (cross-checked with agent R's report and the
   manifest's Z18 layout note): `l25-{A,B}/pass{1,2}/runtime-l25-{A,B}.o`,
   both `TWIN-RECEIPT.json`, `VARIANT-PAIR-RECEIPT.json`,
   `EXECUTION-RECEIPT.json`; `l25-package-{A,B}/pass-{1,2}/` PRX +
   `STATIC-CHECKS.json` + `EXACT-IMPORT-IDENTITIES.json`, root
   `FINAL-RECEIPT-VARIANT-{A,B}.json`, `VARIANT-PAIR-PACKAGING-RECEIPT.json`;
   `2026-10-08-l25-runtime/typescan-r4/results/TARGET-COMPILE.json`
   (content-checked: every unit compile exit 0; produced by
   `target_compile.py`, which must run from the worktree root — it uses
   `Path.cwd()`). If packaging artifacts are missing it prints the exact
   commands first (`l25_packaging_driver.py --plan` / `--execute`); if
   runtime artifacts are missing it prints `build_variants.py --plan` /
   `--execute` + `target_compile.py`. Missing groups abort a real run;
   `--skip-packaging` admits the runtime-only case.
2. **Gate checks** — A≠B pass-1 object sha256 differ + pass1/pass2 twin
   identity per variant (G8/G9 shape, recomputed from disk);
   `EXECUTION-RECEIPT.json` gate sweep (no gate ok=false); import name sets
   equal and exactly 207 each (P7/P11: status PASS + count 207 + set
   equality, parsed exactly like the driver parses them); B's PRX receipt
   labels it UNSAFE (`FINAL-RECEIPT-VARIANT-B.json`: variant.unsafe true +
   safety text contains the UNSAFE label and the
   `MODBRIDGE_L25_SKIP_DELETE_LISTENER` token); static checks 20/20
   (status `STATIC_CHECKS_PASSED`, 20 checks, all pass — both variants ×
   both passes).
3. **Item-6 suite re-run** — `python3 outputs/2026-10-08-l25-runtime/tests/run_tests.py`;
   must exit 0 AND `tests/RESULTS.json` = `PASS_HOST_ONLY_MAC` with 2/2
   results (plain + asan-ubsan) exit 0.
4. **Profile tests re-run** — prefers
   `2026-10-08-l25-runtime/profile/tests/run_tests.py` (per agent AS's
   report, which ran the l25 path), falls back to the l24 runner if absent
   (selection recorded); must exit 0 + RESULTS status PASS.
5. **Packet** — `assemble_packet.py --check` (must exit 0 = all 16 pendings
   resolved) then `assemble_packet.py` (create-only zip); captures the new
   `review-l25-<stamp>.zip` + `VERIFY-<stamp>.md` by before/after diff,
   requires `OVERALL: PASS` in the VERIFY file, records zip sha256.
6. **Evidence + summary** — copies the two PRX receipts, both pair receipts,
   both TWIN receipts, EXECUTION-RECEIPT, TARGET-COMPILE, and all four
   STATIC-CHECKS + four EXACT-IMPORT-IDENTITIES gate tables into
   `evidence/aftermath-<UTC>/` (create-only, COPIES.md with per-file sha256;
   PRX binaries NOT copied — their bytes/sha are pinned in the final
   receipts); writes `POST-BUILD-SUMMARY.md` at the l25-prep root (artifact
   hashes, gate results, suite results, zip + verify paths).
7. **Handoff print** — Claude-review checklist (zip+VERIFY handoff before
   ANY upload per LAUNCH-PLAN §0.3/§6 Stage 1; the specific facts Claude
   confirms from the packet) + the user launch sequence from LAUNCH-PLAN
   v3.2/v4 (B FIRST: reboot → `harvest/prep_modded_launch.py --variant B
   --prx-path .../l25-package-B/pass-1/modbridge_supplier_composition.prx`
   → ONE user launch → 90 s menu → same save → 30 s → L3+R3 → if menu
   opens: harvest, DOWN×3 + CROSS one spawn, harvest again → clean exit →
   restore A resident (B never left resident) → A second per v4.6), plus
   the one-command reminder.

**Abort semantics:** any failed check in a real run writes
`FAILED-AFTERMATH-<UTCSTAMP>.md` (l25-prep root) with the failing step,
evidence, and the remediation command sequence, and exits 1. Dry-run never
writes and never aborts.

## 3. Verification performed (no real artifacts exist yet — dry-run only)

- `python3 -m py_compile` PASS; `--help` renders.
- `--dry-run` (2026-10-08T2137Z): **exit 0**, 136 lines. Honest state:
  all 24 required artifacts `EXPECTED-PENDING` (9 runtime + 15 packaging,
  matching the packet manifest's 16 required-pending rows owned by
  "agent R --execute (root-gated)"; the script cross-references the
  manifest and reports the mapping); gates `EXPECTED-PENDING`; suites and
  packet print `WOULD-RUN`; step 5 additionally shows the READ-ONLY current
  `--check` delta (77 READY / 16 PENDING / RESULT: NOT READY — stated as
  expected pre-build, post-build must see READY/exit 0); step 6 lists all
  16 would-copies with states. Zero writes: no POST-BUILD-SUMMARY.md, no
  FAILED-AFTERMATH-*, no evidence/aftermath-*, no zip/VERIFY, packet dir
  unchanged.
- `--dry-run --skip-packaging`: exit 0; packaging gates SKIPped; packet
  would-lines carry an explicit "a real --skip-packaging run SKIPS both"
  annotation.
- Real-mode abort logic proven in a /tmp sandbox (monkeypatched PREP;
  nothing written to the real tree): `am.fail()` raises `FailedCheck`;
  main's handler writes `FAILED-AFTERMATH-<ts>.md` containing the step,
  evidence, and the remediation commands; dry-run `fail()` records without
  aborting.
- Gate parsing expressions validated against the real l24 receipts (same
  shapes the l25 drivers write): `STATIC-CHECKS.json` → status
  `STATIC_CHECKS_PASSED`, 20/20 checks pass; `EXACT-IMPORT-IDENTITIES.json`
  → status `PASS_MAC_ONLY_EXACT_ELF_IDENTITIES`, count 207, 207 unique
  names. (Source for the parsing: `l25_packaging_driver.py`
  finalize_variant/pair_packaging_receipt, mirrored exactly.)

## 4. Notes / honest scope

- The script never fabricates numbers: every gate reads disk/receipts at
  run time; the dry-run invents nothing and claims nothing.
- If root runs it before packaging, the failure receipt tells them exactly
  which driver commands to run (or to use `--skip-packaging`); if run
  before the runtime build, same for `build_variants.py`.
- `target_compile.py` MUST be invoked from the worktree root (cwd-derived
  paths); the printed command includes the `cd`.
- Packet assembly remains create-only via `assemble_packet.py`'s own exit-3
  discipline; aftermath adds no overwrite paths anywhere (evidence dir is
  created with `exist_ok=False`).
- The real `--execute` path of steps 3-6 will run for the first time after
  the twin build lands; per task scope it was validated by dry-run +
  sandboxed abort/parse tests only.
