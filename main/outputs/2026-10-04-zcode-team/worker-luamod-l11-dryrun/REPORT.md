# REPORT — worker-luamod-l11-dryrun (2026-10-05)

Mode: LOCAL ONLY, host-side. Mission: prove the luamod DIFF-1 admission path
EXECUTES (launch-11 preparation) by dry-running the delivered
`worker-luamod-rebase` INTEGRATION.md runbook in a scratch area. No console
contact, no target execution, no nested subagents, NO PRX packaging (stopped
by mission bound before the shared serialized packaging pipeline), no writes
outside `outputs/2026-10-04-zcode-team/worker-luamod-l11-dryrun/`. The
launch-10 canonical trees (`launch10-build/`, `prx-candidate-*`), and the
worker-fname-heap-runtime-bounds / facts-geometry-fix / lease-coverage-fix
dirs were not touched. Deliverable order: CLAIM.json first, DRYRUN.md,
scratch artifacts, VERDICT.md, this REPORT last.

## 1. Headline result

**The admission path EXECUTES.** A real (scratch) compose with the delivered
L10LM editions produced:

- `retained_units_replaced = ["luamod","program"]` — the frozen-script
  admission (`RETAINED_REPLACEABLE={'program','luamod'}`, the ONE-line
  L10LM E0 edit) accepted the luamod replace-unit; SEC32-3 argv-drift gate
  measured drift indices EXACTLY {165,168,171} = source/-o/-MF under the real
  172-token argv; receipt coverage + 1306/1306 fresh-dep pre-coverage held.
- Composed object `633a8c82d71b6233989bc5ada83bc06ac6b65e1cf11ef769e00601b6ea2ccda4`,
  117,742,096 B, 368 units — reproduced BYTE-IDENTICALLY in two passes.
- The composed run's `luamod.o` = `5595feb785206c0057eb29af0ccdcd53a8898d866
  29db0b10debc4a17aa08dea` — byte-identical to the rebase gate's variant-B AND
  variant-A objects: the executor-tag formulation equivalence is confirmed in
  the real composition.
- vs r3-armed run-a: 368/368 unit names equal, EXACTLY ONE object differs
  (luamod); program + initializer byte-identical; undefined_delta identical
  (single SDK baseline extension `sceKernelGetModuleInfo`; zero new imports
  from DIFF-1).
- Chain-symbols gate 14/14; all INTEGRATION.md §3 fail-fast traps stayed
  silent. Compose wall time: 75 s (first pass) / 30 s (second).

**Link-inertness: HOLDS, stronger than briefed.** llvm-nm on my scratch objects
and on the real composed objects: `arm_funcptr_marshal_drain`,
`funcptr_marshal_hook`, `r3_marshal_carrier_precheck` are NOT "present but
unreferenced" — they are dead-code-ELIMINATED (absent from all 25,373 luamod.o
symbols incl. locals, and from the 117.7 MB composed object), with zero
undefined references anywhere. The live surface IS in: `m_funcptr_marshal_armed`,
`is_funcptr_marshal_armed`, the `FuncPtrMarshal` Lua key. The `R3Marshal*`
symbols seen in the composed object are DEFINED by the initializer unit
(lineage-inherited, byte-same in r3-armed; empty UND sets both lineages).
Consequence unchanged: nothing arms the marshal or drains the deferred pump —
INTEGRATION.md §6 controller decision.

## 2. Answers to the mission's three questions

1. **Does the frozen-script admission accept the luamod replace-unit?** YES —
   in a real compose, exit 0, `retained_replaced: true`, all three SEC32-3
   sub-gates (rr coverage, argv drift, fresh-.d pin coverage) passed.
2. **Does the compose argv build with variant B?** YES — driver compose_argv
   carries the second `--replace-runtime-unit luamod=...staging/luamod-l10rebase/
   LuaMod.cpp`; the fixed script redirected the pinned argv source/-o/-MF and
   compiled it clean inside the compose (object == gate object).
3. **Object results + inertness?** Composed 117,742,096 B / `633a8c82...`;
   luamod.o 6,903,984 B / `5595feb7...`; +1,936 B / +1,808 B vs r3-armed;
   UND 565 == pinned; drain cluster eliminated (see §1). Scratch single-unit
   recompile also documented: exit 0, 1306 deps, symbol sets identical to the
   gate object (size delta fully attributed to the source-basename embedded in
   the static-init symbol).

## 3. Finding for the controller (L11D-F1)

The delivered driver's default `--program-source` points at
`worker-luamod-rebase/staging/comp6-program/UE4SSProgram.cpp` — which does not
exist (the rebase staged only initializer + luamod). The regenerated receipt
covers the canonical `worker-comp6-driver/staging/comp6-program/
UE4SSProgram.cpp` (`75abf2ab...`), which the r3-armed run passed explicitly.
Launch-11 MUST pass that path explicitly (or re-stage + re-run
`regen_receipt_l10lm.py`). INTEGRATION.md §3's "all other arguments have pinned
defaults" is wrong for this one argument; a default-run fails closed in
receipt_preflight. Recorded in CLAIM.json + VERDICT.md step 2.

## 4. Honest limits

- **No packaging.** `build_overlay` + `package_phase` (`package_prx.py`
  `e6fbe6c5...`, the shared serialized pipeline launch-10 uses) were NOT
  executed; no prx-candidate dir exists here. Launch-11 packaging is a
  controller-owned serialized step and remains unproven.
- **Host-side only.** Nothing ran in-game; no console; no I1/I1+ import
  measurement of a packaged candidate; no gate v2 run; no manifest written
  (all controller steps, enumerated in VERDICT.md).
- **Scratch receipts are not canonical.** The run dirs live under this worker
  dir with their own receipts; `receipt.json` shas differ between passes by
  embedded paths — object shas are the durable comparators (proven
  path-independent by run-a == run-b).
- Scratch driver copy carries exactly TWO documented path-redirect edits
  (MY, COMPOSE6_FIXED), diff-proven against the pinned `9b332fc5...` edition;
  the fixed-edition and receipt copies are byte-identical to their pins. All
  originals read-only; nothing was executed in place in another worker's dir.
- The 587 MB scratch run dirs are left in place as evidence; delete only with
  controller concurrence.
- No quality/launch/completion claims: DIFF-1's drain remains inert as
  authored; F-1 stays open; the drift-witness exposure class for OTHER
  retained units (rebase REPORT §3) is untouched by this dry run and remains
  flagged for the controller.

## 5. File inventory (this dir)

| file | what |
|---|---|
| `CLAIM.json` | 8 claims + non-claims + 2 controller findings + full pin list (written first) |
| `DRYRUN.md` | step-by-step execution log with shas (this runbook run) |
| `VERDICT.md` | GO + the exact ordered remaining controller steps |
| `REPORT.md` | this summary |
| `scratch/build_runtime_composition6_fixed_l10lm.py` | byte-identical fixed edition (9de2c4c8...) |
| `scratch/build_runtime_composition6_l10lm.py` | executed driver (2 path redirects vs 9b332fc5...) |
| `scratch/build_runtime_composition6_l10lm.py.pinned-original` | byte-identical driver copy |
| `scratch/receipts/composition6-replace-receipt.json` | byte-identical receipt (27c1b3b0...) |
| `scratch/run_dry_l11.py` | harness (driver main() sequence + STOP bound) |
| `scratch/staging/` | dryrun variant-B object/.d, pinned argv extract, chain-symbols.json, gate logs |
| `scratch/ue4ss-native-supplier-composition-6-l10lm/run-a, run-b` | the two full scratch composes (receipts, objects, commands.json) |
| `logs/` | console logs, sha-gates.txt, lineage diffs, scratch-driver diff, symbol lists, section-size diffs, R3Marshal UND comparisons, run-receipts-summary.json |
