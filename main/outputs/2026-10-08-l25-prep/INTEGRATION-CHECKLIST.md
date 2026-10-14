# l25 integration checklist — gate before the final twin build

Root-maintained. Every box must be checked before agent R's final build runs,
and again before the packet goes to Claude. Status markers: [x] done, [~] in
flight (agent), [ ] pending.

## Code landing

- [x] K review fixes (agent N) — applied; independently verified (X): 85/85
- [x] Item-6 host suite re-pointed to l25 headers (agent S) — 2/2 PASS;
      re-run command: python3 .../2026-10-08-l25-runtime/tests/run_tests.py
- [x] Phase tags (agent P) — applied; verified (Z4): 34/34 + staging pins
      byte-exact (UE4SSProgram/LuaMod edited, ScriptHookThunks diff-empty)
- [x] Listener fix + instrumentation (agent O) — COMPLETE AND FINAL:
      slot-4 GetAllocatedSize added as last virtual (base header row aliases
      tree-wide incl. LuaUObject — no derived edits needed); registration
      hardened + instrumented; engine locked path documented unreachable;
      bounded unlocked write (mutex + GMalloc gate + RISK logging; allocator
      mismatch NOT present per RC FMemory routing); variant-B switch token
      verified vs build_variants.py:39; CHANGES.json final 18 rows / 54 pins
      green / cross-part duplicate scan clean. Z9's LuaUObject-row trap
      sidestepped (no row — file unedited).
- [x] Copyin probe liveness overlays (agent Q) — files landed under
      2026-10-08-l25-runtime/copyin-probe/ + WIRING.md; R rebases Q's patch
      ONTO O's efdef253 UnrealInitializer base (never adopt Q's pre-O file).
- [x] Edit-path determination (agent Z9) — sanctioned = staged candidates +
      CHANGES rows vs l24-consumed originals; R warned on silent-no-op rows.
- [x] Container offsets / items 1-5+7 of Claude's original l25 scope — done
      (earlier batch + M inventory)

- [x] Twin-build drivers (agent R) — build_variants.py + l25_packaging_driver.py;
      dry-run green (662 steps/45 gates + 57 gates); Q rebased onto O base
      (0 deletions/96 additions, O edits verified present); loud no-op-row
      guard; affected-units proof for O's 4 units; pair gates A!=B; B labeled
      unsafe in every receipt. REAL --execute is ROOT-GATED on Z12+Z13 green.

## Final build command sequence (root, after verification gate clears)

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 outputs/2026-10-08-l25-runtime/build_variants.py --plan
python3 outputs/2026-10-08-l25-runtime/build_variants.py --execute
python3 outputs/2026-10-08-l25-runtime/l25_packaging_driver.py --plan
python3 outputs/2026-10-08-l25-runtime/l25_packaging_driver.py --execute
# then: item-6 suite re-run; packet --check + assemble; Claude review
```

## Verification gates

- [x] E1 CLOSED (Z13 found -> Z19 fixed -> Z12 re-verified post-fix):
      UObjectArray.cpp:610 const_cast->static_cast; compiles 0-error on host
      AND real prospero; CHANGES updated (UObjectArray.cpp now 8f45e6ce...).
- [~] E3 IN FLIGHT (Z13 + Z12 corroborate; R resumed): header routing gap —
      without it the slot-4 repair silently drops to 4-slot vtables in most
      TUs. R fixing target_compile.py/vfs routing + bridge-pin verification.
- [x] O-output verification (Z12): GREEN — layout proof on REAL prospero
      compiler (all 3 listeners 5 slots, slot-4 = GetAllocatedSize,
      create-listener correctly 4); hash sweep 108/108; caught E1
      independently, tracked Z19's concurrent fix, re-verified post-fix;
      notes O's REPORT is stale on 2 cosmetic points (CHANGES itself
      consistent).

- [~] O-output verification harness (dispatch when O lands): compile+run the
      listener edits (X/Z4 pattern), prove vtable slot fix with
      -fdump-record-layouts / static_asserts (Z8 pre-validates the layout
      claim), assert variant-B switch compiles both ways.
- [~] Z8: listener usage-site map + layout proof (running)
- [~] Z9: sanctioned edit-path determination for port-root files (running)
- [~] Z6: engine-side GetAllocatedSize reachability during save load (running)
- [ ] Final: item-6 suite re-run against the COMPLETE l25 tree (post-O)
- [ ] Final: target compile (target_compile.py) producing TARGET-COMPILE.json
- [ ] Final: twin build (A + B) via R's driver — per-variant static gates +
      import-identity checks + composed-object hashes + receipts; B labeled
      diagnostic-only/unsafe-long-term.

## Evidence + packet

- [x] Vanilla klog verdict — signature is ours (evidence/ + VANILLA-VERDICT.md)
- [~] Z1: plugin-run crash baseline from l23 klog (running)
- [~] Z5: packet assembler + manifest template (running); run --check after
      each landing; assemble only after build artifacts + verdicts exist.
- [~] Z7: LAUNCH-PLAN interpretation matrix v2 (running)
- [x] Harvest tool (Y) — one command per launch; point at /tmp/l25-vanilla/
      style tap dirs explicitly when needed.
- [x] Provenance audit (Z3) — 24/24; re-run after O/R land.

## Console preconditions for the NEXT launch (post-packet-approval)

1. Settings: restore ORIGINAL 1947 B (b980cea7...) — currently v2 variant.
2. ploader.ini: re-enable ONLY modbridge_supplier_composition.prx (=2) from
   backup /tmp/ploader-ini-backup-20261008.ini (sha 3eab9510...), then dispatch
   loadpayload ONCE (ini edits take effect only at the NEXT arm — Z2 law) and
   confirm a fresh payload.elf pid.
3. klog tap: start BEFORE launch (TCP 3232; klog_capture.py, new-file rule).
4. Variant A first: menu 90 s -> load save -> 30 s -> L3+R3 -> clean exit.
   Variant B only if A stalls/crashes. Harvest after each launch (Y tool).
5. PRX upload discipline: fake-sign (make_fself.py — REAL path in this
   worktree: .superpowers/native-sdk-build-check/ps5-payload-sdk/samples/
   install_app/, per W's finding), decrypt-OFF readback verify (magic
   4f153d1d + sha256 match) BEFORE launch. One launch per clean cycle.
