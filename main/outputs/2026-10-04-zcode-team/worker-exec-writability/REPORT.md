# REPORT — worker-exec-writability (2026-10-04)

Mission: settle what is actually known about in-game mprotect of the exec-only span
`[0x400000,0x787c000)` prot=0x4, and design the single definitive in-game probe plus the
post-probe decision tree for the PLH hook architecture. Evidence synthesis + probe design
only. Deliverable order honored: CLAIM.json first, EXEC-WRITABILITY-EVIDENCE.md,
EXEC-WRITABILITY-PROBE-DESIGN.md, this REPORT last. ~45 tool calls used. No console contact,
no nested subagents, nothing executed, no frozen or wiring file modified.

## 1. Bottom line

**Nothing is proven.** No mprotect transition (X->RX, X->R, X->RWX) and no restore behavior
has ever been demonstrated in-game; no rc and no errno from any mprotect exists anywhere in
the retained records. The single attempt (igf5, raw syscall-74 shim) wrote its seq1-3 records
(entry/startup/mapscan) and died at/before its first mprotect call — console and game lost.
The syscall-fault attribution is the leading hypothesis, never confirmed. The no-syscall
control (igf6, direct read) died twice at its first exec-span read, module-contained,
mechanism NOT localized per the user amendment. Therefore the port's load-bearing question —
can text be made writable/readable — remains fully open, and the hook architecture's viability
is UNPROVEN in both directions.

## 2. Key findings (each cited in EVIDENCE.md)

1. **Recovered raw records correct two stale summaries.** The cumulative console logs
   (`outputs/2026-10-04-zcode-controller/console/igf6-results.jsonl`,
   `c2-alternative-results.jsonl`) retain igf5 seq1-3 ("zero records" was the pre-recovery
   state) and SIX igf6 records from TWO launches (checkpoint said 3 of 8, one launch). The
   precise shared fact: both builds wrote exactly entry/startup/mapscan and stopped at their
   first exec-page-touching operation.
2. **mprotect is import-unavailable:** not in the frozen 191 contract, undeclared for in-game
   SDK use, absent from libkernel exports and all NID tables; libc.a's mprotect() would call
   address 0 in the composed PRX toolchain (Packager resolves the weak kernel_mprotect
   statically to 0). Raw syscall 74 is the ONLY mechanism — and it is the one associated with
   the day's worst outcome.
3. **The VMMAP record itself is solid and repeatable:** exec span `[0x400000,0x787c000)`
   prot=0x4 (=PROT_EXEC) across igf2/igf-f4/igf7 records; 16-KiB pages; LOAD0 file bytes end
   at 0x7879b1c, leaving a mapping-roundup tail that is the safest possible write-test target.
4. **Even with a kernel "yes", PLH is missing its platform layer:** MemProtector/MemAccessor/
   Misc have no Prospero branches (protection translate, old-prot lookup, executable
   allocator) — a missing verified contract, in addition to the kernel question.
5. **Kernel-side expectations are speculation-marked:** FreeBSD-generic EACCES/max-protection
   reasoning is background only; the sole hardware datapoint is igf6's read death (leading
   hypothesis, unlocalized).

## 3. The probe (IGW-1) — designed, not built

One module, return session, rides the comp6 launch as an appended `= 2` ploader.ini line
(after the composition line; distinct log path; mode-0 rollback). No new imports; exactly one
raw syscall site; all constants build-parsed from pinned SDK headers. Ladder (12 contiguous,
write-through records, each persisted before the next syscall): trusted pre-map with gate →
X->RX request/result (far page 0x7878000, write-test address 0x787b800 in the roundup, past
file image) → map verify → roundup read → restore+verify → X->RWX request/result → map verify
→ 1-byte write/readback/revert → restore+verify → fail-closed summary emitting the decision
input {W_WORKS, W_FAILS_R_WORKS, BOTH_FAIL}. Risk stated plainly: the RX-first ordering puts
the highest-value question first, but if syscall 74 is ring-0-fatal the console+game loss
recurs and cannot be excluded by design; go/no-go is controller+user.

## 4. Decision tree (summary; full version in PROBE-DESIGN §7)

- **W works:** port runs as designed; remaining work is the PolyHook Prospero platform layer
  (implement translate/old-prot-via-mapscan/near-allocator), then EngineTick-first integration.
- **W fails, R works:** PLH detours dead in patch form (no no-write mode exists in PLH2);
  switch to dispatch-data writes — vtable swaps (EngineTick/ProcessEvent/C5-C9), GNatives
  table, FC-1 validator replacing the FName detour (now launchable with RX); all FLAG-2/3
  reads unblock.
- **Both fail:** hook architecture dead on this target; disable init-armed hooks via settings,
  keep the data-side port (seed bindings, calls still execute, Lua mods, layout inis, direct
  vtable-target invocation); lose interception and engine-tick closures unless re-planned.
- **Fatal fault inside a syscall:** igf5's shape; terminal for the syscall route; no replay.

## 5. Honesty bounds

All in-game facts are quoted from retained raw records; every prediction about kernel behavior
is marked speculation in EVIDENCE.md §4. No quality/acceptance claim of any kind. IGW-1 is
proposal-only: if staged, it needs the explicit master-plan pre-image update (PROBE-DESIGN
§6.2) and user coordination per R-EXEC-01. FC-1 remains held pending this probe's outcome.
