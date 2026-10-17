# B6p review (one independent reviewer, 2026-10-12)
Round 1: BLOCK. (1) blocker: a freed slot kept the old ObjAt/ClassAt until its re-check; with a budget the wait is
many frames and a new object reusing the same address+class was never reported (Observe early return) -> clear
ObjAt/ClassAt for every Dirty index at once. (2) should-fix: newly freed slots first (engine reuses newest freed).
(3) nit: reserve Watch, keep FreeWatch capacity. All applied.
Round 2: SHIP (no duplicates; InWatch invariant holds; queries exact via full sync; notify never lost, delivered once).
Confirm after build: import-table diff (vector range insert may use memmove/memcpy, expected already imported).
