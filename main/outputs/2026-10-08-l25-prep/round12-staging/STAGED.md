# Round 12 staged items (integrator), already applied to the candidate tree on 2026-10-13. NOT built yet.

Backups of the R11 state are in merge-backup/.

1. [R12-WORLD] LuaMod.cpp world_ev: the counted [REFUSED] lines now come from emit_counted(reason, effect).
   - refuse() uses it with effect accepted-undelivered.
   - At 60 s pending it is also emitted with reason object-index-not-usable-yet-60s, effect accepted-pending
     (R11 review FIX-SOON #4).
2. UObjectGlobals.cpp: FreeWatch comment corrected (R11 review NOTE #1).

Before the next build:
- re-sync typescan-r4/CHANGES.json candidate_sha256 for both files (add an r12_row_sync row);
- add marker 'effect=accepted-pending' (narrow) only if the verify manifest needs it.

Syntax check: both units 0 errors (evidence/gates-R12-precheck-20261013/syntax/).
