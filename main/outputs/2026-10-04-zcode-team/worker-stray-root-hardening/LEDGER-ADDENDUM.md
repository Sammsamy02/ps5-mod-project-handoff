# LEDGER ADDENDUM — PROPOSED TEXT ONLY (worker-stray-root-hardening, 2026-10-05)

**Status: PROPOSAL. This worker did NOT touch the ledger.** If the controller
accepts, append the two entries below verbatim (or with corrected timestamps)
as top-level keys of
`.superpowers/native-observations/transport-experiments-progress.json`,
matching that ledger's existing `zcode_worker_*` entry shape
(`{utc, status, key/evidence, limits}`). The `utc` fields are placeholders —
set them to the actual UTC moment of append.

```json
"zcode_worker_stray_root_incident_quarantine": {
  "utc": "<set-at-append, e.g. 2026-10-05T13:00:00+00:00>",
  "status": "STRAY_SUPERPOWERS_TREE_QUARANTINED_ROOT_RESOLUTION_RESTORED",
  "key": "Stray outputs/2026-10-04-zcode-team/.superpowers/ (6 paths, 14,353 B, all mtime 2026-10-05 05:42:47 local) = botched staging copy of ONE FAILED owned-dll-sdk-candidate spike attempt, build b33f79f5a88b40e6bdfd1aff15816f99 (status FAILED, commands []), created by worker-seh64-trampoline-fix executing vendor/windows-candidate/build.py (ROOT=HERE.parents[2] misroot; receipt path algebra self-records ROOT==team dir); NEVER recorded in the canonical spike tree or this ledger (seh64 F1-F4 entry covers only final work); shadowed package_prx.py::_find_root (first bare .superpowers within 6 ancestors) for every team-dir driver, producing TWO fail-closed wrong-root receipts, zero wrong-root approvals: worker-launch10-walkguard-prep run-a (build-receipt utc 2026-10-05T07:00:29.111910, 12 lineage gaps, preserved as prx-candidate-composition6-l10wg-a.rootgap-firstattempt + logs/run-a.console.log) and worker-launch10-r3-armed run-a (~07:06 local, same 12 gaps, preserved in logs/run-a.console.log); bug class prior instance worker-crt-integrate 2026-10-04 17:08 (same vendor parents[2] misroot, no stray left behind)",
  "disposition": "CONTROLLER EXECUTED quarantine per worker-stray-root-audit option A: renamed to .superpowers.QUARANTINED-stray-20261005T0542-b33f79f5-worker-seh64-trampoline-fix (every byte preserved, no delete), root resolution verified restored; frozen package_prx.py untouched (sha e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313); walkguard WGP E4 becomes verified no-op (its m.ROOT != ROOT guard), r3 L10R3 E3 unaffected; seh64-trampoline-fix owner reconciliation of b33f79f5... still PENDING (receipt remains quarantined, not merged into canonical spike tree)"
}

"zcode_worker_stray_root_hardening": {
  "utc": "<set-at-append>",
  "status": "DELIVERED_FIND_ROOT_V2_HOST_TESTED_9_OF_9_ADOPTION_PROPOSED",
  "output": "outputs/2026-10-04-zcode-team/worker-stray-root-hardening/",
  "key": "Versioned hardened resolver find_root_v2.py (sha 5590f8bdc87593c3500efffe71e1b57a1575bcef5caf3a74d030a9bd26425bc2) replacing frozen _find_root contract OUT OF PLACE: STRONG marker required (.superpowers/native-observations AND >=1 of ledger transport-experiments-progress.json / README-RETURN.md / .superpowers/sdd), walk up to 8 ancestors (frozen 6), ambiguity fails LOUD as RuntimeError subclass listing candidates (multiple strong roots; ANY weak-marker-only decoy e.g. bare .superpowers = the 05:42 condition), --root override honored (verified, --allow-weak-override escape hatch); host tests 9/9 PASS (fake trees incl. stray shape + no-corroboration decoy + >6 depth + override; real-tree equivalence: v2(frozen dir) == frozen module ROOT, quarantined name classifies None, frozen sha unchanged before/after); testing CAUGHT AND FIXED a real v2 bug pre-delivery (inverted dir-marker check would have awarded phantom sdd corroboration); ADOPTION.md gives exact per-pattern one-line import + guard for all 12 frozen-file-loading drivers (9 P1 check_sha + 3 P2 EXPECTED_SHA; walkguard E4 and r3 E3 kept armed, not replaced)",
  "limits": "v2 is host-tested only, no packaging run has executed with v2 adopted; no driver edited yet (adoption is next bounded revision of each driver, controller-owned); corroboration marker set enumerated from today's true root — re-pin v2 if root markers are restructured; frozen file MUST stay byte-identical (12 driver sha pins + receipt provenance); ledger addendum itself is proposal-only until controller appends"
}
```

## Placement note

`transport-experiments-progress.json` is a flat JSON object keyed by
`zcode_worker_*`; the two blocks above go as two new top-level keys (comma
before the first one as required by the existing last entry). A JSON syntax
check after append is advised:

```sh
python3 -c "import json;json.load(open('.superpowers/native-observations/transport-experiments-progress.json'));print('ledger JSON OK')"
```
