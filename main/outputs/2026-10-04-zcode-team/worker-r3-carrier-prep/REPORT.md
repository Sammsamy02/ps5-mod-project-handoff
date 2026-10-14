# REPORT — worker-r3-carrier-prep (2026-10-04)

Mode: OFFLINE DERIVATION + DESIGN ONLY. No console contact, no nested subagents,
nothing executed in-game, nothing staged. Deliverable order honored: CLAIM.json first,
CARRIER-CANDIDATES.md, this REPORT last. Tool calls: ~30 (within the 30–60 budget).

## 1. What was asked and what was delivered

Task: close what is closable OFFLINE about `worker-r3-zerosyscall-tier`'s open fact F-1 —
whether the FuncPtr-marshal (R3D §4) can find a calibration carrier UFunction on
Townfall — and design the launch-10/11 ridealong probe extending the fc1/qdrv1 lineage.

Delivered in `TEAM:worker-r3-carrier-prep/`:
- `CLAIM.json` — 5 claims incl. one recorded oracle REFUTATION (CCP-2), corrected
  mid-session to match actual evidence per the honesty discipline.
- `CARRIER-CANDIDATES.md` — precise carrier requirements C-1..C-7 (R3D §4.2/§4.3-derived,
  incl. the engine-driven/no-mod-called-names rule derived from R3D §2 + IS's six
  `ExecuteInGameThread` closures); the ranked candidate list with per-candidate offline
  records; the in-runtime calibration record shapes (jsonl); the full cc1 ridealong probe
  spec; the honest F-1 verdict + the pre-declared strict-gate tension.
- Raw oracle artifacts: `image-oracle-results.json`, `lea-refs.json`, `ctor-windows.json`,
  `region-names.json` (all generated this session; the 25 MB reloc index stays beside the
  image at `.superpowers/native-observations/townfall-static-executable-2026-10-02/rela_index.bin`).

## 2. Key findings

1. **The pinned image is a richer oracle than expected, but native-ness is not in it.**
   sha re-verified (`3b6cfad4…`); ELF64 parses; segment map cross-checks retained launch
   facts (seg7 vaddr 0x913c000 = L7 span; FName ctor label 0x23d590 visible as a repeated
   callee in class-construction code; pool label 0x9365ec0 lands in seg7 BSS). Three
   oracles were run: in-place pointer pairs (0 hits), DT_RELA addend pairs (1,649,556
   relative relocs indexed; 1 blind false-positive pair), rip-relative LEA references
   (9,248 sites — POSITIVE). Conclusion: this UE5.6 build carries NO statically readable
   name→thunk binding table; FUNC_Native must be read in-game (calibration step 1). This
   refutes the mechanism CLAIM.json originally proposed and is recorded as such.
2. **`PostRender` does not exist on this title** (0 exact occurrences, UTF-16 or ASCII),
   nor `DrawHUD`/`PlayerTick`/`UpdateCamera`/`UpdateGameMode`/`TickComponent`/`TickActor`/
   `NativeTick`. The community-standard carrier class is name-level refuted offline —
   the calibration budget shrinks accordingly.
3. **The surviving set is small and anchored:** `ReceiveTick` (2 ctor clusters — the
   AActor-family blob listing 23 engine UFunction names, and a Character-family blob —
   plus 4 references from the game's own 0x44xxxxx code region that resolve the name at
   runtime), `BlueprintUpdateAnimation` (AnimInstance blob), and ambiguous `"Tick"`.
   `ReceiveAsyncPhysicsTick` and `BlueprintThreadSafeUpdateAnimation` are NAME-CONFIRMED
   but excluded by construction (physics thread / parallel anim worker threads fail the
   game-thread requirement C-4).
4. **The in-game walk needs zero new seeds:** FName ctor (slot 2) and GUObjectArray
   (slot 0) are already `present:true` in the landed seed table; the walk protocol is
   the STF §5 chain + port object services (CARRIER-CANDIDATES §4.4).
5. **cc1 ridealong (fc1 → qdrv1 → cc1 lineage):** a standalone data-side prx that
   censuses the LIVE FName pool for the candidate names (fc1's validated decoder,
   qdrv1's admission gates and 30x4 s cadence, own single-writer log
   `/data/modbridge-carrier-facts.jsonl`). It runs in EVERY launch window (T-A included),
   needs no UClass offsets (B0-independent), and honestly delivers name-truth only —
   rate/thread stay with the R3D §4.2 in-runtime calibration (jsonl shapes §5).

## 3. F-1 verdict (honest)

NARROWED, NOT CLOSED. Offline evidence reduced "does ANY carrier exist" to "is
`ReceiveTick` (or `BlueprintUpdateAnimation`, or whatever the census resolves for
`"Tick"`) FUNC_Native, steady-rate, game-thread-stable" — one calibration session
against a ≤3-candidate trial set, falsifiable either way. If the calibration records
`f1:"FALSE"` with its trial table, R3D §4.2 step 6 fires (R3-only spawner verdict = NO,
loudly) exactly as designed. A pre-declared tension is recorded for the controller: if
the top candidates prove script-only (BP-event class) while still firing, the strict
FUNC_Native gate — not the measurement — would be what refuses them, and that is a
scoped design decision for the R3D owner (its §4.6 extension), not a claim this worker makes.

## 4. Caveats and future work

- Flags-immediate extraction needs a real sequential disassembler (capstone over the ctor
  functions, target the UFunction-ctor call argument); the ad-hoc parser produced the
  non-flag `0x1ba04` and was stopped rather than trusted.
- Class attribution for the candidate names rests on UE5.6 engine knowledge
  (confidence-labeled); the census/UClass walk is the decider.
- The `region-names.json` AActor-blob inventory mixes function and property names; it is
  a stand-in for the in-game function-map enumeration, not a replacement.
- UMG names (e.g. `SetText` UTF-16) absent from the image while `UserWidget` is present
  suggests partial modularization; uninvestigated, no claim made.

## 5. Read-order compliance

R3D read first in full (contract source); L10M, STF, FC STAGE-PLAN, QV1 STAGE-PLAN, and
main.lua read for evidence before design. No event sent, no queue contact, no dispatch.
