# Agent P — l25 item 6: runtime-phase tags

Completed 2026-10-08 ~14:30 CDT. No builds, no console/network contact, no
tests run. Edits confined to the typescan-r4 candidate tree; CHANGES.json
updated only for files touched by this agent.

## 0. Staging decision (files were not yet in the tree)

The task named `typescan-r4/candidate/src/{UE4SSProgram,LuaMod}.cpp` and
`ScriptHookThunks.cpp` as the edit targets, but typescan-r4 carried only the
typescan subset (TypeChecker/UnrealInitializer/CustomProperty/Property/*).
The current runtime sources live as composition pins, and
`thread/CHANGES.json`'s l25 scope note prescribes exactly which paths rows
must use when thread files change:

- UE4SSProgram.cpp -> `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-runtime/thread/src/UE4SSProgram.cpp` (composition unit `program`, sha256 `030407d7…`)
- LuaMod.cpp -> `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l23-runtime/thread/src/LuaMod.cpp` (composition unit `luamod`, sha256 `fc9589db…`)
- ScriptHookThunks.cpp -> `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-r5-source-integration/source/ScriptHookThunks.cpp` (composition unit `native-script-hook-thunks`, sha256 `ecec9159…`)

All three were copied (pin-identical, hashes verified before any edit) into
`typescan-r4/original/src/` (provenance) and `typescan-r4/candidate/src/`
(edit surface), then the phase tags below were added to the candidate copies.
`build_runtime.py` merges `files` rows from every part dir, so rows added in
`typescan-r4/CHANGES.json` integrate with the (still unrun) twin build the
same way; `target_compile.py`, if later run from the worktree root, will now
also recompile these three units — that run is the parent worker's call.

## 1. Markers added (exact grep-able text)

All paths below are under
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-runtime/typescan-r4/candidate/src/`.
Line numbers are post-edit candidate lines; original-line references in
parentheses.

### UE4SSProgram.cpp

| Marker (log line) | diagnostic_event | Location |
|---|---|---|
| `[l25] event loop started` | `event_loop_started=1` | `update()` head, lines 1605-1606, right after `m_event_loop_thread_id = std::this_thread::get_id();` (orig. 1592). This is the first code the `m_event_loop` jthread constructed in `init()` at the :715 region (orig. 715, unchanged) executes, so the line proves both that init() completed past the UnrealInitializer stall window and that the loop thread is actually scheduled — not merely constructed. |
| `[l25] on_program_start entered` | `on_program_start_entered=1` | top of `UE4SSProgram::on_program_start()`, lines 1458-1459, after `using namespace Unreal;` (orig. def. at 1449; called from `update()` at orig. 1598). The install_lua_mods / `LuaMod::on_program_start` / start_lua_mods chain (orig. 1569-1572) runs later in this same function, so the marker bounds that chain on both sides. |

### LuaMod.cpp

| Marker (log line / event tag) | Throttle | Location |
|---|---|---|
| `[l25] marshal hook first invocation (carrier fired)` + `diagnostic_event("r3_marshal_first_invoke", 1)` | once per process (`std::atomic<bool>::exchange`) | top of `funcptr_marshal_hook`, lines 4859-4873 (orig. def. 4851), before the `#ifdef __PROSPERO__` ticket/identity gates so it fires on any platform and regardless of gate outcomes. Proves the ScriptHookThunks `UFunction::SetFuncPtr` swap (ScriptHookThunks.cpp:155) actually delivered an engine call. |
| `diagnostic_event("r3_marshal_pumped", ms)` | >=1 s (`steady_clock` CAS throttle; no allocation, no log line) | `funcptr_marshal_hook`, lines 4909-4923, immediately after the `m_funcptr_marshal_last_fire_ms.store(r3_now_ms(), …)` line (orig. 4889 = Claude's "after :4827" in l22 numbering) and past the `IsInGameThreadRaw()` gate — i.e. the carrier fired on the published game thread and the drain path is reachable. Claude's exact recommendation, tag string verbatim. |
| `[l22] ExecuteInGameThread queued depth={}` + `diagnostic_event("eigt_queue_depth", N)` | >=1 s (CAS throttle) | `ExecuteInGameThread` enqueue site, lines 5653-5678, directly after the existing per-closure queued log (orig. 5617/5615 = task's ":5411/:5541" queue sites — the two cited sites are the two historical enqueue paths this single site now serves, EngineTick and marshal). Depth = sum of all six queues (`m_game_thread_actions` + pending, `m_delayed_game_thread_actions` + pending, `m_engine_tick_actions` + pending), read under the already-held `m_thread_actions_mutex`, so the reads are race-free. |

Supporting edit: `extern "C" void diagnostic_event(const char* tag, unsigned int value);`
declared at LuaMod.cpp:92-96 — the same in-link sink declaration UE4SSProgram.cpp:11-13
carries, placed at global scope before first use.

## 2. Existing lines deliberately NOT duplicated

- **`[R3] FuncPtr-marshal armed on carrier {} (callback id {})`** — already exists at
  LuaMod.cpp candidate line 5089 (orig. 5056), emitted by the arming path the task
  cites (`LuaMod::on_program_start` arming → orig. 7933→5138 region). This covers
  task item 3 ("marshal armed on carrier"); no duplicate was added, and none was
  added in ScriptHookThunks.cpp either (its `SetFuncPtr` swap at :155 is the
  mechanism that line reports; the new first-invocation marker proves the swap
  actually fired, which is the complementary fact).
- **`[l22] ExecuteInGameThread queued method={} state={} ref={}`** (candidate 5650,
  orig. 5617) — existing per-closure queue log kept as-is; the new depth line only
  adds the throttled depth dimension it lacks.
- **`[l22] ExecuteInGameThread drained state={} ref={} (callback entry)`** (candidate
  4367, orig. 4362) — existing drained-per-closure marker; per the task's
  conditional ("IF the existing queued/drained lines don't already exist"), no
  additional drained marker was added.
- `[l22] Lua mod started: {} (main script returned)` (candidate 7615, orig. 7557)
  and `[R3] marshal is the game-thread pump …` (candidate 4884 region) — already
  present, untouched.

## 3. ScriptHookThunks.cpp

Staged into `original/src/` + `candidate/src/` byte-identical to its pin
(sha256 `ecec915976c2b16ccecea11ec44d39c465252c7451e62a4a6671c00704d3c755`,
verified `diff`-empty). **No edit**: item 3 was the only tag that could belong
there and it is already covered by the existing armed-on-carrier line above.
The candidate row exists in CHANGES.json so the composition has an explicit,
hash-pinned record that this file was inspected and intentionally left
unmodified (candidate_sha256 == original_sha256).

## 4. CHANGES.json (typescan-r4)

- 3 rows appended to both `changes` and `files` (15 total each): original =
  the composition-pin paths in §0, copied_original under `typescan-r4/original/src/`,
  candidate under `typescan-r4/candidate/src/`.
- 1 dated entry appended to `review_fixes` (2026-10-08, source: Claude l25
  item 6 / agent P) describing every marker, its file:line, the
  non-duplication decisions, and the not-done list.
- Recomputed sha256 after the edits:
  - UE4SSProgram.cpp original `030407d7351e2d7b78ac817ca572a0574efc985d49983fd24143061d224295b0` → candidate `352da81b6a61712e43d0a1ebe690613d2ceb455bbb793f2f9be389273b207ba5`
  - LuaMod.cpp original `fc9589dbb81ee7128d5f54b46061a01f3faf54b4b038e6bdfc5ac2626348119f` → candidate `1c0c103b04e40d31c958163fe55982aab807b93840e871172dcf984f106846d9`
  - ScriptHookThunks.cpp `ecec915976c2b16ccecea11ec44d39c465252c7451e62a4a6671c00704d3c755` (unchanged both sides)
- Post-write validation: all 15 rows' original/candidate/copied_original
  hashes re-verified from disk, candidates confirmed inside the l25-runtime
  tree — i.e. the exact assertions `build_runtime.py` makes on these rows
  pass. The pre-existing 12 rows and the agent-K/agent-N review entry are
  untouched.

## 5. Diffs (summary)

- UE4SSProgram.cpp: 2 hunks (orig. @1450 +7 lines, orig. @1591 +7 lines) —
  the two phase tags; nothing else changed.
- LuaMod.cpp: 4 hunks (orig. @90 +5 = extern decl; orig. @4851 +14 =
  first-invocation; orig. @4887 +14 = throttled pump event; orig. @5617 +25 =
  throttled depth) — nothing else changed.
- ScriptHookThunks.cpp: zero hunks.

## 6. NOT DONE (never claim otherwise)

No host suite run, no target compile, no twin build, no packaging, no
import-identity check, no console proof. The markers' compile correctness is
reviewed-by-reading only (idioms matched: `Output::send(STR(...))`,
`L22Diagnostics::observe`, `<atomic>`/`<chrono>` availability confirmed via
LuaMod.hpp includes and existing cross-platform uses). The l25 phase tags
become evidence only after the parent's compile + run.
