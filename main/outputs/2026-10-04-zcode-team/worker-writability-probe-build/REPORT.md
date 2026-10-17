# REPORT — worker-writability-probe-build (team 2026-10-04)

Deliverables in this directory (CLAIM.json first, this file last):

| file | what |
|---|---|
| `CLAIM.json` | scope + the mprotect-in-contract verification (written first) |
| `src/exec_write_probe.{h,c}` | the per-attempt machinery (NEW): transition table type, gate ladder, budget, record sequence, restore discipline, fail-open |
| `src/module_entry.c` | NEW variant: dual sink (dedicated jsonl + `/data/plugins/UE4SS.log` write-through mirror), fail-open entry contract |
| `src/record_sink.{h,c}` | vendored igf record_sink pattern; `record_sink.c` carries exactly ONE labeled delta (marker string → `modbridge.execwriteprobe`), diff-verified |
| `src/{anchor_store.h,ingame_abi.h,ingame_libc.*,mapscan.c,vmmap_walk.c,sentinels.c,boundaries.S,import_references.S}` | byte-identical vendored lineage sources (cmp-verified); `support_mem.c` byte-identical from worker-combined-igf7 |
| `TRANSITIONS.json` | EMPTY placeholder table — the design worker's injection point |
| `build_exec_write_probe.py` | packaging pipeline verbatim from the frozen igf6/igf7 recipe; determinism double-build per variant; SKELETON + ARMED variants |
| `check_xwp_artifact.py` | check_diag_artifact-pattern gates (fself segments, init/fini slots, NID dynsym count, forbidden identities, 0F 05 absence, provenance strings, armed typed reference) |
| `tests/` | host suite: scripted **mocked mprotect** + mocked map + JSONL grammar validation (`run_host_checks.py`) |
| `determinism/run-{skeleton,armed}-{1,2}/` | four independent builds, per-run receipts + artifact-checks.json |
| `modbridge_exec_write_probe.prx` | **SKELETON deliverable** igd1-63fa8a03078e, 19213 B, sha256 `960aabe15c7dde53fad3e6235969c605b05941966bf5e2ef8d6f5786ec184c66` |
| `modbridge_exec_write_probe_armed.prx` | ARMED-empty-table staged-next artifact igd1-27618a58f062, 20013 B (post D-1 identity), sha256 `7b9e030c74b501a5aa578a533a4f52f32303284730ff15b3926f3c87d8489402` |
| `build-receipt-{skeleton,armed}.json` | build provenance (frozen lineage hash, contract rows, commands, mprotect verification) |
| `FILL-IN-FOR-DESIGN-WORKER.md` | exact table location + injection procedure + staging order |

## 1. The brief's verification question — answered LOUDLY

The frozen 191-import contract
(`.superpowers/native-dependency-spike/native-full-runtime-aligned-entry-1/import-contract.json`,
191 strong / 6 weak) contains **NO mprotect-class strong row**:

* `sceKernelMprotect` — ABSENT from the 191 strong rows. It exists only in
  (a) igf7's `dropped_identities.derived_strong` (never carried) and (b) the
  SharpProspero `StubCatalog.cs` (packaging tooling knows the libkernel
  stub spelling).
* `kernel_mprotect` — present ONLY in `weak_unresolved_names` (binding WEAK,
  type NOTYPE, status absent, evidence []), and the lineage Packager
  REJECTS every weak import (`allowedWeak` is empty).
* The only mprotect mechanism ever executed on console — igf5's raw
  `syscall` rax=74 shim `igcar_sys_mprotect` — is the suspected
  console+game crash and stays FORBIDDEN (igf6/igf7 inverted syscall gate
  retained: this build asserts 0 `syscall` instructions and no `0F 05`
  pair, PASS).

Resolution per brief: the **SKELETON** carries exactly the 7 console-proven
verbatim rows (zero new identities) with a compile-time stub mprotect
binding; the **ARMED** variant adds exactly ONE derived row
(`sceKernelMprotect`, FUNC/GLOBAL, libkernel group identity matching the 7
proven rows) with the loud not-console-proven flag embedded in the
generated `import-contract.json`. With the default empty table the ARMED
build performs ZERO mprotect calls — it is the design worker's
load-resolution sanity stage.

## 2. What was built

* Per-attempt sequence (igf7 discipline): `row` record → fresh
  same-invocation `sysctl KERN_PROC_VMMAP` scan (mapscan.c, console-proven
  path) → coverage + `from_prot` equality gate → `vmmap_persist` record →
  mprotect attempt → `attempt` record (rc + errno) → gated read test
  (always; igf6-proven safe: NX gates EXECUTE not READ) + content-preserving
  write-back test ONLY when to_prot carries W (`rw_test` record with
  `recovered` flag) → restore to `from_prot` ALWAYS (`restore` record) →
  fresh VMMAP re-proof (`post_restore` record). Summary record carries
  budget state + mirror health.
* Safety rails: attempt budget hard cap 8; `confirmed=1` required per row;
  refusals recorded with `no_memory_touched: 1`; `module_start` returns 0
  on every path (fail-open — a deliberate, documented delta from the
  lineage's -20/-22 entries); re-entry answers 0; single-shot synchronous
  execution, no threads, no unload; dead primary channel mirrors a notice
  to the UE4SS.log sink.
* Packaging: frozen recipe exactly (import_references-first `ld -r`, the
  already-built Packager.dll IN PLACE, frozen `finalize_sdk.py` SDK
  0x11000000 stamp, pinned make_fself). One non-obvious lineage fact
  rediscovered and documented: `ld -r` keeps the FIRST symbol type on
  merge, so the armed generated row object must link immediately after
  `import_references.o` or the packager's "exact typed owned import
  reference" gate refuses.

## 3. Verification (all local; nothing native executed)

* **Host suite PASS** (`tests/test-run.txt`): ARMED binary 93 checks, UNARMED
  6 checks, over a 13-row table covering X->RX ok, X->RWX scripted-ENOMEM
  (restore still succeeds and is proven), X->RWX ok with write
  test + recovery, design-unconfirmed skip, prot-mismatch refusal,
  not-covered refusal, and budget exhaustion. Assertions include: exactly
  16 mock calls (8 attempts + 8 restores, strictly paired, restore to X),
  X->RX FIRST, zero calls from refused/skipped rows, unarmed build makes
  ZERO mprotect calls while emitting the full record sequence with outcome
  11. `json-validation.txt`: every emitted JSONL line parsed with
  `json.loads`, per-attempt record ordering asserted.
* **Determinism PASS**: run1 == run2 byte-equal per variant on final PRX,
  inner ELF, combined object, and import contract
  (`build-receipt-*.json` → `determinism`).
* **Artifact gates PASS** on all four runs (`artifact-checks.json`):
  fself wrapper magic + segment copies, init/fini exactly 1 slot each,
  NID-encoded strong dynsym (7 skeleton / 8 armed), zero weak, forbidden
  identities absent (incl. `igcar_sys_mprotect`; `sceKernelMprotect`
  forbidden in skeleton, required exactly-once-typed in armed), 0F 05
  absent from `.text`, provenance strings present.
* Vendored discipline: 10 files byte-identical to worker-c2-probe/src
  (cmp-verified), `support_mem.c` byte-identical to worker-combined-igf7,
  `record_sink.c` diff = 5 comment lines + 1 marked marker-string line.

## 4. Honest bounds

* The PRXs are PROPOSAL-ONLY: no in-game records exist, no console launch
  was attempted, and nothing here claims engine writability. The design
  worker owns the transition decisions and the user-coordinated staging
  plan; acceptance requires actual seq-ordered JSONL from a real launch.
* The `sceKernelMprotect` row is DERIVED, not console-proven: load
  resolution of the 8th import is itself an experiment (that is what the
  ARMED-empty build is for). The Packager's internal module name stays
  `modbridge_ingame_facts` (hardcoded in the frozen toolchain, used IN
  PLACE per lineage rule) — documented in the FILL-IN note.
* The mock map on host replaces the real VMMAP walk (the walk itself is
  v3-lineage host-tested); console coverage behavior is unverified until a
  real launch.
