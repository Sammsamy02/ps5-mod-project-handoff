# R11 independent review (2026-10-13): VERDICT PASS (Mac-only)

Diff: `round11-staging/R11-runtime.diff`.

Simulation: `review/r11_watch_sim.cpp`, output in `review/r11_watch_sim.out`. It copies Observe/MergeWatch/Sync/note_deleted line for line.

- 600,000 steps, 163,214 exact syncs, 436,786 budgeted syncs, 69,228 cuts, 8.6M same-address reuses: 0 failures.
- ASan/UBSan run: 0 failures.
- 3 deliberately broken copies (no final merge, no freed-slot clear, early-stopping exact pass) all FAIL, so the checks are real.

## Findings

1. NOTE (UObjectGlobals.cpp:1420-1424, comment): a listed slot can also hold a current entry, when a brand-new slot is
   freed and refilled between two syncs.
   - Harmless: the entry is current, and the next pass removes the slot from the list.
   - Comment fix in the next build.
2. NOTE (UObjectGlobals.cpp:1763-1788): budgeted passes no longer check newly freed slots first.
   - With a large list and no exact lookup, a reused slot's new object can reach NotifyOnNewObject or BeginPlay a few
     passes late (worst case in the simulation: 39 passes).
   - This is a delay, never a miss.
3. NOTE: exact lookups still visit every listed slot. Each visit is now one read, but the cost still scales with the list.
   - Measure on the console: R9-NAME find_avg_us after the elevator.
4. FIX-SOON (LuaMod.cpp:5642-5662): if the delete listener never fires, the route stays "pending" forever. That gives
   one warning line and no counted [REFUSED] lines.
   - Next build: emit the counted [REFUSED] lines at 60 s as "accepted-pending". Also have the harvest flag a pending
     state that lasts more than 5 minutes.
5. NOTE (port UEnum.cpp edit functions): they step through the array using a 16-byte pair size.
   - This is correct only while FName is 8 bytes. It is true for this UE 5.6 shipping build, but wrong for
     case-preserving-name builds.
   - InsertIntoNames / RemoveFromNamesAt reallocate engine memory on the PS5 for the first time. Watch the self-test
     enum rows.
