# REPORT — worker-startup-checkpoint (2026-10-04)

**DELIVERED.** Two frozen, deterministic, locally-verified checkpoint
diagnostics that localize the composition-4 startup failure between "ploader
accepted" and "module entry executed", plus the import-surface diff that
motivates them. Cause NOT claimed identified — the staged pair decides it
(`STAGE-PLAN.md` outcome matrix). No staging, no console contact, no launch;
proposal-only payloads.

## 1. Task 1 — import diff (answered decisively)

Aligned-entry module identified: `native-full-runtime-aligned-entry-1/build-1/
modbridge_runtime_aligned_entry.prx` (30,151,623 B ≈ the "30.2 MB" in the
brief; fself `3b998f09…`, the console-proven lineage).

Parsed dynsym UND imports from both inner ELFs on disk
(`dynsym_und.py`, raw output in `import-diff.txt`):

- composition-4 (`worker-composition-4-fix/prx-candidate-composition4-1/
  bare-sdk/…`): **196** imports
- aligned-entry (bare-sdk): **191** imports, all contained in composition-4's set
- **composition-4 adds exactly 5 imports** (new/risky set):

| NID long form | function | how resolved |
|---|---|---|
| `LwG8g3niqwA#B#C` | `sceKernelDlsym` | supplier-import-table relocation slot 0 |
| `kUpgrXIrz7Q#B#C` | `sceKernelGetModuleInfo` | slot 1; ploader itself imports `kUpgrXIrz7Q#A#B` (different flavor) |
| `wzvqT4UqKX8#B#C` | `sceKernelLoadStartModule` | slot 2; NID independently repo-proven via ploader source |
| `QKd0qM58Qes#B#C` | `sceKernelStopUnloadModule` | slot 3 |
| `AEJdIVZTEmo#A#B` | `qsort` | slot 4 (libc) |

Resolution method: the composition-4 binary's own
`modbridge_supplier_import_table` (5 R_X86_64_64 relocs at 0x1c9dd58–78)
maps slot-for-slot onto composition-3's documented
`supplier_import_extension.S` row order (Dlsym, GetModuleInfo,
LoadStartModule, StopUnloadModule, qsort), and slot 2 lands on the NID
ploader's source defines as `sceKernelLoadStartModule` — a self-consistent
cross-check. These five are precisely the rows an earlier packaging candidate
flagged as `imports_outside_composed_baseline`, and the #B#C library flavors
are console-proven by NO loading module to date (v1 probe was load-rejected
on derived identities while the aligned-entry sibling loaded).

Also measured: 4 of the 196 are OBJECT (data) imports — `__mb_cur_max`,
`__stderrp`, `__stdinp`, `__stdoutp`; the rest are FUNC. This matters for
exact replication (types must round-trip).

## 2. Tasks 2–3 — the diagnostic PRX (import surface WAS replicable; built both probes)

The full 196-row surface is fully described by composition-4's own
`supplier-contract.json` (`contract.imports`, Packager input format), so the
fallback two-diagnostic path became the primary plan and BOTH were built
(frozen igd1 recipe vendored end-to-end: import_references-first `ld -r`,
SharpProspero Packager, frozen `finalize_sdk.py` SDK 0x11000000 stamp,
pinned make_fself; SAME module name `modbridge_supplier_composition`; log
path `/data/plugins/UE4SS.log`):

- **sdg1 (min)** — 7 verbatim console-proven rows (byte-equality verified
  against igd1's contract). Tests: path writable + a 7-import module reaches
  module_start in the composition's slot.
- **sdg2 (imp)** — composition-4's EXACT 196-row surface (verbatim contract
  rows, types preserved) with checkpoint-only code. Tests: ALL imports
  resolve on console. 22,087 B total — small, focused, no UE4SS/supplier/
  harness/C++.

Both `module_start`s are identical and do ONLY: `clock_gettime` → open
`/data/plugins/UE4SS.log` (`O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW`, 0600) →
append `DIAG: module_start reached at <epoch>.<ms> pid=<pid> build=<id>
variant=<v>` → close → **return 0 always** (igd1 lesson: never leave a
nonzero entry return as a free variable).

**Exact-surface oracle (stronger than a size/shape check):** each build
fails unless its final bare-PRX dynsym UND set (name+bind+type) equals the
ground truth entry-for-entry — igd1 run1 for sdg1, composition-4 bare-sdk
for sdg2. Both passed: sdg2's dynsym is INDISTINGUISHABLE from
composition-4's, so its on-console import resolution directly proxies the
composition's. Payloads of record (post-determinism run1):

| | sdg1 fself | sdg2 fself |
|---|---|---|
| sha256 | `ac6497c362d9aeb597bb7398aff62b013d4bba33dbf6623f9341b16e3055f910` | `0e5b36bc03d97619990a061baafd077fb331d3c0fd50249488aaaace50d32d86` |
| bytes | 6,557 | 22,087 |
| bare-sdk | 68,896 B / `c9e69cd7…` | 82,928 B / `ff4275bc…` |
| build_id | `sdg1-72abf1a3cd8d` | `sdg2-6f404cee6b94` |

All structural gates match the lineage precedent: LOCAL_PACKAGE_CHECKS_PASSED,
init/fini arrays 1/1 (loader-unowned), explicit DT_INIT=module_start /
DT_FINI=module_stop, SDK 0x11000000, fself per-phdr segment equality.

## 3. Task 4 — two-run determinism

`build_diag.py determinism`: two independent builds per variant, byte-compared
across fself, bare-original, bare-sdk, combined object, contract, and oracle
file — **IDENTICAL / DETERMINISM-PASSED for both variants** (min run1==run2 at
`ac6497c3…`; imp run1==run2 at `0e5b36bc…`).

## 4. Task 5 — stage plan

`STAGE-PLAN.md`: backup composition-4 (`de372302…`) → stage sdg1 at the SAME
path (same ploader slot, igd1 replacement pattern; expected readback
68,896−1560 = 67,336 B per the documented channel law) → one launch → read
log → stage sdg2 (readback 82,928−1560 = 81,368 B) → one launch → read log →
restore backup. Full outcome matrix maps every (sdg1, sdg2) combination to a
unique localization: (no-line) = slot/start-step problem, not imports;
(sdg2 FAIL) = import resolution — bisect the 5 new identities; (sdg2 OK +
line) = imports fine, failure is inside the composed runtime's own entry
path; (sdg2 OK, no line) = size/segment-shaped load→start gap.

## 5. Files (all under `outputs/2026-10-04-zcode-team/worker-startup-checkpoint/`)

- `CLAIM.json` (written first), `REPORT.md` (this file, last)
- `STAGE-PLAN.md` — staging runbook + outcome matrix
- `dynsym_und.py`, `import-diff.txt` — the import diff evidence
- `diag/build_diag.py` — frozen two-variant build + oracle + determinism runner
- `diag/check_diag_artifact.py` (+ `_base.py` vendored v3 checker for provenance)
- `diag/src/` — checkpoint `module_entry.c`, vendored libc/abi/boundaries/
  sentinels, generated `import_references.S`
- `diag/harness/` — vendored Packager (prebuilt `Packager.dll`)
- `diag/determinism/{min,imp}/run{1,2}/` — determinism artifacts; `run1/`
  holds the payloads of record; `diag/build-{min,imp}-*/` first builds kept

## 6. Not done / not claimed

- No staging, launch, console contact, or network; nothing outside this dir
  written; existing files elsewhere untouched (read-only).
- Cause of the zero-output behavior NOT claimed identified — that is what
  the staged pair decides.
- Honest limitations: the checkpoint `module_start` is not the composition's
  real entry body (by design — it isolates load→entry from runtime code);
  sdg2 proves resolution + entry reachability, not UE4SS init health; 4 of
  the 5 new NID→name mappings rest on relocation-order + one repo NID
  cross-check (slot 2), not on a vendor NID table; readback oracles use the
  documented size law (−1560), since full-hash readback equality is
  impossible through this channel.
- Call budget: ~40 of 60.
