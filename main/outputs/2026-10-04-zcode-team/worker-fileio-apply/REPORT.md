# REPORT.md — worker-fileio-apply (2026-10-04)

Worker: worker-fileio-apply. Budget note: ~21 tool calls of 60. All writes under
`outputs/2026-10-04-zcode-team/worker-fileio-apply/`; originals untouched (vendored
copies under `vendor/`).

## Verdict

ALL 7 fileio diffs APPLIED to a vendored copy of the current mutex-integrated
windows-candidate tree. Host validation with the full family stack **PASSED** on the
first run (exit 0, `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`). Full SDK composition with
`--fileio` **PASSED** (7.2 s, `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`).
`FILEIO_DLL_TEST.elf` staging info delivered. Target (PS5) execution NOT attempted,
NOT claimed.

## What was applied

Per worker-fileio-prep REPORT.md apply order: 6 new files first, then patches 01→07
(per-file disjoint; sequential apply, zero failures for 02–07).

- **Patch 01 required a rebase** (only deviation from prep's set): prep's last hunk
  (`@@ -216,7 +231,7 @@`, the ABI-def census `expected={...}` line) predates the ms_abi
  fix now present in `build.py` (3 inserted lines: ms_abi comment ×2 + `flags=bflags if
  source.name=='owned_runtime.c' else nflags` at lines 218–220). I rebuilt the final
  hunk programmatically from the actual vendored file lines —
  `delivery/applied-patches/01-windows-candidate_build.py.diff` is the rebased diff
  (`git apply --check` clean; same content change as prep intended: adds
  `'mb_win_fileio_abi':20` to the census map). Patches 02–07 applied unchanged from
  `worker-fileio-prep/PATCHES/`.
- `--fileio` / `--fileio-root` flags: **yes, added by prep's patch 01 itself** (not
  previously in build.py; verified by grep before apply — zero `fileio` hits pre-patch,
  21 post-patch).

## Host validation (PASSED)

Command: `build.py --host-only --entry --clock --workers --sync --init-once --mutex --fileio`
plus absolute `--sdk/--llvm-bin/--lld-link` (vendor ROOT differs from workspace root).

- Status: `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`, exit 0.
  Receipt: `delivery/receipts/host-validation-receipt.json` (build `47ee2b8391a940f9a1be9b3934064dea`).
- `fileio_profile: {"enabled": true, "win64cc_defs": 20, "actual_pe_imports": 10,
  "namespace_root": "/data/modbridge", "handle_close": "native mb_win_fileio_close_handle delegation"}`.
- Every host result record carries `"mutex":{"enabled":1}` AND `"fileio":{"enabled":1}`
  (normal + release + failure controls); `consumer_imports: 61` (formula incl. mutex +5,
  fileio +10); worker-call and all default-isolation assertions passed (no
  "unexpectedly advertises fileio" errors).
- This resolves prep's open item: the joint mutex+fileio host run now completes serially
  (prep's stalled compose run was contention, not a patch failure — confirmed).

## Full SDK composition (PASSED, timeout-guarded 450 s, actual 7.2 s)

Command: same flags minus `--host-only`, plus `--fileio-root /data/modbridge`.
Status: `OWNED_SDK_ELF_CANDIDATE_COMPILED_NOT_EXECUTED`.
Receipt: `delivery/receipts/sdk-composition-receipt.json` (85 commands, 103 sources).

- Artifact: `out-sdk-composition/cd9b5aec3d344a62bea649b3b78b3ee5/owned-dll-candidate.elf`
  — 279,504 bytes, sha256 `1e91ec1b753ff9b8e729334c2e6722e1f48582efab9315097b82960f206d3d9e`,
  `ELF 64-bit LSB pie executable, x86-64, version 1 (FreeBSD)` (FreeBSD9-frontend target
  per ms_abi fix; genuine SDK crt1 `_start`).
- Pre-existing warnings only: rust-lld `/align specified without /driver` (present on
  prior candidates too; recorded in receipt `warnings`).

## Console staging info

`delivery/FILEIO_DLL_TEST.elf` descriptor → written as `delivery/FILEIO_DLL_TEST.staging.md`:
payload name `FILEIO_DLL_TEST.elf` (INITONCE_DLL_TEST dispatch family), local path,
sha256, bytes, profile, and gates. NO console contact made (no network per task rules);
staging decision belongs to the controller via MASTER-STAGING-PACKAGE gates.

## Environment observation (pre-existing, not mine to fix)

10–11 `test-sdk-driver` processes from earlier workers' runs (prep SELFTEST out-full /
out-compose-fileio-mutex, windows-mutex-candidate-1 incl. mutex-fix-2 repro) are wedged
in state `UE` (exiting) at 0 % CPU for 2.5–5 h, plus one sleeping parent `build.py`
(pid 10970). Left untouched per no-unrelated-termination rules. My own driver ran clean
and exited normally despite them, so they do not block host validation.

## Honest limits

- `target_execution: false` everywhere; ELF never ran on target; no console contact.
- Vendored tree only; original `outputs/ps5-mod-bridge/windows-candidate/` unchanged.
  Promoting the patch set upstream = controller decision; the rebased 01 in
  `delivery/applied-patches/` is drop-in for the current tree.
- Quality: prototype-track DLL-family integration evidence only; no claim beyond
  compile/link/host-control execution.

## Deliverables (all under this directory)

- `delivery/applied-patches/` — 01 (rebased) + 02–07 + `new-files/` (6 files), self-contained.
- `delivery/receipts/` — host-validation + sdk-composition receipts, run log.
- `delivery/FILEIO_DLL_TEST.staging.md` — console staging info.
- `out-host-validation/`, `out-sdk-composition/` — raw build outputs incl. `owned-dll-candidate.elf`.
- `vendor/` — fully patched tree (windows-candidate, windows-services, windows-execution,
  windows-image, native-host/archive/link_layout.py).
