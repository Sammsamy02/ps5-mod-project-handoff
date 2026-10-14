# REPORT.md — worker-no-exec-mode-gate, 2026-10-04

PROPOSALS ONLY. No console contact, no nested subagents, nothing executed in any engine;
the merged initializer was not modified (work ran on a byte-verified local copy). This is
the U-SWAP-5 diff set for the NEXT revision.

## 1. What was delivered

| file | content |
|---|---|
| `CLAIM.json` | 4 claims (NE-1..NE-4) + non-claims; written first per packet order |
| `NO-EXEC-GATE-DESIGN.md` | full design: enumeration (§1), gate + source recommendation (§2), degraded path + signatures (§3), arm/disarm (§4), evidence/bounds (§5) |
| `NO-EXEC-GATE.diff` | the exact unified diff vs the staging baseline — 11 hunks, 374 lines, sha256 `339ef3c46f278d0128d88b10bc7839d2c12d4f8156447a700985473ec7598a92` |
| `UnrealInitializer.staging-baseline.cpp` | byte-identical frozen-unit copy, sha256 `c85c42f7aaab0630569d65db599fbf8cfcbd0b79b8e99c6e27ea709aee5a8b7b` |
| `UnrealInitializer.gated.cpp` | gated candidate (fallback default 0 = frozen behavior until armed), sha256 `40d6cd4974722e0db2af77dc0ca9b648a9d2c8ba3d8bc2143c01d92da5431531` |
| `apply_gate.py` | anchored replayer (28 replacements, aborts on drift) |
| `host-test/gate_pattern_mock.cpp` | compile/warning evidence for the inserted hunks |

## 2. Answers to the four tasks

1. **Enumeration (design §1):** nine exec-site groups, E-1..E-9, current-staging line
   cites, with the matrix/L9 cite-base drift mapped. The two matrix-named unconditional
   sites are confirmed (I7 = EngineTick registrations `:1787-1802`; I12 = SCO callback
   `:480-507`, which has NO gate of any kind). Sites the matrix row did not name:
   LoadMap walk `:1690-1698` and the Tick walk `:1702` (FLAG-2, I6/I7-resolve class),
   the vtable display walk `:1707-1737` (latent when armed), the eight further resolves
   `:1810-1873` (I8), the PI/PLSF derivation `:1914-1975` (FLAG-3, settings-only
   closure), the FName ctor detour verifier `:1317` via `:1424-1434` (FLAG-1, live
   whenever seeds are not armed), and the legacy ps_scan/Lua-AOB address path
   (`:957-981`, `:1186`, `:1286`) — the only address source when the table is absent.
2. **Gate (design §2):** compile-time `UE4SS_NO_EXEC_MODE` → `constexpr
   NoExecGate::kNoExecMode`; build define with in-file fallback default 0. Compile-time
   recommended over a settings key on five grounds, the decisive one being the v3
   precedent's own documented trap (missing/invalid key silently keeps `true` — a typo
   would silently re-arm exec). 15 `if constexpr` sites: 12 named loud refusals
   (counter-fed), 2 throws (seedless startup; defensive FName verifier), 1 summary line.
   Original bytes preserved inside constexpr-dead `else` branches (the unit's own S2
   no-reindent precedent).
3. **Degraded path (design §3):** full data-side keep-alive list (seeds incl. the
   behavioral probe, pools/listeners, reflection, S2 layout gate, Kismet fallback); dead
   list each named at startup (identity publication, ExecuteInGameThread drain AND its
   ProcessEvent cross-fallback, ten virtual hooks, SCO post-init adds, PI/PLSF); R3
   compatibility argued from VT §5 R3 (heap SetFuncPtr write survives BOTH_FAIL/FATAL)
   with the spawner-dependency map marked **PENDING-R3** (worker CLAIM-only at design
   time). Signatures table keyed to matrix consumption: R8's T-A reads byte-identical
   (the `[Seeds]` partial-table throw strictly precedes all `[NoExec]` logic); R9 gets a
   build with no mprotect/jmp-write path at all; the healthy armed row ends with
   `[NoExec] summary: mode=on exec_sites_refused=<N> engine_ready_data_side=1` +
   untouched runtime-status formats and the unchanged I13 ack chain. One new evidence
   string (`[NoExec] REFUSED: …`) is flagged for the next matrix revision's box-2 list.
4. **Diff + procedure (design §4):** `NO-EXEC-GATE.diff` is exact against the hashed
   baseline; arming = flip the in-file define to 1 (receipt-covered, recommended) or the
   build define (manifest-recorded), then BOTH receipt regens with `--program-source`
   at the new staging dir, `--verify-only`, one pass; disarm = flip back to 0 with
   mandatory receipt re-pinning. No runtime arm/disarm exists by design.

## 3. Evidence performed here

- Full read of the 2044-line merged staging unit; cross-checked against
  LAUNCH9-PREDICTION (hop table I1-I13, §1 exec classes), LAUNCH10-MATRIX (U-SWAP-5,
  R5/R8/R9 rows, §4 checklist), SETTINGS-V3-DECISION, SEED-TABLE-FORMAT arming cites,
  the R3 CLAIM, and the ASMHelper/macro spike snapshots (confirming the jmp-probe text
  read and the absent PROSPERO branch).
- Mechanical transform integrity: 28/28 exact anchors; brace-depth landmark equality at
  every function boundary; final depth 0/0; ASCII-only literals; no escape accidents.
- Host mock compile: `-std=c++20 -Wall -Wextra -Werror` PASS under gate=0, gate=1, and
  undef; runtime probes: gate=0 exit 0 (frozen behavior), gate=1 surfaces the designed
  REFUSED throw (exit 2); the `[[maybe_unused]]` suppression for the
  QueryHarnessEngineTickDispatch file-static case reproduced and confirmed.

## 4. Honest limits / handoff

- No real-TU compile (no include tree here), no engine run, no verdict assumed. The
  zero-warning status of the gated file on the actual toolchain must be confirmed by the
  comp pass before staging.
- ExecuteInGameThread's polling replacement (EW §7.3) and the R3 wrappers are
  composition-level dependencies deliberately OUTSIDE this diff; without the former,
  menu acceptance is honestly degraded in R5/R9 (matrix already says so).
- If the frozen staging copy ever moves, `apply_gate.py` against the new base is the
  drift detector; do not hand-patch.
- Codex usage: zero (GLM-only session; local reads only, no web requests).
