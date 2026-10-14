# REPORT — worker-first-run-playbook (restart)

Verdict: DELIVERED — FIRST-RUN-PLAYBOOK.md traces the actual delivered wiring
end-to-end (entry → bridge → ack-supplier → prepare → owner → UE4SS program →
Lua mod) and defines expected observables, failure signatures, the freshness
oracle, and the rung ladder. This restart replaced the prior attempt's
CLAIM.json, which had been written while the wiring was still only planned.

## Deliverables (this dir, in order)
1. `CLAIM.json` — rewritten for the restart (wiring now DELIVERED; prior
   "planned wiring" honesty notes superseded).
2. `FIRST-RUN-PLAYBOOK.md` — the playbook (observables table with file:line
   cites, FAILURE-SIGNATURES F0–F10, FRESHNESS-ORACLE O1–O5, rungs 0–5).
3. `REPORT.md` — this file, last.

## Key findings from the trace
1. **Three sinks, three formats.** Observer log
   `/data/townfall_runtime_aligned_observer.log` (append-only, tagged
   `value=0x…` events, diagnostic.c:146–157); `/data/plugins/UE4SS.log`
   (entry failure lines with NO timestamp, entry:166–192, plus the runtime's
   own `Output::send` — the PROSPERO mod-source branch sets log dir = module
   parent = `/data/plugins`, UE4SSProgram.cpp:776–784);
   `<mod root>/runtime-status.txt` (tab-separated session file, `O_TRUNC` per
   open — each launch REPLACES it, modforge_activation.c:609).
2. **True mods order (corrects a plausible misreading):** `activated`,
   `engine-ready`, `mods-started` are all written BEFORE main.lua executes.
   With `Deferred` policy the backend ctor only builds the inventory
   (`start_native_mods`, UE4SSProgram.cpp:549–592); the Lua VM actually runs
   main.lua later on the event-loop thread via `on_program_start` →
   `start_lua_mods` (UE4SSProgram.cpp:1514, 1481–1490, 1946–1956). A broken
   script therefore does NOT invalidate `mods-started` — failure signature F9
   is mod-level and non-fatal, distinguishable by the runtime's script error
   lines (LuaMod.cpp:6413/6424/6435/6465/6471/6478).
3. **One honest trap documented (F6):** `StartActivatedRuntimeWithAck` does
   not check `owner->start()`'s bool (runtime_ack_supplier.cpp:93–95), so a
   thread-spawn failure yields observer `runtime_start_ok` while the status
   file already shows `failed` — the status file is the session truth.
4. **Status-file replacement subtlety (F5):** the entry's terminal
   `write_status_failed` re-opens `runtime-status.txt` with `O_TRUNC`
   (entry:205, mfa .c:609), REPLACING any earlier ack lines of the same
   launch — after a prepare-time throw the file holds exactly one
   `failed code=3` line, not the ack's earlier `failed code=1`.
5. **Freshness oracle recommendation:** attribute launches primarily by the
   observer's `module_start_entered` count (append-only) + the status file's
   `session <pid> <monotonic-ms>` header + mtime; treat any UE4SS.log line
   not matching the 3 known baseline formats
   (PATH_TEST / ACTIVATION_TEST / sdg2 DIAG 1791155755.879) as a
   composition-5+ launch line. Recommendation: YES, the next entry revision
   should add monotonic-ms + pid to its failure lines (both primitives are
   already measured/linked; the entry already computes them for the status
   path) — but it is NOT a launch blocker.

## Flags for the controller
- **Sha mismatch (must reconcile at staging Gate 1):** task context cites entry
  sha `1c041f8b…`; on-disk `runtime_module_entry.c` is
  `f3654703bf08d9e00d5035c118a0b6f2c5194e41`. All playbook cites are against
  the on-disk file.
- `worker-composition-5-rebuild` had NOT executed its build when this worker
  read it (wiring landed after its run); the playbook therefore traces source,
  not a staged binary. No claim is made that any post-baseline line has ever
  appeared on console.
- Rung 1–2 acceptance language is quoted from worker-query-harness REPORT §5
  (its real-process JSONL criteria); `engine-ready` alone is explicitly NOT
  game-thread evidence.

## Bounds honored
No console contact; no nested subagents; writes confined to this directory;
~40 tool calls total; existing files read-only.
