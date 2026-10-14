# Agent Z4 — l25 phase-tag verification (compile + run harness)

Completed 2026-10-08. Independent verification of agent P's runtime-phase tag
edits in `outputs/2026-10-08-l25-runtime/typescan-r4/` (report:
`2026-10-08-l25-prep/agent-P-phase-tags.md`). Tree treated read-only; all
harness artifacts confined to `/tmp/l25-verify-phasetags/`.

**VERDICT: ALL CLAIMS VERIFIED. 34/34 harness checks passed in both plain and
ASan+UBSan modes (exit 0 each). Staging integrity exact. Zero discrepancies.**

## 1. Marker presence, uniqueness, placement (grep + context reads)

| Marker | File:line (candidate) | Count | Placement verdict |
|---|---|---|---|
| `[l25] event loop started` + `diagnostic_event("event_loop_started",1u)` | UE4SSProgram.cpp:1605-1606 | 1 each | VERIFIED — head of `update()`, directly after `m_event_loop_thread_id = std::this_thread::get_id();` (:1599) and before the shutdown check / `on_program_start()` call (:1612). First code the event-loop jthread body runs. |
| `[l25] on_program_start entered` + `on_program_start_entered` event | UE4SSProgram.cpp:1458-1459 | 1 each | VERIFIED — top of `UE4SSProgram::on_program_start()` (def :1449), after `using namespace Unreal;` (:1452), before the install/start chain. |
| `extern "C" void diagnostic_event(...)` | UE4SSProgram.cpp:13 | 1 | VERIFIED — global scope, before first use. |
| `[l25] marshal hook first invocation (carrier fired)` + `r3_marshal_first_invoke` event | LuaMod.cpp:4865-4872 (comment from :4859) | 1 each | VERIFIED — top of `funcptr_marshal_hook` (def :4856), BEFORE `#ifdef __PROSPERO__` (:4873), so it fires on any platform and regardless of gate outcomes (including early `return`s in the gates). Guard is `static std::atomic<bool> s_marshal_hook_first_invocation{true}` + `.exchange(false, relaxed)` — true once-per-process. |
| `diagnostic_event("r3_marshal_pumped", ms)` | LuaMod.cpp:4909-4922 | 1 (code; 1 additional grep hit is the :4910 comment) | VERIFIED — immediately after `m_funcptr_marshal_last_fire_ms.store(r3_now_ms(), ...)` (:4908) and past the `IsInGameThreadRaw()` early-return (:4903-4906), i.e. only on the published game thread. `>=1000` ms CAS throttle, no log line, no allocation in the pasted code. |
| `[l22] ExecuteInGameThread queued depth={}` + `eigt_queue_depth` event | LuaMod.cpp:5653-5677 | 1 each | VERIFIED — directly after the existing per-closure queued log (:5649-5652, untouched); inside the `std::lock_guard<std::recursive_mutex> guard{LuaMod::m_thread_actions_mutex}` scope (guard at :5615; enclosing scope closes at :5718) — depth reads are race-free. Sums exactly six queues (:5669-5671): game_thread + pending, delayed + pending, engine_tick + pending. `>=1000` ms CAS throttle. |
| `extern "C" void diagnostic_event(...)` | LuaMod.cpp:96 | 1 | VERIFIED — global scope before `namespace RC` (:104), same shape as UE4SSProgram.cpp:13. |

Cosmetic only (no behavioral impact, flagged for the record): the task's
claimed ranges "4909-4923" and "5653-5678" each overstate by one line — the
added statements actually end at :4922 and :5677 respectively (:4923 is the
pre-existing `NativeLuaActionDispatchScope` declaration; :5678 is blank).
Agent P's own hunk math (+14/+25 added lines) matches the real diffs.

## 2. Staging integrity (claim 3) — EXACT MATCH

sha256 recomputed from disk:

| File | original/src/ | Prescribed pin | Match |
|---|---|---|---|
| UE4SSProgram.cpp | `030407d7351e2d7b78ac817ca572a0574efc985d49983fd24143061d224295b0` | `2026-10-08-l23-runtime/thread/src/UE4SSProgram.cpp` = same | YES |
| LuaMod.cpp | `fc9589dbb81ee7128d5f54b46061a01f3faf54b4b038e6bdfc5ac2626348119f` | `2026-10-08-l23-runtime/thread/src/LuaMod.cpp` = same | YES |
| ScriptHookThunks.cpp | `ecec915976c2b16ccecea11ec44d39c465252c7451e62a4a6671c00704d3c755` | `2026-10-08-r5-source-integration/source/ScriptHookThunks.cpp` = same | YES |

Candidates match agent P's reported post-edit hashes exactly
(UE4SSProgram `352da81b…207ba5`, LuaMod `1c0c103b…846d9`,
ScriptHookThunks unchanged). Diff hunk counts vs original match report §5:
UE4SSProgram 2 hunks (@1450 +7, @1591 +7), LuaMod 4 hunks (@90 +5, @4851
+14, @4887 +14, @5617 +25), nothing else changed.

## 3. ScriptHookThunks.cpp (claim 4) — CONFIRMED

`diff original/src/ScriptHookThunks.cpp candidate/src/ScriptHookThunks.cpp`
is empty (exit 0); both sides sha256 `ecec9159…`.

## 4. Compile + run harness (`/tmp/l25-verify-phasetags/`)

Same pattern as `/tmp/l25-sanity/scanbody_harness.cpp` and
`/tmp/l25-verify-fixes/`. Verbatim sed extractions from the candidate files
(region sha256 recorded; full-file sha256 at extraction time matches §2):

| Region | Source lines | sha256 |
|---|---|---|
| upd_head.inc | UE4SSProgram.cpp:1598-1606 | `c6e53f5d865ab47550a077efc390464e1bd3a649341041a7ce07cf0f4dce4332` |
| ops_head.inc | UE4SSProgram.cpp:1451-1459 | `ac0903c06d55223887320546a54e527c8dd5b8de8e10385b1e95b65308360447` |
| marshal_first.inc | LuaMod.cpp:4859-4872 | `bd7664ca30522bc40544e7ba9748a8cf8ec7b19e7368ab5306aeb16227600312` |
| pump_block.inc | LuaMod.cpp:4907-4922 | `757e05e7c7383bf1f5725192a6aed26b6847e854eaa36d3f0aebb0018d88659b` |
| depth_block.inc | LuaMod.cpp:5649-5677 | `fcadd37ad58657b8229407764ef1093afc92cbde32f2ad6533a8b76af886e826` |

Only out-of-region dependencies are stubbed (Output::send capture,
diagnostic_event sink, L22Diagnostics::observe pass-through, mocked
`r3_now_ms` clock, mock queues, real `std::recursive_mutex`). The marker
statements, atomic guards, throttle arithmetic and six-queue summation are
the shipped code character-for-character.

Compile: `clang++ -std=c++20 -include utility` — zero errors, both modes
(plain; `-fsanitize=address,undefined -fno-sanitize-recover=all -g`).

Run results (both modes, exit 0): **ALL CHECKS PASSED: 34 checks, 0 failures.**
Key assertions actually exercised:
- update()/on_program_start() heads print exactly one line + one event per
  call, nothing else, no cross-contamination.
- First-invocation guard: 3 sequential calls → exactly 1 line + 1 event;
  8 threads x 2000 concurrent calls → still exactly 1 (atomic exchange).
- Pump throttle (mocked clock, exact boundaries): t=5000 fires (value 5000);
  +400 and +999 suppressed; exactly +1000 fires (>= semantics); +1 after
  admission suppressed. Store-before-throttle ordering proven: during a
  suppressed call `m_funcptr_marshal_last_fire_ms` still updates to the new
  now-ms. Zero console lines through the whole sequence (log-free as claimed).
- Depth block: mock queue set 2+1+0+3+0+1 → logs `queued depth=7`, event
  value 7, exactly six `.size()` reads, every read under the mutex
  (instrumented); per-closure log still prints each call; immediate repeat
  suppressed; after 1100 ms real sleep re-fires with the new sum (5).

Negative controls (sabotaged region copies, tree untouched) prove the checks
are not vacuous:
- `exchange(false,…)` → `load(…)`: first-invocation once-only checks FAIL
  (and the induced unguarded concurrent logging crashes — the guard is
  load-bearing).
- `>= 1000` → `> 1001`: the exact +1000 ms admission checks FAIL (3
  failures).

## 5. Discrepancies

None behavioral. Only the two cosmetic one-line range overstatements noted
in §1 (claimed 4909-4923/5653-5678 vs actual added statements ending
4922/5677). No compile errors, no placement errors, no behavioral
discrepancies. Harness observed UB: none (ASan/UBSan clean).

Scope note: this verifies the edits compile and behave as specified in a
host harness with stubbed dependencies. It is not a target (PS5) compile,
not a twin build, and not runtime proof on console — consistent with agent
P's NOT DONE list.
