# STAGE-PLAN — igf6-direct-read (worker-igf5-diagnosis-rebuild, 2026-10-04)

STATUS: PROPOSAL-ONLY. Nothing here has executed on a console. The game
launch is USER-COORDINATED, never automated. This plan stages ONE syscall-free
rebuild of the code-anchor probe after the igf5 zero-record + crash event.

## 1. What is being staged

- Deliverable: `modbridge_ingame_facts.prx`, build **igf6-ad9bda1fc50a**
- sha256: `03f6a19c67a8bc955047bc2a10d8833e007e461e90e17024048331817f515a4c`
- size: 24173 bytes (outer fself), inner ELF 85472 bytes
- path on console: `/data/plugins/modbridge_ingame_facts.prx` (SAME path,
  SAME ini line — same-file replacement of igf5-4186048a6dd6; no ini edit)
- log path (UNCHANGED): `/data/modbridge-ingame-facts.jsonl`
- marker: `modbridge.ingamefacts/v4 build=igf6-ad9bda1fc50a`

Determinism: two independent builds (run1/run2) byte-equal on final PRX,
inner ELF, combined object, and import contract (see `build-receipt.json`).

## 2. What changed vs igf5 (and why it is the safest next probe)

| mechanism | igf4 (loaded, logged) | igf5 (zero records, crash) | igf6 (this build) |
|---|---|---|---|
| anchor read | gate REFUSED (VMMAP prot=0x4, policy) | mprotect-first via RAW `syscall` rax=74 (2 sites) | plain direct dereference, 16 B, NO syscall |
| raw syscall sites | 0 | 2 | 0 (asserted by build gate + `0F 05` byte scan of final .text) |
| imports | 7 verbatim NID rows | same 7 | same 7 |
| GUA predicate | uncorrected, ran gated | tbd record, memory untouched | CORRECTED (worker-predicate-gc-fix) cfgH replay over a STACK copy — zero live derefs inside the predicate |
| mapscan (sysctl 202) | ran, console-proven | ran | ran, CONTEXT-ONLY, never gates reads |

Justification for the direct read (recorded in-recode, seq 4/5):
known code addresses inside the mapped exec span; x86-64 NX gates execute,
not read; VMMAP prot=0x4 is kernel policy, not hardware read enforcement;
igf4's refusal was the GATE refusing — the hardware never faulted.

## 3. Host verification already done (no console)

- `check_igf6_artifact.py` PASS (both runs): fself wrapper/segment copies,
  SDK 0x11000000 stamp, init/fini 1 slot each, exactly 7 NID strong imports,
  0 weak, no forbidden identity (incl. `sceKernelMprotect`, `igcar_sys_mprotect`),
  ZERO `syscall` instructions in the combined object, NO `0F 05` pair in the
  final inner-ELF `.text`. See `determinism/run*/artifact-checks.json`.
- Expected bytes stamped from `worker-anchor-validation/expected_bytes_tick_sco.txt`
  and cross-checked against `worker-expected-bytes/EXPECTED-BYTES.json`
  (Tick 0x2fac8c0 / SCO 0x490aa0, `554889e54157415641554154534881ec`,
  retained image sha256 3b6cfad4…).

## 4. Staging steps (user-coordinated)

1. Verify the staged file hash BEFORE replacing the console-side PRX
   (`shasum -a 256` must equal the sha above; refuse divergent content).
2. Copy `modbridge_ingame_facts.prx` (igf6-ad9bda1fc50a) to
   `/data/plugins/modbridge_ingame_facts.prx` — replacing the igf5 payload
   (igf5 must NOT remain staged anywhere).
3. Truncate/rename any stale `/data/modbridge-ingame-facts.jsonl` BEFORE the
   launch so the run's records are unambiguous (recommend rename, keep
   provenance).
4. Launch Townfall via ploader with the plugin ini line UNCHANGED.
5. The plugin is INERT after one synchronous module_start pass (records
   seq 1..7, channel closed, returns 0). No unload, no threads.

## 5. Expected evidence (falsifiable predictions)

- seq1 entry, seq2 startup, seq3 mapscan records: same shape as igf4's
  (write-through channel proven by igf4).
- seq4 `tick_anchor` / seq5 `sco_anchor` with `method_used=
  "direct_dereference_no_syscall"`, `bytes_read_hex` present, `byte_match`
  0 or 1 (either is a VALID result; match=1 confirms the retained-image
  correspondence), `in_known_exec_span=1`.
- seq6 `gua_pred_replay` status=performed, corrected_gc_order_present=1,
  cfgH verdict per the corrected GC-order invariant.
- seq7 summary: `anchors_confirmed`, `pred_validated_any_cfg`,
  `promotion_gate_indicator` — INDICATORS, not claims.
- NO crash is expected: the binary contains zero syscall sites and the only
  foreign dereferences are 3 bounded reads (16 B x2 code + 96 B GUA data)
  at addresses igf4's own evidence marks mapped (code span / readable data).

## 6. Interpretation matrix (decides the igf5 diagnosis)

| outcome | meaning |
|---|---|
| records seq1..7 present, no crash | kernel-level igf5 failure CONFIRMED (raw syscall 74) — igf5's shim must never be reused |
| records seq1..3 then stop, no crash | direct read faulted (contradicts NX reasoning) — stop, reconcile, new design |
| ZERO records again, crash | load-stage/content-derived console rejection OUTSIDE parsed structures — static analysis exhausted; escalate to controller with both binaries |
| ZERO records, no crash, igf4 also now zero | staging/channel issue, not binary — recheck ini/log path |

## 7. Abort / rollback

- Any crash or zero-record outcome: STOP, capture the console state, do NOT
  re-launch without controller reconciliation (never blind replay).
- Rollback payload: igf4 probe (known-good,
  `worker-anchor-validation/modbridge_ingame_facts.prx`,
  sha256 4aa624cc…) restored to the same path.
- igf6 remains proposal-only; no promotion claim is made from host checks
  alone (rubric: in-game evidence required for any >host-check claim).
