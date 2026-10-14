# REPORT — worker-ue4ss-docs-fidelity

2026-10-05 · read-only fidelity pass over the runtime-expectation doc chain; no console
contact, no nested subagents, nothing executed. Deliverables in this directory:
`CLAIM.json` (first), `FIDELITY-REPORT.md` (full evidence), this `REPORT.md` (last).

## Scope

Docs verified: `worker-launch9-prediction/LAUNCH9-PREDICTION.md`,
`worker-engine-ready-walkthrough/ENGINE-READY-PATH.md`,
`worker-launch10-matrix-prep/LAUNCH10-MATRIX.md`,
`worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md` (the CP-table runbook;
`worker-rollback-runbook/ROLLBACK-RUNBOOK.md` was also read — it is ploader/slot mechanics
and cites none of the five runtime strings/constants in scope, so it has no fidelity
findings).

Final artifacts read back: merged initializer staging (2044 lines), bridge r13
(sha `10385e22…`, r12 frozen `feed5254…`), entry r4, `SeedTable.h` (3-slot) vs
`SeedTable.launch10.h` (4-slot, GMalloc linked 0x930add8 / runtime 0x970add8), settings v3
(1448 B, sha `d8faa4ec…`), plus the derivation JSONs and the comp6 driver pins.

## Verdicts

| Doc | Verdict |
|---|---|
| LAUNCH9-PREDICTION | **CURRENT** — every refusal string, settings key, entry format and bridge tag verified exact; MERGED line numbers drifted (2006→2044 lines; key sites :876→:908, :1876→:1914, :1749-1764→:1787-1804, :635-641→:662; GMalloc :396-398 unchanged); bridge citations are exact r12 and the final r13 is behavior-identical with shifted lines + new `bridge_lease_reg_spans` |
| ENGINE-READY-PATH | **STALE (superseded)** — faithful to its own superseded artifact; final chain skips ps_scan/verifier when armed, gates ProcessInternal on v3 keys, binds GMalloc from seed; its reused P1–P6/§6 parts stand |
| LAUNCH10-MATRIX | **CURRENT as router, STALE in details** — U-SWAP-1 has landed (4-slot table staged per the recorded sequencing); `--bridge-sha256` r12 placeholder → r13 default; U-SWAP-5 initializer line cites shifted; tick/SCO still unconditional as it claims |
| MASTER-VERIFICATION-RUNBOOK (CP table) | **STALE-with-fix** — CP2 tick constant invalidated; §4.3a ini-sha restage heuristic invalidated by v3; §1 "no timestamp" stale; error-code table and D1 tags CURRENT |

## Invalidations that matter

1. **CP2 tick constant (the one hard invalidation).** The runbook's expectation
   `0x37ac8c0` = "linked `0x33ac8c0` + base" is reversed by the final byte evidence
   (SEED-TABLE.json slot-10 correction; initializer comment :1746-1757): **runtime Tick =
   base + linked 0x2fac8c0 = 0x33ac8c0**; 0x37AC8C0 was the runtime value mislabeled as
   linked. Also: with seeds armed the scan arm never runs, so CP2's "both addresses equal"
   criterion can never print (vtable-only fallback, live-derived address).
2. **Launch-9 candidate scope resolved.** Shape A shipped (compile-time
   `kRequiredSetComplete` gate, throw at :908) and the gmalloc constant was deliberately
   held out of the launch-9 build (SEED-TABLE.json :106/:112) — so launch 9's predicted stop
   is the loud 3-slot refusal (the doc's candidate #2). Under the launch-10 armed 4-slot
   table the doc's candidates #1 (GMalloc throw) and #2 are UNREACHABLE (gate passes; GMalloc
   binds at :781 before PostInitialize).
3. **Settings v3 restage breaks the old ini-sha tripwire.** The runbook's "ini sha
   `9f3fa127…` diverges ⇒ restage" heuristic is void: v3 (`d8faa4ec…`, 1448 B) is the
   authorized staged content.
4. **IGW gating off, confirmed.** `kExecWritabilityVerdictGating = false` (:620-637,
   "DESIGNED, NOT ARMED") — the merged initializer reads no verdict file; the folded shape
   is the compiled Shape-A default.
5. **STOP_EXN renumber: no doc affected.** Final header is CONVERSION=1, CONTEXT=2,
   START=3, INTERNAL=4, STOP_EXN=5; the only doc table (runbook §1/§6) covers start codes
   1-4, unchanged, and no doc ever cites STOP_EXN. The launch-9 "Zydis block gated off"
   claim is likewise CONFIRMED (not changed) by the v3 key-off (gate :1914).

## Required fixes if the docs are touched again (none applied by this worker)

- Runbook CP2/G3/§6: replace 0x37ac8c0 with runtime 0x33ac8c0 (linked 0x2fac8c0); drop the
  "both addresses equal" criterion under the armed path.
- Runbook §1: add the `[t=<ms>ms pid=<pid>]` prefix to the L failure-line grammar; §4.3a:
  re-pin the settings identity to v3 sha `d8faa4ec…` (1448 B).
- Matrix §3.1 + OPC (:21/:63/:77/:85/:129): `--bridge-sha256` placeholder r12 → r13
  (`10385e22…`); matrix D1 `:876` → `:908`; U-SWAP-5 cites :1876→:1914,
  :1385-1397→:1425-1432, :1749-1764→:1787-1804, :480→:479-481.
- Launch-9 doc (optional refresh): MERGED line table per FIDELITY-REPORT §2; §0 "Bridge r12"
  → note r13 as shipped default; "2006 lines" → 2044; generator cite :93/:126 → :214.

## Honesty

Static file read-back only; no launch occurred and no outcome is predicted beyond what the
recorded sequencing decision already states. Line numbers reflect on-disk mtimes as of this
pass and re-shift with any further merge. No quality or completion claim is made.
