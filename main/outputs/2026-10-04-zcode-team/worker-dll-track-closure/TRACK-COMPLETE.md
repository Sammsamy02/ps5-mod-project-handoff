# TRACK-COMPLETE — Windows-services DLL track (one page)

Milestone definition: what makes the DLL track **done** for the project's
milestone purposes. Nothing here upgrades evidence; each criterion names its
verifier. Checked 2026-10-04 by worker-dll-track-closure.

## The 8 criteria

1. **All 8 families host-complete.** InitOnce, Mutex, FileIO, CRT startup,
   Module enum, VM, Static TLS, SEH64 — each with passing host validation
   receipts (sanitizers/TSan where applicable) and, where composed,
   `OWNED_CANDIDATE_HOST_CONTROLS_PASSED`. — **MET** (owning workers'
   receipts; INDEX Track 4).
2. **Combined-candidate identity frozen and verifiable.** The 8-family ELF
   `c3aba79f` sha256 `79281191…d804fb`, 336552 B, re-hashable at any time;
   readback law (upload == target readback, EXACT) in the frozen plan.
   — **MET** (re-hashed this session; CONSOLE-STAGE-PLAN-8FAM identity table).
3. **Six-family console PASS on record.** ALLFAM_DLL_TEST `133c5e9e`, 4
   target records, ledger match — proves the substrate + 6 families on the
   real target route and pre-proves the 55-symbol SDK import surface.
   — **MET** (CONSOLE-LOG item 13; console receipt + result copy).
4. **The 8-family console run (dll8) executed per the frozen plan with a
   verdict against the prepped oracle.** PASS = all 4 records match the
   8FAM ledger (seh64 key, 90 imports, results [86,93], memory.last
   114688/163840, cleanup retired, first_failure zeros) → all 8 families
   hold console evidence and the combined admission is proven: **track done**.
   - SEH64-specific FAIL (−130..−137 / stage-55/56 phase-2 stop / imports
     88–89) + successful frozen 7-family fallback = **closure with a named
     carve-out**: 7 families console-proven, SEH64 target surface recorded
     as failing with root-cause attribution; reconcile host-side before any
     re-run (new build_id, no blind replay).
   - Shared-surface FAIL (anything outside the SEH64-specific set) = **not
     done**; reconcile against the vendor tree first.
5. **No claim inflation at closure.** Component evidence only: no
   Windows-loader parity, no ntdll/VCRUNTIME parity, no >8 quality score,
   gameplay remains a prototype. The 8fam plan's own honest-scope paragraph
   is the wording of record. — **BINDING at closure.**
6. **UE4SS acceptance untouched.** No acceptance-ladder rung depends on any
   DLL result (EVIDENCE-SPEC independence, documented in DLL-TRACK-STATE
   §7); dll8 stages payload-only with the game closed and touches neither
   ploader.ini nor the PRX slot. **Neither track gates the other.**
   — **MET by construction; re-check at session time.**
7. **Successor queue documented, nothing console-blocking.** The one named
   follow-up — SEH64 PS5-trampoline pre-execution review (naked-trampoline
   register contracts host-verified only; required before any reliance on
   SEH64 beyond the exercised fixture census) — plus the family host-only
   caveats, are recorded in DLL-TRACK-STATE §6 with owners implied (the
   SEH64 lineage workers). — **MET** (this closure package).
8. **Provenance and console hygiene.** Raw worker reports unchanged;
   merge provenance retained; result JSONL append-only intact; console
   receipt + result copy written for the dll8 run; failed attempts recorded,
   never replayed. — **BINDING at closure.**

## Done statement template

> The Windows-services DLL track is COMPLETE when 1–3 hold (already true),
> criterion 4's dll8 verdict is recorded as PASS (or as the documented
> 7-family carve-out), and 5–8 hold at recording time. The remaining
> engineering surface after that point (trampoline review, host-only
> divergences, MSVCP140 as a possible future family) is successor work, not
> open track work.
